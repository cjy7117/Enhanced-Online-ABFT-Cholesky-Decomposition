#include "FT.h"
#include <iostream>
#include <cmath>
using namespace std;


void compareChk(double * chk1, int ldchk1, double * chk2, int ldchk2, int m, int n) {
	double * chk1_host = new double[m * n]();
	magma_dgetmatrix(m, n, chk1, ldchk1, chk1_host, m);

	double * chk2_host = new double[m * n]();
	magma_dgetmatrix(m, n, chk2, ldchk2, chk2_host, m);

    bool correct = true;
    for (int i = 0; i < m * n; i++) {
    	if (abs(chk1_host[i] - chk2_host[i]) > 1e-5) 
    		correct = false;
    }
    if (correct) {
    	cout << "correct!" << "\t";
    } else {
    	cout << "wrong!"<< "\t" ;
    }
}

//recalculate column checksums
//M: number of rows of A
//N: numner of cols of A
//non-col-read-A
//col-read-B
//non-col-write-C
//separated
void col_checksum_kernel_ncns2(int m, int n, int chk_nb, 
						   double * A, int lda, 
						   double * hrz_vd, int hrz_vd_ld, 
						   double * hrz_chk, int hrz_chk_ld, 
						   magma_queue_t * stream) {

	for (int i = 0; i < m; i += chk_nb) {
		magmablasSetKernelStream(stream[1]);
		magma_dgemv(MagmaTrans, 
					chk_nb, n, 
					MAGMA_D_ONE,
					A + i, lda,
					hrz_vd, hrz_vd_ld, 
					MAGMA_D_ZERO, 
					hrz_chk + (i / chk_nb) * 2, hrz_chk_ld);

		magmablasSetKernelStream(stream[2]);
		magma_dgemv(MagmaTrans, 
					chk_nb, n, 
					MAGMA_D_ONE,
					A + i, lda, 
					hrz_vd + 1, hrz_vd_ld, 
					MAGMA_D_ZERO, 
					hrz_chk + (i / chk_nb) * 2 + 1, hrz_chk_ld);
	}
	cudaStreamSynchronize(stream[1]);
	cudaStreamSynchronize(stream[2]);
}


void col_chk_recal_1(ABFTEnv * abftEnv, double * A, int lda, int m, int n) {
	col_checksum_kernel_ncns2(m, n, abftEnv->chk_nb,
						  A, lda, 
						  abftEnv->hrz_vd, abftEnv->hrz_vd_ld, 
						  abftEnv->hrz_recal_chk, abftEnv->hrz_recal_chk_ld, 
						  abftEnv->stream);
}


//recalculate column checksums
//M: number of rows of A
//N: numner of cols of A
//non-col-read-A
//col-read-B
//non-col-write-C
//separated - 4 stream
void col_checksum_kernel_ncns4(int m, int n, int chk_nb, 
						   double * A, int lda,
						   double * hrz_vd, int hrz_vd_ld,
						   double * hrz_chk, int hrz_chk_ld,
						   magma_queue_t * stream) {

	for (int i = 0; i < m; i += chk_nb * 2) {
		magmablasSetKernelStream(stream[1]);
		magma_dgemv(MagmaTrans, 
					chk_nb, n, 
					MAGMA_D_ONE,
					A + i, lda, 
					hrz_vd, hrz_vd_ld, 
					MAGMA_D_ZERO, 
					hrz_chk + (i / chk_nb) * 2, hrz_chk_ld);

		magmablasSetKernelStream(stream[2]);
		magma_dgemv(MagmaTrans, 
					chk_nb, n, 
					MAGMA_D_ONE,
					A + i, lda, 
					hrz_vd + 1, hrz_vd_ld, 
					MAGMA_D_ZERO, 
					hrz_chk + (i / chk_nb) * 2 + 1, hrz_chk_ld);

		if (i + chk_nb < m) {			
			magmablasSetKernelStream(stream[3]);
			magma_dgemv(MagmaTrans, 
						chk_nb, n, 
						MAGMA_D_ONE,
						A + i + chk_nb, lda, 
						hrz_vd, hrz_vd_ld, 
						MAGMA_D_ZERO, 
						hrz_chk + (i / chk_nb) * 2 + 2, hrz_chk_ld);

			magmablasSetKernelStream(stream[4]);
			magma_dgemv(MagmaTrans, 
						chk_nb, n, 
						MAGMA_D_ONE,
						A + i + chk_nb, lda, 
						hrz_vd + 1, hrz_vd_ld, 
						MAGMA_D_ZERO, 
						hrz_chk + (i / chk_nb) * 2 + 3, hrz_chk_ld);
		}
	}
	cudaStreamSynchronize(stream[1]);
	cudaStreamSynchronize(stream[2]);
	cudaStreamSynchronize(stream[3]);
	cudaStreamSynchronize(stream[4]);
	
}


void col_chk_recal_2(ABFTEnv * abftEnv, double * A, int lda,int m, int n){
	col_checksum_kernel_ncns4(m, n, abftEnv->chk_nb,
						  A, lda, 
						  abftEnv->hrz_vd, abftEnv->hrz_vd_ld, 
						  abftEnv->hrz_recal_chk, abftEnv->hrz_recal_chk_ld, 
						  abftEnv->stream);
}



//col-read-A
//col-read-B
//non-col-write-C
//separated 
void col_checksum_kernel_ccns2(int m, int n, int chk_nb, 
						   double * A, int lda, 
						   double * vrt_vd, int vrt_vd_ld, 
						   double * hrz_chk, int hrz_chk_ld, 
						   magma_queue_t * stream) {

	for (int i = 0; i < m; i += chk_nb) {
		magmablasSetKernelStream(stream[1]);
		magma_dgemv(MagmaTrans, 
					chk_nb, n, 
					MAGMA_D_ONE,
					A + i, lda, 
					vrt_vd, 1, 
					MAGMA_D_ZERO, 
					hrz_chk + (i / chk_nb) * 2, hrz_chk_ld);

		magmablasSetKernelStream(stream[2]);
		magma_dgemv(MagmaTrans, 
					chk_nb, n, 
					MAGMA_D_ONE,
					A + i, lda, 
					vrt_vd + vrt_vd_ld, 1, 
					MAGMA_D_ZERO, 
					hrz_chk + (i / chk_nb) * 2 + 1, hrz_chk_ld);
	}
	
	cudaStreamSynchronize(stream[1]);
	cudaStreamSynchronize(stream[2]);
}

void col_chk_recal_3(ABFTEnv * abftEnv, double * A, int lda, int m, int n) {
	col_checksum_kernel_ccns2(m, n, abftEnv->chk_nb, 
						  A, lda, 
						  abftEnv->vrt_vd, abftEnv->vrt_vd_ld, 
						  abftEnv->hrz_recal_chk, abftEnv->hrz_recal_chk_ld, 
						  abftEnv->stream);
}

//col-read-A
//col-read-B
//non-col-write-C
//separated - 4 stream
void col_checksum_kernel_ccns4(int m, int n, int chk_nb, 
						   double * A, int lda, 
						   double * vrt_vd, int vrt_vd_ld, 
						   double * hrz_chk, int hrz_chk_ld, 
						   magma_queue_t * stream) {

	for (int i = 0; i < m; i += chk_nb * 2) {
		magmablasSetKernelStream(stream[1]);
		magma_dgemv(MagmaTrans, 
					chk_nb, n, 
					MAGMA_D_ONE,
					A + i, lda, 
					vrt_vd, 1, 
					MAGMA_D_ZERO, 
					hrz_chk + (i / chk_nb) * 2, hrz_chk_ld );

		magmablasSetKernelStream(stream[2]);
		magma_dgemv(MagmaTrans, 
					chk_nb, n, 
					MAGMA_D_ONE,
					A + i, lda, 
					vrt_vd + vrt_vd_ld, 1, 
					MAGMA_D_ZERO, 
					hrz_chk + (i / chk_nb) * 2 + 1, hrz_chk_ld );

		if (i + chk_nb < m) {
			magmablasSetKernelStream(stream[3]);
			magma_dgemv(MagmaTrans, 
						chk_nb, n, 
						MAGMA_D_ONE,
						A + i + chk_nb, lda, 
						vrt_vd, 1, 
						MAGMA_D_ZERO, 
						hrz_chk + (i / chk_nb) * 2 + 2, hrz_chk_ld );

			magmablasSetKernelStream(stream[4]);
			magma_dgemv(MagmaTrans, 
						chk_nb, n, 
						MAGMA_D_ONE,
						A + i + chk_nb, lda, 
						vrt_vd + vrt_vd_ld, 1, 
						MAGMA_D_ZERO, 
						hrz_chk + (i / chk_nb) * 2 + 3, hrz_chk_ld );
		}
	}
	
	cudaStreamSynchronize(stream[1]);
	cudaStreamSynchronize(stream[2]);
	cudaStreamSynchronize(stream[3]);
	cudaStreamSynchronize(stream[4]);
}


void col_chk_recal_4(ABFTEnv * abftEnv, double * A, int lda, int m, int n) {
	col_checksum_kernel_ccns4(m, n, abftEnv->chk_nb, 
						  A, lda, 
						  abftEnv->vrt_vd, abftEnv->vrt_vd_ld, 
						  abftEnv->hrz_recal_chk, abftEnv->hrz_recal_chk_ld, 
						  abftEnv->stream);
}


//non-col-read-A
//col-read-B
//col-write-C
//separated
void col_checksum_kernel_nccs2(int m, int n, int chk_nb, 
			   	 		   double * A, int lda, 
			     		   double * hrz_vd, int hrz_vd_ld, 
			     		   double * vrt_chk, int vrt_chk_ld, 
			     		   magma_queue_t * stream) {

	for (int i = 0; i < m; i += chk_nb) {
		magmablasSetKernelStream(stream[1]);
		magma_dgemv(MagmaTrans, 
					chk_nb, n, 
					MAGMA_D_ONE,
					A + i, lda, 
					hrz_vd, hrz_vd_ld, 
					MAGMA_D_ZERO, 
					vrt_chk + (i / chk_nb) * 2 * vrt_chk_ld, 1 );

		magmablasSetKernelStream(stream[2]);
		magma_dgemv(MagmaTrans, 
					chk_nb, n, 
					MAGMA_D_ONE,
					A + i, lda, 
					hrz_vd + 1, hrz_vd_ld, 
					MAGMA_D_ZERO, 
					vrt_chk + ((i / chk_nb) * 2 + 1) * vrt_chk_ld, 1 );
	}
	
	cudaStreamSynchronize(stream[1]);
	cudaStreamSynchronize(stream[2]);
}

void col_chk_recal_5(ABFTEnv * abftEnv, double * A, int lda, int m, int n) {
	col_checksum_kernel_nccs2(m, n, abftEnv->chk_nb, 
						  A, lda, 
						  abftEnv->hrz_vd, abftEnv->hrz_vd_ld, 
						  abftEnv->vrt_recal_chk, abftEnv->vrt_recal_chk_ld, 
						  abftEnv->stream);
}


//non-col-read-A
//col-read-B
//col-write-C
//separated - 4 stream
void col_checksum_kernel_nccs4(int m, int n, int chk_nb, 
			   	 		   double * A, int lda, 
			     		   double * hrz_vd, int hrz_vd_ld, 
			     		   double * vrt_chk, int vrt_chk_ld, 
			     		   magma_queue_t * stream) {

	for (int i = 0; i < m; i += chk_nb * 2) {
		magmablasSetKernelStream(stream[1]);
		magma_dgemv(MagmaTrans, 
					chk_nb, n, 
					MAGMA_D_ONE,
					A + i, lda, 
					hrz_vd, hrz_vd_ld, 
					MAGMA_D_ZERO, 
					vrt_chk + (i / chk_nb) * 2 * vrt_chk_ld, 1 );

		magmablasSetKernelStream(stream[2]);
		magma_dgemv(MagmaTrans, 
					chk_nb, n, 
					MAGMA_D_ONE,
					A + i, lda, 
					hrz_vd + 1, hrz_vd_ld, 
					MAGMA_D_ZERO, 
					vrt_chk + ((i / chk_nb) * 2 + 1) * vrt_chk_ld, 1 );

		if (i + chk_nb < m) {
			magmablasSetKernelStream(stream[3]);
			magma_dgemv(MagmaTrans, 
						chk_nb, n, 
						MAGMA_D_ONE,
						A + i + chk_nb, lda, 
						hrz_vd, hrz_vd_ld, 
						MAGMA_D_ZERO, 
						vrt_chk + ((i / chk_nb) * 2 + 2) * vrt_chk_ld, 1 );

			magmablasSetKernelStream(stream[4]);
			magma_dgemv(MagmaTrans, 
						chk_nb, n, 
						MAGMA_D_ONE,
						A + i + chk_nb, lda, 
						hrz_vd + 1, hrz_vd_ld, 
						MAGMA_D_ZERO, 
						vrt_chk + ((i / chk_nb) * 2 + 3) * vrt_chk_ld, 1 );
		}
	}
	cudaStreamSynchronize(stream[1]);
	cudaStreamSynchronize(stream[2]);
	cudaStreamSynchronize(stream[3]);
	cudaStreamSynchronize(stream[4]);
}


void col_chk_recal_6(ABFTEnv * abftEnv, double * A, int lda, int m, int n) {
	col_checksum_kernel_nccs2(m, n, abftEnv->chk_nb, 
						  A, lda, 
						  abftEnv->hrz_vd, abftEnv->hrz_vd_ld, 
						  abftEnv->vrt_recal_chk, abftEnv->vrt_recal_chk_ld, 
						  abftEnv->stream);
}

//col-read-A
//col-read-B
//col-write-C
//separated
void col_checksum_kernel_cccs2(int m, int n, int chk_nb, 
			   	 		   double * A, int lda, 
			     		   double * vrt_vd, int vrt_vd_ld, 
			     		   double * vrt_chk, int vrt_chk_ld, 
			     		   magma_queue_t * stream) {

	for (int i = 0; i < m; i += chk_nb) {
		magmablasSetKernelStream(stream[1]);
		magma_dgemv(MagmaTrans, 
					chk_nb, n, 
					MAGMA_D_ONE,
					A + i, lda, 
					vrt_vd, 1, 
					MAGMA_D_ZERO, 
					vrt_chk + (i / chk_nb) * 2 * vrt_chk_ld, 1 );

		magmablasSetKernelStream(stream[2]);
		magma_dgemv(MagmaTrans, 
					chk_nb, n, 
					MAGMA_D_ONE,
					A + i, lda, 
					vrt_vd + vrt_vd_ld, 1, 
					MAGMA_D_ZERO, 
					vrt_chk + ((i / chk_nb) * 2 + 1) * vrt_chk_ld, 1 );
	}
	
	cudaStreamSynchronize(stream[1]);
	cudaStreamSynchronize(stream[2]);
}

void col_chk_recal_7(ABFTEnv * abftEnv, double * A, int lda, int m, int n) {
	col_checksum_kernel_cccs2(m, n, abftEnv->chk_nb, 
						  A, lda, 
						  abftEnv->vrt_vd, abftEnv->vrt_vd_ld, 
						  abftEnv->vrt_recal_chk, abftEnv->vrt_recal_chk_ld, 
						  abftEnv->stream);
}



//col-read-A
//col-read-B
//col-write-C
//separated - 4 streams
void col_checksum_kernel_cccs4(int m, int n, int chk_nb, 
			   	 		   double * A, int lda, 
			     		   double * vrt_vd, int vrt_vd_ld, 
			     		   double * vrt_chk, int vrt_chk_ld, 
			     		   magma_queue_t * stream) {

	for (int i = 0; i < m; i += chk_nb) {
		magmablasSetKernelStream(stream[1]);
		magma_dgemv(MagmaTrans, 
					chk_nb, n, 
					MAGMA_D_ONE,
					A + i, lda, 
					vrt_vd, 1, 
					MAGMA_D_ZERO, 
					vrt_chk + (i / chk_nb) * 2 * vrt_chk_ld, 1 );

		magmablasSetKernelStream(stream[2]);
		magma_dgemv(MagmaTrans, 
					chk_nb, n, 
					MAGMA_D_ONE,
					A + i, lda, 
					vrt_vd + vrt_vd_ld, 1, 
					MAGMA_D_ZERO, 
					vrt_chk + ((i / chk_nb) * 2 + 1) * vrt_chk_ld, 1 );

		if (i + chk_nb < m) {
			magmablasSetKernelStream(stream[3]);
			magma_dgemv(MagmaTrans, 
						chk_nb, n, 
						MAGMA_D_ONE,
						A + i + chk_nb, lda, 
						vrt_vd, 1, 
						MAGMA_D_ZERO, 
						vrt_chk + ((i / chk_nb) * 2 + 2) * vrt_chk_ld, 1 );

			magmablasSetKernelStream(stream[4]);
			magma_dgemv(MagmaTrans, 
						chk_nb, n, 
						MAGMA_D_ONE,
						A + i + chk_nb, lda, 
						vrt_vd + vrt_vd_ld, 1,
						MAGMA_D_ZERO, 
						vrt_chk + ((i / chk_nb) * 2 + 3) * vrt_chk_ld, 1 );
		}
	}
	cudaStreamSynchronize(stream[1]);
	cudaStreamSynchronize(stream[2]);
	cudaStreamSynchronize(stream[3]);
	cudaStreamSynchronize(stream[4]);


}

void col_chk_recal_8(ABFTEnv * abftEnv, double * A, int lda, int m, int n) {
	col_checksum_kernel_cccs4(m, n, abftEnv->chk_nb, 
						  A, lda, 
						  abftEnv->vrt_vd, abftEnv->vrt_vd_ld, 
						  abftEnv->vrt_recal_chk, abftEnv->vrt_recal_chk_ld, 
						  abftEnv->stream);
}

//non-col-read-A
//col-read-B
//non-col-write-C
//combined
void col_checksum_kernel_ncnc1(int m, int n, int chk_nb, 
			   	 		   double * A, int lda, 
			     		   double * hrz_vd, int hrz_vd_ld, 
			     		   double * hrz_chk, int hrz_chk_ld, 
			     		   magma_queue_t * stream) {

	magmablasSetKernelStream(stream[1]);
	for (int i = 0; i < m; i += chk_nb) {
		magma_dgemm(MagmaNoTrans, MagmaNoTrans,
					2, n, chk_nb,
					MAGMA_D_ONE, 
					hrz_vd, hrz_vd_ld,
					A + i, lda,
					MAGMA_D_ZERO, 
					hrz_chk + (i / chk_nb) * 2, hrz_chk_ld);		
	}
	
	cudaStreamSynchronize(stream[1]);
}


void col_chk_recal_9(ABFTEnv * abftEnv, double * A, int lda, int m, int n) {
	col_checksum_kernel_ncnc1(m, n, abftEnv->chk_nb, 
						  A, lda, 
						  abftEnv->hrz_vd, abftEnv->hrz_vd_ld, 
						  abftEnv->hrz_recal_chk, abftEnv->hrz_recal_chk_ld, 
						  abftEnv->stream);
}


//col-read-A
//col-read-B
//non-col-write-C
//combined
void col_checksum_kernel_ccnc1(int m, int n, int chk_nb, 
			   	 		   double * A, int lda, 
			     		   double * vrt_vd, int vrt_vd_ld, 
			     		   double * hrz_chk, int hrz_chk_ld, 
			     		   magma_queue_t * stream) {


	// testing
	// cublasHandle_t handle;
	// cublasCreate(&handle);
	// cublasSetStream(handle, stream[2]);

	// double one = 1;
	// double zero = 0;

	// cublasOperation_t T = CUBLAS_OP_T;
	// cublasOperation_t N = CUBLAS_OP_N;
	// cublasStatus_t result;
	// result = cublasDgemm(handle,
	// 			T, N,
	// 			2, 15360, 512,
	// 			&one, A, lda,
	// 			A, lda,
	// 			&zero, A, lda);	
	// if (result == CUBLAS_STATUS_SUCCESS) {
	// 	cout << "cublas ok" << endl;
	// } else if (result == CUBLAS_STATUS_NOT_INITIALIZED) {
	// 	cout << "CUBLAS_STATUS_NOT_INITIALIZED" << endl;
	// } else if (result == CUBLAS_STATUS_INVALID_VALUE) {
	// 	cout << "CUBLAS_STATUS_INVALID_VALUE" << endl;
	// } else if (result == CUBLAS_STATUS_ARCH_MISMATCH) {
	// 	cout << "CUBLAS_STATUS_ARCH_MISMATCH" << endl;
	// } else if (result == CUBLAS_STATUS_EXECUTION_FAILED) {
	// 	cout << "CUBLAS_STATUS_EXECUTION_FAILED" << endl;
	// }
	cout << "size:" << m << "," << n << endl;
	 magmablasSetKernelStream(stream[1]);
	cudaError_t r;
	 for (int i = 0; i < m; i += chk_nb) {
		magma_dgemm(MagmaTrans, MagmaNoTrans,
					2, n, chk_nb,
					MAGMA_D_ONE, 
					vrt_vd, vrt_vd_ld,
					A + i, lda,
					MAGMA_D_ZERO, 
					hrz_chk + (i / chk_nb) * 2, hrz_chk_ld);		
	 }
	cudaStreamSynchronize(stream[1]);
	r = cudaGetLastError();
	r = cudaStreamSynchronize(stream[1]);
	if (r != cudaSuccess) {
	 	cout << "cuda sync error" << endl;
	 	magma_queue_t newStream;
	 	magma_queue_create( &newStream );
	 	stream[1] = newStream;
	} else {
	 	cout << "sync success" << endl;
	}
}




void col_chk_recal_10(ABFTEnv * abftEnv, double * A, int lda, int m, int n) {
	col_checksum_kernel_ccnc1(m, n, abftEnv->chk_nb, 
						  A, lda, 
						  abftEnv->vrt_vd, abftEnv->vrt_vd_ld, 
						  abftEnv->hrz_recal_chk, abftEnv->hrz_recal_chk_ld, 
						  abftEnv->stream);
}


//non-col-read-A
//col-read-B
//col-write-C
//combined
void col_checksum_kernel_nccc1(int m, int n, int chk_nb, 
			   	 		   double * A, int lda, 
			     		   double * hrz_vd, int hrz_vd_ld, 
			     		   double * vrt_chk, int vrt_chk_ld, 
			     		   magma_queue_t * stream) {

	magmablasSetKernelStream(stream[1]);
	for (int i = 0; i < m; i += chk_nb) {
		magma_dgemm(MagmaTrans, MagmaTrans,
					n, 2, chk_nb,
					MAGMA_D_ONE, 
					A + i, lda,
					hrz_vd, hrz_vd_ld,
					MAGMA_D_ZERO, 
					vrt_chk + (i / chk_nb) * 2 * vrt_chk_ld, vrt_chk_ld);		
	}
	
	cudaStreamSynchronize(stream[1]);
}


void col_chk_recal_11(ABFTEnv * abftEnv, double * A, int lda, int m, int n) {
	col_checksum_kernel_nccc1(m, n, abftEnv->chk_nb, 
						  A, lda, 
						  abftEnv->hrz_vd, abftEnv->hrz_vd_ld, 
						  abftEnv->vrt_recal_chk, abftEnv->vrt_recal_chk_ld, 
						  abftEnv->stream);
}

//col-read-A
//col-read-B
//col-write-C
//combined
void col_checksum_kernel_cccc1(int m, int n, int chk_nb, 
			   	 		   double * A, int lda, 
			     		   double * vrt_vd, int vrt_vd_ld, 
			     		   double * vrt_chk, int vrt_chk_ld, 
			     		   magma_queue_t * stream) {

	magmablasSetKernelStream(stream[1]);
	for (int i = 0; i < m; i += chk_nb) {
		magma_dgemm(MagmaTrans, MagmaNoTrans,
					n, 2, chk_nb,
					MAGMA_D_ONE, 
					A + i, lda,
					vrt_vd, vrt_vd_ld,
					MAGMA_D_ZERO, 
					vrt_chk + (i / chk_nb) * 2 * vrt_chk_ld, vrt_chk_ld);		
	}
	
	cudaStreamSynchronize(stream[1]);
}


void col_chk_recal_12(ABFTEnv * abftEnv, double * A, int lda, int m, int n) {
	col_checksum_kernel_cccc1(m, n, abftEnv->chk_nb, 
						  A, lda, 
						  abftEnv->vrt_vd, abftEnv->vrt_vd_ld, 
						  abftEnv->vrt_recal_chk, abftEnv->vrt_recal_chk_ld, 
						  abftEnv->stream);
}




void col_ChecksumRecalProfiler(ABFTEnv * abftEnv, double * A, int lda) {

	double gpu_time = 0.0;
	double min_time = 1000;
	int min_choice = 0;
	int num_choice = 8;
	int num_test = 10;
	for (int i = abftEnv->chk_nb; i < abftEnv->gpu_row; i += abftEnv->chk_nb) {
		cout << "[" << i << "]:	";
		for (int j = abftEnv->chk_nb; j < abftEnv->gpu_col; j += abftEnv->chk_nb) {
			cout << "[" << j << "]:	";
			for (int c = 1; c <= num_choice; c++) {
				gpu_time = magma_wtime();
				for (int t = 0; t < num_test; t++) {
					col_chk_recal_select(abftEnv, A, lda, i, j, c);
				}
				gpu_time = magma_wtime() - gpu_time;
				if (min_time > gpu_time) {
					min_time = gpu_time;
					min_choice = c;
				}
			}
			cout << min_choice << "	";
			abftEnv->col_mapping[(i / abftEnv->chk_nb) * abftEnv->col_mapping_ld + j /abftEnv->chk_nb] = min_choice;
		}
		cout << endl;
	}
}

void col_chk_recal_select(ABFTEnv * abftEnv, double * A, int lda, int m, int n, int select) {

		switch(select) {
			case 1: col_chk_recal_1(abftEnv, A, lda, m, n);
					break;
			case 2:	col_chk_recal_2(abftEnv, A, lda, m, n);
					break;
			case 3: col_chk_recal_3(abftEnv, A, lda, m, n);
					break;
			case 4: col_chk_recal_4(abftEnv, A, lda, m, n);
					break;
			case 5: col_chk_recal_5(abftEnv, A, lda, m, n);
					break;	
			case 6: col_chk_recal_6(abftEnv, A, lda, m, n);
					break;	
			case 7: col_chk_recal_7(abftEnv, A, lda, m, n);
					break;		
			case 8: col_chk_recal_8(abftEnv, A, lda, m, n);
					break;
			case 9: col_chk_recal_9(abftEnv, A, lda, m, n);
					break;	
			case 10: col_chk_recal_10(abftEnv, A, lda, m, n);
					break;	
			case 11: col_chk_recal_11(abftEnv, A, lda, m, n);
					break;		
			case 12: col_chk_recal_12(abftEnv, A, lda, m, n);
					break;
			default: cout << "selecting error" << endl;
		}
}


void at_col_chk_recal(ABFTEnv * abftEnv, double * A, int lda, int m, int n){

	// needs to do boundary check first
	//int i = abftEnv->col_mapping[(m / abftEnv->chk_nb) * abftEnv->col_mapping_ld + (n / abftEnv->chk_nb)];
	//i = 7;
	//col_chk_recal_select(abftEnv, A, lda, m, n, i);
	col_chkenc(A, lda, m, n, abftEnv->chk_nb, abftEnv->hrz_recal_chk, abftEnv->hrz_recal_chk_ld, abftEnv->stream[1]);
	//cudaStreamSynchronize(*(abftEnv->stream));

}


void col_benchmark_single(ABFTEnv * abftEnv, double * A, int lda){
	cout << "start banchmarking:" << endl;
	double benchmark_time = 0;

	double * test_chk1;
	size_t test_chk1_pitch = magma_roundup((abftEnv->gpu_row / abftEnv->chk_nb) * 2 * sizeof(double), 32);
    int test_chk1_ld = test_chk1_pitch / sizeof(double);
    magma_dmalloc(&(test_chk1), test_chk1_pitch * abftEnv->gpu_col);
    cudaMemset2D(test_chk1, test_chk1_pitch, 0, (abftEnv->gpu_row / abftEnv->chk_nb) * 2 * sizeof(double), abftEnv->gpu_col);

    double * test_chk2;
	size_t test_chk2_pitch = magma_roundup((abftEnv->gpu_row / abftEnv->chk_nb) * 2 * sizeof(double), 32);
    int test_chk2_ld = test_chk2_pitch / sizeof(double);
    magma_dmalloc(&(test_chk2), test_chk2_pitch * abftEnv->gpu_col);
    cudaMemset2D(test_chk2, test_chk2_pitch, 0, (abftEnv->gpu_row / abftEnv->chk_nb) * 2 * sizeof(double), abftEnv->gpu_col);

	
	for (int i = abftEnv->chk_nb; i <= abftEnv->gpu_col; i += abftEnv->chk_nb) {
    //int i = abftEnv->chk_nb;
	//	for (int j = abftEnv->chk_nb; j < abftEnv->gpu_col; j += abftEnv->chk_nb) {
		long long flops = 3 * i * i;
		flops *= 100;
		flops /=1e9;
		double t1 =0;
		double t2 =0;
		cout << i << "\t";

			// benchmark_time = magma_wtime();
			// for (int t = 0; t < 100; t++) {
			// 	col_chk_recal_select(abftEnv, A, lda, abftEnv->chk_nb, i, 9);
			// }
			// benchmark_time = magma_wtime() - benchmark_time;
			// cout << benchmark_time << "\t";

			benchmark_time = magma_wtime();
			for (int t = 0; t < 1; t++) {
				col_chk_recal_select(abftEnv, A, lda, i, i, 1);
			}
			cudaStreamSynchronize(*(abftEnv->stream));
			benchmark_time = magma_wtime() - benchmark_time;
			cout << benchmark_time << "\t" << flops/benchmark_time << "\t";
			t1 =benchmark_time;

			// benchmark_time = magma_wtime();
			// for (int t = 0; t < 100; t++) {
			// 	col_chk_recal_select(abftEnv, A, lda, abftEnv->chk_nb, i, 7);
			// }
			// benchmark_time = magma_wtime() - benchmark_time;
			// cout << benchmark_time <<"\t" << (flops/benchmark_time)/1e9 << "\t";

			benchmark_time = magma_wtime();
			for (int t = 0; t < 1; t++) {

				// col_checksum_kernel_ncns2(abftEnv->chk_nb, i, abftEnv->chk_nb,
				// 			  A, lda, 
				// 			  abftEnv->hrz_vd, abftEnv->hrz_vd_ld, 
				// 			  test_chk2, test_chk2_ld, 
				// 			  abftEnv->stream);
				// cudaStreamSynchronize(*(abftEnv->stream));

				col_chkenc(A, lda, i, i, abftEnv->chk_nb, test_chk1, test_chk1_ld, 
							  *(abftEnv->stream));
				
			}
			cudaStreamSynchronize(*(abftEnv->stream));
			benchmark_time = magma_wtime() - benchmark_time;
			cout << benchmark_time << "\t" << flops/benchmark_time<< "\t";
			t2=benchmark_time;

			compareChk(test_chk1, test_chk1_ld, abftEnv->hrz_recal_chk, abftEnv->hrz_recal_chk_ld, (i/abftEnv->chk_nb)*2, i);

			cout << t1/t2 << "\t";

			cout << endl;


			 //printMatrix_gpu(A, lda,  i , i, 4, 4);

			 //printMatrix_gpu(abftEnv->hrz_recal_chk, abftEnv->hrz_recal_chk_ld,  2 , i, 2, 4);

			 //printMatrix_gpu(test_chk1, test_chk1_ld,  2 , i, 2, 4);
		

			


		}

//	}
	
	cout << "done benchmarking" << endl;
}

void col_benchmark(ABFTEnv * abftEnv, double * A, int lda){
	cout << "start banchmarking:" << endl;
	double benchmark_time = 0;

	for (int size = 5120; size <= 20480; size += 5120){
		cout << "size=" << size << endl;
	benchmark_time = magma_wtime();
	for (int i = abftEnv->chk_nb; i <= abftEnv->chk_nb; i += abftEnv->chk_nb) {

		for (int j = abftEnv->chk_nb; j <= size; j += abftEnv->chk_nb) {

			//at_col_chk_recal(abftEnv, A, lda, i, j);
			col_chk_recal_select(abftEnv, A, lda, i, j, 9);
		}

	}
	benchmark_time = magma_wtime() - benchmark_time;
	cout << "auto tuning time: " << benchmark_time << endl;



	benchmark_time = magma_wtime();
	for (int i = abftEnv->chk_nb; i <= abftEnv->chk_nb; i += abftEnv->chk_nb) {

		for (int j = abftEnv->chk_nb; j <= size; j += abftEnv->chk_nb) {

			col_chk_recal_select(abftEnv, A, lda, i, j, 1);
		}

	}
	benchmark_time = magma_wtime() - benchmark_time;
	cout << "diff location time: " << benchmark_time << endl;



	benchmark_time = magma_wtime();
	for (int i = abftEnv->chk_nb; i <= abftEnv->chk_nb; i += abftEnv->chk_nb) {

		for (int j = abftEnv->chk_nb; j <= size; j += abftEnv->chk_nb) {

			col_chk_recal_select(abftEnv, A, lda, i, j, 7);
		}

	}
	benchmark_time = magma_wtime() - benchmark_time;
	cout << "diff location time: " << benchmark_time << endl;

}


	cout << "done benchmarking" << endl;
}

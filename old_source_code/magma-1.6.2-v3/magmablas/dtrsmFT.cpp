#include"FT.h"
#include<iostream>
using namespace std;
//TRSM with FT on GPU using cuBLAS
/*
__global__ void detectAndCorrectForTrsm(double * B, int ldb, int n,
		double * chksumB1, int incB1, double * chksumB2, int incB2,
		double * chkB1, int incB1_2, double * chkB2, int incB2_2) {
	//determin the reponsisble column 
	int block = blockIdx.x;
	int col = threadIdx.x;
	double diff = abs(
			*(chkB1 + block + col * incB1_2)
					- *(chksumB1 + block + col * incB1));
	if (diff > 0.1) {
		double diff2 = abs(
				*(chkB2 + block + col * incB2_2)
						- *(chksumB2 + block + col * incB2));
		int row = (int) round(diff2 / diff) - 1;
		*(B + n * block + row + col * ldb) += *(chksumB1 + block + col * incB1)
				- *(chkB1 + block + col * incB1_2);
	}
}
*/
/*
 * m: number of row of B
 * n: number of col of B
 */

void dtrsmFT(int m, int n, double * A, int lda,
		double * B, int ldb, double * checksumB, int checksumB_ld,
		double * vd, int vd_ld,
		double * chk1, int chk1_ld,
		double * chk2, int chk2_ld, 
		magma_queue_t * streams, bool FT, bool DEBUG) {


	double alpha = 1;
	
	magmablasSetKernelStream(streams[1]);
	magma_dtrsm(MagmaRight, MagmaLower, MagmaTrans, MagmaNonUnit,
	                                m, n,
	                                MAGMA_D_ONE, A, lda,
	                                       B, ldb);

	if (FT) {
		//recalculate checksum
		double beta = 0;
		for (int i = 0; i < m; i += n) {
			magmablasSetKernelStream(streams[2]);
			magma_dgemv(MagmaTrans, n, n, MAGMA_D_ONE,
					B + i, ldb, vd, vd_ld, MAGMA_D_ZERO, chk1 + (i / n), chk1_ld );
			magmablasSetKernelStream(streams[3]);
			magma_dgemv(MagmaTrans, n, n, MAGMA_D_ONE,
					B + i, ldb, vd + 1, vd_ld, MAGMA_D_ZERO, chk2 + (i / n), chk2_ld );

		}
		
		//update checksum
		magmablasSetKernelStream(streams[4]);
		magma_dtrsm(MagmaRight, MagmaLower, MagmaTrans, MagmaNonUnit,
			                                (m / n) * 2, n,
			                                MAGMA_D_ONE, A, lda,
			                                checksumB, checksumB_ld);


		if (DEBUG) {
			cout<<"recalculated checksum of B after dtrsm:"<<endl;
			printMatrix_gpu(chk1,chk1_ld, (m / n), n);
			printMatrix_gpu(chk2,chk2_ld, (m / n), n);

			cout<<"updated checksum of B after dtrsm:"<<endl;
			printMatrix_gpu(checksumB, checksumB_ld, (m / n) * 2, n);
		}
		/*detectAndCorrectForTrsm<<<dim3(m/n),dim3(n)>>>(B, ldb, n,
			checksumB1, incB1, checksumB2, incB2,
			chk1, chk1_ld, chk2, chk2_ld);
		*/
	}
}
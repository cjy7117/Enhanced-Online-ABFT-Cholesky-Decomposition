#include"FT.h"
#include<iostream>
using namespace std;
//dgemm with FT

/**
 * m: number of row of A
 * n: number of col of B
 * k: number of col of A / row of B
 */
void dgemmFT(int m, int n, int k, double * A, int lda,
		double * B, int ldb, double * C, int ldc, 
		double * checksumA, int checksumA_ld,
		double * checksumC, int checksumC_ld,
		double * vd, int vd_ld,
		double * chk1, int chk1_ld, 
		double * chk2, int chk2_ld, 
		magma_queue_t * streams,
		bool FT, bool DEBUG) {


	//magma_queue_sync( streams[1] );
	magmablasSetKernelStream(streams[1]);
	magma_dgemm(
				MagmaNoTrans, MagmaTrans,
				m, n, k,
				MAGMA_D_ONE * (-1),
				A, lda, B, ldb,
				MAGMA_D_ONE,
				C, ldc );
	
	
	

	if(FT){	

		
		//recalculate checksum
		//magma_queue_sync( streams[1] );
		magmablasSetKernelStream(streams[1]);
		for (int i = 0; i < m; i += n) {
			//magmablasSetKernelStream(streams[2]);
			magma_dgemv(MagmaTrans, n, n, MAGMA_D_ONE,
					C + i, ldc, vd, vd_ld, MAGMA_D_ZERO, chk1 + (i / n), chk1_ld );
			//magmablasSetKernelStream(streams[3]);
			magma_dgemv(MagmaTrans, n, n, MAGMA_D_ONE,
					C + i, ldc, vd + 1, vd_ld, MAGMA_D_ZERO, chk2 + (i / n), chk2_ld );
		}
		
		//update checksum				
		magmablasSetKernelStream(streams[1]);
		magma_dgemm(
					MagmaNoTrans, MagmaTrans,
					(m / n) * 2, n, k,
					MAGMA_D_ONE * (-1),
					checksumA, checksumA_ld,
					B, ldb,
					MAGMA_D_ONE,
					checksumC, checksumC_ld );
		
	
		if (DEBUG) {
			cout<<"recalculated checksum of C after dgemm:"<<endl;
			printMatrix_gpu(chk1, chk1_ld, (m / n), n);
			printMatrix_gpu(chk2, chk2_ld, (m / n), n);
		
			cout<<"updated checksum of C after dgemm:"<<endl;
			printMatrix_gpu(checksumC, checksumC_ld, (m / n) * 2, n);
		}
		
		//magma_queue_sync( streams[2] );
		//magma_queue_sync( streams[3] );
		//magma_queue_sync( streams[4] );

		//error detection and error correction
		ErrorDetectAndCorrect(C, ldc, n, m, n,
				checksumC, checksumC_ld,
				chk1, chk1_ld,
				chk2, chk2_ld,
				streams[1]);
				
		
	}
}
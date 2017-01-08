#include "FT.h"
#include <iostream>

using namespace std;

//QR Factorization with FT on CPU using ACML
//m: number of row
//n: number of col


void dgeqrfFT( int m, int n, double * A, int lda, double * tau, double * work, int lwork, int * info,
			   ABFTEnv * abftEnv, 
			   bool FT , bool DEBUG, bool CHECK_BEFORE, bool CHECK_AFTER) {

	if (FT && CHECK_BEFORE) {

	}

	if (DEBUG) {
		cout << "[DGEQRF] input matrix before factorization" << endl;
		printMatrix_host(A, lda, m, n, 4, 4);

		cout << "[DGEQRF] column checksum before factorization" << endl;
		printMatrix_host(abftEnv->col_hchk, abftEnv->col_hchk_ld, (m / abftEnv->chk_nb) * 2, n, 2, 4);

		cout << "[DGEQRF] row checksum before factorization" << endl;
		printMatrix_host(abftEnv->row_hchk, abftEnv->row_hchk_ld, m , (n / abftEnv->chk_nb) * 2, 4, 2);

	}

	// double * A2 = new double[lda*n];
	// memcpy(A2, A, lda*n*sizeof(double));

	// cout << "before1" << endl;
	// printMatrix_host(A2, lda, m, n, 4, 4);
	// lapackf77_dgeqrf(&m, &n, A2, &lda, tau, work, &lwork, info);
	// cout << "after1" << endl;
	// printMatrix_host(A2, lda, m, n, 4, 4);


	
	cout << "before2" << endl;
	printMatrix_host(A, lda, m, n, 4, 4);

	int k = min(m, n);
	for (int i = 0; i < k; i++) {
		cout << "i=" << i <<endl;



		int m2 = m - i;
		//int n2 = n - i - 1;
		int n2 = n - i;
		int incx = 1;
		double Aii = *(A + i * lda + i);
		double * v = new double[m2];
		memcpy(v+1, A + i * lda + i + 1, (m2-1)* sizeof(double));

		// lapackf77_dlarfg(&m2,
  //                        A + i * lda + i,
  //                        A + i * lda + i + 1, &incx,
  //                        tau + i);

		lapackf77_dlarfg(&m2,
                 &Aii,
                 v+1, &incx,
                 tau + i);
		v[0] = 1;

		// double t = *(tau + i);
		// double b = *(A + i * lda + i);
		// double scale = 1/(-(t * b));
		// cout << "scale:" << 1/(-(t * b)) <<endl;

		// int n3 = n-i;
		// double alpha = -1;
		// blasf77_daxpy(&n3,
  //                     &alpha,
  //                     A + i * lda + i, &lda,
  //                     abftEnv->col_hchk + i * abftEnv->col_hchk_ld, &abftEnv->col_hchk_ld);

		// alpha = -1 * (i+1);
		// blasf77_daxpy(&n3,
  //                     &alpha,
  //                     A + i * lda + i, &lda,
  //                     abftEnv->col_hchk + i * abftEnv->col_hchk_ld + 1, &abftEnv->col_hchk_ld);

		// *(abftEnv->col_hchk + i * abftEnv->col_hchk_ld) -= 1 * Aii;
		// *(abftEnv->col_hchk + i * abftEnv->col_hchk_ld + 1) -= (i + 1) * Aii;

		// *(abftEnv->col_hchk + i * abftEnv->col_hchk_ld) *=scale;
		// *(abftEnv->col_hchk + i * abftEnv->col_hchk_ld + 1) *= scale;

		// int n4 = 2;
		// int incxx = 1;
		// blasf77_dscal(&n4,
  //                     &scale,
  //                     abftEnv->col_hchk + i * abftEnv->col_hchk_ld, &incxx );




		// printMatrix_host(A, lda, m, n, 4, 4);

		// cout << "column checksum" << endl;
		// printMatrix_host(abftEnv->col_hchk, abftEnv->col_hchk_ld, (m / abftEnv->chk_nb) * 2, n, 2, 4);

		// double AII = *(A + i * lda + i);
		// *(A + i * lda + i) = 1;

		// *(abftEnv->col_hchk + i * abftEnv->col_hchk_ld) += 1;
		// *(abftEnv->col_hchk + i * abftEnv->col_hchk_ld + 1) += i + 1;


		// char L = 'L';
		// lapackf77_dlarf( &L, &m2, &n2,
  //                        A + i * lda + i, &incx,
  //                        tau + i,
  //                        A + (i + 1) * lda + i, &lda,
  //                        work );

		char L = 'L';
		lapackf77_dlarf( &L, &m2, &n2,
                         v, &incx,
                         tau + i,
                         A + i * lda + i, &lda,
                         work );

		// char T = 'T';
		// int two = 2;
		// int one = 1;
		// blasf77_dgemv( &T,
  //                    &m2, &two,
  //                    &one,
  //                    abftEnv->row_hchk, abftEnv->row_hchk_ld,
  //                    const double *x, const magma_int_t *incx,
  //                    const double *beta,
  //                          double *y, const magma_int_t *incy );

		// blasf77_dger(  const magma_int_t *m, const magma_int_t *n,
  //                    const double *alpha,
  //                    const double *x, const magma_int_t *incx,
  //                    const double *y, const magma_int_t *incy,
  //                          double *A, const magma_int_t *lda );

		// blasf77_dger(  const magma_int_t *m, const magma_int_t *n,
  //                    const double *alpha,
  //                    const double *x, const magma_int_t *incx,
  //                    const double *y, const magma_int_t *incy,
  //                          double *A, const magma_int_t *lda );


		// *(A + i * lda + i) = AII;
		// *(abftEnv->col_hchk + i * abftEnv->col_hchk_ld) -= 1;
		// *(abftEnv->col_hchk + i * abftEnv->col_hchk_ld + 1) -= i + 1;



		//printMatrix_host(A, lda, m, n, 4, 4);

	}

	cout << "after2" << endl;
	printMatrix_host(A, lda, m, n, 4, 4);

		



	//}

	//lapackf77_dgeqrf(&m, &n, A, &lda, tau, work, &lwork, info);

	if (FT) {
		int k = min (m, n);
		char L = 'L';

		for (int i = 0; i < k; i++) {

			//update row checksums
			double Aii = *(A + i * lda + i);
			*(A + i * lda + i) = 1;

			int pm = m - i;
			int pn = 2;
			int pincv = 1;

			// lapackf77_dlarf(&L, &pm, &pn,
   //                       	A + i * lda + i, &pincv,
   //                       	tau + i,
   //                       	abftEnv->row_hchk + i, &(abftEnv->row_hchk_ld),
   //                       	work );

			*(A + i * lda + i) = Aii;




			//update column checksums
			double c = 1 / (-1 * (*(tau + i) * Aii));
			int p2n = (m / abftEnv->chk_nb) * 2;
			int pincx = 1;
		//	blasf77_dscal(&p2n, &c, abftEnv->col_hchk + i * abftEnv->col_hchk_ld, &pincx);

			// int p3m = p2n - i;
			// int p3n = n - i - 1;
			// lapackf77_dlarf(&L, &pm, &pn,
   //                       	A + i * lda + i, &pincv,
   //                       	tau + i,
   //                       	abftEnv->row_hchk + i, &(abftEnv->row_hchk_ld),
   //                       	work );
			//construct v with column checksums
			// double * v = new double[m + 2];
			// int j = 0;
			// while (j < i) {
			// 	v[j] = 0.0;
			// 	j++;
			// }
			// v[j] = 1; //j = i
			// j++;
			// while (j < m) {
			// 	v[j] = *(A + i * lda + j);
			// 	j++;
			// }
			// v[j] = *(abftEnv->col_hchk + i);
			// v[j + 1] = *(abftEnv->col_hchk + i + 1);

			// cout << "[DGEQRF] v[" << i << "]:";
			// for (int k = 0; k < m + 2; k++)
			// 	cout << v[k] << "\t";
			// cout << endl;

		}

	}

	if (DEBUG) {
		cout << "[DGEQRF] input matrix after factorization" << endl;
		printMatrix_host(A, lda, m, n, 4, 4);

		cout << "[DGEQRF] TAU after factorization" << endl;
		for (int i = 0; i < n; i++)
			cout << tau[i] << "\t";
		cout << endl;

		cout << "[DGEQRF] column checksum after factorization" << endl;
		printMatrix_host(abftEnv->col_hchk, abftEnv->col_hchk_ld, (m / abftEnv->chk_nb) * 2, n, 2, 4);

		cout << "[DGEQRF] row checksum after factorization" << endl;
		printMatrix_host(abftEnv->row_hchk, abftEnv->row_hchk_ld, m , (n / abftEnv->chk_nb) * 2, 4, 2);
	}

	if (FT && CHECK_AFTER) {

	}



}
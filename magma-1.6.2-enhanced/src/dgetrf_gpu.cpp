                                                                                                                                                             /*
    -- MAGMA (version 1.6.1) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date January 2015

       @author Stan Tomov
       @generated from zgetrf_gpu.cpp normal z -> d, Fri Jan 30 19:00:14 2015
*/
#include "common_magma.h"
#include <iostream>
#include "FT.h"       

using namespace std;

/**
    Purpose
    -------
    DGETRF computes an LU factorization of a general M-by-N matrix A
    using partial pivoting with row interchanges.

    The factorization has the form
        A = P * L * U
    where P is a permutation matrix, L is lower triangular with unit
    diagonal elements (lower trapezoidal if m > n), and U is upper
    triangular (upper trapezoidal if m < n).

    This is the right-looking Level 3 BLAS version of the algorithm.
    
    If the current stream is NULL, this version replaces it with a new
    stream to overlap computation with communication.

    Arguments
    ---------
    @param[in]
    m       INTEGER
            The number of rows of the matrix A.  M >= 0.

    @param[in]
    n       INTEGER
            The number of columns of the matrix A.  N >= 0.

    @param[in,out]
    dA      DOUBLE_PRECISION array on the GPU, dimension (LDDA,N).
            On entry, the M-by-N matrix to be factored.
            On exit, the factors L and U from the factorization
            A = P*L*U; the unit diagonal elements of L are not stored.

    @param[in]
    ldda     INTEGER
            The leading dimension of the array A.  LDDA >= max(1,M).

    @param[out]
    ipiv    INTEGER array, dimension (min(M,N))
            The pivot indices; for 1 <= i <= min(M,N), row i of the
            matrix was interchanged with row IPIV(i).

    @param[out]
    info    INTEGER
      -     = 0:  successful exit
      -     < 0:  if INFO = -i, the i-th argument had an illegal value
                  or another error occured, such as memory allocation failed.
      -     > 0:  if INFO = i, U(i,i) is exactly zero. The factorization
                  has been completed, but the factor U is exactly
                  singular, and division by zero will occur if it is used
                  to solve a system of equations.

    @ingroup magma_dgesv_comp
    ********************************************************************/
extern "C" magma_int_t
magma_dgetrf_gpu(
    magma_int_t m, magma_int_t n,
    magmaDouble_ptr dA, magma_int_t ldda, magma_int_t *ipiv,
    magma_int_t *info)
{
    #define dAT(i_, j_) (dAT + (i_)*nb*lddat + (j_)*nb)

    #define CHK(i_, j_) (checksum + (i_)*nb*checksum_ld + (j_)*2)

    double c_one     = MAGMA_D_ONE;
    double c_neg_one = MAGMA_D_NEG_ONE;

    magma_int_t iinfo, nb;
    magma_int_t maxm, maxn, mindim;
    magma_int_t i, j, rows, cols, s, lddat, ldwork;
    double *dAT, *dAP, *work;

    /* Check arguments */
    *info = 0;
    if (m < 0)
        *info = -1;
    else if (n < 0)
        *info = -2;
    else if (ldda < max(1,m))
        *info = -4;

    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }

    /* Quick return if possible */
    if (m == 0 || n == 0)
        return *info;

    /* Function Body */
    mindim = min(m, n);
    nb     = magma_get_dgetrf_nb(m);

    /* debug use only */
    //nb = 4;

    s      = mindim / nb;

    if (nb <= 1 || nb >= min(m,n)) {
        /* Use CPU code. */
        magma_dmalloc_cpu( &work, m * n );
        if ( work == NULL ) {
            *info = MAGMA_ERR_HOST_ALLOC;
            return *info;
        }
        magma_dgetmatrix( m, n, dA, ldda, work, m );
        lapackf77_dgetrf(&m, &n, work, &m, ipiv, info);
        magma_dsetmatrix( m, n, work, m, dA, ldda );
        magma_free_cpu(work);
    }
    else {
        /* Use hybrid blocked code. */
        maxm = ((m + 31)/32)*32;
        maxn = ((n + 31)/32)*32;

        if (MAGMA_SUCCESS != magma_dmalloc( &dAP, nb*maxm )) {
            *info = MAGMA_ERR_DEVICE_ALLOC;
            return *info;
        }

        // square matrices can be done in place;
        // rectangular requires copy to transpose
        if ( m == n ) {
            dAT = dA;
            lddat = ldda;
            magmablas_dtranspose_inplace( m, dAT, ldda );
        }
        else {
            lddat = maxn;  // N-by-M
            if (MAGMA_SUCCESS != magma_dmalloc( &dAT, lddat*maxm )) {
                magma_free( dAP );
                *info = MAGMA_ERR_DEVICE_ALLOC;
                return *info;
            }
            magmablas_dtranspose( m, n, dA, ldda, dAT, lddat );
        }

        ldwork = maxm;
        if (MAGMA_SUCCESS != magma_dmalloc_pinned( &work, ldwork*nb )) {
            magma_free( dAP );
            if ( ! (m == n))
                magma_free( dAT );
            *info = MAGMA_ERR_HOST_ALLOC;
            return *info;
        }

        /* Define user stream if current stream is NULL */
        magma_queue_t stream[5];
        
        magma_queue_t orig_stream;
        magmablasGetKernelStream( &orig_stream );

        magma_queue_create( &stream[0] );
        magma_queue_create( &stream[2] );
        magma_queue_create( &stream[3] );
        magma_queue_create( &stream[4] );
        if (orig_stream == NULL) {
            magma_queue_create( &stream[1] );
            magmablasSetKernelStream(stream[1]);
        }
        else {
            stream[1] = orig_stream;
        }
  
        /* flags */
        bool FT = true;
        bool DEBUG = false;
        bool VERIFY = true;
        double * v;
        int v_ld;

        double * vd;
        int vd_ld;

        double * v2;
        int v2_ld;

        double * vd2;
        int vd2_ld;


        double * chk;
        int chk_ld;

        double * chk1d;
        double * chk2d;
        int chk1d_ld;
        int chk2d_ld;

        double * chk21d;
        double * chk22d;
        int chk21d_ld;
        int chk22d_ld;

        double * checksum;
        int checksum_ld;

        double * dAP_chk;
        int dAP_chk_ld;

        double * work_chk;
        int work_chk_ld;

        int * mapping;
        int mapping_ld;


        if (FT) {

            /* initialize checksum vectors on CPU */
            /* v =
             * 1 1 1 1
             * 1 2 3 4 
             */
            cout << "checksum vectors initialization on CPU......"; 
            magma_dmalloc_pinned(&v, nb * 2 * sizeof(double));
            v_ld = 2;
            for (int i = 0; i < nb; ++i) {
                *(v + i * v_ld) = 1;
            }
            for (int i = 0; i < nb; ++i) {
                *(v + i * v_ld + 1) = i+1;
            }
            if(DEBUG) {
                cout << "checksum vector on CPU:" << endl;
                printMatrix_host(v, v_ld, 2, nb);
            }
            cout << "done." << endl;



            /* initialize checksum vectors on GPU */
            cout << "checksum vectors initialization on GPU......";
            size_t vd_pitch = magma_roundup(2 * sizeof(double), 32);
            vd_ld = vd_pitch / sizeof(double);  
            magma_dmalloc(&vd, vd_pitch * nb);
            magma_dsetmatrix(2, nb, v, v_ld, vd, vd_ld);
            if(DEBUG) {
                cout << "checksum vector on GPU:" << endl;
                printMatrix_gpu(vd, vd_ld, nb, 2);
            }
            cout << "done." << endl;



            /* initialize checksum vectors on CPU */
            /* v2 =
             * 1 1 
             * 1 2  
             * 1 3
             * 1 4
             */
            cout << "checksum vectors initialization on CPU......"; 
            magma_dmalloc_pinned(&v2, nb * 2 * sizeof(double));
            v2_ld = nb;
            for (int i = 0; i < nb; ++i) {
                *(v2 + i) = 1;
            }
            for (int i = 0; i < nb; ++i) {
                *(v2 + v2_ld + i) = i+1;
            }
            if(DEBUG) {
                cout << "checksum vector on CPU:" << endl;
                printMatrix_host(v2, v2_ld, nb, 2);
            }
            cout << "done." << endl;


            /* initialize checksum vectors on GPU */
            cout << "checksum vectors initialization on GPU......";
            size_t vd2_pitch = magma_roundup(nb * sizeof(double), 32);
            vd2_ld = vd2_pitch / sizeof(double);  
            magma_dmalloc(&vd2, vd2_pitch * 2);
            magma_dsetmatrix(nb, 2, v2, v2_ld, vd2, vd2_ld);
            if(DEBUG) {
                cout << "checksum vector on GPU:" << endl;
                printMatrix_gpu(vd2, vd2_ld, nb, 2);
            }
            cout << "done." << endl;


            /* allocate space for update checksum on CPU */
            cout << "allocate space for checksum of work on CPU......";
            magma_dmalloc_pinned(&work_chk, maxm * 2 * sizeof(double));
            work_chk_ld = maxm;
            cout << "done." << endl;

             cout << "allocate space for recalculated checksum on GPU......";
            /* allocate space for reclaculated checksum on GPU */
            size_t chk1d_pitch = magma_roundup(2 * (n / nb) * sizeof(double), 32);
            chk1d_ld = chk1d_pitch / sizeof(double);
            magma_dmalloc(&chk1d, chk1d_pitch * m);
            
           
            size_t chk2d_pitch = magma_roundup(2 * (n / nb) * sizeof(double), 32);
            chk2d_ld = chk2d_pitch / sizeof(double);
            magma_dmalloc(&chk2d, chk2d_pitch * m);
            cout << "done." << endl;



            cout << "allocate space for recalculated checksum on GPU......";
            /* allocate space for reclaculated checksum on GPU */
            size_t chk21d_pitch = magma_roundup( m * sizeof(double), 32);
            chk21d_ld = chk21d_pitch / sizeof(double);
            magma_dmalloc(&chk21d, chk21d_pitch * 2 * (n / nb));
            
           
            size_t chk22d_pitch = magma_roundup(m * sizeof(double), 32);
            chk22d_ld = chk22d_pitch / sizeof(double);
            magma_dmalloc(&chk22d, chk22d_pitch * 2 * (n / nb));
            cout << "done." << endl;



            /* initialize checksums */
            cout << "checksums initiallization......";
            size_t checksum_pitch = magma_roundup((n / nb) * 2 * sizeof(double), 32);
            checksum_ld = checksum_pitch / sizeof(double);
            magma_dmalloc(&checksum, checksum_pitch * m);
            cudaMemset2D(checksum, checksum_pitch, 0, (n / nb) * 2 * sizeof(double), m);
            
            initializeChecksum(dAT, lddat, n, m, nb, vd, vd_ld, v, v_ld, checksum, checksum_ld, stream);
            if(DEBUG) {
                cout << "input matrix:" << endl;
                printMatrix_gpu(dAT, lddat, n, m);
                cout << "checksum matrix on GPU:" << endl;
                printMatrix_gpu(checksum, checksum_ld, (n / nb) * 2, m);

                recalculateChecksum(dAT, lddat,
                            n, m,
                            nb,
                            vd, vd_ld,
                            chk1d, chk1d_ld,
                            chk2d, chk2d_ld,
                            stream);

                cout<<"recalculated checksum:"<<endl;
                printMatrix_gpu(chk1d, chk1d_ld, n / nb, m);
                printMatrix_gpu(chk2d, chk2d_ld, n / nb, m);
            }
            cout << "done." << endl;

            /* allocate space for checksum of dAP */
            cout << "allocate space for checksum of dAP......";
            size_t dAP_chk_pitch = maxm * sizeof(double);
            dAP_chk_ld = maxm;
            magma_dmalloc(&dAP_chk, dAP_chk_pitch * 2);
            cout << "done." << endl;


        



        cout << "banchmarking:" << endl;
        mapping = new int[n * m];
        mapping_ld = n;
        ChecksumRecalProfiler(dAT, lddat,
               n, m, nb,
               vd, vd_ld,
               vd2, vd2_ld,
               chk1d, chk1d_ld, 
               chk2d, chk2d_ld, 
               chk21d, chk21d_ld, 
               chk22d, chk22d_ld, 
               stream,
               mapping, mapping_ld);

        benchmark(dAT, lddat,
               n, nb*2, nb,
               vd, vd_ld,
               vd2, vd2_ld,
               chk1d, chk1d_ld, 
               chk2d, chk2d_ld, 
               chk21d, chk21d_ld, 
               chk22d, chk22d_ld, 
               stream,
               mapping, mapping_ld);

        } 

        cout << "start computation" << endl;
        double comp_time = magma_wtime();
        for( j=0; j < s; j++ ) {
            
            // download j-th panel
            cols = maxm - j*nb;
            magmablas_dtranspose( nb, m-j*nb, dAT(j,j), lddat, dAP, cols );

            if (FT) {
                // also transpose checksums
                magmablas_dtranspose( 2, m-j*nb, CHK(j,j), checksum_ld, dAP_chk, dAP_chk_ld ); 
            }

            // make sure that the transpose has completed
            magma_queue_sync( stream[1] );

            magma_dgetmatrix_async( m-j*nb, nb, dAP, cols, work, ldwork,
                                    stream[0]);

            if (FT) {
                // also copy checksums to CPU
                magma_dgetmatrix_async( m-j*nb, 2, dAP_chk, dAP_chk_ld, work_chk, work_chk_ld,
                                        stream[0]);
            }

            if ( j > 0 && n - (j+1)*nb > 0 && m-j*nb > 0) {
                // magma_dtrsm( MagmaRight, MagmaUpper, MagmaNoTrans, MagmaUnit,
                //              n - (j+1)*nb, nb,
                //              c_one, dAT(j-1,j-1), lddat,
                //                     dAT(j-1,j+1), lddat );

                dtrsmFT( MagmaRight, MagmaUpper, MagmaNoTrans, MagmaUnit,
                             n - (j+1)*nb, nb,
                             c_one, dAT(j-1,j-1), lddat,
                             dAT(j-1,j+1), lddat,
                             nb,
                             nb,
                             CHK(j-1, j+1), checksum_ld,
                             vd, vd_ld,
                             vd2, vd2_ld,
                             chk1d, chk1d_ld, 
                             chk2d, chk2d_ld, 
                             chk21d, chk21d_ld, 
                             chk22d, chk22d_ld, 
                             FT, DEBUG, VERIFY, stream,
                             mapping, mapping_ld);


                // magma_dgemm( MagmaNoTrans, MagmaNoTrans,
                //              n-(j+1)*nb, m-j*nb, nb,
                //              c_neg_one, dAT(j-1,j+1), lddat,
                //                         dAT(j,  j-1), lddat,
                //              c_one,     dAT(j,  j+1), lddat );

    
                 dgemmFT( MagmaNoTrans, MagmaNoTrans,
                             n-(j+1)*nb, m-j*nb, nb,
                             c_neg_one, dAT(j-1,j+1), lddat,
                                        dAT(j,  j-1), lddat,
                             c_one,     dAT(j,  j+1), lddat,
                             nb,
                             nb,
                             CHK(j-1, j+1), checksum_ld,
                             CHK(j, j-1), checksum_ld,
                             CHK(j, j+1), checksum_ld,
                             vd, vd_ld,
                             vd2, vd2_ld,
                             chk1d, chk1d_ld, 
                             chk2d, chk2d_ld, 
                             chk21d, chk21d_ld, 
                             chk22d, chk22d_ld,
                             FT, DEBUG, VERIFY, stream,
                             mapping, mapping_ld );
            }

            // do the cpu part
            rows = m - j*nb;
            magma_queue_sync( stream[0] );
            //lapackf77_dgetrf( &rows, &nb, work, &ldwork, ipiv+j*nb, &iinfo);


            dgetrfFT(rows, nb, work, ldwork, ipiv+j*nb, &iinfo,
                     nb,
                     work_chk, work_chk_ld, v, v_ld, FT, DEBUG, VERIFY);

            if ( *info == 0 && iinfo > 0 )
                *info = iinfo + j*nb;

            // upload j-th panel
            magma_dsetmatrix_async( m-j*nb, nb, work, ldwork, dAP, maxm,
                                    stream[0]);

            if (FT) {

                // transfer checksums back to GPU.
                magma_dsetmatrix_async( m-j*nb, 2, work_chk, work_chk_ld, dAP_chk, dAP_chk_ld,
                                        stream[0]);
            }


            for( i=j*nb; i < j*nb + nb; ++i ) {
                ipiv[i] += j*nb;
            }

            magmablas_dlaswp( n, dAT, lddat, j*nb + 1, j*nb + nb, ipiv, 1 );

            if (FT) {
                // also do row swap on checksums
                magmablas_dlaswp( (n/nb)*2, checksum, checksum_ld, j*nb + 1, j*nb + nb, ipiv, 1 );
                if (DEBUG) {
                    cout<<"[ipiv] ipiv:"<<endl;
                    for (int i = 0; i < m; i++) {
                        cout << ipiv[i] << " ";
                    }
                    cout << endl;
                }

            }

            magma_queue_sync( stream[0] );

            magmablas_dtranspose( m-j*nb, nb, dAP, maxm, dAT(j,j), lddat );

            if (FT) {
                //transpose checksums back
                magmablas_dtranspose( m-j*nb, 2, dAP_chk, dAP_chk_ld, CHK(j, j), checksum_ld);
            }

            // do the small non-parallel computations (next panel update)
            if ( s > (j+1) ) {
                // magma_dtrsm( MagmaRight, MagmaUpper, MagmaNoTrans, MagmaUnit,
                //              nb, nb,
                //              c_one, dAT(j, j  ), lddat,
                //                     dAT(j, j+1), lddat);
                dtrsmFT( MagmaRight, MagmaUpper, MagmaNoTrans, MagmaUnit,
                         nb, nb, 
                         c_one, dAT(j, j  ), lddat,
                         dAT(j, j+1), lddat,
                         nb,
                         nb,
                         CHK(j, j+1), checksum_ld,
                         vd, vd_ld,
                         vd2, vd2_ld,
                         chk1d, chk1d_ld, 
                         chk2d, chk2d_ld, 
                         chk21d, chk21d_ld, 
                         chk22d, chk22d_ld,
                        FT, DEBUG, VERIFY, stream,
                        mapping, mapping_ld);
                
                // magma_dgemm( MagmaNoTrans, MagmaNoTrans,
                //              nb, m-(j+1)*nb, nb,
                //              c_neg_one, dAT(j,   j+1), lddat,
                //                         dAT(j+1, j  ), lddat,
                //              c_one,     dAT(j+1, j+1), lddat );

                dgemmFT( MagmaNoTrans, MagmaNoTrans,
                            nb, m-(j+1)*nb, nb,
                            c_neg_one,
                            dAT(j,   j+1), lddat,
                            dAT(j+1, j  ), lddat,
                            c_one,     dAT(j+1, j+1), lddat,
                            nb,
                            nb,
                            CHK(j, j+1), checksum_ld,
                            CHK(j+1, j), checksum_ld,
                            CHK(j+1, j+1), checksum_ld,
                             vd, vd_ld,
                             vd2, vd2_ld,
                             chk1d, chk1d_ld, 
                             chk2d, chk2d_ld, 
                             chk21d, chk21d_ld, 
                             chk22d, chk22d_ld,
                            FT, DEBUG, VERIFY, stream,
                            mapping, mapping_ld);

            }
            else if (n-s*nb > 0){
                // magma_dtrsm( MagmaRight, MagmaUpper, MagmaNoTrans, MagmaUnit,
                //              n-s*nb, nb,
                //              c_one, dAT(j, j  ), lddat,
                //                     dAT(j, j+1), lddat);

                dtrsmFT( MagmaRight, MagmaUpper, MagmaNoTrans, MagmaUnit,
                             n-s*nb, nb,
                             c_one, dAT(j, j  ), lddat,
                                    dAT(j, j+1), lddat,
                             nb,
                             nb,
                             CHK(j, j+1), checksum_ld,
                             vd, vd_ld,
                             vd2, vd2_ld,
                             chk1d, chk1d_ld, 
                             chk2d, chk2d_ld, 
                             chk21d, chk21d_ld, 
                             chk22d, chk22d_ld,
                             FT, DEBUG, VERIFY, stream,
                             mapping, mapping_ld);


                // magma_dgemm( MagmaNoTrans, MagmaNoTrans,
                //              n-(j+1)*nb, m-(j+1)*nb, nb,
                //              c_neg_one, dAT(j,   j+1), lddat,
                //                         dAT(j+1, j  ), lddat,
                //              c_one,     dAT(j+1, j+1), lddat );

                dgemmFT( MagmaNoTrans, MagmaNoTrans,
                             n-(j+1)*nb, m-(j+1)*nb, nb,
                             c_neg_one, dAT(j,   j+1), lddat,
                                        dAT(j+1, j  ), lddat,
                             c_one,     dAT(j+1, j+1), lddat,
                             nb,
                             nb,
                             CHK(j, j+1), checksum_ld,
                             CHK(j+1, j), checksum_ld,
                             CHK(j+1, j+1), checksum_ld,
                             vd, vd_ld,
                             vd2, vd2_ld,
                             chk1d, chk1d_ld, 
                             chk2d, chk2d_ld, 
                             chk21d, chk21d_ld, 
                             chk22d, chk22d_ld,
                             FT, DEBUG, VERIFY, stream,
                             mapping, mapping_ld);
            }
        }

        comp_time = magma_wtime() - comp_time;


        cout << "comp_time: " << comp_time << endl;

        magma_int_t nb0 = min(m - s*nb, n - s*nb);
        if ( nb0 > 0 ) {
            rows = m - s*nb;
            cols = maxm - s*nb;
    
            magmablas_dtranspose( nb0, rows, dAT(s,s), lddat, dAP, maxm );
            magma_dgetmatrix( rows, nb0, dAP, maxm, work, ldwork );
    
            // do the cpu part
            lapackf77_dgetrf( &rows, &nb0, work, &ldwork, ipiv+s*nb, &iinfo);
            if ( *info == 0 && iinfo > 0 )
                *info = iinfo + s*nb;
                
            for( i=s*nb; i < s*nb + nb0; ++i ) {
                ipiv[i] += s*nb;
            }
            magmablas_dlaswp( n, dAT, lddat, s*nb + 1, s*nb + nb0, ipiv, 1 );
    
            // upload j-th panel
            magma_dsetmatrix( rows, nb0, work, ldwork, dAP, maxm );
            magmablas_dtranspose( rows, nb0, dAP, maxm, dAT(s,s), lddat );
    
            magma_dtrsm( MagmaRight, MagmaUpper, MagmaNoTrans, MagmaUnit,
                         n-s*nb-nb0, nb0,
                         c_one, dAT(s,s),     lddat,
                                dAT(s,s)+nb0, lddat);
        }
        
        // undo transpose
        if ( m == n ) {
            magmablas_dtranspose_inplace( m, dAT, lddat );
        }
        else {
            magmablas_dtranspose( n, m, dAT, lddat, dA, ldda );
            magma_free( dAT );
        }

        magma_free( dAP );
        magma_free_pinned( work );
        
        magma_queue_destroy( stream[0] );
        if (orig_stream == NULL) {
            magma_queue_destroy( stream[1] );
        }
        magmablasSetKernelStream( orig_stream );
    }
    
    return *info;
} /* magma_dgetrf_gpu */

#undef dAT

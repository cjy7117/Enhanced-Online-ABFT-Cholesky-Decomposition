/*
    -- MAGMA (version 1.5.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date September 2014
  
       @generated from testing_zprint.cpp normal z -> d, Wed Sep 17 15:08:40 2014
       @author Mark Gates
*/
// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <cuda_runtime_api.h>
#include <cublas.h>

// includes, project
#include "flops.h"
#include "magma.h"
#include "magma_lapack.h"
#include "testings.h"


/* ////////////////////////////////////////////////////////////////////////////
   -- Testing dprint
*/
int main( int argc, char** argv)
{
    TESTING_INIT();

    double *hA, *dA;
    //magma_int_t ione     = 1;
    //magma_int_t ISEED[4] = {0,0,0,1};
    magma_int_t M, N, lda, ldda, size;
    magma_int_t status = 0;
    
    magma_opts opts;
    parse_opts( argc, argv, &opts );

    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            M     = opts.msize[itest];
            N     = opts.nsize[itest];
            lda   = M;
            ldda  = ((M + 31)/32)*32;
            size  = lda*N;

            /* Allocate host memory for the matrix */
            TESTING_MALLOC_CPU( hA, double, lda *N );
            TESTING_MALLOC_DEV( dA, double, ldda*N );
        
            //lapackf77_dlarnv( &ione, ISEED, &size, hA );
            for( int j = 0; j < N; ++j ) {
                for( int i = 0; i < M; ++i ) {
                    hA[i + j*lda] = MAGMA_D_MAKE( i + j*0.01, 0. );
                }
            }
            magma_dsetmatrix( M, N, hA, lda, dA, ldda );
            
            printf( "A=" );
            magma_dprint( M, N, hA, lda );
            
            printf( "dA=" );
            magma_dprint_gpu( M, N, dA, ldda );
            
            TESTING_FREE_CPU( hA );
            TESTING_FREE_DEV( dA );
        }
    }

    TESTING_FINALIZE();
    return status;
}

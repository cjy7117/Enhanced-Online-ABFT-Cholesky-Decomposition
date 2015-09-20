/*
    -- MAGMA (version 1.6.1) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date January 2015

       @generated from ztrtri_diag.cu normal z -> c, Fri Jan 30 19:00:10 2015

       @author Peng Du
       @author Tingxing Dong
       @author Mark Gates
       @author Azzam Haidar
       
       File named ctrtri_diag.cu to avoid name conflict with src/ctrtri.o
       in the library. The actual kernels are in ctrtri_lower.cu and ctrtri_upper.cu
*/

#include "common_magma.h"
#include "ctrtri.h"


/**
    Inverts the NB x NB diagonal blocks of a triangular matrix.
    This routine is used in ctrsm.
    
    Same as ctrtri_diag, but adds queue argument.
    
    @ingroup magma_cblas3
    ********************************************************************/
/**
    Purpose
    -------
    ctrtri_diag inverts the NB x NB diagonal blocks of A.

    Arguments
    ----------
    @param[in]
    uplo    magma_uplo_t.
            On entry, uplo specifies whether the matrix A is an upper or
            lower triangular matrix as follows:
      -     = MagmaUpper:  A is an upper triangular matrix.
      -     = MagmaLower:  A is a  lower triangular matrix.

    @param[in]
    diag    magma_diag_t.
            On entry, diag specifies whether or not A is unit triangular
            as follows:
      -     = MagmaUnit:     A is assumed to be unit triangular.
      -     = MagmaNonUnit:  A is not assumed to be unit triangular.

    @param[in]
    n       INTEGER.
            On entry, n specifies the order of the matrix A. N >= 0.

    @param[in]
    dA      COMPLEX array of dimension ( ldda, n )
            The triangular matrix A.
    \n
            If UPLO = 'U', the leading N-by-N upper triangular part of A
            contains the upper triangular matrix, and the strictly lower
            triangular part of A is not referenced.
    \n
            If UPLO = 'L', the leading N-by-N lower triangular part of A
            contains the lower triangular matrix, and the strictly upper
            triangular part of A is not referenced.
    \n
            If DIAG = 'U', the diagonal elements of A are also not referenced
            and are assumed to be 1.

    @param[in]
    ldda    INTEGER.
            The leading dimension of the array A.  LDDA >= max(1,N).

    @param[out]
    d_dinvA COMPLEX array of dimension (NB, ((n+NB-1)/NB)*NB),
            where NB = 128.
            On exit, contains inverses of the NB-by-NB diagonal blocks of A.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_cblas3
    ********************************************************************/
extern "C" void
magmablas_ctrtri_diag_q(
    magma_uplo_t uplo, magma_diag_t diag, magma_int_t n,
    magmaFloatComplex_const_ptr dA, magma_int_t ldda,
    magmaFloatComplex_ptr d_dinvA,
    magma_queue_t queue)
{
    magma_int_t info = 0;
    if (uplo != MagmaLower && uplo != MagmaUpper)
        info = -1;
    else if (diag != MagmaNonUnit && diag != MagmaUnit)
        info = -2;
    else if (n < 0)
        info = -3;
    else if (ldda < n)
        info = -5;

    if (info != 0) {
        magma_xerbla( __func__, -(info) );
        return;  //info
    }
    
    int nblocks = (n + IB - 1)/IB;

    cudaMemset( d_dinvA, 0, ((n+NB-1)/NB)*NB*NB * sizeof(magmaFloatComplex) );
    
    if ( uplo == MagmaLower ) {
        // invert diagonal IB x IB inner blocks
        ctrtri_diag_lower_kernel<<< nblocks, IB, 0, queue >>>( diag, n, dA, ldda, d_dinvA );

        // build up NB x NB blocks (assuming IB=16 here):
        // use   16 x 16  blocks to build  32 x 32  blocks,  1 x (1 x npages) grid,  4 x 4 threads;
        // then  32 x 32  blocks to build  64 x 64  blocks,  1 x (2 x npages) grid,  8 x 4 threads;
        // then  64 x 64  blocks to build 128 x 128 blocks,  1 x (4 x npages) grid, 16 x 4 threads;
        // then 128 x 128 blocks to build 256 x 256 blocks,  2 x (8 x npages) grid, 16 x 4 threads.
        for( int jb=IB; jb < NB; jb *= 2 ) {
            int kb = jb*2;
            int npages = (n + kb - 1)/kb;
            dim3 threads( (jb <= 32 ? jb/4 : 16), 4 );
            dim3 grid( jb/(threads.x*threads.y), npages*(jb/16) );  // emulate 3D grid: NX * (NY*npages), for CUDA ARCH 1.x
            
            //printf( "n %d, jb %d, grid %d x %d (%d x %d)\n", n, jb, grid.x, grid.y, grid.y / npages, npages );
            switch (jb) {
                case 16:
                    triple_cgemm16_part1_lower_kernel<<< grid, threads, 0, queue >>>( n, dA, ldda, d_dinvA, jb, npages );
                    triple_cgemm16_part2_lower_kernel<<< grid, threads, 0, queue >>>( n, dA, ldda, d_dinvA, jb, npages );
                    break;
                case 32:
                    triple_cgemm32_part1_lower_kernel<<< grid, threads, 0, queue >>>( n, dA, ldda, d_dinvA, jb, npages );
                    triple_cgemm32_part2_lower_kernel<<< grid, threads, 0, queue >>>( n, dA, ldda, d_dinvA, jb, npages );
                    break;
                case 64:
                    triple_cgemm64_part1_lower_kernel<<< grid, threads, 0, queue >>>( n, dA, ldda, d_dinvA, jb, npages );
                    triple_cgemm64_part2_lower_kernel<<< grid, threads, 0, queue >>>( n, dA, ldda, d_dinvA, jb, npages );
                    break;
                default:
                    triple_cgemm_above64_part1_lower_kernel<<< grid, threads, 0, queue >>>( n, dA, ldda, d_dinvA, jb, npages );
                    triple_cgemm_above64_part2_lower_kernel<<< grid, threads, 0, queue >>>( n, dA, ldda, d_dinvA, jb, npages );
                    triple_cgemm_above64_part3_lower_kernel<<< grid, threads, 0, queue >>>( n, dA, ldda, d_dinvA, jb, npages );
                    break;
            }
            if ( kb >= n ) break;
        }
    }
    else {
        ctrtri_diag_upper_kernel<<< nblocks, IB, 0, queue >>>( diag, n, dA, ldda, d_dinvA );

        // update the inverse up to the size of IB
        for( int jb=IB; jb < NB; jb*=2 ) {
            int kb = jb*2;
            int npages = (n + kb - 1)/kb;
            dim3 threads( (jb <= 32 ? jb/4 : 16), 4 );
            dim3 grid( jb/(threads.x*threads.y), npages*(jb/16) );  // emulate 3D grid: NX * (NY*npages), for CUDA ARCH 1.x
            
            switch (jb) {
                case 16:
                    triple_cgemm16_part1_upper_kernel<<< grid, threads, 0, queue >>>( n, dA, ldda, d_dinvA, jb, npages );
                    triple_cgemm16_part2_upper_kernel<<< grid, threads, 0, queue >>>( n, dA, ldda, d_dinvA, jb, npages );
                    break;
                case 32:
                    triple_cgemm32_part1_upper_kernel<<< grid, threads, 0, queue >>>( n, dA, ldda, d_dinvA, jb, npages );
                    triple_cgemm32_part2_upper_kernel<<< grid, threads, 0, queue >>>( n, dA, ldda, d_dinvA, jb, npages );
                    break;
                case 64:
                    triple_cgemm64_part1_upper_kernel<<< grid, threads, 0, queue >>>( n, dA, ldda, d_dinvA, jb, npages );
                    triple_cgemm64_part2_upper_kernel<<< grid, threads, 0, queue >>>( n, dA, ldda, d_dinvA, jb, npages );
                    break;
                default:
                    triple_cgemm_above64_part1_upper_kernel<<< grid, threads, 0, queue >>>( n, dA, ldda, d_dinvA, jb, npages );
                    triple_cgemm_above64_part2_upper_kernel<<< grid, threads, 0, queue >>>( n, dA, ldda, d_dinvA, jb, npages );
                    triple_cgemm_above64_part3_upper_kernel<<< grid, threads, 0, queue >>>( n, dA, ldda, d_dinvA, jb, npages );
                    break;
            }
            if ( kb >= n ) break;
        }
    }
}

/**
    @see magmablas_ctrtri_diag_q
    @ingroup magma_cblas3
    ********************************************************************/
extern "C" void
magmablas_ctrtri_diag(
    magma_uplo_t uplo, magma_diag_t diag, magma_int_t n,
    magmaFloatComplex_const_ptr dA, magma_int_t ldda,
    magmaFloatComplex_ptr d_dinvA)
{
    magmablas_ctrtri_diag_q( uplo, diag, n, dA, ldda, d_dinvA, magma_stream );
}

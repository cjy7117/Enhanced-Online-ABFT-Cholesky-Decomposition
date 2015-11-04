/*
    -- MAGMA (version 1.7.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date September 2015
       @author Adrien REMY

       @generated from zhetrs_nopiv_gpu.cpp normal z -> c, Fri Sep 11 18:29:29 2015

*/
#include "common_magma.h"

/**
    Purpose
    -------
    CHETRS solves a system of linear equations A*X = B with a complex
    Hermitian matrix A using the factorization A = U * D * U**H or
    A = L * D * L**H computed by CHETRF_NOPIV_GPU.
    
    Arguments
    ---------
    
    @param[in]
    uplo    magma_uplo_t
      -     = MagmaUpper:  Upper triangle of A is stored;
      -     = MagmaLower:  Lower triangle of A is stored.

    @param[in]
    n       INTEGER
            The order of the matrix A.  N >= 0.

    @param[in]
    nrhs    INTEGER
            The number of right hand sides, i.e., the number of columns
            of the matrix B.  NRHS >= 0.

    @param[in]
    dA      COMPLEX array on the GPU, dimension (LDA,N)
            The block diagonal matrix D and the multipliers used to
            obtain the factor U or L as computed by CHETRF_NOPIV_GPU.

    @param[in]
    ldda    INTEGER
            The leading dimension of the array A.  LDA >= max(1,N).

    @param[in,out]
    dB      COMPLEX array on the GPU, dimension (LDB,NRHS)
            On entry, the right hand side matrix B.
            On exit, the solution matrix X.

    @param[in]
    lddb    INTEGER
            The leading dimension of the array B.  LDB >= max(1,N).

    @param[out]
    info    INTEGER
      -     = 0:  successful exit
      -     < 0:  if INFO = -i, the i-th argument had an illegal value

    @ingroup magma_chesv_comp
    ********************************************************************/
extern "C" magma_int_t
magma_chetrs_nopiv_gpu(
    magma_uplo_t uplo, magma_int_t n, magma_int_t nrhs,
    magmaFloatComplex_ptr dA, magma_int_t ldda,
    magmaFloatComplex_ptr dB, magma_int_t lddb,
    magma_int_t *info)
{
    magmaFloatComplex c_one = MAGMA_C_ONE;

    int                upper = (uplo == MagmaUpper);
    *info = 0;
    if (! upper && uplo != MagmaLower) {
        *info = -1;
    } else if (n < 0) {
        *info = -2;
    } else if (nrhs < 0) {
        *info = -3;
    } else if (ldda < max(1,n)) {
        *info = -5;
    } else if (lddb < max(1,n)) {
        *info = -7;
    }
    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }

    /* Quick return if possible */
    if (n == 0 || nrhs == 0) {
        return *info;
    }


    if (upper) {
        magma_ctrsm( MagmaLeft, MagmaUpper,
                     MagmaConjTrans, MagmaUnit,
                     n, nrhs, c_one,
                     dA, ldda, dB, lddb );
        magmablas_clascl_diag(MagmaUpper, n, nrhs, dA, ldda, dB,lddb, info);
        //for (int i = 0; i < nrhs; i++)
        //    magmablas_clascl_diag(MagmaUpper, 1, n, dA, ldda, dB+(lddb*i),1, info);
        magma_ctrsm( MagmaLeft, MagmaUpper,
                     MagmaNoTrans, MagmaUnit,
                     n, nrhs, c_one,
                     dA, ldda, dB, lddb );
    } else {
        magma_ctrsm( MagmaLeft, MagmaLower,
                     MagmaNoTrans, MagmaUnit,
                     n, nrhs, c_one,
                     dA, ldda, dB, lddb );
        magmablas_clascl_diag(MagmaUpper, n, nrhs, dA, ldda, dB,lddb, info);
        //for (int i = 0; i < nrhs; i++)
        //    magmablas_clascl_diag(MagmaLower, 1, n, dA, ldda, dB+(lddb*i),1, info);
        magma_ctrsm( MagmaLeft, MagmaLower,
                     MagmaConjTrans, MagmaUnit,
                     n, nrhs, c_one,
                     dA, ldda, dB, lddb );
    }
    
    return *info;
}

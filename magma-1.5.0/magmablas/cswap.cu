/*
    -- MAGMA (version 1.5.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date September 2014

       @generated from zswap.cu normal z -> c, Wed Sep 17 15:08:23 2014

*/
#include "common_magma.h"

#define BLOCK_SIZE 64

/*********************************************************
 *
 * SWAP BLAS: permute to set of N elements
 *
 ********************************************************/
/*
 *  First version: line per line
 */
typedef struct {
    magmaFloatComplex *A1;
    magmaFloatComplex *A2;
    int n, lda1, lda2;
} magmagpu_cswap_params_t;

__global__ void magmagpu_cswap( magmagpu_cswap_params_t params )
{
    unsigned int x = threadIdx.x + blockDim.x*blockIdx.x;
    unsigned int offset1 = x*params.lda1;
    unsigned int offset2 = x*params.lda2;
    if( x < params.n )
    {
        magmaFloatComplex *A1  = params.A1 + offset1;
        magmaFloatComplex *A2  = params.A2 + offset2;
        magmaFloatComplex temp = *A1;
        *A1 = *A2;
        *A2 = temp;
    }
}


extern "C" void 
magmablas_cswap_q(
    magma_int_t n, magmaFloatComplex *dA1T, magma_int_t lda1, 
    magmaFloatComplex *dA2T, magma_int_t lda2,
    magma_queue_t queue )
{
    int blocksize = 64;
    dim3 blocks( (n+blocksize-1) / blocksize, 1, 1);
    magmagpu_cswap_params_t params = { dA1T, dA2T, n, lda1, lda2 };
    magmagpu_cswap<<< blocks, blocksize, 0, queue >>>( params );
}


extern "C" void 
magmablas_cswap(
    magma_int_t n, magmaFloatComplex *dA1T, magma_int_t lda1, 
    magmaFloatComplex *dA2T, magma_int_t lda2)
{
    magmablas_cswap_q( n, dA1T, lda1, dA2T, lda2, magma_stream );
}

/*
    -- MAGMA (version 1.6.2) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date May 2015

       @generated from ziterilu.cu normal z -> c, Sun May  3 11:22:58 2015

*/
#include "common_magmasparse.h"

#define PRECISION_c


__global__ void 
magma_citerilu_csr_kernel(   
    magma_int_t num_rows, 
    magma_int_t nnz,  
    magma_index_t *rowidxA, 
    magma_index_t *colidxA,
    const magmaFloatComplex * __restrict__ A, 
    magma_index_t *rowptrL, 
    magma_index_t *colidxL, 
    magmaFloatComplex *valL, 
    magma_index_t *rowptrU, 
    magma_index_t *rowidxU, 
    magmaFloatComplex *valU ){

    int i, j;
    int k = blockDim.x * blockIdx.x + threadIdx.x;


    magmaFloatComplex zero = MAGMA_C_MAKE(0.0, 0.0);
    magmaFloatComplex s, sp;
    int il, iu, jl, ju;
    

    if (k < nnz)
    {     

         
        i = rowidxA[k];
        j = colidxA[k];

#if (__CUDA_ARCH__ >= 350) && (defined(PRECISION_d) || defined(PRECISION_s))
        s =  __ldg( A+k );
#else
        s =  A[k];
#endif

        il = rowptrL[i];
        iu = rowptrU[j];

        while (il < rowptrL[i+1] && iu < rowptrU[j+1])
        {
            sp = zero;
            jl = colidxL[il];
            ju = rowidxU[iu];

            // avoid branching
            sp = ( jl == ju ) ? valL[il] * valU[iu] : sp;
            s = ( jl == ju ) ? s-sp : s;
            il = ( jl <= ju ) ? il+1 : il;
            iu = ( jl >= ju ) ? iu+1 : iu;

        }
        // undo the last operation (it must be the last)
        s += sp;
        __syncthreads();
        
        if ( i>j )        // modify l entry
            valL[il-1] =  s / valU[rowptrU[j+1]-1];
        else{             // modify u entry
            valU[iu-1] = s;
        }
    }

}// kernel 





/**
    Purpose
    -------
    
    This routine iteratively computes an incomplete LU factorization.
    The idea is according to Edmond Chow's presentation at SIAM 2014.
    This routine was used in the ISC 2015 paper:
    E. Chow et al.: 'Study of an Asynchronous Iterative Algorithm
                     for Computing Incomplete Factorizations on GPUs'
 
    The input format of the matrix is Magma_CSRCOO for the upper and lower 
    triangular parts. Note however, that we flip col and rowidx for the 
    U-part.
    Every component of L and U is handled by one thread. 

    Arguments
    ---------

    @param[in]
    A           magma_c_matrix
                input matrix A determing initial guess & processing order

    @param[in][out]
    L           magma_c_matrix
                input/output matrix L containing the ILU approximation

    @param[in][out]
    U           magma_c_matrix
                input/output matrix U containing the ILU approximation
                
    @param[in]
    A_CSR       magma_c_matrix
                input/output matrix containing the IC approximation

    @ingroup magmasparse_cgegpuk
    ********************************************************************/

extern "C" magma_int_t
magma_citerilu_csr( 
    magma_c_matrix A,
    magma_c_matrix L,
    magma_c_matrix U,
    magma_queue_t queue ){
    
    int blocksize1 = 128;
    int blocksize2 = 1;

    int dimgrid1 = ( A.nnz + blocksize1 -1 ) / blocksize1;
    int dimgrid2 = 1;
    int dimgrid3 = 1;

    // Runtime API
    // cudaFuncCachePreferShared: shared memory is 48 KB
    // cudaFuncCachePreferEqual: shared memory is 32 KB
    // cudaFuncCachePreferL1: shared memory is 16 KB
    // cudaFuncCachePreferNone: no preference
    //cudaFuncSetCacheConfig(cudaFuncCachePreferShared);

    cudaDeviceSetCacheConfig( cudaFuncCachePreferL1 );

    dim3 grid( dimgrid1, dimgrid2, dimgrid3 );
    dim3 block( blocksize1, blocksize2, 1 );
    magma_citerilu_csr_kernel<<< grid, block, 0, magma_stream >>>
        ( A.num_rows, A.nnz, 
          A.rowidx, A.col, A.val, 
          L.row, L.col, L.val, 
          U.row, U.col, U.val );


    return MAGMA_SUCCESS;
}

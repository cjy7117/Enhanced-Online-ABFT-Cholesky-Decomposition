/*
    -- MAGMA (version 1.6.1) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       November 2013
       
       @author Azzam Haidar
       @author Tingxing Dong

       @generated from zgetrf_panel_batched.cpp normal z -> d, Fri Jan 30 19:00:19 2015
*/
#include "common_magma.h"



////////////////////////////////////////////////////////////////////////////////////////
extern "C" magma_int_t
magma_dgetrf_recpanel_batched(
    magma_int_t m, magma_int_t n, magma_int_t min_recpnb,    
    double** dA_array,    magma_int_t ldda,
    magma_int_t** dipiv_array, magma_int_t** dpivinfo_array,
    double** dX_array,    magma_int_t dX_length,
    double** dinvA_array, magma_int_t dinvA_length,
    double** dW1_displ, double** dW2_displ,  
    double** dW3_displ, double** dW4_displ,
    double** dW5_displ,
    magma_int_t *info_array, magma_int_t gbstep,  
    magma_int_t batchCount,  cublasHandle_t myhandle, magma_queue_t queue)
{

    //magma_int_t DEBUG = 3;
    // Quick return if possible
    if (m ==0 || n == 0) {
        return 0;
    }


    double **dA_displ  = NULL;
    magma_malloc((void**)&dA_displ,   batchCount * sizeof(*dA_displ));
    magma_int_t **dipiv_displ = NULL;
    magma_malloc((void**)&dipiv_displ, batchCount * sizeof(*dipiv_displ));
    
    magma_int_t panel_nb = n;
    if(panel_nb <= min_recpnb){
        //if(DEBUG>0)printf("calling bottom panel recursive with m=%d nb=%d\n",m,n);
        //  panel factorization
        //magma_ddisplace_pointers(dA_displ, dA_array, ldda, 0, 0, batchCount);
        magma_dgetf2_batched(m, panel_nb,
                           dA_array, ldda,
                           dW1_displ, dW2_displ, dW3_displ,
                           dipiv_array, info_array, gbstep, batchCount, myhandle, queue);
    }
    else{
        // split A over two [A A2]
        // panel on A1, update on A2 then panel on A1    
        magma_int_t n1 = n/2;
        magma_int_t n2 = n-n1;
        magma_int_t m1 = m;
        magma_int_t m2 = m-n1;
        magma_int_t p1 = 0;
        magma_int_t p2 = n1;
        // panel on A1
        //if(DEBUG>0)printf("calling recursive panel on A1 with m=%d nb=%d min_recpnb %d\n",m1,n1,min_recpnb);
        magma_ddisplace_pointers(dA_displ, dA_array, ldda, p1, p1, batchCount, queue); 
        magma_idisplace_pointers(dipiv_displ, dipiv_array, 1, p1, 0, batchCount, queue);
        magma_dgetrf_recpanel_batched(
                           m1, n1, min_recpnb,
                           dA_displ, ldda,
                           dipiv_displ, dpivinfo_array,
                           dX_array, dX_length,
                           dinvA_array, dinvA_length,
                           dW1_displ, dW2_displ,
                           dW3_displ, dW4_displ, dW5_displ,
                           info_array, gbstep, batchCount, myhandle, queue);

        // update A2
        //if(DEBUG>0)printf("calling TRSM  with             m=%d n=%d \n",m1,n2);
        
        // setup pivinfo 
        setup_pivinfo_batched(dpivinfo_array, dipiv_displ, m1, n1, batchCount, queue);
        magma_ddisplace_pointers(dW5_displ, dA_array, ldda, p1, p2, batchCount, queue); 
        magma_dlaswp_rowparallel_batched( n2, dW5_displ, ldda,
                           dX_array, n1,
                           0, n1,
                           dpivinfo_array, batchCount, queue);
        magmablas_dtrsm_outofplace_batched(MagmaLeft, MagmaLower, MagmaNoTrans, MagmaUnit, 1,
                              n1, n2,
                              MAGMA_D_ONE,
                              dA_displ,    ldda, // dA
                              dX_array,  n1, // dB
                              dW5_displ,   ldda, // dX
                              dinvA_array, dinvA_length,
                              dW1_displ,   dW2_displ, 
                              dW3_displ,   dW4_displ,
                              0, batchCount, queue);

        magma_ddisplace_pointers(dW1_displ, dA_array, ldda, p2, 0, batchCount, queue); 
        magma_ddisplace_pointers(dA_displ, dA_array, ldda, p2, p2, batchCount, queue); 

        //if(DEBUG>0)printf("calling update A2(%d,%d) -= A(%d,%d)*A(%d,%d)  with             m=%d n=%d k=%d ldda %d\n",p2,p2,p2,0,p1,p2,m2,n2,n1,ldda);

#if 0
        double neg_one = MAGMA_D_NEG_ONE;
        double one  = MAGMA_D_ONE;
        cublasDgemmBatched(myhandle, CUBLAS_OP_N, CUBLAS_OP_N, m2, n2, n1,
                                     &neg_one, (const double**) dW1_displ, ldda,
                                               (const double**) dW5_displ, ldda,
                                     &one,  dA_displ, ldda, batchCount );


#else

        magmablas_dgemm_batched( MagmaNoTrans, MagmaNoTrans, m2, n2, n1, 
                              MAGMA_D_NEG_ONE, dW1_displ, ldda, 
                              dW5_displ, ldda, 
                              MAGMA_D_ONE,  dA_displ, ldda, 
                              batchCount, queue);
#endif

        // panel on A2
        //if(DEBUG>0)printf("calling recursive panel on A2 with m=%d nb=%d min_recpnb %d\n",m2,n2,min_recpnb);
        magma_idisplace_pointers(dipiv_displ, dipiv_array, 1, p2, 0, batchCount, queue);
        magma_dgetrf_recpanel_batched(
                           m2, n2, min_recpnb,
                           dA_displ, ldda,
                           dipiv_displ, dpivinfo_array,
                           dX_array, dX_length,
                           dinvA_array, dinvA_length,
                           dW1_displ, dW2_displ,
                           dW3_displ, dW4_displ, dW5_displ,
                           info_array, gbstep+p2, batchCount, myhandle, queue);

        // setup pivinfo
        setup_pivinfo_batched(dpivinfo_array, dipiv_displ, m2, n2, batchCount, queue);
        adjust_ipiv_batched(dipiv_displ, n2, n1, batchCount, queue);
        
        magma_ddisplace_pointers(dW1_displ, dA_array, ldda, p2, 0, batchCount, queue); // no need since it is above
        magma_dlaswp_rowparallel_batched( n1, dW1_displ, ldda,
                           dW1_displ, ldda,
                           n1, n,
                           dpivinfo_array, batchCount, queue);

        
    }

    magma_free(dA_displ);
    magma_free(dipiv_displ);
    return 0;
}


////////////////////////////////////////////////////////////////////////////////////////


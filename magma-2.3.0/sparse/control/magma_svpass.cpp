/*
    -- MAGMA (version 2.3.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date November 2017

       @generated from sparse/control/magma_zvpass.cpp, normal z -> s, Wed Nov 15 00:34:25 2017
       @author Hartwig Anzt
*/

//  in this file, many routines are taken from
//  the IO functions provided by MatrixMarket

#include "magmasparse_internal.h"


/**
    Purpose
    -------

    Passes a vector to MAGMA.

    Arguments
    ---------

    @param[in]
    m           magma_int_t
                number of rows

    @param[in]
    n           magma_int_t
                number of columns

    @param[in]
    val         float*
                array containing vector entries

    @param[out]
    v           magma_s_matrix*
                magma vector
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_saux
    ********************************************************************/

extern "C"
magma_int_t
magma_svset(
    magma_int_t m, magma_int_t n,
    float *val,
    magma_s_matrix *v,
    magma_queue_t queue )
{
    // make sure the target structure is empty
    magma_smfree( v, queue );
    
    v->num_rows = m;
    v->num_cols = n;
    v->nnz = m*n;
    v->memory_location = Magma_CPU;
    v->val = val;
    v->major = MagmaColMajor;
    v->storage_type = Magma_DENSE;

    return MAGMA_SUCCESS;
}


/**
    Purpose
    -------

    Passes a MAGMA vector back.

    Arguments
    ---------

    @param[in]
    v           magma_s_matrix
                magma vector

    @param[out]
    m           magma_int_t
                number of rows

    @param[out]
    n           magma_int_t
                number of columns

    @param[out]
    val         float*
                array containing vector entries

    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_saux
    ********************************************************************/

extern "C"
magma_int_t
magma_svget(
    magma_s_matrix v,
    magma_int_t *m, magma_int_t *n,
    float **val,
    magma_queue_t queue )
{
    magma_s_matrix v_CPU={Magma_CSR};
    magma_int_t info =0;
    
    if ( v.memory_location == Magma_CPU ) {
        *m = v.num_rows;
        *n = v.num_cols;
        *val = v.val;
    } else {
        CHECK( magma_smtransfer( v, &v_CPU, v.memory_location, Magma_CPU, queue ));
        CHECK( magma_svget( v_CPU, m, n, val, queue ));
    }
    
cleanup:
    return info;
}

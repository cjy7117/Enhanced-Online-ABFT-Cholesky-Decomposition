/*
    -- MAGMA (version 1.6.2) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date May 2015

       @generated from zjacobisetup.cu normal z -> d, Sun May  3 11:22:58 2015
       @author Hartwig Anzt

*/
#include "common_magmasparse.h"

#define BLOCK_SIZE 128


#define PRECISION_d

__global__ void 
dvjacobisetup_gpu(  int num_rows, 
                    int num_vecs,
                    double *b, 
                    double *d, 
                    double *c,
                    double *x){

    int row = blockDim.x * blockIdx.x + threadIdx.x ;

    if(row < num_rows ){
        for( int i=0; i<num_vecs; i++ ){
            c[row+i*num_rows] = b[row+i*num_rows] / d[row];
            x[row+i*num_rows] = c[row+i*num_rows];
        }
    }
}





/**
    Purpose
    -------

    Prepares the Jacobi Iteration according to
       x^(k+1) = D^(-1) * b - D^(-1) * (L+U) * x^k
       x^(k+1) =      c     -       M        * x^k.

    Returns the vector c. It calls a GPU kernel

    Arguments
    ---------

    @param[in]
    num_rows    magma_int_t
                number of rows
                
    @param[in]
    b           magma_d_matrix
                RHS b

    @param[in]
    d           magma_d_matrix
                vector with diagonal entries

    @param[out]
    c           magma_d_matrix*
                c = D^(-1) * b

    @param[out]
    x           magma_d_matrix*
                iteration vector
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_dgegpuk
    ********************************************************************/

extern "C" magma_int_t
magma_djacobisetup_vector_gpu(
    int num_rows, 
    magma_d_matrix b, 
    magma_d_matrix d, 
    magma_d_matrix c,
    magma_d_matrix *x,
    magma_queue_t queue )
{
    dim3 grid( magma_ceildiv( num_rows, BLOCK_SIZE ) );
    int num_vecs = b.num_rows / num_rows;
    magma_int_t threads = BLOCK_SIZE;
    dvjacobisetup_gpu<<< grid, threads, 0 >>>
                ( num_rows, num_vecs, b.dval, d.dval, c.dval, x->val );

    return MAGMA_SUCCESS;
}






__global__ void 
djacobidiagscal_kernel(  int num_rows,
                         int num_vecs, 
                    double *b, 
                    double *d, 
                    double *c){

    int row = blockDim.x * blockIdx.x + threadIdx.x ;

    if(row < num_rows ){
        for( int i=0; i<num_vecs; i++)
            c[row+i*num_rows] = b[row+i*num_rows] * d[row];
    }
}





/**
    Purpose
    -------

    Prepares the Jacobi Iteration according to
       x^(k+1) = D^(-1) * b - D^(-1) * (L+U) * x^k
       x^(k+1) =      c     -       M        * x^k.

    Returns the vector c. It calls a GPU kernel

    Arguments
    ---------

    @param[in]
    num_rows    magma_int_t
                number of rows
                
    @param[in]
    b           magma_d_matrix
                RHS b

    @param[in]
    d           magma_d_matrix
                vector with diagonal entries

    @param[out]
    c           magma_d_matrix*
                c = D^(-1) * b
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_d
    ********************************************************************/

extern "C" magma_int_t
magma_djacobi_diagscal(
    int num_rows, 
    magma_d_matrix d, 
    magma_d_matrix b, 
    magma_d_matrix *c,
    magma_queue_t queue )
{
    dim3 grid( magma_ceildiv( num_rows, BLOCK_SIZE ));
    int num_vecs = b.num_rows*b.num_cols/num_rows;
    magma_int_t threads = BLOCK_SIZE;
    djacobidiagscal_kernel<<< grid, threads, 0 >>>( num_rows, num_vecs, b.dval, d.dval, c->val );

    return MAGMA_SUCCESS;
}













__global__ void 
djacobiupdate_kernel(  int num_rows,
                       int num_cols, 
                    double *t, 
                    double *b, 
                    double *d, 
                    double *x){

    int row = blockDim.x * blockIdx.x + threadIdx.x ;

    if(row < num_rows ){
        for( int i=0; i<num_cols; i++)
            x[row+i*num_rows] += (b[row+i*num_rows]-t[row+i*num_rows]) * d[row];
    }
}


/**
    Purpose
    -------

    Updates the iteration vector x for the Jacobi iteration
    according to
        x=x+d.*(b-t)
    where d is the diagonal of the system matrix A and t=Ax.

    Arguments
    ---------

    @param[in]
    num_rows    magma_int_t
                number of rows
                
    @param[in]
    num_cols    magma_int_t
                number of cols
                
    @param[in]
    t           magma_d_matrix
                t = A*x
                
    @param[in]
    b           magma_d_matrix
                RHS b
                
    @param[in]
    d           magma_d_matrix
                vector with diagonal entries

    @param[out]
    x           magma_d_matrix*
                iteration vector
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_d
    ********************************************************************/

extern "C" magma_int_t
magma_djacobiupdate(
    magma_d_matrix t, 
    magma_d_matrix b, 
    magma_d_matrix d, 
    magma_d_matrix *x,
    magma_queue_t queue )
{

    dim3 grid( magma_ceildiv( t.num_rows, BLOCK_SIZE ));
    magma_int_t threads = BLOCK_SIZE;
    djacobiupdate_kernel<<< grid, threads, 0 >>>( t.num_rows, t.num_cols, t.dval, b.dval, d.dval, x->dval );

    return MAGMA_SUCCESS;
}










__global__ void 
djacobispmvupdate_kernel(  
    int num_rows,
    int num_cols, 
    double * dval, 
    magma_index_t * drowptr, 
    magma_index_t * dcolind,
    double *t, 
    double *b, 
    double *d, 
    double *x ){



    int row = blockDim.x * blockIdx.x + threadIdx.x ;
    int j;

    if(row<num_rows){
        double dot = MAGMA_D_ZERO;
        int start = drowptr[ row ];
        int end = drowptr[ row+1 ];
        for( int i=0; i<num_cols; i++){
            for( j=start; j<end; j++){
                dot += dval[ j ] * x[ dcolind[j]+i*num_rows ];
            }
            x[row+i*num_rows] += (b[row+i*num_rows]-dot) * d[row];
        }
    }
}





/**
    Purpose
    -------

    Updates the iteration vector x for the Jacobi iteration
    according to
        x=x+d.*(b-Ax)


    Arguments
    ---------

    @param[in]
    maxiter     magma_int_t
                number of Jacobi iterations   
                
    @param[in]
    A           magma_d_matrix
                system matrix
                
    @param[in]
    t           magma_d_matrix
                workspace
                
    @param[in]
    b           magma_d_matrix
                RHS b
                
    @param[in]
    d           magma_d_matrix
                vector with diagonal entries

    @param[out]
    x           magma_d_matrix*
                iteration vector
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_d
    ********************************************************************/

extern "C" magma_int_t
magma_djacobispmvupdate(
    magma_int_t maxiter,
    magma_d_matrix A,
    magma_d_matrix t, 
    magma_d_matrix b, 
    magma_d_matrix d, 
    magma_d_matrix *x,
    magma_queue_t queue )
{

    // local variables
    double c_zero = MAGMA_D_ZERO, c_one = MAGMA_D_ONE;
    dim3 grid( magma_ceildiv( t.num_rows, BLOCK_SIZE ));
    magma_int_t threads = BLOCK_SIZE;

    for( magma_int_t i=0; i<maxiter; i++ ) {
        // distinct routines imply synchronization
        // magma_d_spmv( c_one, A, *x, c_zero, t, queue );                // t =  A * x
        // djacobiupdate_kernel<<< grid, threads, 0 >>>( t.num_rows, t.num_cols, t.dval, b.dval, d.dval, x->dval );
        // merged in one implies asynchronous update
        djacobispmvupdate_kernel<<< grid, threads, 0 >>>
            ( t.num_rows, t.num_cols, A.dval, A.drow, A.dcol, t.dval, b.dval, d.dval, x->dval );

    }

    return MAGMA_SUCCESS;
}








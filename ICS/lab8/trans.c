/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 *
 * Student Name:Jin Ruiyang
 * Student ID:516030910408
 *
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);
void trans(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 *
 *     This function uses 12 local variables: i,j,a,b as iteration variables and t1-t8 as buffers.
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    int t1,t2,t3,t4,t5,t6,t7,t8;

    /*
     * test case 1: 32*32 
     * since the cache is 32*32 bytes, it can accommodate at most 8 lines of A or B without overlapping, so use 8*8 blocking.
     * in this case just divide those two matrices into 8*8 submatrices and transport submatrix of A to corrsponding submatrix of B one by one
     */
    if(32 == M && 32 == N)
    {
        for(int i = 0; i < N; i+=8)
        {
            for(int j = 0; j < M; j+=8)
            {
                for(int a = i; a < i+8; a++)
                {
                    int b = j;

                    // get a quarter of a line in A
                    t1 = A[a][b];
                    t2 = A[a][b+1];
                    t3 = A[a][b+2];
                    t4 = A[a][b+3];
                    t5 = A[a][b+4];
                    t6 = A[a][b+5];
                    t7 = A[a][b+6];
                    t8 = A[a][b+7];

                    // put the line in correct place in B
                    B[b][a] = t1;
                    B[b+1][a] = t2;
                    B[b+2][a] = t3;
                    B[b+3][a] = t4;
                    B[b+4][a] = t5;
                    B[b+5][a] = t6;
                    B[b+6][a] = t7;
                    B[b+7][a] = t8;
                }
            }
        }
    }

    /*
     * test case 2: 64*64
     * in this case the cache can only contain 4 lines of A or B, which means line x and line x+4 will be put in the same place and makes 8*8 blocking more likely to miss
     * to improve the situation, although 8*8 blocking is still used, the submatrices are first divided again into 4 4*4 parts
     */
    else if(64 == M && 64 == N)
    {
	    int t1,t2,t3,t4,t5,t6,t7,t8;
    	for(int i = 0; i < N; i += 8)
    	    {
            for(int j = 0; j < M; j += 8)
            {
                for(int a = i; a < i+4; a++)
                {
                    int b = j;

		            // first, move the top-left part as before
                    t1 = A[a][b];
                    t2 = A[a][b+1];
                    t3 = A[a][b+2];
                    t4 = A[a][b+3];

                    B[b][a] = t1;
                    B[b+1][a] = t2;
                    B[b+2][a] = t3;
                    B[b+3][a] = t4;

		            // then, move the top-right part of submatrix in A to the same part of the one in B via those buffer variables
                    // at the same time adjust its order
     		        // the idea is to use the top-right part of submatrix in B as a giant buffer 
    		        // and transfer that part of submatrix in A without switching between upper part and lower part repeatedly
   		            // which significantly reduce the number of miss in this case
                    t5 = A[a][b+4];
                    t6 = A[a][b+5];
                    t7 = A[a][b+6];
                    t8 = A[a][b+7];

                    B[b][a+4] = t5;
                    B[b+1][a+4] = t6;
                    B[b+2][a+4] = t7;
                    B[b+3][a+4] = t8;
                }
		        // next, move the buffered part in B to its place while moving the bottom-left part of the submatrix in A
                // on moving the buffered part, there are inevitable accesses jumping between the upper part and lower part in the submatrix in B
                // to reduce the cost as much as possible, move 4 elements in a row at a time 
                // so that there are only 7 switches during the movement
                for(int b = j; b < j+4; b++)
                {
                    int a = i+4;
                    t1 = A[a][b];
                    t2 = A[a+1][b];
                    t3 = A[a+2][b];
                    t4 = A[a+3][b];

                    t5 = B[b][a];
                    t6 = B[b][a+1];
                    t7 = B[b][a+2];
                    t8 = B[b][a+3];

                    B[b][a] = t1;
                    B[b][a+1] = t2;
                    B[b][a+2] = t3;
                    B[b][a+3] = t4;

                    B[b+4][a-4] = t5;
                    B[b+4][a-3] = t6;
                    B[b+4][a-2] = t7;
                    B[b+4][a-1] = t8;
                }
                // finally, move the bottom-right part like before
                for(int a = i+4; a < i+8; a++)
                {
                    int b = j+4;
                    t1 = A[a][b];
                    t2 = A[a][b+1];
                    t3 = A[a][b+2];
                    t4 = A[a][b+3];

                    B[b][a] = t1;
                    B[b+1][a] = t2;
                    B[b+2][a] = t3;
                    B[b+3][a] = t4;
                }
            }
        }
    }

    /*
     * test case 3: 61*67
     * since the constraint for this case is relatively loose, just use 16*16 blocking is enough to get full points
     * by the way I just can't understand why 16*16 blocking gets better performance than 8*8 blocking in this case
     */
    else if(61 == M && 67 == N)
    {
		for(int i = 0; i < N; i+=16)
        {
            for(int j = 0; j < M; j+=16)
            {
                for(int a = i; a < i+16 && a < N; a++)
                {
                    for(int b = j; b < j+16 && b < M; b++)
                    {
                        B[b][a] = A[a][b];
                    }
                }
            }
        }
    }
    else
    {
        trans(M, N, A, B);
    }
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    
}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc);
}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}


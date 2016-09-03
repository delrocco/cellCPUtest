// local
#include "../defines.h"
// cell
#include <spu_mfcio.h>
#include <spu_intrinsics.h>
//#include <libmisc.h>
// ansi
#include <stdio.h>
//#include <stdlib.h>

//=====================================
// CONSTANTS, MACROS, DEFINITIONS
//=====================================

#define TAG_READ_COL  0 // (idx)   double buffering
#define TAG_READ_COLX 1 // (idx^1) double buffering
#define TAG_READ_ROW  2
#define TAG_READ_ARGS 3
#define TAG_WRITE     4

//=====================================
// GLOBALS
//=====================================

volatile spu_argument_t args        __attribute__((aligned(16)));
volatile float row[MATRIX_WIDTH]    __attribute__((aligned(128))); // row of matrix A
volatile float col[2][MATRIX_WIDTH] __attribute__((aligned(128))); // col of matrix B (double buffered)
volatile float out[MATRIX_WIDTH]    __attribute__((aligned(128))); // row of matrix G

//=====================================
// FUNCTIONS
//=====================================

int main(unsigned long long __attribute__ ((unused)) id, unsigned long long argptr)
{
  unsigned int memsize;     // byte size of matrix row/column
  unsigned int i,j,k;       // loop counters
  unsigned int idx,idxnext; // double buffering

  // fetch program arguments structure
  mfc_get((void*)&args, (unsigned int)argptr, sizeof(args), TAG_READ_ARGS, 0,0);

  // init any variables
  memsize = sizeof(float)*MATRIX_WIDTH;

  // wait for program arguments fetch to finish
  mfc_write_tag_mask(1<<TAG_READ_ARGS);
  mfc_read_tag_status_all();

  // loop over rows of matrixA that this spu is interested in
  for (j=args.threadidx; j<MATRIX_WIDTH; j+=args.threadcnt)
  {
    // fetch appropriate row of input matrix A
    //dma_get((void*)row, (unsigned int)(args.matrixA+(j*MATRIX_WIDTH)), memsize, TAG_READ_ROW);
    mfc_get((void*)row, (unsigned int)(args.matrixA+(j*MATRIX_WIDTH)), memsize, TAG_READ_ROW, 0,0);

    // init vars
    idx = 0;

    // fetch first col of input matrix B (really a row because B is transposed)
    mfc_get((void*)col[idx], (unsigned int)args.matrixB, memsize, idx, 0,0);

    // wait for row fetch to finish
    mfc_write_tag_mask(1<<TAG_READ_ROW);
    mfc_read_tag_status_all();

    // loop over all columns in matrixB
    for (i=0; i<MATRIX_WIDTH-1; i++)
    {
      idxnext = idx^1;

      // fetch next col of input matrix B (really a row because B is transposed)
      //dma_get((void*)col, (unsigned int)(args.matrixB+(i*MATRIX_WIDTH)), memsize, TAG_READ_COL);
      mfc_get((void*)col[idxnext], (unsigned int)(args.matrixB+((i+1)*MATRIX_WIDTH)), memsize, idxnext, 0,0);

      // wait for previous column fetch to finish
      mfc_write_tag_mask(1<<idx);
      mfc_read_tag_status_all();

      // multiply row by col and sum the result
      out[i] = 0;
      for (k=0; k<MATRIX_WIDTH; k++)
        out[i] += row[k] * col[idx][k];

      // reassign double buffer index
      idx = idxnext;
    }

    // wait for last column fetch to finish
    mfc_write_tag_mask(1<<idx);
    mfc_read_tag_status_all();

    // multiply row by last col and sum the result
    out[i] = 0;
    for (k=0; k<MATRIX_WIDTH; k++)
      out[i] += row[k] * col[idx][k];

    // write back a whole row of results
    //dma_put((void*)out, (unsigned int)(args.matrixG+(j*MATRIX_WIDTH)), memsize, TAG_WRITE);
    mfc_write_tag_mask(1<<TAG_WRITE);
    mfc_read_tag_status_all();
    mfc_put((void*)out, (unsigned int)(args.matrixG+(j*MATRIX_WIDTH)), memsize, TAG_WRITE, 0,0);
  }

  return 0;
}




/* MEMSET FUNCTION
void *spu_memset(void *b, int c, unsigned int len)
{
  char *bb;
  for (bb = (char *)b; len--;) *bb++ = c;
  return (b);
}
*/

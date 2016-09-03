
// local
#include "defines.h"
// cell
#include <libspe2.h>
#include <pthread.h>
#include <libmisc.h>
#include <malloc_align.h>
#include <free_align.h>
// linux
#include <sys/times.h>
#include <time.h>
#include <unistd.h>
// ansi
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>


//=====================================
// CONSTANTS, MACROS, DEFINITIONS
//=====================================

typedef struct _spu_environment
{
  spe_context_ptr_t   program;
  spu_argument_t      arguments __attribute__((aligned(16)));
  pthread_t           pthread;
} spu_program_t;


//=====================================
// GLOBALS
//=====================================

extern spe_program_handle_t matrix_spu;


//=====================================
// FUNCTIONS
//=====================================

void *ppu_pthread_function(void *arg)
{
  spu_program_t *spu  = (spu_program_t*)arg;
  unsigned int entry  = SPE_DEFAULT_ENTRY;

  if (spe_context_run(spu->program, &entry, 0, &spu->arguments, NULL, NULL) < 0)
  {
    perror ("Failed running context");
    exit (1);
  }

  pthread_exit(NULL);
}

void ppu_matrix_multiply(float *C, const float *A, const float *B, unsigned int width)
{
  unsigned int i,j,k;
  double a,b,sum=0;
  for (i=0; i<width; i++)
  {
    for (j=0; j<width; j++)
    {
      sum = 0;
      for (k=0; k<width; k++)
      {
          a = A[i * width + k];
          b = B[k * width + j];
          sum += a * b;
      }
      C[i * width + j] = (float)sum;
    }
  }
}

int main()
{
  spu_program_t   spus[MAX_SPUS];
  float          *matrixA;  // input A
  float          *matrixB;  // input B
  float          *matrixG;  // output GPU
  float          *matrixC;  // output CPU
  unsigned int    numThreads=0;
  unsigned int    i;
  struct tms      timer;
	int             tstart;
	double          tgpu=0, tcpu=0;
	double          gflops=0;

  // setup matrices
  matrixA = (float*)malloc_align(sizeof(float)*(MATRIX_WIDTH*MATRIX_WIDTH), 7);
  matrixB = (float*)malloc_align(sizeof(float)*(MATRIX_WIDTH*MATRIX_WIDTH), 7);
  matrixG = (float*)malloc_align(sizeof(float)*(MATRIX_WIDTH*MATRIX_WIDTH), 7);
  matrixC = (float*)malloc_align(sizeof(float)*(MATRIX_WIDTH*MATRIX_WIDTH), 7);
  memset(matrixG,0,sizeof(float)*(MATRIX_WIDTH*MATRIX_WIDTH));
  memset(matrixC,0,sizeof(float)*(MATRIX_WIDTH*MATRIX_WIDTH));
  fillMatrix(matrixA, MATRIX_WIDTH, 1.0f, FILL_POS);
  fillMatrix(matrixB, MATRIX_WIDTH, 1.0f, FILL_POS);
  transposeMatrix(matrixB, MATRIX_WIDTH);

  // get available/useable number of threads
  numThreads = spe_cpu_info_get(SPE_COUNT_USABLE_SPES, -1);
  if (numThreads > MAX_SPUS) numThreads = MAX_SPUS;

  // setup spe threads
  for (i=0; i<numThreads; i++)
  {
    // create context pointer
    if ((spus[i].program = spe_context_create(0, NULL)) == NULL)
    {
      perror("Failed creating context");
      exit(1);
    }
    // assign program to context pointer
    if (spe_program_load(spus[i].program, &matrix_spu))
    {
      perror("Failed loading program");
      exit(1);
    }

    // fill in program arguments
    spus[i].arguments.threadcnt = numThreads;
    spus[i].arguments.threadidx = i;
    spus[i].arguments.matrixA   = matrixA;
    spus[i].arguments.matrixB   = matrixB;
    spus[i].arguments.matrixG   = matrixG;

    // create thread
    if (pthread_create(&spus[i].pthread, NULL, &ppu_pthread_function, &spus[i]))
    {
      perror("Failed creating thread");
      exit(1);
    }
  }

  // sync threads to wait for final execution
  tstart = times(&timer);
  for (i=0; i<numThreads; i++)
  {
    if (pthread_join(spus[i].pthread, NULL))
    {
      perror("Failed pthread_join");
      exit (1);
    }
    if (spe_context_destroy(spus[i].program) != 0)
    {
      perror("Failed destroying context");
      exit (1);
    }
  }
  tgpu = (double)(times(&timer) - tstart) / (double)sysconf(_SC_CLK_TCK);

#if DO_PPU
  transposeMatrix(matrixB, MATRIX_WIDTH);
  tstart = times(&timer);
  ppu_matrix_multiply(matrixC, matrixA, matrixB, MATRIX_WIDTH);
  tcpu = (double)(times(&timer) - tstart) / (double)sysconf(_SC_CLK_TCK);
#endif

  // report timings
  gflops = ((double)(2.0 * MATRIX_WIDTH * MATRIX_WIDTH * MATRIX_WIDTH))/((double)(1024 * 1024 * 1024));
  printf("PPU Time: %lfs\n", tcpu);
  printf("SPU Time: %lfs\n", tgpu);
  printf("Gigflops: %3.5lf\n", gflops/tgpu);
#if DO_PPU
#if DO_SPU
  //diffMatrix(matrixC, matrixG, MATRIX_WIDTH, 1.0);
#endif
#endif

#if PRINT_MATRIX
  printf("\n");
  dumpMatrix("A:\n", "%6.2f ", matrixA, MATRIX_WIDTH);
  dumpMatrix("B:\n", "%6.2f ", matrixB, MATRIX_WIDTH);
#if DO_PPU
  dumpMatrix("PPU:\n", "%9.2f ", matrixC, MATRIX_WIDTH);
#endif
#if DO_SPU
  dumpMatrix("SPUs:\n", "%9.2f ", matrixG, MATRIX_WIDTH);
#endif
#endif

  // cleanup
  free_align(matrixA);
  free_align(matrixB);
  free_align(matrixG);
  free_align(matrixC);

  return (0);
}

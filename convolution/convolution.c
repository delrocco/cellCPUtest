
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

extern spe_program_handle_t convolution_spu;


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

void ppu_convolution(float *c, const float *a, const float *h, unsigned int cWidth, unsigned int aWidth, unsigned int hWidth)
{
    unsigned int i,j,k;   // loop counter variables
    unsigned int m,n;     // idx into a image
    int ax,ay;            // idx into a image
    unsigned int cLength; // length of c image
    float sum;            // value of an element of c

    cLength = cWidth*cWidth;
    m = 0;
    n = 0;

    for (i=0; i<cLength; i++, m++)
    {
        sum = 0;
        if (i!=0 && (i%cWidth)==0) { m=0; n++; }

        for (k=0; k<hWidth; k++)
        {
            for (j=0; j<hWidth; j++)
            {
                ax = m+j;
                ay = n+k;
                sum += h[k*hWidth+j] * a[ay*aWidth+ax];
            }
        }

        c[i] = sum;
    }
}

int main()
{
  spu_program_t   spus[MAX_SPUS];
  unsigned int    numThreads=0;
  unsigned int    i;
  float          *imageA;   // input image A
  float          *filterH;  // input filter H
  float          *imageG;   // output image GPU
  float          *imageC;   // output image CPU
  struct tms      timer;
  int             tstart;
  double          tgpu=0, tcpu=0, gflops=0;

  // setup matrices
  imageA  = (float*)malloc_align(sizeof(float)*(IMAGE_PADDED*IMAGE_PADDED), 7);
  filterH = (float*)malloc_align(sizeof(float)*(FILTER_WIDTH*FILTER_WIDTH), 7);
  imageG  = (float*)malloc_align(sizeof(float)*(IMAGE_WIDTH*IMAGE_WIDTH), 7);
  imageC  = (float*)malloc_align(sizeof(float)*(IMAGE_WIDTH*IMAGE_WIDTH), 7);
  memset(imageA,0,sizeof(float)*(IMAGE_PADDED*IMAGE_PADDED));
  memset(imageG,0,sizeof(float)*(IMAGE_WIDTH*IMAGE_WIDTH));
  memset(imageC,0,sizeof(float)*(IMAGE_WIDTH*IMAGE_WIDTH));
  fillMatrixPadded(imageA, IMAGE_WIDTH, IMAGE_PADDED, 1.0f, FILL_CONST);
  fillMatrix(filterH, FILTER_WIDTH, 1.0f, FILL_CONST);

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
    if (spe_program_load(spus[i].program, &convolution_spu))
    {
      perror("Failed loading program");
      exit(1);
    }

    // fill in program arguments
    spus[i].arguments.threadcnt = numThreads;
    spus[i].arguments.threadidx = i;
    spus[i].arguments.imageA    = imageA;
    spus[i].arguments.filterH   = filterH;
    spus[i].arguments.imageG    = imageG;

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
  tstart = times(&timer);
  ppu_convolution(imageC, imageA, filterH, IMAGE_WIDTH, IMAGE_PADDED, FILTER_WIDTH);
  tcpu = (double)(times(&timer) - tstart) / (double)sysconf(_SC_CLK_TCK);
#endif

  // report timings
  gflops = ((double)(2.0 * IMAGE_WIDTH*IMAGE_WIDTH*FILTER_WIDTH*FILTER_WIDTH))/((double)(1024*1024*1024));
  printf("PPU Time: %lfs\n", tcpu);
  printf("SPU Time: %lfs\n", tgpu);
  printf("Gigflops: %3.5lf\n", gflops/tgpu);
#if DO_PPU
#if DO_SPU
  diffMatrix(imageC, imageG, IMAGE_WIDTH, 1.0);
#endif
#endif

#if PRINT_IMAGE
  printf("\n");
  //dumpMatrix("A:\n", "%7.2f ", imageA, IMAGE_PADDED);
  //dumpMatrixPadded("A:\n", "%7.2f ", imageA, IMAGE_WIDTH, IMAGE_PADDED);
  //dumpMatrix("H:\n", "%7.2f ", filterH, FILTER_WIDTH);
#if DO_PPU
  dumpMatrix("PPU:\n", "%8.2f ", imageC, IMAGE_WIDTH);
#endif
#if DO_SPU
  dumpMatrix("SPUs:\n", "%8.2f ", imageG, IMAGE_WIDTH);
#endif
#endif

  // cleanup
  free_align(imageA);
  free_align(imageG);
  free_align(imageC);
  free_align(filterH);

  return (0);
}

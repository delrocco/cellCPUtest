#ifndef _DEFINES_H_
#define _DEFINES_H_

//=====================================
// CONSTANTS, MACROS, DEFINITIONS
//=====================================

#define MAX_SPUS 	    8     // maximum possible number of spus
#define DO_SPU        1     // calc gpu version
#define DO_PPU        0     // calc cpu version
#define PRINT_IMAGE   0     // debug matrices
#define FILL_CONST    0     // matrix fill type
#define FILL_POS      1     // matrix fill type
#define FILL_ADD      2     // matrix fill type

#define IMAGE_WIDTH   4096  // w & h of image (multiple of TILE_WIDTH)
#define FILTER_WIDTH  4     // w & h of filter
#define FILTER_MAX    16    // w & h of filter (maximum size)
#define TILE_WIDTH    32    // w & h of image tile to process
#define TILE_BLOCK    (TILE_WIDTH+FILTER_WIDTH) // w & h of image tile to send
#define IMAGE_PADDED  (IMAGE_WIDTH+FILTER_MAX)  // w & h of image A padded

typedef struct __attribute__ ((aligned(128)))
{
	float        *imageA    __attribute__((aligned(16)));
	float        *filterH   __attribute__((aligned(16)));
  float        *imageG    __attribute__((aligned(16)));
  unsigned int  threadcnt __attribute__((aligned(16)));
  unsigned int  threadidx __attribute__((aligned(16)));
} spu_argument_t;

// matrix functions
void dumpMatrix(const char *title, const char *fmt, const float *matrix, unsigned int width);
void dumpMatrixPadded(const char *title, const char *fmt, const float *matrix, unsigned int width, unsigned int widthPadded);
void diffMatrix(const float *matrixA, const float *matrixB, unsigned int width, double epsilon);
void fillMatrix(float *matrix, unsigned int width, float value, unsigned int type);
void fillMatrixPadded(float *matrix, unsigned int width, unsigned int widthPadded, float value, unsigned int type);

#endif

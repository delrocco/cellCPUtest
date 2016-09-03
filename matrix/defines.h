#ifndef _DEFINES_H_
#define _DEFINES_H_

//=====================================
// CONSTANTS, MACROS, DEFINITIONS
//=====================================

#define DO_SPU        1       // calc gpu version
#define DO_PPU        0       // calc cpu version
#define PRINT_MATRIX  0       // debug matrices
#define MATRIX_WIDTH  2048    // w & h
#define MAX_SPUS 	    8       // maximum possible number of spus

#define FILL_CONST    0       // matrix fill type
#define FILL_POS      1       // matrix fill type
#define FILL_ADD      2       // matrix fill type

typedef struct __attribute__ ((aligned(16)))
{
	float        *matrixA;
	float        *matrixB;
	float        *matrixG;
  unsigned int  threadcnt;
  unsigned int  threadidx;
} spu_argument_t;

// matrix functions
void dumpMatrix(const char *title, const char *fmt, const float *matrix, unsigned int width);
void diffMatrix(const float *matrixA, const float *matrixB, unsigned int width, double epsilon);
void fillMatrix(float *matrix, unsigned int width, float value, unsigned int type);
void transposeMatrix(float *matrix, unsigned int width);

#endif

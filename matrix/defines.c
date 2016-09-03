
// local
#include "defines.h"
// ansi
#include <stdio.h>
#include <stdlib.h>


//=====================================
// MATRIX FUNCTIONS
//=====================================

void dumpMatrix(const char *title, const char *fmt, const float *matrix, unsigned int width)
{
  unsigned int i,j;
  if (!matrix || !fmt) return;
  if (title) printf(title);
  for (j=0; j<width; j++)
  {
      for (i=0; i<width; i++)
          printf(fmt, matrix[j*width+i]);
      printf("\n");
  }
  printf("\n");
}

void diffMatrix(const float *matrixA, const float *matrixB, unsigned int width, double epsilon)
{
  unsigned int i;
  unsigned int size;
	float delta;
  if (!matrixA) return;
  if (!matrixB) return;
  size = width*width;
  for (i=0; i<size; i++)
  {
    //delta = abs((double)matrixA[i] - (double)matrixB[i]);
    delta = matrixA[i] - matrixB[i];
    delta = (delta<0?-delta:delta);
    if (delta > (float)epsilon)
    {
      printf("Test Failed!\n");
      return;
    }
  }
  printf("Test Succeeded!\n");
}

void fillMatrix(float *matrix, unsigned int width, float value, unsigned int type)
{
  unsigned int i,j;
  if (!matrix) return;
  for (j=0; j<width; j++)
  {
      for (i=0; i<width; i++)
      {
          if      (type == FILL_CONST) matrix[j*width+i] = value;
          else if (type == FILL_POS  ) matrix[j*width+i] = j*width+i;
          else if (type == FILL_ADD  ) matrix[j*width+i] = value * (j*width+i);
      }
  }
}

void transposeMatrix(float *matrix, unsigned int width)
{
  unsigned int i,j;
  unsigned int idx1, idx2;
  float value;
  for (j=0; j<width; j++)
  {
    for (i=0; i<j; i++)
    {
      idx2 = i*width + j;
      idx1 = j*width + i;
      value = matrix[idx1];
      matrix[idx1] = matrix[idx2];
      matrix[idx2] = value;
    }
  }
}

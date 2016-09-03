// local
#include "../defines.h"
// cell
#include <spu_mfcio.h>
#include <spu_intrinsics.h>
//#include <libmisc.h>
// ansi
#include <stdio.h>
#include <math.h>
//#include <stdlib.h>

//=====================================
// CONSTANTS, MACROS, DEFINITIONS
//====================================

#define TAG_READ_BLCK 0 // reading a tile block of A (block)
#define TAG_READ_NEXT 1 // reading a tile block of A (block^1)
#define TAG_READ_FLTR 2 // reading filter H
#define TAG_READ_ARGS 3 // reading the arguments context
#define TAG_WRIT_TILE 4 // writing resulting tile of G

//=====================================
// GLOBALS
//=====================================

volatile spu_argument_t args                     __attribute__((aligned(128)));
volatile float filter[FILTER_WIDTH*FILTER_WIDTH] __attribute__((aligned(128)));
volatile float blockA[2][TILE_BLOCK*TILE_BLOCK]  __attribute__((aligned(128)));
volatile float tileG[TILE_WIDTH*TILE_WIDTH]      __attribute__((aligned(128)));

//=====================================
// FUNCTIONS
//=====================================

inline void dma_get(volatile void *ls_addr, unsigned int mem_addr, unsigned int size, unsigned int tag)
{
  mfc_get(ls_addr,mem_addr,size,tag,0,0);
  mfc_write_tag_mask(1<<tag);
  mfc_read_tag_status_all();
}

inline void dma_put(volatile void *ls_addr, unsigned int mem_addr, unsigned int size, unsigned int tag)
{
  mfc_put(ls_addr,mem_addr,size,tag,0,0);
  mfc_write_tag_mask(1<<tag);
  mfc_read_tag_status_all();
}

void dma_blockA(unsigned int ax, unsigned int ay, unsigned int buffer, unsigned int tag)
{
  unsigned int z;
  unsigned int rowmemsize = sizeof(float)*TILE_BLOCK;

  // loop over all rows in block (padded tile)
  for (z=0; z<TILE_BLOCK; z++)
  {
    mfc_get((void*)(blockA[buffer]+(z*TILE_BLOCK)), (unsigned int)(args.imageA+(((ay+z)*IMAGE_PADDED)+ax)), rowmemsize, tag, 0,0);
  }
}

void dma_tileG(unsigned int gx, unsigned int gy)
{
  unsigned int z;
  unsigned int rowmemsize = sizeof(float)*TILE_WIDTH;  

  // wait for any previous writes to finish
  mfc_write_tag_mask(1<<TAG_WRIT_TILE);
  mfc_read_tag_status_all();

  // loop over all rows in tile
  for (z=0; z<TILE_WIDTH; z++)
  {
    mfc_put((void*)(tileG+(z*TILE_WIDTH)), (unsigned int)(args.imageG+(((gy+z)*IMAGE_WIDTH)+gx)), rowmemsize, TAG_WRIT_TILE, 0,0);
  }
}

int main(unsigned long long __attribute__ ((unused)) id, unsigned long long argptr)
{
  unsigned int i,j;             // start of tile of image A & image C
  unsigned int ax,ay;           // index into image A
  unsigned int hx,hy,hidx;      // index into filter H
  unsigned int gx,gy,gidx;      // index into image G
  unsigned int block,blocknext; // double buffering
  float sum;

  // fetch program arguments structure
  dma_get((void*)&args, (unsigned int)argptr, sizeof(args), TAG_READ_ARGS);

  // fetch entire filter H (supports H up to 64x64)
  dma_get((void*)filter, (unsigned int)(args.filterH), sizeof(float)*(FILTER_WIDTH*FILTER_WIDTH), TAG_READ_FLTR);

  // loop over rows of imageA that this spu is interested in,
  // note that each spu will only process certain rows
  for (j=(args.threadidx*TILE_WIDTH); j<IMAGE_WIDTH; j+=(args.threadcnt*TILE_WIDTH))
  {
    block = 0;
    dma_blockA(0,j,block,block);

    // loop over all tiles in the current row
    for (i=0; i<IMAGE_WIDTH-TILE_WIDTH; i+=TILE_WIDTH)
    {
      blocknext = block^1;
      dma_blockA(i+TILE_WIDTH,j,blocknext,blocknext);

      // wait for previous block fetch to finish
      mfc_write_tag_mask(1<<block);
      mfc_read_tag_status_all();

      //-------------------------------------------------------------
      // standard convolution
      gidx=0;
      for (gy=0; gy<TILE_WIDTH; gy++)
      {
        for (gx=0; gx<TILE_WIDTH; gx++)
        {
          sum  = 0;
          hidx = 0;
          for (hy=0; hy<FILTER_WIDTH; hy++)
          {
              for (hx=0; hx<FILTER_WIDTH; hx++)
              {
                  ax = gx+hx;
                  ay = gy+hy;
                  sum += filter[hidx++] * blockA[block][ay*TILE_BLOCK+ax];
              }
          }

          tileG[gidx++] = sum;
        }
      }
      //-------------------------------------------------------------

      // write back a tile of output image G
      dma_tileG(i,j);

      // reassign double buffer index
      block = blocknext;
    }

    // wait for last block fetch to finish
    mfc_write_tag_mask(1<<block);
    mfc_read_tag_status_all();

    //-------------------------------------------------------------
    // standard convolution
    gidx=0;
    for (gy=0; gy<TILE_WIDTH; gy++)
    {
      for (gx=0; gx<TILE_WIDTH; gx++)
      {
        sum  = 0;
        hidx = 0;
        for (hy=0; hy<FILTER_WIDTH; hy++)
        {
            for (hx=0; hx<FILTER_WIDTH; hx++)
            {
                ax = gx+hx;
                ay = gy+hy;
                sum += filter[hidx++] * blockA[block][ay*TILE_BLOCK+ax];
            }
        }

        tileG[gidx++] = sum;
      }
    }
    //-------------------------------------------------------------

    // write back a tile of output image G
    dma_tileG(i,j);
  }

  // make sure all writes are done before returning
  mfc_write_tag_mask(1<<TAG_WRIT_TILE);
  mfc_read_tag_status_all();

  return 0;
}

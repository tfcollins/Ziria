/* 
   Copyright (c) Microsoft Corporation
   All rights reserved. 

   Licensed under the Apache License, Version 2.0 (the ""License""); you
   may not use this file except in compliance with the License. You may
   obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

   THIS CODE IS PROVIDED ON AN *AS IS* BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT
   LIMITATION ANY IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR
   A PARTICULAR PURPOSE, MERCHANTABLITY OR NON-INFRINGEMENT.

   See the Apache Version 2.0 License for specific language governing
   permissions and limitations under the License.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "types.h"
#include "wpl_alloc.h"
#include "params.h"


// Called manually from driver
void initHeapCtxBlock(HeapContextBlock *hblk, memsize_int max_heap_size)
{
	//hblk->wpl_heap = NULL;
	hblk->wpl_heap_siz = max_heap_size;
	hblk->wpl_heap = try_alloc_bytes(hblk, max_heap_size);
}


char * try_alloc_bytes(HeapContextBlock *hblk, memsize_int siz)
{
  char *buf = (char *) malloc(siz);
  if (buf == NULL) 
  {
	fprintf(stderr, "Error: could not allocate buffer of size %ul\n", siz);
	exit(1); 
  }

  // This will lock all memory allocated into real mem
  // However, there is a subtle problem - if we do real-time scheduling
  // it seems that occasional swaps helped OS interrupt our high-priority code
  // and stopped PC from freezing. With VirtualLock it seems as it freezes more often
  // But this hypothesis is not properly tested
  // #ifdef SORA_PLATFORM
  //  VirtualLock(buf, siz);
  // #endif

  return buf;
}


/* DV: This is really the smallest and simplest implementation.
   Allocate memory for all threads on the same global heap. 
   Most likely this will have to change at some point (move to TLS).
 ******************************************************************/


#define CALIGN16(x) (((((x) + 15) >> 4) << 4))


// Called automtically from Ziria's produced C code
void wpl_init_heap(HeapContextBlock *hblk, memsize_int max_heap_size)
{
  // if (num_of_allocs == 0) return; // we need no heap!
  
  // Here we don't want to redo malloc, but just reset the start pointer
  //hblk->wpl_heap_siz = max_heap_size;
  //hblk->wpl_heap = try_alloc_bytes(hblk, max_heap_size);
	hblk->wpl_free_idx = CALIGN16((memsize_int)hblk->wpl_heap) - (memsize_int)hblk->wpl_heap;
}


unsigned int wpl_get_free_idx(HeapContextBlock *hblk)
{ 
	return hblk->wpl_free_idx;
}

// precondition: 16-aligned
void wpl_restore_free_idx(HeapContextBlock *hblk, memsize_int idx)
{ 
	hblk->wpl_free_idx = idx;
}


void * wpl_alloca(HeapContextBlock *hblk, memsize_int bytes)
{
	memsize_int allocunit = CALIGN16(bytes);

  //printf("Allocating: %d bytes\n", bytes); 
  //printf("wpl_heap_siz: %d bytes\n", wpl_heap_siz); 
  //printf("Remaining heap: %d bytes\n", wpl_heap_siz - (wpl_free_idx + allocunit));
  
  if (hblk->wpl_free_idx + allocunit >= hblk->wpl_heap_siz) {
    fprintf(stderr, "WPL allocator out of memory, try increasing heap size (heap size = %lld, allocated = %lld, trying to allocate = %lld)!\n", hblk->wpl_heap_siz, hblk->wpl_free_idx, allocunit);
    exit(-1);
  }

  // memsize_int is an int type to which we can safely cast 64 bits pointers 
  void * ret = (void *)((memsize_int)hblk->wpl_heap + hblk->wpl_free_idx);

  hblk->wpl_free_idx += allocunit;

  return ret;

}

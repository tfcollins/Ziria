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
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#ifdef WINDDK
// These need to be included in WinDDK environment but not in VS
// Not sure why...
#include <winsock2.h> // ws2_32.lib required
#include <ws2tcpip.h>
#endif 

#include "wpl_alloc.h"
#include "params.h"
#include "types.h"
#include "buf.h"

#ifdef SORA_PLATFORM
#include "sora_ip.h"
#endif



void resetBufCtxBlock(BufContextBlock *blk)
{
	blk->bit_input_idx = 0;
	blk->bit_input_repetitions = 1;
	blk->bit_input_dummy_samples = 0;
	blk->bit_fst = 1;
	blk->bit_output_idx = 0;
	blk->chunk_input_idx = 0;
	blk->chunk_input_repetitions = 1;
	blk->chunk_input_dummy_samples = 0;
	blk->chunk_fst = 1;
	blk->chunk_output_idx = 0;
	blk->num8_input_idx = 0;
	blk->num8_input_repeats = 1;
	blk->num8_input_dummy_samples = 0;
	blk->num8_fst = 1;
	blk->num8_output_idx = 0;
	blk->num16_input_idx = 0;
	blk->num16_input_repeats = 1;
	blk->num16_input_dummy_samples = 0;
	blk->num16_fst = 1;
	blk->num16_output_idx = 0;
	blk->num_input_idx = 0;
	blk->num_input_repeats = 1;
	blk->num_input_dummy_samples = 0;
	blk->num_fst = 1;
	blk->num_output_idx = 0;

	blk->total_in = 0;
	blk->total_out = 0;
	blk->size_in = 0;
	blk->size_out = 0;

	blk->buf_input_callback = NULL;
	blk->buf_output_callback = NULL;
}



void initBufCtxBlock(BufContextBlock *blk)
{
	resetBufCtxBlock(blk);
	blk->mem_input_buf = NULL;
	blk->mem_output_buf = NULL;
	blk->mem_input_buf_size = 0;
	blk->mem_output_buf_size = 0;
}





void fprint_bit(BufContextBlock* blk, FILE *f, Bit val)
{
	if (blk->bit_fst == 1) {
		fprintf(f,"%d",val);
		blk->bit_fst = 0;
	}
	else
		fprintf(f,",%d",val);
}

void fprint_arrbit(BufContextBlock* blk, FILE *f, BitArrPtr src, unsigned int vlen)
{
	Bit x = 0;
	unsigned int i;
	
	for (i=0; i < vlen; i++) 
	{
		bitRead(src,i,&x);
		fprint_bit(blk, f,x);
	}
}

void parse_bit(char *token, Bit *val) 
{
  long x = strtol(token,NULL,10);
  if (errno == EINVAL) 
  {
      fprintf(stderr,"Parse error when loading debug file.");
      exit(1);
  }
  if (x <= 1) 
	  *val = ((Bit) x) & 1; 
  else {
      fprintf(stderr,"Debug file does not contain binary data (%s)\n", token);
      exit(1);
  }
}
unsigned int parse_dbg_bit(char *dbg_buf, BitArrPtr target)
{
	Bit val;
	long x;
	unsigned int c = 0;
	char *s = NULL;

  char* trailing_comma = delete_trailing_comma(dbg_buf);
	s = strtok(dbg_buf, ",");
	
 	if (s == NULL) 
	{ 
		fprintf(stderr,"Input (debug) file contains no samples.");
		exit(1);
	}

	//parse_bit(s,&val);
    x = strtol(s,NULL,10);
	if (errno == EINVAL) 
	{
      fprintf(stderr,"Parse error when loading debug file.");
      exit(1);
	}
	if (x <= 1) 
	  val = ((Bit) x) & 1; 
	else 
	{
		fprintf(stderr, "Debug file does not contain binary data (%s)\n", s);
		exit(1);
	}

	bitWrite(target, c++, val);
  
	while (s = strtok(NULL, ",")) 
	{
		x = strtol(s,NULL,10);

		if (errno == EINVAL) 
		{
			fprintf(stderr,"Parse error when loading debug file.");
			exit(1);
		}
		if (x <= 1)
		{
			val = ((Bit)x) & 1;
		}
		else 
		{
			fprintf(stderr, "Debug file does not contain binary data (%s)\n", s);
			exit(1);
		}

		bitWrite(target, c++, val);
	}

  restore_trailing_comma(trailing_comma);
	return c;
}

void init_getbit(BlinkParams *params, BufContextBlock* blk, HeapContextBlock *hblk, size_t unit_size)
{
	blk->size_in = 1;
	blk->total_in = 0;

	if (params->inType == TY_DUMMY)
	{
		blk->bit_max_dummy_samples = params->dummySamples;
	}

	if (params->inType == TY_MEM)
	{
		if (blk->mem_input_buf == NULL || blk->mem_input_buf_size == 0)
		{
			fprintf(stderr, "Error: input memory buffer not initialized\n");
			exit(1);
		}
		else
		{
			blk->bit_input_buffer = (BitArrPtr)blk->mem_input_buf;
			blk->bit_input_entries = 8 * blk->mem_input_buf_size;
		}
	}

	if (params->inType == TY_FILE)
	{
		memsize_int sz; 
		char *filebuffer;
		try_read_filebuffer(hblk, params->inFileName, params->inFileMode, &filebuffer, &sz);

		if (params->inFileMode == MODE_BIN)
		{ 
			blk->bit_input_buffer = (BitArrPtr)filebuffer;
			blk->bit_input_entries = 8 * sz;
		}
		else 
		{
			blk->bit_input_buffer = (BitArrPtr)try_alloc_bytes(hblk, sz);
			blk->bit_input_entries = parse_dbg_bit(filebuffer, blk->bit_input_buffer);
		}
	}

	if (params->inType == TY_SDR)
	{
		fprintf(stderr, "Error: Sora does not support bit receive\n");
		exit(1);
	}


	if (params->inType == TY_IP)
	{
#ifdef SORA_PLATFORM
	  // Receiving from IP
	  //Ndis_init(NULL);
#endif
	}

}

GetStatus buf_getbit(BlinkParams *params, BufContextBlock* blk, Bit *x)
{
	if (params->timeStampAtRead)
		write_time_stamp(params);
	blk->total_in++;

	if (params->inType == TY_IP)
	{
		fprintf(stderr, "Error: IP does not support single bit receive\n");
		exit(1);
	}

	if (params->inType == TY_DUMMY)
	{
		if (blk->bit_input_dummy_samples >= blk->bit_max_dummy_samples && params->dummySamples != INF_REPEAT)
		{
			return GS_EOF;
		}
		blk->bit_input_dummy_samples++;
		// No real need to do this, and it slows down substantially
		//*x = 0;
		return GS_SUCCESS;
	}

	// If we reached the end of the input buffer 
	if (blk->bit_input_idx >= blk->bit_input_entries)
	{
		// If no more repetitions are allowed 
		if (params->inFileRepeats != INF_REPEAT && blk->bit_input_repetitions >= params->inFileRepeats)
		{
			return GS_EOF;
		}
		// Otherwise we set the index to 0 and increase repetition count
		blk->bit_input_idx = 0;
		blk->bit_input_repetitions++;
	}

	bitRead(blk->bit_input_buffer, blk->bit_input_idx++, x);

	return GS_SUCCESS;
}

FORCE_INLINE
GetStatus buf_getarrbit(BlinkParams *params, BufContextBlock* blk, BitArrPtr x, unsigned int vlen)
{
	if (params->timeStampAtRead)
		write_time_stamp(params);
	blk->total_in += vlen;

	if (params->inType == TY_IP)
	{
#ifdef SORA_PLATFORM
	  UINT len = ReadFragment(x, RADIO_MTU);
	  return GS_SUCCESS;
#endif
	}

	if (params->inType == TY_DUMMY)
	{
		if (blk->bit_input_dummy_samples >= blk->bit_max_dummy_samples && params->dummySamples != INF_REPEAT)
		{
			return GS_EOF;
		}
		blk->bit_input_dummy_samples += vlen;
		// No real need to do this, and it slows down substantially
		//memset(x,0,(vlen+7)/8);
		return GS_SUCCESS;
	}

	if (blk->bit_input_idx + vlen > blk->bit_input_entries)
	{
		if (params->inFileRepeats != INF_REPEAT && blk->bit_input_repetitions >= params->inFileRepeats)
		{
			if (blk->bit_input_idx != blk->bit_input_entries)
				fprintf(stderr, "Warning: Unaligned data in input file, ignoring final get()!\n");
			return GS_EOF;
		}
		// Otherwise ignore trailing part of the file, not clear what that part may contain ...
		blk->bit_input_idx = 0;
		blk->bit_input_repetitions++;
	}
	
	bitArrRead(blk->bit_input_buffer, blk->bit_input_idx, vlen, x);

	blk->bit_input_idx += vlen;
	return GS_SUCCESS;
}


void init_putbit(BlinkParams *params, BufContextBlock* blk, HeapContextBlock *hblk, size_t unit_size)
{
	blk->size_out = 1;
	blk->total_out = 0;

	if (params->outType == TY_DUMMY || params->outType == TY_FILE)
	{
		blk->bit_output_buffer = (unsigned char *)malloc(params->outBufSize);
		blk->bit_output_entries = params->outBufSize * 8;
		if (params->outType == TY_FILE)
		{
			if (params->outFileMode == MODE_BIN)
			{
				blk->bit_output_file = try_open(params->outFileName, "wb");
			}
			else
			{
				blk->bit_output_file = try_open(params->outFileName, "w");
			}
		}
	}

	if (params->outType == TY_MEM)
	{
		if (blk->mem_output_buf == NULL || blk->mem_output_buf_size == 0)
		{
			fprintf(stderr, "Error: output memory buffer not initialized\n");
			exit(1);
		}
		else
		{
			blk->bit_output_buffer = (unsigned char*)blk->mem_output_buf;
			blk->bit_output_entries = blk->mem_output_buf_size * 8;
		}
	}

	if (params->outType == TY_SDR)
	{
		fprintf(stderr, "Error: Sora TX does not support bits\n");
		exit(1);
	}

	if (params->outType == TY_IP)
	{
#ifdef SORA_PLATFORM
	  // Sending to IP
	  //Ndis_init(NULL);
#endif
	}

}
void buf_putbit(BlinkParams *params, BufContextBlock* blk, Bit x)
{
	if (!params->timeStampAtRead)
		write_time_stamp(params);
	blk->total_out++;

	if (params->outType == TY_IP)
	{
		fprintf(stderr, "Error: IP does not support single bit transmit\n");
		exit(1);
	}

	if (params->outType == TY_DUMMY)
	{
		return;
	}

	if (params->outType == TY_MEM)
	{
		bitWrite(blk->bit_output_buffer, blk->bit_output_idx++, x);
	}

	if (params->outType == TY_FILE)
	{
		if (params->outFileMode == MODE_DBG)
			fprint_bit(blk, blk->bit_output_file, x);
		else 
		{
			if (blk->bit_output_idx == blk->bit_output_entries)
			{
				fwrite(blk->bit_output_buffer, blk->bit_output_entries / 8, 1, blk->bit_output_file);
				blk->bit_output_idx = 0;
			}
			bitWrite(blk->bit_output_buffer, blk->bit_output_idx++, x);
		}
	}
}

FORCE_INLINE
void buf_putarrbit(BlinkParams *params, BufContextBlock* blk, BitArrPtr x, unsigned int vlen)
{
	blk->total_out+= vlen;
	if (!params->timeStampAtRead)
		write_time_stamp(params);

	if (params->outType == TY_IP)
	{
#ifdef SORA_PLATFORM
	   int n = WriteFragment(x);
	   return;
#endif
	}

	if (params->outType == TY_DUMMY) return;

	if (params->outType == TY_MEM)
	{
		bitArrWrite(x, blk->bit_output_idx, vlen, blk->bit_output_buffer);
		blk->bit_output_idx += vlen;
	}

	if (params->outType == TY_FILE)
	{
		if (params->outFileMode == MODE_DBG) 
			fprint_arrbit(blk, blk->bit_output_file, x, vlen);
		else
		{ 
			if (blk->bit_output_idx + vlen >= blk->bit_output_entries)
			{
				// first write the first (output_entries - output_idx) entries
				unsigned int m = blk->bit_output_entries - blk->bit_output_idx;

				bitArrWrite(x, blk->bit_output_idx, m, blk->bit_output_buffer);

				// then flush the buffer
				fwrite(blk->bit_output_buffer, blk->bit_output_entries / 8, 1, blk->bit_output_file);

				// then write the rest
				// BOZIDAR: Here we used to have bitArrRead but I believe it was wrong
				bitArrWrite(x, m, vlen - m, blk->bit_output_buffer);
				blk->bit_output_idx = vlen - m;
			}
			else
			{
				bitArrWrite(x, blk->bit_output_idx, vlen, blk->bit_output_buffer);
				blk->bit_output_idx += vlen;
			}
		}
	}
}

void reset_putbit(BlinkParams *params, BufContextBlock* blk)
{
	if (params->outType == TY_FILE)
	{
		if (params->outFileMode == MODE_BIN) {
			fwrite(blk->bit_output_buffer, 1, (blk->bit_output_idx + 7) / 8, blk->bit_output_file);
		}
	}
	blk->bit_output_idx = 0;
}

void flush_putbit(BlinkParams *params, BufContextBlock* blk)
{
	reset_putbit(params, blk);
	if (params->outType == TY_FILE)
	{
		fclose(blk->bit_output_file);
	}
}




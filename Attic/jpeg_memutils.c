
/*
** jpeg_memutils.c
** assists the IJG libjpeg with handling data in memory instead of to/from files.
*/



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <jerror.h>
#include "jpeg_memutils.h"

#if JPEG_LIB_VERSION < 80

#define OUTPUT_BUF_SIZE 4096

/*
** TYPEDEFS
*/
typedef struct
{
	struct jpeg_destination_mgr pub;
	JOCTET *outbuffer;
	int outbuffer_size;
	int *outbuffer_nbytes_p;
	JOCTET *buffer;
} mem_destination_mgr;

typedef struct
{
	struct jpeg_source_mgr pub;
	JOCTET *inbuffer;
	int inbuffer_size;
	JOCTET *buffer;
	boolean start_of_data;
} mem_source_mgr;

/*
** PROTOTYPES
*/
static void mem_destination_init(j_compress_ptr cinfo);
static boolean mem_destination_emptybuf(j_compress_ptr cinfo);
static void mem_destination_terminate(j_compress_ptr cinfo);
static void mem_source_init(j_decompress_ptr cinfo);
static boolean mem_source_fillbuf(j_decompress_ptr cinfo);
static void mem_source_skipinputdata(j_decompress_ptr cinfo, long num_bytes);
static void mem_source_terminate(j_decompress_ptr cinfo);

void jpeg_mem_dest(j_compress_ptr cinfo, JOCTET *outbuffer, int outbuffer_size, int *outbuffer_nbytes_p)
{
	mem_destination_mgr *dest = NULL;

	if (cinfo->dest == NULL)
		cinfo->dest = (struct jpeg_destination_mgr *)(*cinfo->mem->alloc_small)((j_common_ptr)cinfo, JPOOL_PERMANENT, sizeof(mem_destination_mgr));
	dest = (mem_destination_mgr *)cinfo->dest;
	dest->pub.init_destination = mem_destination_init;
	dest->pub.empty_output_buffer = mem_destination_emptybuf;
	dest->pub.term_destination = mem_destination_terminate;
	dest->outbuffer = outbuffer;
	dest->outbuffer_size = outbuffer_size;
	dest->outbuffer_nbytes_p = outbuffer_nbytes_p;
}

static void mem_destination_init(j_compress_ptr cinfo)
{
	mem_destination_mgr *dest = (mem_destination_mgr *)cinfo->dest;

	dest->buffer = (JOCTET *)(*cinfo->mem->alloc_small)((j_common_ptr)cinfo, JPOOL_IMAGE, OUTPUT_BUF_SIZE * sizeof(JOCTET));
	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
	*(dest->outbuffer_nbytes_p) = 0;
}

static boolean mem_destination_emptybuf(j_compress_ptr cinfo)
{
	mem_destination_mgr *dest = (mem_destination_mgr *)cinfo->dest;

	if (*(dest->outbuffer_nbytes_p) + OUTPUT_BUF_SIZE > dest->outbuffer_size)
	{
		fprintf(stderr, "JPEG error\n");
		return FALSE;
	}
	memcpy(dest->outbuffer + *(dest->outbuffer_nbytes_p), dest->buffer, OUTPUT_BUF_SIZE);
	*(dest->outbuffer_nbytes_p) += OUTPUT_BUF_SIZE;
	dest->pub.next_output_byte = dest->buffer;
	dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
	return TRUE;
}

static void mem_destination_terminate(j_compress_ptr cinfo)
{
	mem_destination_mgr *dest = (mem_destination_mgr *)cinfo->dest;
	size_t datacount = OUTPUT_BUF_SIZE - dest->pub.free_in_buffer;

	if (datacount > 0)
	{
		if (*(dest->outbuffer_nbytes_p) + datacount > dest->outbuffer_size)
		{
			fprintf(stderr, "JPEG error\n");
			return;
		}
		memcpy(dest->outbuffer + *(dest->outbuffer_nbytes_p), dest->buffer, datacount);
		*(dest->outbuffer_nbytes_p) += datacount;
	}
	return;
}

void jpeg_mem_src(j_decompress_ptr cinfo, JOCTET *inbuffer, int inbuffer_size)
{
	mem_source_mgr *src = NULL;

	if (cinfo->src == NULL)
	{
		cinfo->src = (struct jpeg_source_mgr *)(*cinfo->mem->alloc_small)((j_common_ptr)cinfo, JPOOL_PERMANENT, sizeof(mem_source_mgr));
		src = (mem_source_mgr *)cinfo->src;
		src->buffer = (JOCTET *)(*cinfo->mem->alloc_small)((j_common_ptr)cinfo, JPOOL_PERMANENT, inbuffer_size * sizeof(JOCTET));
	}
	else
	{
		src = (mem_source_mgr *)cinfo->src;
		if (src->inbuffer_size < inbuffer_size)
			src->buffer = (JOCTET *)(*cinfo->mem->alloc_small)((j_common_ptr)cinfo, JPOOL_PERMANENT, inbuffer_size * sizeof(JOCTET));
	}
	src->pub.init_source = mem_source_init;
	src->pub.fill_input_buffer = mem_source_fillbuf;
	src->pub.skip_input_data = mem_source_skipinputdata;
	src->pub.resync_to_restart = jpeg_resync_to_restart;
	src->pub.term_source = mem_source_terminate;
	src->inbuffer = inbuffer;
	src->inbuffer_size = inbuffer_size;
	src->pub.bytes_in_buffer = 0;
	src->pub.next_input_byte = NULL;
}

static void mem_source_init(j_decompress_ptr cinfo)
{
	mem_source_mgr *src = (mem_source_mgr *)cinfo->src;

	src->start_of_data = TRUE;
	src->pub.bytes_in_buffer = 0;
}

static boolean mem_source_fillbuf(j_decompress_ptr cinfo)
{
	mem_source_mgr *src = (mem_source_mgr *)cinfo->src;
	size_t nbytes;

	memcpy(src->buffer, src->inbuffer, src->inbuffer_size);
	nbytes = src->inbuffer_size;

	if (nbytes <= 0)
	{
		if (src->start_of_data)
		{
			fprintf(stderr, "ERROR in JPEG decompression - out of data\n");
			return FALSE;
		}
		/* insert a fake EOI marker */
		src->buffer[0] = (JOCTET) 0xff;
		src->buffer[1] = (JOCTET) JPEG_EOI;
		nbytes = 2;
	}
	src->pub.next_input_byte = src->buffer;
	src->pub.bytes_in_buffer = nbytes;
	src->start_of_data = FALSE;
	return TRUE;
}

static void mem_source_skipinputdata(j_decompress_ptr cinfo, long num_bytes)
{
	mem_source_mgr *src = (mem_source_mgr *)cinfo->src;

	if (num_bytes > 0)
	{
		while (num_bytes > (long)src->pub.bytes_in_buffer)
		{
			num_bytes -= (long)src->pub.bytes_in_buffer;
			mem_source_fillbuf(cinfo);
		}
		src->pub.next_input_byte += (size_t)num_bytes;
		src->pub.bytes_in_buffer -= (size_t)num_bytes;
	}
	return;
}

static void mem_source_terminate(j_decompress_ptr cinfo)
{
	return;
}

#else
static void dummy() {}

#endif

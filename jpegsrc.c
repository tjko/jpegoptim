/*
 * jpegsrc.c
 *
 * Copyright (C) 2022 Timo Kokkonen
 * All Rights Reserved.
 *
 * Custom libjpeg "Source Manager" for reading from a file handle
 * and optionally saving the input also into a memory buffer.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of JPEGoptim.
 *
 * JPEGoptim is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * JPEGoptim is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with JPEGoptim. If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jpeglib.h>
#include <jerror.h>

#include "jpegoptim.h"

#define STDIO_BUFFER_SIZE 4096


/* Custom jpeg source manager object */

typedef struct {
	struct jpeg_source_mgr pub; /* public fields */

	unsigned char **buf_ptr;
	size_t *bufsize_ptr;
	size_t *bufused_ptr;
	size_t incsize;
	unsigned char *buf;
	size_t bufsize;
	size_t bufused;

	FILE *infile;
	JOCTET *stdio_buffer;
	boolean start_of_file;
} jpeg_custom_source_mgr;

typedef jpeg_custom_source_mgr* jpeg_custom_source_mgr_ptr;



void custom_init_source (j_decompress_ptr dinfo)
{
	jpeg_custom_source_mgr_ptr src = (jpeg_custom_source_mgr_ptr) dinfo->src;

	src->bufused = 0;
	if (src->bufused_ptr)
		*src->bufused_ptr = 0;
	src->start_of_file = TRUE;
}


boolean custom_fill_input_buffer (j_decompress_ptr dinfo)
{
	jpeg_custom_source_mgr_ptr src = (jpeg_custom_source_mgr_ptr) dinfo->src;
	size_t bytes_read;
	unsigned char *newbuf;

	bytes_read = fread(src->stdio_buffer, 1, STDIO_BUFFER_SIZE, src->infile);
	if (bytes_read <= 0) {
		if (src->start_of_file)
			ERREXIT(dinfo, JERR_INPUT_EMPTY);
		WARNMS(dinfo, JWRN_JPEG_EOF);
		/* Insert fake EOI marker if read failed */
		src->stdio_buffer[0] = (JOCTET) 0xff;
		src->stdio_buffer[1] = (JOCTET) JPEG_EOI;
		bytes_read = 2;
	} else if (src->buf_ptr && src->buf) {
		if (bytes_read > (src->bufsize - src->bufused)) {
			/* Need to allocate more memory for the buffer. */
			src->bufsize += src->incsize;
			newbuf = realloc(src->buf, src->bufsize);
			if (!newbuf) ERREXIT1(dinfo, JERR_OUT_OF_MEMORY, 42);
			src->buf = newbuf;
			*src->buf_ptr = newbuf;
			src->incsize *= 2;
		}
		memcpy(&src->buf[src->bufused], src->stdio_buffer, bytes_read);
		src->bufused += bytes_read;
		if (src->bufused_ptr)
			*src->bufused_ptr = src->bufused;
	}

	src->pub.next_input_byte = src->stdio_buffer;
	src->pub.bytes_in_buffer = bytes_read;
	src->start_of_file = FALSE;

	return TRUE;
}


void custom_skip_input_data (j_decompress_ptr dinfo, long num_bytes)
{
	jpeg_custom_source_mgr_ptr src = (jpeg_custom_source_mgr_ptr) dinfo->src;

	if (num_bytes <= 0)
		return;

	/* skip "num_bytes" bytes of data from input... */
	while (num_bytes > (long) src->pub.bytes_in_buffer) {
		num_bytes -= src->pub.bytes_in_buffer;
		(void) custom_fill_input_buffer(dinfo);
	}
	src->pub.next_input_byte += (size_t) num_bytes;
	src->pub.bytes_in_buffer -= (size_t) num_bytes;
}


void custom_term_source (j_decompress_ptr dinfo)
{
	jpeg_custom_source_mgr_ptr src = (jpeg_custom_source_mgr_ptr) dinfo->src;

	if (src->bufused_ptr)
		*src->bufused_ptr = src->bufused;
}


void jpeg_custom_src(j_decompress_ptr dinfo, FILE *infile,
		unsigned char **bufptr,	size_t *bufsizeptr, size_t *bufusedptr, size_t incsize)
{
	jpeg_custom_source_mgr_ptr src;

	if (!dinfo->src) {
		/* Allocate source manager object if needed */
		dinfo->src = (struct jpeg_source_mgr *)
			(*dinfo->mem->alloc_small) ((j_common_ptr) dinfo, JPOOL_PERMANENT,
						sizeof(jpeg_custom_source_mgr));
		src = (jpeg_custom_source_mgr_ptr) dinfo->src;
		src->stdio_buffer = (JOCTET *)
			(*dinfo->mem->alloc_small) ((j_common_ptr) dinfo, JPOOL_PERMANENT,
						STDIO_BUFFER_SIZE * sizeof(JOCTET));
	} else {
		src = (jpeg_custom_source_mgr_ptr) dinfo->src;
	}

	src->pub.init_source = custom_init_source;
	src->pub.fill_input_buffer = custom_fill_input_buffer;
	src->pub.resync_to_restart = jpeg_resync_to_restart;
	src->pub.skip_input_data = custom_skip_input_data;
	src->pub.term_source = custom_term_source;
	src->infile = infile;
	src->pub.bytes_in_buffer = 0;
	src->pub.next_input_byte = NULL;

	src->buf_ptr = bufptr;
	src->buf = (bufptr ? *bufptr : NULL);
	src->bufsize_ptr = bufsizeptr;
	src->bufused_ptr = bufusedptr;
	src->bufsize = (bufsizeptr ? *bufsizeptr : 0);
	src->incsize = incsize;
}


/* eof :-) */

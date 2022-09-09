/*
 * jpegdest.c
 *
 * Copyright (C) 2014-2022 Timo Kokkonen
 * All Rights Reserved.
 *
 * custom libjpeg "Destination Manager" for saving into RAM
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



/* custom jpeg destination manager object */

typedef struct {
	struct jpeg_destination_mgr pub; /* public fields */

	unsigned char **buf_ptr;
	size_t *bufsize_ptr;
	size_t incsize;

	unsigned char *buf;
	size_t bufsize;

} jpeg_memory_destination_mgr;

typedef jpeg_memory_destination_mgr* jpeg_memory_destination_ptr;




void jpeg_memory_init_destination (j_compress_ptr cinfo)
{
	jpeg_memory_destination_ptr dest = (jpeg_memory_destination_ptr) cinfo->dest;

	dest->pub.next_output_byte = dest->buf;
	dest->pub.free_in_buffer = dest->bufsize;
}


boolean jpeg_memory_empty_output_buffer (j_compress_ptr cinfo)
{
	jpeg_memory_destination_ptr dest = (jpeg_memory_destination_ptr) cinfo->dest;
	unsigned char *newbuf;

	/* abort if incsize is 0 (no expansion of buffer allowed) */
	if (dest->incsize == 0) ERREXIT1(cinfo, JERR_OUT_OF_MEMORY, 42);

	/* otherwise, try expanding buffer... */
	newbuf = realloc(dest->buf,dest->bufsize + dest->incsize);
	if (!newbuf) ERREXIT1(cinfo, JERR_OUT_OF_MEMORY, 42);

	dest->pub.next_output_byte = newbuf + dest->bufsize;
	dest->pub.free_in_buffer = dest->incsize;

	*dest->buf_ptr = newbuf;
	dest->buf = newbuf;
	dest->bufsize += dest->incsize;
	dest->incsize *= 2;

	return TRUE;
}



void jpeg_memory_term_destination (j_compress_ptr cinfo)
{
	jpeg_memory_destination_ptr dest = (jpeg_memory_destination_ptr) cinfo->dest;

	*dest->buf_ptr = dest->buf;
	*dest->bufsize_ptr = dest->bufsize - dest->pub.free_in_buffer;
}



void jpeg_memory_dest (j_compress_ptr cinfo, unsigned char **bufptr, size_t *bufsizeptr, size_t incsize)
{
	jpeg_memory_destination_ptr dest;

	if (!cinfo || !bufptr || !bufsizeptr)
		fatal("invalid call to jpeg_memory_dest()");
	if (!*bufptr || *bufsizeptr == 0)
		fatal("invalid buffer passed to jpeg_memory_dest()");


	/* Allocate destination manager object for compress object, if needed. */
	if (!cinfo->dest) {
		cinfo->dest = (struct jpeg_destination_mgr *)
			(*cinfo->mem->alloc_small) ( (j_common_ptr) cinfo,
						JPOOL_PERMANENT,
						sizeof(jpeg_memory_destination_mgr) );
	}

	dest = (jpeg_memory_destination_ptr)cinfo->dest;

	dest->buf_ptr = bufptr;
	dest->buf = *bufptr;
	dest->bufsize_ptr = bufsizeptr;
	dest->bufsize = *bufsizeptr;
	dest->incsize = incsize;

	dest->pub.init_destination = jpeg_memory_init_destination;
	dest->pub.empty_output_buffer = jpeg_memory_empty_output_buffer;
	dest->pub.term_destination = jpeg_memory_term_destination;
}


/* eof :-) */

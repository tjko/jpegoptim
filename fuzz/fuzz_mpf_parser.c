/*
 * fuzz_mpf_parser.c
 *
 * Component harness for the MPF (CIPA DC-007) marker parser in jpegmpf.c.
 *
 * The input is treated as the payload of an APP2 marker (what libjpeg saves
 * minus the two byte segment length): the harness wraps it in a synthetic
 * minimal JPEG (SOI + APP2 + EOI) and drives the exact same three entry
 * points that optimize() calls inside a real jpegoptim run:
 *   find_mpf_marker()   - marker scan over the whole file image
 *   valid_mpf_marker()  - full Index/Attribute IFD validation
 *   patch_mpf_marker()  - MP entry offset/size rewriting after re-encode
 * A jpeg_saved_marker record is constructed over a private copy of the
 * payload, mirroring the libjpeg-saved marker list the real tool walks.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of JPEGoptim.
 *
 * JPEGoptim is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, either version 3
 * of the License, or (at your option) any later version.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jpeglib.h>
#include "jpegmpf.h"

/* Synthetic minimal JPEG: SOI + APP2(payload) + EOI */
static unsigned char *build_image(const uint8_t *data, size_t size,
				  size_t *image_len)
{
	unsigned char *buf, *p;

	if (size < 4 || size > 0xFFFD)
		return NULL;

	*image_len = size + 2 + 2 + 2 + size + 2;
	buf = malloc(*image_len);
	if (!buf)
		return NULL;

	p = buf;
	*p++ = 0xFF;
	*p++ = 0xD8; /* SOI */
	*p++ = 0xFF;
	*p++ = 0xE2; /* APP2 */
	*p++ = (unsigned char)(((size + 2) >> 8) & 0xFF);
	*p++ = (unsigned char)((size + 2) & 0xFF);
	memcpy(p, data, size);
	p += size;
	*p++ = 0xFF;
	*p++ = 0xD9; /* EOI */

	return buf;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, const size_t size)
{
	unsigned char *image = NULL;
	struct jpeg_marker_struct *marker = NULL;
	uint8_t *payload = NULL;
	mpf_marker_t mpf;
	size_t image_len = 0, offset = 0;

	if (size < 4 || size > 0xFFFD)
		return 0;

	memset(&mpf, 0, sizeof(mpf));

	image = build_image(data, size, &image_len);
	if (!image)
		return 0;

	payload = malloc(size);
	marker = calloc(1, sizeof(*marker));
	if (!payload || !marker) {
		free(payload);
		free(marker);
		free(image);
		return 0;
	}
	memcpy(payload, data, size);

	/* 1) marker scan, as parse_markers() runs it over the file image */
	if (find_mpf_marker(image, image_len, &offset) == 1) {
		/* 2) validation over a saved-marker record, exactly like
		   valid_mpf_marker(marker, offset, &mpf_info, true) */
		marker->marker = 0xE2;
		marker->original_length = (unsigned int)size;
		marker->data_length = (unsigned int)size;
		marker->data = payload;

		if (valid_mpf_marker(marker, offset, &mpf, 1)) {
			/* 3) MP entry rewrite, as done on the re-encoded
			   output buffer (new offset = marker data position,
			   input image size = whole image) */
			patch_mpf_marker(image, image_len, &mpf, offset,
					 image_len);
		}
	}

	free(marker);
	free(payload);
	free(image);

	return 0;
}

/* eof :-) */

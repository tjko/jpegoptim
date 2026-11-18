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
 * After a successful parse, a second synthetic image is built with the APP2
 * marker relocated six bytes deeper and an APP1 stub behind the payload, so
 * patch_mpf_marker() is called the way a real re-encode calls it: on a
 * different image, with the marker at a new offset and the input image size
 * passed separately from the MPF metadata parsed from the input.
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

	/* SOI (2) + APP2 marker (2) + segment length (2) + payload (size) +
	   EOI (2): the payload is counted exactly once. */
	*image_len = size + 8;
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

/* Synthetic re-encoded output: SOI + COM("AB") + APP2(payload) + APP1 stub
   + EOI.  Compared to the input image, the APP2 marker has moved six bytes
   deeper and the image is eighteen bytes longer, so patch_mpf_marker() gets
   a marker that is not at its original offset in an image of a different
   size, while mpf and in_image_size still describe the input image. */
static unsigned char *build_output_image(const uint8_t *data, size_t size,
					 size_t *image_len, size_t *marker_offset)
{
	static const unsigned char com[] =
		{ 0xFF, 0xFE, 0x00, 0x04, 'A', 'B' }; /* COM "AB" */
	static const unsigned char app1[] =
		{ 0xFF, 0xE1, 0x00, 0x02 };           /* empty APP1 stub */
	unsigned char *buf, *p;
	const size_t payload_len = size + 2;

	if (size < 4 || size > 0xFFFD)
		return NULL;

	*marker_offset = 2 + sizeof(com);
	*image_len = size + 8 + sizeof(com) + sizeof(app1);
	buf = malloc(*image_len);
	if (!buf)
		return NULL;

	p = buf;
	*p++ = 0xFF;
	*p++ = 0xD8; /* SOI */
	memcpy(p, com, sizeof(com));
	p += sizeof(com);
	*p++ = 0xFF;
	*p++ = 0xE2; /* APP2 */
	*p++ = (unsigned char)(((size + 2) >> 8) & 0xFF);
	*p++ = (unsigned char)((size + 2) & 0xFF);
	memcpy(p, data, size);
	p += size;
	memcpy(p, app1, sizeof(app1));
	p += sizeof(app1);
	*p++ = 0xFF;
	*p++ = 0xD9; /* EOI */

	return buf;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, const size_t size)
{
	unsigned char *in_image = NULL, *out_image = NULL;
	struct jpeg_marker_struct *marker = NULL;
	uint8_t *payload = NULL;
	mpf_marker_t mpf;
	size_t in_len = 0, out_len = 0, offset = 0, out_offset = 0;

	if (size < 4 || size > 0xFFFD)
		return 0;

	memset(&mpf, 0, sizeof(mpf));

	in_image = build_image(data, size, &in_len);
	if (!in_image)
		return 0;

	payload = malloc(size);
	marker = calloc(1, sizeof(*marker));
	if (!payload || !marker) {
		free(payload);
		free(marker);
		free(in_image);
		return 0;
	}
	memcpy(payload, data, size);

	/* 1) marker scan, as parse_markers() runs it over the file image */
	if (find_mpf_marker(in_image, in_len, &offset) == 1) {
		/* 2) validation over a saved-marker record, exactly like
		   valid_mpf_marker(marker, offset, &mpf_info, true) */
		marker->marker = 0xE2;
		marker->original_length = (unsigned int)size;
		marker->data_length = (unsigned int)size;
		marker->data = payload;

		if (valid_mpf_marker(marker, offset, &mpf, 1)) {
			/* 3) MP entry rewrite for a re-encoded output image:
			   the APP2 marker has moved and the image is longer,
			   while mpf still carries the input image's marker
			   offset and the input size is passed separately --
			   exactly the arguments a real re-encode hands over.
			   This way the secondary-image offset rewrite and the
			   primary-image size update both run. */
			out_image = build_output_image(data, size, &out_len, &out_offset);
			if (out_image)
				patch_mpf_marker(out_image, out_len, &mpf, out_offset, in_len);
		}
	}

	free(marker);
	free(payload);
	free(out_image);
	free(in_image);

	return 0;
}

/* eof :-) */

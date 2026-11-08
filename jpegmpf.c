/* jpegmpf.c - JPEG MPF marker related functions
 *
 * Copyright (c) 2026 Timo Kokkonen
 * All Rights Reserved.
 *
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of JPEGinfo.
 *
 * JPEGinfo is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * JPEGinfo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with JPEGinfo. If not, see <https://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <jpeglib.h>
#include "easy-endian.h"
#include "jpegmpf.h"

//#define DEBUG 1

#if DEBUG > 0
#define DEBUG_PRINT(fmt, ...) {						                 \
		printf("%s:%d %s(): " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
	}
#else
#define DEBUG_PRINT(...)
#endif


/* These come from CIPA DC-007-2009 standard */

static const char *mp_format_identifier = "MPF\0";
static const int mp_format_identifier_len = 4;

static const char *mp_little_endian_magic = "II*\0";
static const char *mp_big_endian_magic = "MM\0*";
static const int mp_endian_magic_len = 4;

enum mpf_tag_ids {
	MPFVersion           = 0xb000,
	NumberOfImages       = 0xb001,
	MPEntry              = 0xb002,
	ImageUIDList         = 0xb003,
	TotalFrames          = 0xb004,

	MPIndividualNum      = 0xb101,

	PanOrientation       = 0xb201,
	PanOverlap_H         = 0xb202,
	PanOverlap_V         = 0xb203,
	BaseViewpointNum     = 0xb204,
	ConvergenceAngle     = 0xb205,
	BaselineLength       = 0xb206,
	VerticalDivergence   = 0xb207,
	AxisDistance_X       = 0xb208,
	AxisDistance_Y       = 0xb209,
	AxisDistacne_Z       = 0xb20a,
	YawAngle             = 0x20b,
	PitchAngle           = 0xb20c,
	RollAngle            = 0xb20d,
};

enum mpf_data_types {
	MP_LONG      = 4,
	MP_UNDEFINED = 7,
};

enum mpf_image_attributes {
	/* Flag bits */
	DependentParentImage_Flag   = (1UL << 31),
	DependentChildImage_Flag    = (1UL << 30),
	RepresentativeImage_Flag    = (1UL << 29),

	/* MP Type codes */
	MPPrimaryImage              = 0x030000,
	LargeThumbnail_Class1       = 0x010001,
	LargeThumbnail_Class2       = 0x010002,
	MultiFrameImage_Panorama    = 0x020001,
	MultiFrameImage_Disparity   = 0x020002,
	MultiFrameImage_MultiAngle  = 0x020003,
	Undefined                   = 0x000000,

	MP_Type_Mask                = 0x03ffff,
};



static uint32_t read32(const void *ptr, mpf_marker_t *mpf)
{
	uint32_t val;

	memcpy(&val, ptr, sizeof(val));

	if (mpf->big_endian)
		return be32toh(val);

	return le32toh(val);
}


static uint16_t read16(const void *ptr, mpf_marker_t *mpf)
{
	uint16_t val;

	memcpy(&val, ptr, sizeof(val));

	if (mpf->big_endian)
		return be16toh(val);

	return le16toh(val);
}


static void write32(void *ptr, uint32_t val, mpf_marker_t *mpf)
{
	uint32_t n_val;

	if (mpf->big_endian)
		n_val = htobe32(val);
	else
		n_val = htole32(val);

	memcpy(ptr, &n_val, sizeof(n_val));
}



static int parse_idf(const char *marker, uint32_t len, uint32_t offset, mpf_marker_t *mpf)
{
	if (!marker || len <= 0 || offset < 0 || offset >= len || !mpf)
		return -1;

	/* Get count of entries in this IDF */
	uint32_t o = offset;
	if (o + 2 > len)
		return -2;
	uint16_t count = read16(marker + o, mpf);
	o += 2;
	uint32_t idf_len = 2 + (count * 12) + 4;
	DEBUG_PRINT("IDF tag_count=%u, len=%u\n", count, idf_len);
	if (idf_len > len - offset)
		return -3;


	/* Parse tags in the IFD */
	uint16_t previous_tag = 0;

	for(int i = 0; i < count; i++) {
		const char *m = marker + o;
		uint16_t tag = read16(m, mpf);
		uint16_t type = read16(m + 2, mpf);
		uint32_t count = read32(m + 4, mpf);
		uint32_t val = read32(m + 8, mpf);

		DEBUG_PRINT("IDFtag=%04x, type=%u, count=%u, default=%u\n", tag, type, count, val);

		/* Tags must be specified in the order of their tag IDs */
		if (tag < previous_tag) {
			DEBUG_PRINT("Invalid tag order (%04x after %04x)\n", tag, previous_tag);
			return -4;
		}
		previous_tag = tag;

		switch(tag) {
		case MPFVersion:
			if (type != MP_UNDEFINED || count !=4)
				return -5;
			memcpy(mpf->version, m + 8, 4);
			mpf->version[4] = 0;
			DEBUG_PRINT(" MPVersion: '%s'\n", mpf->version);
			break;

		case NumberOfImages:
			if (type != MP_LONG || count != 1 || val < 2)
				return -6;
			mpf->number_of_images = val;
			DEBUG_PRINT(" NumberOfImages: %u\n", mpf->number_of_images);
			break;

		case MPEntry:
			val += 4;
			if (type != MP_UNDEFINED ||
				count != (uint64_t)16 * mpf->number_of_images ||
				val < (uint64_t)offset + idf_len ||
				(uint64_t)val + count > len)
				return -7;
			mpf->mpentry_list_offset = val;
			DEBUG_PRINT(" MPEntry list offset: %d (len=%u)\n", mpf->mpentry_list_offset, len);
#if DEBUG > 0
			for(uint32_t img = 0; img < mpf->number_of_images; img++) {
				const char *e = marker + mpf->mpentry_list_offset + (img * 16);
				uint32_t attr = read32(e, mpf);
				uint32_t imgsize = read32(e + 4, mpf);
				uint32_t imgoffset = read32(e + 8, mpf);
				uint16_t dep1 = read16(e + 12, mpf);
				uint16_t dep2 = read16(e + 14, mpf);
				uint32_t imgoffset2 = imgoffset > 0 ? imgoffset + mpf->marker_offset + 4 : 0;

				DEBUG_PRINT("  MPentry%d: attr=%08x, imgsize=%u, imgoffset=%u (%u), dep1=%u, dep2=%u\n",
					img, attr, imgsize, imgoffset, imgoffset2, dep1, dep2);
			}
#endif
			break;

		case ImageUIDList:
			val += 4;
			if (type != MP_UNDEFINED ||
				count != (uint64_t)33 * mpf->number_of_images ||
				val < (uint64_t)offset + idf_len ||
				(uint64_t)val + count > len)
				return -8;
			mpf->imageuid_list_offset = val;
			DEBUG_PRINT(" ImageUIDList offset: %u\n", mpf->imageuid_list_offset);
			break;

		case TotalFrames:
			if (count != 1 || type != MP_LONG)
				return -9;
			mpf->total_frames = val;
			DEBUG_PRINT(" TotalFrames: %u\n", mpf->total_frames);
			break;
		}

		o += 12;
	}


	/* Offset of Next IFD */
	if (o + 4 > len)
		return -10;
	uint32_t next_ifd = read32(marker + o, mpf);
	DEBUG_PRINT(" Offset of Next IFD: %u\n", next_ifd);

	/* Check that offset is within the marker */
	if (next_ifd + 4 >= len)
		return -11;

	return next_ifd;
}


static int read_mpf_marker(const char *data, unsigned int len, mpf_marker_t *mpf, size_t marker_offset)
{
	const char *m = data;

	if (!data || len <= 0 || !mpf)
		return -1;

	DEBUG_PRINT("MPF Maker offset: %lu\n", marker_offset);

	memset(mpf, 0, sizeof(mpf_marker_t));
	mpf->marker_offset = marker_offset;
	mpf->data = data;
	mpf->data_len = len;

	if (len < mp_format_identifier_len + mp_endian_magic_len + 4)
		return 1;

	/* Check MP Format Identifier (4 bytes) */
	if (memcmp(m, mp_format_identifier, mp_format_identifier_len))
		return 2;
	m += mp_format_identifier_len;

	/* Check MP Endian Tag (4 bytes) */
	if (!memcmp(m, mp_little_endian_magic, mp_endian_magic_len)) {
		mpf->big_endian = false;
		DEBUG_PRINT("MP Endian: Little Endian\n");
	}
	else if (!memcmp(m, mp_big_endian_magic, mp_endian_magic_len)) {
		mpf->big_endian = true;
		DEBUG_PRINT("MP Endian: Big Endian\n");
	}
	else {
		return 3;
	}
	m += mp_endian_magic_len;

	/* Offset to First IFD (4 bytes) */
	mpf->first_ifd_offset = read32(m, mpf);
	DEBUG_PRINT("MP First IFD offset: %u\n", mpf->first_ifd_offset);
	if (mpf->first_ifd_offset < 8 || mpf->first_ifd_offset >= len - 4)
		return 4;


	/* Parse IFDs */
	int64_t offset = mpf->first_ifd_offset;
	int ifd_parse_limit = 2;
	do {
		if (ifd_parse_limit-- <= 0)
			return 5;
		DEBUG_PRINT("Parse IFD at %"PRId64"\n", offset);
		/* Note, offset is from start of "MP HEader", so we must add 4
		   to convert to offset from beginning of the marker data. */
		offset = parse_idf(data, len, offset + 4, mpf);
	} while (offset > 0);

	if (offset < 0)
		return 6;

	/* MP Entry list is required for patching the image offsets/sizes */
	if (mpf->mpentry_list_offset == 0 || mpf->number_of_images < 2)
		return 7;

	return 0;
}


int valid_mpf_marker(const jpeg_saved_marker_ptr marker, size_t offset, mpf_marker_t *mpf)
{
	if (!marker || !mpf)
		return -1;

	return (read_mpf_marker((const char*)marker->data, marker->data_length, mpf, offset) == 0);
}


int patch_mpf_marker(unsigned char *image, size_t image_len, mpf_marker_t *mpf, size_t new_offset, size_t in_image_size)
{
	if (!image || image_len <= 0 || !mpf || new_offset <= 0 || in_image_size <= 0)
		return -1;

	DEBUG_PRINT("New MPF offset: %lu (new image size: %lu)\n", new_offset, image_len);

	/* Check that the new MPF marker is actually at 'new_offset' */
	if (memcmp(image + new_offset, mpf->data, mpf->data_len))
		return -2;


	DEBUG_PRINT("Checking %u records in MP Entry list\n", mpf->number_of_images);
	for(int img = 0; img < mpf->number_of_images; img++) {
		unsigned char *e = image + new_offset + mpf->mpentry_list_offset + (img * 16);
		uint32_t imgsize = read32(e + 4, mpf);
		uint32_t imgoffset = read32(e + 8, mpf);
#if DEBUG >0
		uint32_t attr = read32(e, mpf);
		uint16_t dep1 = read16(e + 12, mpf);
		uint16_t dep2 = read16(e + 14, mpf);
#endif

		DEBUG_PRINT(" MPentry%d: attr=%08x, imgsize=%u, imgoffset=%u, dep1=%u, dep2=%u\n",
			img, attr, imgsize, imgoffset, dep1, dep2);

		if (imgoffset > 0) {
			/* Update offset of additional image(s) */

			/* Calculate relative offset from end of original image */
			int64_t rel_o  = (int64_t)imgoffset + (mpf->marker_offset + 4) - in_image_size;
			if (rel_o < 0) {
				DEBUG_PRINT("Invalid relative offset!\n");
				return -3;
			}
			/* Calculate new offset (as MPF marker may have moved and image size may have changed) */
			int64_t new_o = (int64_t)image_len - (new_offset + 4) + rel_o;
			if (new_o < 0) {
				DEBUG_PRINT("Invalid new offset!\n");
				return -4;
			}
			if (imgoffset != new_o) {
				DEBUG_PRINT("  update image offset %u -> %"PRId64" (relative offset: %"PRId64")\n\n",
					imgoffset, new_o, rel_o);
				write32(e + 8, new_o, mpf);
			}
		} else {
			/* Update size of primary image */
			if (imgsize != image_len) {
				DEBUG_PRINT("  update image size %u -> %lu\n", imgsize, image_len);
				write32(e + 4, image_len, mpf);
			}
		}
	}

	return 0;
}


int find_mpf_marker(const unsigned char *image, size_t image_len, size_t *offset)
{
	if (!image || image_len < 4)
		return -1;

	/* Check that image has JPEG signature */
	if (image[0] != 0xff || image[1] != 0xd8)
		return -2;
	DEBUG_PRINT("JPEG marker found at %p (len=%lu)\n", image, image_len);

	int found = 0;
	size_t pos = 2;

	/* Walk the JPEG segmet markers and look for APP2 "MPF" segments. */
	while (pos + 4 <= image_len && image[pos] == 0xff) {
		unsigned char marker = image[pos + 1];

		if (marker == 0xff) {
			/* Skip any padding (fill bytes) */
			pos++;
			continue;
		}
		if (marker == 0x01 || (marker >= 0xd0 && marker <= 0xd7)) {
			/* Skip stand-alone markers (no data payload):
			   TEM (0x01), RST0-RST7 (0xd0-0xd7) */
			DEBUG_PRINT("Skip stand-alone marker 0x%02x\n", marker);
			pos += 2;
			continue;
		}
		if (marker == 0xda || marker == 0xd8 || marker == 0xd9) {
			/* Stop when we see SOS (0xda), SOI (0xd8) or EOI (0xd9) */
			DEBUG_PRINT("Stoping search at marker 0x%02x\n", marker);
			break;
		}

		/* Get marker length */
		size_t len = ((size_t)image[pos + 2] << 8) | image[pos + 3];
		if (len < 2 || pos + 2 + len > image_len) {
			/* Stop if length is not valid */
			DEBUG_PRINT("Marker with bad length 0x%02x (length=%lu)\n", marker, len);
			break;
		}
		DEBUG_PRINT("Found marker 0x%02x at offset %lu (length=%lu)\n", marker, pos, len);

		if (marker == 0xe2 && len >= 6
			&& !memcmp(image + pos + 4, mp_format_identifier, mp_format_identifier_len)) {
			/* Found a APP2 marker that has MPF signature */
			DEBUG_PRINT("Found MPF marker (data offset=%lu)\n", pos + 4);
			if (!found && offset)
				*offset = pos + 4;
			found++;
		}

		pos += 2 + len;
	}

	DEBUG_PRINT("%d MPF marker(s) found\n", found);

	return found;
}

/* eof :-) */

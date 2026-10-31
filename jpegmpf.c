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
#include <string.h>
#include <endian.h>
#include <jpeglib.h>

#include "jpegmpf.h"

#define DEBUG 1

#if DEBUG > 0
#define DEBUG_PRINT(fmt, ...) {						                 \
		printf("%s:%d %s(): " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
	}
#else
#define DEBUG_PRINT(...)
#endif


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
	DependentParentImage_Flag   = (1UL << 31),
	DependentChildImage_Flag    = (1UL << 30),
	RepresentativeImage_Flag    = (1UL << 29),

	MPPrimaryImage              = 0x030000,
	LargeThumbnail_Class1       = 0x010001,
	LargeThumbnail_Class2       = 0x010002,
	MultiFrameImage_Panorama    = 0x020001,
	MultiFrameImage_Disparity   = 0x020002,
	MultiFrameImage_MultiAngle  = 0x020003,

	Undefined                   = 0x000000,
};


static uint32_t read32(const void *ptr, mpf_marker_t *mpf)
{
	uint32_t val = *((uint32_t*)ptr);

	if (mpf->big_endian)
		return be32toh(val);

	return le32toh(val);
}


static uint32_t read16(const void *ptr, mpf_marker_t *mpf)
{
	uint16_t val = *((uint16_t*)ptr);

	if (mpf->big_endian)
		return be16toh(val);

	return le16toh(val);
}


static int parse_idf(const char *marker, unsigned int len, int offset, mpf_marker_t *mpf)
{
	if (!marker || len <= 0 || offset < 0 || !mpf)
		return -1;

	/* Get count of entries in this IDF */
	int o = offset;
	if (o + 2 > len)
		return -2;
	uint16_t count = read16(marker + o, mpf);
	o += 2;
	uint16_t idf_len = 2 + (count * 12) + 4;
	DEBUG_PRINT("IDF tag_count=%u, len=%u\n", count, idf_len);
	if (idf_len > len)
		return -3;


	/* Parse entries */
	for(int i = 0; i < count; i++) {
		const char *m = marker + o;
		uint16_t tag = read16(m, mpf);
		uint16_t type = read16(m + 2, mpf);
		uint32_t count = read32(m + 4, mpf);
		uint32_t val = read32(m + 8, mpf);

		DEBUG_PRINT("IDFtag=%04x, type=%u, count=%u, default=%u\n", tag, type, count, val);

		switch(tag) {
		case MPFVersion:
			if (count != 4 || type != MP_UNDEFINED)
				return -3;
			memcpy(mpf->version, m + 8, 4);
			mpf->version[4] = 0;
			DEBUG_PRINT(" MPVersion: '%s'\n", mpf->version);
			break;

		case NumberOfImages:
			if (count != 1 || type != MP_LONG || val < 2)
				return -4;
			mpf->number_of_images = val;
			DEBUG_PRINT(" NumberOfImages: %u\n", mpf->number_of_images);
			break;

		case MPEntry:
			val += 4;
			if (count != 16 * mpf->number_of_images || type != MP_UNDEFINED ||
				val < offset + idf_len || val + count > len)
				return -5;
			mpf->mpentry_list_offset = val;
			DEBUG_PRINT(" MPEntry list offset: %d (len=%u)\n", mpf->mpentry_list_offset, len);
			for(int img = 0; img < mpf->number_of_images; img++) {
				const char *e = marker + mpf->mpentry_list_offset + (img * 16);
				uint32_t attr = read32(e, mpf);
				uint32_t imgsize = read32(e + 4, mpf);
				uint32_t imgoffset = read32(e + 8, mpf);
				uint16_t dep1 = read16(e + 12, mpf);
				uint16_t dep2 = read16(e + 14, mpf);
				DEBUG_PRINT("  MPentry%d: attr=%08x, imgsize=%u, imgoffset=%u, dep1=%u, dep2=%u\n",
					img, attr, imgsize, imgoffset, dep1, dep2);
			}
			break;

		case ImageUIDList:
			val += 4;
			if (count != 33 * mpf->number_of_images || type != MP_UNDEFINED ||
				val < offset + idf_len || val + count > len)
				return -7;
			mpf->imageuid_list_offset = val;
			DEBUG_PRINT(" ImageUIDList offset: %u\n", mpf->imageuid_list_offset);
			break;

		case TotalFrames:
			if (count != 1 || type != MP_UNDEFINED)
				return -8;
			mpf->total_frames = val;
			DEBUG_PRINT(" TotalFrames: %u\n", mpf->total_frames);
			break;
		}

		o += 12;
	}


	/* Offset of Next IFD */
	if (o + 4 > len)
		return -3;
	uint16_t next_ifd = read32(marker + o, mpf);
	DEBUG_PRINT(" Offset of Next IFD: %u\n", next_ifd);

	return next_ifd;
}


static int read_mpf_marker(const char *marker, unsigned int len, mpf_marker_t *mpf)
{
	const char *m = marker;

	if (!marker || len <= 0 || !mpf)
		return -1;

	memset(mpf, 0, sizeof(mpf_marker_t));

	if (len < 12)
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
	if (mpf->first_ifd_offset < 8)
		return 4;


	/* Parse IFDs */
	int offset = mpf->first_ifd_offset;
	do {
		DEBUG_PRINT("Parse IFD at %d\n", offset);
		offset = parse_idf(marker, len, offset + 4, mpf);
	} while (offset > 0);
	if (offset < 0)
		return 5;


	return 0;
}


int valid_mpf_marker(const jpeg_saved_marker_ptr marker)
{
	mpf_marker_t mpf;

	if (!marker)
		return -1;

	return (read_mpf_marker((const char*)marker->data, marker->data_length, &mpf) == 0);
}



/* eof :-) */

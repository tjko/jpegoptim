/* jpegmarker.c - JPEG marker functions
 *
 * Copyright (c) 1997-2023 Timo Kokkonen
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
#include <ctype.h>
#include <jpeglib.h>

#include "jpegmarker.h"


struct marker_name {
	unsigned char marker;
	char *name;
};

const struct marker_name jpeg_marker_names[] = {
	{ JPEG_COM,		"COM" },
	{ JPEG_APP0 + 0,	"APP0" },
	{ JPEG_APP0 + 1,	"APP1" },
	{ JPEG_APP0 + 2,	"APP2" },
	{ JPEG_APP0 + 3,	"APP3" },
	{ JPEG_APP0 + 4,	"APP4" },
	{ JPEG_APP0 + 5,	"APP5" },
	{ JPEG_APP0 + 6,	"APP6" },
	{ JPEG_APP0 + 7,	"APP7" },
	{ JPEG_APP0 + 8,	"APP8" },
	{ JPEG_APP0 + 9,	"APP9" },
	{ JPEG_APP0 + 10,	"APP10" },
	{ JPEG_APP0 + 11,	"APP11" },
	{ JPEG_APP0 + 12,	"APP12" },
	{ JPEG_APP0 + 13,	"APP13" },
	{ JPEG_APP0 + 14,	"APP14" },
	{ JPEG_APP0 + 15,	"APP15" },
	{ 0, 0 }
};

const struct jpeg_special_marker_type jpeg_special_marker_types[] = {
	{ JPEG_APP0,		"JFIF",		5,	"JFIF\0" },
	{ JPEG_APP0,		"JFXX",		5,	"JFXX\0" },
	{ JPEG_APP0,		"CIFF",		2,	"II" },
	{ JPEG_APP0,		"CIFF",		2,	"MM" },
	{ JPEG_APP0,		"AVI1",		4,	"AVI1" },
	{ JPEG_APP0 + 1,	"Exif",		6,	"Exif\0\0" },
	{ JPEG_APP0 + 1,	"XMP",		29,	"http://ns.adobe.com/xap/1.0/\0" },
	{ JPEG_APP0 + 1,	"XMP",		34,	"http://ns.adobe.com/xmp/extension/\0" },
	{ JPEG_APP0 + 1,	"QVCI",		5,	"QVCI\0" },
	{ JPEG_APP0 + 1,	"FLIR",		5,	"FLIR\0" },
	{ JPEG_APP0 + 2,	"ICC",		12,	"ICC_PROFILE\0" },
	{ JPEG_APP0 + 2,	"FPXR",		5,	"FPXR\0" },
	{ JPEG_APP0 + 2,	"MPF",		4,	"MPF\0" },
	{ JPEG_APP0 + 3,	"Meta",		6,	"Meta\0\0" },
	{ JPEG_APP0 + 3,	"Meta",		6,	"META\0\0" },
	{ JPEG_APP0 + 3,	"Meta",		6,	"Exif\0\0" },
	{ JPEG_APP0 + 3,	"Stim",		5,	"Stim\0" },
	{ JPEG_APP0 + 3,	"JPS",		8,	"_JPSJPS_" },
	{ JPEG_APP0 + 4,	"Scalado",	8,	"SCALADO\0" },
	{ JPEG_APP0 + 4,	"FPXR",		5,	"FPXR\0" },
	{ JPEG_APP0 + 5,	"RMETA",	6,	"RMETA\0" },
	{ JPEG_APP0 + 6,	"EPPIM",	6,	"EPPIM\0" },
	{ JPEG_APP0 + 6,	"NITF",		5,	"NTIF\0" },
	{ JPEG_APP0 + 6,	"GoPro",	6,	"GoPro\0" },
	{ JPEG_APP0 + 8,	"SPIFF",	6,	"SPIFF\0" },
	{ JPEG_APP0 + 10,	"AROT",		6,	"AROT\0\0" },
	{ JPEG_APP0 + 11,	"HDR",		6,	"HDR_RI" },
	{ JPEG_APP0 + 13,	"IPTC",		14,	"Photoshop 3.0\0" },
	{ JPEG_APP0 + 13,	"IPTC",		18,	"Adobe_Photoshop2.5" },
	{ JPEG_APP0 + 13,	"AdobeCM",	8,	"Adobe_CM" },
	{ JPEG_APP0 + 14,	"Adobe",	5,	"Adobe" },
	{ 0, NULL, 0, NULL }
};



const char* jpeg_marker_name(unsigned int marker)
{
	int i = 0;

	while (jpeg_marker_names[i].name) {
		if (jpeg_marker_names[i].marker == marker)
			return jpeg_marker_names[i].name;
		i++;
	}

	return "Unknown";
}

size_t jpeg_special_marker_types_count()
{
	int i = 0;

	while (jpeg_special_marker_types[i].name)
		i++;

	return i;
}

int jpeg_special_marker(jpeg_saved_marker_ptr marker)
{
	int i = 0;

	if (!marker)
		return -1;

	while (jpeg_special_marker_types[i].name) {
		const struct jpeg_special_marker_type *m = &jpeg_special_marker_types[i];

		if (marker->marker == m->marker && marker->data_length >= m->ident_len) {
			if (m->ident_len < 1)
				return i;
			if (!memcmp(marker->data, m->ident_str, m->ident_len))
				return i;
		}
		i++;
	}

	return -2;
}


const char* jpeg_special_marker_name(jpeg_saved_marker_ptr marker)
{
	int i = jpeg_special_marker(marker);

	return (i >= 0 ? jpeg_special_marker_types[i].name : "Unknown");
}


/* eof :-) */

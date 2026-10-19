/*
 * stripai.c
 *
 * Copyright (C) 2026 Matteo Croce
 * All Rights Reserved.
 *
 * Removal of the metadata telling that an image was generated or edited
 * with AI: C2PA manifests (Content Credentials), the IPTC digital source
 * type in XMP, and the generation parameters (prompt, seed, sampler...)
 * that Stable Diffusion UIs store in the Exif UserComment.
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
#include <ctype.h>
#include <jpeglib.h>

#include "jpegoptim.h"

#define XMP_ID "http://ns.adobe.com/xap/1.0/"
#define XMP_ID_LEN (sizeof(XMP_ID))	/* including the NUL terminator */
#define EXIF_ID_LEN 6			/* "Exif\0\0" */

#define EXIF_IFD_POINTER 0x8769
#define EXIF_USER_COMMENT 0x9286


/* Algorithmic IPTC digital source types: trainedAlgorithmicMedia,
   compositeWithTrainedAlgorithmicMedia, algorithmicMedia, ... */
static const char *ai_source_types[] = {
	"algorithmicmedia",
	"compositesynthetic",
	"algorithmicallyenhanced",
	NULL
};

/* References to a C2PA manifest (dcterms:provenance) */
static const char *c2pa_refs[] = {
	"c2pa",
	NULL
};

/* Generation parameters, as written by AUTOMATIC1111, Forge and SD.Next */
static const char *ai_parameters[] = {
	"negative prompt:",
	"cfg scale:",
	"model hash:",
	NULL
};


/* Case insensitive search of any of the strings in buf. NUL bytes in buf
   are skipped, so that UTF-16 text matches as well. */
static int has_any(const unsigned char *buf, size_t len, const char **strs)
{
	for (; *strs; strs++) {
		for (size_t i = 0; i < len; i++) {
			const char *s = *strs;
			size_t j = i;

			if (buf[i] == 0)
				continue;

			while (*s && j < len) {
				if (buf[j] == 0) {
					j++;
					continue;
				}
				if (tolower(buf[j]) != *s)
					break;
				s++;
				j++;
			}
			if (!*s)
				return 1;
		}
	}

	return 0;
}


static long find(const unsigned char *buf, size_t len, size_t pos,
		const char *str, size_t str_len)
{
	for (; pos + str_len <= len; pos++) {
		if (!memcmp(buf + pos, str, str_len))
			return pos;
	}

	return -1;
}


static int is_name_char(int c)
{
	return (isalnum(c) || c == '_' || c == '-' || c == '.');
}


/* Remove from an XMP packet the properties having the given local name
   and a value containing any of the given strings. Both the attribute
   (prefix:name="value") and the element (<prefix:name>...</prefix:name>)
   forms are handled. Returns the number of properties removed. */
static int xmp_remove_property(unsigned char *buf, size_t *len,
			const char *name, const char **values)
{
	size_t name_len = strlen(name);
	size_t pos = 0;
	int count = 0;
	long i;

	while ((i = find(buf, *len, pos, name, name_len)) >= 0) {
		size_t qname, start, end, value;

		pos = value = i + name_len;

		/* the name must be prefixed by a namespace: "prefix:name" */
		if (i < 2 || buf[i - 1] != ':')
			continue;
		qname = i - 1;
		while (qname > 0 && is_name_char(buf[qname - 1]))
			qname--;
		if (qname == 0 || qname == (size_t)i - 1)
			continue;

		if (buf[qname - 1] == '<') {
			/* element: <prefix:name .../> or <prefix:name ...>...</prefix:name> */
			long gt = find(buf, *len, pos, ">", 1);

			if (gt < 0)
				continue;
			start = qname - 1;
			end = gt + 1;
			if (buf[gt - 1] != '/') {
				size_t qname_len = value - qname;
				long close = gt;

				while ((close = find(buf, *len, close + 1, "</", 2)) >= 0) {
					if (close + 2 + qname_len < *len
					    && !memcmp(buf + close + 2, buf + qname, qname_len)
					    && !is_name_char(buf[close + 2 + qname_len]))
						break;
				}
				if (close < 0 || (gt = find(buf, *len, close, ">", 1)) < 0)
					continue;
				end = gt + 1;
			}
		} else if (isspace(buf[qname - 1])) {
			/* attribute: prefix:name="value", removed with its leading space */
			size_t p = pos;
			long quote;

			while (p < *len && isspace(buf[p]))
				p++;
			if (p >= *len || buf[p++] != '=')
				continue;
			while (p < *len && isspace(buf[p]))
				p++;
			if (p >= *len || (buf[p] != '"' && buf[p] != '\''))
				continue;
			if ((quote = find(buf, *len, p + 1, (char *)buf + p, 1)) < 0)
				continue;
			start = qname - 1;
			end = quote + 1;
		} else {
			continue;
		}

		if (!has_any(buf + value, end - value, values))
			continue;

		memmove(buf + start, buf + end, *len - end);
		*len -= end - start;
		pos = start;
		count++;
	}

	return count;
}


static unsigned int get16(const unsigned char *p, int le)
{
	return (le ? p[0] | p[1] << 8 : p[0] << 8 | p[1]);
}


static unsigned long get32(const unsigned char *p, int le)
{
	return (le ? get16(p, le) | (unsigned long)get16(p + 2, le) << 16
		: (unsigned long)get16(p, le) << 16 | get16(p + 2, le));
}


/* Look for a tag in a TIFF IFD, returning the offset of its entry,
   or 0 if not found. */
static size_t ifd_find_tag(const unsigned char *tiff, size_t len, int le,
			size_t ifd, unsigned int tag)
{
	unsigned int n;

	if (ifd < 8 || ifd >= len || len - ifd < 2)
		return 0;
	n = get16(tiff + ifd, le);
	for (unsigned int i = 0; i < n; i++) {
		size_t entry = ifd + 2 + i * 12;

		if (entry + 12 > len)
			break;
		if (get16(tiff + entry, le) == tag)
			return entry;
	}

	return 0;
}


/* Blank in place the Exif UserComment, if it holds generation parameters.
   It is not removed, so that no offset in the Exif data changes. */
static int exif_clear_user_comment(unsigned char *data, size_t len)
{
	unsigned char *tiff = data + EXIF_ID_LEN;
	size_t entry, offset, count;
	int le;

	if (len < EXIF_ID_LEN + 8)
		return 0;
	len -= EXIF_ID_LEN;
	if (!memcmp(tiff, "II", 2))
		le = 1;
	else if (!memcmp(tiff, "MM", 2))
		le = 0;
	else
		return 0;

	if (!(entry = ifd_find_tag(tiff, len, le, get32(tiff + 4, le), EXIF_IFD_POINTER)))
		return 0;
	if (!(entry = ifd_find_tag(tiff, len, le, get32(tiff + entry + 8, le), EXIF_USER_COMMENT)))
		return 0;

	/* 8 bytes of character code, then the comment: as the UserComment
	   is of UNDEFINED type, its size is the count itself */
	count = get32(tiff + entry + 4, le);
	if (count <= 8)
		return 0;
	offset = get32(tiff + entry + 8, le);
	if (offset > len || count > len - offset)
		return 0;
	if (!has_any(tiff + offset + 8, count - 8, ai_parameters))
		return 0;

	memset(tiff + offset + 8, 0, count - 8);

	return 1;
}


/* Remove the AI metadata from the markers saved by the decompressor.
   Returns the number of items removed. */
int strip_ai_markers(struct jpeg_decompress_struct *dinfo)
{
	jpeg_saved_marker_ptr *mp = &dinfo->marker_list;
	long c2pa_box = -1;
	int count = 0;

	while (*mp) {
		jpeg_saved_marker_ptr m = *mp;

		if (m->marker == JPEG_APP0 + 11 && m->data_length >= 8
		    && !memcmp(m->data, "JP", 2)) {
			/* JUMBF: "JP", box instance and sequence number, then the
			   superbox, which may continue in other APP11 markers of the
			   same box instance. The first one has the description box
			   telling the C2PA content type. */
			long box = get16(m->data + 2, 0);

			if (m->data_length >= 28 && !memcmp(m->data + 12, "jumb", 4)
			    && !memcmp(m->data + 20, "jumdc2pa", 8))
				c2pa_box = box;
			if (box == c2pa_box) {
				*mp = m->next;
				count++;
				continue;
			}
		} else if (m->marker == JPEG_APP0 + 1 && m->data_length > XMP_ID_LEN
			   && !memcmp(m->data, XMP_ID, XMP_ID_LEN)) {
			size_t len = m->data_length - XMP_ID_LEN;
			int n;

			n = xmp_remove_property(m->data + XMP_ID_LEN, &len,
						"DigitalSourceType", ai_source_types);
			n += xmp_remove_property(m->data + XMP_ID_LEN, &len,
						"provenance", c2pa_refs);
			m->data_length = XMP_ID_LEN + len;
			count += n;
		} else if (m->marker == JPEG_APP0 + 1 && m->data_length > EXIF_ID_LEN
			   && !memcmp(m->data, "Exif\0\0", EXIF_ID_LEN)) {
			count += exif_clear_user_comment(m->data, m->data_length);
		}

		mp = &m->next;
	}

	return count;
}

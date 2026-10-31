/* jpegmpf.h
 *
 * Copyright (c) 2026 Timo Kokkonen
 * All Rights Reserved.
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

#ifndef JPEGMPF_H
#define JPEGMPF_H 1

#include <stdint.h>
#include <stdbool.h>
#include <jpeglib.h>

typedef struct mpf_marker_t {
	bool big_endian;
	uint32_t first_ifd_offset;
	char version[4 + 1];
	uint32_t number_of_images;
	uint32_t mpentry_list_offset;
	uint32_t imageuid_list_offset;
	uint32_t total_frames;
} mpf_marker_t;


int valid_mpf_marker(const jpeg_saved_marker_ptr marker);

#endif /* JPEGMPF_H */

/* eof :-) */

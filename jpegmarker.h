/* jpegmarker.h
 *
 * Copyright (c) 1997-2023 Timo Kokkonen
 *
 */

#ifndef JPEGMARKER_H
#define JPEGMARKER_H 1

struct jpeg_special_marker_type {
	unsigned int marker;
	char *name;
	unsigned int ident_len;
	char *ident_str;
};


const extern struct jpeg_special_marker_type jpeg_special_marker_types[];


const char* jpeg_marker_name(unsigned int marker);
const char* jpeg_special_marker_name(jpeg_saved_marker_ptr marker);
int jpeg_special_marker(jpeg_saved_marker_ptr marker);
size_t jpeg_special_marker_types_count();


#endif /* JPEGMARKER_H */

/* eof :-) */

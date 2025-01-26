/* misc.c
 *
 * Copyright (C) 1996-2025 Timo Kokkonen
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>


#include "jpegoptim.h"


FILE* create_file(const char *name)
{
	FILE *f;
	int fd;

	if (!name)
		return NULL;

#ifdef WIN32
	fd = open(name, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, _S_IREAD | _S_IWRITE);
#else
	fd = open(name, O_WRONLY | O_CREAT | O_TRUNC, S_IWUSR | S_IRUSR);
#endif
	if (fd < 0)
		return NULL;
	if (!(f = fdopen(fd, "wb"))) {
		close(fd);
		return NULL;
	}

	return f;
}


FILE *create_temp_file(const char *tmpdir, const char *name, char *filename, size_t filename_len)
{
	FILE *f;
	int newlen;

#ifdef HAVE_MKSTEMPS
	/* Rely on mkstemps() to create us temporary file safely... */
	newlen = snprintf(filename, filename_len, "%s%s-%u-%u.XXXXXX.tmp",
			tmpdir, name, getuid(), getpid());
#else
	/* If platform is missing mkstemps(), try to create at least somewhat "safe" temp file... */
	newlen = snprintf(filename, filename_len, "%s%s-%u-%u.%lu.tmp",
			tmpdir, name, getuid(), getpid(), (unsigned long)time(NULL));
#endif
	if (newlen >= filename_len) {
		warn("temp filename too long: %s", filename);
		return NULL;
	}

#ifdef HAVE_MKSTEMPS
	int tmpfd = mkstemps(filename, 4);
	if (tmpfd < 0) {
		warn("error creating temp file: mkstemps('%s', 4) failed", filename);
		return NULL;
	}
	f = fdopen(tmpfd, "wb");
#else
	f = create_file(filename);
#endif

	return f;
}


int delete_file(const char *name)
{
	int retval;

	if (!name)
		return -1;

	if (verbose_mode > 1 && !quiet_mode)
		fprintf(stderr,"deleting: %s\n",name);
	if ((retval=unlink(name)) && !quiet_mode)
		warn("error removing file: %s",name);

	return retval;
}


long file_size(FILE *fp)
{
	struct stat buf;

	if (!fp)
		return -1;
	if (fstat(fileno(fp),&buf) != 0)
		return -2;

	return (long)buf.st_size;
}


int is_directory(const char *pathname)
{
	struct stat buf;

	if (!pathname)
		return 0;

	if (stat(pathname,&buf) != 0)
		return 0;

	return (S_ISDIR(buf.st_mode) ? 1 : 0);
}


int is_file(const char *filename, struct stat *st)
{
	struct stat buf;

	if (!filename)
		return 0;

	if (lstat(filename,&buf) != 0)
		return 0;
	if (st)
		*st=buf;

	return (S_ISREG(buf.st_mode) ? 1 : 0);
}


int file_exists(const char *pathname)
{
	struct stat buf;

	if (!pathname)
		return 0;

	return (stat(pathname,&buf) == 0 ? 1 : 0);
}


int rename_file(const char *old_path, const char *new_path)
{
	if (!old_path || !new_path)
		return -1;
#ifdef WIN32
	if (file_exists(new_path))
		delete_file(new_path);
#endif
	return rename(old_path,new_path);
}


#define COPY_BUF_SIZE  (256 * 1024)

int copy_file(const char *srcfile, const char *dstfile)
{
	FILE *in,*out;
	unsigned char *buf;
	int r,w;
	int err=0;

	if (!srcfile || !dstfile)
		return -1;

	if (!(in = fopen(srcfile, "rb"))) {
		warn("failed to open file for reading: %s", srcfile);
		return -2;
	}
	if (!(out = create_file(dstfile))) {
		fclose(in);
		warn("failed to open file for writing: %s", dstfile);
		return -3;
	}

	if (!(buf = calloc(COPY_BUF_SIZE, 1)))
		fatal("out of memory");


	do {
		r = fread(buf, 1, COPY_BUF_SIZE, in);
		if (r > 0) {
			w = fwrite(buf, 1, r, out);
			if (w != r) {
				err=1;
				warn("error writing to file: %s", dstfile);
				break;
			}
		} else {
			if (ferror(in)) {
				err=2;
				warn("error reading from file: %s", srcfile);
				break;
			}
		}
	} while (!feof(in));

	fclose(out);
	fclose(in);
	free(buf);

	return err;
}


char *fgetstr(char *s, size_t size, FILE *stream)
{
	char *p;

	if (!s || size < 1 || !stream)
		return NULL;

	if (!fgets(s, size, stream))
		return NULL;

	p = s + strnlen(s, size) - 1;
	while ((p >= s) && ((*p == 10) || (*p == 13)))
		*p--=0;

	return s;
}


char *splitdir(const char *pathname, char *buf, size_t size)
{
	char *s;
	int len = 0;

	if (!pathname || !buf || size < 1)
		return NULL;

	if ((s = strrchr(pathname, DIR_SEPARATOR_C)))
		len = (s - pathname) + 1;
	if (len >= size)
		return NULL;
	if (len > 0)
		memcpy(buf, pathname, len);
	buf[len] = 0;

	return buf;
}


char *splitname(const char *pathname, char *buf, size_t size)
{
	const char *s = NULL;
	int len;

	if (!pathname || !buf || size < 1)
		return NULL;

	if ((s = strrchr(pathname, DIR_SEPARATOR_C)))
		s++;
	else
		s=pathname;

	if ((len = strlen(s)) >= size)
		return NULL;
	if (len > 0)
		memcpy(buf, s, len);
	buf[len] = 0;

	return buf;
}


char *strncopy(char *dst, const char *src, size_t size)
{
	if (!dst || !src || size < 1)
		return dst;

	if (size > 1)
		strncpy(dst, src, size - 1);
	dst[size - 1] = 0;

	return dst;
}


char *strncatenate(char *dst, const char *src, size_t size)
{
	int used, free;

	if (!dst || !src || size < 1)
		return dst;

	/* Check if dst string is already "full" ... */
	used = strnlen(dst, size);
	if ((free = size - used) <= 1)
		return dst;

	return strncat(dst + used, src, free - 1);
}


char *str_add_list(char *dst, size_t size, const char *src, const char *delim)
{
	if (!dst || !src || !delim || size < 1)
		return dst;

	if (strnlen(dst, size) > 0)
		strncatenate(dst, delim, size);

	return strncatenate(dst, src, size);
}


void fatal(const char *format, ...)
{
	va_list args;

	fprintf(stderr, PROGRAMNAME ": ");
	va_start(args,format);
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr,"\n");
	fflush(stderr);

	exit(3);
}


void warn(const char *format, ...)
{
	va_list args;

	if (quiet_mode) return;

	fprintf(stderr, PROGRAMNAME ": ");
	va_start(args,format);
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr,"\n");
	fflush(stderr);
}


/* eof :-) */

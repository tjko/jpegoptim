/* jpegoptim.h
 *
 * JPEGoptim headers
 */

#ifndef _JPEGOPTIM_H
#define _JPEGOPTIM_H 1

#ifdef	__cplusplus
extern "C" {
#endif


#ifdef WIN32
#include "win32_compat.h"
#define DIR_SEPARATOR_C '\\'
#define DIR_SEPARATOR_S "\\"
#else
#include <sys/param.h>
#include <utime.h>
#define DIR_SEPARATOR_C '/'
#define DIR_SEPARATOR_S "/"
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <jpeglib.h>

#ifdef BROKEN_METHODDEF
#undef METHODDEF
#define METHODDEF(x) static x
#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif

#define EXIF_JPEG_MARKER   JPEG_APP0+1
#define EXIF_IDENT_STRING  "Exif\000\000"
#define EXIF_IDENT_STRING_SIZE 6

#define IPTC_JPEG_MARKER   JPEG_APP0+13

#define ICC_JPEG_MARKER   JPEG_APP0+2
#define ICC_IDENT_STRING  "ICC_PROFILE\0"
#define ICC_IDENT_STRING_SIZE 12

#define XMP_JPEG_MARKER   JPEG_APP0+1
#define XMP_IDENT_STRING  "http://ns.adobe.com/xap/1.0/\000"
#define XMP_IDENT_STRING_SIZE 29


#define PROGRAMNAME "jpegoptim"

extern int verbose_mode;
extern int quiet_mode;


/* misc.c */
int delete_file(char *name);
long file_size(FILE *fp);
int is_directory(const char *path);
int is_file(const char *filename, struct stat *st);
int file_exists(const char *pathname);
int rename_file(const char *old_path, const char *new_path);
char *splitdir(const char *pathname, char *buf, int buflen);
char *splitname(const char *pathname, char *buf, int buflen);
void fatal(const char *format, ...);
void warn(const char *format, ...);


/* jpegdest.c */
void jpeg_memory_dest (j_compress_ptr cinfo, unsigned char **bufptr, size_t *bufsizeptr, size_t incsize);



#ifdef	__cplusplus
}
#endif

#endif /* _JPEGOPTIM_H */

/* misc.c
 *
 * Copyright (C) 1996-2014 Timo Kokkonen
 * All Rights Reserved.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>


#include "jpegoptim.h"


int delete_file(char *name)
{
  int retval;

  if (!name) return -1;
  if (verbose_mode > 1 && !quiet_mode) fprintf(stderr,"deleting: %s\n",name);
  if ((retval=unlink(name)) && !quiet_mode)
    warn("error removing file: %s",name);

  return retval;
}


long file_size(FILE *fp)
{
  struct stat buf;

  if (!fp) return -1;
  if (fstat(fileno(fp),&buf)) return -2;
  return (long)buf.st_size;
}


int is_directory(const char *pathname)
{
  struct stat buf;

  if (!pathname) return 0;
  if (stat(pathname,&buf) != 0) return 0;
  if (S_ISDIR(buf.st_mode)) return 1;
  return 0;
}


int is_file(const char *filename, struct stat *st)
{
 struct stat buf;

 if (!filename) return 0;
 if (lstat(filename,&buf) != 0) return 0;
 if (st) *st=buf;
 if (S_ISREG(buf.st_mode)) return 1;
 return 0;
}


int file_exists(const char *pathname)
{
  struct stat buf;

  if (!pathname) return 0;
  if (stat(pathname,&buf) != 0) return 0;
  return 1;
}


int rename_file(const char *old_path, const char *new_path)
{
  if (!old_path || !new_path) return -1;
#ifdef WIN32
  if (file_exists(new_path)) delete_file(new_path);
#endif
  return rename(old_path,new_path);
}


#define COPY_BUF_SIZE  (256*1024)

int copy_file(const char *srcfile, const char *dstfile)
{
  FILE *in,*out;
  unsigned char buf[COPY_BUF_SIZE];
  int r,w;
  int err=0;

  if (!srcfile || !dstfile) return -1;

  in=fopen(srcfile,"rb");
  if (!in) {
    warn("failed to open file for reading: %s", srcfile);
    return -2;
  }
  out=fopen(dstfile,"wb");
  if (!out) {
    fclose(in);
    warn("failed to open file for writing: %s", dstfile);
    return -3;
  }


  do {
    r=fread(buf,1,sizeof(buf),in);
    if (r > 0) {
      w=fwrite(buf,1,r,out);
      if (w != r) {
	err=1;
	warn("error writing to file: %s", dstfile);
	break;
      }
    } else {
      if (ferror(in)) {
	  err=2;
	  warn("error reading file: %s", srcfile);
	  break;
      }
    }
  } while (!feof(in));

  fclose(out);
  fclose(in);
  return err;
}


char *splitdir(const char *pathname, char *buf, int buflen)
{
  char *s = NULL;
  int size = 0;

  if (!pathname || !buf || buflen < 2) return NULL;

  if ((s = strrchr(pathname,DIR_SEPARATOR_C))) size = (s-pathname)+1;
  if (size >= buflen) return NULL;
  if (size > 0) memcpy(buf,pathname,size);
  buf[size]=0;

  return buf;
}

char *splitname(const char *pathname, char *buf, int buflen)
{
  const char *s = NULL;
  int size = 0;

  if (!pathname || !buf || buflen < 2) return NULL;

  if ((s = strrchr(pathname,DIR_SEPARATOR_C))) {
    s++;
  } else {
    s=pathname;
  }

  size=strlen(s);
  if (size >= buflen) return NULL;
  if (size > 0) memcpy(buf,s,size);
  buf[size]=0;

  return buf;
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


/* eof */

/*******************************************************************
 * JPEGoptim
 * Copyright (c) Timo Kokkonen, 1996.
 *
 * requires libjpeg.a (from JPEG Group's JPEG software 
 *                     release 6a or later...)
 *
 * to compile type: gcc -o jpegoptim jpegoptim.c -ljpeg
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#ifndef HPUX
#include <getopt.h>
#endif
#include <signal.h>
#include <string.h>
#include <jpeglib.h>
#include <setjmp.h>
#ifdef SGI
#include <libgen.h>
#endif

#define VERSIO "1.1"

#define TEMPDIR "."
#define TEMP2DIR "/tmp"

#ifdef SGI
#undef METHODDEF
#define METHODDEF(x) static x
#endif


struct my_error_mgr {
  struct jpeg_error_mgr pub;
  jmp_buf setjmp_buffer;   
};

typedef struct my_error_mgr * my_error_ptr;

struct jpeg_decompress_struct dinfo;
struct jpeg_compress_struct cinfo;
struct my_error_mgr jcerr,jderr;

#ifdef LONG_OPTIONS
struct option long_options[] = {
  {"verbose",0,0,'v'},
  {"help",0,0,'h'},
  {"quiet",0,0,'q'},
  {"max",1,0,'m'},
  {"totals",0,0,'t'},
  {"noaction",0,0,'n'},
  {"dest",1,0,'d'},
  {"force",0,0,'f'},
  {0,0,0,0}
};
#endif

int verbose_mode = 0;
int quiet_mode = 0;
int global_error_counter = 0;
char *outfname = NULL;
FILE *infile = NULL, *outfile = NULL;

/*****************************************************************/

METHODDEF(void) 
my_error_exit (j_common_ptr cinfo)
{
  my_error_ptr myerr = (my_error_ptr)cinfo->err;
  (*cinfo->err->output_message) (cinfo);
  longjmp(myerr->setjmp_buffer,1);
}

METHODDEF(void)
my_output_message (j_common_ptr cinfo)
{
  char buffer[JMSG_LENGTH_MAX];

  (*cinfo->err->format_message) (cinfo, buffer); 
  printf(" %s ",buffer);
  global_error_counter++;
}


void no_memory(void)
{
  if (!quiet_mode) fprintf(stderr,"jpegoptim: not enough memory.\n");
  exit(3);
}

void p_usage(void) 
{
 if (!quiet_mode) {
  fprintf(stderr,"jpegoptim " VERSIO 
	  " Copyright (c) Timo Kokkonen, 1996.\n"); 

  fprintf(stderr,
       "Usage: jpegoptim [options] <filenames> \n\n"
#ifdef LONG_OPTIONS
       "  -d<path>, --dest=<path>\n"
       "                 specify alternative destination directory for \n"
       "                 optimized files (default is to overwrite originals)\n"
       "  -f, --force    force optimization\n"
       "  -h, --help     display this help and exit\n"
       "  -m[0..100], --max=[0..100] \n"
       "                 set maximum image quality factor (disables lossless\n"
       "                 optimization mode, which is by default on)\n"
       "  -n, --noaction don't really optimize files, just print results\n"
       "  -q, --quiet    quiet mode\n"
       "  -t, --totals   print totals after processing all files\n"
       "  -v, --verbose  enable verbose mode (positively chatty)\n"
#else
       "  -d<path>       specify alternative destination directory for \n"
       "                 optimized files (default is to overwrite originals)\n"
       "  -f             force optimization\n"
       "  -h             display this help and exit\n"
       "  -m[0..100]     set maximum image quality factor (disables lossless\n"
       "                 optimization mode, which is by default on)\n"
       "  -n             don't really optimize files, just print results\n"
       "  -q             quiet mode\n"
       "  -t             print totals after processing all files\n"
       "  -v             enable verbose mode (positively chatty)\n"
#endif
       "\n\n");
 }

 exit(1);
}

int delete_file(char *name)
{
  int retval;

  if (verbose_mode&&!quiet_mode) fprintf(stderr,"deleting: %s\n",name);
  if ((retval=unlink(name))&&!quiet_mode) {
    fprintf(stderr,"jpegoptim: error removing file: %s\n",name);
  }

  return retval;
}

int file_size(FILE *fp)
{
  long size=0,save=0;
  
  fgetpos(fp,&save);
  fseek(fp,0L,SEEK_END);
  fgetpos(fp,&size);
  fseek(fp,save,SEEK_SET);
  
  return size;
}

int is_directory(const char *path)
{
  DIR *dir = opendir(path);
  
  if (!dir) {
    return 0;
  }
  closedir(dir);
  return 1;
}


int is_dir(FILE *fp)
{
 struct stat buf;
 if (fstat(fileno(fp),&buf)) {
   fprintf(stderr,"jpeginfo: fstat() failed.\n");
   exit(3);
 }
 
 if (S_ISDIR(buf.st_mode)) return 1;

 return 0;
}


int file_exists(const char *pathname)
{
  FILE *file=fopen(pathname,"r");
  
  if (!file) return 0;
  fclose(file);
  return 1;
}

void own_signal_handler(int a)
{
  if (verbose_mode) printf("jpegoptim: signal: %d\n",a);
  if (outfile) fclose(outfile);
  if (outfname) if (file_exists(outfname)) delete_file(outfname);
  exit(1);
}

/*****************************************************************/
int main(int argc, char **argv) 
{
  JSAMPARRAY buf = NULL;
  char name1[MAXPATHLEN],name2[MAXPATHLEN];
  char newname[MAXPATHLEN], dest_path[MAXPATHLEN];
  jvirt_barray_ptr *coef_arrays;
  int c,i,j,lines_read, err_count;
  int totals_mode = 0;
  int opt_index = 0;
  int quality = -1;
  int noaction = 0;
  int retry = 0;
  int dest = 0;
  int force = 0;
  long insize,outsize,average_count=0;
  double ratio,average_rate = 0.0,total_save=0.0;
  pid_t cur_pid = getpid();
  uid_t cur_uid = getuid();

  sprintf(name1,TEMPDIR "/jpegoptim.%06x.%04x.tmp",cur_pid,cur_uid);
  sprintf(name2,TEMP2DIR "/jpegoptim.%06x.%04x.tmp",cur_pid,cur_uid);

  signal(SIGINT,own_signal_handler);
  signal(SIGTERM,own_signal_handler);

  /* initialize decompression object */
  dinfo.err = jpeg_std_error(&jderr.pub);
  jpeg_create_decompress(&dinfo);
  jderr.pub.error_exit=my_error_exit;
  jderr.pub.output_message=my_output_message;

  /* initialize compression object */
  cinfo.err = jpeg_std_error(&jcerr.pub);
  jpeg_create_compress(&cinfo);
  jcerr.pub.error_exit=my_error_exit;
  jcerr.pub.output_message=my_output_message;


  if (argc<2) {
    if (!quiet_mode) fprintf(stderr,"jpegoptim: file arguments missing\n"
			     "Try 'jpegoptim "
#ifdef LONG_OPTIONS
			     "--help"
#else
			     "-h"
#endif
			     "' for more information.\n");
    exit(1);
  }
 
  /* parse command line parameters */
  while(1) {
    opt_index=0;
#ifdef LONG_OPTIONS
    if ((c=getopt_long(argc,argv,"d:hm:ntqvf",long_options,&opt_index))==-1) 
#else
    if ((c=getopt(argc,argv,"d:hm:ntqvf"))==-1) 
#endif
      break;
    switch (c) {
    case 'm':
      {
        int tmpvar;

        if (sscanf(optarg,"%d",&tmpvar) == 1) {
	  quality=tmpvar;
	  if (quality < 0) quality=0;
	  if (quality > 100) quality=100;
	}
	else if (!quiet_mode) {
	  fprintf(stderr,"jpegoptim: invalid argument for -m, --max\n");
	  exit(1);
	}
      }
      break;
    case 'd':
      if (realpath(optarg,dest_path)==NULL || !is_directory(dest_path)) {
	fprintf(stderr,"jpegoptim: invalid argument for -d, --dest\n");
	exit(1);
      }
      if (verbose_mode) 
	fprintf(stderr,"Destination directory: %s\n",dest_path);
      dest=1;
      break;
    case 'v':
      verbose_mode=1;
      break;
    case 'h':
      p_usage();
      break;
    case 'q':
      quiet_mode=1;
      break;
    case 't':
      totals_mode=1;
      break;
    case 'n':
      noaction=1;
      break;
    case 'f':
      force=1;
      break;
    case '?':
      break;

    default:
      if (!quiet_mode) 
	fprintf(stderr,"jpegoptim: error parsing parameters.\n");
    }
  }


  if (verbose_mode && (quality>0)) 
    fprintf(stderr,"Image quality limit set to: %d\n",quality);


  /* loop to process the input files */
  i=1;  
  do {
   if (!argv[i][0]) continue;
   if (argv[i][0]=='-') continue;

  retry_point:
   if ((infile=fopen(argv[i],"r"))==NULL) {
     if (!quiet_mode) fprintf(stderr, "jpegoptim: can't open %s\n", argv[i]);
     continue;
   }
   if (is_dir(infile)) {
     fclose(infile);
     if (verbose_mode) printf("directory: %s  skipped\n",argv[i]); 
     continue;
   }

   /* setup error handling for decompress */
   if (setjmp(jderr.setjmp_buffer)) {
      jpeg_abort_decompress(&dinfo);
      fclose(infile);
      printf(" [ERROR]\n");
      continue;
   }

   if (!retry) { printf("%s ",argv[i]); fflush(stdout); }

   /* prepare to decompress */
   global_error_counter=0;
   err_count=jderr.pub.num_warnings;
   jpeg_stdio_src(&dinfo, infile);
   jpeg_read_header(&dinfo, TRUE); 

   if (!retry) {
     printf("%dx%d %dbit ",(int)dinfo.image_width,
	    (int)dinfo.image_height,(int)dinfo.num_components*8);
     if (dinfo.saw_Adobe_marker) printf("Adobe ");
     else if (dinfo.saw_JFIF_marker) printf("JFIF ");
     else printf("Unknown ");
     fflush(stdout);
   }

   insize=file_size(infile);

  /* decompress the file */
   if (quality>=0 && !retry) {
     jpeg_start_decompress(&dinfo);

     buf = malloc(sizeof(JSAMPROW)*dinfo.output_height);
     if (!buf) no_memory();
     for (j=0;j<dinfo.output_height;j++) {
       buf[j]=malloc(sizeof(JSAMPLE)*dinfo.output_width*
		     dinfo.out_color_components);
       if (!buf[j]) no_memory();
     }

     while (dinfo.output_scanline < dinfo.output_height) {
       jpeg_read_scanlines(&dinfo,&buf[dinfo.output_scanline],
			   dinfo.output_height-dinfo.output_scanline);
     }
   } else {
     coef_arrays = jpeg_read_coefficients(&dinfo);
   }

   if (!retry) {
     if (!global_error_counter) printf(" [OK] ");
     else printf(" [WARNING] ");
     fflush(stdout);
   }

   outfname=(noaction ? name2 : name1);

   if (dest && !noaction) {
     strcpy(newname,dest_path);
     strcat(newname,"/"); strcat(newname,(char*)basename(argv[i]));
     if (file_exists(newname)) {
       fprintf(stderr,"jpegoptim: target file already exists: %s\n",
	       newname);
       jpeg_abort_decompress(&dinfo);
       fclose(infile);
       for (j=0;j<dinfo.output_height;j++) free(buf[j]);
       free(buf); buf=NULL;
       outfname=NULL;
       continue;
     }
     outfname=newname;
   }

   if ((outfile=fopen(outfname,"w"))==NULL) {
     if (!quiet_mode) fprintf(stderr,"\njpegoptim: error creating "
			      "file: %s\n", outfname);
     exit(1);
   }


   if (setjmp(jcerr.setjmp_buffer)) {
      jpeg_abort_compress(&cinfo);
      jpeg_abort_decompress(&dinfo);
      fclose(outfile);
      if (infile) fclose(infile);
      printf(" [Compress ERROR]\n");
      for (j=0;j<dinfo.output_height;j++) free(buf[j]);
      free(buf); buf=NULL;
      if (file_exists(outfname)) delete_file(outfname);
      outfname=NULL;
      continue;
   }


   jpeg_stdio_dest(&cinfo, outfile);

   if (quality>=0 && !retry) {
     cinfo.in_color_space=dinfo.out_color_space;
     cinfo.input_components=dinfo.output_components;
     cinfo.image_width=dinfo.image_width;
     cinfo.image_height=dinfo.image_height;
     jpeg_set_defaults(&cinfo); 
     jpeg_set_quality(&cinfo,quality,TRUE);
     cinfo.optimize_coding = TRUE;
     
     j=0;
     jpeg_start_compress(&cinfo,TRUE);
     while (cinfo.next_scanline < cinfo.image_height) {
       jpeg_write_scanlines(&cinfo,&buf[cinfo.next_scanline],
			    dinfo.output_height);
     }
     
     for (j=0;j<dinfo.output_height;j++) free(buf[j]);
     free(buf); buf=NULL;
   } else {
     jpeg_copy_critical_parameters(&dinfo, &cinfo);
     cinfo.optimize_coding = TRUE;
     jpeg_write_coefficients(&cinfo, coef_arrays);
   }



   jpeg_finish_compress(&cinfo);
   jpeg_finish_decompress(&dinfo);
   fclose(infile);
   outsize=file_size(outfile);
   fclose(outfile);

   if (quality>=0 && outsize>=insize && !retry) {
     if (verbose_mode) printf("(retry w/lossless) ");
     retry=1;
     goto retry_point; 
   }

   retry=0;
   ratio=(insize-outsize)*100.0/insize;
   printf("%d --> %d bytes (%0.2lf%%), ",insize,outsize,ratio);
   average_count++;
   average_rate+=(ratio<0 ? 0.0 : ratio);

   if (outsize<insize || force) {
        total_save+=(insize-outsize)/1024.0;
	printf("optimized.\n");
        if (noaction || dest) { continue; }
	if (!delete_file(argv[i])) {
		if (verbose_mode&&!quiet_mode) 
		  fprintf(stderr,"Renaming: %s to %s\n",outfname,argv[i]);
		if (rename(outfname,argv[i])) {
		  fprintf(stderr,"Cannot rename temp file.\n");
	          exit(3);
		}
	}
	
   } else {
	printf("skipped.\n");
	if (!noaction) delete_file(outfname);
   }
   

  } while (++i<argc);

  if (noaction && file_exists(outfname)) delete_file(outfname);
  if (totals_mode&&!quiet_mode)
    printf("Average ""compression"" (%ld files): %0.2lf%% (%0.0lfk)\n",
	   average_count, average_rate/average_count, total_save);
  jpeg_destroy_decompress(&dinfo);
  jpeg_destroy_compress(&cinfo);
  return 0;
}

/* :-) */

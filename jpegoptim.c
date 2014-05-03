/*******************************************************************
 * JPEGoptim
 * Copyright (c) Timo Kokkonen, 1996-2014.
 * All Rights Reserved.
 *
 * requires libjpeg.a (from JPEG Group's JPEG software 
 *                     release 6a or later...)
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <dirent.h>
#if HAVE_GETOPT_H && HAVE_GETOPT_LONG
#include <getopt.h>
#else
#include "getopt.h"
#endif
#include <signal.h>
#include <string.h>
#include <jpeglib.h>
#include <setjmp.h>
#include <time.h>
#include <math.h>
#ifdef HAVE_LIBGEN_H
#include <libgen.h>
#endif
#include "jpegoptim.h"


#define VERSIO "1.4.0beta"


struct my_error_mgr {
  struct jpeg_error_mgr pub;
  jmp_buf setjmp_buffer;   
};

typedef struct my_error_mgr * my_error_ptr;

struct jpeg_decompress_struct dinfo;
struct jpeg_compress_struct cinfo;
struct my_error_mgr jcerr,jderr;

const char *rcsid = "$Id$";


int verbose_mode = 0;
int quiet_mode = 0;
int global_error_counter = 0;
int preserve_mode = 0;
int overwrite_mode = 0;
int totals_mode = 0;
int stdin_mode = 0;
int stdout_mode = 0;
int noaction = 0;
int quality = -1;
int retry = 0;
int dest = 0;
int force = 0;
int save_exif = 1;
int save_iptc = 1;
int save_com = 1;
int save_icc = 1;
int save_xmp = 1;
int threshold = -1;
int csv = 0;
int all_normal = 0;
int all_progressive = 0;
int target_size = 0;
int logs_to_stdout = 1;


#define LOG_FH (logs_to_stdout ? stdout : stderr)


struct option long_options[] = {
  {"verbose",0,0,'v'},
  {"help",0,0,'h'},
  {"quiet",0,0,'q'},
  {"max",1,0,'m'},
  {"totals",0,0,'t'},
  {"noaction",0,0,'n'},
  {"dest",1,0,'d'},
  {"force",0,0,'f'},
  {"version",0,0,'V'},
  {"preserve",0,0,'p'},
  {"strip-all",0,0,'s'},
  {"strip-com",0,&save_com,0},
  {"strip-exif",0,&save_exif,0},
  {"strip-iptc",0,&save_iptc,0},
  {"strip-icc",0,&save_icc,0},
  {"strip-xmp",0,&save_xmp,0},
  {"threshold",1,0,'T'},
  {"csv",0,0,'b'},
  {"all-normal",0,&all_normal,1},
  {"all-progressive",0,&all_progressive,1},
  {"size",1,0,'S'},
  {"stdout",0,&stdout_mode,1},
  {"stdin",0,&stdin_mode,1},
  {0,0,0,0}
};

JSAMPARRAY buf = NULL;
jvirt_barray_ptr *coef_arrays = NULL;
jpeg_saved_marker_ptr exif_marker = NULL;
jpeg_saved_marker_ptr iptc_marker = NULL;
jpeg_saved_marker_ptr icc_marker = NULL;
jpeg_saved_marker_ptr xmp_marker = NULL;
long average_count = 0;
double average_rate = 0.0, total_save = 0.0;

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
  if (verbose_mode) fprintf(LOG_FH," (%s) ",buffer);
  global_error_counter++;
}


void p_usage(void) 
{
 if (!quiet_mode) {
  fprintf(stderr,PROGRAMNAME " v" VERSIO 
	  "  Copyright (c) Timo Kokkonen, 1996-2014.\n"); 

  fprintf(stderr,
       "Usage: " PROGRAMNAME " [options] <filenames> \n\n"
    "  -d<path>, --dest=<path>\n"
    "                    specify alternative destination directory for \n"
    "                    optimized files (default is to overwrite originals)\n"
    "  -f, --force       force optimization\n"
    "  -h, --help        display this help and exit\n"
    "  -m<quality>, --max=<quality>\n"
    "                    set maximum image quality factor (disables lossless\n"
    "                    optimization mode, which is by default on)\n"
    "                    Valid quality values: 0 - 100\n"
    "  -n, --noaction    don't really optimize files, just print results\n"
    "  -S<size>, --size=<size>\n"
    "                    Try to optimize file to given size (disables lossless\n"
    "                    optimization mode). Target size is specified either in\n"
    "                    kilo bytes (1 - n) or as percentage (1%% - 99%%)\n"
    "  -T<threshold>, --threshold=<threshold>\n"
    "                    keep old file if the gain is below a threshold (%%)\n"
    "  -b, --csv         print progress info in CSV format\n"
    "  -o, --overwrite   overwrite target file even if it exists\n"
    "  -p, --preserve    preserve file timestamps\n"
    "  -q, --quiet       quiet mode\n"
    "  -t, --totals      print totals after processing all files\n"
    "  -v, --verbose     enable verbose mode (positively chatty)\n"
    "  -V, --version     print program version\n\n"
    "  -s, --strip-all   strip all (Comment & Exif) markers from output file\n"
    "  --strip-com       strip Comment markers from output file\n"
    "  --strip-exif      strip Exif markers from output file\n"
    "  --strip-iptc      strip IPTC markers from output file\n"
    "  --strip-icc       strip ICC profile markers from output file\n"
    "  --strip-xmp       strip XMP markers markers from output file\n"
    "\n"
    "  --all-normal      force all output files to be non-progressive\n"
    "  --all-progressive force all output files to be progressive\n"
    "  --stdout          send output to standard output (instead of a file)\n"
    "  --stdin           read input from standard input (instead of a file)\n"
    "\n\n");
 }

 exit(1);
}


void own_signal_handler(int a)
{
  if (verbose_mode > 1) 
    fprintf(stderr,PROGRAMNAME ": signal: %d\n",a);
  exit(1);
}






void write_markers(struct jpeg_decompress_struct *dinfo,
		   struct jpeg_compress_struct *cinfo)
{
  jpeg_saved_marker_ptr mrk;

  if (!cinfo || !dinfo) return;

  mrk=dinfo->marker_list;
  while (mrk) {
    if (save_com && mrk->marker == JPEG_COM) 
      jpeg_write_marker(cinfo,JPEG_COM,mrk->data,mrk->data_length);

    if (save_iptc && mrk->marker == IPTC_JPEG_MARKER) 
      jpeg_write_marker(cinfo,IPTC_JPEG_MARKER,mrk->data,mrk->data_length);

    if (save_exif && mrk->marker == EXIF_JPEG_MARKER) {
      if (!memcmp(mrk->data,EXIF_IDENT_STRING,EXIF_IDENT_STRING_SIZE)) {
	jpeg_write_marker(cinfo,EXIF_JPEG_MARKER,mrk->data,mrk->data_length);
      }
    }

    if (save_icc && mrk->marker == ICC_JPEG_MARKER) {
      if (!memcmp(mrk->data,ICC_IDENT_STRING,ICC_IDENT_STRING_SIZE)) {
	jpeg_write_marker(cinfo,ICC_JPEG_MARKER,mrk->data,mrk->data_length);
      }
    }

    if (save_xmp && mrk->marker == XMP_JPEG_MARKER) {
      if (!memcmp(mrk->data,XMP_IDENT_STRING,XMP_IDENT_STRING_SIZE)) {
	jpeg_write_marker(cinfo,XMP_JPEG_MARKER,mrk->data,mrk->data_length);
      }
    }

    mrk=mrk->next;
  }
}



/*****************************************************************/
int main(int argc, char **argv) 
{
  char tmpfilename[MAXPATHLEN],tmpdir[MAXPATHLEN];
  char newname[MAXPATHLEN], dest_path[MAXPATHLEN];
  volatile int i;
  int c,j, tmpfd, searchcount, searchdone;
  int opt_index = 0;
  long insize,outsize,lastsize;
  int oldquality;
  double ratio;
  struct stat file_stat;
  jpeg_saved_marker_ptr cmarker; 
  unsigned char *outbuffer = NULL;
  size_t outbuffersize;
  char *outfname = NULL;
  FILE *infile = NULL, *outfile = NULL;
  int compress_err_count = 0;
  int decompress_err_count = 0;


  if (rcsid)
  ; /* so compiler won't complain about "unused" rcsid string */

  umask(077);
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
    if (!quiet_mode) fprintf(stderr,PROGRAMNAME ": file arguments missing\n"
			     "Try '" PROGRAMNAME " --help' for more information.\n");
    exit(1);
  }
 
  /* parse command line parameters */
  while(1) {
    opt_index=0;
    if ((c=getopt_long(argc,argv,"d:hm:nstqvfVpoT:S:b",long_options,&opt_index))
	      == -1) 
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
	else 
	  fatal("invalid argument for -m, --max");
      }
      break;
    case 'd':
      if (realpath(optarg,dest_path)==NULL || !is_directory(dest_path)) {
	fatal("invalid argument for option -d, --dest");
      }
      if (verbose_mode) 
	fprintf(stderr,"Destination directory: %s\n",dest_path);
      dest=1;
      break;
    case 'v':
      verbose_mode++;
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
    case 'b':
      csv=1;
      quiet_mode=1;
      break;
    case '?':
      break;
    case 'V':
      printf(PROGRAMNAME " v%s  %s\n",VERSIO,HOST_TYPE);
      printf("Copyright (c) Timo Kokkonen, 1996-2014.\n");
      exit(0);
      break;
    case 'o':
      overwrite_mode=1;
      break;
    case 'p':
      preserve_mode=1;
      break;
    case 's':
      save_exif=0;
      save_iptc=0;
      save_com=0;
      save_icc=0;
      save_xmp=0;
      break;
    case 'T':
      {
	int tmpvar;
	if (sscanf(optarg,"%d",&tmpvar) == 1) {
	  threshold=tmpvar;
	  if (threshold < 0) threshold=0;
	  if (threshold > 100) threshold=100;
	}
	else fatal("invalid argument for -T, --threshold");
      }
      break;
    case 'S':
      {
	unsigned int tmpvar;
	if (sscanf(optarg,"%u",&tmpvar) == 1) {
	  if (tmpvar > 0 && tmpvar < 100 && optarg[strlen(optarg)-1] == '%' ) {
	    target_size=-tmpvar;
	  } else {
	    target_size=tmpvar;
	  }
	  quality=100;
	}
	else fatal("invalid argument for -S, --size");
      }
      break;

    }
  }

  if (stdin_mode) stdout_mode=1;
  if (stdout_mode) { logs_to_stdout=0; }


  if (all_normal && all_progressive)
    fatal("cannot specify both --all-normal and --all-progressive"); 

  if (verbose_mode) {
    if (quality>=0 && target_size==0) 
      fprintf(stderr,"Image quality limit set to: %d\n",quality);
    if (threshold>=0) 
      fprintf(stderr,"Compression threshold (%%) set to: %d\n",threshold);
    if (all_normal) 
      fprintf(stderr,"All output files will be non-progressive\n");
    if (all_progressive) 
      fprintf(stderr,"All output files will be progressive\n");
    if (target_size > 0) 
      fprintf(stderr,"Target size for output files set to: %u Kbytes.\n",
	      target_size);
    if (target_size < 0) 
      fprintf(stderr,"Target size for output files set to: %u%%\n",
	      -target_size);
  }


  /* loop to process the input files */
  i=1;  
  do {
    if (stdin_mode) {
      infile=stdin;
    } else {
      if (!argv[i][0]) continue;
      if (argv[i][0]=='-') continue;
      
      if (!noaction) {
	/* generate temp (& new) filename */
	if (dest) {
	  strncpy(tmpdir,dest_path,sizeof(tmpdir));
	  strncpy(newname,dest_path,sizeof(newname));
	  if (tmpdir[strlen(tmpdir)-1] != DIR_SEPARATOR_C) {
	    strncat(tmpdir,DIR_SEPARATOR_S,sizeof(tmpdir)-strlen(tmpdir)-1);
	    strncat(newname,DIR_SEPARATOR_S,sizeof(newname)-strlen(newname)-1);
	  }
	  strncat(newname,(char*)basename(argv[i]),
		  sizeof(newname)-strlen(newname)-1);
	} else {
	  if (!splitdir(argv[i],tmpdir,sizeof(tmpdir))) 
	    fatal("splitdir() failed!");
	  strncpy(newname,argv[i],sizeof(newname));
	}
      }
      
    retry_point:
      
      if (!is_file(argv[i],&file_stat)) {
	if (!quiet_mode) {
	  if (S_ISDIR(file_stat.st_mode)) 
	    warn("skipping directory: %s",argv[i]);
	  else
	    warn("skipping special file: %s",argv[i]); 
	}
	continue;
      }
      if ((infile=fopen(argv[i],"r"))==NULL) {
	if (!quiet_mode) warn("cannot open file: %s", argv[i]);
	continue;
      }
    }

   if (setjmp(jderr.setjmp_buffer)) {
     /* error handler for decompress */
     jpeg_abort_decompress(&dinfo);
     fclose(infile);
     if (buf) {
       for (j=0;j<dinfo.output_height;j++) free(buf[j]);
       free(buf); buf=NULL;
     }
     if (!quiet_mode || csv) 
       fprintf(LOG_FH,csv ? ",,,,,error\n" : " [ERROR]\n");
     decompress_err_count++;
     continue;
   }

   if (!retry && (!quiet_mode || csv)) {
     fprintf(LOG_FH,csv ? "%s," : "%s ",(stdin_mode?"stdin":argv[i])); fflush(LOG_FH); 
   }

   /* prepare to decompress */
   global_error_counter=0;
   if (save_com) jpeg_save_markers(&dinfo, JPEG_COM, 0xffff);
   if (save_iptc) jpeg_save_markers(&dinfo, IPTC_JPEG_MARKER, 0xffff);
   if (save_exif) jpeg_save_markers(&dinfo, EXIF_JPEG_MARKER, 0xffff);
   if (save_icc) jpeg_save_markers(&dinfo, ICC_JPEG_MARKER, 0xffff);
   if (save_xmp) jpeg_save_markers(&dinfo, XMP_JPEG_MARKER, 0xffff);

   jpeg_stdio_src(&dinfo, infile);
   jpeg_read_header(&dinfo, TRUE); 

   /* check for Exif/IPTC markers */
   exif_marker=NULL;
   iptc_marker=NULL;
   icc_marker=NULL;
   xmp_marker=NULL;
   cmarker=dinfo.marker_list;
   while (cmarker) {
     if (cmarker->marker == EXIF_JPEG_MARKER) {
       if (!memcmp(cmarker->data,EXIF_IDENT_STRING,EXIF_IDENT_STRING_SIZE)) 
	 exif_marker=cmarker;
     }
     if (cmarker->marker == IPTC_JPEG_MARKER) {
       iptc_marker=cmarker;
     }
     if (cmarker->marker == ICC_JPEG_MARKER) {
       if (!memcmp(cmarker->data,ICC_IDENT_STRING,ICC_IDENT_STRING_SIZE)) 
	 icc_marker=cmarker;
     }
     if (cmarker->marker == XMP_JPEG_MARKER) {
       if (!memcmp(cmarker->data,XMP_IDENT_STRING,XMP_IDENT_STRING_SIZE)) 
	 xmp_marker=cmarker;
     }
     cmarker=cmarker->next;
   }


   if (!retry && (!quiet_mode || csv)) {
     fprintf(LOG_FH,csv ? "%dx%d,%dbit,%c," : "%dx%d %dbit %c ",(int)dinfo.image_width,
	     (int)dinfo.image_height,(int)dinfo.num_components*8,
	     (dinfo.progressive_mode?'P':'N'));

     if (!csv) {
       if (exif_marker) fprintf(LOG_FH,"Exif ");
       if (iptc_marker) fprintf(LOG_FH,"IPTC ");
       if (icc_marker) fprintf(LOG_FH,"ICC ");
       if (xmp_marker) fprintf(LOG_FH,"XMP ");
       if (dinfo.saw_Adobe_marker) fprintf(LOG_FH,"Adobe ");
       if (dinfo.saw_JFIF_marker) fprintf(LOG_FH,"JFIF ");
     }
     fflush(LOG_FH);
   }

   insize=file_size(infile);

  /* decompress the file */
   if (quality>=0 && !retry) {
     jpeg_start_decompress(&dinfo);

     buf = malloc(sizeof(JSAMPROW)*dinfo.output_height);
     if (!buf) fatal("not enough memory");
     for (j=0;j<dinfo.output_height;j++) {
       buf[j]=malloc(sizeof(JSAMPLE)*dinfo.output_width*
		     dinfo.out_color_components);
       if (!buf[j]) fatal("not enough memory");
     }

     while (dinfo.output_scanline < dinfo.output_height) {
       jpeg_read_scanlines(&dinfo,&buf[dinfo.output_scanline],
			   dinfo.output_height-dinfo.output_scanline);
     }
   } else {
     coef_arrays = jpeg_read_coefficients(&dinfo);
   }

   if (!retry && !quiet_mode) {
     if (global_error_counter==0) fprintf(LOG_FH," [OK] ");
     else fprintf(LOG_FH," [WARNING] ");
     fflush(LOG_FH);
   }

   fclose(infile);
   infile=NULL;
     

   if (dest && !noaction) {
     if (file_exists(newname) && !overwrite_mode) {
       fprintf(stderr,"target file already exists: %s\n",newname);
       jpeg_abort_decompress(&dinfo);
       if (buf) {
	 for (j=0;j<dinfo.output_height;j++) free(buf[j]);
	 free(buf); buf=NULL;
       }
       continue;
     }
   }


   if (setjmp(jcerr.setjmp_buffer)) {
     /* error handler for compress failures */
     
     jpeg_abort_compress(&cinfo);
     jpeg_abort_decompress(&dinfo);
     if (!quiet_mode) fprintf(LOG_FH," [Compress ERROR]\n");
     if (buf) {
       for (j=0;j<dinfo.output_height;j++) free(buf[j]);
       free(buf); buf=NULL;
     }
     compress_err_count++;
     continue;
   }


   lastsize = 0;
   searchcount = 0;
   searchdone = 0;
   oldquality = 200;



  binary_search_loop:

   if (outbuffer) free(outbuffer);
   outbuffersize=insize + 32768;
   outbuffer=malloc(outbuffersize);
   if (!outbuffer) fatal("not enough memory");
   jpeg_memory_dest(&cinfo, &outbuffer, &outbuffersize, 65536);

   if (quality>=0 && !retry) {
     /* lossy "optimization" ... */

     cinfo.in_color_space=dinfo.out_color_space;
     cinfo.input_components=dinfo.output_components;
     cinfo.image_width=dinfo.image_width;
     cinfo.image_height=dinfo.image_height;
     jpeg_set_defaults(&cinfo); 
     jpeg_set_quality(&cinfo,quality,TRUE);
     if ( (dinfo.progressive_mode || all_progressive) && !all_normal )
       jpeg_simple_progression(&cinfo);
     cinfo.optimize_coding = TRUE;

     j=0;
     jpeg_start_compress(&cinfo,TRUE);
     
     /* write markers */
     write_markers(&dinfo,&cinfo);

     /* write image */
     while (cinfo.next_scanline < cinfo.image_height) {
       jpeg_write_scanlines(&cinfo,&buf[cinfo.next_scanline],
			    dinfo.output_height);
     }

   } else {
     /* lossless "optimization" ... */

     jpeg_copy_critical_parameters(&dinfo, &cinfo);
     if ( (dinfo.progressive_mode || all_progressive) && !all_normal )
       jpeg_simple_progression(&cinfo);
     cinfo.optimize_coding = TRUE;

     /* write image */
     jpeg_write_coefficients(&cinfo, coef_arrays);

     /* write markers */
     write_markers(&dinfo,&cinfo);

   }

   jpeg_finish_compress(&cinfo);
   outsize=outbuffersize;

   if (target_size != 0 && !retry) {
     /* perform (binary) search to try to reach target file size... */

     long osize = outsize/1024;
     long isize = insize/1024;
     long tsize = target_size;

     if (tsize < 0) { 
       tsize=((-target_size)*insize/100)/1024; 
       if (tsize < 1) tsize=1;
     }

     if (osize == tsize || searchdone || searchcount >= 8 || tsize > isize) {
       if (searchdone < 42 && lastsize > 0) {
	 if (abs(osize-tsize) > abs(lastsize-tsize)) {
	   if (verbose_mode) fprintf(LOG_FH,"(revert to %d)",oldquality);
	   searchdone=42;
	   quality=oldquality;
	   goto binary_search_loop;
	 }
       }
       if (verbose_mode) fprintf(LOG_FH," ");
       
     } else {
       int newquality;
       int dif = round(abs(oldquality-quality)/2.0);
       if (osize > tsize) {
	 newquality=quality-dif;
	 if (dif < 1) { newquality--; searchdone=1; }
	 if (newquality < 0) { newquality=0; searchdone=2; }
       } else {
	 newquality=quality+dif;
	 if (dif < 1) { newquality++; searchdone=3; }
	 if (newquality > 100) { newquality=100; searchdone=4; }
       }
       oldquality=quality;
       quality=newquality;

       if (verbose_mode) fprintf(LOG_FH,"(try %d)",quality);

       lastsize=osize;
       searchcount++;
       goto binary_search_loop;
     }
   } 

   if (buf) {
     for (j=0;j<dinfo.output_height;j++) free(buf[j]);
     free(buf); buf=NULL;
   }
   jpeg_finish_decompress(&dinfo);


   if (quality>=0 && outsize>=insize && !retry && !stdin_mode) {
     if (verbose_mode) fprintf(LOG_FH,"(retry w/lossless) ");
     retry=1;
     goto retry_point; 
   }

   retry=0;
   ratio=(insize-outsize)*100.0/insize;
   if (!quiet_mode || csv)
     fprintf(LOG_FH,csv ? "%ld,%ld,%0.2f," : "%ld --> %ld bytes (%0.2f%%), ",insize,outsize,ratio);
   average_count++;
   average_rate+=(ratio<0 ? 0.0 : ratio);

   if ((outsize < insize && ratio >= threshold) || force) {
        total_save+=(insize-outsize)/1024.0;
	if (!quiet_mode || csv) fprintf(LOG_FH,csv ? "optimized\n" : "optimized.\n");
        if (noaction) continue;



	if (stdout_mode) {
	  outfile=stdout;
	  outfname=NULL;
	} else {
#ifdef HAVE_MKSTEMPS
          /* rely on mkstemps() to create us temporary file safely... */  
	  snprintf(tmpfilename,sizeof(tmpfilename),
		   "%sjpegoptim-%d-%d.XXXXXX.tmp", tmpdir, (int)getuid(), (int)getpid());
	  if ((tmpfd = mkstemps(tmpfilename,4)) < 0) 
	    fatal("error creating temp file: mkstemps() failed");
	  if ((outfile=fdopen(tmpfd,"w"))==NULL) 
#else
	  /* if platform is missing mkstemps(), try to create at least somewhat "safe" temp file... */  
	  snprintf(tmpfilename,sizeof(tmpfilename),
		   "%sjpegoptim-%d-%d.%d.tmp", tmpdir, (int)getuid(), (int)getpid(),time(NULL));
	  tmpfd=0;
	  if ((outfile=fopen(tmpfilename,"w"))==NULL) 
#endif
	    fatal("error opening temporary file: %s",tmpfilename);
	  outfname=tmpfilename;
	}

	if (verbose_mode > 1 && !quiet_mode) 
	  fprintf(LOG_FH,"writing %ld bytes to temporary file: %s\n",outbuffersize,outfname);
	if (fwrite(outbuffer,outbuffersize,1,outfile) != 1)
	  fatal("write failed to temporary file");
	fclose(outfile);
	outfile=NULL;

	if (outfname) {
	  /* preserve file mode */
	  if (chmod(outfname,(file_stat.st_mode & 0777)) != 0) {
	    if (!quiet_mode) warn("failed to set output file mode"); 
	  }
	  if (geteuid() == 0) {
	    /* preserve file ownership */
	    if (chown(outfname,file_stat.st_uid,file_stat.st_gid) != 0) {
	      if (!quiet_mode) warn("failed to reset output file owner");
	    }
	  }
	  if (preserve_mode) {
	    /* preserve file modification time */
	    struct utimbuf time_save;
	    time_save.actime=file_stat.st_atime;
	    time_save.modtime=file_stat.st_mtime;
	    if (utime(outfname,&time_save) != 0) {
	      if (!quiet_mode) warn("failed to reset output file time/date");
	    }
	  }

	  if (verbose_mode > 1 && !quiet_mode) 
	    fprintf(LOG_FH,"renaming: %s to %s\n",outfname,newname);
	  if (rename_file(outfname,newname)) fatal("cannot rename temp file");
	}
   } else {
     if (!quiet_mode || csv) fprintf(LOG_FH,csv ? "skipped\n" : "skipped.\n");
   }
   

  } while (++i<argc && !stdin_mode);


  if (totals_mode && !quiet_mode)
    fprintf(LOG_FH,"Average ""compression"" (%ld files): %0.2f%% (%0.0fk)\n",
	    average_count, average_rate/average_count, total_save);
  jpeg_destroy_decompress(&dinfo);
  jpeg_destroy_compress(&cinfo);

  return (decompress_err_count > 0 || compress_err_count > 0 ? 1 : 0);;
}

/* :-) */

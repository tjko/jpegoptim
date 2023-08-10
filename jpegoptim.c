/*******************************************************************
 * JPEGoptim
 * Copyright (c) Timo Kokkonen, 1996-2023.
 * All Rights Reserved.
 *
 * requires libjpeg (Independent JPEG Group's JPEG software
 *                     release 6a or later...)
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
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_DIRENT_H
#include <dirent.h>
#endif
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#if HAVE_GETOPT_H && HAVE_GETOPT_LONG
#include <getopt.h>
#else
#include "getopt.h"
#endif
#include <signal.h>
#include <string.h>
#include <jpeglib.h>
#include <jerror.h>
#include <setjmp.h>
#include <time.h>
#include <math.h>

#include "jpegmarker.h"
#include "jpegoptim.h"


#define VERSION "1.5.5"
#define COPYRIGHT  "Copyright (C) 1996-2023, Timo Kokkonen"

#if HAVE_WAIT && HAVE_FORK
#define PARALLEL_PROCESSING 1
#define MAX_WORKERS 256
#endif


#define FREE_LINE_BUF(buf,lines)  {			\
		int j;					\
		for (j=0;j<lines;j++) free(buf[j]);	\
		free(buf);				\
		buf=NULL;				\
	}

struct my_error_mgr {
	struct jpeg_error_mgr pub;
	jmp_buf setjmp_buffer;
	int     jump_set;
};
typedef struct my_error_mgr * my_error_ptr;


#ifdef PARALLEL_PROCESSING
struct worker {
	pid_t pid;
	int   read_pipe;
};
struct worker *workers;
int worker_count = 0;
#endif


int verbose_mode = 0;
int quiet_mode = 0;
int preserve_mode = 0;
int preserve_perms = 0;
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
int save_adobe = 0;
int save_jfxx = 0;
int save_jfif = 1;
int strip_none = 0;
double threshold = -1.0;
int csv = 0;
int all_normal = 0;
int all_progressive = 0;
int target_size = 0;
#ifdef HAVE_ARITH_CODE
int arith_mode = -1;
#endif
int max_workers = 1;
int nofix_mode = 0;
int files_stdin = 0;
FILE *files_from = NULL;

int compress_err_count = 0;
int decompress_err_count = 0;
int global_error_counter = 0;
char last_error[JMSG_LENGTH_MAX+1];
FILE *jpeg_log_fh;
long average_count = 0;
double average_rate = 0.0;
double total_save = 0.0;

struct option long_options[] = {
#ifdef HAVE_ARITH_CODE
	{ "all-arith",          0, &arith_mode,          1 },
	{ "all-huffman",        0, &arith_mode,          0 },
#endif
	{ "all-normal",         0, &all_normal,          1 },
	{ "all-progressive",    0, &all_progressive,     1 },
	{ "csv",                0, 0,                    'b' },
	{ "dest",               1, 0,                    'd' },
	{ "files-stdin",        0, &files_stdin,         1 },
	{ "files-from",         1, 0,                    'F' },
	{ "force",              0, 0,                    'f' },
	{ "help",               0, 0,                    'h' },
	{ "keep-adobe",         0, &save_adobe,          1 },
	{ "keep-all",           0, &strip_none,          1 },
	{ "keep-com",           0, &save_com,            1 },
	{ "keep-exif",          0, &save_exif,           1 },
	{ "keep-iptc",          0, &save_iptc,           1 },
	{ "keep-icc",           0, &save_icc,            1 },
	{ "keep-jfif",          0, &save_jfif,           1 },
	{ "keep-jfxx",          0, &save_jfxx,           1 },
	{ "keep-xmp",           0, &save_xmp,            1 },
	{ "max",                1, 0,                    'm' },
	{ "noaction",           0, 0,                    'n' },
	{ "nofix",              0, &nofix_mode,          1 },
	{ "overwrite",          0, 0,                    'o' },
	{ "preserve",           0, 0,                    'p' },
	{ "preserve-perms",     0, 0,                    'P' },
	{ "quiet",              0, 0,                    'q' },
	{ "size",               1, 0,                    'S' },
	{ "stdin",              0, &stdin_mode,          1 },
	{ "stdout",             0, &stdout_mode,         1 },
	{ "strip-all",          0, 0,                    's' },
	{ "strip-none",         0, &strip_none,          1 },
	{ "strip-com",          0, &save_com,            0 },
	{ "strip-exif",         0, &save_exif,           0 },
	{ "strip-iptc",         0, &save_iptc,           0 },
	{ "strip-icc",          0, &save_icc,            0 },
	{ "strip-xmp",          0, &save_xmp,            0 },
	{ "strip-jfif",         0, &save_jfif,           0 },
	{ "strip-jfxx",         0, &save_jfxx,           0 },
	{ "strip-adobe",        0, &save_adobe,          0 },
	{ "threshold",          1, 0,                    'T' },
	{ "totals",             0, 0,                    't' },
	{ "verbose",            0, 0,                    'v' },
	{ "version",            0, 0,                    'V' },
#ifdef PARALLEL_PROCESSING
	{ "workers",            1, &max_workers,         'w' },
#endif
	{ 0, 0, 0, 0 }
};


/*****************************************************************/

METHODDEF(void)	my_error_exit (j_common_ptr cinfo)
{
	my_error_ptr myerr = (my_error_ptr)cinfo->err;

	(*cinfo->err->output_message)(cinfo);
	if (myerr->jump_set)
		longjmp(myerr->setjmp_buffer, 1);
	else
		fatal("fatal error");
}


METHODDEF(void) my_output_message (j_common_ptr cinfo)
{
	char buffer[JMSG_LENGTH_MAX+1];

	(*cinfo->err->format_message)((j_common_ptr)cinfo, buffer);
	buffer[sizeof(buffer) - 1] = 0;

	if (verbose_mode)
		fprintf(jpeg_log_fh, " (%s) ", buffer);

	global_error_counter++;
	strncopy(last_error, buffer, sizeof(last_error));
}


void print_usage(void)
{
	fprintf(stderr,PROGRAMNAME " v" VERSION "  " COPYRIGHT "\n");

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
#ifdef PARALLEL_PROCESSING
		"  -w<max>, --workers=<max>\n"
		"                    set maximum number of parallel threads (default is 1)\n"
#endif
		"  -b, --csv         print progress info in CSV format\n"
		"  -o, --overwrite   overwrite target file even if it exists (meaningful\n"
		"                    only when used with -d, --dest option)\n"
		"  -p, --preserve    preserve file timestamps\n"
		"  -P, --preserve-perms\n"
		"                    preserve original file permissions by overwriting it\n"
		"  -q, --quiet       quiet mode\n"
		"  -t, --totals      print totals after processing all files\n"
		"  -v, --verbose     enable verbose mode (positively chatty)\n"
		"  -V, --version     print program version\n\n"
		"  -s, --strip-all   strip all markers from output file\n"
		"  --strip-none      do not strip any markers\n"
		"  --strip-adobe     strip Adobe (APP14) markers from output file\n"
		"  --strip-com       strip Comment markers from output file\n"
		"  --strip-exif      strip Exif markers from output file\n"
		"  --strip-iptc      strip IPTC/Photoshop (APP13) markers from output file\n"
		"  --strip-icc       strip ICC profile markers from output file\n"
		"  --strip-jfif      strip JFIF markers from output file\n"
		"  --strip-jfxx      strip JFXX (JFIF Extension) markers from output file\n"
		"  --strip-xmp       strip XMP markers markers from output file\n"
		"\n"
		"  --keep-all        do not strip any markers (same as --strip-none)\n"
		"  --keep-adobe      preserve any Adobe (APP14) markers\n"
		"  --keep-com        preserve any Comment markers\n"
		"  --keep-exif       preserve any Exif markers\n"
		"  --keep-iptc       preserve any IPTC/Photoshop (APP13) markers\n"
		"  --keep-icc        preserve any ICC profile markers\n"
		"  --keep-jfif       preserve any JFIF markers\n"
		"  --keep-jfxx       preserve any JFXX (JFIF Extension) markers\n"
		"  --keep-xmp        preserve any XMP markers markers\n"
		"\n"
		"  --all-normal      force all output files to be non-progressive\n"
		"  --all-progressive force all output files to be progressive\n"
#ifdef HAVE_ARITH_CODE
		"  --all-arith       force all output files to use arithmetic coding\n"
		"  --all-huffman     force all output files to use Huffman coding\n"
#endif
		"  --stdout          send output to standard output (instead of a file)\n"
		"  --stdin           read input from standard input (instead of a file)\n"
		"  --files-stdin     Read names of files to process from stdin\n"
		"  --files-from=FILE Read names of files to process from a file\n"
		"  --nofix           skip processing of input files if they contain any errors\n"
		"\n\n");
}


void print_version()
{
	struct jpeg_error_mgr jerr;

#ifdef  __DATE__
	printf(PROGRAMNAME " v%s  %s (%s)\n",VERSION,HOST_TYPE,__DATE__);
#else
	printf(PROGRAMNAME " v%s  %s\n",VERSION,HOST_TYPE);
#endif
	printf(COPYRIGHT "\n\n");
	printf("This program comes with ABSOLUTELY NO WARRANTY. This is free software,\n"
		"and you are welcome to redistribute it under certain conditions.\n"
		"See the GNU General Public License for more details.\n\n");

	if (!jpeg_std_error(&jerr))
		fatal("jpeg_std_error() failed");

	printf("\nlibjpeg version: %s\n%s\n",
		jerr.jpeg_message_table[JMSG_VERSION],
		jerr.jpeg_message_table[JMSG_COPYRIGHT]);
}


void parse_arguments(int argc, char **argv, char *dest_path, size_t dest_path_len)
{
	int opt_index;
	int i, c;

	while(1) {
		opt_index=0;
		if ((c = getopt_long(argc,argv,"d:hm:nstqvfVpPoT:S:bw:",
						long_options, &opt_index)) == -1)
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
			if (realpath(optarg,dest_path)==NULL)
				fatal("invalid destination directory: %s", optarg);
			if (!is_directory(dest_path))
				fatal("destination not a directory: %s", dest_path);
			strncatenate(dest_path, DIR_SEPARATOR_S, dest_path_len);
			if (verbose_mode)
				fprintf(stderr,"Destination directory: %s\n",dest_path);
			dest=1;
			break;

		case 'v':
			verbose_mode++;
			break;

		case 'h':
			print_usage();
			exit(0);
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

		case 'V':
			print_version();
			exit(0);
			break;

		case 'o':
			overwrite_mode=1;
			break;

		case 'p':
			preserve_mode=1;
			break;

		case 'P':
			preserve_perms=1;
			break;

		case 's':
			save_exif=0;
			save_iptc=0;
			save_com=0;
			save_icc=0;
			save_xmp=0;
			save_adobe=0;
			save_jfif=0;
			save_jfxx=0;
			break;

		case 'T':
		        {
				double tmpvar;
				if (sscanf(optarg,"%lf", &tmpvar) == 1) {
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
				if (sscanf(optarg,"%u", &tmpvar) == 1) {
					if (tmpvar > 0 && tmpvar < 100 &&
						optarg[strlen(optarg)-1] == '%' ) {
						target_size=-tmpvar;
					} else {
						target_size=tmpvar;
					}
					quality=100;
				}
				else fatal("invalid argument for -S, --size");
			}
			break;

#ifdef PARALLEL_PROCESSING
		case 'w':
		        {
				int tmpvar;
				if (sscanf(optarg, "%d", &tmpvar) == 1) {
					if (tmpvar > 0 && tmpvar <= MAX_WORKERS)
						max_workers = tmpvar;
				}
				else fatal("invalid argument for -w, --workers");
			}
			break;
#endif

		case 'F':
		        {
				if (optarg[0] == '-' && optarg[1] == 0) {
					files_stdin = 1;
					break;
				}
				if (!is_file(optarg, NULL))
					fatal("argument for --files-from must be a file");
				if ((files_from = fopen(optarg, "r")) == NULL)
					fatal("cannot open file: '%s'", optarg);
			}
			break;

		case '?':
			exit(1);

		}
	}


	/* check for '-' option indicating input is from stdin... */
	i = optind;
	while (argv[i]) {
		if (argv[i][0]=='-' && argv[i][1]==0)
			stdin_mode=1;
		i++;
	}

	if (stdin_mode)
		stdout_mode=1;

	if (all_normal && all_progressive)
		fatal("cannot specify both --all-normal and --all-progressive");

	if (files_stdin)
		files_from = stdin;

	if (stdin_mode && files_from == stdin)
		fatal("cannot specify both --stdin and --files-stdin");
}


void own_signal_handler(int a)
{
	if (verbose_mode > 1)
		fprintf(stderr,PROGRAMNAME ": died from signal: %d\n",a);
	exit(2);
}


void write_markers(struct jpeg_decompress_struct *dinfo,
		struct jpeg_compress_struct *cinfo)
{
	jpeg_saved_marker_ptr mrk;
	int write_marker;
	const char *s_name;

	if (!cinfo || !dinfo)
		fatal("invalid call to write_markers()");

	mrk=dinfo->marker_list;
	while (mrk) {
		write_marker = 0;
		s_name = jpeg_special_marker_name(mrk);

		/* Check for markers to save... */

		if (save_com && mrk->marker == JPEG_COM)
			write_marker++;

		if (save_iptc && !strncmp(s_name, "IPTC", 5))
			write_marker++;

		if (save_exif && !strncmp(s_name, "Exif", 5))
			write_marker++;

		if (save_icc && !strncmp(s_name, "ICC", 4))
			write_marker++;

		if (save_xmp && !strncmp(s_name, "XMP", 4))
			write_marker++;

		if (save_jfxx && !strncmp(s_name, "JFXX", 5))
			write_marker++;

		if (save_adobe && !strncmp(s_name, "Adobe", 6))
			write_marker++;

		if (strip_none)
			write_marker++;


		/* libjpeg emits some markers automatically so skip these to avoid duplicates... */

		if (!strncmp(s_name, "JFIF", 5))
			write_marker=0;


		if (verbose_mode > 2)
			fprintf(jpeg_log_fh, " (Marker %s [%s]: %s) ", jpeg_marker_name(mrk->marker),
				s_name, (write_marker ? "Keep" : "Discard"));
		if (write_marker)
			jpeg_write_marker(cinfo, mrk->marker, mrk->data, mrk->data_length);

		mrk=mrk->next;
	}
}


unsigned int parse_markers(const struct jpeg_decompress_struct *dinfo,
			char *str, unsigned int str_size, unsigned int *markers_total_size)
{
	jpeg_saved_marker_ptr m;
	unsigned int count = 0;
	char *seen;
	size_t marker_types = jpeg_special_marker_types_count();
	int com_seen = 0;
	int special;

	if ((seen = malloc(marker_types)) == NULL)
		fatal("not enough of memory");

	memset(seen, 0, marker_types);
	str[0] = 0;
	*markers_total_size = 0;

	m = dinfo->marker_list;
	while (m) {
		count++;
		*markers_total_size += m->data_length;

		if ((special = jpeg_special_marker(m)) >= 0) {
			if (!seen[special])
				str_add_list(str, str_size, jpeg_special_marker_types[special].name, ",");
			seen[special]++;
		}

		if (m->marker == JPEG_COM && !com_seen) {
			str_add_list(str, str_size, "COM", ",");
			com_seen = 1;
		}

		m = m->next;
	}

	free(seen);

	return count;
}


int optimize(FILE *log_fh, const char *filename, const char *newname,
	const char *tmpdir, struct stat *file_stat,
	double *rate, double *saved)
{
	FILE *infile = NULL;
	FILE *outfile = NULL;
	const char *outfname = NULL;
	char tmpfilename[MAXPATHLEN];
	struct jpeg_decompress_struct dinfo;
	struct jpeg_compress_struct cinfo;
	struct my_error_mgr jcerr, jderr;
	JSAMPARRAY buf = NULL;

	unsigned char *outbuffer = NULL;
	size_t outbuffersize = 0;
	unsigned char *inbuffer = NULL;
	size_t inbuffersize = 0;
	size_t inbufferused = 0;

	jvirt_barray_ptr *coef_arrays = NULL;
	char marker_str[256];
	unsigned int marker_in_count, marker_in_size;

	long insize = 0, outsize = 0, lastsize = 0;
	long inpos;
	int j;
	int oldquality, searchcount, searchdone;
	double ratio;
	int res = -1;

	jpeg_log_fh = log_fh;

	/* Initialize decompression object */
	dinfo.err = jpeg_std_error(&jderr.pub);
	jpeg_create_decompress(&dinfo);
	jderr.pub.error_exit=my_error_exit;
	jderr.pub.output_message=my_output_message;
	jderr.jump_set = 0;

	/* Initialize compression object */
	cinfo.err = jpeg_std_error(&jcerr.pub);
	jpeg_create_compress(&cinfo);
	jcerr.pub.error_exit=my_error_exit;
	jcerr.pub.output_message=my_output_message;
	jcerr.jump_set = 0;

	if (rate)
		*rate = 0.0;
	if (saved)
		*saved = 0.0;

retry_point:
	if (filename) {
		if ((infile = fopen(filename, "rb")) == NULL) {
			warn("cannot open file: %s", filename);
			res = 1;
			goto exit_point;
		}
	} else {
		infile = stdin;
		set_filemode_binary(infile);
	}

	if (setjmp(jderr.setjmp_buffer)) {
		/* Error handler for decompress */
	abort_decompress:
		jpeg_abort_decompress(&dinfo);
		fclose(infile);
		if (buf)
			FREE_LINE_BUF(buf,dinfo.output_height);
		if (!quiet_mode || csv)
			fprintf(log_fh,csv ? ",,,,,error\n" : " [ERROR]\n");
		jderr.jump_set=0;
		res = 1;
		goto exit_point;
	} else {
		jderr.jump_set=1;
	}

	if (!retry && (!quiet_mode || csv)) {
		fprintf(log_fh,csv ? "%s," : "%s ",(filename ? filename:"stdin"));
		fflush(log_fh);
	}

	/* Prepare to decompress */
	if (stdin_mode || stdout_mode) {
		if (inbuffer)
			free(inbuffer);
		inbuffersize=65536;
		inbuffer=malloc(inbuffersize);
		if (!inbuffer)
			fatal("not enough memory");
	}
	global_error_counter=0;
	jpeg_save_markers(&dinfo, JPEG_COM, 0xffff);
	for (j=0;j<=15;j++) {
		jpeg_save_markers(&dinfo, JPEG_APP0+j, 0xffff);
	}
	jpeg_custom_src(&dinfo, infile, &inbuffer, &inbuffersize, &inbufferused, 65536);
	jpeg_read_header(&dinfo, TRUE);

	/* Check for known (Exif, IPTC, ICC , XMP, ...) markers */
	marker_in_count = parse_markers(&dinfo, marker_str, sizeof(marker_str),
					&marker_in_size);
	if (verbose_mode > 1) {
		fprintf(log_fh,"%d markers found in input file (total size %d bytes)\n",
			marker_in_count,marker_in_size);
		fprintf(log_fh,"coding: %s\n", (dinfo.arith_code == TRUE ? "Arithmetic" : "Huffman"));
	}


	if (!retry && (!quiet_mode || csv)) {
		fprintf(log_fh,csv ? "%dx%d,%dbit,%c," : "%dx%d %dbit %c ",(int)dinfo.image_width,
			(int)dinfo.image_height,(int)dinfo.num_components*8,
			(dinfo.progressive_mode?'P':'N'));
		if (!csv)
			fprintf(log_fh,"%s",marker_str);
		fflush(log_fh);
	}

	if ((insize=file_size(infile)) < 0)
		fatal("failed to stat() input file");

	/* Decompress the image */
	if (quality >= 0 && !retry) {
		jpeg_start_decompress(&dinfo);

		/* Allocate line buffer to store the decompressed image */
		buf = malloc(sizeof(JSAMPROW)*dinfo.output_height);
		if (!buf) fatal("not enough memory");
		for (j=0;j<dinfo.output_height;j++) {
			buf[j]=malloc(sizeof(JSAMPLE)*dinfo.output_width*
				dinfo.out_color_components);
			if (!buf[j])
				fatal("not enough memory");
		}

		while (dinfo.output_scanline < dinfo.output_height) {
			jpeg_read_scanlines(&dinfo,&buf[dinfo.output_scanline],
					dinfo.output_height-dinfo.output_scanline);
		}
	} else {
		coef_arrays = jpeg_read_coefficients(&dinfo);
		if (!coef_arrays) {
			if (!quiet_mode)
				fprintf(log_fh, " (failed to read coefficients) ");
			goto abort_decompress;
		}
	}

	inpos = ftell(infile);
	if (inpos > 0 && inpos < insize) {
		if (!quiet_mode)
			fprintf(log_fh, " (%lu bytes extraneous data found after end of image) ",
				insize - inpos);
		if (nofix_mode)
			global_error_counter++;
	}

	if (!retry && !quiet_mode) {
		fprintf(log_fh,(global_error_counter==0 ? " [OK] " : " [WARNING] "));
		fflush(log_fh);
	}

	if (stdin_mode)
		insize = inbufferused;

	if (nofix_mode && global_error_counter != 0) {
		/* Skip files containing any errors (or warnings) */
		goto abort_decompress;
	}

	if (dest && !noaction) {
		if (file_exists(newname) && !overwrite_mode) {
			if (!quiet_mode)
				fprintf(log_fh, " (target file already exists) ");
			goto abort_decompress;
		}
	}


	if (setjmp(jcerr.setjmp_buffer)) {
		/* Error handler for compress failures */
		jpeg_abort_compress(&cinfo);
		jpeg_abort_decompress(&dinfo);
		fclose(infile);
		if (!quiet_mode)
			fprintf(log_fh," [Compress ERROR: %s]\n",last_error);
		if (buf)
			FREE_LINE_BUF(buf,dinfo.output_height);
		jcerr.jump_set=0;
		res = 2;
		goto exit_point;
	} else {
		jcerr.jump_set=1;
	}

	lastsize = 0;
	searchcount = 0;
	searchdone = 0;
	oldquality = 200;
	if (target_size != 0) {
		/* Always start with quality 100 if -S option specified... */
		quality = 100;
	}


binary_search_loop:

	/* Allocate memory buffer that should be large enough to store the output JPEG... */
	if (outbuffer)
		free(outbuffer);
	outbuffersize=insize + 32768;
	outbuffer=malloc(outbuffersize);
	if (!outbuffer)
		fatal("not enough memory");

	/* setup custom "destination manager" for libjpeg to write to our buffer */
	jpeg_memory_dest(&cinfo, &outbuffer, &outbuffersize, 65536);


	if (quality >= 0 && !retry) {
		/* Lossy "optimization" ... */

		cinfo.in_color_space=dinfo.out_color_space;
		cinfo.input_components=dinfo.output_components;
		cinfo.image_width=dinfo.image_width;
		cinfo.image_height=dinfo.image_height;
		jpeg_set_defaults(&cinfo);
		jpeg_set_quality(&cinfo,quality,TRUE);
#ifdef HAVE_JINT_DC_SCAN_OPT_MODE
		if (jpeg_c_int_param_supported(&cinfo, JINT_DC_SCAN_OPT_MODE))
			jpeg_c_set_int_param(&cinfo, JINT_DC_SCAN_OPT_MODE, 1);
#endif
		if (all_normal || (!dinfo.progressive_mode && !all_progressive)) {
			/* Explicitly disable progressive mode. */
			cinfo.scan_info = NULL;
			cinfo.num_scans = 0;
		} else if (all_progressive || dinfo.progressive_mode) {
			/* Enable progressive mode. */
			jpeg_simple_progression(&cinfo);
		}
		cinfo.optimize_coding = TRUE;
#ifdef HAVE_ARITH_CODE
		if (arith_mode >= 0)
			cinfo.arith_code = (arith_mode > 0 ? TRUE : FALSE);
#endif
		if (dinfo.saw_JFIF_marker && (save_jfif || strip_none)) {
			cinfo.write_JFIF_header = TRUE;
		} else {
			cinfo.write_JFIF_header = FALSE;
		}
		if (dinfo.saw_Adobe_marker && (save_adobe || strip_none)) {
			/* If outputting Adobe marker, don't write JFIF marker... */
			cinfo.write_JFIF_header = FALSE;
		}

		j=0;
		jpeg_start_compress(&cinfo,TRUE);

		/* Write markers */
		write_markers(&dinfo,&cinfo);

		/* Write image */
		while (cinfo.next_scanline < cinfo.image_height) {
			jpeg_write_scanlines(&cinfo,&buf[cinfo.next_scanline],
					dinfo.output_height);
		}

	} else {
		/* Lossless optimization ... */

		jpeg_copy_critical_parameters(&dinfo, &cinfo);
#ifdef HAVE_JINT_DC_SCAN_OPT_MODE
		if (jpeg_c_int_param_supported(&cinfo, JINT_DC_SCAN_OPT_MODE))
			jpeg_c_set_int_param(&cinfo, JINT_DC_SCAN_OPT_MODE, 1);
#endif
		if (all_normal || (!dinfo.progressive_mode && !all_progressive)) {
			/* Explicitly disable progressive mode. */
			cinfo.scan_info = NULL;
			cinfo.num_scans = 0;
		} else if (all_progressive || dinfo.progressive_mode) {
			/* Enable progressive mode. */
			jpeg_simple_progression(&cinfo);
		}
		cinfo.optimize_coding = TRUE;
#ifdef HAVE_ARITH_CODE
		if (arith_mode >= 0)
			cinfo.arith_code = (arith_mode > 0 ? TRUE : FALSE);
#endif
		if (dinfo.saw_JFIF_marker && (save_jfif || strip_none)) {
			cinfo.write_JFIF_header = TRUE;
		} else {
			cinfo.write_JFIF_header = FALSE;
		}
		if (dinfo.saw_Adobe_marker && (save_adobe || strip_none)) {
			/* If outputting Adobe marker, don't write JFIF marker... */
			cinfo.write_JFIF_header = FALSE;
		}

		/* Write image */
		jpeg_write_coefficients(&cinfo, coef_arrays);

		/* Write markers */
		write_markers(&dinfo,&cinfo);

	}

	jpeg_finish_compress(&cinfo);
	outsize=outbuffersize;

	if (target_size != 0 && !retry) {
		/* Perform (binary) search to try to reach target file size... */

		long osize = outsize/1024;
		long isize = insize/1024;
		long tsize = target_size;

		if (tsize < 0) {
			tsize=((-target_size)*insize/100)/1024;
			if (tsize < 1)
				tsize = 1;
		}

		if (osize == tsize || searchdone || searchcount >= 8 || tsize > isize) {
			if (searchdone < 42 && lastsize > 0) {
				if (labs(osize-tsize) > labs(lastsize-tsize)) {
					if (verbose_mode) fprintf(log_fh,"(revert to %d)",oldquality);
					searchdone=42;
					quality=oldquality;
					goto binary_search_loop;
				}
			}
			if (verbose_mode)
				fprintf(log_fh," ");

		} else {
			int newquality;
			int dif = floor((abs(oldquality-quality)/2.0)+0.5);
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

			if (verbose_mode)
				fprintf(log_fh,"(try %d)",quality);

			lastsize=osize;
			searchcount++;
			goto binary_search_loop;
		}
	}

	if (buf)
		FREE_LINE_BUF(buf,dinfo.output_height);
	jpeg_finish_decompress(&dinfo);
	fclose(infile);


	if (quality >= 0 && outsize >= insize && !retry && !stdin_mode) {
		if (verbose_mode)
			fprintf(log_fh,"(retry w/lossless) ");
		retry=1;
		goto retry_point;
	}

	retry=0;
	ratio=(insize-outsize)*100.0/insize;
	if (!quiet_mode || csv)
		fprintf(log_fh,csv ? "%ld,%ld,%0.2f," : "%ld --> %ld bytes (%0.2f%%), ",insize,outsize,ratio);
	if (rate) {
		*rate = (ratio < 0 ? 0.0 : ratio);
	}

	if ((outsize < insize && ratio >= threshold) || force) {
		if (saved) {
			*saved = (insize - outsize) / 1024.0;
		}
		if (!quiet_mode || csv)
			fprintf(log_fh,csv ? "optimized\n" : "optimized.\n");
		if (noaction) {
			res = 0;
			goto exit_point;
		}

		if (stdout_mode) {
			outfname=NULL;
			set_filemode_binary(stdout);
			if (fwrite(outbuffer,outbuffersize,1,stdout) != 1)
				fatal("%s, write failed to stdout",(stdin_mode ? "stdin" : filename));
		} else {
			if (preserve_perms && !dest) {
				/* make backup of the original file */
				int newlen = snprintf(tmpfilename, sizeof(tmpfilename),
						"%s.jpegoptim.bak", newname);
				if (newlen >= sizeof(tmpfilename))
					warn("temp filename too long: %s", tmpfilename);

				if (verbose_mode > 1 && !quiet_mode)
					fprintf(log_fh,"%s, creating backup as: %s\n",
						(stdin_mode ? "stdin" : filename), tmpfilename);
				if (file_exists(tmpfilename))
					fatal("%s, backup file already exists: %s",
						(stdin_mode ?" stdin" : filename), tmpfilename);
				if (copy_file(newname,tmpfilename))
					fatal("%s, failed to create backup: %s",
						(stdin_mode ? "stdin" : filename), tmpfilename);
				if ((outfile=fopen(newname,"wb"))==NULL)
					fatal("%s, error opening output file: %s",
						(stdin_mode ? "stdin" : filename), newname);
				outfname=newname;
			} else {
#ifdef HAVE_MKSTEMPS
				/* rely on mkstemps() to create us temporary file safely... */
				int newlen = snprintf(tmpfilename,sizeof(tmpfilename),
						"%sjpegoptim-%d-%d.XXXXXX.tmp",
						tmpdir, (int)getuid(), (int)getpid());
				if (newlen >= sizeof(tmpfilename))
					warn("temp filename too long: %s", tmpfilename);
				int tmpfd = mkstemps(tmpfilename,4);
				if (tmpfd < 0)
					fatal("%s, error creating temp file %s: mkstemps() failed",
						(stdin_mode ? "stdin" : filename), tmpfilename);
				if ((outfile = fdopen(tmpfd,"wb")) == NULL)
#else
					/* if platform is missing mkstemps(), try to create
					   at least somewhat "safe" temp file... */
					snprintf(tmpfilename,sizeof(tmpfilename),
						"%sjpegoptim-%d-%d.%ld.tmp", tmpdir,
						(int)getuid(), (int)getpid(), (long)time(NULL));
				if ((outfile = fopen(tmpfilename,"wb")) == NULL)
#endif
					fatal("error opening temporary file: %s", tmpfilename);
				outfname=tmpfilename;
			}

			if (verbose_mode > 1 && !quiet_mode)
				fprintf(log_fh,"writing %lu bytes to file: %s\n",
					(long unsigned int)outbuffersize, outfname);
			if (fwrite(outbuffer,outbuffersize,1,outfile) != 1)
				fatal("write failed to file: %s", outfname);
			fclose(outfile);
		}

		if (outfname) {
			if (preserve_mode) {
				/* preserve file modification time */
				if (verbose_mode > 1 && !quiet_mode)
					fprintf(log_fh,"set file modification time same as in original: %s\n",
						outfname);
#if defined(HAVE_UTIMENSAT) && defined(HAVE_STRUCT_STAT_ST_MTIM)
				struct timespec time_save[2];
				time_save[0].tv_sec = 0;
				time_save[0].tv_nsec = UTIME_OMIT;	/* omit atime */
				time_save[1] = file_stat->st_mtim;
				if (utimensat(AT_FDCWD,outfname,time_save,0) != 0)
					warn("failed to reset output file time/date");
#else
				struct utimbuf time_save;
				time_save.actime=file_stat->st_atime;
				time_save.modtime=file_stat->st_mtime;
				if (utime(outfname,&time_save) != 0)
					warn("failed to reset output file time/date");
#endif
			}

			if (preserve_perms && !dest) {
				/* original file was already replaced, remove backup... */
				if (verbose_mode > 1 && !quiet_mode)
					fprintf(log_fh,"removing backup file: %s\n", tmpfilename);
				if (delete_file(tmpfilename))
					warn("failed to remove backup file: %s", tmpfilename);
			} else {
				/* make temp file to be the original file... */

				/* preserve file mode */
				if (chmod(outfname,(file_stat->st_mode & 0777)) != 0)
					warn("failed to set output file mode");

				/* preserve file group (and owner if run by root) */
				if (chown(outfname,
						(geteuid()==0 ? file_stat->st_uid : -1),
						file_stat->st_gid) != 0)
					warn("failed to reset output file group/owner");

				if (verbose_mode > 1 && !quiet_mode)
					fprintf(log_fh,"renaming: %s to %s\n", outfname, newname);
				if (rename_file(outfname, newname))
					fatal("cannot rename temp file");
			}
		}
	} else {
		if (!quiet_mode || csv)
			fprintf(log_fh,csv ? "skipped\n" : "skipped.\n");
		if (stdout_mode) {
			set_filemode_binary(stdout);
			if (fwrite(inbuffer, inbufferused, 1, stdout) != 1)
				fatal("%s, write failed to stdout",
					(stdin_mode ? "stdin" : filename));
		}
	}

	res = 0;

 exit_point:
	if (inbuffer)
		free(inbuffer);
	if (outbuffer)
		free(outbuffer);
	jpeg_destroy_compress(&cinfo);
	jpeg_destroy_decompress(&dinfo);

	return res;
}


#ifdef PARALLEL_PROCESSING
int wait_for_worker(FILE *log_fh)
{
	FILE *p;
	struct worker *w;
	char buf[1024];
	int wstatus;
	pid_t pid;
	int j, e;
	int state = 0;
	double val;
	double rate = 0.0;
	double saved = 0.0;


	if ((pid = wait(&wstatus)) < 0)
		return pid;

	w = NULL;
	for (j = 0; j < MAX_WORKERS; j++) {
		if (workers[j].pid == pid) {
			w = &workers[j];
			break;
		}
	}
	if (!w)
		fatal("Unknown worker[%d] process found\n", pid);

	if (WIFEXITED(wstatus)) {
		e = WEXITSTATUS(wstatus);
		if (verbose_mode)
			fprintf(log_fh, "worker[%d] [slot=%d] exited: %d\n",
				pid, j, e);
		if (e == 0) {
			//average_count++;
			//average_rate += rate;
			//total_save += saved;
		} else if (e == 1) {
			decompress_err_count++;
		} else if (e == 2) {
			compress_err_count++;
		}
	} else {
		fatal("worker[%d] killed", pid);
	}

	p = fdopen(w->read_pipe, "r");
	if (!p) fatal("fdopen failed()");
	while (fgets(buf, sizeof(buf), p)) {
		if (verbose_mode > 2)
			fprintf(log_fh, "worker[%d] PIPE: %s", pid, buf);
		if (state == 0 && buf[0] == '\n') {
			state=1;
			continue;
		}
		if (state == 1 && !strncmp(buf, "STAT", 4)) {
			state=2;
			continue;
		}
		if (state >= 2) {
			if (sscanf(buf, "%lf", &val) == 1) {
				if (state == 2) {
					rate = val;
				}
				else if (state == 3) {
					saved = val;
					average_count++;
					average_rate += rate;
					total_save += saved;
				}
			}
			state++;
			continue;
		}
		if (state == 0)
			fprintf(log_fh, "%s", buf);
	}
	close(w->read_pipe);
	w->pid = -1;
	w->read_pipe = -1;
	worker_count --;

	return pid;
}
#endif


/****************************************************************************/
int main(int argc, char **argv)
{
	struct stat file_stat;
	char tmpfilename[MAXPATHLEN + 1],tmpdir[MAXPATHLEN + 1];
	char newname[MAXPATHLEN + 1], dest_path[MAXPATHLEN + 1];
	char namebuf[MAXPATHLEN + 2];
	const char *filename;
	volatile int i;
	int res;
	double rate, saved;
	FILE *log_fh;
#ifdef PARALLEL_PROCESSING
	struct worker *w;
	int pipe_fd[2];
	pid_t pid;
	int j;

	/* Allocate table to keep track of child processes... */
	if (!(workers = malloc(sizeof(struct worker) * MAX_WORKERS)))
		fatal("not enough memory");
	for (i = 0; i < MAX_WORKERS; i++) {
		workers[i].pid = -1;
		workers[i].read_pipe = -1;
	}
#endif

	umask(077);
	signal(SIGINT,own_signal_handler);
	signal(SIGTERM,own_signal_handler);

	/* Parse command line parameters */
	parse_arguments(argc, argv, dest_path, sizeof(dest_path));
	log_fh = (stdout_mode ? stderr : stdout);

	if (verbose_mode) {
		if (quality >= 0 && target_size == 0)
			fprintf(log_fh, "Image quality limit set to: %d\n", quality);
		if (threshold >= 0)
			fprintf(log_fh, "Compression threshold (%%) set to: %0.1lf\n", threshold);
		if (all_normal)
			fprintf(log_fh, "All output files will be non-progressive\n");
		if (all_progressive)
			fprintf(log_fh, "All output files will be progressive\n");
		if (target_size > 0)
			fprintf(log_fh, "Target size for output files set to: %d Kbytes.\n",
				target_size);
		if (target_size < 0)
			fprintf(log_fh, "Target size for output files set to: %d%%\n",
				-target_size);
#ifdef PARALLEL_PROCESSING
		if (max_workers > 0)
			fprintf(log_fh, "Using maximum of %d parallel threads\n", max_workers);
#endif
	}


	if (stdin_mode) {
		/* Process just one file, if source is stdin... */
		res = optimize(stderr, NULL, NULL, NULL, &file_stat, NULL, NULL);
		return (res == 0 ? 0 : 1);
	}

	i=(optind > 0 ? optind : 1);
	if (files_from == NULL && argc <= i) {
		if (!quiet_mode)
			fprintf(stderr, PROGRAMNAME ": file argument(s) missing\n"
				"Try '" PROGRAMNAME " --help' for more information.\n");
		exit(1);
	}


	/* Main loop to process input files */
	do {
		if (files_from) {
			if (!fgetstr(namebuf, sizeof(namebuf), files_from))
				break;
			filename = namebuf;
		} else {
			filename = argv[i];
		}

		if (*filename == 0)
			continue;
		if (verbose_mode > 1)
			fprintf(log_fh, "processing file: %s\n", filename);
		if (strnlen(filename, MAXPATHLEN + 1) > MAXPATHLEN) {
			warn("skipping too long filename: %s", filename);
			continue;
		}

		if (!noaction) {
			/* generate tmp dir & new filename */
			if (dest) {
				strncopy(tmpdir, dest_path, sizeof(tmpdir));
				strncopy(newname, dest_path, sizeof(newname));
				if (!splitname(filename, tmpfilename, sizeof(tmpfilename)))
					fatal("splitname() failed for: %s", filename);
				strncatenate(newname, tmpfilename, sizeof(newname));
			} else {
				if (!splitdir(filename, tmpdir, sizeof(tmpdir)))
					fatal("splitdir() failed for: %s", filename);
				strncopy(newname, filename, sizeof(newname));
			}
		}

		if (file_exists(filename)) {
			if (!is_file(filename, &file_stat)) {
				if (is_directory(filename))
					warn("skipping directory: %s", filename);
				else
					warn("skipping special file: %s", filename);
				continue;
			}
		} else {
			warn("file not found: %s", filename);
			continue;
		}

#ifdef PARALLEL_PROCESSING
		if (max_workers > 1) {
			/* Multi process mode, run up to max_workers processes simultaneously... */

			if (worker_count >= max_workers) {
				// wait for a worker to exit...
				wait_for_worker(log_fh);
			}
			if (pipe(pipe_fd) < 0)
				fatal("failed to open pipe");
			pid = fork();
			if (pid < 0)
				fatal("fork() failed");
			if (pid == 0) {
				/* Child process starts here... */
				if (files_from)
					fclose(files_from);
				close(pipe_fd[0]);
				FILE *p;

				if (!(p = fdopen(pipe_fd[1],"w")))
					fatal("worker: fdopen failed");

				res = optimize(p, filename, newname, tmpdir, &file_stat, &rate, &saved);
				if (res == 0)
					fprintf(p, "\n\nSTATS\n%lf\n%lf\n", rate, saved);
				exit(res);
			} else {
				/* Parent continues here... */
				close(pipe_fd[1]);

				w = NULL;
				for (j = 0; j < MAX_WORKERS; j++) {
					if (workers[j].pid < 0) {
						w = &workers[j];
						break;
					}
				}
				if (!w)
					fatal("no space to start a new worker (%d)", worker_count);
				w->pid = pid;
				w->read_pipe = pipe_fd[0];
				worker_count++;
				if (verbose_mode > 0)
					fprintf(log_fh, "worker[%d] [slot=%d] started\n", pid, j);;
			}

		} else
#endif
		{
			/* Single process mode, process one file at a time... */

			res = optimize(log_fh, filename, newname, tmpdir, &file_stat, &rate, &saved);
			if (res == 0) {
				average_count++;
				average_rate += rate;
				total_save += saved;
			} else if (res == 1) {
				decompress_err_count++;
			} else if (res == 2) {
				compress_err_count++;
			}
		}

	} while (files_from || ++i < argc);


#ifdef PARALLEL_PROCESSING
	/* Wait for any child processes to exit... */
	if (max_workers > 1) {
		if (verbose_mode) {
			fprintf(log_fh, "Waiting for %d workers to finish...\n", worker_count);
		}
		while ((pid = wait_for_worker(log_fh)) > 0) {
			if (verbose_mode > 2)
				fprintf(log_fh, "worker[%d] done\n", pid);
		}
	}
#endif

	if (totals_mode && !quiet_mode)
		fprintf(log_fh, "Average ""compression"" (%ld files): %0.2f%% (total saved %0.0fk)\n",
			average_count, average_rate/average_count, total_save);


	return (decompress_err_count > 0 || compress_err_count > 0 ? 1 : 0);;
}

/* eof :-) */

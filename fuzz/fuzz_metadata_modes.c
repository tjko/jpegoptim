/*
 * fuzz_metadata_modes.c
 *
 * Fuzz harness exercising jpegoptim's metadata handling across the full
 * option matrix. The fuzz build compiles main() out (BUILD_FOR_OSS_FUZZ),
 * and the non-static option globals (save_mpf, strip_ai, ...) are the
 * designed fuzz seam: they mirror the command line options parsed by
 * parse_argv() in a real invocation, so setting them from the harness
 * drives exactly the code paths a user's jpegoptim run would reach.
 *
 * Input format:
 *   bytes 0-3 : little endian option profile (see CFG_* bits below)
 *   bytes 4-  : JPEG file payload, written to the harness input file
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This file is part of JPEGoptim.
 *
 * JPEGoptim is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, either version 3
 * of the License, or (at your option) any later version.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "fuzz_manager.h"
#include "jpegoptim.h"

/* Option globals from jpegoptim.c (set from the command line by the real
   tool; kept at CLI defaults here unless the input profile says otherwise) */
extern int save_extra, save_exif, save_iptc, save_com, save_icc, save_xmp;
extern int save_adobe, save_jfxx, save_jfif, save_mpf, save_iso21496;
extern int strip_none, strip_ai, retry_mode, verbose_mode, quiet_mode, csv;
extern int noaction, dest, overwrite_mode, force, nofix_mode;
extern int preserve_mtime, preserve_perms, stdin_mode, quality;
extern double threshold;
extern uint64_t max_pixels;

/* Option profile bits (from the 4 byte config header) */
#define CFG_MPF             (1u << 0)  /* --keep-mpf (implies --save-extra) */
#define CFG_ISO21496        (1u << 1)  /* --keep-hdr (implies mpf/xmp/extra) */
#define CFG_KEEP_ALL        (1u << 2)  /* --keep-all / --strip-none */
#define CFG_STRIP_AI        (1u << 3)  /* --strip-ai */
#define CFG_KEEP_ADOBE      (1u << 4)  /* --keep-adobe */
#define CFG_KEEP_JFXX       (1u << 5)  /* --keep-jfxx */
#define CFG_RETRY           (1u << 6)  /* --retry shrink loop */
#define CFG_NOACTION        (1u << 7)  /* --noaction */
#define CFG_THRESHOLD       (1u << 8)  /* --threshold 50 */
#define CFG_FORCE           (1u << 9)  /* --force */
#define CFG_CSV             (1u << 10) /* --csv */
#define CFG_QUIET           (1u << 11) /* --quiet */
#define CFG_DEST            (1u << 12) /* --dest (into the temp dir) */
#define CFG_OVERWRITE       (1u << 13) /* --overwrite */
#define CFG_PRESERVE_PERMS  (1u << 14) /* --preserve-perms (backup path) */
#define CFG_PRESERVE_MTIME  (1u << 15) /* --preserve */
#define CFG_NOFIX           (1u << 16) /* --nofix */
#define CFG_MAX_PIXELS      (1u << 17) /* --max-pixels 1 */
#define CFG_STDIN           (1u << 18) /* --stdin (buffer/insize handling) */
#define CFG_SAVE_EXTRA      (1u << 19) /* --save-extra */
#define CFG_NO_COM          (1u << 20) /* --strip-com */
#define CFG_NO_EXIF         (1u << 21) /* --strip-exif */
#define CFG_NO_IPTC         (1u << 22) /* --strip-iptc */
#define CFG_NO_ICC          (1u << 23) /* --strip-icc */
#define CFG_NO_XMP          (1u << 24) /* --strip-xmp */
#define CFG_NO_JFIF         (1u << 25) /* --strip-jfif */
#define CFG_VERBOSE_SHIFT   26         /* --verbose 0..3 */
#define CFG_VERBOSE_MASK    (3u << CFG_VERBOSE_SHIFT)
#define CFG_TSIZE_SHIFT     28         /* --size modes: 0=none, %75, %40, 2k */
#define CFG_TSIZE_MASK      (3u << CFG_TSIZE_SHIFT)
#define CFG_MODE_SHIFT      30         /* 0=default, 1=normal, 2=progressive, 3=auto */
#define CFG_MODE_MASK       (3u << CFG_MODE_SHIFT)

static void apply_options(uint32_t cfg, int *auto_mode, int *all_normal,
			  int *all_progressive)
{
	/* Reset to CLI defaults every iteration to keep iterations independent */
	save_extra = 0;
	save_exif = 1;
	save_iptc = 1;
	save_com = 1;
	save_icc = 1;
	save_xmp = 1;
	save_adobe = 0;
	save_jfxx = 0;
	save_jfif = 1;
	save_mpf = 0;
	save_iso21496 = 0;
	strip_none = 0;
	strip_ai = 0;
	retry_mode = 0;
	verbose_mode = 0;
	quiet_mode = 0;
	csv = 0;
	noaction = 0;
	dest = 0;
	overwrite_mode = 0;
	force = 0;
	nofix_mode = 0;
	preserve_mtime = 0;
	preserve_perms = 0;
	stdin_mode = 0;
	quality = -1;
	threshold = -1.0;
	max_pixels = 0;

	if (cfg & CFG_MPF) {
		save_mpf = 1;
		save_extra = 1;
	}
	if (cfg & CFG_ISO21496) {
		save_iso21496 = 1;
		save_mpf = 1;
		save_xmp = 1;
		save_extra = 1;
	}
	if (cfg & CFG_KEEP_ALL) {
		strip_none = 1;
		/* As in the CLI: --keep-all implies --keep-mpf, and
		   --keep-mpf implies --save-extra. */
		save_mpf = 1;
		save_extra = 1;
	}
	if (cfg & CFG_STRIP_AI)
		strip_ai = 1;
	if (cfg & CFG_KEEP_ADOBE)
		save_adobe = 1;
	if (cfg & CFG_KEEP_JFXX)
		save_jfxx = 1;
	if (cfg & CFG_RETRY)
		retry_mode = 1;
	if (cfg & CFG_NOACTION)
		noaction = 1;
	if (cfg & CFG_THRESHOLD)
		threshold = 50.0;
	if (cfg & CFG_FORCE)
		force = 1;
	if (cfg & CFG_CSV)
		csv = 1;
	if (cfg & CFG_QUIET)
		quiet_mode = 1;
	if (cfg & CFG_DEST)
		dest = 1;
	if (cfg & CFG_OVERWRITE)
		overwrite_mode = 1;
	if (cfg & CFG_PRESERVE_PERMS)
		preserve_perms = 1;
	if (cfg & CFG_PRESERVE_MTIME)
		preserve_mtime = 1;
	if (cfg & CFG_NOFIX)
		nofix_mode = 1;
	if (cfg & CFG_MAX_PIXELS)
		max_pixels = 1000000;
	if (cfg & CFG_STDIN)
		stdin_mode = 1;
	if (cfg & CFG_SAVE_EXTRA)
		save_extra = 1;
	if (cfg & CFG_NO_COM)
		save_com = 0;
	if (cfg & CFG_NO_EXIF)
		save_exif = 0;
	if (cfg & CFG_NO_IPTC)
		save_iptc = 0;
	if (cfg & CFG_NO_ICC)
		save_icc = 0;
	if (cfg & CFG_NO_XMP)
		save_xmp = 0;
	if (cfg & CFG_NO_JFIF)
		save_jfif = 0;

	verbose_mode = (int)((cfg & CFG_VERBOSE_MASK) >> CFG_VERBOSE_SHIFT);

	switch ((cfg & CFG_TSIZE_MASK) >> CFG_TSIZE_SHIFT) {
	case 1:
		fuzz_set_target_size(-75);
		break;
	case 2:
		fuzz_set_target_size(-40);
		break;
	case 3:
		fuzz_set_target_size(2);
		break;
	default:
		fuzz_set_target_size(0);
		break;
	}

	*auto_mode = 0;
	*all_normal = 0;
	*all_progressive = 0;
	switch ((cfg & CFG_MODE_MASK) >> CFG_MODE_SHIFT) {
	case 1:
		*all_normal = 1;
		break;
	case 2:
		*all_progressive = 1;
		break;
	case 3:
		*auto_mode = 1;
		break;
	default:
		break;
	}
}

/* Guard against pathologically large images: jpegoptim allocates
   full-image buffers (line buffers / coefficient arrays), so a mutated
   giant SOF dimension would just exhaust the fuzz process memory. A real
   user hits the same wall and protects themselves with --max-pixels; the
   harness mirrors that contract by only feeding oversized images to
   profiles that select CFG_MAX_PIXELS, in which case optimize() itself
   rejects the image before decoding (the too-large-input path).
   The scan is deliberately liberal: every SOF-shaped byte sequence in the
   payload is checked, so mutated headers cannot slip past a strict
   marker-walk desync the way libjpeg's resynchronizing parser would not. */
static int image_too_large(const uint8_t *data, size_t size)
{
	if (size < 9)
		return 0;

	for (size_t i = 0; i + 9 <= size; i++) {
		uint32_t height, width;
		uint8_t marker = data[i + 1];

		if (data[i] != 0xFF)
			continue;
		switch (marker) {
		case 0xC0:
		case 0xC1:
		case 0xC2:
		case 0xC3:
		case 0xC9:
		case 0xCA:
		case 0xCF:
			break;
		default:
			continue;
		}
		height = ((uint32_t)data[i + 5] << 8) | data[i + 6];
		width = ((uint32_t)data[i + 7] << 8) | data[i + 8];
		if ((uint64_t)height * width > 16000000)
			return 1;
	}

	return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, const size_t size)
{
	int rc = -1;
	int auto_mode, all_normal, all_progressive;
	struct stat file_stat;
	uint32_t cfg;
	FILE *fuzz_file;

	if (size < 4)
		return 0;

	cfg = (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
	      ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
	data += 4;
	const size_t payload_size = size - 4;

	if (image_too_large(data, payload_size) && !(cfg & CFG_MAX_PIXELS))
		return 0;

	fuzz_manager_init();

	apply_options(cfg, &auto_mode, &all_normal, &all_progressive);

	if (0 != lstat(fuzz_manager.fuzz_file_name, &file_stat)) {
		/* Getting file stat failed */
		goto end;
	}

	/* Write payload to fuzz file */
	fuzz_file = fopen(fuzz_manager.fuzz_file_name, "wb+");
	if (!fuzz_file) {
		goto end;
	}
	if (payload_size !=
	    fwrite(data, sizeof(uint8_t), payload_size, fuzz_file)) {
		fclose(fuzz_file);
		goto end;
	}
	fclose(fuzz_file);

	/*
	 * As in the CLI: without --dest the file is optimized in
	 * place, so newname is the input pathname itself; only with
	 * --dest does the output go to the manager's separate path.
	 * The preserve_perms backup copies and truncates newname,
	 * so it must point at the file being optimized.
	 */
	optimize(fuzz_manager.log_fh, fuzz_manager.fuzz_file_name,
		 dest ? fuzz_manager.new_fuzz_file_name :
		 fuzz_manager.fuzz_file_name, fuzz_manager.tmp_dir,
		 &file_stat, &fuzz_manager.rate, &fuzz_manager.saved,
		 auto_mode, all_normal, all_progressive);

	rc = 0;

end:
	return rc;
}

/* eof :-) */

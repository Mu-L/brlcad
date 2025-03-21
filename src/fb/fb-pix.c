/*                        F B - P I X . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2025 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file fb-pix.c
 *
 * Program to take a frame buffer image and write a .pix image.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <sys/stat.h>

#include "bio.h"

#include "bu/app.h"
#include "bu/getopt.h"
#include "bu/exit.h"
#include "bu/malloc.h"
#include "vmath.h"
#include "dm.h"

#include "pkg.h"


char *framebuffer = NULL;
char *file_name;
FILE *outfp;

static int crunch = 0;		/* Color map crunch? */
static int inverse = 0;		/* Draw upside-down */
int screen_height;			/* input height */
int screen_width;			/* input width */

/* in cmap-crunch.c */
extern void cmap_crunch(RGBpixel (*scan_buf), int pixel_ct, ColorMap *colormap);


int
get_args(int argc, char **argv)
{
    int c;

    while ((c = bu_getopt(argc, argv, "ciF:s:w:n:h?")) != -1) {
	switch (c) {
	    case 'c':
		crunch = 1;
		break;
	    case 'i':
		inverse = 1;
		break;
	    case 'F':
		framebuffer = bu_optarg;
		break;
	    case 's':
		/* square size */
		screen_height = screen_width = atoi(bu_optarg);
		break;
	    case 'w':
		screen_width = atoi(bu_optarg);
		break;
	    case 'n':
		screen_height = atoi(bu_optarg);
		break;

	    default:		/* '?' */
		return 0;
	}
    }

    if (bu_optind >= argc) {
	if (isatty(fileno(stdout)))
	    return 0;
	file_name = "-";
	outfp = stdout;
	setmode(fileno(stdout), O_BINARY);
    } else {
	file_name = argv[bu_optind];
	if ((outfp = fopen(file_name, "wb")) == NULL) {
	    fprintf(stderr,
		    "fb-pix: cannot open \"%s\" for writing\n",
		    file_name);
	    return 0;
	}
	(void)bu_fchmod(fileno(outfp), 0444);
    }

    if (argc > ++bu_optind)
	fprintf(stderr, "fb-pix: excess argument(s) ignored\n");

    return 1;		/* OK */
}


int
main(int argc, char **argv)
{
    struct fb *fbp;
    int y;

    unsigned char *scanline = NULL;	/* 1 scanline pixel buffer */
    int scanbytes;		/* # of bytes of scanline */
    int scanpix;		/* # of pixels of scanline */
    ColorMap cmap;		/* libfb color map */

    char usage[] = "\
Usage: fb-pix [-i -c] [-F framebuffer]\n\
	[-s squaresize] [-w width] [-n height] [file.pix]\n";

    screen_height = screen_width = 512;		/* Defaults */

    bu_setprogname(argv[0]);

    if (!get_args(argc, argv)) {
	(void)fputs(usage, stderr);
	bu_exit(1, NULL);
    }

    setmode(fileno(stdout), O_BINARY);

    scanpix = screen_width;
    scanbytes = scanpix * sizeof(RGBpixel);
    if ((scanline = (unsigned char *)malloc(scanbytes)) == RGBPIXEL_NULL) {
	fprintf(stderr,
		"fb-pix:  malloc(%d) failure\n", scanbytes);
	bu_exit(2, NULL);
    }

    if ((fbp = fb_open(framebuffer, screen_width, screen_height)) == NULL) {
	bu_exit(12, NULL);
    }

    V_MIN(screen_height, fb_getheight(fbp));
    V_MIN(screen_width, fb_getwidth(fbp));

    if (crunch) {
	if (fb_rmap(fbp, &cmap) == -1) {
	    crunch = 0;
	} else if (fb_is_linear_cmap(&cmap)) {
	    crunch = 0;
	}
    }

    if (!inverse) {
	/* Regular -- read bottom to top */
	for (y=0; y < screen_height; y++) {
	    fb_read(fbp, 0, y, scanline, screen_width);
	    if (crunch)
		cmap_crunch((RGBpixel *)scanline, scanpix, &cmap);
	    if (fwrite((char *)scanline, scanbytes, 1, outfp) != 1) {
		perror("fwrite");
		break;
	    }
	}
    } else {
	/* Inverse -- read top to bottom */
	for (y = screen_height-1; y >= 0; y--) {
	    fb_read(fbp, 0, y, scanline, screen_width);
	    if (crunch)
		cmap_crunch((RGBpixel *)scanline, scanpix, &cmap);
	    if (fwrite((char *)scanline, scanbytes, 1, outfp) != 1) {
		perror("fwrite");
		break;
	    }
	}
    }
    fb_close(fbp);
    if (scanline)
	bu_free(scanline, "scanline");
    return 0;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

/*                          D M O D . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2025 United States Government as represented by
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
 */
/** @file dmod.c
 *
 * Modify a stream of doubles.
 *
 * Allows any number of add, subtract, multiply, divide, or
 * exponentiation operations to be performed on a stream of values.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bio.h"

#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/getopt.h"
#include "bu/exit.h"
#include "vmath.h"


#define ADD 1
#define MULT 2
#define ABS 3
#define POW 4


static const char usage[] = "Usage: dmod [-a add | -s sub | -m mult | -d div | -A | -e exp | -r root] [doubles]\n";
static const char progname[] = "dmod";
static FILE *infp = NULL;
static int numop = 0;		/* number of operations */
static int op[256] = {0};		/* operations */
static double val[256] = {0.0};		/* arguments to operations */


static int
get_args(int argc, char *argv[])
{
    int c;
    double d;

    while ((c = bu_getopt(argc, argv, "a:s:m:d:Ae:r:h?")) != -1) {
	switch (c) {
	    case 'a':
		op[ numop ] = ADD;
		val[ numop++ ] = atof(bu_optarg);
		break;
	    case 's':
		op[ numop ] = ADD;
		val[ numop++ ] = - atof(bu_optarg);
		break;
	    case 'm':
		op[ numop ] = MULT;
		val[ numop++ ] = atof(bu_optarg);
		break;
	    case 'd':
		op[ numop ] = MULT;
		d = atof(bu_optarg);
		if (ZERO(d)) {
		    bu_exit(2, "%s: divide by zero!\n", progname);
		}
		val[ numop++ ] = 1.0 / d;
		break;
	    case 'A':
		op[ numop ] = ABS;
		val[ numop++ ] = 0;
		break;
	    case 'e':
		op[ numop ] = POW;
		val[ numop++ ] = atof(bu_optarg);
		break;
	    case 'r':
		op[ numop ] = POW;
		d = atof(bu_optarg);
		if (ZERO(d)) {
		    bu_exit(2, "%s: zero root!\n", progname);
		}
		val[ numop++ ] = 1.0 / d;
		break;

	    default:		/* '?' */
		bu_exit(1, "%s", usage);
	}
    }

    if (bu_optind >= argc) {
	if (isatty(fileno(stdin)))
	    return 0;
	infp = stdin;
	setmode(fileno(stdin), O_BINARY);
    } else {
	static char *file_name = NULL;
	file_name = argv[bu_optind];
	infp = fopen(file_name, "rb");
	if (!infp) {
	    fprintf(stderr,
		    "%s: cannot open \"%s\" for reading\n",
		    progname, file_name);
	    return 0;
	}
    }

    if (argc > ++bu_optind)
	fprintf(stderr, "%s: excess argument(s) ignored\n", progname);

    return 1;		/* OK */
}


int
main(int argc, char *argv[])
{
    int i, n;
    double *bp;
    double arg;
    int j;
    size_t ret;

    int infpfd = 1; /* default stdin */
    int soutfd = 0; /* default stdout */

    double buf[BU_PAGE_SIZE] = {0.0};		/* working buffer */

    bu_setprogname(argv[0]);

    if (infp)
	infpfd = fileno(infp);
    if (stdout)
	soutfd = fileno(stdout);

    if (!get_args(argc, argv)
	|| !infp
	|| !stdout
	|| isatty(infpfd)
	|| isatty(soutfd))
    {
	bu_exit(1, "%s", usage);
    }

    setmode(soutfd, O_BINARY);

    while ((n = fread(buf, sizeof(*buf), BU_PAGE_SIZE, infp)) > 0) {
	for (i = 0; i < numop; i++) {
	    arg = val[ i ];
	    switch (op[i]) {
		double d;

		case ADD:
		    bp = &buf[0];
		    for (j = n; j > 0; j--) {
			*bp++ += arg;
		    }
		    break;
		case MULT:
		    bp = &buf[0];
		    for (j = n; j > 0; j--) {
			*bp++ *= arg;
		    }
		    break;
		case POW:
		    bp = &buf[0];
		    for (j = n; j > 0; j--) {
			d = pow(*bp, arg);
			*bp++ = d;
		    }
		    break;
		case ABS:
		    bp = &buf[0];
		    for (j = n; j > 0; j--) {
			if (*bp < 0.0)
			    *bp = - *bp;
			bp++;
		    }
		    break;
		default:
		    break;
	    }
	}
	ret = fwrite(buf, sizeof(*buf), n, stdout);
	if (ret != (size_t)n)
	    perror("fwrite");
    }

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

/*                            F B . C
 * BRL-CAD
 *
 * Copyright (c) 1997-2025 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup libstruct fb */
/** @{ */
/**
 *
 * A framebuffer object contains the attributes and
 * methods for controlling framebuffers.
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "tcl.h"
#include "bu/cmd.h"
#include "bu/color.h"
#include "bu/getopt.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "../libdm/include/private.h"
#include "dm.h"
#include "tclcad.h"
#include "./tclcad_private.h"
#include "./view/view.h"

#define FBO_CONSTRAIN(_v, _a, _b)		\
    ((_v > _a) ? (_v < _b ? _v : _b) : _a)

#define FB_OBJ_LIST_INIT_CAPACITY 8

struct fb_obj {
    struct bu_vls fbo_name;	/* framebuffer object name/cmd */
    struct fbserv_obj fbo_fbs;	/* fbserv object */
    Tcl_Interp *fbo_interp;
};

static struct fb_obj fb_obj_list_init[FB_OBJ_LIST_INIT_CAPACITY];

static struct fb_obj_list {
    size_t capacity, size;
    struct fb_obj *objs;
} fb_objs;


static int
fbo_coords_ok(struct fb *fbp, int x, int y)
{
    int width;
    int height;
    int errors;
    width = fb_getwidth(fbp);
    height = fb_getheight(fbp);

    errors = 0;

    if (x < 0) {
	bu_log("fbo_coords_ok: Error!: X value < 0\n");
	++errors;
    }

    if (y < 0) {
	bu_log("fbo_coords_ok: Error!: Y value < 0\n");
	++errors;
    }

    if (x > width - 1) {
	bu_log("fbo_coords_ok: Error!: X value too large\n");
	++errors;
    }

    if (y > height - 1) {
	bu_log("fbo_coords_ok: Error!: Y value too large\n");
	++errors;
    }

    if (errors) {
	return 0;
    } else {
	return 1;
    }
}


/*
 * Called when the object is destroyed.
 */
static void
fbo_deleteProc(void *clientData)
{
    struct fb_obj *fbop = (struct fb_obj *)clientData;

    /* close framebuffer */
    fb_close(fbop->fbo_fbs.fbs_fbp);

    bu_vls_free(&fbop->fbo_name);
    memmove(fbop,
	    fbop + 1,
	    sizeof (struct fb_obj) * (fb_objs.objs + fb_objs.size - fbop));
	fb_objs.size--;
    bu_free((void *)fbop, "fbo_deleteProc: fbop");
}


/*
 * Close a framebuffer object.
 *
 * Usage:
 * procname close
 */
static int
fbo_close_tcl(void *clientData, int argc, const char **UNUSED(argv))
{
    struct fb_obj *fbop = (struct fb_obj *)clientData;

    if (argc != 2) {
	bu_log("ERROR: expecting two arguments\n");
	return BRLCAD_ERROR;
    }

    /* Among other things, this will call dmo_deleteProc. */
    Tcl_DeleteCommand(fbop->fbo_interp, bu_vls_addr(&fbop->fbo_name));

    return BRLCAD_OK;
}


static int
fbo_tcllist2color(const char *str, unsigned char *pixel)
{
    int r, g, b;

    if (sscanf(str, "%d %d %d", &r, &g, &b) != 3) {
	bu_log("fb_clear: bad color spec - %s", str);
	return BRLCAD_ERROR;
    }

    pixel[RED] = FBO_CONSTRAIN (r, 0, 255);
    pixel[GRN] = FBO_CONSTRAIN (g, 0, 255);
    pixel[BLU] = FBO_CONSTRAIN (b, 0, 255);

    return BRLCAD_OK;
}


/*
 * Clear the framebuffer with the specified color.
 * Otherwise, clear the framebuffer with black.
 *
 * Usage:
 * procname clear [rgb]
 */
static int
fbo_clear_tcl(void *clientData, int argc, const char **argv)
{
    struct fb_obj *fbop = (struct fb_obj *)clientData;
    int status;
    RGBpixel pixel;
    unsigned char *ms;


    if (argc < 2 || 3 < argc) {
	bu_log("ERROR: expecting only two or three arguments\n");
	return BRLCAD_ERROR;
    }

    if (argc == 3) {
	/*
	 * Decompose the color list into its constituents.
	 * For now must be in the form of rrr ggg bbb.
	 */
	if (fbo_tcllist2color(argv[6], pixel) == BRLCAD_ERROR) {
	    bu_log("fb_cell: invalid color spec: %s.", argv[6]);
	    return BRLCAD_ERROR;
	}

	ms = pixel;
    } else
	ms = RGBPIXEL_NULL;

    status = fb_clear(fbop->fbo_fbs.fbs_fbp, ms);

    if (status < 0)
	return BRLCAD_ERROR;

    return BRLCAD_OK;
}


/*
 *
 * Usage:
 * procname cursor mode x y
 */
static int
fbo_cursor_tcl(void *clientData, int argc, const char **argv)
{
    struct fb_obj *fbop = (struct fb_obj *)clientData;
    int mode;
    int x, y;
    int status;

    if (argc != 5) {
	bu_log("ERROR: expecting five arguments\n");
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[2], "%d", &mode) != 1) {
	bu_log("fb_cursor: bad mode - %s", argv[2]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[3], "%d", &x) != 1) {
	bu_log("fb_cursor: bad x value - %s", argv[3]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[4], "%d", &y) != 1) {
	bu_log("fb_cursor: bad y value - %s", argv[4]);
	return BRLCAD_ERROR;
    }

    status = fb_cursor(fbop->fbo_fbs.fbs_fbp, mode, x, y);
    if (status == 0)
	return BRLCAD_OK;

    return BRLCAD_ERROR;
}


/*
 *
 * Usage:
 * procname getcursor
 */
static int
fbo_getcursor_tcl(void *clientData, int argc, const char **argv)
{
    struct fb_obj *fbop = (struct fb_obj *)clientData;
    int status;
    int mode;
    int x, y;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (argc != 2
	|| !BU_STR_EQUIV(argv[1], "getcursor"))
    {
	bu_log("ERROR: unexpected argument(s)\n");
	return BRLCAD_ERROR;
    }

    status = fb_getcursor(fbop->fbo_fbs.fbs_fbp, &mode, &x, &y);
    if (status == 0) {
	bu_vls_printf(&vls, "%d %d %d", mode, x, y);
	Tcl_AppendResult(fbop->fbo_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return BRLCAD_OK;
    }

    return BRLCAD_ERROR;
}


/*
 * Refresh the entire framebuffer or that part specified by
 * a rectangle (i.e. x y width height)
 * Usage:
 * procname refresh [rect]
 */
static int
fbo_refresh_tcl(void *clientData, int argc, const char **argv)
{
    struct fb_obj *fbop = (struct fb_obj *)clientData;
    int x, y, w, h;		       /* rectangle to be refreshed */

    if (argc < 2 || 3 < argc) {
	bu_log("ERROR: expecting only two or three arguments\n");
	return BRLCAD_ERROR;
    }

    if (argc == 2) {
	/* refresh the whole display */
	x = y = 0;
	w = fbop->fbo_fbs.fbs_fbp->i->if_width;
	h = fbop->fbo_fbs.fbs_fbp->i->if_height;
    } else if (sscanf(argv[2], "%d %d %d %d", &x, &y, &w, &h) != 4) {
	/* refresh rectangular area */
	bu_log("fb_refresh: bad rectangle - %s", argv[2]);
	return BRLCAD_ERROR;
    }

    return fb_refresh(fbop->fbo_fbs.fbs_fbp, x, y, w, h);
}


/*
 * Listen for framebuffer clients.
 *
 * Usage:
 * procname listen port
 *
 * Returns the port number actually used.
 *
 */
static int
fbo_listen_tcl(void *clientData, int argc, const char **argv)
{
    struct fb_obj *fbop = (struct fb_obj *)clientData;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (fbop->fbo_fbs.fbs_fbp == FB_NULL) {
	bu_log("%s listen: framebuffer not open!\n", argv[0]);
	return BRLCAD_ERROR;
    }

    if (argc != 2 && argc != 3) {
	bu_log("ERROR: expecting only two or three arguments\n");
	return BRLCAD_ERROR;
    }

    if (argc == 3) {
	int port;

	if (sscanf(argv[2], "%d", &port) != 1) {
	    bu_log("listen: bad value - %s\n", argv[2]);
	    return BRLCAD_ERROR;
	}

	if (port >= 0) {
	    //Set up fbo_fbs callbacks, then call fbs_open
	    fbop->fbo_fbs.fbs_is_listening = &tclcad_is_listening;
	    fbop->fbo_fbs.fbs_listen_on_port = &tclcad_listen_on_port;
	    fbop->fbo_fbs.fbs_open_server_handler = &tclcad_open_server_handler;
	    fbop->fbo_fbs.fbs_close_server_handler = &tclcad_close_server_handler;
	    fbop->fbo_fbs.fbs_open_client_handler = &tclcad_open_client_handler;
	    fbop->fbo_fbs.fbs_close_client_handler = &tclcad_close_client_handler;
	    fbs_open(&fbop->fbo_fbs, port);
	} else {
	    fbs_close(&fbop->fbo_fbs);
	}
	bu_vls_printf(&vls, "%d", fbop->fbo_fbs.fbs_listener.fbsl_port);
	Tcl_AppendResult(fbop->fbo_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return BRLCAD_OK;
    }

    /* return the port number */
    /* argc == 2 */
    bu_vls_printf(&vls, "%d", fbop->fbo_fbs.fbs_listener.fbsl_port);
    Tcl_AppendResult(fbop->fbo_interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    return BRLCAD_OK;
}


/*
 * Set/get the pixel value at position (x, y).
 *
 * Usage:
 * procname pixel x y [rgb]
 */
static int
fbo_pixel_tcl(void *clientData, int argc, const char **argv)
{
    struct fb_obj *fbop = (struct fb_obj *)clientData;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    int x, y; 	/* pixel position */
    RGBpixel pixel;

    if (argc != 4 && argc != 5) {
	bu_log("ERROR: expecting five arguments\n");
	return BRLCAD_ERROR;
    }

    /* get pixel position */
    if (sscanf(argv[2], "%d", &x) != 1) {
	bu_log("fb_pixel: bad x value - %s", argv[2]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[3], "%d", &y) != 1) {
	bu_log("fb_pixel: bad y value - %s", argv[3]);
	return BRLCAD_ERROR;
    }

    /* check pixel position */
    if (!fbo_coords_ok(fbop->fbo_fbs.fbs_fbp, x, y)) {
	bu_log("fb_pixel: coordinates (%s, %s) are invalid.", argv[2], argv[3]);
	return BRLCAD_ERROR;
    }

    /* get pixel value */
    if (argc == 4) {
	fb_rpixel(fbop->fbo_fbs.fbs_fbp, pixel);
	bu_vls_printf(&vls, "%d %d %d", pixel[RED], pixel[GRN], pixel[BLU]);
	Tcl_AppendResult(fbop->fbo_interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return BRLCAD_OK;
    }

    /*
     * Decompose the color list into its constituents.
     * For now must be in the form of rrr ggg bbb.
     */

    /* set pixel value */
    if (fbo_tcllist2color(argv[4], pixel) == BRLCAD_ERROR) {
	bu_log("fb_pixel: invalid color spec - %s", argv[4]);
	return BRLCAD_ERROR;
    }

    fb_write(fbop->fbo_fbs.fbs_fbp, x, y, pixel, 1);

    return BRLCAD_OK;
}


/*
 *
 * Usage:
 * procname cell xmin ymin width height color
 */
static int
fbo_cell_tcl(void *clientData, int argc, const char **argv)
{
    struct fb_obj *fbop = (struct fb_obj *)clientData;
    int xmin, ymin;
    long width;
    long height;
    size_t i;
    RGBpixel pixel;
    unsigned char *pp;


    if (argc != 7) {
	bu_log("ERROR: expecting seven arguments\n");
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[2], "%d", &xmin) != 1) {
	bu_log("fb_cell: bad xmin value - %s", argv[2]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[3], "%d", &ymin) != 1) {
	bu_log("fb_cell: bad ymin value - %s", argv[3]);
	return BRLCAD_ERROR;
    }

    /* check coordinates */
    if (!fbo_coords_ok(fbop->fbo_fbs.fbs_fbp, xmin, ymin)) {
	bu_log("fb_cell: coordinates (%s, %s) are invalid.", argv[2], argv[3]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[4], "%ld", &width) != 1) {
	bu_log("fb_cell: bad width - %s", argv[4]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[5], "%ld", &height) != 1) {
	bu_log("fb_cell: bad height - %s", argv[5]);
	return BRLCAD_ERROR;
    }


    /* check width and height */
    if (width <=0  || height <=0) {
	bu_log("fb_cell: width and height must be > 0");
	return BRLCAD_ERROR;
    }

    /*
     * Decompose the color list into its constituents.
     * For now must be in the form of rrr ggg bbb.
     */
    if (fbo_tcllist2color(argv[6], pixel) == BRLCAD_ERROR) {
	bu_log("fb_cell: invalid color spec: %s", argv[6]);
	return BRLCAD_ERROR;
    }

    pp = (unsigned char *)bu_calloc(width*height, sizeof(RGBpixel), "allocate pixel array");
    for (i = 0; i < width*height*sizeof(RGBpixel); i+=sizeof(RGBpixel)) {
	pp[i] = pixel[0];
	pp[i+1] = pixel[1];
	pp[i+2] = pixel[2];
    }
    fb_writerect(fbop->fbo_fbs.fbs_fbp, xmin, ymin, width, height, pp);
    bu_free((void *)pp, "free pixel array");

    return BRLCAD_OK;
}


/*
 *
 * Usage:
 * procname flush
 */
static int
fbo_flush_tcl(void *clientData, int argc, const char **argv)
{
    struct fb_obj *fbop = (struct fb_obj *)clientData;

    if (argc != 2
	|| !BU_STR_EQUIV(argv[1], "flush"))
    {
	bu_log("ERROR: expecting two arguments\n");
	return BRLCAD_ERROR;
    }

    fb_flush(fbop->fbo_fbs.fbs_fbp);

    return BRLCAD_OK;
}


/*
 *
 * Usage:
 * procname getheight
 */
static int
fbo_getheight_tcl(void *clientData, int argc, const char **argv)
{
    struct fb_obj *fbop = (struct fb_obj *)clientData;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (argc != 2
	|| !BU_STR_EQUIV(argv[1], "getheight"))
    {
	bu_log("ERROR: expecting two arguments\n");
	return BRLCAD_ERROR;
    }

    bu_vls_printf(&vls, "%d", fb_getheight(fbop->fbo_fbs.fbs_fbp));
    Tcl_AppendResult(fbop->fbo_interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    return BRLCAD_OK;
}


/*
 *
 * Usage:
 * procname getwidth
 */
static int
fbo_getwidth_tcl(void *clientData, int argc, const char **argv)
{
    struct fb_obj *fbop = (struct fb_obj *)clientData;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (argc != 2
	|| !BU_STR_EQUIV(argv[1], "getwidth"))
    {
	bu_log("ERROR: expecting two arguments\n");
	return BRLCAD_ERROR;
    }

    bu_vls_printf(&vls, "%d", fb_getwidth(fbop->fbo_fbs.fbs_fbp));
    Tcl_AppendResult(fbop->fbo_interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    return BRLCAD_OK;
}


/*
 *
 * Usage:
 * procname getsize
 */
static int
fbo_getsize_tcl(void *clientData, int argc, const char **argv)
{
    struct fb_obj *fbop = (struct fb_obj *)clientData;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    if (argc != 2
	|| !BU_STR_EQUIV(argv[1], "getsize"))
    {
	bu_log("ERROR: expecting two arguments\n");
	return BRLCAD_ERROR;
    }

    bu_vls_printf(&vls, "%d %d",
		  fb_getwidth(fbop->fbo_fbs.fbs_fbp),
		  fb_getheight(fbop->fbo_fbs.fbs_fbp));
    Tcl_AppendResult(fbop->fbo_interp, bu_vls_addr(&vls), (char *)NULL);
    bu_vls_free(&vls);

    return BRLCAD_OK;
}


/*
 *
 * Usage:
 * procname cell xmin ymin width height color
 */
static int
fbo_rect_tcl(void *clientData, int argc, const char **argv)
{
    struct fb_obj *fbop = (struct fb_obj *)clientData;
    int xmin, ymin;
    int xmax, ymax;
    int width;
    int height;
    int i;
    RGBpixel pixel;

    if (argc != 7) {
	bu_log("ERROR: expecting seven arguments\n");
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[2], "%d", &xmin) != 1) {
	bu_log("fb_rect: bad xmin value - %s", argv[2]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[3], "%d", &ymin) != 1) {
	bu_log("fb_rect: bad ymin value - %s", argv[3]);
	return BRLCAD_ERROR;
    }

    /* check coordinates */
    if (!fbo_coords_ok(fbop->fbo_fbs.fbs_fbp, xmin, ymin)) {
	bu_log("fb_rect: coordinates (%s, %s) are invalid.", argv[2], argv[3]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[4], "%d", &width) != 1) {
	bu_log("fb_rect: bad width - %s", argv[4]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[5], "%d", &height) != 1) {
	bu_log("fb_rect: bad height - %s", argv[5]);
	return BRLCAD_ERROR;
    }


    /* check width and height */
    if (width <=0  || height <=0) {
	bu_log("fb_rect: width and height must be > 0");
	return BRLCAD_ERROR;
    }

    /*
     * Decompose the color list into its constituents.
     * For now must be in the form of rrr ggg bbb.
     */
    if (fbo_tcllist2color(argv[6], pixel) == BRLCAD_ERROR) {
	bu_log("fb_rect: invalid color spec: %s", argv[6]);
	return BRLCAD_ERROR;
    }

    xmax = xmin + width;
    ymax = ymin + height;

    /* draw horizontal lines */
    for (i = xmin; i <= xmax; ++i) {
	/* working on bottom line */
	fb_write(fbop->fbo_fbs.fbs_fbp, i, ymin, pixel, 1);

	/* working on top line */
	fb_write(fbop->fbo_fbs.fbs_fbp, i, ymax, pixel, 1);
    }

    /* draw vertical lines */
    for (i = ymin; i <= ymax; ++i) {
	/* working on left line */
	fb_write(fbop->fbo_fbs.fbs_fbp, xmin, i, pixel, 1);

	/* working on right line */
	fb_write(fbop->fbo_fbs.fbs_fbp, xmax, i, pixel, 1);
    }

    return BRLCAD_OK;
}


/*
 * Usage:
 * procname configure width height
 */
static int
fbo_configure_tcl(void *clientData, int argc, const char **argv)
{
    struct fb_obj *fbop = (struct fb_obj *)clientData;
    int width, height;

    if (argc != 4) {
	bu_log("ERROR: expecting four arguments\n");
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[2], "%d", &width) != 1) {
	bu_log("fb_configure: bad width - %s", argv[2]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[3], "%d", &height) != 1) {
	bu_log("fb_configure: bad height - %s", argv[3]);
	return BRLCAD_ERROR;
    }

    /* configure the framebuffer window */
    if (fbop->fbo_fbs.fbs_fbp != FB_NULL)
	(void)fb_configure_window(fbop->fbo_fbs.fbs_fbp, width, height);

    return BRLCAD_OK;
}


/*
 * Generic interface for framebuffer object routines.
 * Usage:
 * procname cmd ?args?
 *
 * Returns: result of FB command.
 */
static int
fbo_cmd(ClientData clientData, Tcl_Interp *UNUSED(interp), int argc, const char **argv)
{
    int ret;

    static struct bu_cmdtab fbo_cmds[] = {
	{"cell",	fbo_cell_tcl},
	{"clear",	fbo_clear_tcl},
	{"close",	fbo_close_tcl},
	{"configure",	fbo_configure_tcl},
	{"cursor",	fbo_cursor_tcl},
	{"pixel",	fbo_pixel_tcl},
	{"flush",	fbo_flush_tcl},
	{"getcursor",	fbo_getcursor_tcl},
	{"getheight",	fbo_getheight_tcl},
	{"getsize",	fbo_getsize_tcl},
	{"getwidth",	fbo_getwidth_tcl},
	{"listen",	fbo_listen_tcl},
	{"rect",	fbo_rect_tcl},
	{"refresh",	fbo_refresh_tcl},
	{(const char *)NULL, BU_CMD_NULL}
    };

    if (bu_cmd(fbo_cmds, argc, argv, 1, clientData, &ret) == BRLCAD_OK)
	return ret;

    bu_log("ERROR: '%s' command not found\n", argv[1]);
    return BRLCAD_ERROR;
}


/*
 * Open/create a framebuffer object.
 *
 * Usage:
 * fb_open [name device [args]]
 */
static int
fbo_open_tcl(void *UNUSED(clientData), Tcl_Interp *interp, int argc, const char **argv)
{
    struct fb_obj *fbop;
    struct fb *ifp;
    int width = 512;
    int height = 512;
    register int c;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    size_t i;

    if (argc == 1) {
	/* get list of framebuffer objects */
	for (i = 0; i < fb_objs.size; i++) {
	    fbop = &fb_objs.objs[fb_objs.size];
	    Tcl_AppendResult(interp, bu_vls_addr(&fbop->fbo_name), " ", (char *)NULL);
	}

	return BRLCAD_OK;
    }

    if (argc < 3) {
	bu_vls_printf(&vls, "helplib fb_open");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return BRLCAD_ERROR;
    }

    /* process args */
    bu_optind = 3;
    bu_opterr = 0;
    while ((c = bu_getopt(argc, (char * const *)argv, "w:W:s:S:n:N:")) != -1) {
	switch (c) {
	    case 'W':
	    case 'w':
		width = atoi(bu_optarg);
		break;
	    case 'N':
	    case 'n':
		height = atoi(bu_optarg);
		break;
	    case 'S':
	    case 's':
		width = atoi(bu_optarg);
		height = width;
		break;
	    case '?':
	    default:
		bu_log("fb_open: bad option - %s", bu_optarg);
		return BRLCAD_ERROR;
	}
    }

    if ((ifp = fb_open(argv[2], width, height)) == FB_NULL) {
	bu_log("fb_open: bad device - %s", argv[2]);
	return BRLCAD_ERROR;
    }

    if (fb_ioinit(ifp) != 0) {
	bu_log("fb_open: fb_ioinit() failed.");
	return BRLCAD_ERROR;
    }

    if (fb_objs.capacity == 0) {
	fb_objs.capacity = FB_OBJ_LIST_INIT_CAPACITY;
	fb_objs.objs = fb_obj_list_init;
    } else if (fb_objs.size == fb_objs.capacity && fb_objs.capacity == FB_OBJ_LIST_INIT_CAPACITY) {
	fb_objs.capacity *= 2;
	fb_objs.objs = (struct fb_obj *)bu_malloc(
		sizeof (struct fb_obj) * fb_objs.capacity,
		"first resize of fb_obj list");
    } else if (fb_objs.size == fb_objs.capacity) {
	fb_objs.capacity *= 2;
	fb_objs.objs = (struct fb_obj *)bu_realloc(
		fb_objs.objs,
		sizeof (struct fb_obj) * fb_objs.capacity,
		"additional resize of fb_obj list");
    }

    /* append to list of fb_obj's */
    fbop = &fb_objs.objs[fb_objs.size];
    bu_vls_init(&fbop->fbo_name);
    bu_vls_strcpy(&fbop->fbo_name, argv[1]);
    fbop->fbo_fbs.fbs_fbp = ifp;
    fbop->fbo_fbs.fbs_listener.fbsl_fbsp = &fbop->fbo_fbs;
    fbop->fbo_fbs.fbs_listener.fbsl_fd = -1;
    fbop->fbo_fbs.fbs_listener.fbsl_port = -1;
    fbop->fbo_interp = interp;

    fb_objs.size++;

    (void)Tcl_CreateCommand(interp,
			    bu_vls_addr(&fbop->fbo_name),
			    (Tcl_CmdProc *)fbo_cmd,
			    (ClientData)fbop,
			    fbo_deleteProc);

    /* Return new function name as result */
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, bu_vls_addr(&fbop->fbo_name), (char *)NULL);
    return BRLCAD_OK;
}


int
Fbo_Init(Tcl_Interp *interp)
{
    if (fb_objs.capacity == 0) {
	fb_objs.capacity = FB_OBJ_LIST_INIT_CAPACITY;
	fb_objs.objs = fb_obj_list_init;
    }

    (void)Tcl_CreateCommand(interp, "fb_open", (Tcl_CmdProc *)fbo_open_tcl,
			    (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

    return BRLCAD_OK;
}



void
to_fbs_callback(void *clientData)
{
    struct bview *gdvp = (struct bview *)clientData;

    to_refresh_view(gdvp);
}


int
to_close_fbs(struct bview *gdvp)
{
    struct tclcad_view_data *tvd = (struct tclcad_view_data *)gdvp->u_data;
    if (tvd->gdv_fbs.fbs_fbp == FB_NULL)
	return TCL_OK;

    fb_flush(tvd->gdv_fbs.fbs_fbp);
    fb_close_existing(tvd->gdv_fbs.fbs_fbp);
    tvd->gdv_fbs.fbs_fbp = FB_NULL;

    return TCL_OK;
}


/*
 * Open/activate the display managers framebuffer.
 */
int
to_open_fbs(struct bview *gdvp, Tcl_Interp *interp)
{
    /* already open */
    struct tclcad_view_data *tvd = (struct tclcad_view_data *)gdvp->u_data;
    if (tvd->gdv_fbs.fbs_fbp != FB_NULL)
	return TCL_OK;

    tvd->gdv_fbs.fbs_fbp = dm_get_fb((struct dm *)gdvp->dmp);

    if (tvd->gdv_fbs.fbs_fbp == FB_NULL) {
	Tcl_Obj *obj;

	obj = Tcl_GetObjResult(interp);
	if (Tcl_IsShared(obj))
	    obj = Tcl_DuplicateObj(obj);

	Tcl_AppendStringsToObj(obj, "openfb: failed to allocate framebuffer memory\n", (char *)NULL);

	Tcl_SetObjResult(interp, obj);
	return TCL_ERROR;
    }

    return TCL_OK;
}



int
to_set_fb_mode(struct ged *gedp,
	       int argc,
	       const char *argv[],
	       ged_func_ptr UNUSED(func),
	       const char *usage,
	       int UNUSED(maxargs))
{
    int mode;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (3 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    struct bview *gdvp = bv_set_find_view(&gedp->ged_views, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    /* Get fb mode */
    if (argc == 2) {
	struct tclcad_view_data *tvd = (struct tclcad_view_data *)gdvp->u_data;
	bu_vls_printf(gedp->ged_result_str, "%d", tvd->gdv_fbs.fbs_mode);
	return BRLCAD_OK;
    }

    /* Set fb mode */
    if (bu_sscanf(argv[2], "%d", &mode) != 1) {
	bu_vls_printf(gedp->ged_result_str, "set_fb_mode: bad value - %s\n", argv[2]);
	return BRLCAD_ERROR;
    }

    if (mode < 0)
	mode = 0;
    else if (TCLCAD_OBJ_FB_MODE_OVERLAY < mode)
	mode = TCLCAD_OBJ_FB_MODE_OVERLAY;

    {
	struct tclcad_view_data *tvd = (struct tclcad_view_data *)gdvp->u_data;
	tvd->gdv_fbs.fbs_mode = mode;
    }
    to_refresh_view(gdvp);

    return BRLCAD_OK;
}


int
to_listen(struct ged *gedp,
	  int argc,
	  const char *argv[],
	  ged_func_ptr UNUSED(func),
	  const char *usage,
	  int UNUSED(maxargs))
{
    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (3 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    struct bview *gdvp = bv_set_find_view(&gedp->ged_views, argv[1]);
    if (!gdvp) {
	bu_vls_printf(gedp->ged_result_str, "View not found - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    struct tclcad_view_data *tvd = (struct tclcad_view_data *)gdvp->u_data;
    if (tvd->gdv_fbs.fbs_fbp == FB_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s listen: framebuffer not open!\n", argv[0]);
	return BRLCAD_ERROR;
    }

    /* return the port number */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "%d", tvd->gdv_fbs.fbs_listener.fbsl_port);
	return BRLCAD_OK;
    }

    if (argc == 3) {
	int port;

	if (bu_sscanf(argv[2], "%d", &port) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "listen: bad value - %s\n", argv[2]);
	    return BRLCAD_ERROR;
	}

	if (port >= 0) {
	    // Set up fbo_fbs callbacks, then call fbs_open
	    tvd->gdv_fbs.fbs_is_listening = &tclcad_is_listening;
	    tvd->gdv_fbs.fbs_listen_on_port = &tclcad_listen_on_port;
	    tvd->gdv_fbs.fbs_open_server_handler = &tclcad_open_server_handler;
	    tvd->gdv_fbs.fbs_close_server_handler = &tclcad_close_server_handler;
	    tvd->gdv_fbs.fbs_open_client_handler = &tclcad_open_client_handler;
	    tvd->gdv_fbs.fbs_close_client_handler = &tclcad_close_client_handler;
	    fbs_open(&tvd->gdv_fbs, port);
	} else {
	    fbs_close(&tvd->gdv_fbs);
	}
	bu_vls_printf(gedp->ged_result_str, "%d", tvd->gdv_fbs.fbs_listener.fbsl_port);
	return BRLCAD_OK;
    }

    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return BRLCAD_ERROR;
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

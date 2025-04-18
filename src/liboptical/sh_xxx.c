/*                        S H _ X X X . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2025 United States Government as represented by
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
/** @file liboptical/sh_xxx.c
 *
 * To add a new shader to the "rt" program's LIBOPTICAL library:
 *
 * 1) Copy this file to sh_shadername.c
 *
 * 2) edit sh_shadername.c:
 *	change "X X X" to "S H A D E R N A M E"
 *	change "xxx" to "shadername"
 *	Set a new number for the xxx_MAGIC define
 *	define shader specific structure and defaults
 *	edit/build parse table for bu_structparse from xxx_parse
 *	edit/build shader_mfuncs tables from xxx_mfuncs for
 *		each shader name being built.
 *	edit the xxx_setup function to do shader-specific setup
 *	edit the xxx_render function to do the actual rendering
 *
 * If you are building a dynamically loaded shader, compile this into a
 * shared library called "shadername.so" or "libshadername.so".
 *
 * RT will look in the following locations for DSO shaders:
 *		./
 *		$prefix/lib/
 *		$LD_LIBRARY_PATH
 *
 * If you are adding the shader to "rt" as a permanent shader, then the
 * following steps are necessary:
 *
 * 3) Edit init.c to add extern for xxx_mfuncs and a call to
 * mlib_add_shader().
 *
 * 4) Edit CMakeLists.txt to add shader file to the compilation
 *
 * 5) replace this list with a description of the shader, its
 * parameters and use.
 *
 * 6) Edit shaders.tcl and comb.tcl in the ../tclscripts/mged
 * directory to add a new gui for this shader.
 */

#include "common.h"

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "bu/units.h"
#include "vmath.h"
#include "raytrace.h"
#include "optical.h"


#define xxx_MAGIC 0x1834    /* make this a unique number for each shader */
#define CK_xxx_SP(_p) BU_CKMAG(_p, xxx_MAGIC, "xxx_specific")

/*
 * the shader specific structure contains all variables which are unique
 * to any particular use of the shader.
 */
struct xxx_specific {
    uint32_t magic;	/* magic # for memory validity check, must come 1st */
    double xxx_val;	/* variables for shader ... */
    double xxx_dist;
    vect_t xxx_delta;
    point_t xxx_min;
    point_t xxx_max;
    mat_t xxx_m_to_sh;	/* model to shader space matrix */
    mat_t xxx_m_to_r;	/* model to shader space matrix */
};


/* The default values for the variables in the shader specific structure */
static const
struct xxx_specific xxx_defaults = {
    xxx_MAGIC,
    1.0,				/* xxx_val */
    0.0,				/* xxx_dist */
    VINITALL(1.0),		/* xxx_delta */
    VINIT_ZERO,			/* xxx_min */
    VINIT_ZERO,			/* xxx_max */
    MAT_INIT_ZERO,		/* xxx_m_to_sh */
    MAT_INIT_ZERO		/* xxx_m_to_r */
};


#define SHDR_NULL ((struct xxx_specific *)0)
#define SHDR_O(m) bu_offsetof(struct xxx_specific, m)


/* description of how to parse/print the arguments to the shader
 * There is at least one line here for each variable in the shader specific
 * structure above
 */
struct bu_structparse xxx_print_tab[] = {
    {"%g", 1, "val",		SHDR_O(xxx_val),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%g", 1, "dist",		SHDR_O(xxx_dist),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f", 3, "delta",		SHDR_O(xxx_delta),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f", 3, "max",		SHDR_O(xxx_max),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f", 3, "min",		SHDR_O(xxx_min),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",   0, (char *)0,	0,			BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }

};
struct bu_structparse xxx_parse_tab[] = {
    {"%p", 1, "xxx_print_tab", bu_byteoffset(xxx_print_tab[0]), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%g", 1, "v",		SHDR_O(xxx_val),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%g", 1, "dist",		SHDR_O(xxx_dist),	bu_mm_cvt, NULL, NULL },
    {"%f", 3, "d",		SHDR_O(xxx_delta),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",   0, (char *)0,	0,			BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


static int xxx_setup(register struct region *rp, struct bu_vls *matparm, void **dpp, const struct mfuncs *mfp, struct rt_i *rtip);
static int xxx_render(struct application *ap, const struct partition *pp, struct shadework *swp, void *dp);
static void xxx_print(register struct region *rp, void *dp);
static void xxx_free(void *cp);

/* The "mfuncs" structure defines the external interface to the shader.
 * Note that more than one shader "name" can be associated with a given
 * shader by defining more than one mfuncs struct in this array.
 * See sh_phong.c for an example of building more than one shader "name"
 * from a set of source functions.  There you will find that "glass" "mirror"
 * and "plastic" are all names for the same shader with different default
 * values for the parameters.
 */
struct mfuncs xxx_mfuncs[] = {
    {MF_MAGIC,	"xxx",	0,	MFI_NORMAL|MFI_HIT|MFI_UV,	0,     xxx_setup,	xxx_render,	xxx_print,	xxx_free },
    {0,		(char *)0,	0,		0,		0,     0,		0,		0,		0 }
};


/*
 * This routine is called (at prep time)
 * once for each region which uses this shader.
 * Any shader-specific initialization should be done here.
 *
 * Returns:
 * 1 success
 * 0 success, but delete region
 * -1 failure
 */
static int
xxx_setup(register struct region *rp, struct bu_vls *matparm, void **dpp, const struct mfuncs *UNUSED(mfp), struct rt_i *rtip)
/* pointer to reg_udata in *rp */
/* New since 4.4 release */
{
    register struct xxx_specific *xxx_sp;

    /* check the arguments */
    RT_CHECK_RTI(rtip);
    BU_CK_VLS(matparm);
    RT_CK_REGION(rp);


    if (optical_debug&OPTICAL_DEBUG_SHADE)
	bu_log("xxx_setup(%s)\n", rp->reg_name);

    /* Get memory for the shader parameters and shader-specific data */
    BU_GET(xxx_sp, struct xxx_specific);
    *dpp = xxx_sp;

    /* initialize the default values for the shader */
    memcpy(xxx_sp, &xxx_defaults, sizeof(struct xxx_specific));

    /* parse the user's arguments for this use of the shader. */
    if (bu_struct_parse(matparm, xxx_parse_tab, (char *)xxx_sp, NULL) < 0)
	return -1;

    /* Optional:
     *
     * If the shader needs to operate in a coordinate system which stays
     * fixed on the region when the region is moved (as in animation)
     * we need to get a matrix to perform the appropriate transform(s).
     *
     * rt_shader_mat() returns a matrix which maps points on/in the
     * region into the unit cube.  This unit cube is formed by first
     * mapping from world coordinates into "region coordinates" (the
     * coordinate system in which the region is defined).  Then the
     * bounding box of the region is used to establish a mapping to
     * the unit cube
     *
     * rt_shader_mat(xxx_sp->xxx_m_to_sh, rtip, rp, xxx_sp->xxx_min,
     * xxx_sp->xxx_max);
     *
     * Alternatively, shading may be done in "region coordinates"
     * if desired:
     *
     * db_region_mat(xxx_sp->xxx_m_to_r, rtip->rti_dbip, rp->reg_name, &rt_uniresource);
     *
     */

    if (optical_debug&OPTICAL_DEBUG_SHADE) {
	bu_struct_print(" Parameters:", xxx_print_tab, (char *)xxx_sp);
	bn_mat_print("m_to_sh", xxx_sp->xxx_m_to_sh);
    }

    return 1;
}


static void
xxx_print(register struct region *rp, void *dp)
{
    bu_struct_print(rp->reg_name, xxx_print_tab, (char *)dp);
}


static void
xxx_free(void *cp)
{
    BU_PUT(cp, struct xxx_specific);
}


/*
 * This is called (from viewshade() in shade.c) once for each hit point
 * to be shaded.  The purpose here is to fill in values in the shadework
 * structure.
 */
int
xxx_render(struct application *ap, const struct partition *pp, struct shadework *swp, void *dp)
/* defined in ../h/shadework.h */
/* ptr to the shader-specific struct */
{
    register struct xxx_specific *xxx_sp =
	(struct xxx_specific *)dp;
    point_t pt;
    VSETALL(pt, 0);

    /* check the validity of the arguments we got */
    RT_AP_CHECK(ap);
    RT_CHECK_PT(pp);
    CK_xxx_SP(xxx_sp);

    if (optical_debug&OPTICAL_DEBUG_SHADE)
	bu_struct_print("xxx_render Parameters:", xxx_print_tab, (char *)xxx_sp);

    /* If we are performing the shading in "region" space, we must
     * transform the hit point from "model" space to "region" space.
     * See the call to db_region_mat in xxx_setup().
     MAT4X3PNT(pt, xxx_sp->xxx_m_to_sh, swp->sw_hit.hit_point);
     MAT4X3PNT(pt, xxx_sp->xxx_m_to_r, swp->sw_hit.hit_point);

     if (optical_debug&OPTICAL_DEBUG_SHADE) {
     bu_log("xxx_render() model:(%g %g %g) shader:(%g %g %g)\n",
     V3ARGS(swp->sw_hit.hit_point),
     V3ARGS(pt));
     }
    */

    /* XXX perform shading operations here */
    VMOVE(swp->sw_color, pt);

    /* shader must perform transmission/reflection calculations
     *
     * 0 < swp->sw_transmit <= 1 causes transmission computations
     * 0 < swp->sw_reflect <= 1 causes reflection computations
     */
    if (swp->sw_reflect > 0 || swp->sw_transmit > 0)
	(void)rr_render(ap, pp, swp);

    return 1;
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

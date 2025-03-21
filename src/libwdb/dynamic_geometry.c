/*              D Y N A M I C _ G E O M E T R Y . C
 * BRL-CAD
 *
 * Copyright (c) 2003-2025 United States Government as represented by
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
/** @file libwdb/dynamic_geometry.c
 *
 * Library for dynamically changing BRL-CAD geometry (i.e., changing
 * already prepped geometry)
 *
 * The typical use of dynamic geometry involves these steps:
 *
 * 1. Normal use of " db_dirbuild", "gettrees", and "prep".
 *
 * 2. Call "wdb_dbopen" to get a "struct rt_wdb" pointer for the
 * database (typically using the RT_WDB_TYPE_DB_INMEM flag)
 *
 * 3. Create and fill an "rt_reprep_obj_list" structure.
 *
 * 4. Call "rt_unprep" and pass it the "rt_reprep_obj_list" structure.
 *
 * 5. Modify geometry (note, you may only modify geometry that you
 * listed in the "rt_reprep_obj_list" as "unprepped"). You may add,
 * remove, modify, translate, rotate, scale objects under the
 * "unprepped" combinations. (Use the "make_hole" routine here).
 *
 * 6. Call "rt_reprep" with same parameters as you called "rt_unprep".
 *
 * 7. If you are using liboptical, call "view_re_setup" to setup
 * shaders for regions involved.
 *
 * 8. Do raytracing.
 *
 * If you only need to make holes, a simpler and quicker approach is
 * to use the "make_hole_in_prepped_regions" routine:
 *
 * Steps 1 and 2 are the same as above, then 3. Call "make_hole" and
 * 4. Do raytracing.
 */

#include "common.h"

#include <math.h>
#include <string.h>
#include "bio.h"

#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "wdb.h"


int
make_hole(struct rt_wdb *wdbp,		/* database to be modified */
	  point_t hole_start,		/* center of start of hole */
	  vect_t hole_depth,		/* depth and direction of hole */
	  fastf_t hole_radius,		/* radius of hole */
	  int num_objs,			/* number of objects that this hole affects */
	  struct directory **dp)	/* array of directory pointers
					 * [num_objs] of objects to
					 * get this hole applied
					 */
{
    struct bu_vls tmp_name = BU_VLS_INIT_ZERO;
    int i, base_len, count=0;

    RT_CK_WDB(wdbp);

    /* make sure we are only making holes in combinations, they do not
     * have to be regions
     */
    for (i=0; i<num_objs; i++) {
	RT_CK_DIR(dp[i]);
	if (!(dp[i]->d_flags & RT_DIR_COMB)) {
	    bu_log("make_hole(): can only make holes in combinations\n");
	    bu_log("\t%s is not a combination\n", dp[i]->d_namep);
	    return 4;
	}
    }

    /* make a unique name for the RCC we will use (of the form
     * "make_hole_%d")
     */
    bu_vls_strcat(&tmp_name, "make_hole_");
    base_len = bu_vls_strlen(&tmp_name);
    bu_vls_strcat(&tmp_name, "0");
    while ((db_lookup(wdbp->dbip, bu_vls_addr(&tmp_name), LOOKUP_QUIET)) != RT_DIR_NULL) {
	count++;
	bu_vls_trunc(&tmp_name, base_len);
	bu_vls_printf(&tmp_name, "%d", count);
    }

    /* build the RCC based on parameters passed in */
    if (mk_rcc(wdbp, bu_vls_addr(&tmp_name), hole_start, hole_depth, hole_radius)) {
	bu_log("Failed to create hole cylinder!!!\n");
	bu_vls_free(&tmp_name);
	return 2;
    }

    /* subtract this RCC from each combination in the list passed in */
    for (i=0; i<num_objs; i++) {
	struct rt_db_internal intern;
	struct rt_comb_internal *comb;
	union tree *tree;

	/* get the internal form of the combination */
	if (rt_db_get_internal(&intern, dp[i], wdbp->dbip, NULL, wdbp->wdb_resp) < 0) {
	    bu_log("Failed to get %s\n", dp[i]->d_namep);
	    bu_vls_free(&tmp_name);
	    return 3;
	}
	comb = (struct rt_comb_internal *)intern.idb_ptr;

	/* Build a new "subtract" node (will be the root of the new tree) */
	BU_ALLOC(tree, union tree);
	RT_TREE_INIT(tree);
	tree->tr_b.tb_op = OP_SUBTRACT;
	tree->tr_b.tb_left = comb->tree;	/* subtract from the original tree */
	comb->tree = tree;

	/* Build a node for the RCC to be subtracted */
	BU_ALLOC(tree, union tree);
	RT_TREE_INIT(tree);
	tree->tr_l.tl_op = OP_DB_LEAF;
	tree->tr_l.tl_mat = NULL;
	tree->tr_l.tl_name = bu_strdup(bu_vls_addr(&tmp_name)); /* copy name of RCC */

	/* Put the RCC node to the right of the root */
	comb->tree->tr_b.tb_right = tree;

	/* Save the modified combination.  This will overwrite the
	 * original combination if wdbp was opened with the
	 * RT_WDB_TYPE_DB_DISK flag. If wdbp was opened with the
	 * RT_WDB_TYPE_DB_INMEM flag, then the combination will be
	 * temporarily over-written in memory only and the disk file
	 * will not be modified.
	 */
	wdb_put_internal(wdbp, dp[i]->d_namep, &intern, 1.0);
    }
    return 0;
}


int
make_hole_in_prepped_regions(struct rt_wdb *wdbp,	/* database to be modified */
			     struct rt_i *rtip,		/* rt_i pointer for the same database */
			     point_t hole_start,	/* center of start of hole */
			     vect_t hole_depth,		/* depth and direction of hole */
			     fastf_t radius,		/* radius of hole */
			     struct bu_ptbl *regions)	/* list of region structures to which this hole
							 * is to be applied
							 */
{
    struct bu_vls tmp_name = BU_VLS_INIT_ZERO;
    size_t i, base_len, count=0;
    struct directory *dp;
    struct rt_db_internal intern;
    struct soltab *stp;

    RT_CHECK_WDB(wdbp);

    /* make a unique name for the RCC we will use (of the form "make_hole_%d") */
    bu_vls_strcat(&tmp_name, "make_hole_");
    base_len = bu_vls_strlen(&tmp_name);
    bu_vls_strcat(&tmp_name, "0");
    while ((db_lookup(wdbp->dbip, bu_vls_addr(&tmp_name), LOOKUP_QUIET)) != RT_DIR_NULL) {
	count++;
	bu_vls_trunc(&tmp_name, base_len);
	bu_vls_printf(&tmp_name, "%zu", count);
    }

    /* build the RCC based on parameters passed in */
    if (mk_rcc(wdbp, bu_vls_addr(&tmp_name), hole_start, hole_depth, radius)) {
	bu_log("Failed to create hole cylinder!!!\n");
	bu_vls_free(&tmp_name);
	return 2;
    }

    /* lookup the newly created RCC */
    dp=db_lookup(wdbp->dbip, bu_vls_addr(&tmp_name), LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
      bu_log("Failed to lookup RCC (%s) just made by make_hole_in_prepped_regions()!!!\n",
	       bu_vls_addr(&tmp_name));
	bu_bomb("Failed to lookup RCC just made by make_hole_in_prepped_regions()!!!\n");
    }

    /* get the internal form of the new RCC */
    if (rt_db_get_internal(&intern, dp, wdbp->dbip, NULL, wdbp->wdb_resp) < 0) {
	bu_log("Failed to get internal form of RCC (%s) just made by make_hole_in_prepped_regions()!!!\n",
	       bu_vls_addr(&tmp_name));
	bu_bomb("Failed to get internal form of RCC just made by make_hole_in_prepped_regions()!!!\n");
    }

    /* Build a soltab structure for the new RCC */
    BU_ALLOC(stp, struct soltab);
    stp->l.magic = RT_SOLTAB_MAGIC;
    stp->l2.magic = RT_SOLTAB2_MAGIC;
    stp->st_uses = 1;
    stp->st_dp = dp;
    stp->st_bit = rtip->nsolids++;

    /* Add the new soltab structure to the rt_i structure */
    rtip->rti_Solids = (struct soltab **)bu_realloc(rtip->rti_Solids,
						    rtip->nsolids * sizeof(struct soltab *),
						    "new rti_Solids");
    rtip->rti_Solids[stp->st_bit] = stp;

    /* actually prep the new RCC */
    if (intern.idb_meth->ft_prep(stp, &intern, rtip)) {
	bu_log("Failed to prep RCC (%s) just made by make_hole_in_prepped_regions()!!!\n",
	       bu_vls_addr(&tmp_name));
	bu_bomb("Failed to prep RCC just made by make_hole_in_prepped_regions()!!!\n");
    }

    /* initialize the soltabs list of containing regions */
    bu_ptbl_init(&stp->st_regions, BU_PTBL_LEN(regions), "stp->st_regions");

    /* Subtract the new RCC from each region structure in the list provided */
    for (i=0; i<BU_PTBL_LEN(regions); i++) {
	struct region *rp;
	union tree *treep;

	/* get the next region structure */
	rp = (struct region *)BU_PTBL_GET(regions, i);

	RT_CK_REGION(rp);

	/* create a tree node for the subtraction operation, this will be the new tree root */
	BU_ALLOC(treep, union tree);
	RT_TREE_INIT(treep);
	treep->tr_b.tb_op = OP_SUBTRACT;
	treep->tr_b.tb_left = rp->reg_treetop;	/* subtract from the old treetop */
	treep->tr_b.tb_regionp = rp;

	/* make the new node the new treetop */
	rp->reg_treetop = treep;

	/* create a tree node for the new RCC */
	BU_ALLOC(treep, union tree);
	RT_TREE_INIT(treep);
	treep->tr_a.tu_op = OP_SOLID;
	treep->tr_a.tu_stp = stp;
	treep->tr_a.tu_regionp = rp;

	/* the new RCC gets hung on the right of the subtract node */
	rp->reg_treetop->tr_b.tb_right = treep;

	/* make sure the "all unions" flag is not set on this region */
	rp->reg_all_unions = 0;

	/* Add this region to the list of containing regions for the new RCC */
	bu_ptbl_ins(&stp->st_regions, (long *)rp);
    }

    /* insert the new RCC soltab structure into the already existing space partitioning tree */
    insert_in_bsp(stp, &rtip->rti_CutHead);

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

/*                         W C O D E S . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
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
/** @file libged/wcodes.c
 *
 * The wcodes command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "../ged_private.h"


#define ABORTED -99
#define OLDSOLID 0
#define NEWSOLID 1
#define SOL_TABLE 1
#define REG_TABLE 2
#define ID_TABLE 3


#define PATH_STEP 256
static struct directory **path = NULL;
static size_t path_capacity = 0;


static int wcodes_printcodes(struct ged *gedp, FILE *fp, struct directory *dp, size_t pathpos);


static void
wcodes_printnode(struct db_i *dbip, struct rt_comb_internal *UNUSED(comb), union tree *comb_leaf, void *user_ptr1, void *user_ptr2, void *user_ptr3, void *UNUSED(user_ptr4))
{
    FILE *fp;
    size_t *pathpos;
    struct directory *nextdp;
    struct ged *gedp;

    RT_CK_DBI(dbip);
    RT_CK_TREE(comb_leaf);

    nextdp = db_lookup(dbip, comb_leaf->tr_l.tl_name, LOOKUP_NOISY);
    if (nextdp == RT_DIR_NULL)
	return;

    fp = (FILE *)user_ptr1;
    pathpos = (size_t *)user_ptr2;
    gedp = (struct ged *)user_ptr3;

    /* recurse on combinations */
    if (nextdp->d_flags & RT_DIR_COMB)
	(void)wcodes_printcodes(gedp, fp, nextdp, (*pathpos)+1);
}


static int
wcodes_printcodes(struct ged *gedp, FILE *fp, struct directory *dp, size_t pathpos)
{
    size_t i;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    int id;

    if (!(dp->d_flags & RT_DIR_COMB))
	return BRLCAD_OK;

    id = rt_db_get_internal(&intern, dp, gedp->dbip, (matp_t)NULL, &rt_uniresource);
    if (id < 0) {
	bu_vls_printf(gedp->ged_result_str, "Cannot get records for %s\n", dp->d_namep);
	return BRLCAD_ERROR;
    }

    if (id != ID_COMBINATION) {
	intern.idb_meth->ft_ifree(&intern);
	return BRLCAD_OK;
    }

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);

    if (comb->region_flag) {
	fprintf(fp, "%-6ld %-3ld %-3ld %-4ld  ",
		comb->region_id,
		comb->aircode,
		comb->GIFTmater,
		comb->los);
	for (i =0 ; i < pathpos; i++)
	    fprintf(fp, "/%s", path[i]->d_namep);
	fprintf(fp, "/%s\n", dp->d_namep);
	intern.idb_meth->ft_ifree(&intern);
	return BRLCAD_OK;
    }

    if (comb->tree) {
	if (pathpos >= path_capacity) {
	    path_capacity += PATH_STEP;
	    path = (struct directory **)bu_realloc(path, sizeof(struct directory *) * path_capacity, "realloc path bigger");
	}
	path[pathpos] = dp;
	db_tree_funcleaf(gedp->dbip, comb, comb->tree, wcodes_printnode,
			 (void *)fp, (void *)&pathpos, (void *)gedp, (void *)gedp);
    }

    intern.idb_meth->ft_ifree(&intern);
    return BRLCAD_OK;
}


int
ged_wcodes_core(struct ged *gedp, int argc, const char *argv[])
{
    int i;
    int status;
    FILE *fp;
    struct directory *dp;
    static const char *usage = "filename object(s)";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	if (argc == 1)
	    return GED_HELP;
	return BRLCAD_ERROR;
    }

    fp = fopen(argv[1], "w");
    if (fp == NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: Failed to open file - %s",
		      argv[0], argv[1]);
	return BRLCAD_ERROR;
    }

    path = (struct directory **)bu_calloc(PATH_STEP, sizeof(struct directory *), "alloc initial path");
    path_capacity = PATH_STEP;

    for (i = 2; i < argc; ++i) {
	if ((dp = db_lookup(gedp->dbip, argv[i], LOOKUP_NOISY)) != RT_DIR_NULL) {
	    status = wcodes_printcodes(gedp, fp, dp, 0);

	    if (status & BRLCAD_ERROR) {
		(void)fclose(fp);
		return BRLCAD_ERROR;
	    }
	}
    }

    (void)fclose(fp);
    bu_free(path, "dealloc path");
    path = NULL;
    path_capacity = 0;

    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl wcodes_cmd_impl = {
    "wcodes",
    ged_wcodes_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd wcodes_cmd = { &wcodes_cmd_impl };
const struct ged_cmd *wcodes_cmds[] = { &wcodes_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  wcodes_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info(void)
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

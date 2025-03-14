/*                         C O P Y E V A L . C
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
/** @file libged/copyeval.c
 *
 * The copyeval command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"

#include "../ged_private.h"


int
ged_copyeval_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "path_to_old_prim new_prim";
    struct _ged_trace_data gtd;
    struct directory *dp;
    struct rt_db_internal *ip;
    struct rt_db_internal internal = RT_DB_INTERNAL_INIT_ZERO;
    struct rt_db_internal new_int = RT_DB_INTERNAL_INIT_ZERO;

    char *tok;
    int endpos = 0;
    int i;
    mat_t start_mat;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /* initialize gtd */
    gtd.gtd_gedp = gedp;
    gtd.gtd_flag = _GED_CPEVAL;
    gtd.gtd_prflag = 0;

    /* check if new solid name already exists in description */
    GED_CHECK_EXISTS(gedp, argv[2], LOOKUP_QUIET, BRLCAD_ERROR);

    MAT_IDN(start_mat);

    /* build directory pointer array for desired path */
    if (strchr(argv[1], '/')) {
	tok = strtok((char *)argv[1], "/");
	while (tok) {
	    GED_DB_LOOKUP(gedp, gtd.gtd_obj[endpos], tok, LOOKUP_NOISY, BRLCAD_ERROR & GED_QUIET);
	    endpos++;
	    tok = strtok((char *)NULL, "/");
	}
    } else {
	GED_DB_LOOKUP(gedp, gtd.gtd_obj[endpos], argv[1], LOOKUP_NOISY, BRLCAD_ERROR & GED_QUIET);
	endpos++;
    }

    if (endpos < 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    gtd.gtd_objpos = endpos - 1;

    GED_DB_GET_INTERNAL(gedp, &internal, gtd.gtd_obj[endpos - 1], bn_mat_identity, &rt_uniresource, BRLCAD_ERROR);

    if (endpos > 1) {
	/* Make sure that final component in path is a solid */
	if (internal.idb_type == ID_COMBINATION) {
	    rt_db_free_internal(&internal);
	    bu_vls_printf(gedp->ged_result_str, "final component on path must be a primitive!\n");
	    return BRLCAD_ERROR;
	}

	/* Accumulate the matrices */
	ged_trace(gtd.gtd_obj[0], 0, start_mat, &gtd, 1);

	if (gtd.gtd_prflag == 0) {
	    bu_vls_printf(gedp->ged_result_str, "PATH:  ");

	    for (i = 0; i < gtd.gtd_objpos; i++)
		bu_vls_printf(gedp->ged_result_str, "/%s", gtd.gtd_obj[i]->d_namep);

	    bu_vls_printf(gedp->ged_result_str, "  NOT FOUND\n");
	    rt_db_free_internal(&internal);
	    return BRLCAD_ERROR;
	}

	/* Have found the desired path - wdb_xform is the transformation matrix */
	/* wdb_xform matrix calculated in wdb_trace() */

	/* create the new solid */
	RT_DB_INTERNAL_INIT(&new_int);
	if (rt_generic_xform(&new_int, gtd.gtd_xform, &internal, 0, gedp->dbip)) {
	    rt_db_free_internal(&internal);
	    bu_vls_printf(gedp->ged_result_str, "ged_copyeval: rt_generic_xform failed\n");
	    return BRLCAD_ERROR;
	}

	ip = &new_int;
    } else
	ip = &internal;

    /* should call GED_DB_DIRADD() but need to deal with freeing the
     * internals on failure.
     */
    dp = db_diradd(gedp->dbip, argv[2], RT_DIR_PHONY_ADDR, 0, gtd.gtd_obj[endpos-1]->d_flags, (void *)&ip->idb_type);
    if (dp == RT_DIR_NULL) {
	rt_db_free_internal(&internal);
	if (ip == &new_int)
	    rt_db_free_internal(&new_int);
	bu_vls_printf(gedp->ged_result_str, "An error has occurred while adding a new object to the database.");
	return BRLCAD_ERROR;
    }

    /* should call GED_DB_DIRADD() but need to deal with freeing the
     * internals on failure.
     */
    if (rt_db_put_internal(dp, gedp->dbip, ip, &rt_uniresource) < 0) {
	/* if (ip == &new_int) then new_int gets freed by the rt_db_put_internal above
	 * regardless of whether it succeeds or not. At this point only internal needs
	 * to be freed. On the other hand if (ip == &internal), the internal gets freed
	 * freed by the rt_db_put_internal above. In this case memory for new_int has
	 * not been allocated.
	 */
	if (ip == &new_int)
	    rt_db_free_internal(&internal);

	bu_vls_printf(gedp->ged_result_str, "Database write error, aborting");
	return BRLCAD_ERROR;
    }

    /* see previous comment */
    if (ip == &new_int)
	rt_db_free_internal(&internal);

    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl copyeval_cmd_impl = {
    "copyeval",
    ged_copyeval_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd copyeval_cmd = { &copyeval_cmd_impl };
const struct ged_cmd *copyeval_cmds[] = { &copyeval_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  copyeval_cmds, 1 };

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

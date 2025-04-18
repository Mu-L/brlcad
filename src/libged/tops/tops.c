/*                         T O P S . C
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
/** @file libged/tops.c
 *
 * The tops command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "bu/getopt.h"

#include "../ged_private.h"


int
ged_tops_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    int i;
    struct directory **dirp;
    struct directory **dirp0 = (struct directory **)NULL;
    int c;

    int no_decorate = 0;
    int aflag = 0;
    int hflag = 0;
    int pflag = 0;

    /* static const char *usage = "[-a|-h|-n|-p]"; */

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* process any options */
    bu_optind = 1;	/* re-init bu_getopt() */
    while ((c = bu_getopt(argc, (char * const *)argv, "ahnp")) != -1) {
	switch (c) {
	    case 'a':
		aflag = 1;
		break;
	    case 'h':
		hflag = 1;
		break;
	    case 'n':
		no_decorate = 1;
		break;
	    case 'p':
		pflag = 1;
		break;
	    default:
		break;
	}
    }

    /* Can this be executed only sometimes?
       Perhaps a "dirty bit" on the database? */
    db_update_nref(gedp->dbip, &rt_uniresource);

    /*
     * Find number of possible entries and allocate memory
     */
    dirp = _ged_dir_getspace(gedp->dbip, 0);
    dirp0 = dirp;

    if (db_version(gedp->dbip) < 5) {
	for (i = 0; i < RT_DBNHASH; i++)
	    for (dp = gedp->dbip->dbi_Head[i];
		 dp != RT_DIR_NULL;
		 dp = dp->d_forw) {
		if (dp->d_nref == 0)
		    *dirp++ = dp;
	    }
    } else {
	for (i = 0; i < RT_DBNHASH; i++)
	    for (dp = gedp->dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw) {

		if (dp->d_nref != 0) {
		    continue;
		}

		if ((aflag) ||
		    (hflag && (dp->d_flags & RT_DIR_HIDDEN)) ||
		    (pflag && dp->d_addr == RT_DIR_PHONY_ADDR)) {

		    /* add object because it matches an option */
		    *dirp++ = dp;

		} else if (!aflag && !hflag && !pflag &&
			   !(dp->d_flags & RT_DIR_HIDDEN) &&
			   (dp->d_addr != RT_DIR_PHONY_ADDR)) {

		    /* add non-hidden real object */
		    *dirp++ = dp;

		}
	    }
    }

    _ged_vls_col_pr4v(gedp->ged_result_str, dirp0, (int)(dirp - dirp0), no_decorate, 0);
    bu_free((void *)dirp0, "wdb_tops_cmd: wdb_dir_getspace");

    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl tops_cmd_impl = {
    "tops",
    ged_tops_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd tops_cmd = { &tops_cmd_impl };
const struct ged_cmd *tops_cmds[] = { &tops_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  tops_cmds, 1 };

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

/*                        C O M B _ C O L O R . C
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
/** @file libged/comb_color.c
 *
 * The comb_color command.
 *
 */

#include "ged.h"


int
ged_comb_color_core(struct ged *gedp, int argc, const char *argv[])
{
    int i;
    int val;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    static const char *usage = "combination R G B";

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

    if (argc != 5) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    GED_DB_LOOKUP(gedp, dp, argv[1], LOOKUP_NOISY, BRLCAD_ERROR);
    GED_CHECK_COMB(gedp, dp, BRLCAD_ERROR);
    GED_DB_GET_INTERNAL(gedp, &intern, dp, (fastf_t *)NULL, &rt_uniresource, BRLCAD_ERROR);

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);

    for (i = 0; i < 3; ++i) {
	if (sscanf(argv[i+2], "%d", &val) != 1 || val < 0 || 255 < val) {
	    bu_vls_printf(gedp->ged_result_str, "RGB value out of range: %s", argv[i + 2]);
	    rt_db_free_internal(&intern);
	    return BRLCAD_ERROR;
	} else
	    comb->rgb[i] = val;
    }

    comb->rgb_valid = 1;
    GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, BRLCAD_ERROR);

    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl comb_color_cmd_impl = {
    "comb_color",
    ged_comb_color_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd comb_color_cmd = { &comb_color_cmd_impl };
const struct ged_cmd *comb_color_cmds[] = { &comb_color_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  comb_color_cmds, 1 };

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

/*                         B O T _ F U S E . C
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
/** @file libged/bot_fuse.c
 *
 * The bot_fuse command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/getopt.h"
#include "bu/parallel.h"
#include "rt/geom.h"
#include "bv/plot3.h"

#include "../ged_private.h"


static size_t
show_dangling_edges(struct ged *gedp, const uint32_t *magic_p, const char *name, int out_type, struct bu_list *vlfree)
{
    FILE *plotfp = NULL;
    const char *manifolds = NULL;
    const struct edgeuse *eur;
    int done;
    point_t pt1, pt2;
    size_t i, cnt;
    struct bv_vlblock *vbp = NULL;
    struct bu_list *vhead = NULL;
    struct bu_ptbl faces;
    struct bu_vls plot_file_name = BU_VLS_INIT_ZERO;
    struct edgeuse *eu = NULL;
    struct face *fp = NULL;
    struct faceuse *fu, *fu1, *fu2;
    struct faceuse *newfu = NULL;
    struct loopuse *lu = NULL;

    /* out_type: 0 = none, 1 = show, 2 = plot */
    if (out_type < 0 || out_type > 2) {
	bu_log("Internal error, open edge test failed.\n");
	return 0;
    }

    if (out_type == 1) {
	vbp = bv_vlblock_init(vlfree, 32);
	vhead = bv_vlblock_find(vbp, 0xFF, 0xFF, 0x00);
    }

    bu_ptbl_init(&faces, 64, "faces buffer");
    nmg_face_tabulate(&faces, magic_p, vlfree);

    cnt = 0;
    for (i = 0; i < (size_t)BU_PTBL_LEN(&faces) ; i++) {
	fp = (struct face *)BU_PTBL_GET(&faces, i);
	NMG_CK_FACE(fp);
	fu = fu1 = fp->fu_p;
	NMG_CK_FACEUSE(fu1);
	fu2 = fp->fu_p->fumate_p;
	NMG_CK_FACEUSE(fu2);
	done = 0;
	while (!done) {
	    NMG_CK_FACEUSE(fu);
	    for (BU_LIST_FOR(lu, loopuse, &fu->lu_hd)) {
		NMG_CK_LOOPUSE(lu);
		if (BU_LIST_FIRST_MAGIC(&lu->down_hd) == NMG_EDGEUSE_MAGIC) {
		    for (BU_LIST_FOR(eu, edgeuse, &lu->down_hd)) {
			NMG_CK_EDGEUSE(eu);
			eur = nmg_radial_face_edge_in_shell(eu);
			newfu = eur->up.lu_p->up.fu_p;
			while (manifolds &&
			       NMG_MANIFOLDS(manifolds, newfu) &
			       NMG_2MANIFOLD &&
			       eur != eu->eumate_p) {
			    eur = nmg_radial_face_edge_in_shell(eur->eumate_p);
			    newfu = eur->up.lu_p->up.fu_p;
			}
			if (eur == eu->eumate_p) {
			    VMOVE(pt1, eu->vu_p->v_p->vg_p->coord);
			    VMOVE(pt2, eu->eumate_p->vu_p->v_p->vg_p->coord);
			    if (out_type == 1) {
				BV_ADD_VLIST(vbp->free_vlist_hd, vhead, pt1, BV_VLIST_LINE_MOVE);
				BV_ADD_VLIST(vbp->free_vlist_hd, vhead, pt2, BV_VLIST_LINE_DRAW);
			    } else if (out_type == 2) {
				if (!plotfp) {
				    bu_vls_sprintf(&plot_file_name, "%s.%p.pl", name, (void *)magic_p);
				    plotfp = fopen(bu_vls_addr(&plot_file_name), "wb");
				    if (plotfp == (FILE *)NULL) {
					bu_vls_free(&plot_file_name);
					bu_log("Error, unable to create plot file (%s), open edge test failed.\n",
					       bu_vls_addr(&plot_file_name));
					return 0;
				    }
				}
				pdv_3line(plotfp, pt1, pt2);
			    }
			    cnt++;
			}
		    }
		}
	    }
	    if (fu == fu1) fu = fu2;
	    if (fu == fu2) done = 1;
	};

    }

    if (out_type == 1) {
	/* Add overlay */
	if (gedp->new_cmd_forms) {
	    struct bu_vls nroot = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&nroot, "bot_fuse::%s", name);
	    struct bview *view = gedp->ged_gvp;
	    bv_vlblock_obj(vbp, view, bu_vls_cstr(&nroot));
	    bu_vls_free(&nroot);
	} else {
	    _ged_cvt_vlblock_to_solids(gedp, vbp, name, 0);
	}
	bv_vlblock_free(vbp);
	bu_log("Showing open edges...\n");
    } else if (out_type == 2) {
	if (plotfp) {
	    (void)fclose(plotfp);
	    bu_log("Wrote plot file (%s)\n", bu_vls_addr(&plot_file_name));
	    bu_vls_free(&plot_file_name);
	}
    }
    bu_ptbl_free(&faces);

    return cnt;
}


int
ged_bot_fuse_core(struct ged *gedp, int argc, const char **argv)
{
    struct directory *old_dp, *new_dp;
    struct rt_db_internal intern, intern2;
    struct rt_bot_internal *bot;
    int count=0;
    static const char *usage = "new_bot old_bot";
    struct bu_list *vlfree = &rt_vlfree;

    struct model *m;
    struct nmgregion *r;
    int ret, c, i;
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    struct bn_tol *tol = &wdbp->wdb_tol;
    int total = 0;
    volatile int out_type = 0; /* open edge output type: 0 = none, 1 = show, 2 = plot */
    size_t open_cnt;
    struct bu_vls name_prefix = BU_VLS_INIT_ZERO;

    /* bu_getopt() options */
    static const char *bot_fuse_options = "sp";
    static const char *bot_fuse_options_str = "[-s|-p]";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc != 3 && argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s %s", argv[0], bot_fuse_options_str, usage);
	return GED_HELP;
    }

    /* Turn off getopt's error messages */
    bu_opterr = 0;
    bu_optind = 1;

    /* get all the option flags from the command line */
    while ((c=bu_getopt(argc, (char **)argv, bot_fuse_options)) != -1) {
	switch (c) {
	    case 's':
		{
		    out_type = 1; /* show open edges */
		    break;
		}
	    case 'p':
		{
		    out_type = 2; /* plot open edges */
		    break;
		}
	    default :
		{
		    bu_vls_printf(gedp->ged_result_str, "Unknown option: '%c'", c);
		    return GED_HELP;
		}
	}
    }

    i = argc - 2;

    bu_log("%s: start\n", argv[0]);

    GED_DB_LOOKUP(gedp, old_dp, argv[i+1], LOOKUP_NOISY, BRLCAD_ERROR & GED_QUIET);
    GED_DB_GET_INTERNAL(gedp, &intern, old_dp, bn_mat_identity, &rt_uniresource, BRLCAD_ERROR);

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s is not a BOT solid!\n", argv[0], argv[i+1]);
	return BRLCAD_ERROR;
    }

    /* create nmg model structure */
    m = nmg_mm();

    /* place bot in nmg structure */
    bu_log("%s: running rt_bot_tess\n", argv[0]);
    ret = rt_bot_tess(&r, m, &intern, &wdbp->wdb_ttol, tol);

    /* free internal representation of original bot */
    rt_db_free_internal(&intern);

    if (ret != 0) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s fuse failed (1).\n", argv[0], argv[i+1]);
	nmg_km(m);
	return BRLCAD_ERROR;
    }

    total = 0;

    /* Step 1 -- the vertices. */
    bu_log("%s: running nmg_vertex_fuse\n", argv[0]);
    count = nmg_vertex_fuse(&m->magic, vlfree, tol);
    total += count;
    bu_log("%s: %s, %d vertex fused\n", argv[0], argv[i+1], count);

    /* Step 1.5 -- break edges on vertices, before fusing edges */
    bu_log("%s: running nmg_break_e_on_v\n", argv[0]);
    count = nmg_break_e_on_v(&m->magic, vlfree, tol);
    total += count;
    bu_log("%s: %s, %d broke 'e' on 'v'\n", argv[0], argv[i+1], count);

    if (total) {
	struct nmgregion *r2;
	struct shell *s;

	bu_log("%s: running nmg_make_faces_within_tol\n", argv[0]);

	/* vertices and/or edges have been moved,
	 * may have created out-of-tolerance faces
	 */

	for (BU_LIST_FOR(r2, nmgregion, &m->r_hd)) {
	    for (BU_LIST_FOR(s, shell, &r2->s_hd))
		nmg_make_faces_within_tol(s, vlfree, tol);
	}
    }

    /* Step 2 -- the face geometry */
    bu_log("%s: running nmg_model_face_fuse\n", argv[0]);
    count = nmg_model_face_fuse(m, vlfree, tol);
    total += count;
    bu_log("%s: %s, %d faces fused\n", argv[0], argv[i+1], count);

    /* Step 3 -- edges */
    bu_log("%s: running nmg_edge_fuse\n", argv[0]);
    count = nmg_edge_fuse(&m->magic, vlfree, tol);
    total += count;

    bu_log("%s: %s, %d edges fused\n", argv[0], argv[i+1], count);

    bu_log("%s: %s, %d total fused\n", argv[0], argv[i+1], total);

    if (!BU_SETJUMP) {
	/* try */

	/* convert the nmg model back into a bot */
	bot = nmg_bot(BU_LIST_FIRST(shell, &r->s_hd), vlfree, tol);

	bu_vls_sprintf(&name_prefix, "open_edges.%s", argv[i]);
	bu_log("%s: running show_dangling_edges\n", argv[0]);
	open_cnt = show_dangling_edges(gedp, &m->magic, bu_vls_addr(&name_prefix), out_type, vlfree);
	bu_log("%s: WARNING %zu open edges, new BOT may be invalid!!!\n", argv[0], open_cnt);
	bu_vls_free(&name_prefix);

	/* free the nmg model structure */
	nmg_km(m);
    } else {
	/* catch */
	BU_UNSETJUMP;
	bu_vls_printf(gedp->ged_result_str, "%s: %s fuse failed (2).\n", argv[0], argv[i+1]);
	return BRLCAD_ERROR;
    } BU_UNSETJUMP;

    RT_DB_INTERNAL_INIT(&intern2);
    intern2.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern2.idb_type = ID_BOT;
    intern2.idb_meth = &OBJ[ID_BOT];
    intern2.idb_ptr = (void *)bot;

    GED_DB_DIRADD(gedp, new_dp, argv[i], RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern2.idb_type, BRLCAD_ERROR);
    GED_DB_PUT_INTERNAL(gedp, new_dp, &intern2, &rt_uniresource, BRLCAD_ERROR);

    bu_log("%s: Created new BOT (%s)\n", argv[0], argv[i]);
    bu_log("%s: Done.\n", argv[0]);

    return BRLCAD_OK;
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

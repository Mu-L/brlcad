/*                         E D R P C . C
 * BRL-CAD
 *
 * Copyright (c) 1996-2025 United States Government as represented by
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
/** @file primitives/edrpc.c
 *
 */

#include "common.h"

#include <math.h>
#include <string.h>

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"

#include "../edit_private.h"

#define ECMD_RPC_B		17043
#define ECMD_RPC_H		17044
#define ECMD_RPC_R		17045

void
rt_edit_rpc_set_edit_mode(struct rt_edit *s, int mode)
{
    rt_edit_set_edflag(s, mode);

    switch (mode) {
	case ECMD_RPC_B:
	case ECMD_RPC_H:
	case ECMD_RPC_R:
	    s->edit_mode = RT_PARAMS_EDIT_SCALE;
	    break;
	default:
	    break;
    };

    bu_clbk_t f = NULL;
    void *d = NULL;
    int flag = 1;
    rt_edit_map_clbk_get(&f, &d, s->m, ECMD_EAXES_POS, BU_CLBK_DURING);
    if (f)
	(*f)(0, NULL, d, &flag);
}

static void
rpc_ed(struct rt_edit *s, int arg, int UNUSED(a), int UNUSED(b), void *UNUSED(data))
{
    rt_edit_rpc_set_edit_mode(s, arg);
}

struct rt_edit_menu_item rpc_menu[] = {
    { "RPC MENU", NULL, 0 },
    { "Set B", rpc_ed, ECMD_RPC_B },
    { "Set H", rpc_ed, ECMD_RPC_H },
    { "Set r", rpc_ed, ECMD_RPC_R },
    { "", NULL, 0 }
};

struct rt_edit_menu_item *
rt_edit_rpc_menu_item(const struct bn_tol *UNUSED(tol))
{
    return rpc_menu;
}

#define V3BASE2LOCAL(_pt) (_pt)[X]*base2local, (_pt)[Y]*base2local, (_pt)[Z]*base2local

void
rt_edit_rpc_write_params(
	struct bu_vls *p,
       	const struct rt_db_internal *ip,
       	const struct bn_tol *UNUSED(tol),
	fastf_t base2local)
{
    struct rt_rpc_internal *rpc = (struct rt_rpc_internal *)ip->idb_ptr;
    RT_RPC_CK_MAGIC(rpc);

    bu_vls_printf(p, "Vertex: %.9f %.9f %.9f\n", V3BASE2LOCAL(rpc->rpc_V));
    bu_vls_printf(p, "Height: %.9f %.9f %.9f\n", V3BASE2LOCAL(rpc->rpc_H));
    bu_vls_printf(p, "Breadth: %.9f %.9f %.9f\n", V3BASE2LOCAL(rpc->rpc_B));
    bu_vls_printf(p, "Half-width: %.9f\n", rpc->rpc_r * base2local);
}


#define read_params_line_incr \
    lc = (ln) ? (ln + lcj) : NULL; \
    if (!lc) { \
	bu_free(wc, "wc"); \
	return BRLCAD_ERROR; \
    } \
    ln = strchr(lc, tc); \
    if (ln) *ln = '\0'; \
    while (lc && strchr(lc, ':')) lc++

int
rt_edit_rpc_read_params(
	struct rt_db_internal *ip,
	const char *fc,
	const struct bn_tol *UNUSED(tol),
	fastf_t local2base
	)
{
    double a = 0.0;
    double b = 0.0;
    double c = 0.0;
    struct rt_rpc_internal *rpc = (struct rt_rpc_internal *)ip->idb_ptr;
    RT_RPC_CK_MAGIC(rpc);

    if (!fc)
	return BRLCAD_ERROR;

    // We're getting the file contents as a string, so we need to split it up
    // to process lines. See https://stackoverflow.com/a/17983619

    // Figure out if we need to deal with Windows line endings
    const char *crpos = strchr(fc, '\r');
    int crlf = (crpos && crpos[1] == '\n') ? 1 : 0;
    char tc = (crlf) ? '\r' : '\n';
    // If we're CRLF jump ahead another character.
    int lcj = (crlf) ? 2 : 1;

    char *ln = NULL;
    char *wc = bu_strdup(fc);
    char *lc = wc;

    // Set up initial line (Vertex)
    ln = strchr(lc, tc);
    if (ln) *ln = '\0';

    // Trim off prefixes, if user left them in
    while (lc && strchr(lc, ':')) lc++;

    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(rpc->rpc_V, a, b, c);
    VSCALE(rpc->rpc_V, rpc->rpc_V, local2base);

    // Set up Height line
    read_params_line_incr;

    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(rpc->rpc_H, a, b, c);
    VSCALE(rpc->rpc_H, rpc->rpc_H, local2base);

    // Set up Breadth line
    read_params_line_incr;

    sscanf(lc, "%lf %lf %lf", &a, &b, &c);
    VSET(rpc->rpc_B, a, b, c);
    VSCALE(rpc->rpc_B, rpc->rpc_B, local2base);

    // Set up Half-width line
    read_params_line_incr;

    sscanf(lc, "%lf", &a);
    rpc->rpc_r = a * local2base;

    // Cleanup
    bu_free(wc, "wc");
    return BRLCAD_OK;
}

/* scale vector B */
void
ecmd_rpc_b(struct rt_edit *s)
{
    struct rt_rpc_internal *rpc =
	(struct rt_rpc_internal *)s->es_int.idb_ptr;
    RT_RPC_CK_MAGIC(rpc);

    if (s->e_inpara) {
	/* take s->e_mat[15] (path scaling) into account */
	s->e_para[0] *= s->e_mat[15];
	s->es_scale = s->e_para[0] / MAGNITUDE(rpc->rpc_B);
    }
    VSCALE(rpc->rpc_B, rpc->rpc_B, s->es_scale);
}

/* scale vector H */
void
ecmd_rpc_h(struct rt_edit *s)
{
    struct rt_rpc_internal *rpc =
	(struct rt_rpc_internal *)s->es_int.idb_ptr;

    RT_RPC_CK_MAGIC(rpc);
    if (s->e_inpara) {
	/* take s->e_mat[15] (path scaling) into account */
	s->e_para[0] *= s->e_mat[15];
	s->es_scale = s->e_para[0] / MAGNITUDE(rpc->rpc_H);
    }
    VSCALE(rpc->rpc_H, rpc->rpc_H, s->es_scale);
}

/* scale rectangular half-width of RPC */
void
ecmd_rpc_r(struct rt_edit *s)
{
    struct rt_rpc_internal *rpc =
	(struct rt_rpc_internal *)s->es_int.idb_ptr;

    RT_RPC_CK_MAGIC(rpc);
    if (s->e_inpara) {
	/* take s->e_mat[15] (path scaling) into account */
	s->e_para[0] *= s->e_mat[15];
	s->es_scale = s->e_para[0] / rpc->rpc_r;
    }
    rpc->rpc_r *= s->es_scale;
}

static int
rt_edit_rpc_pscale(struct rt_edit *s)
{
    if (s->e_inpara > 1) {
	bu_vls_printf(s->log_str, "ERROR: only one argument needed\n");
	s->e_inpara = 0;
	return BRLCAD_ERROR;
    }

    if (s->e_inpara) {
	if (s->e_para[0] <= 0.0) {
	    bu_vls_printf(s->log_str, "ERROR: SCALE FACTOR <= 0\n");
	    s->e_inpara = 0;
	    return BRLCAD_ERROR;
	}

	/* must convert to base units */
	s->e_para[0] *= s->local2base;
	s->e_para[1] *= s->local2base;
	s->e_para[2] *= s->local2base;
    }

    switch (s->edit_flag) {
	case ECMD_RPC_B:
	    ecmd_rpc_b(s);
	    break;
	case ECMD_RPC_H:
	    ecmd_rpc_h(s);
	    break;
	case ECMD_RPC_R:
	    ecmd_rpc_r(s);
	    break;
    };

    return 0;
}

int
rt_edit_rpc_edit(struct rt_edit *s)
{
    switch (s->edit_flag) {
	case RT_PARAMS_EDIT_SCALE:
	    /* scale the solid uniformly about its vertex point */
	    return edit_sscale(s);
	case RT_PARAMS_EDIT_TRANS:
	    /* translate solid */
	    edit_stra(s);
	    break;
	case RT_PARAMS_EDIT_ROT:
	    /* rot solid about vertex */
	    edit_srot(s);
	    break;
	default:
	    return rt_edit_rpc_pscale(s);
    }
    return 0;
}

int
rt_edit_rpc_edit_xy(
        struct rt_edit *s,
        const vect_t mousevec
        )
{
    vect_t pos_view = VINIT_ZERO;       /* Unrotated view space pos */
    struct rt_db_internal *ip = &s->es_int;
    bu_clbk_t f = NULL;
    void *d = NULL;

    switch (s->edit_flag) {
        case RT_PARAMS_EDIT_SCALE:
	case ECMD_RPC_B:
	case ECMD_RPC_H:
	case ECMD_RPC_R:
            edit_sscale_xy(s, mousevec);
            return 0;
        case RT_PARAMS_EDIT_TRANS:
            edit_stra_xy(&pos_view, s, mousevec);
            edit_abs_tra(s, pos_view);
            return 0;
        case RT_PARAMS_EDIT_ROT:
            bu_vls_printf(s->log_str, "RT_PARAMS_EDIT_ROT XY editing setup unimplemented in %s_edit_xy callback\n", EDOBJ[ip->idb_type].ft_label);
            rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
            if (f)
                (*f)(0, NULL, d, NULL);
            return BRLCAD_ERROR;
        default:
            bu_vls_printf(s->log_str, "%s: XY edit undefined in solid edit mode %d\n", EDOBJ[ip->idb_type].ft_label, s->edit_flag);
            rt_edit_map_clbk_get(&f, &d, s->m, ECMD_PRINT_RESULTS, BU_CLBK_DURING);
            if (f)
                (*f)(0, NULL, d, NULL);
            return BRLCAD_ERROR;
    }
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

/*                   I G E S _ S T R U C T . H
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

#ifndef CONV_IGES_IGES_STRUCT_H
#define CONV_IGES_IGES_STRUCT_H

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"


#define NAMESIZE 16 /* from db.h */

#define TOL 0.0005

#define Union 1
#define Intersect 2
#define Subtract 3

/* Circularly linked list of files and names for external references */
struct file_list
{
    struct bu_list l;			/* for links */
    char *file_name;		/* Name of external file */
    char obj_name[NAMESIZE+1];	/* BRL-CAD name for top level object */
};
#define IGES_FILE_LIST_INIT_ZERO {BU_LIST_INIT_ZERO, NULL, {0}}

/* linked list of used BRL-CAD object names */
struct name_list
{
    char name[NAMESIZE+1];
    struct name_list *next;
};


/* Structure for storing info from the IGES file directory section along with
   transformation matrices */
struct iges_directory
{
    int type; /* IGES entity type */
    int form; /* IGES form number */
    int view; /* View field from DE, indicates which views this entity is in */
    int param; /* record number for parameter entry */
    int paramlines; /* number of lines for this entity in parameter section */
    int direct; /* directory entry sequence number */
    int status; /* status entry from directory entry */
/*
 * Note that the directory entry sequence number and the directory structure
 * array index are not the same.  The index into the array can be calculated
 * as (sequence number - 1)/2.
 */
    int trans; /* index into directory array for transformation matrix */
    int colorp; /* pointer to color definition entity (or color number) */
    unsigned char rgb[3]; /* Actual color */
    char *mater;	/* material parameter string */
    int referenced;
/*
 * uses of the "referenced" field:
 *	for solid objects - number of times this entity is referenced by
 *		boolean trees or solid assemblies.
 *	For transformation entities - it indicates whether the matrix list
 *		has been evaluated.
 *	for attribute instances - It contains the DE for the attribute
 *		definition entity.
 */
    char *name; /* entity name */
    mat_t *rot; /* transformation matrix. */
};


/* Structure used in building boolean trees in memory */
struct node
{
    int op; /* if positive, this is an operator (Union, Intersect, or Subtract)
	       if negative, this is a directory entry sequence number (operand) */
    struct node *left, *right, *parent;
};


/* structure for storing attributes */
struct brlcad_att
{
    char *material_name;
    char *material_params;
    int region_flag;
    int ident;
    int air_code;
    int material_code;
    int los_density;
    int inherit;
    int color_defined;
};


/* Structure for linked list of points */
struct ptlist
{
    point_t pt;
    struct ptlist *next, *prev;
};


/* Structures for Parametric Splines */
struct segment
{
    int segno;
    fastf_t tmin, tmax;
    fastf_t cx[4], cy[4], cz[4];
    struct segment *next;
};


struct spline
{
    int ndim, nsegs;
    struct segment *start;
};


struct reglist
{
    char name[NAMESIZE+1];
    struct node *tree;
    struct reglist *next;
};


struct types
{
    int type;
    char *name;
    int count;
};


struct iges_edge_use
{
    int edge_de;		/* directory sequence number for edge list or vertex list */
    int edge_is_vertex;		/* flag */
    int index;			/* index into list for this edge */
    int orient;			/* orientation flag (1 => agrees with geometry */
    struct iges_param_curve *root;
};


struct iges_vertex
{
    point_t pt;
    struct vertex *v;
};


struct iges_vertex_list
{
    int vert_de;
    int no_of_verts;
    struct iges_vertex *i_verts;
    struct iges_vertex_list *next;
};


struct iges_edge
{
    int curve_de;
    int start_vert_de;
    int start_vert_index;
    int end_vert_de;
    int end_vert_index;
};


struct iges_param_curve
{
    int curve_de;
    struct iges_param_curve *next;
};


struct iges_edge_list
{
    int edge_de;
    int no_of_edges;
    struct iges_edge *i_edge;
    struct iges_edge_list *next;
};


extern char *iges_type(int type_no);
extern char *Make_unique_brl_name(char *name);
extern int Add_loop_to_face(struct shell *s, struct faceuse *fu, size_t entityno, int face_orient, struct bu_list *vlfree);
extern int Add_nurb_loop_to_face(struct shell *s, struct faceuse *fu, int loop_entityno);
extern int arb_to_iges(struct rt_db_internal *ip, char *name, FILE *fp_dir, FILE *fp_param);
extern int ell_to_iges(struct rt_db_internal *ip, char *name, FILE *fp_dir, FILE *fp_param);
extern int nmg_to_iges(struct rt_db_internal *ip, char *name, FILE *fp_dir, FILE *fp_param);
extern int nmgregion_to_iges(char *name, struct nmgregion *r, int dependent, FILE *fp_dir, FILE *fp_param);
extern int null_to_iges(struct rt_db_internal *ip, char *name, FILE *fp_dir, FILE *fp_param);
extern int sph_to_iges(struct rt_db_internal *ip, char *name, FILE *fp_dir, FILE *fp_param);
extern int tgc_to_iges(struct rt_db_internal *ip, char *name, FILE *fp_dir, FILE *fp_param);
extern int tor_to_iges(struct rt_db_internal *ip, char *name, FILE *fp_dir, FILE *fp_param);
extern int write_shell_face_loop(struct nmgregion *r, int edge_de, struct bu_ptbl *etab, int vert_de, struct bu_ptbl *vtab, FILE *fp_dir, FILE *fp_param);
extern struct edge_g_cnurb *Get_cnurb(size_t entityno);
extern struct edge_g_cnurb *Get_cnurb_curve(int curve_de, int *linear);
extern struct faceuse *Add_face_to_shell(struct shell *s, size_t entityno, int face_orient, struct bu_list *vlfree);
extern struct faceuse *Make_nurb_face(struct shell *s, int surf_entityno);
extern struct faceuse *Make_planar_face(struct shell *s, size_t entityno, int face_orient, struct bu_list *vlfree);
extern struct iges_edge_list *Get_edge_list(struct iges_edge_use *edge);
extern struct iges_edge_list *Read_edge_list(struct iges_edge_use *edge);
extern struct iges_vertex *Get_iges_vertex(struct vertex *v);
extern struct iges_vertex_list *Get_vertex_list(int vert_de);
extern struct iges_vertex_list *Read_vertex_list(int vert_de);
extern struct loopuse *Make_loop(size_t entityno, int orientation, int on_surf_de, struct face_g_snurb *surf, struct faceuse *fu);
extern struct shell *Add_inner_shell(struct nmgregion *r, size_t entityno, struct bu_list *vlfree);
extern struct shell *Get_outer_shell(struct nmgregion *r, size_t entityno, struct bu_list *vlfree);
extern struct face_g_snurb *Get_nurb_surf(size_t entityno, struct model *m);
extern struct vertex **Get_vertex(struct iges_edge_use *edge);
extern union tree *do_nmg_region_end(struct db_tree_state *tsp, const struct db_full_path *pathp, union tree *curtree);
extern void count_refs(struct db_i *dbip, struct directory *dp);
extern void csg_comb_func(struct db_i *dbip, struct directory *dp);
extern void csg_leaf_func(struct db_i *dbip, struct directory *dp);
extern void nmg_region_edge_list(struct bu_ptbl *tab, struct nmgregion *r);
extern void set_iges_tolerances(struct bn_tol *set_tol, struct bg_tess_tol *set_ttol);
extern void w_start_global(FILE *fp_dir, FILE *fp_param, char *db_name, char *prog_name, char *output_file, char *id, char *version);
extern void w_terminate(FILE *fp);
extern void write_edge_list(struct nmgregion *r, int vert_de, struct bu_ptbl *etab, struct bu_ptbl *vtab, FILE *fp_dir, FILE *fp_param);
extern void write_vertex_list(struct nmgregion *r, struct bu_ptbl *vtab, FILE *fp_dir, FILE *fp_param);
extern struct iges_edge *Get_edge(struct iges_edge_use *e_use);
extern struct vertex *Get_edge_start_vertex(struct iges_edge *e);
extern struct vertex *Get_edge_end_vertex(struct iges_edge *e);
extern void usage(const char *);
extern void Initstack(void);
extern void Push(union tree *ptr);
extern union tree *Pop(void);
extern void Freestack(void);
extern int Recsize(void);
extern void Zero_counts(void);
extern void Summary(void);
extern void Readstart(void);
extern void Readglobal(int file_count);
extern int Findp(void);
extern void Free_dir(void);
extern void Makedir(void);
extern void Docolor(void);
extern void Gett_att(void);
extern void Evalxform(void);
extern void Check_names(void);
extern void Conv_drawings(struct bu_list *vlfree);
extern void Do_subfigs(void);
extern void Convtrimsurfs(struct bu_list *vlfree);
extern void Convsurfs(void);
extern void Convinst(void);
extern void Convsolids(struct bu_list *vlfree);
extern void Get_att(void);
extern void Convtree(void);
extern void Convassem(void);
extern int Readrec(int recno);
extern void Readint(int *inum, char *id);
extern void Readflt(fastf_t *inum, char *id);
extern void Readdbl(double *inum, char *id);
extern void Readstrg(char *id);
extern void Readname(char **ptr, char *id);
extern void Readcnv(fastf_t *inum, char *id);
extern void Assign_surface_to_fu(struct faceuse *fu, struct face_g_snurb *srf);
extern void Assign_cnurb_to_eu(struct edgeuse *eu, struct edge_g_cnurb *crv);
extern int Put_vertex(struct vertex *v, struct iges_edge_use *edge);
extern int Getcurve(size_t curve, struct ptlist **curv_pts);
extern void Orient_loops(struct nmgregion *r, struct bu_list *vlfree);
extern int spline(size_t entityno, struct face_g_snurb **b_patch);
extern void Freeknots(void);

/* temp defs while working on replacing local function Matmult with libbn functions */
/* #define USE_BN_MULT_ */
#if defined(USE_BN_MULT_)
#include "bn.h"
#else
extern void Matmult(mat_t a, mat_t b, mat_t c);
#endif

extern int Extrudcon(size_t entityno, int curve, vect_t evect);
extern int Extrudcirc(size_t entityno, int curve, vect_t evect);
extern void Read_att(size_t att_de, struct brlcad_att *att);
extern int block(size_t entityno);
extern int wedge(size_t entityno);
extern int cyl(size_t entityno);
extern int cone(size_t entityno);
extern int sphere(size_t entityno);
extern int torus(size_t entityno);
extern int revolve(size_t entityno);
extern int extrude(size_t entityno, struct bu_list *vlfree);
extern int ell(size_t entityno);
extern int brep(size_t entityno, struct bu_list *vlfree);
extern void Readtime(char *id);
extern void Readcols(char *id, int cols);
extern void Readmatrix(int xform, mat_t rot);

#endif /* CONV_IGES_IGES_STRUCT_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

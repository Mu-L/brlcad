/*                     P H O T O N M A P . C
 * BRL-CAD
 *
 * Copyright (c) 2002-2025 United States Government as represented by
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
/** @file liboptical/photonmap.c
 *
 * Implementation of Photon Mapping
 *
 */

#include "common.h"

#include <limits.h>
#include <stdlib.h>
#include <time.h>

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef HAVE_ALARM
#  include <signal.h>
#endif

#include "bu/parallel.h"
#include "photonmap.h"

#if defined(HAVE_SRAND48) && !defined(HAVE_DECL_SRAND48)
extern void srand48(long int seedval);
#endif

int PM_Activated;
int PM_Visualize;

struct PhotonMap *PMap[PM_MAPS];/* Photon Map (KD-TREE) */
struct Photon *Emit[PM_MAPS];	/* Emitted Photons */
struct Photon CurPh;
vect_t BBMin;			/* Min Bounding Box */
vect_t BBMax;			/* Max Bounding Box */
int Depth;			/* Used to determine how many times the photon has propagated */
int PType;			/* Used to determine the type of Photon: Direct, Indirect, Specular, Caustic */
int PInit;
int EPL;			/* Emitted Photons For the Light */
int EPS[PM_MAPS];		/* Emitted Photons For the Light */
int ICSize;
double ScaleFactor;
struct IrradCache *IC;		/* Irradiance Cache for Hypersampling */
char *Map;			/* Used for Irradiance HyperSampling Cache */
int GPM_IH;			/* Irradiance Hypersampling Toggle, 0=off, 1=on */
int GPM_WIDTH;
int GPM_HEIGHT;
int GPM_RAYS;			/* Number of Sample Rays for each Direction in Irradiance Hemi */
double GPM_ATOL;		/* Angular Tolerance for Photon Gathering */
struct resource GPM_RTAB[MAX_PSW];	/* Resource Table for Multi-threading */
int HitG, HitB;


/* Split so that equal numbers are above and below the splitting plane */
int
FindMedian(struct Photon *List, int Num, int Axis)
{
    int i;
    fastf_t Min, Max, Mean;

    Min = Max = List[0].Pos[Axis];
    for (i = 1; i < Num; i++) {
	if (List[i].Pos[Axis] < Min)
	    Min = List[i].Pos[Axis];
	if (List[i].Pos[Axis] > Max)
	    Max = List[i].Pos[Axis];
    }
    Mean = (Min+Max)/2.0;
    i = 0;
    while (List[i].Pos[Axis] < Mean && i < Num)
	i++;

    return i;
}


/* Generate a KD-Tree from a Flat Array of Photons */
void
BuildTree(struct Photon *EList, int ESize, struct PNode *Root)
{
    struct Photon *LList, *RList;
    vect_t Min, Max;
    int i, Axis, MedianIndex, LInd, RInd;


    /* Allocate memory for left and right lists */
    LList = (struct Photon*)bu_calloc(ESize, sizeof(struct Photon), "LList");
    RList = (struct Photon*)bu_calloc(ESize, sizeof(struct Photon), "RList");

    /* Find the Bounding volume of the Current list of photons */
    Min[0] = Max[0] = EList[0].Pos[0];
    Min[1] = Max[1] = EList[0].Pos[1];
    Min[2] = Max[2] = EList[0].Pos[2];
    for (i = 1; i < ESize; i++) {
	if (EList[i].Pos[0] < Min[0]) Min[0] = EList[i].Pos[0];
	if (EList[i].Pos[1] < Min[1]) Min[1] = EList[i].Pos[1];
	if (EList[i].Pos[2] < Min[2]) Min[2] = EList[i].Pos[2];
	if (EList[i].Pos[0] > Max[0]) Max[0] = EList[i].Pos[0];
	if (EList[i].Pos[1] > Max[1]) Max[1] = EList[i].Pos[1];
	if (EList[i].Pos[2] > Max[2]) Max[2] = EList[i].Pos[2];
    }

    /* Obtain splitting Axis, which is the largest dimension of the bounding volume */
    Max[0] -= Min[0];
    Max[1] -= Min[1];
    Max[2] -= Min[2];
    Axis = 0;
    if (Max[1] > Max[0] && Max[1] > Max[2]) Axis = 1;
    if (Max[2] > Max[0] && Max[2] > Max[1]) Axis = 2;

    /* Find Median Photon to split by. */
    MedianIndex = FindMedian(EList, ESize, Axis);

    /* Build Left and Right Lists and make sure the Median Photon is not included in either list. */
    LInd = RInd = 0;
    for (i = 0; i < ESize; i++) {
	if (i != MedianIndex) {
	    if (EList[i].Pos[Axis] < EList[MedianIndex].Pos[Axis]) {
		LList[LInd++] = EList[i];
	    } else {
		RList[RInd++] = EList[i];
	    }
	    /*
	      if (EList[i].Pos[Axis] < Median.Pos[Axis]) {
	      LList[LInd++] = EList[i];
	      } else {
	      RList[RInd++] = EList[i];
	      }
	    */
	}
    }

    /* Store the Median Photon into the KD-Tree. */
    /* bu_log("insertKD: %.3f, %.3f, %.3f\n", EList[MedianIndex].Pos[0], EList[MedianIndex].Pos[1], EList[MedianIndex].Pos[2]);*/
    Root->P = EList[MedianIndex];
    Root->P.Axis = Axis;
    Root->C = 0;

    /* With Left and Right if either contain any photons then repeat this process */
    /* if (LInd) bu_log("Left Branch\n");*/
    if (LInd) {
	BU_ALLOC(Root->L, struct PNode);
	Root->L->L = 0;
	Root->L->R = 0;
	BuildTree(LList, LInd, Root->L);
    }
    /* if (RInd) bu_log("Right Branch\n");*/
    if (RInd) {
	BU_ALLOC(Root->R, struct PNode);
	Root->R->L = 0;
	Root->R->R = 0;
	BuildTree(RList, RInd, Root->R);
    }

    bu_free(LList, "LList");
    bu_free(RList, "RList");
}


void
LocatePhotons(struct PhotonSearch *Search, struct PNode *Root)
{
    fastf_t Dist, TDist, angle, MDist;
    int i, MaxInd, Axis;

    if (!Root)
	return;

    Axis = Root->P.Axis;
    Dist = Search->Pos[Axis] - Root->P.Pos[Axis];

    if (Dist < 0) {
	/* Left of plane - search left subtree first */
	LocatePhotons(Search, Root->L);
	if (Dist*Dist < Search->RadSq)
	    LocatePhotons(Search, Root->R);
    } else {
	/* Right of plane - search right subtree first */
	LocatePhotons(Search, Root->R);
	if (Dist*Dist < Search->RadSq)
	    LocatePhotons(Search, Root->L);
    }

    /* REPLACE, Find Distance between Root Photon and NP->Pos */
    Dist = (Root->P.Pos[0] - Search->Pos[0])*(Root->P.Pos[0] - Search->Pos[0]) + (Root->P.Pos[1] - Search->Pos[1])*(Root->P.Pos[1] - Search->Pos[1]) + (Root->P.Pos[2] - Search->Pos[2])*(Root->P.Pos[2] - Search->Pos[2]);

    angle = VDOT(Search->Normal, Root->P.Normal);
    if (Dist < Search->RadSq && angle > GPM_ATOL) {
	/* Check that Result is within Radius and Angular Tolerance */
	/* if (Dist < NP->RadSq) {*/
	if (Search->Found < Search->Max) {
	    Search->List[Search->Found++].P = Root->P;
	} else {
	    MDist = (Search->Pos[0] - Search->List[0].P.Pos[0])*(Search->Pos[0] - Search->List[0].P.Pos[0])+(Search->Pos[1] - Search->List[0].P.Pos[1])*(Search->Pos[1] - Search->List[0].P.Pos[1])+(Search->Pos[2] - Search->List[0].P.Pos[2])*(Search->Pos[2] - Search->List[0].P.Pos[2]);
	    MaxInd = 0;
	    for (i = 1; i < Search->Found; i++) {
		TDist = (Search->Pos[0] - Search->List[i].P.Pos[0])*(Search->Pos[0] - Search->List[i].P.Pos[0])+(Search->Pos[1] - Search->List[i].P.Pos[1])*(Search->Pos[1] - Search->List[i].P.Pos[1])+(Search->Pos[2] - Search->List[i].P.Pos[2])*(Search->Pos[2] - Search->List[i].P.Pos[2]);
		if (TDist > MDist) {
		    MDist = TDist;
		    MaxInd = i;
		}
	    }

	    if (Dist < MDist)
		Search->List[MaxInd].P = Root->P;
	}
    }
}


/* Places photon into flat array that wwill form the final kd-tree. */
void
Store(point_t Pos, vect_t Dir, vect_t Normal, int map)
{
    struct PhotonSearch Search;
    int i;

    /* If Importance Mapping is enabled, Check to see if the Photon is in an area that is considered important, if not then disregard it */
    if (map != PM_IMPORTANCE && PMap[PM_IMPORTANCE]->StoredPhotons) {
	/* Do a KD-Tree lookup and if the photon is within a distance of sqrt(ScaleFactor) from the nearest importon then keep it, otherwise discard it */

	Search.RadSq = ScaleFactor;
	Search.Found = 0;
	Search.Max = 1;
	Search.Pos[0] = Pos[0];
	Search.Pos[1] = Pos[1];
	Search.Pos[2] = Pos[2];
	Search.Normal[0] = Normal[0];
	Search.Normal[1] = Normal[1];
	Search.Normal[2] = Normal[2];

	Search.List = (struct PSN*)bu_calloc(Search.Max, sizeof(struct PSN), "Search.List");
	LocatePhotons(&Search, PMap[PM_IMPORTANCE]->Root);
	bu_free(Search.List, "Search.List");

	if (!Search.Found) {
	    HitB++;
	    return;
	}
    }


    if (PMap[map]->StoredPhotons < PMap[map]->MaxPhotons) {
	HitG++;
	for (i = 0; i < 3; i++) {
	    /* Store Position, Direction, and Power of Photon */
	    Emit[map][PMap[map]->StoredPhotons].Pos[i] = Pos[i];
	    Emit[map][PMap[map]->StoredPhotons].Dir[i] = Dir[i];
	    Emit[map][PMap[map]->StoredPhotons].Normal[i] = Normal[i];
	    Emit[map][PMap[map]->StoredPhotons].Power[i] = CurPh.Power[i];
	}
	PMap[map]->StoredPhotons++;
	/*
	  if (map != PM_IMPORTANCE && PMap[PM_IMPORTANCE]->StoredPhotons < PMap[PM_IMPORTANCE]->MaxPhotons)
	  bu_log("Map2: %d, Size: %d\n", map, PMap[map]->StoredPhotons);
	*/
	/*
	  if (map == PM_IMPORTANCE)
	  bu_log("Map: %d, Size: %d, [%.3f, %.3f, %.3f] [%.3f, %.3f, %.3f]\n", map, PMap[map]->StoredPhotons, Pos[0], Pos[1], Pos[2], CurPh.Power[0], CurPh.Power[1], CurPh.Power[2]);
	*/
    }

    /*
      bu_log("[%d][%d][%.3f, %.3f, %.3f]\n", map, PMap[map]->StoredPhotons, CurPh.Power[0], CurPh.Power[1], CurPh.Power[2]);
      if (!(PMap[map]->StoredPhotons % 64))
      bu_log("[%d][%d][%.3f, %.3f, %.3f]\n", map, PMap[map]->StoredPhotons, Pos[0], Pos[1], Pos[2]);
    */
}


/* Compute a specular reflection */
void
SpecularReflect(vect_t normal, vect_t rdir)
{
    vect_t d;
    fastf_t dot;

    VSCALE(d, rdir, -1);
    dot = VDOT(d, normal);

    if (dot < 0.0f) {
	rdir[0] = rdir[1] = rdir[2] = 0;
    } else {
	VSCALE(rdir, normal, 2*dot);
	VSUB2(rdir, rdir, d);
    }
}


/* Compute a random reflected diffuse direction */
void
DiffuseReflect(vect_t normal, vect_t rdir)
{
    /* Allow Photons to get a random direction at most 60 degrees to the normal */
    do {
#ifndef HAVE_DRAND48
	rdir[0] = 2.0*rand()/(double)RAND_MAX-1.0;
	rdir[1] = 2.0*rand()/(double)RAND_MAX-1.0;
	rdir[2] = 2.0*rand()/(double)RAND_MAX-1.0;
#else
	rdir[0] = 2.0*drand48()-1.0;
	rdir[1] = 2.0*drand48()-1.0;
	rdir[2] = 2.0*drand48()-1.0;
#endif
	VUNITIZE(rdir);
    } while (VDOT(rdir, normal) < 0.5);
}


/* Compute refracted ray given Incident Ray, Normal, and 2 refraction indices */
int
Refract(vect_t I, vect_t N, fastf_t n1, fastf_t n2)
{
    fastf_t n, c1, c2, radicand;
    vect_t t, r;

    n = n1/n2;
    c1 = -VDOT(I, N);
    radicand = 1.0 - (n*n)*(1.0-c1*c1);
    if (radicand < 0) {
	/* Total Internal Reflection */
	I[0] = I[1] = I[2] = 0;
	return 0;
    }
    c2 = sqrt(radicand);

    VSCALE(r, I, n);
    VSCALE(t, N, n*c1-c2);
    VADD2(I, r, t);
    return 1;
}


int
CheckMaterial(char *cmp, char *MS)
{
    size_t i;

    if (MS) {
	for (i = 0; i < strlen(cmp) && i < strlen(MS); i++)
	    if (MS[i] != cmp[i])
		return 0;
	return 1;
    } else {
	return 0;
    }
}


/* This function parses the material string to obtain specular and refractive values */
void
GetMaterial(char *MS, vect_t spec, fastf_t *refi, fastf_t *transmit)
{
    struct phong_specific *phong_sp;
    struct bu_vls matparm = BU_VLS_INIT_ZERO;

    BU_GET(phong_sp, struct phong_specific);

    /* Initialize spec and refi */
    spec[0] = spec[1] = spec[2] = *refi = *transmit = 0;
    if (CheckMaterial("plastic", MS)) {
	/* Checks that the first 7 chars match any of the characters found in plastic */
	/* Plastic Shader */
	phong_sp->magic = PL_MAGIC;
	phong_sp->shine = 10;
	phong_sp->wgt_specular = 0.7;
	phong_sp->wgt_diffuse = 0.3;
	phong_sp->transmit = 0.0;
	phong_sp->reflect = 0.0;
	phong_sp->refrac_index = 1.0;
	phong_sp->extinction = 0.0;

	MS += 7;
	bu_vls_printf(&matparm, "%s", MS);
	if (bu_struct_parse(&matparm, phong_parse, (char *)phong_sp, NULL) < 0)
	  bu_log("Warning - bu_struct_parse failure (matparm, phone_parse, material = plastic) in GetMaterial!\n");
	bu_vls_free(&matparm);

	/* bu_log("string: %s\n", MS);*/
	/* bu_log("info[%d]: sh: %d, spec: %.3f, diff: %.3f, tran: %.3f, refl: %.3f, refr: %.3f\n", PMap->StoredPhotons, phong_sp->shine, phong_sp->wgt_specular, phong_sp->wgt_diffuse, phong_sp->transmit, phong_sp->reflect, phong_sp->refrac_index);*/
	/* bu_log("spec[%d]: %.3f\n", PMap->StoredPhotons, phong_sp->wgt_specular);*/

	spec[0] = spec[1] = spec[2] = phong_sp->wgt_specular;
	*refi = phong_sp->refrac_index;
	spec[0] = spec[1] = spec[2] = 0.7;
	/*
	 *refi = 1.0;
	 *transmit = 0.0;
	 */
	/*
	  if (phong_sp->refrac_index != 1.0)
	  bu_log("refi: %.3f\n", phong_sp->refrac_index);
	*/
    } else if (CheckMaterial("glass", MS)) {
	/* Glass Shader */
	phong_sp->magic = PL_MAGIC;
	phong_sp->shine = 4;
	phong_sp->wgt_specular = 0.7;
	phong_sp->wgt_diffuse = 0.3;
	phong_sp->transmit = 0.8;
	phong_sp->reflect = 0.1;
	phong_sp->refrac_index = 1.65;
	phong_sp->extinction = 0.0;

	MS += 5; /* move pointer past "pm " (3 characters) */
	bu_vls_printf(&matparm, "%s", MS);
	if (bu_struct_parse(&matparm, phong_parse, (char *)phong_sp, NULL) < 0)
	  bu_log("Warning - bu_struct_parse failure (matparm, phone_parse, material = glass) in GetMaterial!\n");
	bu_vls_free(&matparm);

	/* bu_log("string: %s\n", MS);*/
	/* bu_log("info[%d]: sh: %d, spec: %.3f, diff: %.3f, tran: %.3f, refl: %.3f, refr: %.3f\n", PMap->StoredPhotons, phong_sp->shine, phong_sp->wgt_specular, phong_sp->wgt_diffuse, phong_sp->transmit, phong_sp->reflect, phong_sp->refrac_index);*/
	/* bu_log("spec[%d]: %.3f\n", PMap->StoredPhotons, phong_sp->wgt_specular);*/

	spec[0] = spec[1] = spec[2] = phong_sp->wgt_specular;
	*refi = phong_sp->refrac_index;
	*transmit = phong_sp->transmit;

	/*
	  if (phong_sp->refrac_index != 1.0)
	  bu_log("refi: %.3f\n", phong_sp->refrac_index);
	*/
    }

    BU_PUT(phong_sp, struct phong_specific);
}


static fastf_t
MaxFloat(fastf_t a, fastf_t b, fastf_t c)
{
    return a > b ? a > c ? a : c : b > c ? b : c;
}


/* break PHit <-> HitRef cycle */
int PHit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(finished_segs));

int
HitRef(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(finished_segs))
{
    struct partition *part;
    vect_t pt, normal, spec;
    fastf_t refi, transmit;

    ap->a_hit = PHit;
    part = PartHeadp->pt_forw;

    /* Compute Intersection Point, Pt = o + td */
    VJOIN1(pt, ap->a_ray.r_pt, part->pt_outhit->hit_dist, ap->a_ray.r_dir);

    /* Fetch Intersection Normal */
    RT_HIT_NORMAL(normal, part->pt_inhit, part->pt_inseg->seg_stp, &(ap->a_ray), part->pt_inflip);
    /* RT_HIT_NORMAL(normal, part->pt_outhit, part->pt_inseg->seg_stp, &(ap->a_ray), part->pt_inflip);*/
    /* RT_HIT_NORMAL(normal, part->pt_outhit, part->pt_outseg->seg_stp, &(ap->a_ray), part->pt_outflip);*/

    /* Assign pt */
    ap->a_ray.r_pt[0] = pt[0];
    ap->a_ray.r_pt[1] = pt[1];
    ap->a_ray.r_pt[2] = pt[2];


    /* Fetch Material */
    GetMaterial(part->pt_regionp->reg_mater.ma_shader, spec, &refi, &transmit);


    if (Refract(ap->a_ray.r_dir, normal, refi, 1.0)) {
	/*
	  bu_log("1D: %d, [%.3f, %.3f, %.3f], [%.3f, %.3f, %.3f], [%.3f, %.3f, %.3f]\n", Depth, pt[0], pt[1], pt[2], ap->a_ray.r_dir[0], ap->a_ray.r_dir[1], ap->a_ray.r_dir[2], normal[0], normal[1], normal[2]);
	  bu_log("p1: [%.3f, %.3f, %.3f]\n", part->pt_inhit->hit_point[0], part->pt_inhit->hit_point[1], part->pt_inhit->hit_point[2]);
	  bu_log("p2: [%.3f, %.3f, %.3f]\n", part->pt_outhit->hit_point[0], part->pt_outhit->hit_point[1], part->pt_outhit->hit_point[2]);
	*/
	Depth++;
	rt_shootray(ap);
    } else {
	bu_log("TIF\n");
    }

    ap->a_onehit = 0;
    return 1;
}

//#define PHIT_DEBUG
/* Callback for Photon Hit, The 'current' photon is Emit[PMap->StoredPhotons] */
int
PHit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(finished_segs))
{
    struct partition *part;
    vect_t pt, normal, color, spec, power;
    fastf_t refi, transmit, prob, prob_diff, prob_spec, prob_ref;
#ifdef PHIT_DEBUG
    int hit;
#endif


    /* Move ptr forward to next region and call Hit recursively until reaching a region this is either
       not a light or none at all */
#ifdef PHIT_DEBUG
    hit = 0;
#endif
    for (BU_LIST_FOR(part, partition, (struct bu_list *)PartHeadp)) {
	if (part != PartHeadp) {
#ifdef PHIT_DEBUG
	    hit++;
#endif
	    VJOIN1(pt, ap->a_ray.r_pt, part->pt_inhit->hit_dist, ap->a_ray.r_dir);
#ifdef PHIT_DEBUG
	    printf("pt[%d][%d]: --- [%.3f, %.3f, %.3f], %s\n", hit, CheckMaterial("light", part->pt_regionp->reg_mater.ma_shader), pt[0], pt[1], pt[2], part->pt_regionp->reg_mater.ma_shader);
#endif

	    if (!CheckMaterial("light", part->pt_regionp->reg_mater.ma_shader)) {
		/* bu_log("  Found object!\n");*/
		break;
	    }
	}
    }

    if (!part || part == PartHeadp)
	return 0;


    if (CheckMaterial("light", part->pt_regionp->reg_mater.ma_shader))
	return 0;


    /* Compute Intersection Point, Pt = o + td */
    VJOIN1(pt, ap->a_ray.r_pt, part->pt_inhit->hit_dist, ap->a_ray.r_dir);


    /* Generate Bounding Box for Scaling Phase */
    if (PInit) {
	BBMin[0] = BBMax[0] = pt[0];
	BBMin[1] = BBMax[1] = pt[1];
	BBMin[2] = BBMax[2] = pt[2];
	PInit = 0;
    } else {
	if (pt[0] < BBMin[0])
	    BBMin[0] = pt[0];
	if (pt[0] > BBMax[0])
	    BBMax[0] = pt[0];
	if (pt[1] < BBMin[1])
	    BBMin[1] = pt[1];
	if (pt[1] > BBMax[1])
	    BBMax[1] = pt[1];
	if (pt[2] < BBMin[2])
	    BBMin[2] = pt[2];
	if (pt[2] > BBMax[2])
	    BBMax[2] = pt[2];
    }

    /* Fetch Intersection Normal */
    RT_HIT_NORMAL(normal, part->pt_inhit, part->pt_inseg->seg_stp, &(ap->a_ray), part->pt_inflip);

    /* Fetch Material */
    GetMaterial(part->pt_regionp->reg_mater.ma_shader, spec, &refi, &transmit);
    /* bu_log("Spec: [%.3f, %.3f, %.3f], Refi: %.3f\n", spec[0], spec[1], spec[2], refi);*/


    /* Compute Diffuse, Specular, and Caustics */
    color[0] = part->pt_regionp->reg_mater.ma_color[0];
    color[1] = part->pt_regionp->reg_mater.ma_color[1];
    color[2] = part->pt_regionp->reg_mater.ma_color[2];

    prob_ref = MaxFloat(color[0]+spec[0], color[1]+spec[1], color[2]+spec[2]);
    prob_diff = ((color[0]+color[1]+color[2])/(color[0]+color[1]+color[2]+spec[0]+spec[1]+spec[2]))*prob_ref;
    prob_spec = prob_ref - prob_diff;
#ifndef HAVE_DRAND48
    prob = rand()/(double)RAND_MAX;
#else
    prob = drand48();
#endif

    /* bu_log("pr: %.3f, pd: %.3f, [%.3f, %.3f, %.3f] [%.3f, %.3f, %.3f]\n", prob_ref, prob_diff, color[0], color[1], color[2], spec[0], spec[1], spec[2]);*/
    /* bu_log("prob: %.3f, prob_diff: %.3f, pd+ps: %.3f\n", prob, prob_diff, prob_diff+prob_spec);*/

    if (prob < 1.0 - transmit) {
	if (prob < prob_diff) {
	    /* Store power of incident Photon */
	    power[0] = CurPh.Power[0];
	    power[1] = CurPh.Power[1];
	    power[2] = CurPh.Power[2];


	    /* Scale Power of reflected photon */
	    CurPh.Power[0] = power[0]*color[0]/prob_diff;
	    CurPh.Power[1] = power[1]*color[1]/prob_diff;
	    CurPh.Power[2] = power[2]*color[2]/prob_diff;

	    /* Store Photon */
	    Store(pt, ap->a_ray.r_dir, normal, PType);

	    /* Assign diffuse reflection direction */
	    DiffuseReflect(normal, ap->a_ray.r_dir);

	    /* Assign pt */
	    ap->a_ray.r_pt[0] = pt[0];
	    ap->a_ray.r_pt[1] = pt[1];
	    ap->a_ray.r_pt[2] = pt[2];

	    if (PType != PM_CAUSTIC) {
		Depth++;
		rt_shootray(ap);
	    }
	} else if (prob >= prob_diff && prob < prob_diff + prob_spec) {
	    /* Store power of incident Photon */
	    power[0] = CurPh.Power[0];
	    power[1] = CurPh.Power[1];
	    power[2] = CurPh.Power[2];

	    /* Scale power of reflected photon */
	    CurPh.Power[0] = power[0]*spec[0]/prob_spec;
	    CurPh.Power[1] = power[1]*spec[1]/prob_spec;
	    CurPh.Power[2] = power[2]*spec[2]/prob_spec;

	    /* Reflective */
	    SpecularReflect(normal, ap->a_ray.r_dir);

	    /* Assign pt */
	    ap->a_ray.r_pt[0] = pt[0];
	    ap->a_ray.r_pt[1] = pt[1];
	    ap->a_ray.r_pt[2] = pt[2];

	    if (PType != PM_IMPORTANCE)
		PType = PM_CAUSTIC;
	    Depth++;
	    rt_shootray(ap);
	} else {
	    /* Store Photon */
	    Store(pt, ap->a_ray.r_dir, normal, PType);
	}
    } else {
	if (refi > 1.0 && (PType == PM_CAUSTIC || Depth == 0)) {
	    if (PType != PM_IMPORTANCE)
		PType = PM_CAUSTIC;

	    /* Store power of incident Photon */
	    power[0] = CurPh.Power[0];
	    power[1] = CurPh.Power[1];
	    power[2] = CurPh.Power[2];

	    /* Scale power of reflected photon */
	    CurPh.Power[0] = power[0]*spec[0]/prob_spec;
	    CurPh.Power[1] = power[1]*spec[1]/prob_spec;
	    CurPh.Power[2] = power[2]*spec[2]/prob_spec;

	    /* Refractive or Reflective */
	    if (refi > 1.0 && prob < transmit) {
		CurPh.Power[0] = power[0];
		CurPh.Power[1] = power[1];
		CurPh.Power[2] = power[2];

		if (!Refract(ap->a_ray.r_dir, normal, 1.0, refi))
		    printf("TIF0\n");

		ap->a_hit = HitRef;

		/*
		  bu_log("dir: [%.3f, %.3f, %.3f]\n", ap->a_ray.r_dir[0], ap->a_ray.r_dir[1], ap->a_ray.r_dir[2]);
		  bu_log("ref: [%.3f, %.3f, %.3f]\n", ap->a_ray.r_dir[0], ap->a_ray.r_dir[1], ap->a_ray.r_dir[2]);
		  bu_log("nor: [%.3f, %.3f, %.3f]\n", normal[0], normal[1], normal[2]);
		  bu_log("p0: [%.3f, %.3f, %.3f], [%.3f, %.3f, %.3f]\n", pt[0], pt[1], pt[2], ap->a_ray.r_dir[0], ap->a_ray.r_dir[1], ap->a_ray.r_dir[2]);
		*/
		ap->a_onehit = 0;
	    } else {
		SpecularReflect(normal, ap->a_ray.r_dir);
	    }

	    /* Assign pt */
	    ap->a_ray.r_pt[0] = pt[0];
	    ap->a_ray.r_pt[1] = pt[1];
	    ap->a_ray.r_pt[2] = pt[2];

	    /* bu_log("2D: %d, [%.3f, %.3f, %.3f], [%.3f, %.3f, %.3f], [%.3f, %.3f, %.3f]\n", Depth, pt[0], pt[1], pt[2], ap->a_ray.r_dir[0], ap->a_ray.r_dir[1], ap->a_ray.r_dir[2], normal[0], normal[1], normal[2]);*/
	    Depth++;
	    rt_shootray(ap);
	}
    }
    return 1;
}


/* Callback for Photon Miss */
int
PMiss(struct application *UNUSED(ap))
{
    return 0;
}


/* ScalePhotonPower() is used to scale the power of all photons once they
 * have been emitted from the light source.  Scale = 1/(#emitted photons).
 * Call this function after each light source is processed.
 * This function also handles setting a default power for the photons based
 * on the size of the scene, i.e. power of light source */
void
ScalePhotonPower(int map)
{
    int i;

    for (i = 0; i < PMap[map]->StoredPhotons; i++) {
	Emit[map][i].Power[0] *= ScaleFactor/(double)EPS[map];
	Emit[map][i].Power[1] *= ScaleFactor/(double)EPS[map];
	Emit[map][i].Power[2] *= ScaleFactor/(double)EPS[map];
    }
}


/* Generate Importons and emit them into the scene from the eye position */
void
EmitImportonsRandom(struct application *ap, point_t eye_pos)
{
    while (PMap[PM_IMPORTANCE]->StoredPhotons < PMap[PM_IMPORTANCE]->MaxPhotons) {
	do {
	    /* Set Ray Direction to application ptr */
#ifndef HAVE_DRAND48
	    ap->a_ray.r_dir[0] = 2.0*rand()/(double)RAND_MAX-1.0;
	    ap->a_ray.r_dir[1] = 2.0*rand()/(double)RAND_MAX-1.0;
	    ap->a_ray.r_dir[2] = 2.0*rand()/(double)RAND_MAX-1.0;
#else
	    ap->a_ray.r_dir[0] = 2.0*drand48()-1.0;
	    ap->a_ray.r_dir[1] = 2.0*drand48()-1.0;
	    ap->a_ray.r_dir[2] = 2.0*drand48()-1.0;
#endif
	} while (ap->a_ray.r_dir[0]*ap->a_ray.r_dir[0] + ap->a_ray.r_dir[1]*ap->a_ray.r_dir[1] + ap->a_ray.r_dir[2]*ap->a_ray.r_dir[2] > 1);

	/* Normalize Ray Direction */
	VUNITIZE(ap->a_ray.r_dir);

	/* Set Ray Position to application ptr */
	ap->a_ray.r_pt[0] = eye_pos[0];
	ap->a_ray.r_pt[1] = eye_pos[1];
	ap->a_ray.r_pt[2] = eye_pos[2];

	/* Shoot Importon into Scene */
	CurPh.Power[0] = 0;
	CurPh.Power[1] = 100000000;
	CurPh.Power[2] = 0;

	Depth = 0;
	PType = PM_IMPORTANCE;
	rt_shootray(ap);
    }
}


/* Emit a photons in a random direction based on a point light */
void
EmitPhotonsRandom(struct application *ap, double ScaleIndirect)
{
    struct light_specific *lp;
    int i;

    /*
      for (i = 0; i < 8; i++)
      bu_log("sample points: [%.3f, %.3f, %.3f]\n", lp->lt_sample_pts[i].lp_pt[0], lp->lt_sample_pts[i].lp_pt[1], lp->lt_sample_pts[i].lp_pt[2]);
    */
    while (1) {
	for (BU_LIST_FOR(lp, light_specific, &(LightHead.l))) {
	    /* If the Global Photon Map Completes before the Caustics Map, then it probably means there are no caustic objects in the Scene */
	    if (PMap[PM_GLOBAL]->StoredPhotons == PMap[PM_GLOBAL]->MaxPhotons && (!PMap[PM_CAUSTIC]->StoredPhotons || PMap[PM_CAUSTIC]->StoredPhotons == PMap[PM_CAUSTIC]->MaxPhotons))
		return;

	    do {
		/* do {*/
		/* Set Ray Direction to application ptr */
#ifndef HAVE_DRAND48
		ap->a_ray.r_dir[0] = 2.0*rand()/(double)RAND_MAX-1.0;
		ap->a_ray.r_dir[1] = 2.0*rand()/(double)RAND_MAX-1.0;
		ap->a_ray.r_dir[2] = 2.0*rand()/(double)RAND_MAX-1.0;
#else
		ap->a_ray.r_dir[0] = 2.0*drand48()-1.0;
		ap->a_ray.r_dir[1] = 2.0*drand48()-1.0;
		ap->a_ray.r_dir[2] = 2.0*drand48()-1.0;
#endif
	    } while (ap->a_ray.r_dir[0]*ap->a_ray.r_dir[0] + ap->a_ray.r_dir[1]*ap->a_ray.r_dir[1] + ap->a_ray.r_dir[2]*ap->a_ray.r_dir[2] > 1);
	    /* Normalize Ray Direction */
	    VUNITIZE(ap->a_ray.r_dir);

	    /* Set Ray Position to application ptr */
	    ap->a_ray.r_pt[0] = lp->lt_pos[0];
	    ap->a_ray.r_pt[1] = lp->lt_pos[1];
	    ap->a_ray.r_pt[2] = lp->lt_pos[2];


	    /* Shoot Photon into Scene, (4.0) is used to align phong's attenuation with photonic energies, it's a heuristic */
	    /*bu_log("Shooting Ray: [%.3f, %.3f, %.3f] [%.3f, %.3f, %.3f]\n", lp->lt_pos[0], lp->lt_pos[1], lp->lt_pos[2], x, y, z);*/
	    CurPh.Power[0] = 1000.0 * ScaleIndirect * lp->lt_intensity * lp->lt_color[0];
	    CurPh.Power[1] = 1000.0 * ScaleIndirect * lp->lt_intensity * lp->lt_color[1];
	    CurPh.Power[2] = 1000.0 * ScaleIndirect * lp->lt_intensity * lp->lt_color[2];

	    Depth = 0;
	    PType = PM_GLOBAL;

	    EPL++;
	    for (i = 0; i < PM_MAPS; i++)
		if (PMap[i]->StoredPhotons < PMap[i]->MaxPhotons)
		    EPS[i]++;

	    rt_shootray(ap);
	    /* bu_log("1: %d, 2: %d\n", PMap[PM_GLOBAL]->StoredPhotons, PMap[PM_CAUSTIC]->StoredPhotons);*/
	}
    }
}


void
SanityCheck(struct PNode *Root, int LR)
{
    if (!Root)
	return;

    bu_log("Pos[%d]: [%.3f, %.3f, %.3f]\n", LR, Root->P.Pos[0], Root->P.Pos[1], Root->P.Pos[2]);
    SanityCheck(Root->L, 1);
    SanityCheck(Root->R, 2);
}


fastf_t
GaussFilter(fastf_t dist, fastf_t rad)
{
    return 0.918 * (1.0 - (1.0 - exp(-1.953*dist*dist/(2.0*rad*rad)))/(1.0 - exp(-1.953)));
}


fastf_t
ConeFilter(fastf_t dist, fastf_t rad)
{
    return 1.0 - dist/rad;
}


void
GetEstimate(vect_t irrad, point_t pos, vect_t normal, fastf_t rad, int np, int map, double max_rad, int centog, int min_np)
{
    struct PhotonSearch Search;
    int i;
    fastf_t tmp, dist, Filter, ScaleFilter;
    vect_t t, Centroid;


    irrad[0] = irrad[1] = irrad[2] = 0;
    if (!PMap[map]->StoredPhotons) return;

    Search.Pos[0] = pos[0];
    Search.Pos[1] = pos[1];
    Search.Pos[2] = pos[2];

    Search.RadSq = rad*rad/4.0;
    Search.Max = np < min_np ? min_np : np;
    Search.Normal[0] = normal[0];
    Search.Normal[1] = normal[1];
    Search.Normal[2] = normal[2];

    Search.List = (struct PSN*)bu_calloc(Search.Max, sizeof(struct PSN), "PSN");
    do {
	Search.Found = 0;
	Search.RadSq *= 4.0;
	LocatePhotons(&Search, PMap[map]->Root);
	if (!Search.Found && Search.RadSq > ScaleFactor*ScaleFactor/100.0)
	    break;
    } while (Search.Found < Search.Max && Search.RadSq < max_rad*max_rad);

    /* bu_log("Found: %d\n", Search.Found);*/
    if (Search.Found < min_np) {
	bu_free(Search.List, "Search.List");
	return;
    }

    /* Calculate Max Distance */
    Search.RadSq = 1;
    Centroid[0] = Centroid[1] = Centroid[2] = 0;

    for (i = 0; i < Search.Found; i++) {
	t[0] = Search.List[i].P.Pos[0] - pos[0];
	t[1] = Search.List[i].P.Pos[1] - pos[1];
	t[2] = Search.List[i].P.Pos[2] - pos[2];

	Centroid[0] += Search.List[i].P.Pos[0];
	Centroid[1] += Search.List[i].P.Pos[1];
	Centroid[2] += Search.List[i].P.Pos[2];

	dist = t[0]*t[0] + t[1]*t[1] + t[2]*t[2];
	if (dist > Search.RadSq)
	    Search.RadSq = dist;
    }

    if (Search.Found) {
	Centroid[0] /= (double)Search.Found;
	Centroid[1] /= (double)Search.Found;
	Centroid[2] /= (double)Search.Found;
    }


    /* This needs a little debugging, splotches in moss cause tmp gets too small, will look at later, ||1 to turn it off */
    if (!centog||1) {
	Centroid[0] = pos[0];
	Centroid[1] = pos[1];
	Centroid[2] = pos[2];
	ScaleFilter = 2.0;
    } else {
	ScaleFilter = 1.0;
    }

    for (i = 0; i < Search.Found; i++) {
	t[0] = Search.List[i].P.Pos[0] - pos[0];
	t[1] = Search.List[i].P.Pos[1] - pos[1];
	t[2] = Search.List[i].P.Pos[2] - pos[2];

	dist = t[0]*t[0] + t[1]*t[1] + t[2]*t[2];
	/* Filter= 0.50;*/
	/* Filter= ConeFilter(dist, NP.RadSq);*/
	Filter = 0.5*GaussFilter(dist, Search.RadSq);

	irrad[0] += Search.List[i].P.Power[0]*Filter*ScaleFilter;
	irrad[1] += Search.List[i].P.Power[1]*Filter*ScaleFilter;
	irrad[2] += Search.List[i].P.Power[2]*Filter*ScaleFilter;
    }

    t[0] = sqrt((Centroid[0] - pos[0])*(Centroid[0] - pos[0])+(Centroid[1] - pos[1])*(Centroid[1] - pos[1])+(Centroid[2] - pos[2])*(Centroid[2] - pos[2]));
    tmp = M_PI*(sqrt(Search.RadSq)-t[0])*(sqrt(Search.RadSq)-t[0]);


    irrad[0] /= tmp;
    irrad[1] /= tmp;
    irrad[2] /= tmp;

    /*
      if (irrad[0] > 10 || irrad[1] > 10 || irrad[2] > 10) {
      bu_log("found: %d, tmp: %.1f\n", Search.Found, tmp);
      }
    */
    if (map == PM_CAUSTIC) {
	tmp = (double)Search.Found/(double)Search.Max;
	irrad[0] *= tmp;
	irrad[1] *= tmp;
	irrad[2] *= tmp;
    }

    /*
      irrad[0] *= M_1_PI / NP.RadSq;
      irrad[1] *= M_1_PI / NP.RadSq;
      irrad[2] *= M_1_PI / NP.RadSq;
    */
    bu_free(Search.List, "Search.List");
    /* bu_log("Radius: %.3f, Max Phot: %d, Found: %d, Power: [%.4f, %.4f, %.4f], Pos: [%.3f, %.3f, %.3f]\n", sqrt(NP.RadSq), NP.Max, NP.Found, irrad[0], irrad[1], irrad[2], pos[0], pos[1], pos[2]);*/
}


int
ICHit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(finished_segs))
{
    struct partition *part;
    point_t pt, normal;
    vect_t C1, C2;

    part = PartHeadp->pt_forw;

    VJOIN1(pt, ap->a_ray.r_pt, part->pt_inhit->hit_dist, ap->a_ray.r_dir);

    RT_HIT_NORMAL(normal, part->pt_inhit, part->pt_inseg->seg_stp, &(ap->a_ray), part->pt_inflip);
    /* GetEstimate(C1, pt, normal, ScaleFactor/10.0, PMap[PM_GLOBAL]->StoredPhotons / 100, PM_GLOBAL, 5, 1);*/
    /* GetEstimate(C1, pt, normal, ScaleFactor/1000.0, 10.0*log(PMap[PM_GLOBAL]->StoredPhotons), PM_GLOBAL, ScaleFactor/5.0, 1);*/
    GetEstimate(C1, pt, normal, ScaleFactor/1024.0, 128, PM_GLOBAL, ScaleFactor/8.0, 1, 15);
    /* GetEstimate(C2, pt, normal, (int)(ScaleFactor/pow(2, (log(PMap[PM_CAUSTIC]->MaxPhotons/2)/log(4)))), PMap[PM_CAUSTIC]->MaxPhotons / 50, PM_CAUSTIC, 0, 0);*/
    GetEstimate(C2, pt, normal, ScaleFactor/1024.0, PMap[PM_CAUSTIC]->MaxPhotons / 100, PM_CAUSTIC, ScaleFactor/256.0, 1, 15);
    /* GetEstimate(IMColor2, pt, normal, (int)(ScaleFactor/100.0), PMap[PM_CAUSTIC]->MaxPhotons/50, PM_CAUSTIC, 1, 0);*/

    (*(vect_t*)ap->a_purpose)[0] += C1[0] + C2[0];
    (*(vect_t*)ap->a_purpose)[1] += C1[1] + C2[1];
    (*(vect_t*)ap->a_purpose)[2] += C1[2] + C2[2];

    return 1;
}


int
ICMiss(struct application *UNUSED(ap))
{
    /* Set to Background/Ambient Color later */
    return 0;
}


/* Convert a Polar vector into a euclidean vector:
 * - Requires that an orthogonal basis, so generate one using the photon normal as the first vector.
 * - The Normal passed to this function must be unitized.
 * - This took me almost 3 hours to write, and it's tight.
 */
void
Polar2Euclidian(vect_t Dir, vect_t Normal, double Theta, double Phi)
{
    int i;
    vect_t BasisX, BasisY;


    BasisX[0] = fabs(Normal[0]) < 0.001 ? 1.0 : fabs(Normal[0]) > 0.999 ? 0.0 : -(1.0/Normal[0]);
    BasisX[1] = fabs(Normal[1]) < 0.001 ? 1.0 : fabs(Normal[1]) > 0.999 ? 0.0 :  (1.0/Normal[1]);
    BasisX[2] = 0.0;
    VUNITIZE(BasisX);
    VCROSS(BasisY, Normal, BasisX);

    for (i = 0; i < 3; i++)
	Dir[i] = sin(Theta)*cos(Phi)*BasisX[i] + sin(Theta)*sin(Phi)*BasisY[i] + cos(Theta)*Normal[i];
}


/*
 * Irradiance Calculation for a given position
 */
void
Irradiance(int pid, struct Photon *P, struct application *ap)
{
    struct application *lap;		/* local application instance */
    int i, j, M, N;
    double theta, phi, Coef;

    BU_ALLOC(lap, struct application);
    RT_APPLICATION_INIT(lap);
    lap->a_rt_i = ap->a_rt_i;
    lap->a_hit = ap->a_hit;
    lap->a_miss = ap->a_miss;
    lap->a_resource = &GPM_RTAB[pid];
    lap->a_logoverlap = ap->a_logoverlap;

    M = N = GPM_RAYS;
    P->Irrad[0] = P->Irrad[1] = P->Irrad[2] = 0.0;
    for (i = 1; i <= M; i++) {
	for (j = 1; j <= N; j++) {
#ifndef HAVE_DRAND48
	    theta = asin(sqrt((j-rand()/(double)RAND_MAX)/M));
	    phi = (M_2PI)*((i-rand()/(double)RAND_MAX)/N);
#else
	    theta = asin(sqrt((j-drand48())/M));
	    phi = (M_2PI)*((i-drand48())/N);
#endif

	    /* Assign pt */
	    lap->a_ray.r_pt[0] = P->Pos[0];
	    lap->a_ray.r_pt[1] = P->Pos[1];
	    lap->a_ray.r_pt[2] = P->Pos[2];

	    /* Assign Dir */
	    Polar2Euclidian(lap->a_ray.r_dir, P->Normal, theta, phi);

	    /* Utilize the purpose pointer as a pointer to the Irradiance Color */
	    lap->a_purpose = (const char *)P->Irrad;

	    /* bu_log("Vec: [%.3f, %.3f, %.3f]\n", ap->a_ray.r_dir[0], ap->a_ray.r_dir[1], ap->a_ray.r_dir[2]);*/
	    rt_shootray(lap);

	    /* bu_log("[%.3f, %.3f, %.3f] [%.3f, %.3f, %.3f] [%.3f, %.3f, %.3f]\n", P.Pos[0], P.Pos[1], P.Pos[2], P.Normal[0], P.Normal[1], P.Normal[2], IMColor[0], IMColor[1], IMColor[2]);*/
	}
    }

    Coef = 1.0/((double)(M*N));
    P->Irrad[0] *= Coef;
    P->Irrad[1] *= Coef;
    P->Irrad[2] *= Coef;

    bu_free(lap, "app");
}


/*
 * Irradiance Cache for Indirect Illumination
 * Go through each photon and use it for the position of the hemisphere
 * and then determine whether that should be included as a Cache Pt.
 */
void
BuildIrradianceCache(int pid, struct PNode *Node, struct application *ap)
{
    static int sem_photonmap = 0;
    if (!sem_photonmap)
	sem_photonmap = bu_semaphore_register("sem_photonmap");

    if (!Node)
	return;

    /* Determine if this pt will be used by calculating a weight */
    bu_semaphore_acquire(sem_photonmap);
    if (!Node->C) {
	ICSize++;
	Node->C++;
#ifndef HAVE_ALARM
	if (!(ICSize%(PMap[PM_GLOBAL]->MaxPhotons/8)))
	    bu_log("    Irradiance Cache Progress: %d%%\n", (int)(0.5+100.0*ICSize/PMap[PM_GLOBAL]->MaxPhotons));
#endif
	bu_semaphore_release(sem_photonmap);
	Irradiance(pid, &Node->P, ap);
    } else {
	bu_semaphore_release(sem_photonmap);
    }

    BuildIrradianceCache(pid, Node->L, ap);
    BuildIrradianceCache(pid, Node->R, ap);
}


#ifdef HAVE_ALARM
static int starttime = 0;
void
alarmhandler(int sig)
{
    int t;
    float p, tl;
    if (sig != SIGALRM)
	bu_bomb("Funky signals\n");
    t = time(NULL) - starttime;
    p = (float)ICSize/PMap[PM_GLOBAL]->MaxPhotons + .015;
    tl = (float)t*1.0/p - t;
    bu_log("    Irradiance Cache Progress: %d%%  Approximate time left: %f%f",
	   (int)(100.0*p), (1.0/p-1.0)*(float)t, (float)t*1.0/p);
#define BAH(s, w) if (tl > (s)) { float d = floor(tl / (((s) == 0)?1.0:(float)(s))); \
	tl -= d * (s); bu_log("%d "w, (int)d, d>1?"s":""); }
    BAH(60*60*24, "day%s, ");
    BAH(60*60, "hour%s, ");
    BAH(60, "minute%s, ");
    BAH(0, "second%s.");
#undef BAH
    bu_log("\n");
    alarm(60);
    return;
}
#endif

void
IrradianceThread(int pid, void *arg)
{
#ifdef HAVE_ALARM
    starttime = time(NULL);
    signal(SIGALRM, alarmhandler);
    alarm(60);
    BuildIrradianceCache(pid, PMap[PM_GLOBAL]->Root, (struct application*)arg);
    alarm(0);
    starttime = 0;
#else
    BuildIrradianceCache(pid, PMap[PM_GLOBAL]->Root, (struct application*)arg);
#endif
}


void
Initialize(int MAP, int MapSize)
{
    BU_ALLOC(PMap[MAP], struct PhotonMap);
    PMap[MAP]->MaxPhotons = MapSize;

    BU_ALLOC(PMap[MAP]->Root, struct PNode);
    PMap[MAP]->StoredPhotons = 0;
    if (MapSize > 0)
	BU_ALLOC(Emit[MAP], struct Photon);
    else
	Emit[MAP] = NULL;
}


int
LoadFile(char *pmfile)
{
    size_t ret;
    FILE *FH;
    int I1 = 0, i;
    short S1;
    char C1;

    FH = fopen(pmfile, "rb");
    if (FH) {
	bu_log("  Reading Irradiance Cache File...\n");
	ret = fread(&S1, sizeof(short), 1, FH);
	if (ret != 1) {
	    fclose(FH);
	    bu_log("Error reading irradiance cache file (endian)\n");
	    return 0;
	}

	bu_log("endian: %d\n", S1);

	ret = fread(&S1, sizeof(short), 1, FH);
	if (ret != 1) {
	    fclose(FH);
	    bu_log("Error reading irradiance cache file (revision)\n");
	    return 0;
	}
	bu_log("revision: %d\n", S1);

	ret = fread(&ScaleFactor, sizeof(double), 1, FH);
	if (ret != 1) {
	    fclose(FH);
	    bu_log("Error reading irradiance cache file (scale factor)\n");
	    return 0;
	}
	bu_log("Scale Factor: %.3f\n", ScaleFactor);

	/* Read in Map Type */
	ret = fread(&C1, sizeof(char), 1, FH);
	if (ret != 1) {
	    fclose(FH);
	    bu_log("Error reading irradiance cache file (map type)\n");
	    return 0;
	}
	ret = fread(&I1, sizeof(int), 1, FH);
	if (ret != 1 || I1 < 0 || I1 > INT_MAX) {
	    fclose(FH);
	    bu_log("Error reading irradiance cache file (map type)\n");
	    return 0;
	}

	Initialize(PM_GLOBAL, I1);
	bu_log("Reading Global: %d\n", I1);
	for (i = 0; i < I1; i++) {
	    ret = fread(&Emit[PM_GLOBAL][i], sizeof(struct Photon), 1, FH);
	    if (ret != 1) {
		fclose(FH);
		bu_log("Error reading irradiance cache file (global)\n");
	    /* bu_log("Pos: [%.3f, %.3f, %.3f], Power: [%.3f, %.3f, %.3f]\n", Emit[PM_GLOBAL][i].Pos[0], Emit[PM_GLOBAL][i].Pos[1], Emit[PM_GLOBAL][i].Pos[2], Emit[PM_GLOBAL][i].Power[0], Emit[PM_GLOBAL][i].Power[1], Emit[PM_GLOBAL][i].Power[2]);*/
	    /* bu_log("Pos: [%.3f, %.3f, %.3f], Irrad: [%.3f, %.3f, %.3f]\n", Emit[PM_GLOBAL][i].Pos[0], Emit[PM_GLOBAL][i].Pos[1], Emit[PM_GLOBAL][i].Pos[2], Emit[PM_GLOBAL][i].Irrad[0], Emit[PM_GLOBAL][i].Irrad[1], Emit[PM_GLOBAL][i].Irrad[2]);*/
		return 0;
	    }
	}

	ret = fread(&C1, sizeof(char), 1, FH);
	if (ret != 1) {
	    fclose(FH);
	    bu_log("Error reading irradiance cache file (C1)\n");
	    return 0;
	}

	ret = fread(&I1, sizeof(int), 1, FH);
	if (ret != 1 || I1 < 0 || I1 > INT_MAX) {
	    fclose(FH);
	    bu_log("Error reading irradiance cache file (l1)\n");
	    return 0;
	}

	Initialize(PM_CAUSTIC, I1);
	bu_log("Reading Caustic: %d\n", I1);
	for (i = 0; i < I1; i++) {
	    ret = fread(&Emit[PM_CAUSTIC][i], sizeof(struct Photon), 1, FH);
	    if (ret != 1) {
		fclose(FH);
		bu_log("Error reading irradiance cache file (caustic)\n");
		return 0;
	    }
	}

	PMap[PM_GLOBAL]->StoredPhotons = PMap[PM_GLOBAL]->MaxPhotons;
	BuildTree(Emit[PM_GLOBAL], PMap[PM_GLOBAL]->StoredPhotons, PMap[PM_GLOBAL]->Root);

	PMap[PM_CAUSTIC]->StoredPhotons = PMap[PM_CAUSTIC]->MaxPhotons;
	BuildTree(Emit[PM_CAUSTIC], PMap[PM_CAUSTIC]->StoredPhotons, PMap[PM_CAUSTIC]->Root);
	fclose(FH);
	return 1;
    }

    return 0;
}


void
WritePhotons(struct PNode *Root, FILE *FH)
{
    size_t ret;
    if (!Root)
	return;

    ret = fwrite(&Root->P, sizeof(struct Photon), 1, FH);
    if (ret != 1)
	bu_log("Unable to write photons\n");
    WritePhotons(Root->L, FH);
    WritePhotons(Root->R, FH);
}


void
WritePhotonFile(char *pmfile)
{
    FILE *FH;
    int I1;
    short S1;
    char C1;
    size_t ret;

    FH = fopen(pmfile, "wb");
    if (FH) {
	/* Write 2 Byte Endian Code and 2 Byte Revision Code */
	S1 = 1;
	ret = fwrite(&S1, sizeof(short), 1, FH);
	if (ret != 1)
	    bu_log("Error writing irradiance cache file (endian)\n");

	S1 = 0;
	ret = fwrite(&S1, sizeof(short), 1, FH);
	if (ret != 1)
	    bu_log("Error writing irradiance cache file (revision)\n");

	/* Write Scale Factor */
	bu_log("writing sf: %.3f\n", ScaleFactor);
	ret = fwrite(&ScaleFactor, sizeof(double), 1, FH);
	if (ret != 1)
	    bu_log("Error writing irradiance cache file (scale factor)\n");

	/* === Write PM_GLOBAL Data === */
	C1 = PM_GLOBAL;
	ret = fwrite(&C1, sizeof(char), 1, FH);
	if (ret != 1)
	    bu_log("Error writing irradiance cache file (PM_GLOBAL)\n");

	/* Write number of Photons */
	I1 = PMap[PM_GLOBAL]->StoredPhotons;
	ret = fwrite(&I1, sizeof(int), 1, FH);
	if (ret != 1)
	    bu_log("Error writing irradiance cache file (photons)\n");

	/* Write each photon to file */
	if (PMap[PM_GLOBAL]->StoredPhotons)
	    WritePhotons(PMap[PM_GLOBAL]->Root, FH);

	/* === Write PM_CAUSTIC Data === */
	C1 = PM_CAUSTIC;
	ret = fwrite(&C1, sizeof(char), 1, FH);
	if (ret != 1)
	    bu_log("Error writing irradiance cache file (PM_CAUSTIC)\n");

	/* Write number of Photons */
	I1 = PMap[PM_CAUSTIC]->StoredPhotons;
	ret = fwrite(&I1, sizeof(int), 1, FH);
	if (ret != 1)
	    bu_log("Error writing irradiance cache file (number of photons)\n");

	/* Write each photon to file */
	if (PMap[PM_CAUSTIC]->StoredPhotons)
	    WritePhotons(PMap[PM_CAUSTIC]->Root, FH);

	fclose(FH);
    }
}


/*
 * Main Photon Mapping Function
 */
void
BuildPhotonMap(struct application *ap, point_t eye_pos, int cpus, int width, int height, int UNUSED(Hypersample), int GlobalPhotons, double CausticsPercent, int Rays, double AngularTolerance, int RandomSeed, int ImportanceMapping, int IrradianceHypersampling, int VisualizeIrradiance, double ScaleIndirect, char pmfile[255])
{
    int i, MapSize[PM_MAPS];
    double ratio;

    PM_Visualize = VisualizeIrradiance;
    GPM_IH = IrradianceHypersampling;
    GPM_WIDTH = width;
    GPM_HEIGHT = height;

    /* If the user has specified a cache file then first check to see if there is any valid data within it,
       otherwise utilize the file to push the resulting irradiance cache data into for future use. */
    if (!LoadFile(pmfile)) {
	/*
	  bu_log("pos: [%.3f, %.3f, %.3f]\n", eye_pos[0], eye_pos[1], eye_pos[2]);
	  bu_log("I, V, Imp, H: %.3f, %d, %d, %d\n", LightIntensity, VisualizeIrradiance, ImportanceMapping, IrradianceHypersampling);
	*/
	bu_log("Building Photon Map:\n");


	GPM_RAYS = Rays;
	GPM_ATOL = cos(AngularTolerance*DEG2RAD);

	PInit = 1;

#ifndef HAVE_SRAND48
	srand(RandomSeed);
#else
	srand48(RandomSeed);
#endif
	/* bu_log("Photon Structure Size: %d\n", sizeof(struct PNode));*/

	/*
	  bu_log("Checking application struct\n");
	  RT_CK_APPLICATION(ap);
	*/

	/* Initialize Emitted Photons for each map to 0 */
	EPL = 0;
	for (i = 0; i < PM_MAPS; i++)
	    EPS[i] = 0;

	CausticsPercent /= 100.0;
	MapSize[PM_IMPORTANCE] = GlobalPhotons/8;
	MapSize[PM_GLOBAL] = (int)((1.0-CausticsPercent)*GlobalPhotons);
	MapSize[PM_CAUSTIC] = (int)(CausticsPercent*GlobalPhotons);
	MapSize[PM_SHADOW] = 0;

	/* bu_log("Caustic Photons: %d\n", MapSize[PM_CAUSTIC]);*/
	/* Allocate Memory for Photon Maps */
	Initialize(PM_GLOBAL, MapSize[PM_GLOBAL]);
	Initialize(PM_CAUSTIC, MapSize[PM_CAUSTIC]);
	Initialize(PM_SHADOW, MapSize[PM_SHADOW]);
	Initialize(PM_IMPORTANCE, MapSize[PM_IMPORTANCE]);

	/* Populate Application Structure */
	/* Set Recursion Level, Magic Number, Hit/Miss Callbacks, and Purpose */
	ap->a_level = 1;
	ap->a_onehit = 0;
	ap->a_ray.magic = RT_RAY_MAGIC;
	ap->a_hit = PHit;
	ap->a_miss = PMiss;
	ap->a_logoverlap = rt_silent_logoverlap;
	ap->a_purpose = "Importance Mapping";


	if (ImportanceMapping) {
	    bu_log("  Building Importance Map...\n");
	    EmitImportonsRandom(ap, eye_pos);
	    BuildTree(Emit[PM_IMPORTANCE], PMap[PM_IMPORTANCE]->StoredPhotons, PMap[PM_IMPORTANCE]->Root);
	    ScaleFactor = MaxFloat(BBMax[0]-BBMin[0], BBMax[1]-BBMin[1], BBMax[2]-BBMin[2]);
	}

	HitG = HitB = 0;
	bu_log("  Emitting Photons...\n");
	EmitPhotonsRandom(ap, ScaleIndirect);
	/* EmitPhotonsRandom(ap, &(LightHead.l), LightIntensity);*/

	/* Generate Scale Factor */
	ScaleFactor = MaxFloat(BBMax[0]-BBMin[0], BBMax[1]-BBMin[1], BBMax[2]-BBMin[2]);

	bu_log("HitGB: %d, %d\n", HitG, HitB);
	bu_log("Scale Factor: %.3f\n", ScaleFactor);
	ratio = (double)HitG/((double)(HitG+HitB));
	bu_log("EPL: %d, Adjusted EPL: %d\n", (int)EPL, (int)(EPL*ratio));
	EPS[PM_GLOBAL] *= ratio;
	EPS[PM_CAUSTIC] *= ratio;

	/* Scale Photon Power */
	ScalePhotonPower(PM_GLOBAL);
	ScalePhotonPower(PM_CAUSTIC);

	/*
	  for (i = 0; i < PMap->StoredPhotons; i++)
	  bu_log("insertLS[%d]: %.3f, %.3f, %.3f\n", i, Emit[i].Pos[0], Emit[i].Pos[1], Emit[i].Pos[2]);
	*/


	bu_log("  Building KD-Tree...\n");
	/* Balance KD-Tree */
	for (i = 0; i < 3; i++)
	    if (PMap[i]->StoredPhotons)
		BuildTree(Emit[i], PMap[i]->StoredPhotons, PMap[i]->Root);


	bu_log("  Building Irradiance Cache...\n");
	ap->a_level = 1;
	ap->a_onehit = 0;
	ap->a_ray.magic = RT_RAY_MAGIC;
	ap->a_hit = ICHit;
	ap->a_miss = ICMiss;
	ap->a_logoverlap = rt_silent_logoverlap;
	ICSize = 0;

	if (cpus > 1) {
	    memset(GPM_RTAB, 0, sizeof(GPM_RTAB));
	    for (i = 0; i < MAX_PSW; i++) {
		rt_init_resource(&GPM_RTAB[i], i, ap->a_rt_i);
	    }
	    bu_parallel(IrradianceThread, cpus, ap);
	} else {
	    /* This will allow profiling for single threaded rendering */
	    IrradianceThread(0, ap);
	}

	/* Allocate Memory for Irradiance Cache and Initialize Pixel Map */
	/* bu_log("Image Size: %d, %d\n", width, height);*/
	if (GPM_IH) {
	    Map = (char*)bu_calloc(width*height, sizeof(char), "Map");
	    IC = (struct IrradCache*)bu_malloc(sizeof(struct IrradCache)*width*height, "IrradCache");
	    for (i = 0; i < width*height; i++) {
		BU_ALLOC(IC[i].List, struct IrradNode);
		IC[i].Num = 0;
	    }
	}


	/*
	  bu_log("  Sanity Check...\n");
	  SanityCheck(PMap[PM_GLOBAL]->Root, 0);
	*/

	WritePhotonFile(pmfile);

	for (i = 0; i < PM_MAPS; i++) {
	    bu_free(Emit[i], "Photons");
	    Emit[i] = NULL;
	}

    }
}


void
Swap(struct PSN *a, struct PSN *b)
{
    struct PSN c;

    /*
      c.P = a->P;
      c.Dist = a->Dist;
      a->P = b->P;
      a->Dist = b->Dist;
      b->P = c.P;
      b->Dist = c.Dist;
    */
    /* bu_log("  SWAP_IN: %.3f, %.3f\n", a->Dist, b->Dist);*/
    memcpy(&c, a, sizeof(struct PSN));
    memcpy(a, b, sizeof(struct PSN));
    memcpy(b, &c, sizeof(struct PSN));
    /* bu_log("  SWAP_OT: %.3f, %.3f\n", a->Dist, b->Dist);*/
}


/*
  After inserting a new node it must be brought upwards until both children
  are less than it.
*/
void
HeapUp(struct PhotonSearch *S, int ind)
{
    int i;

    if (!ind)
	return;

    i = ((ind+1)-(ind+1)%2)/2-1;
    /* bu_log("  CHECK: %.3f > %.3f :: [%d] > [%d]\n", S->List[ind].Dist, S->List[i].Dist, ind, i);*/
    if (S->List[ind].Dist > S->List[i].Dist) {
	/* bu_log("SWAP_A: %.3f, %.3f\n", S->List[i].Dist, S->List[ind].Dist);*/
	Swap(&S->List[i], &S->List[ind]);
	/* bu_log("SWAP_B: %.3f, %.3f\n", S->List[i].Dist, S->List[ind].Dist);*/
    }
    HeapUp(S, i);
}


/*
  Sift the new Root node down, by choosing the child with the highest number
  since choosing a child with the highest number may reduce the number of
  recursions the number will have to propagate
*/
void
HeapDown(struct PhotonSearch *S, int ind)
{
    int c;

    if (2*ind+1 > S->Found)
	return;

    c = 2*ind+1 < S->Found ? S->List[2*ind+2].Dist > S->List[2*ind+1].Dist ? 2*ind+2 : 2*ind+1 : 2*ind+1;
    /* bu_log(" c: %d\n", c);*/

    if (S->List[c].Dist > S->List[ind].Dist) {
	/* bu_log("SWAP_C: %.3f, %.3f :: %d, %d :: %d\n", S->List[c].Dist, S->List[ind].Dist, c, ind, S->Found);*/
	Swap(&S->List[c], &S->List[ind]);
	/* bu_log("SWAP_D: %.3f, %.3f :: %d, %d :: %d\n", S->List[c].Dist, S->List[ind].Dist, c, ind, S->Found);*/
    }
    HeapDown(S, c);
}


fastf_t
Dist(point_t a, point_t b)
{
    return (a[0]-b[0])*(a[0]-b[0]) + (a[1]-b[1])*(a[1]-b[1]) + (a[2]-b[2])*(a[2]-b[2]);
}


void
IrradianceEstimate(struct application *ap, vect_t irrad, point_t pos, vect_t normal)
{
    struct PhotonSearch Search;
    int i, idx;
    fastf_t dist, TotDist;
    vect_t t, cirrad;


    idx = 0;
    if (GPM_IH) {
	idx = ap->a_x + ap->a_y*GPM_WIDTH;
	/* See if there is a cached irradiance calculation for this point */
	for (i = 0; i < IC[idx].Num; i++) {
	    dist = (pos[0]-IC[idx].List[i].Pos[0])*(pos[0]-IC[idx].List[i].Pos[0])+(pos[1]-IC[idx].List[i].Pos[1])*(pos[1]-IC[idx].List[i].Pos[1])+(pos[2]-IC[idx].List[i].Pos[2])*(pos[2]-IC[idx].List[i].Pos[2]);
	    if (dist < (ScaleFactor/100.0)*(ScaleFactor*100.0)) {
		irrad[0] = IC[idx].List[i].RGB[0];
		irrad[1] = IC[idx].List[i].RGB[1];
		irrad[2] = IC[idx].List[i].RGB[2];
		return;
	    }
	}

	/* There is no precomputed irradiance for this point, allocate space
	   for a new one if necessary. */
	if (IC[idx].Num) {
	    IC[idx].List = (struct IrradNode*)bu_realloc(IC[idx].List, sizeof(struct IrradNode)*(IC[idx].Num+1), "List");
	}
    }

    Search.Pos[0] = pos[0];
    Search.Pos[1] = pos[1];
    Search.Pos[2] = pos[2];

    /* NP.RadSq = (ScaleFactor/(10.0*pow(2, (log(PMap[PM_GLOBAL]->MaxPhotons/2)/log(4))))) * (ScaleFactor/(10.0*pow(2, (log(PMap[PM_GLOBAL]->MaxPhotons/2)/log(4)))));*/
    /* bu_log("SF: %.3f\n", (ScaleFactor/(10.0*pow(2, (log(PMap[PM_GLOBAL]->MaxPhotons/2)/log(4))))));*/
    /* bu_log("SF: %.3f\n", ScaleFactor/pow(PMap[PM_GLOBAL]->MaxPhotons, 0.5));*/

    /*
      Search.RadSq = 0.5*ScaleFactor/pow(PMap[PM_GLOBAL]->MaxPhotons, 0.5) * ScaleFactor/pow(PMap[PM_GLOBAL]->MaxPhotons, 0.5);
      Search.Max = pow(PMap[PM_GLOBAL]->StoredPhotons, 0.5);
    */

    Search.RadSq = (ScaleFactor/2048.0);
    Search.RadSq *= Search.RadSq;
    /* NP.RadSq = (4.0*ScaleFactor/PMap[PM_GLOBAL]->MaxPhotons) * (4.0*ScaleFactor/PMap[PM_GLOBAL]->MaxPhotons);*/
    /* NP.Max = 2.0*pow(PMap[PM_GLOBAL]->StoredPhotons, 0.5);*/
    /* Search.Max = PMap[PM_GLOBAL]->StoredPhotons / 50;*/
    Search.Max = 32;

    Search.Normal[0] = normal[0];
    Search.Normal[1] = normal[1];
    Search.Normal[2] = normal[2];

    Search.List = (struct PSN*)bu_calloc(Search.Max, sizeof(struct PSN), "Search.List");
    do {
	Search.Found = 0;
	Search.RadSq *= 4.0;
	LocatePhotons(&Search, PMap[PM_GLOBAL]->Root);
    } while (Search.Found < Search.Max && Search.RadSq < ScaleFactor * ScaleFactor / 64.0);


    irrad[0] = irrad[1] = irrad[2] = 0;
    TotDist = 0;
    for (i = 0; i < Search.Found; i++) {
	t[0] = Search.List[i].P.Pos[0] - pos[0];
	t[1] = Search.List[i].P.Pos[1] - pos[1];
	t[2] = Search.List[i].P.Pos[2] - pos[2];
	TotDist += t[0]*t[0] + t[1]*t[1] + t[2]*t[2];
    }


    for (i = 0; i < Search.Found; i++) {
	t[0] = Search.List[i].P.Pos[0] - pos[0];
	t[1] = Search.List[i].P.Pos[1] - pos[1];
	t[2] = Search.List[i].P.Pos[2] - pos[2];

	t[0] = (t[0]*t[0] + t[1]*t[1] + t[2]*t[2])/TotDist;
	/*
	  irrad[0] += Search.List[i].P.Irrad[0] * t[0];
	  irrad[1] += Search.List[i].P.Irrad[1] * t[0];
	  irrad[2] += Search.List[i].P.Irrad[2] * t[0];
	*/
	irrad[0] += Search.List[i].P.Irrad[0];
	irrad[1] += Search.List[i].P.Irrad[1];
	irrad[2] += Search.List[i].P.Irrad[2];
    }
    if (Search.Found) {
	irrad[0] /= (double)Search.Found;
	irrad[1] /= (double)Search.Found;
	irrad[2] /= (double)Search.Found;
    }
    bu_free(Search.List, "Search.List");

    /* GetEstimate(cirrad, pos, normal, (int)(ScaleFactor/100.0), PMap[PM_CAUSTIC]->MaxPhotons/50, PM_CAUSTIC, 1, 0);*/
    /* GetEstimate(cirrad, pos, normal, (int)(ScaleFactor/pow(2, (log(PMap[PM_CAUSTIC]->MaxPhotons/2)/log(4)))), PMap[PM_CAUSTIC]->MaxPhotons / 50, PM_CAUSTIC, 0, 0);*/
    GetEstimate(cirrad, pos, normal, ScaleFactor/1024.0, PMap[PM_CAUSTIC]->MaxPhotons / 100, PM_CAUSTIC, ScaleFactor/128.0, 1, 15);

    irrad[0] += cirrad[0];
    irrad[1] += cirrad[1];
    irrad[2] += cirrad[2];


    /* Visualize Green Importons */
    /*
      GetEstimate(cirrad, pos, normal, ScaleFactor/512.0, 1, PM_IMPORTANCE, ScaleFactor/256.0, 1, 1);

      irrad[0] += cirrad[0];
      irrad[1] += cirrad[1];
      irrad[2] += cirrad[2];
    */

    if (GPM_IH) {
	/* Store Irradiance */
	IC[idx].List[IC[idx].Num].RGB[0] = irrad[0];
	IC[idx].List[IC[idx].Num].RGB[1] = irrad[1];
	IC[idx].List[IC[idx].Num].RGB[2] = irrad[2];

	IC[idx].List[IC[idx].Num].Pos[0] = pos[0];
	IC[idx].List[IC[idx].Num].Pos[1] = pos[1];
	IC[idx].List[IC[idx].Num].Pos[2] = pos[2];

	IC[idx].Num++;
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

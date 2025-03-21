/*                    O S G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2011-2025 United States Government as represented by
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
/** @addtogroup libged */
/** @{ */
/** @file libged/osg.cpp
 *
 * An interface to OSG.
 *
 */
/** @} */

#include "common.h"

#include "ged.h"
#include "bv/defines.h"
#include <assert.h>

#include <osg/Geode>
#include <osg/Geometry>
#include <osg/RenderInfo>
#include <osgViewer/Viewer>
#include <osgUtil/Optimizer>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>


__BEGIN_DECLS
void _ged_osgLoadScene(struct bu_list *hdlp, void *osgData);
__END_DECLS


struct osg_stuff {
    osg::ref_ptr<osgViewer::Viewer>	viewer;
    double left, right, bottom, top, near, far;
    int prev_pflag;
};

static void
_osgLoadHiddenSolid(osg::Geode *geode, struct bv_scene_obj *sp)
{
    register struct bv_vlist *vp = (struct bv_vlist *)&sp->s_vlist;
    osg::Vec3dArray* vertices;
}


static void
_osgLoadSolid(osg::Geode *geode, osg::Geometry *geom, osg::Vec3dArray *vertices, osg::Vec3dArray *normals, struct bv_scene_obj *sp)
{
    struct bv_vlist *tvp;
    int first;
    register struct bv_vlist *vp = (struct bv_vlist *)&sp->s_vlist;
    int begin;
    int nverts;

    bu_log("_ged_osgLoadSolid: enter\n");



    /* Viewing region is from -1.0 to +1.0 */
    begin = 0;
    nverts = 0;
    first = 1;
    for (BU_LIST_FOR(tvp, bv_vlist, &vp->l)) {
	int i;
	int nused = tvp->nused;
	int *cmd = tvp->cmd;
	point_t *pt = tvp->pt;
	for (i = 0; i < nused; i++, cmd++, pt++) {
	    switch (*cmd) {
		case BV_VLIST_LINE_MOVE:
		    /* Move, start line */
		    if (first == 0) {
			geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::LINE_STRIP,begin,nverts));
			//geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::LINE_STRIP,0,vertices->size()));

			bu_log("Add linestrip: begin - %d, nverts - %d\n", begin, nverts);

			// add the points geometry to the geode.
			//geode->addDrawable(geom);
		    } else
			first = 0;

		    vertices->push_back(osg::Vec3d((*pt)[X], (*pt)[Y], (*pt)[Z]));
		    normals->push_back(osg::Vec3(0.0f,-1.0f,0.0f));
		    begin += nverts;
		    nverts = 1;
		    //bu_log("_ged_osgLoadSolid: loaded point - (%lf %lf %lf)\n", (*pt)[X], (*pt)[Y], (*pt)[Z]);
		    break;
		case BV_VLIST_POLY_START:
		    normals->push_back(osg::Vec3d((*pt)[X], (*pt)[Y], (*pt)[Z]));
		    begin += nverts;
		    nverts = 0;

		    break;
		case BV_VLIST_LINE_DRAW:
		case BV_VLIST_POLY_MOVE:
		case BV_VLIST_POLY_DRAW:
		    vertices->push_back(osg::Vec3d((*pt)[X], (*pt)[Y], (*pt)[Z]));
		    ++nverts;

		    //bu_log("_ged_osgLoadSolid: loaded point - (%lf %lf %lf)\n", (*pt)[X], (*pt)[Y], (*pt)[Z]);
		    break;
		case BV_VLIST_POLY_END:
		    //vertices->push_back(osg::Vec3d((*pt)[X], (*pt)[Y], (*pt)[Z]));
		    //++nverts;
		    geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::POLYGON,begin,nverts));
		    first = 1;

		    bu_log("Add polygon: begin - %d, nverts - %d\n", begin, nverts);

		    break;
		case BV_VLIST_POLY_VERTNORM:
		    break;
	    }
	}
    }

    if (first == 0) {
	geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::LINE_STRIP,begin,nverts));
	//geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::LINE_STRIP,0,vertices->size()));

	bu_log("Add linestrip: begin - %d, nverts - %d\n", begin, nverts);

	// add the points geometry to the geode.
	//geode->addDrawable(geom);
    }

    bu_log("_ged_osgLoadSolid: leave\n");
}


void
_ged_osgLoadScene(struct bu_list *hdlp, void *osgData)
{
    register struct display_list *gdlp;
    register struct display_list *next_gdlp;
    struct bv_scene_obj *sp;
    struct osg_stuff *osp = (struct osg_stuff *)osgData;

    bu_log("_ged_osgLoadScene: part B\n");
    osg::Group* root = new osg::Group();

    // create the Geode (Geometry Node) to contain all our osg::Geometry objects.
    osg::Geode* geode = new osg::Geode();

    bu_log("before: max frame rate - %lf\n", osp->viewer->getRunMaxFrameRate());
    bu_log("_ged_osgLoadScene: enter\n");
    gdlp = BU_LIST_NEXT(display_list, hdlp);
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	FOR_ALL_SOLIDS(sp, &gdlp->dl_head_scene_obj) {
	    if (sp->s_dmode == 4) {
		_osgLoadHiddenSolid(geode, sp);
	    } else {
		osg::Geometry* geom = new osg::Geometry();
		osg::Vec3dArray* vertices = new osg::Vec3dArray;
		osg::Vec3dArray* normals = new osg::Vec3dArray;
		_osgLoadSolid(geode, geom, vertices, normals, sp);
		geom->setVertexArray(vertices);
		geom->setNormalArray(normals);
		geom->setNormalBinding(osg::Geometry::BIND_PER_PRIMITIVE_SET);
		//osg::RenderInfo ri(osp->viewer->getCamera()->getGraphicsContext()->getState(), osp->viewer->getCamera()->getView());
		//geom->compileGLObjects(ri);
		geom->setUseDisplayList(true);
		geode->addDrawable(geom);
	    }
	}

	gdlp = next_gdlp;
    }

    root->addChild(geode);
    osp->viewer->setSceneData(root);
    bu_log("after: max frame rate - %lf\n", osp->viewer->getRunMaxFrameRate());


    bu_log("_ged_osgLoadScene: loaded geode\n");
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8


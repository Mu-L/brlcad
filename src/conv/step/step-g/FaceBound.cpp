/*                 FaceBound.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2025 United States Government as represented by
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
/** @file step/FaceBound.cpp
 *
 * Routines to convert STEP "FaceBound" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "EdgeLoop.h"
#include "FaceBound.h"


#define CLASSNAME "FaceBound"
#define ENTITYNAME "Face_Bound"
string FaceBound::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)FaceBound::Create);

FaceBound::FaceBound()
{
    step = NULL;
    id = 0;
    bound = NULL;
    inner = true;
    ON_face_index = -1;
    orientation = BUnset;
}

FaceBound::FaceBound(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    bound = NULL;
    inner = true;
    ON_face_index = -1;
    orientation = BUnset;
}

FaceBound::~FaceBound()
{
    bound = NULL;
}

bool
FaceBound::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    // load base class attributes
    if (!TopologicalRepresentationItem::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::TopologicalRepresentationItem." << std::endl;
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    if (bound == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "bound");
	if (entity) {
	    bound = dynamic_cast<Loop *>(Factory::CreateObject(sw, entity));
	    if (!bound) {
		sw->entity_status[id] = STEP_LOAD_ERROR;
	    }
	} else {
	    std::cerr << CLASSNAME << ": Error loading 'bound' entity." << std::endl;
	    sw->entity_status[id] = STEP_LOAD_ERROR;
	    return false;
	}
    }
    orientation = step->getBooleanAttribute(sse, "orientation");

    if (sw->entity_status[id] == STEP_LOAD_ERROR) return false;

    sw->entity_status[id] = STEP_LOADED;

    return true;
}

void
FaceBound::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "bound:" << std::endl;
    bound->Print(level + 1);

    TAB(level + 1);
    std::cout << "orientation:" << step->getBooleanString((Boolean)orientation) << std::endl;

    TAB(level);
    std::cout << "Inherited Attributes:" << std::endl;
    TopologicalRepresentationItem::Print(level + 1);
}

bool
FaceBound::Oriented()
{
    if (orientation == BTrue) {
	return true;
    }
    return false;
}

STEPEntity *
FaceBound::GetInstance(STEPWrapper *sw, int id)
{
    return new FaceBound(sw, id);
}

STEPEntity *
FaceBound::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

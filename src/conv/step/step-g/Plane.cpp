/*                 Plane.cpp
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
/** @file step/Plane.cpp
 *
 * Routines to interface to STEP "Plane".
 *
 */
#include "STEPWrapper.h"
#include "Factory.h"

#include "Axis2Placement3D.h"

#include "Plane.h"

#define CLASSNAME "Plane"
#define ENTITYNAME "Plane"
string Plane::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)Plane::Create);

Plane::Plane()
{
    step = NULL;
    id = 0;
}

Plane::Plane(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
}

Plane::~Plane()
{
}

const double *
Plane::GetOrigin()
{
    return position->GetOrigin();
}

const double *
Plane::GetNormal()
{
    return position->GetNormal();
}

const double *
Plane::GetXAxis()
{
    return position->GetXAxis();
}

const double *
Plane::GetYAxis()
{
    return position->GetYAxis();
}

bool
Plane::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    if (!ElementarySurface::Load(step, sse)) {
	std::cout << CLASSNAME << ":Error loading base class ::Surface." << std::endl;
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    sw->entity_status[id] = STEP_LOADED;

    return true;
}

void
Plane::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    ElementarySurface::Print(level + 1);
}

STEPEntity *
Plane::GetInstance(STEPWrapper *sw, int id)
{
    return new Plane(sw, id);
}

STEPEntity *
Plane::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
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

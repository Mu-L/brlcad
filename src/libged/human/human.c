/*                          H U M A N . C
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
/** @file libged/human.c
 *
 * Generator for human models based on height, and other stuff eventually,
 * and for the humanwizard plugin (mostly).
 *
 * Here is a description of where the bounding boxes should be
 * pivoting for each joint during articulation:
 *
 * head.r and neck.r
 * - The pivot point is located on the XY center of the top of the
 *   neck.r .
 *
 * neck.r and upperchest.r
 * - The neck.r pivots from the top XY center of the upperchest.r .
 *
 * shoulder*.r
 * - The shoulder*.r pivots around the X-axis in its XYZ center (half
 *   the distance in each axis to reach the center of the box).
 *
 * upperchest.r and lowerchest.r
 * - No movement, but the upperchest.r is specifically centered above
 *   the lowerchest.r .
 *
 * shoulder*.r and lowerchest.r
 * - No movement, but the shoulder*.r and upperchest.r align
 *   horizontally (along the X-axis) and are together centered over
 *   the lowerchest.r .
 *
 * lowerchest.r and hips.r
 * - No movement, but the lowerchest.r is aligned specifically
 *   centered above the hips.r .
 *
 * hips.r and thigh*.r
 * - There are two pivot points, one for the thighL.r and the other
 *   for the thighR.r . Divide the hips.r box in half in the X
 *   direction, and find the XY centers of each.  These pivot points
 *   are at the base of the hips.r .
 *
 * thigh*.r and calf*.r
 * - The pivot point occurs at the XY center of the thigh*.r base.
 *
 * calf*.r and foot*r
 * - The pivot point of the foot*.r is located in XYZ center of the
 *   overlap, meaning half way across the X-axis, Y-axis, and Z-axis
 *   of all area of the foot*.r overlapped by the calf*.r.
 *
 * shoulder*.r and upperarm*.r
 * - The pivot point occurs at the XY center of the top of the
 *   upperarm*.r .
 *
 * upperarm*.r and forearm*.r
 * - The pivot point occurs at the XY center of the upperarm*.r base.
 *
 * forearm*.r and hand*.r
 * - The pivot point occurs at the XY center of the forearm*.r base.
 *
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "bu/getopt.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "wdb.h"
#include "ged.h"

/*
 * Default height is 5 feet, 8 inches, arbitrarily
 */
#define DEFAULT_HEIGHT_INCHES 68.0
#define DEFAULT_FILENAME "human.g"
#define DEFAULT_HUMANNAME "Body.c"

#define MAXLENGTH 64	/*Maxlength of things like object names, filenames */
#define IN2MM	25.4	/*Convert an inch measurement to millimeters */
#define CM2MM	10.0	/*Convert centimeters to millimeters */

char *progname = "Human Model";
char filename[MAXLENGTH] = DEFAULT_FILENAME;
char humanName[MAXLENGTH] = DEFAULT_HUMANNAME;

/** Human information structures */
/** Location of all joints on the body located here */
struct jointInfo
{
    point_t headJoint;
    point_t neckJoint;
    point_t leftShoulderJoint;
    point_t rightShoulderJoint;
    point_t elbowJoint;
    point_t wristJoint;
    point_t abdomenJoint;
    point_t pelvisJoint;
    point_t leftThighJoint;
    point_t rightThighJoint;
    point_t kneeJoint;
    point_t ankleJoint;
};
#define JOINT_INFO_INIT {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO, VINIT_ZERO, VINIT_ZERO, VINIT_ZERO, VINIT_ZERO, VINIT_ZERO, VINIT_ZERO, VINIT_ZERO, VINIT_ZERO, VINIT_ZERO}

/** Information for building the head */
struct headInfo
{
    fastf_t headSize;
    fastf_t neckLength, neckWidth;

    vect_t headVector;
    vect_t neckVector;
};
#define HEAD_INFO_INIT {0.0, 0.0, 0.0, VINIT_ZERO, VINIT_ZERO}


/** All information needed to build the torso lies here */
struct torsoInfo
{
    fastf_t torsoLength;
    fastf_t topTorsoLength, lowTorsoLength;
    fastf_t shoulderWidth;
    fastf_t shoulderDepth;
    fastf_t abWidth;
    fastf_t abDepth;
    fastf_t pelvisWidth;
    fastf_t pelvisDepth;

    vect_t topTorsoVector;
    vect_t lowTorsoVector;
};
#define TORSO_INFO_INIT {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, VINIT_ZERO, VINIT_ZERO}

/** All information needed to build the arms lie here */
struct armInfo
{
    fastf_t armLength;
    fastf_t upperArmWidth;
    fastf_t upperArmLength;
    fastf_t lowerArmLength, elbowWidth;
    fastf_t wristWidth;
    fastf_t handLength, handWidth;

    vect_t armVector;
    vect_t lArmDirection;   /* Arm direction vectors */
    vect_t rArmDirection;
    vect_t lElbowDirection;
    vect_t rElbowDirection;
    vect_t lWristDirection;
    vect_t rWristDirection;
};
#define ARM_INFO_INIT {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, VINIT_ZERO, VINIT_ZERO, VINIT_ZERO, VINIT_ZERO, VINIT_ZERO, VINIT_ZERO, VINIT_ZERO}

/** All information needed to build the legs lies here */
struct legInfo
{
    fastf_t legLength;
    fastf_t thighLength, thighWidth;
    fastf_t calfLength, kneeWidth;
    fastf_t footLength, ankleWidth;
    fastf_t toeWidth;

    vect_t thighVector;
    vect_t calfVector;
    vect_t footVector;
    vect_t lLegDirection;   /* Leg direction vectors */
    vect_t rLegDirection;
    vect_t lKneeDirection;
    vect_t rKneeDirection;
    vect_t lFootDirection;
    vect_t rFootDirection;
};
#define LEG_INFO_INIT {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, VINIT_ZERO, VINIT_ZERO, VINIT_ZERO, VINIT_ZERO, VINIT_ZERO, VINIT_ZERO, VINIT_ZERO, VINIT_ZERO, VINIT_ZERO}

enum sexes { male, female };
enum ethnicities { generic, white, black, hispanic, asian, other }; /* divisions taken from army demographic sheet */

/*Top level struct that holds all body information */
struct human_data_t
{
    fastf_t height;         	/* Height of person standing, inches */
    int age;                	/* Age of person, (still relevant?) */
    enum sexes sex;    	        /* Sex of person */
    enum ethnicities ethnicity;	/* Ethnicity of person */

    /* Various part lengths */
    struct headInfo head;
    struct torsoInfo torso;
    struct armInfo arms;
    struct legInfo legs;
    struct jointInfo joints;

    /* Various triggers */
    int textread;
    int textwrite;
    int verbread;
    int verbwrite;

};
#define HUMAN_DATA_INIT_ZERO {0.0, 0, male, generic, HEAD_INFO_INIT, TORSO_INFO_INIT, ARM_INFO_INIT, LEG_INFO_INIT, JOINT_INFO_INIT, 0, 0, 0, 0}

static void Auto(struct human_data_t *dude);
static void RandAuto(struct human_data_t *dude);

/**
 * This function takes in a vector, then applies a new direction to it
 * using x, y, and z degrees, and exports it to resultVect, and
 * returns the distance of the vector.
 */
static fastf_t
setDirection(fastf_t *inVect, fastf_t *resultVect, fastf_t *outMatrix, fastf_t x, fastf_t y, fastf_t z)
{
    vect_t outVect;
    mat_t rotMatrix;
    MAT_ZERO(rotMatrix);

    /*
     * x y z placed inside rotation matrix and applied to vector.
     * bn_mat_angles(rotMatrix, x, y, z) matrix rot in degrees
     * MAT4X3VEC(outVect, rotMatrix, inVect);
     */
    bn_mat_angles(rotMatrix, x, y, z);
    MAT3X3VEC(outVect, rotMatrix, inVect);

/* Print rotation matrix
 *	int i = 0;
 *	for (i = 1; i <= 16; i++) {
 *		bu_log("%3.4f\t", rotMatrix[(i-1)]);
 *		if (i%4 == 0)
 *			bu_log("\n");
 *	}
 */
    VMOVE(resultVect, outVect);
    /*return MAGNITUDE(outVect);*/
    MAT_COPY(outMatrix, rotMatrix);
    return *rotMatrix;
}


/**
 * Create a bounding box around the individual part, this one has only
 * 1 value for depth and width.  Currently is a big mess, as the boxes
 * don't want to rotate in the correct manner, and insist upon rotating
 * around an incorrect vertex.
 */
static void
boundingBox(struct rt_wdb *file, char *name, fastf_t *startPoint, fastf_t *lengthVector, fastf_t partWidth, fastf_t *rotMatrix)
{
    /* Make the arb8/rpp that will bound the part as it were straight up and down,
     * And then rotate it to the current position as given in rotMatrix,
     * followed by naming it by taking name, and cat-ing BOX to the end of it.
     */
    vect_t vects[8];
    vect_t distance;
    vect_t JVEC;
    char newName[MAXLENGTH] = "a";
    char debug[MAXLENGTH] = "a";
    int i = 0;
    int w = 0;

    bu_strlcpy(newName, name, MAXLENGTH);
    bu_strlcpy(debug, name, MAXLENGTH);
    bu_strlcat(newName, "Box", MAXLENGTH);
    bu_strlcat(debug, "Joint", MAXLENGTH);

    VADD2(distance, startPoint, lengthVector);
    VSET(distance, distance[X], distance[Y], startPoint[Z]-lengthVector[Z]);

    VSET(vects[0], (-partWidth+startPoint[X]), (-partWidth+startPoint[Y]), (startPoint[Z]));
    VSET(vects[1], (-partWidth+startPoint[X]), (partWidth+startPoint[Y]), (startPoint[Z]));
    VSET(vects[2], (partWidth+startPoint[X]), (partWidth+startPoint[Y]), (startPoint[Z]));
    VSET(vects[3], (partWidth+startPoint[X]), (-partWidth+startPoint[Y]), (startPoint[Z]));
    VSET(vects[4], (-partWidth+startPoint[X]), (-partWidth+startPoint[Y]), (distance[Z]));
    VSET(vects[5], (-partWidth+startPoint[X]), (partWidth+startPoint[Y]), (distance[Z]));
    VSET(vects[6], (partWidth+startPoint[X]), (partWidth+startPoint[Y]), (distance[Z]));
    VSET(vects[7], (partWidth+startPoint[X]), (-partWidth+startPoint[Y]), (distance[Z]));

    for (i = 0; i<8; i++) {
	vects[i][Y] *= -1;
    }

/* Print rotation matrix */
    for (w = 1; w <= 16; w++) {

/*These z, y, x, rot matrices were for debugging purposes but didn't help. */
	/*Z rotation matrix */
/*		if (w == 1 || w == 2 || w == 5 || w == 6 || w == 11)
 *			rotMatrix[(w-1)] = rotMatrix[(w-1)] * -1;
 */
	/*Y rotation Matrix */
/*		if (w == 1 || w == 3 || w == 6 || w == 9 || w == 11)
 *			rotMatrix[(w-1)] = rotMatrix[(w-1)] * -1;
 */
	/*X rotation Matrix */
/*		if (w == 1 || w == 6 || w == 7 || w == 10 || w == 11)
 *			rotMatrix[(w-1)] = rotMatrix[(w-1)] * -1;
 */
/*
 *		bu_log("%3.4f\t", rotMatrix[(w-1)]);
 *		if (w%4 == 0)
 *			bu_log("\n");
 */
    }
/*	bu_log("-------------------------------+\n");
 */

    MAT3X3VEC(JVEC, rotMatrix, lengthVector);
    mk_trc_h(file, debug, startPoint, JVEC, 4, 1);
    mk_arb8(file, newName, *vects);
}


/**
 * Create a bounding rectangle around the individual part, and this
 * one has 2 separate values for depth and width
 */
static void
boundingRectangle(struct rt_wdb *file, char *name, fastf_t *startPoint, fastf_t *lengthVector, fastf_t partWidth, fastf_t partDepth, fastf_t *UNUSED(rotMatrix))
{
    /* Make the arb8/rpp that will bound the part as it were straight
     * up and down, And then rotate it to the current position as
     * given in rotMatrix, followed by naming it by taking name, and
     * cat-ing BOX to the end of it.
     */
    vect_t vects[8];
    vect_t distance;
    char newName[MAXLENGTH] = "a";

    bu_strlcpy(newName, name, MAXLENGTH);
    bu_strlcat(newName, "Box", MAXLENGTH);

    VADD2(distance, startPoint, lengthVector);
    VSET(distance, distance[X], distance[Y], startPoint[Z]-lengthVector[Z]);

    /* Set first 4 points to be on the same plane as the starting point, and the last 4 points to be the distance vector point plane */
/*
 *	fastf_t length = findVector(partWidth, partWidth);
 */
    VSET(vects[0], (-partDepth+startPoint[X]), (-partWidth+startPoint[Y]), (startPoint[Z]));
    VSET(vects[1], (-partDepth+startPoint[X]), (partWidth+startPoint[Y]), (startPoint[Z]));
    VSET(vects[2], (partDepth+startPoint[X]), (partWidth+startPoint[Y]), (startPoint[Z]));
    VSET(vects[3], (partDepth+startPoint[X]), (-partWidth+startPoint[Y]), (startPoint[Z]));
    VSET(vects[4], (-partDepth+startPoint[X]), (-partWidth+startPoint[Y]), (distance[Z]));
    VSET(vects[5], (-partDepth+startPoint[X]), (partWidth+startPoint[Y]), (distance[Z]));
    VSET(vects[6], (partDepth+startPoint[X]), (partWidth+startPoint[Y]), (distance[Z]));
    VSET(vects[7], (partDepth+startPoint[X]), (-partWidth+startPoint[Y]), (distance[Z]));

    mk_arb8(file, newName, *vects);
}


/******************************************/
/*					  */
/***** Body Parts, from the head down *****/
/*					  */
/******************************************/

static fastf_t
makeHead(struct rt_wdb (*file), char *name, struct human_data_t *dude, fastf_t *direction, int showBoxes)
{
    fastf_t head = dude->head.headSize / 2;
    vect_t startVector, lengthVector;
    point_t headFix;
    mat_t rotMatrix;
    /*Length vector is just the diameter of the head, currently*/
    VSET(lengthVector, 0, 0, dude->head.headSize);
    VSET(startVector, 0, 0, 0);
    setDirection(startVector, dude->head.headVector, rotMatrix, direction[X], direction[Y], direction[Z]);
    mk_sph(file, name, dude->joints.headJoint, head);

    VSET(headFix, dude->joints.headJoint[X], dude->joints.headJoint[Y], dude->joints.headJoint[Z]+head);

    if (showBoxes)
	boundingBox(file, name, headFix, lengthVector, (lengthVector[Z]/2), rotMatrix);
    return 0;
}


static fastf_t
makeNeck(struct rt_wdb *file, char *name, struct human_data_t *dude, fastf_t *direction, int showBoxes)
{
    vect_t startVector;
    mat_t rotMatrix;
    VSET(startVector, 0, 0, dude->head.neckLength);
    setDirection(startVector, dude->head.neckVector, rotMatrix, direction[X], direction[Y], direction[Z]);
    VADD2(dude->joints.neckJoint, dude->joints.headJoint, dude->head.neckVector);
    mk_rcc(file, name, dude->joints.headJoint, dude->head.neckVector, dude->head.neckWidth);

    if (showBoxes)
	boundingBox(file, name, dude->joints.headJoint, startVector, dude->head.neckWidth, rotMatrix);
    return dude->head.neckWidth;
}


static fastf_t
makeUpperTorso(struct rt_wdb *file, char *name, struct human_data_t *dude, fastf_t *direction, int showBoxes)
{
    vect_t startVector;
    vect_t a, b, c, d;
    vect_t leftVector, rightVector;
    mat_t rotMatrix;

    /* Set length of top torso portion */
    VSET(startVector, 0, 0, dude->torso.topTorsoLength);
    setDirection(startVector, dude->torso.topTorsoVector, rotMatrix, direction[X], direction[Y], direction[Z]);
    VADD2(dude->joints.abdomenJoint, dude->joints.neckJoint, dude->torso.topTorsoVector);

    /* change shoulder joints to match up to torso */
    VSET(leftVector, 0, (dude->torso.shoulderWidth+(dude->arms.upperArmWidth)), 0);
    VSET(rightVector, 0, (dude->torso.shoulderWidth+(dude->arms.upperArmWidth))*-1, 0);

    VADD2(dude->joints.leftShoulderJoint, dude->joints.neckJoint, leftVector);
    VADD2(dude->joints.rightShoulderJoint, dude->joints.neckJoint, rightVector);

    /*Take shoulder width, and abWidth and convert values to vectors for tgc */
    VSET(a, (dude->torso.shoulderDepth), 0, 0);
    VSET(b, 0, dude->torso.shoulderWidth, 0);
    VSET(c, (dude->torso.abDepth), 0, 0);
    VSET(d, 0, (dude->torso.abWidth), 0);

    /* Torso will be an ellipsoidal tgc, for more realistic shape */
    mk_tgc(file, name, dude->joints.neckJoint, dude->torso.topTorsoVector, a, b, c, d);

    if (showBoxes)
	boundingRectangle(file, name, dude->joints.neckJoint, startVector, dude->torso.shoulderWidth, (dude->torso.shoulderWidth/2), rotMatrix);
    return dude->torso.abWidth;
}


static fastf_t
makeLowerTorso(struct rt_wdb *file, char *name, struct human_data_t *dude, fastf_t *direction, int showBoxes)
{
    vect_t startVector, leftVector, rightVector;
    vect_t a, b, c, d;
    mat_t rotMatrix;

    VSET(startVector, 0, 0, dude->torso.lowTorsoLength);
    setDirection(startVector, dude->torso.lowTorsoVector, rotMatrix, direction[X], direction[Y], direction[Z]);
    VADD2(dude->joints.pelvisJoint, dude->joints.abdomenJoint, dude->torso.lowTorsoVector);

    /* Set locations of legs */
    VSET(leftVector, 0, (dude->torso.pelvisWidth/2), 0);
    VSET(rightVector, 0, ((dude->torso.pelvisWidth/2)*-1), 0);
    VADD2(dude->joints.leftThighJoint, dude->joints.pelvisJoint, leftVector);
    VADD2(dude->joints.rightThighJoint, dude->joints.pelvisJoint, rightVector);

    VSET(a, (dude->torso.abDepth), 0, 0);
    VSET(b, 0, dude->torso.abWidth, 0);
    VSET(c, (dude->torso.pelvisDepth), 0, 0);
    VSET(d, 0, dude->torso.pelvisWidth, 0);

    mk_tgc(file, name, dude->joints.abdomenJoint, dude->torso.lowTorsoVector, a, b, c, d);
/*
 *	mk_trc_h(file, name, abdomenJoint, lowTorsoVector, abWidth, pelvisWidth);
 */
    if (showBoxes)
	boundingRectangle(file, name, dude->joints.abdomenJoint, startVector, dude->torso.pelvisWidth, (dude->torso.pelvisWidth/2), rotMatrix);
    return dude->torso.pelvisWidth;
}


static fastf_t
makeShoulderJoint(struct rt_wdb *file, int isLeft, char *name, struct human_data_t *dude, int showBoxes)
{
    vect_t startVector, lengthVector;
    point_t leftFix, rightFix;
    fastf_t shoulder;
    mat_t rotMatrix;
    VSET(startVector, 0, 0, dude->arms.upperArmWidth*2);
    VSET(lengthVector, 0, 0, dude->arms.upperArmWidth);
    shoulder = dude->arms.upperArmWidth;

    if (isLeft)
	mk_sph(file, name, dude->joints.leftShoulderJoint, (dude->arms.upperArmWidth));
    else
	mk_sph(file, name, dude->joints.rightShoulderJoint, (dude->arms.upperArmWidth));

    if (showBoxes) {
	if (isLeft) {
	    setDirection(startVector, lengthVector, rotMatrix, dude->arms.lArmDirection[X], dude->arms.lArmDirection[Y], dude->arms.lArmDirection[Z]);
	    VSET(leftFix, dude->joints.leftShoulderJoint[X], dude->joints.leftShoulderJoint[Y], (dude->joints.leftShoulderJoint[Z]-shoulder));
	    boundingBox(file, name, leftFix, lengthVector, (lengthVector[Z]/2), rotMatrix);
	} else {
	    setDirection(startVector, lengthVector, rotMatrix, dude->arms.rArmDirection[X], dude->arms.rArmDirection[Y], dude->arms.rArmDirection[Z]);
	    VSET(rightFix, dude->joints.rightShoulderJoint[X], dude->joints.rightShoulderJoint[Y], (dude->joints.rightShoulderJoint[Z]-shoulder));
	    boundingBox(file, name, rightFix, lengthVector, (lengthVector[Z]/2), rotMatrix);
	}
    }
    return dude->torso.shoulderWidth;
}


static fastf_t
makeShoulder(struct rt_wdb *UNUSED(file), fastf_t UNUSED(isLeft), char *UNUSED(partName), struct human_data_t *UNUSED(dude), int UNUSED(showBoxes))
{
    return 1;
}


static fastf_t
makeUpperArm(struct rt_wdb *file, int isLeft, char *partName, struct human_data_t *dude, int showBoxes)
{
    vect_t startVector;
    mat_t rotMatrix;

    VSET(startVector, 0, 0, dude->arms.upperArmLength);
    if (isLeft) {
	setDirection(startVector, dude->arms.armVector, rotMatrix, dude->arms.lArmDirection[X], dude->arms.lArmDirection[Y], dude->arms.lArmDirection[Z]); /* set y to 180 to point down */
	VADD2(dude->joints.elbowJoint, dude->joints.leftShoulderJoint, dude->arms.armVector);
	mk_trc_h(file, partName, dude->joints.leftShoulderJoint, dude->arms.armVector, dude->arms.upperArmWidth, dude->arms.elbowWidth);
    } else{
	setDirection(startVector, dude->arms.armVector, rotMatrix, dude->arms.rArmDirection[X], dude->arms.rArmDirection[Y], dude->arms.rArmDirection[Z]); /* set y to 180 to point down */
	VADD2(dude->joints.elbowJoint, dude->joints.rightShoulderJoint, dude->arms.armVector);
	mk_trc_h(file, partName, dude->joints.rightShoulderJoint, dude->arms.armVector, dude->arms.upperArmWidth, dude->arms.elbowWidth);
    }

/* Vectors and equations for making TGC arms
 *	vect_t a, b, c, d;
 *	VSET(a, upperArmWidth, 0 , 0);
 *	VSET(b, 0, upperArmWidth, 0);
 *	VSET(c, ((elbowWidth*3)/4), 0, 0)
 *	VSET(d, 0, elbowWidth, 0);
 *
 *	mk_tgc(file, partName, ShoulderJoint, armVector, a, b, c, d);
 */

    if (showBoxes) {
	if (isLeft)
	    boundingBox(file, partName, dude->joints.leftShoulderJoint, startVector, dude->arms.upperArmWidth, rotMatrix);
	else
	    boundingBox(file, partName, dude->joints.rightShoulderJoint, startVector, dude->arms.upperArmWidth, rotMatrix);
    }
    return dude->arms.elbowWidth;
}


static fastf_t
makeElbow(struct rt_wdb *file, int UNUSED(isLeft), char *name, struct human_data_t *dude)
{
    vect_t a, b, c;
    VSET(a, (dude->arms.elbowWidth), 0, 0);
    VSET(b, 0, (dude->arms.elbowWidth), 0);
    VSET(c, 0, 0, dude->arms.elbowWidth);

    mk_ell(file, name, dude->joints.elbowJoint, a, b, c);
/*
 *	mk_sph(file, name, dude->joints.elbowJoint, dude->arms.elbowWidth);
 */
    return dude->arms.elbowWidth;
}


static fastf_t
makeLowerArm(struct rt_wdb *file, int isLeft, char *name, struct human_data_t *dude, int showBoxes)
{
    vect_t startVector;
    mat_t rotMatrix;

    VSET(startVector, 0, 0, dude->arms.lowerArmLength);
    if (isLeft) {
	setDirection(startVector, dude->arms.armVector, rotMatrix, dude->arms.lElbowDirection[X], dude->arms.lElbowDirection[Y], dude->arms.lElbowDirection[Z]);
	VADD2(dude->joints.wristJoint, dude->joints.elbowJoint, dude->arms.armVector);
    } else{
	setDirection(startVector, dude->arms.armVector, rotMatrix, dude->arms.rElbowDirection[X], dude->arms.rElbowDirection[Y], dude->arms.rElbowDirection[Z]);
	VADD2(dude->joints.wristJoint, dude->joints.elbowJoint, dude->arms.armVector);
    }

/* vectors for building a tgc arm (which looks weird when rotated) */
/*	vect_t a, b, c, d;
 *
 *	VSET(a, ((elbowWidth*3)/4), 0, 0);
 *	VSET(b, 0, (elbowWidth), 0);
 *	VSET(c, wristWidth, 0, 0);
 *	VSET(d, 0, wristWidth, 0);
 *
 *	mk_tgc(file, name, elbowJoint, armVector, a, b, c, d);
 */
    mk_trc_h(file, name, dude->joints.elbowJoint, dude->arms.armVector, dude->arms.elbowWidth, dude->arms.wristWidth);

    if (showBoxes) {
	if (isLeft)
	    boundingBox(file, name, dude->joints.elbowJoint, startVector, dude->arms.elbowWidth, rotMatrix);
	else
	    boundingBox(file, name, dude->joints.elbowJoint, startVector, dude->arms.elbowWidth, rotMatrix);
    }

    return dude->arms.wristWidth;
}


static fastf_t
makeWrist(struct rt_wdb *file, int UNUSED(isLeft), char *name, struct human_data_t *dude)
{
    mk_sph(file, name, dude->joints.wristJoint, dude->arms.wristWidth);
    return dude->arms.wristWidth;
}


static void
makeHand(struct rt_wdb *file, int isLeft, char *name, struct human_data_t *dude, int showBoxes)
{
    mat_t rotMatrix;
    vect_t startVector;
    VSET(startVector, 0, 0, dude->arms.handLength);
    if (isLeft) {
	setDirection(startVector, dude->arms.armVector, rotMatrix, dude->arms.lWristDirection[X], dude->arms.lWristDirection[Y], dude->arms.lWristDirection[Z]);
    } else{
	setDirection(startVector, dude->arms.armVector, rotMatrix, dude->arms.rWristDirection[X], dude->arms.rWristDirection[Y], dude->arms.rWristDirection[Z]);
    }

    mk_sph(file, name, dude->joints.wristJoint, dude->arms.wristWidth);

    if (showBoxes) {
	boundingRectangle(file, name, dude->joints.wristJoint, startVector, dude->arms.handWidth, dude->arms.handLength, rotMatrix);
    }
}


static fastf_t
makeThighJoint(struct rt_wdb *file, int isLeft, char *name, struct human_data_t *dude)
{
    if (isLeft)
	mk_sph(file, name, dude->joints.leftThighJoint, dude->legs.thighWidth);
    else
	mk_sph(file, name, dude->joints.rightThighJoint, dude->legs.thighWidth);

    return dude->legs.thighWidth;
}


static fastf_t
makeThigh(struct rt_wdb *file, int isLeft, char *name, struct human_data_t *dude, int showBoxes)
{
    vect_t startVector;
    mat_t rotMatrix;
    VSET(startVector, 0, 0, dude->legs.thighLength);

    if (isLeft) {
	setDirection(startVector, dude->legs.thighVector, rotMatrix, dude->legs.lLegDirection[X], dude->legs.lLegDirection[Y], dude->legs.lLegDirection[Z]);
	VADD2(dude->joints.kneeJoint, dude->joints.leftThighJoint, dude->legs.thighVector);
	mk_trc_h(file, name, dude->joints.leftThighJoint, dude->legs.thighVector, dude->legs.thighWidth, dude->legs.kneeWidth);
    } else{
	setDirection(startVector, dude->legs.thighVector, rotMatrix, dude->legs.rLegDirection[X], dude->legs.rLegDirection[Y], dude->legs.rLegDirection[Z]);
	VADD2(dude->joints.kneeJoint, dude->joints.rightThighJoint, dude->legs.thighVector);
	mk_trc_h(file, name, dude->joints.rightThighJoint, dude->legs.thighVector, dude->legs.thighWidth, dude->legs.kneeWidth);
    }
    if (showBoxes) {
	if (isLeft)
	    boundingBox(file, name, dude->joints.leftThighJoint, startVector, dude->legs.thighWidth, rotMatrix);
	else
	    boundingBox(file, name, dude->joints.rightThighJoint, startVector, dude->legs.thighWidth, rotMatrix);
    }
    return dude->legs.kneeWidth;
}


static fastf_t
makeKnee(struct rt_wdb *file, int UNUSED(isLeft), char *name, struct human_data_t *dude)
{
    mk_sph(file, name, dude->joints.kneeJoint, dude->legs.kneeWidth);
    return dude->legs.kneeWidth;
}


static fastf_t
makeCalf(struct rt_wdb *file, int isLeft, char *name, struct human_data_t *dude, int showBoxes)
{
    vect_t startVector;
    mat_t rotMatrix;
    VSET(startVector, 0, 0, dude->legs.calfLength);

/*
 * Find a better way of error checking to see if the legs go thru the ground.
 *	if (ankleJoint[Y] <= 0) {
 *                ankleJoint[Y] = ankleWidth;
 *        }
 */
    if (isLeft)
	setDirection(startVector, dude->legs.calfVector, rotMatrix, dude->legs.lKneeDirection[X], dude->legs.lKneeDirection[Y], dude->legs.lKneeDirection[Z]);
    else
	setDirection(startVector, dude->legs.calfVector, rotMatrix, dude->legs.rKneeDirection[X], dude->legs.rKneeDirection[Y], dude->legs.rKneeDirection[Z]);

    VADD2(dude->joints.ankleJoint, dude->joints.kneeJoint, dude->legs.calfVector);
    mk_trc_h(file, name, dude->joints.kneeJoint, dude->legs.calfVector, dude->legs.kneeWidth, dude->legs.ankleWidth);

    if (showBoxes) {
	if (isLeft)
	    boundingBox(file, name, dude->joints.kneeJoint, startVector, dude->legs.kneeWidth, rotMatrix);
	else
	    boundingBox(file, name, dude->joints.kneeJoint, startVector, dude->legs.kneeWidth, rotMatrix);
    }
    return dude->legs.ankleWidth;
}


static fastf_t
makeAnkle(struct rt_wdb *file, int UNUSED(isLeftr), char *name, struct human_data_t *dude)
{
    mk_sph(file, name, dude->joints.ankleJoint, dude->legs.ankleWidth);
    return dude->legs.ankleWidth;
}


static fastf_t
makeFoot(struct rt_wdb *file, int isLeft, char *name, struct human_data_t *dude, int showBoxes)
{
    vect_t startVector, boxVector;
    mat_t rotMatrix;
    VSET(startVector, 0, 0, dude->legs.footLength);
    VSET(boxVector, 0, 0, dude->legs.footLength + dude->legs.toeWidth);

    if (isLeft)
	setDirection(startVector, dude->legs.footVector, rotMatrix, dude->legs.lFootDirection[X], dude->legs.lFootDirection[Y], dude->legs.lFootDirection[Z]);
    else
	setDirection(startVector, dude->legs.footVector, rotMatrix, dude->legs.rFootDirection[X], dude->legs.rFootDirection[Y], dude->legs.rFootDirection[Z]);

    mk_particle(file, name, dude->joints.ankleJoint, dude->legs.footVector, dude->legs.ankleWidth, dude->legs.toeWidth);

    if (showBoxes) {
	if (isLeft)
	    boundingBox(file, name, dude->joints.ankleJoint, boxVector, dude->legs.toeWidth, rotMatrix);
	else
	    boundingBox(file, name, dude->joints.ankleJoint, boxVector, dude->legs.toeWidth, rotMatrix);
    }
    return 0;
}


/**
 * Make profile makes the head and neck of the body
 */
static void
makeProfile(struct rt_wdb (*file), char *suffix, struct human_data_t *dude, fastf_t *direction, int showBoxes)
{
    char headName[MAXLENGTH] = "Head.s";
    char neckName[MAXLENGTH] = "Neck.s";
    bu_strlcat(headName, suffix, MAXLENGTH);
    bu_strlcat(neckName, suffix, MAXLENGTH);
    dude->head.neckLength = dude->head.headSize / 2;
    makeHead(file, headName, dude, direction, showBoxes);
    makeNeck(file, neckName, dude, direction, showBoxes);
}


/*
 * Create all the torso parts, and set joint locations for each arm, and each leg.
 */
static void
makeTorso(struct rt_wdb (*file), char *suffix, struct human_data_t *dude, fastf_t *direction, int showBoxes)
{
    char upperTorsoName[MAXLENGTH] = "UpperTorso.s";
    char lowerTorsoName[MAXLENGTH] = "LowerTorso.s";
    char leftShoulderName[MAXLENGTH] = "LeftShoulder.s";
    char rightShoulderName[MAXLENGTH] = "RightShoulder.s";

    bu_strlcat(upperTorsoName, suffix, MAXLENGTH);
    bu_strlcat(lowerTorsoName, suffix, MAXLENGTH);
    bu_strlcat(leftShoulderName, suffix, MAXLENGTH);
    bu_strlcat(rightShoulderName, suffix, MAXLENGTH);

    makeUpperTorso(file, upperTorsoName, dude, direction, showBoxes);

    makeShoulder(file, 1, leftShoulderName, dude, showBoxes);
    makeShoulder(file, 0, rightShoulderName, dude, showBoxes);

    makeLowerTorso(file, lowerTorsoName, dude, direction, showBoxes);
}


/**
 * Make the 3 components of the arm:the upper arm, the lower arm, and the hand.
 */
static void
makeArm(struct rt_wdb (*file), char *suffix, int isLeft, struct human_data_t *dude, int showBoxes)
{
    char shoulderJointName[MAXLENGTH];
    char upperArmName[MAXLENGTH];
    char elbowName[MAXLENGTH];
    char lowerArmName[MAXLENGTH];
    char wristName[MAXLENGTH];
    char handName[MAXLENGTH];

    if (isLeft) {
	bu_strlcpy(shoulderJointName, "LeftShoulderJoint.s", MAXLENGTH);
	bu_strlcpy(upperArmName, "LeftUpperArm.s", MAXLENGTH);
	bu_strlcpy(elbowName, "LeftElbowJoint.s", MAXLENGTH);
	bu_strlcpy(lowerArmName, "LeftLowerArm.s", MAXLENGTH);
	bu_strlcpy(wristName, "LeftWristJoint.s", MAXLENGTH);
	bu_strlcpy(handName, "LeftHand.s", MAXLENGTH);
    } else{
	bu_strlcpy(shoulderJointName, "RightShoulderJoint.s", MAXLENGTH);
	bu_strlcpy(upperArmName, "RightUpperArm.s", MAXLENGTH);
	bu_strlcpy(elbowName, "RightElbowJoint.s", MAXLENGTH);
	bu_strlcpy(lowerArmName, "RightLowerArm.s", MAXLENGTH);
	bu_strlcpy(wristName, "RightWristJoint.s", MAXLENGTH);
	bu_strlcpy(handName, "RightHand.s", MAXLENGTH);
    }

    bu_strlcat(shoulderJointName, suffix, MAXLENGTH);
    bu_strlcat(upperArmName, suffix, MAXLENGTH);
    bu_strlcat(elbowName, suffix, MAXLENGTH);
    bu_strlcat(lowerArmName, suffix, MAXLENGTH);
    bu_strlcat(wristName, suffix, MAXLENGTH);
    bu_strlcat(handName, suffix, MAXLENGTH);

    makeShoulderJoint(file, isLeft, shoulderJointName, dude, showBoxes);
    makeUpperArm(file, isLeft, upperArmName, dude, showBoxes);
    makeElbow(file, isLeft, elbowName, dude);

    makeLowerArm(file, isLeft, lowerArmName, dude, showBoxes);
    makeWrist(file, isLeft, wristName, dude);
    makeHand(file, isLeft, handName, dude, showBoxes);
}


/**
 * Create the leg to be length 'legLength' by making a thigh, calf, and foot to meet requirements.
 */
static void
makeLeg(struct rt_wdb (*file), char *suffix, int isLeft, struct human_data_t *dude, int showBoxes)
{
    char thighJointName[MAXLENGTH];
    char thighName[MAXLENGTH];
    char kneeName[MAXLENGTH];
    char calfName[MAXLENGTH];
    char ankleName[MAXLENGTH];
    char footName[MAXLENGTH];

    if (isLeft) {
	bu_strlcpy(thighJointName, "LeftThighJoint.s", MAXLENGTH);
	bu_strlcpy(thighName, "LeftThigh.s", MAXLENGTH);
	bu_strlcpy(kneeName, "LeftKneeJoint.s", MAXLENGTH);
	bu_strlcpy(calfName, "LeftCalf.s", MAXLENGTH);
	bu_strlcpy(ankleName, "LeftAnkleJoint.s", MAXLENGTH);
	bu_strlcpy(footName, "LeftFoot.s", MAXLENGTH);
    } else{
	bu_strlcpy(thighJointName, "RightThighJoint.s", MAXLENGTH);
	bu_strlcpy(thighName, "RightThigh.s", MAXLENGTH);
	bu_strlcpy(kneeName, "RightKneeJoint.s", MAXLENGTH);
	bu_strlcpy(calfName, "RightCalf.s", MAXLENGTH);
	bu_strlcpy(ankleName, "RightAnkleJoint.s", MAXLENGTH);
	bu_strlcpy(footName, "RightFoot.s", MAXLENGTH);
    }
    bu_strlcat(thighJointName, suffix, MAXLENGTH);
    bu_strlcat(thighName, suffix, MAXLENGTH);
    bu_strlcat(kneeName, suffix, MAXLENGTH);
    bu_strlcat(calfName, suffix, MAXLENGTH);
    bu_strlcat(ankleName, suffix, MAXLENGTH);
    bu_strlcat(footName, suffix, MAXLENGTH);

    makeThighJoint(file, isLeft, thighJointName, dude);
    makeThigh(file, isLeft, thighName, dude, showBoxes);
    makeKnee(file, isLeft, kneeName, dude);
    makeCalf(file, isLeft, calfName, dude, showBoxes);
    makeAnkle(file, isLeft, ankleName, dude);
    makeFoot(file, isLeft, footName, dude, showBoxes);
}


/**
 * Make the head, shoulders knees and toes, so to speak.  Head, neck,
 * torso, arms, hands, legs, feet.  And dude, a very technical term,
 * is the human_data in a shorter, more readable name
 */
static void
makeBody(struct rt_wdb (*file), char *suffix, struct human_data_t *dude, fastf_t *location, int showBoxes)
{
    vect_t direction;
    bu_log("Making Body\n");
    /*
     * Make sure that vectors, points, and widths are sent to each function
     * for direction, location, and correct sizing, respectively.
     */
    bu_log("Setting Direction\n");
    VSET(dude->joints.headJoint, location[X], location[Y], (location[Z]+((dude->height*IN2MM)-(dude->head.headSize/2))));
    VSET(direction, 180, 0, 0); /*Make the body build down, from head to toe. Or else it's upsidedown */

    /**
     * Head Parts
     */
    /*makeProfile makes the head and the neck */
    makeProfile(file, suffix, dude, direction, showBoxes);

    /**
     * Torso Parts
     */
    makeTorso(file, suffix, dude, direction, showBoxes);

    /**
     * Arm Parts
     */
    /*The second argument is whether or not it is the left side, 1 = yes, 0 = no) */
    makeArm(file, suffix, 1, dude, showBoxes);
    makeArm(file, suffix, 0, dude, showBoxes);

    /**
     * Leg Parts
     */
    makeLeg(file, suffix, 1, dude, showBoxes);
    makeLeg(file, suffix, 0, dude, showBoxes);
    bu_log("Body Built\n");
}


/**
 * MakeArmy makes a square of persons n by n large, where n is the
 * number of persons entered using -N if N is large (>= 20) Parts
 * start disappearing, oddly enough.
 */
static void
makeArmy(struct rt_wdb (*file), struct human_data_t *dude, int number, int showBoxes)
{
    point_t locations;
    int x = 0;
    int y = 0;
    int num;
    char testname[10] = {'0'};
    char suffix[MAXLENGTH];
    VSET(locations, 0, 0, 0); /* Starting location */
    num = 0.0;

    for (x = 0; x<number; x++) {
	for (y = 0; y<number; y++) {
	    sprintf(testname, "%d", num);
	    bu_strlcpy(suffix, testname, MAXLENGTH);
	    RandAuto(dude);	/*Generates random heights for random height, and thus random size, creation. */
	    Auto(dude);
	    makeBody(file, suffix, dude, locations, showBoxes);
	    VSET(locations, (locations[X]- (dude->torso.shoulderWidth + dude->arms.upperArmWidth)*4), locations[Y], 0);
	    num++;
	}
	VSET(locations, 0, (locations[Y]- (dude->torso.shoulderWidth + dude->arms.upperArmWidth)*4), 0);
    }
}


/* User inputs X, Y, Z coordinates for use in manual positioning */
static void
grabCoordinates(fastf_t *positions)
{
    int ret;
    double scan[3];

    printf("X: ");
    ret = scanf("%lf", &scan[X]);
    if (ret != 1)
	bu_log("Failed to get position value\n");
    fflush(stdin);

    printf("Y: ");
    ret = scanf("%lf", &scan[Y]);
    if (ret != 1)
	bu_log("Failed to get position value\n");
    fflush(stdin);

    printf("Z: ");
    ret = scanf("%lf", &scan[Z]);
    if (ret != 1)
	bu_log("Failed to get position value\n");
    fflush(stdin);

    /* convert from double to fastf_t */
    VMOVE(positions, scan);
}


/**
 * User manual poses the human model by inputting absolute angles for
 * limb positions.
 */
static void
manualPosition(struct human_data_t *dude)
{
    vect_t positions;
    printf("Manually Setting All limb Positions in degrees\n");
    printf("Remember, a 180 value for Y means pointing down\nSo for your left arm, xyz of 0 180 0 would be on the side\n");
    printf("Head is always at standing height, at origin\n");

    printf("Left Arm\n");
    printf("Upper Arm\n");
    grabCoordinates(positions);
    VMOVE(dude->arms.lArmDirection, positions);
    printf("Lower Arm\n");
    grabCoordinates(positions);
    VMOVE(dude->arms.lElbowDirection, positions);
    printf("Hand\n");
    grabCoordinates(positions);
    VMOVE(dude->arms.lWristDirection, positions);

    printf("Right Arm\n");
    printf("Upper Arm\n");
    grabCoordinates(positions);
    VMOVE(dude->arms.rArmDirection, positions);
    printf("Lower Arm\n");
    grabCoordinates(positions);
    VMOVE(dude->arms.rElbowDirection, positions);
    printf("Hand\n");
    grabCoordinates(positions);
    VMOVE(dude->arms.rWristDirection, positions);

    printf("Left Leg\n");
    printf("Thigh\n");
    grabCoordinates(positions);
    VMOVE(dude->legs.lLegDirection, positions);
    printf("Calf\n");
    grabCoordinates(positions);
    VMOVE(dude->legs.lKneeDirection, positions);
    printf("Foot\n");
    grabCoordinates(positions);
    VMOVE(dude->legs.lFootDirection, positions);

    printf("Right Leg\n");
    printf("Thigh\n");
    grabCoordinates(positions);
    VMOVE(dude->legs.rLegDirection, positions);
    printf("Calf\n");
    grabCoordinates(positions);
    VMOVE(dude->legs.rKneeDirection, positions);
    printf("Foot\n");
    grabCoordinates(positions);
    VMOVE(dude->legs.rFootDirection, positions);
}


/* position limbs to fit stance input in command line (or just default to standing) */
static void
setStance(fastf_t stance, struct human_data_t *dude)
{
    vect_t downVect, forwardVect, rightVect, leftVect; /* generic vectors for holding temp values */

    /**
     * The stances are the way the body is positioned via
     * adjusting limbs and whatnot. THEY ARE:
     * 0: Standing
     * 1: Sitting
     * 2: Driving
     * 3: Arms Out
     * 4: The Letterman
     * 5: The Captain
     * #: and more as needed
     * 999: Custom (done interactively)
     */

    VSET(downVect, 0, 180, 0); /*straight down*/
    VSET(forwardVect, 0, 90, 0); /*forwards, down X axis */
    VSET(rightVect, 90, 0, 0); /*Right, down Y axis */
    VSET(leftVect, -90, 0, 0); /*Left, up Y axis */

    switch ((int)stance) {
	case 0:
	    bu_log("Making it stand\n");
	    VMOVE(dude->arms.lArmDirection, downVect);
	    VMOVE(dude->arms.rArmDirection, downVect);
	    VMOVE(dude->arms.lElbowDirection, downVect);
	    VMOVE(dude->arms.rElbowDirection, downVect);
	    VMOVE(dude->arms.lWristDirection, downVect);
	    VMOVE(dude->arms.rWristDirection, downVect);
	    VMOVE(dude->legs.lLegDirection, downVect);
	    VMOVE(dude->legs.rLegDirection, downVect);
	    VMOVE(dude->legs.lKneeDirection, downVect);
	    VMOVE(dude->legs.rKneeDirection, downVect);
	    VMOVE(dude->legs.lFootDirection, forwardVect);
	    VMOVE(dude->legs.rFootDirection, forwardVect);
	    bu_log("Standing\n");
	    break;
	case 1:
	    bu_log("Making it sit\n");
	    VMOVE(dude->arms.lArmDirection, downVect);
	    VMOVE(dude->arms.rArmDirection, downVect);
	    VMOVE(dude->arms.lElbowDirection, downVect);
	    VMOVE(dude->arms.rElbowDirection, downVect);
	    VMOVE(dude->arms.lWristDirection, downVect);
	    VMOVE(dude->arms.rWristDirection, downVect);
	    VMOVE(dude->legs.lLegDirection, forwardVect);
	    VMOVE(dude->legs.rLegDirection, forwardVect);
	    VMOVE(dude->legs.lKneeDirection, downVect);
	    VMOVE(dude->legs.rKneeDirection, downVect);
	    VMOVE(dude->legs.lFootDirection, forwardVect);
	    VMOVE(dude->legs.rFootDirection, forwardVect);
	    break;
	case 2:
	    bu_log("Making it Drive\n"); /* it's like sitting, but with the arms extended */
	    VMOVE(dude->arms.lArmDirection, downVect);
	    VMOVE(dude->arms.rArmDirection, downVect);
	    VMOVE(dude->arms.lElbowDirection, forwardVect);
	    VMOVE(dude->arms.rElbowDirection, forwardVect);
	    VMOVE(dude->arms.lWristDirection, forwardVect);
	    VMOVE(dude->arms.rWristDirection, forwardVect);
	    VMOVE(dude->legs.lLegDirection, forwardVect);
	    VMOVE(dude->legs.rLegDirection, forwardVect);
	    VMOVE(dude->legs.lKneeDirection, downVect);
	    VMOVE(dude->legs.rKneeDirection, downVect);
	    VMOVE(dude->legs.lFootDirection, forwardVect);
	    VMOVE(dude->legs.rFootDirection, forwardVect);
	    break;
	case 3:
	    bu_log("Making arms out (standing)\n");
	    VMOVE(dude->arms.lArmDirection, leftVect);
	    VMOVE(dude->arms.rArmDirection, rightVect);
	    VMOVE(dude->arms.lElbowDirection, leftVect);
	    VMOVE(dude->arms.rElbowDirection, rightVect);
	    VMOVE(dude->arms.lWristDirection, leftVect);
	    VMOVE(dude->arms.rWristDirection, rightVect);
	    VMOVE(dude->legs.lLegDirection, downVect);
	    VMOVE(dude->legs.rLegDirection, downVect);
	    VMOVE(dude->legs.lKneeDirection, downVect);
	    VMOVE(dude->legs.rKneeDirection, downVect);
	    VMOVE(dude->legs.lFootDirection, forwardVect);
	    VMOVE(dude->legs.rFootDirection, forwardVect);
	    break;
	case 4: {
	    vect_t larm4, rarm4, knee4, lleg4;
	    bu_log("Making the Letterman\n");
	    VSET(larm4, -32, 135, 0);
	    VSET(rarm4, 32, 135, 0);
	    VSET(knee4, 90, 5, 0);
	    VSET(lleg4, 0, 75, 0);
	    VMOVE(dude->arms.lArmDirection, larm4);
	    VMOVE(dude->arms.rArmDirection, rarm4);
	    VMOVE(dude->arms.lElbowDirection, larm4);
	    VMOVE(dude->arms.rElbowDirection, rarm4);
	    VMOVE(dude->arms.lWristDirection, larm4);
	    VMOVE(dude->arms.rWristDirection, rarm4);
	    VMOVE(dude->legs.lLegDirection, lleg4);
	    VMOVE(dude->legs.rLegDirection, forwardVect);
	    VMOVE(dude->legs.lKneeDirection, knee4);
	    VMOVE(dude->legs.rKneeDirection, downVect);
	    VMOVE(dude->legs.lFootDirection, forwardVect);
	    VMOVE(dude->legs.rFootDirection, forwardVect);
	}
	    break;
	case 5: {
	    vect_t larm5, rarm5, llower5, rlower5;
	    vect_t rthigh5;
	    bu_log("Making the Captain\n");
	    VSET(larm5, 45, 180, 5);
	    VSET(rarm5, -45, 180, -5);
	    VSET(llower5, -45, 180, 5);
	    VSET(rlower5, 45, 180, -5);
	    VSET(rthigh5, 0, 85, 0);
	    VMOVE(dude->arms.lArmDirection, larm5);
	    VMOVE(dude->arms.lElbowDirection, llower5);
	    VMOVE(dude->arms.rArmDirection, rarm5);
	    VMOVE(dude->arms.rElbowDirection, rlower5);
	    VMOVE(dude->arms.lWristDirection, downVect);
	    VMOVE(dude->arms.rWristDirection, downVect);
	    VMOVE(dude->legs.lLegDirection, downVect);
	    VMOVE(dude->legs.rLegDirection, rthigh5);
	    VMOVE(dude->legs.lKneeDirection, downVect);
	    VMOVE(dude->legs.rKneeDirection, downVect);
	    VMOVE(dude->legs.lFootDirection, forwardVect);
	    VMOVE(dude->legs.rFootDirection, forwardVect);
	}
	    break;

	    /*Following cases are tests */
	case 10: {
	    vect_t test1;
	    vect_t test2;
	    vect_t test3;
	    vect_t test4;
	    vect_t test5;
	    bu_log("Test1 15 degree incs\n");
	    VSET(test1, 0, 0, 0);
	    VSET(test2, 0, 15, 0);
	    VSET(test3, 0, 30, 0);
	    VSET(test4, 0, 45, 0);
	    VSET(test5, 0, 60, 0);
	    VMOVE(dude->arms.lArmDirection, test1);
	    VMOVE(dude->arms.lElbowDirection, test1);
	    VMOVE(dude->arms.rArmDirection, test2);
	    VMOVE(dude->arms.rElbowDirection, test2);
	    VMOVE(dude->arms.lWristDirection, test1);
	    VMOVE(dude->arms.rWristDirection, test2);
	    VMOVE(dude->legs.lLegDirection, test3);
	    VMOVE(dude->legs.rLegDirection, test4);
	    VMOVE(dude->legs.lKneeDirection, test3);
	    VMOVE(dude->legs.rKneeDirection, test4);
	    VMOVE(dude->legs.lFootDirection, test5);
	    VMOVE(dude->legs.rFootDirection, test5);
	}
	    break;

	    /* Additional Positions go here*/

	case 999:
	    /* interactive position-setter-thingermajiger */
	    manualPosition(dude);
	    break;
	default:
	    bu_log("Bad Input, defaulting to Stand\n");
	    VMOVE(dude->arms.lArmDirection, downVect);
	    VMOVE(dude->arms.rArmDirection, downVect);
	    VMOVE(dude->arms.lElbowDirection, downVect);
	    VMOVE(dude->arms.rElbowDirection, downVect);
	    VMOVE(dude->arms.lWristDirection, downVect);
	    VMOVE(dude->arms.rWristDirection, downVect);
	    VMOVE(dude->legs.lLegDirection, downVect);
	    VMOVE(dude->legs.rLegDirection, downVect);
	    VMOVE(dude->legs.lKneeDirection, downVect);
	    VMOVE(dude->legs.rKneeDirection, downVect);
	    VMOVE(dude->legs.lFootDirection, forwardVect);
	    VMOVE(dude->legs.rFootDirection, forwardVect);
	    break;
    }
    bu_log("Exiting stance maker\n");
}


/**
 * Auto Set all data for all parts, in inches
 */
static void
Auto(struct human_data_t *dude)
{
    bu_log("Auto Setting\n");
    bu_log("Height=%lf\n", dude->height);
    dude->torso.torsoLength = 0;
    dude->head.headSize = (dude->height / 8) * IN2MM;
    dude->arms.armLength = (dude->height / 2) * IN2MM;
    dude->legs.legLength = ((dude->height * 4) / 8) * IN2MM;
    dude->torso.torsoLength = ((dude->height * 3) / 8) * IN2MM;
    dude->torso.topTorsoLength = (dude->torso.torsoLength *5) / 8;
    dude->torso.lowTorsoLength = (dude->torso.torsoLength *3) / 8;
    dude->torso.shoulderWidth = (dude->height / 8) *IN2MM;
    dude->torso.shoulderDepth = (dude->torso.shoulderWidth/2);
    dude->torso.abWidth = (dude->height / 9) * IN2MM;
    dude->torso.abDepth = dude->torso.abWidth / 2;
    dude->torso.pelvisWidth = (dude->height / 8) * IN2MM;
    dude->torso.pelvisDepth = dude->torso.pelvisWidth/2;
    dude->legs.thighLength = dude->legs.legLength / 2;
    dude->legs.calfLength = dude->legs.legLength / 2;
    dude->legs.thighWidth = dude->legs.thighLength / 5;
    dude->legs.kneeWidth = dude->legs.thighLength / 6;
    dude->legs.ankleWidth = dude->legs.calfLength / 8;
    dude->arms.upperArmWidth = dude->arms.armLength / 12;
    dude->arms.elbowWidth = dude->arms.armLength / 13;
    dude->arms.wristWidth = dude->arms.armLength / 15;
    dude->legs.footLength = dude->legs.ankleWidth * 3;
    dude->legs.toeWidth = dude->legs.ankleWidth * 1.2;
    dude->arms.handLength = (dude->height / 16) * IN2MM;
    dude->arms.handWidth = (dude->height / 32) * IN2MM;
    dude->arms.upperArmLength = (dude->height / 4.5) * IN2MM;
    dude->arms.lowerArmLength = (dude->height / 4.5) * IN2MM;
    dude->head.neckWidth = dude->head.headSize / 4;
}


/**
 * Random height generator
 */
static void
RandAuto(struct human_data_t *dude)
{
    fastf_t val = 0;
    val = ((rand()%12)+54);
    dude->height = val;
}


/**
 * Manually Set all data for all parts, in inches
 */
static void
Manual(struct human_data_t *dude)
{
    int ret;
    double x = 0; /* for stashing user input */

    bu_log("Manual Size Mode. All data to be entered is in inches\n");

    bu_log("HeadSize, radius\n");
    ret = scanf("%lf", &x);
    if (ret != 1)
	bu_log("Failed to get value\n");
    x *= IN2MM;
    dude->head.headSize = x;

    bu_log("NeckLength\n");
    ret = scanf("%lf", &x);
    if (ret != 1)
	bu_log("Failed to get value\n");
    x *= IN2MM;
    dude->head.neckLength = x;

    bu_log("NeckWidth, radius\n");
    ret = scanf("%lf", &x);
    if (ret != 1)
	bu_log("Failed to get value\n");
    x *= IN2MM;
    dude->head.neckWidth = x;

    bu_log("Top Torso Length\n");
    ret = scanf("%lf", &x);
    if (ret != 1)
	bu_log("Failed to get value\n");
    x *= IN2MM;
    dude->torso.topTorsoLength = x;

    bu_log("Shoulder Width\n");
    ret = scanf("%lf", &x);
    if (ret != 1)
	bu_log("Failed to get value\n");
    x *= IN2MM;
    dude->torso.shoulderWidth = x;

    bu_log("Shoulder Depth\n");
    ret = scanf("%lf", &x);
    if (ret != 1)
	bu_log("Failed to get value\n");
    x *= IN2MM;
    dude->torso.shoulderDepth = x;

    bu_log("Low Torso Length\n");
    ret = scanf("%lf", &x);
    if (ret != 1)
	bu_log("Failed to get value\n");
    x *= IN2MM;
    dude->torso.lowTorsoLength = x;

    bu_log("Ab Width\n");
    ret = scanf("%lf", &x);
    if (ret != 1)
	bu_log("Failed to get value\n");
    x *= IN2MM;
    dude->torso.abWidth = x;

    bu_log("Ab Depth\n");
    ret = scanf("%lf", &x);
    if (ret != 1)
	bu_log("Failed to get value\n");
    x *= IN2MM;
    dude->torso.abDepth = x;

    bu_log("Pelvis Width\n");
    ret = scanf("%lf", &x);
    if (ret != 1)
	bu_log("Failed to get value\n");
    x *= IN2MM;
    dude->torso.pelvisWidth = x;

    bu_log("Pelvis Depth\n");
    ret = scanf("%lf", &x);
    if (ret != 1)
	bu_log("Failed to get value\n");
    x *= IN2MM;
    dude->torso.pelvisDepth = x;
    bu_log("Upper Arm Length\n");
    ret = scanf("%lf", &x);
    if (ret != 1)
	bu_log("Failed to get value\n");
    x *= IN2MM;
    dude->arms.upperArmLength = x;

    bu_log("Upper Arm Width\n");
    ret = scanf("%lf", &x);
    if (ret != 1)
	bu_log("Failed to get value\n");
    x *= IN2MM;
    dude->arms.upperArmWidth = x;

    bu_log("Lower Arm Length\n");
    ret = scanf("%lf", &x);
    if (ret != 1)
	bu_log("Failed to get value\n");
    x *= IN2MM;
    dude->arms.lowerArmLength = x;

    bu_log("Elbow Width, radius\n");
    ret = scanf("%lf", &x);
    if (ret != 1)
	bu_log("Failed to get value\n");
    x *= IN2MM;
    dude->arms.elbowWidth = x;

    bu_log("Wrist Width, radius\n");
    ret = scanf("%lf", &x);
    if (ret != 1)
	bu_log("Failed to get value\n");
    x *= IN2MM;
    dude->arms.wristWidth = x;

    bu_log("Hand Length\n");
    ret = scanf("%lf", &x);
    if (ret != 1)
	bu_log("Failed to get value\n");
    x *= IN2MM;
    dude->arms.handLength = x;

    bu_log("Hand Width\n");
    ret = scanf("%lf", &x);
    if (ret != 1)
	bu_log("Failed to get value\n");
    x *= IN2MM;
    dude->arms.handWidth = x;

    bu_log("Thigh Length\n");
    ret = scanf("%lf", &x);
    if (ret != 1)
	bu_log("Failed to get value\n");
    x *= IN2MM;
    dude->legs.thighLength = x;

    bu_log("Thigh Width, radius\n");
    ret = scanf("%lf", &x);
    if (ret != 1)
	bu_log("Failed to get value\n");
    x *= IN2MM;
    dude->legs.thighWidth = x;

    bu_log("Knee Width, radius\n");
    ret = scanf("%lf", &x);
    if (ret != 1)
	bu_log("Failed to get value\n");
    x *= IN2MM;
    dude->legs.kneeWidth = x;

    bu_log("Calf Length\n");
    ret = scanf("%lf", &x);
    if (ret != 1)
	bu_log("Failed to get value\n");
    x *= IN2MM;
    dude->legs.calfLength = x;

    bu_log("Ankle Width, radius\n");
    ret = scanf("%lf", &x);
    if (ret != 1)
	bu_log("Failed to get value\n");
    x *= IN2MM;
    dude->legs.ankleWidth = x;

    bu_log("Foot Length\n");
    ret = scanf("%lf", &x);
    if (ret != 1)
	bu_log("Failed to get value\n");
    x *= IN2MM;
    dude->legs.footLength = x;

    bu_log("Toe Width, radius\n");
    ret = scanf("%lf", &x);
    if (ret != 1)
	bu_log("Failed to get value\n");
    x *= IN2MM;
    dude->legs.toeWidth = x;

    dude->torso.torsoLength = dude->torso.topTorsoLength + dude->torso.lowTorsoLength;
    dude->arms.armLength = dude->arms.upperArmLength + dude->arms.lowerArmLength + dude->arms.handLength;
    dude->legs.legLength = dude->legs.thighLength + dude->legs.calfLength;
    dude->height = 0;

    dude->height = (dude->torso.torsoLength/IN2MM + dude->legs.legLength/IN2MM + dude->head.headSize/IN2MM);

    bu_log("Headsize=%lf\n", dude->head.headSize);
    bu_log("Neck Length=%lf\n", dude->head.neckLength);
    bu_log("Neck Width=%lf\n", dude->head.neckWidth);

    bu_log("Top Torso=%lf\n", dude->torso.topTorsoLength);
    bu_log("Low Torso=%lf\n", dude->torso.lowTorsoLength);
    bu_log("Shoulders=%lf\n", dude->torso.shoulderWidth);
    bu_log("ShoulderDepth=%lf\n", dude->torso.shoulderDepth);
    bu_log("Abs=%lf\n", dude->torso.abWidth);
    bu_log("AbDepth=%lf\n", dude->torso.abDepth);
    bu_log("Pelvis=%lf\n", dude->torso.pelvisWidth);
    bu_log("PelvisDepth=%lf\n", dude->torso.pelvisDepth);
    bu_log("Torso Length=%lf\n", dude->torso.torsoLength);

    bu_log("Upper Arm Width=%lf\n", dude->arms.upperArmWidth);
    bu_log("Upper Arm Length=%lf\n", dude->arms.upperArmLength);
    bu_log("Lower Arm Length=%lf\n", dude->arms.lowerArmLength);
    bu_log("Elbow Width=%lf\n", dude->arms.elbowWidth);
    bu_log("Wrist Width=%lf\n", dude->arms.wristWidth);
    bu_log("Hand Length=%lf\n", dude->arms.handLength);
    bu_log("Hand Width=%lf\n", dude->arms.handWidth);
    bu_log("Arm Length=%lf\n", dude->arms.armLength);

    bu_log("Thigh Length=%lf\n", dude->legs.thighLength);
    bu_log("Thigh Width=%lf\n", dude->legs.thighWidth);
    bu_log("Calf Length=%lf\n", dude->legs.calfLength);
    bu_log("Knee Width=%lf\n", dude->legs.kneeWidth);
    bu_log("Foot Length=%lf\n", dude->legs.footLength);
    bu_log("Ankle Width=%lf\n", dude->legs.ankleWidth);
    bu_log("Toe Width=%lf\n", dude->legs.toeWidth);
    bu_log("Leg Length=%lf\n", dude->legs.legLength);
    bu_log("Height=%lf\n", dude->height);
}


/**
 * Help message printed when -h/-? option is supplied
 */
static void
show_help(const char *name, const char *optstr)
{
    struct bu_vls str = BU_VLS_INIT_ZERO;
    const char *cp = optstr;

    while (cp && *cp != '\0') {
	if (*cp == ':' || *cp == 'h' || *cp == '?') {
	    cp++;
	    continue;
	}
	bu_vls_strncat(&str, cp, 1);
	cp++;
    }

    bu_log("Usage: %s [%s]\n", name, bu_vls_addr(&str));
    bu_log("options ('Set' means 1 argument required unless otherwise noted):\n");
    bu_log("\t-A\t\tAutoMake defaults\n");
    bu_log("\t-m\t\tManual sizing mode\n");
    bu_log("\t-H\t\tSet height (inches)\n");
    bu_log("\t-L or -l\tSet center point (inches), at body's feet (default 0 0 0; interactive input)\n");
    /* bu_log("\t-o\t\tSet output file name\n" */
    bu_log("\t-b\t\tShow bounding Boxes\n");
    bu_log("\t-n\t\tSet bounding region name (default Body.c )\n");
    bu_log("\t-N\t\tSet number to make (input will be squared by the program)\n");
    bu_log("\t-s\t\tSet stance to take; 0-Stand 1-Sit 2-Drive 3-Arms out 4-Letterman 5-Captain 999-Custom\n");
    bu_log("\t-p\t\tSet percentile (not implemented yet) 1-99\n");
    bu_log("\t-t\t\tSave bounding box information to file [stats.txt]\n");
    bu_log("\t-T\t\tRead bounding box information from file [stats.txt]\n");
    bu_log("\t-v\t\tSave verbose output of all data used to build human model, to file [verbose.txt]\n");
    bu_log("\t-V\t\tRead verbose input of all data used to build human model, from file [verbose.txt]\n");
    bu_log("\t 1 - 9, 0, =, and succeeding characters are used for wizard purposes, ignore them.\n");
    bu_log("\t Last word on command line is also top level object. No argument needed!\n");

    bu_vls_free(&str);
    return;
}


/**
 * User inputs the XYZ coordinates of the center point of the human model.
 */
static void
getLocation(fastf_t *location)
{
    int ret;
    double x = 0.0 , y = 0.0 , z = 0.0 ;

    bu_log("Enter center point (inches)\n");
    bu_log("X: ");
    ret = scanf("%lf", &x);
    if (ret != 1)
	bu_log("Failed to get value\n");
    bu_log("Y: ");
    ret = scanf("%lf", &y);
    if (ret != 1)
	bu_log("Failed to get value\n");
    bu_log("Z: ");
    ret = scanf("%lf", &z);
    if (ret != 1)
	bu_log("Failed to get value\n");
    x*= IN2MM;
    y*= IN2MM;
    z*= IN2MM;
    VSET(location, x, y, z);
}


/* Process command line arguments, all 43 of them */
static int
read_args(int argc, const char **argv, char *topLevel, struct human_data_t *dude, fastf_t *percentile, fastf_t *location, int *stance, int *troops, int *showBoxes)
{
    int c;
    float height = 0;
    int soldiers = 0;
    int pose = 0;
    int percent = 50;
    double x = 0; /* for stashing user input */
    int have_name = 0;
/*  char *options = "AbH:Llmn:N:O:o:p:s:tTvVw1:2:3:4:5:6:7:8:9:0:=:+:_:*:^:%:$:#:@:!:Q:~:Z:Y:W:h?"; */
    char *options = "AbH:Llmn:N:p:s:tTvVw1:2:3:4:5:6:7:8:9:0:=:+:_:*:^:%:$:#:@:!:Q:~:Z:Y:W:h?";

    /* don't report errors (this is before bu_opterr was changed to 1 immed. below) */
    bu_opterr = 1;
    bu_optind = 1;
    while ((c = bu_getopt(argc, (char * const *)argv, options)) != -1) {
	/*bu_log("%c \n", c); Testing to see if args are getting read */
	switch (c) {
	    case 'A':
		bu_log("AutoMode, making 50 percentile man\n");
		*percentile = 50;
		Auto(dude);
		fflush(stdin);
		break;

	    case 'b':
		*showBoxes = 1;
		bu_log("Drawing bounding boxes\n");
		fflush(stdin);
		break;

	    case 'H':
		sscanf(bu_optarg, "%f", &height);
		if (height < 1) {
		    bu_log("Impossible height, setting default height!\n");
		    height = DEFAULT_HEIGHT_INCHES;
		}
		dude->height = height;
		bu_log("%.2f = height in inches\n", height);
		Auto(dude);
		fflush(stdin);
		break;

	    case 'L':
	    case 'l':
		bu_log("Location\n");
		getLocation(location);
		fflush(stdin);
		break;

	    case 'm':
		bu_log("Manual Mode\n");
		Manual(dude);
		break;

	    case 'n':
		memset(humanName, 0, MAXLENGTH);
		bu_strlcpy(humanName, bu_optarg, MAXLENGTH);
		bu_strlcpy(topLevel, humanName, MAXLENGTH);
		have_name = 1;
		fflush(stdin);
		break;

	    case 'N':
		sscanf(bu_optarg, "%d", &soldiers);
		if (soldiers <= 1) {
		    bu_log("Only 1 person. Making 16\n");
		    soldiers = 4;
		}
		bu_log("Auto %d (squared) troop formation\n", soldiers);
		*troops = (float)soldiers;
		fflush(stdin);
		break;
/*
 *	    case 'o':
 *	    case 'O':
 *		memset(filename, 0, MAXLENGTH);
 *		bu_strlcpy(filename, bu_optarg, MAXLENGTH);
 *		fflush(stdin);
 *		have_name = 1;
 *		break;
 */
	    case 'p':
		sscanf(bu_optarg, "%d", &percent);
		if (percent < 1)
		    percent = 1;
		else if (percent > 99)
		    percent = 99;
		*percentile = percent;
		fflush(stdin);
		break;

	    case 's':
		sscanf(bu_optarg, "%d", &pose);
		if (pose < 0)
		    pose = 0;
		*stance = (float)pose;
		fflush(stdin);
		break;

		/*Output a text file with height x width x depth sizes for bounding boxes. */
	    case 't':
		dude->textwrite = 1;
		break;

		/*Input a text file with height x width x depth sizes for bounding boxes. */
	    case 'T':
		dude->textread = 1;
		break;

		/*Output a text file with all measurements of the human model */
	    case 'v':
		dude->verbwrite = 1;
		break;

		/*Input a text file with all measurements for a human model */
	    case 'V':
		dude->verbread = 1;
		break;

		/* These following arguments are for the wizard program, allowing easy access to each variable.
		 * as they will only be callable by using a number (e.g. 1 = head, 2 = neck width, 3 = neck height etc.)
		 * and should not be called otherwise
		 */
	    case '1':
		sscanf(bu_optarg, "%lf", &x);
		x *= IN2MM;
		dude->head.headSize = x;
		break;
	    case '2':
		sscanf(bu_optarg, "%lf", &x);
		x *= IN2MM;
		dude->head.neckLength = x;
		break;
	    case '3':
		sscanf(bu_optarg, "%lf", &x);
		x *= IN2MM;
		dude->head.neckWidth = x;
		break;
	    case '4':
		sscanf(bu_optarg, "%lf", &x);
		x *= IN2MM;
		dude->torso.topTorsoLength = x;
		break;
	    case '5':
		sscanf(bu_optarg, "%lf", &x);
		x *= IN2MM;
		dude->torso.lowTorsoLength = x;
		dude->torso.torsoLength = dude->torso.topTorsoLength + dude->torso.lowTorsoLength;
		break;
	    case '6':
		sscanf(bu_optarg, "%lf", &x);
		x *= IN2MM;
		dude->torso.shoulderWidth = x;
		break;
	    case '7':
		sscanf(bu_optarg, "%lf", &x);
		x *= IN2MM;
		dude->torso.abWidth = x;
		break;
	    case '8':
		sscanf(bu_optarg, "%lf", &x);
		x *= IN2MM;
		dude->torso.pelvisWidth = x;
		break;
	    case '9':
		sscanf(bu_optarg, "%lf", &x);
		x *= IN2MM;
		dude->arms.upperArmWidth = x;
		break;
	    case '0':
		sscanf(bu_optarg, "%lf", &x);
		x *= IN2MM;
		dude->arms.upperArmLength = x;
		break;
	    case '=':
		sscanf(bu_optarg, "%lf", &x);
		x *= IN2MM;
		dude->arms.lowerArmLength = x;
		break;
	    case '+':
		sscanf(bu_optarg, "%lf", &x);
		x *= IN2MM;
		dude->arms.elbowWidth = x;
		break;
	    case '_':
		sscanf(bu_optarg, "%lf", &x);
		x *= IN2MM;
		dude->arms.wristWidth = x;
		break;
	    case 'Q':
		sscanf(bu_optarg, "%lf", &x);
		x *= IN2MM;
		dude->arms.handLength = x;
		dude->arms.armLength = dude->arms.upperArmLength + dude->arms.lowerArmLength + dude->arms.handLength;
		break;
	    case '~':
		sscanf(bu_optarg, "%lf", &x);
		x *= IN2MM;
		dude->arms.handWidth = x;
		break;
	    case '*':
		sscanf(bu_optarg, "%lf", &x);
		x *= IN2MM;
		dude->legs.thighLength = x;
		break;
	    case '!':
		sscanf(bu_optarg, "%lf", &x);
		x *= IN2MM;
		dude->legs.thighWidth = x;
		break;
	    case '^':
		sscanf(bu_optarg, "%lf", &x);
		x *= IN2MM;
		dude->legs.calfLength = x;
		dude->legs.legLength = dude->legs.thighLength + dude->legs.calfLength;
		break;
	    case '%':
		sscanf(bu_optarg, "%lf", &x);
		x *= IN2MM;
		dude->legs.kneeWidth = x;
		break;
	    case '$':
		sscanf(bu_optarg, "%lf", &x);
		x *= IN2MM;
		dude->legs.footLength = x;
		break;
	    case '#':
		sscanf(bu_optarg, "%lf", &x);
		x *= IN2MM;
		dude->legs.ankleWidth = x;
		break;
	    case '@':
		sscanf(bu_optarg, "%lf", &x);
		x *= IN2MM;
		dude->legs.toeWidth = x;
		break;
	    case 'Z':
		sscanf(bu_optarg, "%lf", &x);
		x *= IN2MM;
		dude->torso.shoulderDepth = x;
		break;
	    case 'Y':
		sscanf(bu_optarg, "%lf", &x);
		x *= IN2MM;
		dude->torso.abDepth = x;
		break;
	    case 'W':
		sscanf(bu_optarg, "%lf", &x);
		x *= IN2MM;
		dude->torso.pelvisDepth = x;
		break;

	    default:
		show_help(*argv, options);
		bu_exit(EXIT_SUCCESS, NULL);
		fflush(stdin);
		break;
	}
    }
    dude->height = (dude->legs.legLength + dude->torso.torsoLength + dude->head.headSize) / IN2MM;

    if ((argc - bu_optind) == 1) {
	/* Yes, there is a top-level name at the end of this argument
	 * chain, let's dump it into the file.
	 */
	have_name = 1;
	memset(humanName, 0, MAXLENGTH);
	memset(topLevel, 0, MAXLENGTH);
	bu_strlcpy(topLevel, argv[bu_optind], MAXLENGTH);
	bu_strlcpy(humanName, topLevel, MAXLENGTH);
	bu_log("First top-level object name: %s\n", topLevel);
	bu_log("Second top-level object name: %s\n", humanName);
    }
    if (!have_name) {
	/* If there is no top level name at the end, go with the
	 * default.
	 */
	bu_log("Setting default top-level object name: %s\n", DEFAULT_HUMANNAME);
	memset(humanName, 0, MAXLENGTH);
	memset(topLevel, 0, MAXLENGTH);
	bu_strlcpy(humanName, DEFAULT_HUMANNAME, MAXLENGTH);
	bu_strlcpy(topLevel, DEFAULT_HUMANNAME, MAXLENGTH);
    }
    fflush(stdout);
    return bu_optind;
}


/**
 * The text function takes the dimensions of each region on the body,
 * and finds the measurements for each bounding box to be output to a
 * text file. All dimensions are in mm, because it seems everyone just
 * /loves/ millimeters for analytical purposes.  Hard Coded to dump
 * out everything related to boundingboxes.
 */
static void
text(struct human_data_t *dude)
{
    fastf_t x = 0, y = 0, z = 0;
    FILE *dump;

    bu_log("Outputting text file\n");

    dump = fopen("stats.txt", "w+");

    fprintf(dump, "Name, X, Y, Z, all in millimeters\n");

    /*Head*/
    x = dude->head.headSize;
    y = x;
    z = x;
    fprintf(dump, "Head\t%f\t%f\t%f\n", x, y, z);

    /*Neck*/
    x = dude->head.neckWidth * 2;
    y = x;
    z = dude->head.neckLength;
    fprintf(dump, "Neck\t%f\t%f\t%f\n", x, y, z);

    /*Upper Torso*/
    if (dude->torso.shoulderDepth > dude->torso.abDepth)
	x = dude->torso.shoulderDepth;
    else
	x = dude->torso.abDepth;
    y = dude->torso.shoulderWidth;
    z = dude->torso.topTorsoLength;
    fprintf(dump, "UpperTorso\t%f\t%f\t%f\n", x, y, z);

    /*Lower Torso*/
    if (dude->torso.abDepth > dude->torso.pelvisDepth)
	x = dude->torso.abDepth;
    else
	x = dude->torso.pelvisDepth;
    y = dude->torso.pelvisWidth;
    z = dude->torso.lowTorsoLength;
    fprintf(dump, "LowerTorso\t%f\t%f\t%f\n", x, y, z);

    /*Shoulder*/
    x = dude->arms.upperArmWidth *2;
    y = x;
    z = y;
    fprintf(dump, "ShoulderJoint\t%f\t%f\t%f\n", x, y, z);

    /*Upper Arm*/
    x = dude->arms.upperArmWidth *2;
    y = x;
    z = dude->arms.upperArmLength;
    fprintf(dump, "UpperArm\t%f\t%f\t%f\n", x, y, z);

    /*Lower Arm*/
    x = dude->arms.elbowWidth * 2;
    y = x;
    z = dude->arms.lowerArmLength;
    fprintf(dump, "LowerArm\t%f\t%f\t%f\n", x, y, z);

    /*Elbow*/
    x = dude->arms.elbowWidth *2;
    y = x;
    z = y;
    fprintf(dump, "Elbow\t%f\t%f\t%f\n", x, y, z);

    /*Wrist*/
    x = dude->arms.wristWidth *2;
    y = x;
    z = y;
    fprintf(dump, "Wrist\t%f\t%f\t%f\n", x, y, z);

    /*Hand*/
    x = dude->arms.handWidth;
    y = dude->arms.handWidth;
    z = dude->arms.handLength;
    fprintf(dump, "Hand\t%f\t%f\t%f\n", x, y, z);

    /*Thigh Joint*/
    x = dude->legs.thighWidth*2;
    y = x;
    z = y;
    fprintf(dump, "ThighJoint\t%f\t%f\t%f\n", x, y, z);

    /*Thigh*/
    x = dude->legs.thighWidth*2;
    y = x;
    z = dude->legs.thighLength;
    fprintf(dump, "Thigh\t%f\t%f\t%f\n", x, y, z);

    /*Knee*/
    x = dude->legs.kneeWidth;
    y = x;
    z = y;
    fprintf(dump, "Knee\t%f\t%f\t%f\n", x, y, z);

    /*Calf*/
    x = dude->legs.kneeWidth*2;
    y = x;
    z = dude->legs.calfLength;
    fprintf(dump, "Calf\t%f\t%f\t%f\n", x, y, z);

    /*Ankle*/
    x = dude->legs.ankleWidth * 2;
    y = x;
    z = y;
    fprintf(dump, "Ankle\t%f\t%f\t%f\n", x, y, z);

    /*Foot*/
    x = dude->legs.footLength;
    y = dude->legs.toeWidth * 2;
    z = dude->legs.toeWidth * 2;
    fprintf(dump, "Foot\t%f\t%f\t%f\n", x, y, z);

    /*Total Height*/
    fprintf(dump, "Height\t%f\n", (dude->height * IN2MM));

    fclose(dump);
}


/**
 * Spit out every measurement of the model in a textfile called
 * Verbose.txt Includes all measurements: sans angles, joint
 * information
 */
static void
verbose(struct human_data_t *dude)
{
    FILE *dump;

    bu_log("Verbose Text Dump\n");
    dump = fopen("verbose.txt", "w+");
    fprintf(dump, "#All Sizes are in mm\n");

    fprintf(dump, "headSize\t%f\n", dude->head.headSize);
    fprintf(dump, "neckLength\t%f\n", dude->head.neckLength);
    fprintf(dump, "neckWidth\t%f\n", dude->head.neckWidth);

    fprintf(dump, "topTorsoLength\t%f\n", dude->torso.topTorsoLength);
    fprintf(dump, "lowTorsoLength\t%f\n", dude->torso.lowTorsoLength);
    fprintf(dump, "shoulderWidth\t%f\n", dude->torso.shoulderWidth);
    fprintf(dump, "shoulderDepth\t%f\n", dude->torso.shoulderDepth);
    fprintf(dump, "abWidth\t%f\n", dude->torso.abWidth);
    fprintf(dump, "abDepth\t%f\n", dude->torso.abDepth);
    fprintf(dump, "pelvisWidth\t%f\n", dude->torso.pelvisWidth);
    fprintf(dump, "pelvisDepth\t%f\n", dude->torso.pelvisDepth);

    fprintf(dump, "upperArmWidth\t%f\n", dude->arms.upperArmWidth);
    fprintf(dump, "upperArmLength\t%f\n", dude->arms.upperArmLength);
    fprintf(dump, "lowerArmLength\t%f\n", dude->arms.lowerArmLength);
    fprintf(dump, "elbowWidth\t%f\n", dude->arms.elbowWidth);
    fprintf(dump, "wristWidth\t%f\n", dude->arms.wristWidth);
    fprintf(dump, "handLength\t%f\n", dude->arms.handLength);
    fprintf(dump, "handWidth\t%f\n", dude->arms.handWidth);

    fprintf(dump, "thighLength\t%f\n", dude->legs.thighLength);
    fprintf(dump, "thighWidth\t%f\n", dude->legs.thighWidth);
    fprintf(dump, "calfLength\t%f\n", dude->legs.calfLength);
    fprintf(dump, "kneeWidth\t%f\n", dude->legs.kneeWidth);
    fprintf(dump, "footLength\t%f\n", dude->legs.footLength);
    fprintf(dump, "ankleWidth\t%f\n", dude->legs.ankleWidth);
    fprintf(dump, "toeWidth\t%f\n", dude->legs.toeWidth);

    fclose(dump);
    bu_log("Verbose Output saved\n");
}


/**
 * This function reads in a file with bounding box information and
 * inputs into the program, instead of manual mode or auto mode.
 */
static void
getText(struct human_data_t *UNUSED(dude))
{
    char buffer[80];
    FILE *input;

    bu_log("Reading Textfile for Input\n");

    input = fopen("stats.txt", "r");

    if (input == NULL) {
	bu_log("Non-existent input file.\n");
    } else {
	bu_log("File opened, reading data:\n");

	/* Inside this while loop, values which are input will be
	 * placed into their respective holders in the human data
	 * type, based upon name.
	 */
	while (bu_fgets(buffer, 80, input)!=NULL) {
	    bu_log("%s", buffer);
	}
	bu_log("Input file read.\n");
	fclose(input);
    }
}


static void
verbIn(struct human_data_t *dude)
{
    char buffer[80];
    char s[80];
    double holder;
    FILE *input;

    bu_log("Reading textfile for all measurements\n");

    input = fopen("verbose.txt", "r");
    if (input == NULL) {
	bu_log("Non-existent input file\n");
    } else {
	bu_log("File opened, setting measurements\n");

	/* Inside this while loop, values which are input will be
	 * placed into their respective holders in the human data
	 * type, based upon name.
	 */
	while (bu_fgets(buffer, 80, input)!=NULL) {
	    /*check string for comment symbols (#, ;, (whitespace)) and ignore them.*/
	    sscanf(buffer, "%c", s);
	    if (s[0] == '#'|| s[0] == ';')
		/*bu_log("COMMENT!!\n");*/
		/*Go to next line...*/
		;
	    else {
		/*Then check remaining strings against existing variables*/
		sscanf(buffer, "%s %lf", s, &holder);
		/*bu_log("%s, %f\n", s, holder);*/
		if (holder <= 0)
		    bu_log("Bad inputs, zero and non-negative values for inputs!\n");
		/*Big statement of matching names with variables*/
		if (BU_STR_EQUAL(s, "headSize"))
		    dude->head.headSize = holder;
		else if (BU_STR_EQUAL(s, "neckWidth"))
		    dude->head.neckWidth = holder;
		else if (BU_STR_EQUAL(s, "neckLength"))
		    dude->head.neckLength = holder;
		else if (BU_STR_EQUAL(s, "topTorsoLength"))
		    dude->torso.topTorsoLength = holder;
		else if (BU_STR_EQUAL(s, "lowTorsoLength"))
		    dude->torso.lowTorsoLength = holder;
		else if (BU_STR_EQUAL(s, "shoulderWidth"))
		    dude->torso.shoulderWidth = holder;
		else if (BU_STR_EQUAL(s, "shoulderDepth"))
		    dude->torso.shoulderDepth = holder;
		else if (BU_STR_EQUAL(s, "abWidth"))
		    dude->torso.abWidth = holder;
		else if (BU_STR_EQUAL(s, "abDepth"))
		    dude->torso.abDepth = holder;
		else if (BU_STR_EQUAL(s, "pelvisWidth"))
		    dude->torso.pelvisWidth = holder;
		else if (BU_STR_EQUAL(s, "pelvisDepth"))
		    dude->torso.pelvisDepth = holder;
		else if (BU_STR_EQUAL(s, "upperArmWidth"))
		    dude->arms.upperArmWidth = holder;
		else if (BU_STR_EQUAL(s, "upperArmLength"))
		    dude->arms.upperArmLength = holder;
		else if (BU_STR_EQUAL(s, "lowerArmLength"))
		    dude->arms.lowerArmLength = holder;
		else if (BU_STR_EQUAL(s, "elbowWidth"))
		    dude->arms.elbowWidth = holder;
		else if (BU_STR_EQUAL(s, "wristWidth"))
		    dude->arms.wristWidth = holder;
		else if (BU_STR_EQUAL(s, "handLength"))
		    dude->arms.handLength = holder;
		else if (BU_STR_EQUAL(s, "handWidth"))
		    dude->arms.handWidth = holder;
		else if (BU_STR_EQUAL(s, "thighLength"))
		    dude->legs.thighLength = holder;
		else if (BU_STR_EQUAL(s, "thighWidth"))
		    dude->legs.thighWidth = holder;
		else if (BU_STR_EQUAL(s, "calfLength"))
		    dude->legs.calfLength = holder;
		else if (BU_STR_EQUAL(s, "kneeWidth"))
		    dude->legs.kneeWidth = holder;
		else if (BU_STR_EQUAL(s, "footLength"))
		    dude->legs.footLength = holder;
		else if (BU_STR_EQUAL(s, "ankleWidth"))
		    dude->legs.ankleWidth = holder;
		else if (BU_STR_EQUAL(s, "toeWidth"))
		    dude->legs.toeWidth = holder;
		else
		    bu_log("Bad spelling somewhere\n");
	    }
	}
	bu_log("File read\n");
	dude->legs.legLength = dude->legs.thighLength + dude->legs.calfLength;
	dude->torso.torsoLength = dude->torso.topTorsoLength + dude->torso.lowTorsoLength;
	dude->height = (dude->legs.legLength + dude->torso.torsoLength + dude->head.headSize) / IN2MM;
	bu_log("In Height = %f\n", dude->height);
	fclose(input);
    }
}


/**
 * ged_human_core is the function which is called from an outside function
 * in the /shapes directory. It's essentially a main function without
 * main.
 */
int
ged_human_core(struct ged *gedp, int ac, const char *av[])
{
    struct wmember human;
    struct wmember boxes;
    struct wmember hollow;
    struct wmember crowd;
    struct bu_vls name = BU_VLS_INIT_ZERO;
    struct bu_vls str = BU_VLS_INIT_ZERO;
    struct human_data_t human_data = HUMAN_DATA_INIT_ZERO;
    int showBoxes = 0, troops = 0, stance = 0;
    fastf_t percentile = (fastf_t)50.0;
    char suffix[MAXLENGTH]="";
    point_t location;
    int is_region = 0;
    unsigned char rgb[3], rgb2[3], rgb3[3];
    char topLevel[MAXLENGTH] = "";

    bu_log("Entering Human Builder\n");
    srand(time(NULL));
    human_data.height = DEFAULT_HEIGHT_INCHES;
    VSET(location, 0, 0, 0); /* Default standing location */

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    /* Process command line arguments */
    read_args(ac, av, topLevel, &human_data, &percentile, location, &stance, &troops, &showBoxes);

    GED_CHECK_EXISTS(gedp, bu_vls_addr(&name), LOOKUP_QUIET, BRLCAD_ERROR);
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    bu_log("Center Location: ");
    bu_log("%.2f %.2f %.2f\n", location[X], location[Y], location[Z]);

/******MAGIC******/

    /* This applies the generic end-name to the high-level object */
    bu_log("%s\n", topLevel);
    memset(humanName, 0, MAXLENGTH);
    bu_strlcpy(humanName, topLevel, MAXLENGTH);

    setStance(stance, &human_data);
    if (human_data.textread == 1)
	getText(&human_data);
    if (human_data.verbread == 1)
	verbIn(&human_data);
    if (troops <= 1) {
	makeBody(wdbp, suffix, &human_data, location, showBoxes);
	mk_id_units(wdbp, "A single Human", "in");

	/*This function dumps out a text file of all dimensions of bounding boxes/anthro-data/whatever on human model.*/
	if (human_data.textwrite == 1)
	    text(&human_data);
	if (human_data.verbwrite == 1)
	    verbose(&human_data);
    }
    if (troops > 1) {
	makeArmy(wdbp, &human_data, troops, showBoxes);
	mk_id_units(wdbp, "An army of people", "in");
    }
/****End Magic****/

/** Make the Regions (.r's) of the body */
/* Make the .r for the real body */
    bu_log("Building Regions\n");
    if (!troops) {
	BU_LIST_INIT(&human.l);
	(void)mk_addmember("Head.s", &human.l, NULL, WMOP_UNION);
	(void)mk_addmember("Neck.s", &human.l, NULL, WMOP_UNION);
	(void)mk_addmember("UpperTorso.s", &human.l, NULL, WMOP_UNION);
	(void)mk_addmember("LowerTorso.s", &human.l, NULL, WMOP_UNION);
	(void)mk_addmember("LeftUpperArm.s", &human.l, NULL, WMOP_UNION);
	(void)mk_addmember("RightUpperArm.s", &human.l, NULL, WMOP_UNION);
	(void)mk_addmember("LeftShoulderJoint.s", &human.l, NULL, WMOP_UNION);
	(void)mk_addmember("RightShoulderJoint.s", &human.l, NULL, WMOP_UNION);
	(void)mk_addmember("LeftElbowJoint.s", &human.l, NULL, WMOP_UNION);
	(void)mk_addmember("RightElbowJoint.s", &human.l, NULL, WMOP_UNION);
	(void)mk_addmember("LeftLowerArm.s", &human.l, NULL, WMOP_UNION);
	(void)mk_addmember("RightLowerArm.s", &human.l, NULL, WMOP_UNION);
	(void)mk_addmember("LeftWristJoint.s", &human.l, NULL, WMOP_UNION);
	(void)mk_addmember("RightWristJoint.s", &human.l, NULL, WMOP_UNION);
	(void)mk_addmember("LeftHand.s", &human.l, NULL, WMOP_UNION);
	(void)mk_addmember("RightHand.s", &human.l, NULL, WMOP_UNION);
	(void)mk_addmember("LeftThigh.s", &human.l, NULL, WMOP_UNION);
	(void)mk_addmember("RightThigh.s", &human.l, NULL, WMOP_UNION);
	(void)mk_addmember("LeftThighJoint.s", &human.l, NULL, WMOP_UNION);
	(void)mk_addmember("RightThighJoint.s", &human.l, NULL, WMOP_UNION);
	(void)mk_addmember("LeftKneeJoint.s", &human.l, NULL, WMOP_UNION);
	(void)mk_addmember("RightKneeJoint.s", &human.l, NULL, WMOP_UNION);
	(void)mk_addmember("LeftCalf.s", &human.l, NULL, WMOP_UNION);
	(void)mk_addmember("RightCalf.s", &human.l, NULL, WMOP_UNION);
	(void)mk_addmember("LeftAnkleJoint.s", &human.l, NULL, WMOP_UNION);
	(void)mk_addmember("RightAnkleJoint.s", &human.l, NULL, WMOP_UNION);
	(void)mk_addmember("LeftFoot.s", &human.l, NULL, WMOP_UNION);
	(void)mk_addmember("RightFoot.s", &human.l, NULL, WMOP_UNION);

	is_region = 1;
	VSET(rgb, 128, 255, 128); /* some wonky bright green color */
	mk_lcomb(wdbp,
		 humanName,
		 &human,
		 is_region,
		 "plastic",
		 "di=.99 sp=.01",
		 rgb,
		 0);

/* make the .r for the bounding boxes */
	if (showBoxes) {
	    /*
	     * Create opaque bounding boxes for representations of where the person model
	     * may lay up next to another model
	     */
	    BU_LIST_INIT(&boxes.l)
		(void)mk_addmember("Head.sBox", &boxes.l, NULL, WMOP_UNION);
	    (void)mk_addmember("Neck.sBox", &boxes.l, NULL, WMOP_UNION);
	    (void)mk_addmember("UpperTorso.sBox", &boxes.l, NULL, WMOP_UNION);
	    (void)mk_addmember("LowerTorso.sBox", &boxes.l, NULL, WMOP_UNION);
	    (void)mk_addmember("LeftUpperArm.sBox", &boxes.l, NULL, WMOP_UNION);
	    (void)mk_addmember("RightUpperArm.sBox", &boxes.l, NULL, WMOP_UNION);
	    (void)mk_addmember("LeftLowerArm.sBox", &boxes.l, NULL, WMOP_UNION);
	    (void)mk_addmember("RightLowerArm.sBox", &boxes.l, NULL, WMOP_UNION);
	    (void)mk_addmember("LeftHand.sBox", &boxes.l, NULL, WMOP_UNION);
	    (void)mk_addmember("RightHand.sBox", &boxes.l, NULL, WMOP_UNION);
	    (void)mk_addmember("LeftThigh.sBox", &boxes.l, NULL, WMOP_UNION);
	    (void)mk_addmember("RightThigh.sBox", &boxes.l, NULL, WMOP_UNION);
	    (void)mk_addmember("LeftCalf.sBox", &boxes.l, NULL, WMOP_UNION);
	    (void)mk_addmember("RightCalf.sBox", &boxes.l, NULL, WMOP_UNION);
	    (void)mk_addmember("LeftFoot.sBox", &boxes.l, NULL, WMOP_UNION);
	    (void)mk_addmember("RightFoot.sBox", &boxes.l, NULL, WMOP_UNION);
	    is_region = 1;
	    VSET(rgb2, 255, 128, 128); /* redish color */
	    mk_lcomb(wdbp,
		     "Boxes.r",
		     &boxes,
		     is_region,
		     "plastic",
		     "di=0.5 sp=0.5",
		     rgb2,
		     0);
	    /*
	     * Creating a hollow box that would allow for a person to see inside the
	     * bounding boxes to the actual body representation inside.
	     */

	    BU_LIST_INIT(&hollow.l);
	    (void)mk_addmember("Head.sBox", &hollow.l, NULL, WMOP_UNION);
	    (void)mk_addmember("Head.s", &hollow.l, NULL, WMOP_SUBTRACT);

	    (void)mk_addmember("Neck.sBox", &hollow.l, NULL, WMOP_UNION);
	    (void)mk_addmember("Neck.s", &hollow.l, NULL, WMOP_SUBTRACT);
	    (void)mk_addmember("Head.s", &hollow.l, NULL, WMOP_SUBTRACT);

	    (void)mk_addmember("UpperTorso.sBox", &hollow.l, NULL, WMOP_UNION);
	    (void)mk_addmember("UpperTorso.s", &hollow.l, NULL, WMOP_SUBTRACT);

	    (void)mk_addmember("LowerTorso.sBox", &hollow.l, NULL, WMOP_UNION);
	    (void)mk_addmember("LowerTorso.s", &hollow.l, NULL, WMOP_SUBTRACT);

	    (void)mk_addmember("LeftShoulderJoint.sBox", &hollow.l, NULL, WMOP_UNION);
	    (void)mk_addmember("LeftShoulderJoint.s", &hollow.l, NULL, WMOP_SUBTRACT);

	    (void)mk_addmember("RightShoulderJoint.sBox", &hollow.l, NULL, WMOP_UNION);
	    (void)mk_addmember("RightShoulderJoint.s", &hollow.l, NULL, WMOP_SUBTRACT);

	    (void)mk_addmember("LeftUpperArm.sBox", &hollow.l, NULL, WMOP_UNION);
	    (void)mk_addmember("LeftUpperArm.s", &hollow.l, NULL, WMOP_SUBTRACT);

	    (void)mk_addmember("RightUpperArm.sBox", &hollow.l, NULL, WMOP_UNION);
	    (void)mk_addmember("RightUpperArm.s", &hollow.l, NULL, WMOP_SUBTRACT);

	    (void)mk_addmember("LeftLowerArm.sBox", &hollow.l, NULL, WMOP_UNION);
	    (void)mk_addmember("LeftLowerArm.s", &hollow.l, NULL, WMOP_SUBTRACT);

	    (void)mk_addmember("RightLowerArm.sBox", &hollow.l, NULL, WMOP_UNION);
	    (void)mk_addmember("RightLowerArm.s", &hollow.l, NULL, WMOP_SUBTRACT);

	    (void)mk_addmember("LeftHand.sBox", &hollow.l, NULL, WMOP_UNION);
	    (void)mk_addmember("LeftHand.s", &hollow.l, NULL, WMOP_SUBTRACT);

	    (void)mk_addmember("RightHand.sBox", &hollow.l, NULL, WMOP_UNION);
	    (void)mk_addmember("RightHand.s", &hollow.l, NULL, WMOP_SUBTRACT);

	    (void)mk_addmember("LeftThigh.sBox", &hollow.l, NULL, WMOP_UNION);
	    (void)mk_addmember("LeftThigh.s", &hollow.l, NULL, WMOP_SUBTRACT);

	    (void)mk_addmember("RightThigh.sBox", &hollow.l, NULL, WMOP_UNION);
	    (void)mk_addmember("RightThigh.s", &hollow.l, NULL, WMOP_SUBTRACT);

	    (void)mk_addmember("LeftCalf.sBox", &hollow.l, NULL, WMOP_UNION);
	    (void)mk_addmember("LeftCalf.s", &hollow.l, NULL, WMOP_SUBTRACT);

	    (void)mk_addmember("RightCalf.sBox", &hollow.l, NULL, WMOP_UNION);
	    (void)mk_addmember("RightCalf.s", &hollow.l, NULL, WMOP_SUBTRACT);

	    (void)mk_addmember("LeftFoot.sBox", &hollow.l, NULL, WMOP_UNION);
	    (void)mk_addmember("LeftFoot.s", &hollow.l, NULL, WMOP_SUBTRACT);

	    (void)mk_addmember("RightFoot.sBox", &hollow.l, NULL, WMOP_UNION);
	    (void)mk_addmember("RightFoot.s", &hollow.l, NULL, WMOP_SUBTRACT);

	    is_region = 1;
	    VSET(rgb3, 128, 128, 255); /* blueish color */
	    mk_lcomb(wdbp,
		     "Hollow.r",
		     &hollow,
		     is_region,
		     "glass",
		     "di=0.5 sp=0.5 tr=0.75 ri=1",
		     rgb3,
		     0);
	}
    }
    if (troops) {
	/*Build body regions for each troop*/
	/*append number to end of part name, (Head.s0, LeftElbowJoint.s99, etc.) */
	int num = 0;
	int w = 0;
	int x = 0;
	char holder[12] = {'0'};
	int z = 0;
	char thing[12] = "0";
	char thing2[12] = "0";
	bu_log("Naming\n");

	for (w=0; w<(troops*troops); w++) {
	    char names[MAXLENGTH][MAXLENGTH] = {"Head.s", "Neck.s", "UpperTorso.s", "LowerTorso.s", "LeftShoulderJoint.s", "LeftUpperArm.s", "LeftElbowJoint.s",
					      "LeftLowerArm.s", "LeftWristJoint.s", "LeftHand.s", "RightShoulderJoint.s", "RightUpperArm.s", "RightElbowJoint.s", "RightLowerArm.s",
					      "RightWristJoint.s", "RightHand.s", "LeftThighJoint.s", "LeftThigh.s", "LeftKneeJoint.s", "LeftCalf.s", "LeftAnkleJoint.s", "LeftFoot.s",
					      "RightThighJoint.s", "RightThigh.s", "RightKneeJoint.s", "RightCalf.s", "RightAnkleJoint.s", "RightFoot.s", "0"};
	    char body[MAXLENGTH][MAXLENGTH];
	    char box[MAXLENGTH][MAXLENGTH];

	    bu_strlcpy(body[0], topLevel, MAXLENGTH);
	    bu_strlcpy(box[0], "Box.r", MAXLENGTH);

	    bu_log("%d\n", w);

	    sprintf(holder, "%d", num);
	    bu_strlcpy(suffix, holder, MAXLENGTH);
	    bu_strlcat(body[0], suffix, MAXLENGTH);
	    bu_strlcat(box[0], suffix, MAXLENGTH);
	    bu_log("Adding Members\n");
	    BU_LIST_INIT(&human.l);
	    BU_LIST_INIT(&crowd.l);
	    if (showBoxes)
		BU_LIST_INIT(&boxes.l);

	    /*This value is the number of items in char names */
	    while (x<28) {
		bu_log("%s : ", names[x]);
		bu_strlcat(names[x], suffix, MAXLENGTH);
		(void)mk_addmember(names[x], &human.l, NULL, WMOP_UNION);
		if (showBoxes) {
		    bu_strlcat(names[x], "Box", MAXLENGTH);
		    bu_log("%s : ", names[x]);
		    (void)mk_addmember(names[x], &boxes.l, NULL, WMOP_UNION);
		}
		x++;
	    }
	    x = 0;
	    VSET(rgb, 128, 255, 128); /* some wonky bright green color */
	    bu_log("Combining\n");
	    is_region = 1;
	    mk_lcomb(wdbp,
		     body[0],
		     &human,
		     is_region,
		     "plastic",
		     "di=.99 sp=.01",
		     rgb,
		     0);

	    if (showBoxes) {
		VSET(rgb2, 255, 128, 128); /* redish color */
		mk_lcomb(wdbp,
			 box[0],
			 &boxes,
			 is_region,
			 "plastic",
			 "di=0.5 sp=0.5",
			 rgb2,
			 0);
	    }
	    bu_log("%s\n", body[0]);
	    num++;
	}
	for (z=0; z<(troops*troops); z++) {
	    char comber[MAXLENGTH];

	    bu_strlcpy(comber, topLevel, MAXLENGTH);

	    sprintf(thing, "%d", z);
	    bu_strlcpy(thing2, thing, MAXLENGTH);
	    bu_strlcat(comber, thing2, MAXLENGTH);
	    (void)mk_addmember(comber, &crowd.l, NULL, WMOP_UNION);
	}
    }
    if (troops)
	mk_lcomb(wdbp, "Crowd.c", &crowd, 0, NULL, NULL, NULL, 0);

    /* Close database */
    bu_log("Regions Built\n");
    bu_vls_free(&name);
    bu_vls_free(&str);

    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl human_cmd_impl = {
    "human",
    ged_human_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd human_cmd = { &human_cmd_impl };
const struct ged_cmd *human_cmds[] = { &human_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  human_cmds, 1 };

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

/*                       C O L O R . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2025 United States Government as represented by
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

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bu.h"

#include "vmath.h"


static int
test_bu_rgb_to_hsv(int argc, char *argv[])
{
    fastf_t expected_hsv_color[3] = {11.0, 22.0, 33.0};
    fastf_t actual_hsv_color[3] = {11.0, 22.0, 33.0};
    unsigned int scanned_rgb_color[3] = {11, 22, 33};
    unsigned char rgb_color[3] = {11, 22, 33};

    if (argc != 4) {
	bu_exit(1, "ERROR: input format is rgb_values expected_hsv_values [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%u, %u, %u", &scanned_rgb_color[RED], &scanned_rgb_color[GRN], &scanned_rgb_color[BLU]);
    VMOVE(rgb_color, scanned_rgb_color);
    sscanf(argv[3], "%lf, %lf, %lf", &expected_hsv_color[HUE], &expected_hsv_color[SAT], &expected_hsv_color[VAL]);

    bu_rgb_to_hsv(rgb_color, actual_hsv_color);

    VPRINT("Result:", actual_hsv_color);

    /* Use 0.01 as tolerance to allow the numbers in CMakeLists.txt to
     * be a reasonable length.
     */
    return !(VNEAR_EQUAL(expected_hsv_color, actual_hsv_color, 0.01));
}


static int
test_bu_hsv_to_rgb(int argc, char *argv[])
{
    unsigned int expected_rgb_color[3] = {11, 22, 33};
    unsigned char actual_rgb_color[3] = {11, 22, 33};
    fastf_t hsv_color[3] = {11.0, 22.0, 33.0};

    if (argc != 4) {
	bu_exit(1, "ERROR: input format is hsv_values expected_rgb_values [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lf, %lf, %lf", &hsv_color[HUE], &hsv_color[SAT], &hsv_color[VAL]);
    sscanf(argv[3], "%u, %u, %u", &expected_rgb_color[RED], &expected_rgb_color[GRN], &expected_rgb_color[BLU]);

    bu_hsv_to_rgb(hsv_color, actual_rgb_color);

    bu_log("Result: %u, %u, %u", actual_rgb_color[RED], actual_rgb_color[GRN], actual_rgb_color[BLU]);

    return !(VEQUAL(expected_rgb_color, actual_rgb_color));
}


static int
test_bu_str_to_rgb(int argc, char *argv[])
{
    unsigned int expected_rgb_color[3] = {11, 22, 33};
    unsigned char actual_rgb_color[3] = {11, 22, 33};
    char *rgb_string;

    if (argc != 4) {
	bu_exit(1, "ERROR: input format is rgb_string expected_rgb_values [%s]\n", argv[0]);
    }

    rgb_string = argv[2];
    sscanf(argv[3], "%u, %u, %u", &expected_rgb_color[RED], &expected_rgb_color[GRN], &expected_rgb_color[BLU]);

    bu_str_to_rgb(rgb_string, actual_rgb_color);

    bu_log("Result: %u, %u, %u", actual_rgb_color[RED], actual_rgb_color[GRN], actual_rgb_color[BLU]);

    return !(VEQUAL(expected_rgb_color, actual_rgb_color));
}


static int
test_bu_color_to_rgb_floats(int argc, char *argv[])
{
    fastf_t expected_rgb_color[3] = {11.0, 22.0, 33.0};
    fastf_t actual_rgb_color[3] = {11.0, 22.0, 33.0};
    struct bu_color color = BU_COLOR_INIT_ZERO;

    if (argc != 3) {
	bu_exit(1, "ERROR: input format is rgb_color [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lf, %lf, %lf", &expected_rgb_color[RED], &expected_rgb_color[GRN], &expected_rgb_color[BLU]);

    VSCALE(color.buc_rgb, expected_rgb_color, 1.0 / 255.0);

    /* this is a simple pass-through test of bu_color_to_rgb_floats()
     * that shouldn't result in change so long as our naive
     * normalization math behaves within typical floating point fuzz..
     */

    bu_color_to_rgb_floats(&color, actual_rgb_color);

    VSCALE(actual_rgb_color, actual_rgb_color, 255.0);

    return !(VEQUAL(expected_rgb_color, actual_rgb_color));
}


static int
test_bu_color_from_rgb_floats(int argc, char *argv[])
{
    fastf_t expected_rgb_color[3] = {11.0, 22.0, 33.0};
    fastf_t actual_rgb_color[3] = {11.0, 22.0, 33.0};
    struct bu_color color = BU_COLOR_INIT_ZERO;

    if (argc != 3) {
	bu_exit(1, "ERROR: input format is rgb_color [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%lf, %lf, %lf", &expected_rgb_color[RED], &expected_rgb_color[GRN], &expected_rgb_color[BLU]);

    bu_color_from_rgb_floats(&color, expected_rgb_color);

    VSCALE(actual_rgb_color, color.buc_rgb, 255.0);

    VPRINT("Result:", actual_rgb_color);

    return !(VEQUAL(expected_rgb_color, actual_rgb_color));
}


static int
test_bu_color_to_rgb_chars(int argc, char *argv[])
{
    int expected_rgb_color[3] = {11, 22, 33};
    unsigned char actual_rgb_color[3] = {11, 22, 33};

    struct bu_color color = BU_COLOR_INIT_ZERO;

    if (argc != 3) {
	bu_exit(1, "ERROR: input format is rgb_color [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%d, %d, %d", &expected_rgb_color[RED], &expected_rgb_color[GRN], &expected_rgb_color[BLU]);

    VSCALE(color.buc_rgb, expected_rgb_color, 1.0 / 255.0);

    bu_color_to_rgb_chars(&color, actual_rgb_color);

    bu_log("Result: %d, %d, %d", actual_rgb_color[RED], actual_rgb_color[GRN], actual_rgb_color[BLU]);

    return !(VEQUAL(expected_rgb_color, actual_rgb_color));
}


static int
test_bu_color_from_rgb_chars(int argc, char *argv[])
{
    int scanned_rgb_color[3] = {11, 22, 33};
    unsigned char expected_rgb_color[3] = {11, 22, 33};
    unsigned char actual_rgb_color[3] = {11, 22, 33};
    struct bu_color color = BU_COLOR_INIT_ZERO;

    if (argc != 3) {
	bu_exit(1, "ERROR: input format is rgb_color [%s]\n", argv[0]);
    }

    sscanf(argv[2], "%d, %d, %d", &scanned_rgb_color[RED], &scanned_rgb_color[GRN], &scanned_rgb_color[BLU]);

    VMOVE(expected_rgb_color, scanned_rgb_color);

    bu_color_from_rgb_chars(&color, expected_rgb_color);
    bu_color_to_rgb_chars(&color, actual_rgb_color);

    bu_log("Result: %d, %d, %d", actual_rgb_color[RED], actual_rgb_color[GRN], actual_rgb_color[BLU]);

    return !(VEQUAL(expected_rgb_color, actual_rgb_color));
}


int
main(int argc, char *argv[])
{
    int function_num = 0;

    // Normally this file is part of bu_test, so only set this if it
    // looks like the program name is still unset.
    if (bu_getprogname()[0] == '\0')
	bu_setprogname(argv[0]);

    if (argc < 2) {
	bu_log("Usage: %s {function_num} {function_test_arg0} {...}", argv[0]);
	bu_exit(1, "ERROR: missing function number\n");
    }

    sscanf(argv[1], "%d", &function_num);

    switch (function_num) {
	case 1:
	    return test_bu_rgb_to_hsv(argc, argv);
	case 2:
	    return test_bu_hsv_to_rgb(argc, argv);
	case 3:
	    return test_bu_str_to_rgb(argc, argv);
	case 4:
	    return test_bu_color_to_rgb_floats(argc, argv);
	case 5:
	    return test_bu_color_from_rgb_floats(argc, argv);
	case 6:
	    return test_bu_color_to_rgb_chars(argc, argv);
	case 7:
	    return test_bu_color_from_rgb_chars(argc, argv);
    }

    bu_log("ERROR: function_num %d is not valid [%s]\n", function_num, argv[0]);
    return 1;
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

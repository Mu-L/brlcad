/*                       R E A D F L T . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2025 United States Government as represented by
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
/** @file iges/readflt.c
 *
 * This routine reads the next field in "card" buffer.  It expects the
 * field to contain a string representing a "float".  The string is
 * read and converted to type "float" and returned in "inum".  If "id"
 * is not the null string, then "id" is printed followed by the
 * number.
 *
 * "eofd" is the "end-of-field" delimiter
 * "eord" is the "end-of-record" delimiter
 *
 */

#include "./iges_struct.h"
#include "./iges_extern.h"

#define MAX_NUM 4096


void
Readflt(fastf_t *inum, char *id)
{
    int i = 0, done = 0, lencard;
    char num[MAX_NUM] = {0};

    if (card[counter] == eofd) {
	/* This is an empty field */
	counter++;
	return;
    } else if (card[counter] == eord) /* Up against the end of record */
	return;

    if (card[72] == 'P')
	lencard = PARAMLEN;
    else
	lencard = CARDLEN;

    if (counter >= lencard)
	Readrec(++currec);

    while (!done && i < MAX_NUM-1) {
	while (i < MAX_NUM-1 &&
	       (num[i] = card[counter++]) != eofd &&
	       num[i] != eord && counter <= lencard)
	{
	    if (num[i] == 'D') {
		num[i] = 'e';
	    }
	    if (i >= MAX_NUM-1) {
		done = 1;
	    }
	    i++;
	}

	if (counter > lencard && num[i] != eord && num[i] != eofd) {
	    Readrec(++currec);
	} else {
	    done = 1;
	}
    }

    if (num[i] == eord)
	counter--;

    *inum = atof(num);
    if (*id != '\0')
	bu_log("%s%g\n", id, *inum);
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

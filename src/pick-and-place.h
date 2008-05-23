/*
 * gEDA - GNU Electronic Design Automation
 * This files is a part of gerbv.
 *
 *   Copyright (C) 2000-2001 Stefan Petersen (spe@stacken.kth.se)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 */

/** \file pick-and-place.h
    \brief Header info for the PNP (pick-and-place) parsing functions
    \ingroup libgerbv
*/

#ifndef GERBV_LAYERTYPE_PICKANDPLACE_H
#define GERBV_LAYERTYPE_PICKANDPLACE_H

#include <glib.h>
 
#define MAXL 200

typedef struct gerb_transf {
    double r_mat[2][2];
    double scale;
    double offset[2];
} gerbv_transf_t;

enum e_footprint {
    PART_SHAPE_UNKNOWN = 0, /* drawn as circle with line*/
    PART_SHAPE_RECTANGLE = 1, /* rectangle with one side marked*/
    PART_SHAPE_STD = 2 /* rectangle with one corner marked*/
};

typedef struct {
    char     designator[MAXL];
    char     footprint[MAXL];
    double   mid_x;
    double   mid_y;
    double   ref_x;
    double   ref_y;
    double   pad_x;
    double   pad_y;
    char     layer[MAXL]; /*T is top B is bottom*/
    double   rotation;
    char     comment[MAXL];    
    int      shape;
    double   width;
    double   length;
    unsigned int nuf_push;  /* Nuf pushes to estimate stack size */
} PnpPartData;

GArray *pick_and_place_parse_file (gerb_file_t *fd);

gerbv_image_t *pick_and_place_parse_file_to_image (gerb_file_t *fd);

void
pick_and_place_parse_file_to_images (gerb_file_t *fd, gerbv_image_t **topImage,
			gerbv_image_t **bottomImage);

gboolean
pick_and_place_check_file_type(gerb_file_t *fd, gboolean *returnFoundBinary);

#endif /* GERBV_LAYERTYPE_PICKANDPLACE_H */

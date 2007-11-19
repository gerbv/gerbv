/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2000-2002 Stefan Petersen (spe@stacken.kth.se)
 *
 * $Id$
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

#ifndef DRAW_H
#define DRAW_H

#include <gdk/gdk.h>
#include "gerber.h"

/* Default mouse cursor. Perhaps redefine this to a variable later? */
#define GERBV_DEF_CURSOR	NULL

typedef enum {
	CIRCLE_EXPOSURE,
	CIRCLE_DIAMETER
} AMACRO_CIRCLE_INDEX;

typedef enum {
	OUTLINE_EXPOSURE,
	OUTLINE_NUMBER_OF_POINTS,
	OUTLINE_FIRST_X,
	OUTLINE_FIRST_Y,
	OUTLINE_ROTATION
} AMACRO_OUTLINE_INDEX;

typedef enum {
	POLYGON_EXPOSURE,
	POLYGON_NUMBER_OF_POINTS,
	POLYGON_CENTER_X,
	POLYGON_CENTER_Y,
	POLYGON_DIAMETER,
	POLYGON_ROTATION
} AMACRO_POLYGON_INDEX;

typedef enum {
	MOIRE_CENTER_X,
	MOIRE_CENTER_Y,
	MOIRE_OUTSIDE_DIAMETER,
	MOIRE_CIRCLE_THICKNESS,
	MOIRE_GAP_WIDTH,
	MOIRE_NUMBER_OF_CIRCLES,
	MOIRE_CROSSHAIR_THICKNESS,
	MOIRE_CROSSHAIR_LENGTH,
	MOIRE_ROTATION
} AMACRO_MOIRE_INDEX;

typedef enum {
	THERMAL_CENTER_X,
	THERMAN_CENTER_Y,
	THERMAL_OUTSIDE_DIAMETER,
	THERMAL_INSIDE_DIAMETER,
	THERMAL_CROSSHAIR_THICKNESS,
	THERMAL_ROTATION
} AMACRO_THERMAL_INDEX;
    
typedef enum {
	LINE20_EXPOSURE,
	LINE20_LINE_WIDTH,
	LINE20_START_X,
	LINE20_START_Y,
	LINE20_END_X,
	LINE20_END_Y,
	LINE20_ROTATION
} AMACRO_LINE20_INDEX;

enum {
	LINE21_EXPOSURE,
	LINE21_WIDTH,
	LINE21_HEIGHT,
	LINE21_CENTER_X,
	LINE21_CENTER_Y,
	LINE21_ROTATION
} AMACRO_LINE21_INDEX;

enum {
	LINE22_EXPOSURE,
	LINE22_WIDTH,
	LINE22_HEIGHT,
	LINE22_LOWER_LEFT_X,
	LINE22_LOWER_LEFT_Y,
	LINE22_ROTATION
} AMACRO_LINE22_INDEX;

/*
 * Convert a gerber image to a GDK clip mask to be used when creating pixmap
 */
int 
image2pixmap(GdkPixmap **pixmap, struct gerb_image *image, 
	     int scale, double trans_x, double trans_y,
	     enum polarity_t polarity);


#endif /* DRAW_H */

int
render_image_to_cairo_target (cairo_t *cairoTarget, struct gerb_image *image);

/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2000-2001 Stefan Petersen (spe@stacken.kth.se)
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

#include <stdio.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include "color.h"

/*
 * Allocates a color in the systems colormap.
 */
GdkColor *
alloc_color(int r, int g, int b, char *colorname)
{
    GdkColor *color;

    if (r < 0 || r > MAX_COLOR_RESOLUTION)
	fprintf(stderr, "Red out range: %d\n", r);
    if (g < 0 || g > MAX_COLOR_RESOLUTION)
	fprintf(stderr, "Green out range: %d\n", g);
    if (b < 0 || b > MAX_COLOR_RESOLUTION)
	fprintf(stderr, "Blue out range: %d\n", b);

    color = (struct _GdkColor *)malloc(sizeof(struct _GdkColor));
    if (color == NULL)
	return NULL;

    if (colorname)
	gdk_color_parse(colorname, color);
    else {
	color->pixel = 0;
	color->red   = (gushort)r;
	color->green = (gushort)g;
	color->blue  = (gushort)b;
    }
    gdk_colormap_alloc_color(gdk_colormap_get_system(), color, FALSE, TRUE);
    
    return color;
} /* alloc_color */

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

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include "color.h"

void
alloc_colors(gerbv_color_t colors[], int nuf_colors,
	     gerbv_color_t *background)
{
    int i;
    GdkColormap *colormap = gdk_colormap_get_system();
    
    for (i = 0; i < nuf_colors; i++) {
	colors[i].color = (struct _GdkColor *)malloc(sizeof(struct _GdkColor));
	gdk_color_parse(colors[i].name, colors[i].color);
	gdk_colormap_alloc_color(colormap, colors[i].color, FALSE, TRUE);
    }
    
    background->color = (struct _GdkColor *)malloc(sizeof(struct _GdkColor));
    gdk_color_parse(background->name, background->color);
    gdk_colormap_alloc_color(colormap, background->color, FALSE, TRUE);
    
    return;
} /* alloc_colors */


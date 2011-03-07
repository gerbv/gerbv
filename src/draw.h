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

/** \file draw.h
    \brief Header info for the cairo rendering functions and the related 
selection calculating functions
    \ingroup gerbv
*/

#ifndef DRAW_H
#define DRAW_H

#include <gdk/gdk.h>
#include <cairo.h>
#include <cairo-ps.h>
#include <cairo-svg.h>
#include <cairo-pdf.h>

#endif /* DRAW_H */


/*
 * Convert a gerber image to a GDK clip mask to be used when creating pixmap
 */
int
draw_image_to_cairo_target (cairo_t *cairoTarget, gerbv_image_t *image,
					gdouble pixelWidth,
					gchar drawMode, gerbv_selection_info_t *selectionInfo,
					gerbv_render_info_t *renderInfo, gboolean allowOptimization,
 					gerbv_user_transformation_t transform, gboolean pixelOutput);




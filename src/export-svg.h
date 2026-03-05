/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2026 Source Parts contributors
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

/** \file export-svg.h
    \brief Header for optimized SVG export that bypasses Cairo's SVG surface
    \ingroup libgerbv
*/

#ifndef EXPORT_SVG_H
#define EXPORT_SVG_H

#include "gerbv.h"

#define SVG_MAX_STYLES 256
#define SVG_COORD_PRECISION 3

/** A unique fill/stroke style, emitted once in <style> and referenced by class name */
typedef struct svg_style_class {
	gdouble fill_r, fill_g, fill_b;
	gdouble stroke_r, stroke_g, stroke_b;
	gdouble stroke_width;
	gint    stroke_linecap;       /* 0=butt, 1=round, 2=square */
	gboolean has_fill;
	gboolean has_stroke;
	gchar   class_name[16];       /* "c0", "c1", ... */
} svg_style_class_t;

/** Registry of all unique styles discovered during rendering */
typedef struct svg_style_registry {
	svg_style_class_t classes[SVG_MAX_STYLES];
	gint count;
} svg_style_registry_t;

/** Main context for the custom SVG writer */
typedef struct svg_writer_ctx {
	GString              *body;
	GString              *defs;       /* <defs> content: styles + aperture symbols */
	svg_style_registry_t  styles;
	gerbv_render_info_t  *renderInfo;
	gint                  id_counter;
	gdouble               bg_r, bg_g, bg_b;
	gint                  indent;
} svg_writer_ctx_t;

/** Render the entire project to an optimized SVG file */
void export_svg_render_project (gerbv_project_t *project,
		gerbv_render_info_t *renderInfo,
		const gchar *filename,
		gboolean exportLayersAsSvgLayers);

#endif /* EXPORT_SVG_H */

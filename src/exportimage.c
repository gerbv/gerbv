/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2000-2002 Stefan Petersen (spe@stacken.kth.se)
 *
 * Contributed by Dino Ghilardi <dino.ghilardi@ieee.org> 
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

/** \file exportimage.c
    \brief This file contains image exporting functions for exporting to PNG, PDF, SVG, and Postscript formats
    \ingroup libgerbv
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <math.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <png.h>

#include "gerbv.h"

#include "draw.h"
#include <cairo.h>
#include <cairo-pdf.h>
#include <cairo-ps.h>
#include <cairo-svg.h>

extern gerbv_render_info_t screenRenderInfo;

void exportimage_render_to_surface_and_destroy (gerbv_project_t *gerbvProject,
		cairo_surface_t *cSurface, gerbv_render_info_t *renderInfo, gchar const* filename) {
      cairo_t *cairoTarget = cairo_create (cSurface);
      
      gerbv_render_all_layers_to_cairo_target_for_vector_output (gerbvProject, cairoTarget, renderInfo);
	cairo_destroy (cairoTarget);
	cairo_surface_destroy (cSurface);
}

gerbv_render_info_t gerbv_export_autoscale_project (gerbv_project_t *gerbvProject) {
	gerbv_render_size_t bb;
	gerbv_render_get_boundingbox(gerbvProject, &bb);
	// add a border around the bounding box
	gfloat tempWidth = bb.right  - bb.left;
	gfloat tempHeight = bb.bottom - bb.top;
	bb.right += (tempWidth*0.05);
	bb.left -= (tempWidth*0.05);
	bb.bottom += (tempHeight*0.05);
	bb.top -= (tempHeight*0.05);
	float width  = bb.right  - bb.left + 0.001;	// Plus a little extra to prevent from 
	float height = bb.bottom - bb.top + 0.001; // missing items due to round-off errors

	gerbv_render_info_t renderInfo = {72, 72, bb.left, bb.top, 3, width*72, height*72};
	return renderInfo;
}

void gerbv_export_png_file_from_project_autoscaled (gerbv_project_t *gerbvProject, int widthInPixels,
		int heightInPixels, gchar const* filename) {
	gerbv_render_info_t renderInfo = {1, 1, 0, 0, 3, widthInPixels, heightInPixels};

	gerbv_render_zoom_to_fit_display (gerbvProject, &renderInfo);
	gerbv_export_png_file_from_project (gerbvProject, &renderInfo, filename);
}

void gerbv_export_png_file_from_project (gerbv_project_t *gerbvProject, gerbv_render_info_t *renderInfo, gchar const* filename) {
	cairo_surface_t *cSurface = cairo_image_surface_create  (CAIRO_FORMAT_ARGB32,
                                                         renderInfo->displayWidth, renderInfo->displayHeight);
	cairo_t *cairoTarget = cairo_create (cSurface);
	gerbv_render_all_layers_to_cairo_target (gerbvProject, cairoTarget, renderInfo);
	cairo_surface_write_to_png (cSurface, filename);
	cairo_destroy (cairoTarget);
	cairo_surface_destroy (cSurface);
}

void gerbv_export_pdf_file_from_project_autoscaled (gerbv_project_t *gerbvProject, gchar const* filename) {
	gerbv_render_info_t renderInfo = gerbv_export_autoscale_project(gerbvProject);
	gerbv_export_pdf_file_from_project (gerbvProject, &renderInfo, filename);
}

void gerbv_export_pdf_file_from_project (gerbv_project_t *gerbvProject, gerbv_render_info_t *renderInfo,
		gchar const* filename) {
	cairo_surface_t *cSurface = cairo_pdf_surface_create (filename, renderInfo->displayWidth,
								renderInfo->displayHeight);

      exportimage_render_to_surface_and_destroy (gerbvProject, cSurface, renderInfo, filename);
}

void gerbv_export_postscript_file_from_project_autoscaled (gerbv_project_t *gerbvProject, gchar const* filename) {
	gerbv_render_info_t renderInfo = gerbv_export_autoscale_project(gerbvProject);
	gerbv_export_postscript_file_from_project (gerbvProject, &renderInfo, filename);
}

void gerbv_export_postscript_file_from_project (gerbv_project_t *gerbvProject, gerbv_render_info_t *renderInfo,
		gchar const* filename) {
	cairo_surface_t *cSurface = cairo_ps_surface_create (filename, renderInfo->displayWidth,
								renderInfo->displayHeight);
      exportimage_render_to_surface_and_destroy (gerbvProject, cSurface, renderInfo, filename);
}

void gerbv_export_svg_file_from_project_autoscaled (gerbv_project_t *gerbvProject, gchar const* filename) {
	gerbv_render_info_t renderInfo = gerbv_export_autoscale_project(gerbvProject);
	gerbv_export_svg_file_from_project (gerbvProject, &renderInfo, filename);
}

void gerbv_export_svg_file_from_project (gerbv_project_t *gerbvProject, gerbv_render_info_t *renderInfo,
		gchar const* filename) {
	cairo_surface_t *cSurface = cairo_svg_surface_create (filename, renderInfo->displayWidth,
								renderInfo->displayHeight);
      exportimage_render_to_surface_and_destroy (gerbvProject, cSurface, renderInfo, filename);
}


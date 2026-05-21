/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2026 Source Parts Contributors
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

/** \file export-diff.c
    \brief Layer diff/compare export using Cairo alpha-mask compositing
    \ingroup libgerbv
*/

#include "export-diff.h"
#include "common.h"

#include <cairo.h>

void
gerbv_diff_options_init(gerbv_diff_options_t *opts) {
	/* Red for removed geometry */
	opts->removed_color.red   = 0xCC00;
	opts->removed_color.green = 0x0000;
	opts->removed_color.blue  = 0x0000;

	/* Green for added geometry */
	opts->added_color.red   = 0x0000;
	opts->added_color.green = 0x8800;
	opts->added_color.blue  = 0x0000;

	/* Dim gray for unchanged geometry */
	opts->unchanged_color.red   = 0x9999;
	opts->unchanged_color.green = 0x9999;
	opts->unchanged_color.blue  = 0x9999;

	opts->show_unchanged = TRUE;
}

static cairo_surface_t *
render_layer_mask(gerbv_fileinfo_t *fileInfo, gerbv_render_info_t *renderInfo) {
	int w = renderInfo->displayWidth;
	int h = renderInfo->displayHeight;

	cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	cairo_t *cr = cairo_create(surface);

	/* Transparent background (default for ARGB32) */
	cairo_save(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
	cairo_paint(cr);
	cairo_restore(cr);

	/* Temporarily override layer color to opaque white so geometry
	   renders as a solid alpha mask */
	GdkColor savedColor = fileInfo->color;
	guint16 savedAlpha = fileInfo->alpha;

	fileInfo->color.red   = G_MAXUINT16;
	fileInfo->color.green = G_MAXUINT16;
	fileInfo->color.blue  = G_MAXUINT16;
	fileInfo->alpha = G_MAXUINT16;

	gerbv_render_layer_to_cairo_target(cr, fileInfo, renderInfo);

	fileInfo->color = savedColor;
	fileInfo->alpha = savedAlpha;

	cairo_destroy(cr);
	return surface;
}

static cairo_surface_t *
copy_surface(cairo_surface_t *src) {
	int w = cairo_image_surface_get_width(src);
	int h = cairo_image_surface_get_height(src);

	cairo_surface_t *dst = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	cairo_t *cr = cairo_create(dst);
	cairo_set_source_surface(cr, src, 0, 0);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cr);
	cairo_destroy(cr);
	return dst;
}

static void
paint_mask_with_color(cairo_t *cr, cairo_surface_t *mask, const GdkColor *color) {
	cairo_set_source_rgb(cr,
		(double)color->red   / G_MAXUINT16,
		(double)color->green / G_MAXUINT16,
		(double)color->blue  / G_MAXUINT16);
	cairo_mask_surface(cr, mask, 0, 0);
}

void
gerbv_export_diff_png(gerbv_fileinfo_t *fileInfoA,
                      gerbv_fileinfo_t *fileInfoB,
                      gerbv_render_info_t *renderInfo,
                      const gerbv_diff_options_t *opts,
                      const gchar *filename) {
	int w = renderInfo->displayWidth;
	int h = renderInfo->displayHeight;

	/* 1. Render each layer as an alpha mask */
	cairo_surface_t *maskA = render_layer_mask(fileInfoA, renderInfo);
	cairo_surface_t *maskB = render_layer_mask(fileInfoB, renderInfo);

	/* 2. Compute A_only = A DEST_OUT B (geometry in A but not B = removed) */
	cairo_surface_t *aOnly = copy_surface(maskA);
	{
		cairo_t *cr = cairo_create(aOnly);
		cairo_set_source_surface(cr, maskB, 0, 0);
		cairo_set_operator(cr, CAIRO_OPERATOR_DEST_OUT);
		cairo_paint(cr);
		cairo_destroy(cr);
	}

	/* 3. Compute B_only = B DEST_OUT A (geometry in B but not A = added) */
	cairo_surface_t *bOnly = copy_surface(maskB);
	{
		cairo_t *cr = cairo_create(bOnly);
		cairo_set_source_surface(cr, maskA, 0, 0);
		cairo_set_operator(cr, CAIRO_OPERATOR_DEST_OUT);
		cairo_paint(cr);
		cairo_destroy(cr);
	}

	/* 4. Compute intersection = A IN B (geometry in both = unchanged) */
	cairo_surface_t *intersection = NULL;
	if (opts->show_unchanged) {
		intersection = copy_surface(maskA);
		cairo_t *cr = cairo_create(intersection);
		cairo_set_source_surface(cr, maskB, 0, 0);
		cairo_set_operator(cr, CAIRO_OPERATOR_IN);
		cairo_paint(cr);
		cairo_destroy(cr);
	}

	/* 5. Compose final image: white background + colored regions */
	cairo_surface_t *final = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	cairo_t *cr = cairo_create(final);

	/* White background */
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_paint(cr);

	/* Paint unchanged first (underneath), then removed and added on top */
	if (opts->show_unchanged && intersection) {
		paint_mask_with_color(cr, intersection, &opts->unchanged_color);
	}
	paint_mask_with_color(cr, aOnly, &opts->removed_color);
	paint_mask_with_color(cr, bOnly, &opts->added_color);

	/* 6. Write to PNG */
	if (CAIRO_STATUS_SUCCESS != cairo_surface_write_to_png(final, filename)) {
		GERB_COMPILE_ERROR(_("Exporting error to file \"%s\""), filename);
	}

	/* Cleanup */
	cairo_destroy(cr);
	cairo_surface_destroy(final);
	if (intersection)
		cairo_surface_destroy(intersection);
	cairo_surface_destroy(bOnly);
	cairo_surface_destroy(aOnly);
	cairo_surface_destroy(maskB);
	cairo_surface_destroy(maskA);
}

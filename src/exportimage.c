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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <math.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <png.h>
#include "gerbv_screen.h"
#include "render.h"
#include "exportimage.h"

#ifdef RENDER_USING_GDK
  #include "draw-gdk.h"
#else
  #include "draw.h"
  #include <cairo.h>
#endif

#include "gerbv_screen.h"

extern gerbv_render_info_t screenRenderInfo;

#ifdef RENDER_USING_GDK
static gboolean 
exportimage_save_pixbuf_to_file (GdkPixbuf *pixbuf, char *filename);
#endif

#ifndef RENDER_USING_GDK
void exportimage_render_to_surface_and_destroy (cairo_surface_t *cSurface, gerbv_render_info_t *renderInfo,
						gchar *filename) {
      cairo_t *cairoTarget = cairo_create (cSurface);
      
      render_all_layers_to_cairo_target_for_vector_output (cairoTarget, renderInfo);
	cairo_destroy (cairoTarget);
	cairo_surface_destroy (cSurface);
}


#endif

#ifdef EXPORT_PNG
void exportimage_export_to_png_file_autoscaled (int widthInPixels, int heightInPixels, gchar *filename) {
	gerbv_render_info_t renderInfo = {1.0, 0, 0, 1.0, widthInPixels, heightInPixels};
	
	render_zoom_to_fit_display (&renderInfo);
	exportimage_export_to_png_file (&renderInfo, filename);
}

void exportimage_export_to_png_file (gerbv_render_info_t *renderInfo, gchar *filename) {
#ifdef RENDER_USING_GDK

	GdkPixmap *renderedPixmap = gdk_pixmap_new (NULL, renderInfo->displayWidth,
								renderInfo->displayHeight, 24);
	GdkColormap *colormap=NULL;
	GdkPixbuf *tempPixbuf=NULL;
	
	render_to_pixmap_using_gdk (renderedPixmap, renderInfo);
	colormap = gdk_drawable_get_colormap(renderedPixmap);
	if ((tempPixbuf = gdk_pixbuf_get_from_drawable(tempPixbuf, renderedPixmap, 
					      colormap, 0, 0, 0, 0, 
					      renderInfo->displayWidth, renderInfo->displayHeight))) {
		exportimage_save_pixbuf_to_file (tempPixbuf, filename);
		gdk_pixbuf_unref(tempPixbuf);
	}
	gdk_pixmap_unref(renderedPixmap);
	
	//png_export (NULL, filename);
#else
	cairo_surface_t *cSurface = cairo_image_surface_create  (CAIRO_FORMAT_ARGB32,
                                                         renderInfo->displayWidth, renderInfo->displayHeight);

      cairo_t *cairoTarget = cairo_create (cSurface);
      
      render_all_layers_to_cairo_target (cairoTarget, renderInfo);
	cairo_surface_write_to_png (cSurface, filename);
	cairo_destroy (cairoTarget);
	cairo_surface_destroy (cSurface);
#endif
}
#endif

void exportimage_export_to_pdf_file_autoscaled (int widthInPoints, int heightInPoints, gchar *filename) {
	gerbv_render_info_t renderInfo = {1.0, 0, 0, 1.0, widthInPoints, heightInPoints};
	
	render_zoom_to_fit_display (&renderInfo);
	exportimage_export_to_pdf_file (&renderInfo, filename);
}

void exportimage_export_to_pdf_file (gerbv_render_info_t *renderInfo, gchar *filename) {
#ifndef RENDER_USING_GDK
	cairo_surface_t *cSurface = cairo_pdf_surface_create (filename, renderInfo->displayWidth,
								renderInfo->displayHeight);

      exportimage_render_to_surface_and_destroy (cSurface, renderInfo, filename);
#endif
}

void exportimage_export_to_postscript_file_autoscaled (int widthInPoints, int heightInPoints, gchar *filename) {
	gerbv_render_info_t renderInfo = {1.0, 0, 0, 1.0, widthInPoints, heightInPoints};
	
	render_zoom_to_fit_display (&renderInfo);
	exportimage_export_to_postscript_file (&renderInfo, filename);
}

void exportimage_export_to_postscript_file (gerbv_render_info_t *renderInfo, gchar *filename) {
#ifndef RENDER_USING_GDK
	cairo_surface_t *cSurface = cairo_ps_surface_create (filename, renderInfo->displayWidth,
								renderInfo->displayHeight);
      exportimage_render_to_surface_and_destroy (cSurface, renderInfo, filename);
#endif
}

void exportimage_export_to_svg_file_autoscaled (int widthInPoints, int heightInPoints, gchar *filename) {
	gerbv_render_info_t renderInfo = {1.0, 0, 0, 1.0, widthInPoints, heightInPoints};
	
	render_zoom_to_fit_display (&renderInfo);
	exportimage_export_to_svg_file (&renderInfo, filename);
}

void exportimage_export_to_svg_file (gerbv_render_info_t *renderInfo, gchar *filename) {
#ifndef RENDER_USING_GDK
	cairo_surface_t *cSurface = cairo_svg_surface_create (filename, renderInfo->displayWidth,
								renderInfo->displayHeight);
      exportimage_render_to_surface_and_destroy (cSurface, renderInfo, filename);
#endif
}

#ifdef EXPORT_PNG
#ifdef RENDER_USING_GDK
static gboolean 
exportimage_save_pixbuf_to_file (GdkPixbuf *pixbuf, char *filename)
{
	FILE *handle;
	int width, height, depth, rowstride;
  	guchar *pixels;
  	png_structp png_ptr;
  	png_infop info_ptr;
  	png_text text[2];
	png_byte **row_ptr;
  	int i;

	g_return_val_if_fail (pixbuf != NULL, FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);
	g_return_val_if_fail (filename[0] != '\0', FALSE);

        handle = fopen (filename, "wb");
        if (handle == NULL) {
	    return FALSE;
	}

	png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL) {
	    fclose (handle);
	    return FALSE;
	}

	info_ptr = png_create_info_struct (png_ptr);
	if (info_ptr == NULL) {
	    png_destroy_write_struct (&png_ptr, (png_infopp)NULL);
	    fclose (handle);
	    return FALSE;
	}

	png_init_io (png_ptr, handle);
	
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	depth = gdk_pixbuf_get_bits_per_sample (pixbuf);
	pixels = gdk_pixbuf_get_pixels (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	
	png_set_IHDR (png_ptr, info_ptr, width, height,
		      depth, PNG_COLOR_TYPE_RGB,
		      PNG_INTERLACE_NONE,
		      PNG_COMPRESSION_TYPE_DEFAULT,
		      PNG_FILTER_TYPE_DEFAULT);
	
	/* Some text to go with the png image */
	text[0].key = "Title";
	text[0].text = filename;
	text[0].compression = PNG_TEXT_COMPRESSION_NONE;
	text[1].key = "Generator";
	text[1].text = "gerbv";
	text[1].compression = PNG_TEXT_COMPRESSION_NONE;
	png_set_text (png_ptr, info_ptr, text, 2);
	
	/* Write header data */
	png_write_info (png_ptr, info_ptr);
	
	/* Build up a vector of row pointers */	
	row_ptr = (png_byte **)g_malloc(height * sizeof(png_byte *));
	for (i = 0; i < height; i++) {
	    row_ptr[i] = (png_byte *)pixels;
	    pixels += rowstride;
	}

	/* Write it and free the row vector */
	png_write_image(png_ptr, row_ptr);
	g_free(row_ptr);

	/* Finish of PNG writing */
	png_write_end (png_ptr, info_ptr);
	png_destroy_write_struct (&png_ptr, &info_ptr);
	
	fclose (handle);
	return TRUE;
} /* pixbuf_to_file_as_png */
#endif
#endif

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

/*
 * Included only if requested at configure time
 */
#ifdef EXPORT_PNG

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <png.h>

#include "exportimage.h"

/* Function prototypes */
static gboolean pixbuf_to_file_as_png (GdkPixbuf *pixbuf, char *filename);

gboolean
png_export(GdkPixmap* imagetosave, char* filename)
{
	GdkPixbuf *tempPixBuf=NULL;
	GdkColormap *colormap=NULL; 
	int width = 0, height = 0;
	gboolean result = FALSE;

	colormap = gdk_colormap_get_system();
	if (colormap == NULL)
	    return FALSE;

	gdk_window_get_size(imagetosave, &width, &height);

	tempPixBuf = 
	    gdk_pixbuf_get_from_drawable(tempPixBuf, imagetosave, colormap,
					 0, 0, 0, 0, width, height);
	if (tempPixBuf == NULL)
	    return FALSE;

	result = pixbuf_to_file_as_png (tempPixBuf, filename);
	
	gdk_pixbuf_unref(tempPixBuf);
	gdk_colormap_unref(colormap);

	return result;
}

/*The following routine has been taken from GQView 1.0.2 (comment included)*/

/*
 *-----------------------------------------------------------------------------
 * simple png save (please read comment)
 *-----------------------------------------------------------------------------
 */

/*
 * This save_pixbuf_to_file utility was copied from the nautilus 0.1 preview,
 * and judging by the message they got it from gnome-iconedit.
 *
 * I have only changed the source to match my coding style in GQview,
 * make it work in here, and rename to pixbuf_to_file_as_png
 *                                                                   -John
 *
 * === message in nautilus source:
 * utility routine for saving a pixbuf to a png file.
 * This was adapted from Iain Holmes' code in gnome-iconedit, and probably
 * should be in a utility library, possibly in gdk-pixbuf itself.
 * ===
 */
static gboolean 
pixbuf_to_file_as_png (GdkPixbuf *pixbuf, char *filename)
{
	FILE *handle;
  	char *buffer;
	gboolean has_alpha;
	int width, height, depth, rowstride;
  	guchar *pixels;
  	png_structp png_ptr;
  	png_infop info_ptr;
  	png_text text[2];
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
	
        has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	depth = gdk_pixbuf_get_bits_per_sample (pixbuf);
	pixels = gdk_pixbuf_get_pixels (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	
	png_set_IHDR (png_ptr, info_ptr, width, height,
		      depth, PNG_COLOR_TYPE_RGB_ALPHA,
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
	
	/* if there is no alpha in the data, allocate buffer to expand into */
	if (has_alpha)
	    buffer = NULL;
	else
	    buffer = g_malloc(4 * width);
	
	/* pump the raster data into libpng, one scan line at a time */	
	for (i = 0; i < height; i++) {
	    if (has_alpha) {
		png_bytep row_pointer = pixels;
		png_write_row (png_ptr, row_pointer);
	    } else {
		/* expand RGB to RGBA using an opaque alpha value */
		int x;
		char *buffer_ptr = buffer;
		char *source_ptr = pixels;
		for (x = 0; x < width; x++) {
		    *buffer_ptr++ = *source_ptr++;
		    *buffer_ptr++ = *source_ptr++;
		    *buffer_ptr++ = *source_ptr++;
		    *buffer_ptr++ = 255;
		}
		png_write_row (png_ptr, (png_bytep) buffer);		
	    }
	    pixels += rowstride;
	}
	
	png_write_end (png_ptr, info_ptr);
	png_destroy_write_struct (&png_ptr, &info_ptr);
	
	g_free (buffer);
	
	fclose (handle);
	return TRUE;
}

#endif /* EXPORT_PNG */

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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <math.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <png.h>

#include "exportimage.h"
#include "draw.h"
#include "gerbv_screen.h"

/* Function prototypes */
static gboolean pixbuf_to_file_as_png (GdkPixbuf *pixbuf, char *filename);

gboolean
png_export(GdkPixmap* imagetosave, char* filename)
{
	GdkPixbuf *tempPixBuf=NULL;
	GdkColormap *colormap=NULL; 
	GdkPixmap *curr_pixmap = NULL, *out_pixmap = NULL, *clipmask = NULL;
	GdkGC *gc = NULL;
	int width = 0, height = 0, i;
	double dmin_x, dmin_y, dmax_x, dmax_y;
	gboolean result = FALSE;
	
	colormap = gdk_colormap_get_system();
	if (colormap == NULL)
	    return FALSE;
	
	if (imagetosave) {
	    gdk_window_get_size(imagetosave, &width, &height);
	    tempPixBuf = gdk_pixbuf_get_from_drawable(tempPixBuf, imagetosave, 
						      colormap, 
						      0, 0, 0, 0, 
						      width, height);
	} else {

	    /*
	     * Max and min of image gives the size of the image.
	     */
	    dmax_x = screen.gerber_bbox.x2;
	    dmax_y = screen.gerber_bbox.y2;
	    dmin_x = screen.gerber_bbox.x1;
	    dmin_y = screen.gerber_bbox.y1;
	    
	    /*
	     * Width and height to scaled full size.
	     */
	    width = (int)floor((dmax_x - dmin_x) * 
			       (double)screen.scale);
	    height = (int)floor((dmax_y - dmin_y) * 
			    (double)screen.scale);
	    
	    /*
	     * Allocate the pixmap and the clipmask (a one pixel pixmap)
	     */
	    gc = gdk_gc_new(screen.drawing_area->window);
	    curr_pixmap = gdk_pixmap_new(screen.drawing_area->window, width, height,  -1);
	    out_pixmap = gdk_pixmap_new(screen.drawing_area->window, width, height,  -1);
	    clipmask = gdk_pixmap_new(screen.drawing_area->window, width, height,  1);
	    
	    /* 
	     * This now allows drawing several layers on top of each other.
	     * Higher layer numbers have higher priority in the Z-order. 
	     */
	    for(i = 0; i < MAX_FILES; i++) {
		if (GTK_TOGGLE_BUTTON(screen.layer_button[i])->active &&
		    screen.file[i]) {
		
		    /*
		     * Fill up image with all the foreground color. Excess
		     * pixels will be removed by clipmask.
		     */
		    gdk_gc_set_foreground(gc, screen.file[i]->color);
		    gdk_draw_rectangle(curr_pixmap, gc, TRUE, 0, 0, -1, -1);
		    
		    /*
		     * Translation is to get it inside the allocated pixmap,
		     * which is not always centered perfectly for GTK/X.
		     */
		    image2pixmap(&clipmask, screen.file[i]->image, screen.scale, 
				 -dmin_x*screen.scale,
				 dmax_y*screen.scale,
				 screen.file[i]->image->info->polarity);
		    /* 
		     * Set clipmask and draw the clipped out image onto the
		     * screen pixmap. Afterwards we remove the clipmask, else
		     * it will screw things up when run this loop again.
		     */
		    gdk_gc_set_clip_mask(gc, clipmask);
		    gdk_gc_set_clip_origin(gc, 0, 0);
		    gdk_draw_pixmap(out_pixmap, gc, curr_pixmap, 0,	0,
				    0, 0, -1, -1);
		    gdk_gc_set_clip_mask(gc, NULL);
		}
	    }
	    tempPixBuf = gdk_pixbuf_get_from_drawable(tempPixBuf, out_pixmap,
						      colormap,
						      0, 0, 0, 0, 
						      width, height);
	}

	if (tempPixBuf == NULL)
	    return FALSE;

	result = pixbuf_to_file_as_png (tempPixBuf, filename);
	
	gdk_pixbuf_unref(tempPixBuf);
	gdk_colormap_unref(colormap);
	if (curr_pixmap) gdk_pixmap_unref(curr_pixmap);
	if (out_pixmap) gdk_pixmap_unref(out_pixmap);
	if (clipmask) gdk_pixmap_unref(clipmask);
	if (gc) gdk_gc_unref(gc);

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

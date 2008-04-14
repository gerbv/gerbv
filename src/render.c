/*
 * gEDA - GNU Electronic Design Automation
 *
 * render.c -- this file is a part of gerbv.
 *
 *   Copyright (C) 2007 Stuart Brorson (SDB@cloud9.net)
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
#  include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_LIBGEN_H
#include <libgen.h> /* dirname */
#endif

#include <math.h>
#include "gerber.h"
#include "drill.h"
#include "gerb_error.h"
#include "gerb_stats.h"
#include "drill_stats.h"

#ifdef RENDER_USING_GDK
  #include "draw-gdk.h"
#else
  #ifdef WIN32
    #include <cairo-win32.h>
  #else
    #include <cairo-xlib.h>
  #endif
  #include <cairo.h>
  #include "draw-gdk.h"
  #include "draw.h"
#endif

#include "gerbv_screen.h"
#include "log.h"
#include "setup.h"
#include "project.h"

#include "callbacks.h"
#include "interface.h"

#include "render.h"

#ifdef EXPORT_PNG
#include "exportimage.h"
#endif /* EXPORT_PNG */

#define dprintf if(DEBUG) printf

/**Global variable to keep track of what's happening on the screen.
   Declared extern in gerbv_screen.h
 */
extern gerbv_screen_t screen;

gerbv_render_info_t screenRenderInfo;

/*
static void
render_layer_to_cairo_target_without_transforming(cairo_t *cr, gerbv_fileinfo_t *fileInfo, gerbv_render_info_t *renderInfo );
*/

void
render_zoom_display (gint zoomType, gdouble scaleFactor, gdouble mouseX, gdouble mouseY) {
	double us_midx, us_midy;	/* unscaled translation for screen center */
	int half_w, half_h;		/* cache for half window dimensions */
	gdouble mouseCoordinateX = 0.0;
	gdouble mouseCoordinateY = 0.0;
	double oldWidth, oldHeight;

	half_w = screenRenderInfo.displayWidth / 2;
	half_h = screenRenderInfo.displayHeight / 2;

	oldWidth = screenRenderInfo.displayWidth / screenRenderInfo.scaleFactorX;
	oldHeight = screenRenderInfo.displayHeight / screenRenderInfo.scaleFactorY;
	if (zoomType == ZOOM_IN_CMOUSE || zoomType == ZOOM_OUT_CMOUSE) {
		/* calculate what user coordinate the mouse is pointing at */
		mouseCoordinateX = mouseX / screenRenderInfo.scaleFactorX + screenRenderInfo.lowerLeftX;
		mouseCoordinateY = (screenRenderInfo.displayHeight - mouseY) /
			screenRenderInfo.scaleFactorY + screenRenderInfo.lowerLeftY;
	}

	us_midx = screenRenderInfo.lowerLeftX + (screenRenderInfo.displayWidth / 2.0 )/
			screenRenderInfo.scaleFactorX;
	us_midy = screenRenderInfo.lowerLeftY + (screenRenderInfo.displayHeight / 2.0 )/
			screenRenderInfo.scaleFactorY;		

	switch(zoomType) {
		case ZOOM_IN : /* Zoom In */
		case ZOOM_IN_CMOUSE : /* Zoom In Around Mouse Pointer */
			screenRenderInfo.scaleFactorX += screenRenderInfo.scaleFactorX/3;
			screenRenderInfo.scaleFactorY += screenRenderInfo.scaleFactorY/3;
			screenRenderInfo.lowerLeftX += (oldWidth - (screenRenderInfo.displayWidth /
				screenRenderInfo.scaleFactorX)) / 2.0;
			screenRenderInfo.lowerLeftY += (oldHeight - (screenRenderInfo.displayHeight /
				screenRenderInfo.scaleFactorY)) / 2.0;	
			break;
		case ZOOM_OUT :  /* Zoom Out */
		case ZOOM_OUT_CMOUSE : /* Zoom Out Around Mouse Pointer */
			if ((screenRenderInfo.scaleFactorX > 10)&&(screenRenderInfo.scaleFactorY > 10)) {
				screenRenderInfo.scaleFactorX -= screenRenderInfo.scaleFactorX/3;
				screenRenderInfo.scaleFactorY -= screenRenderInfo.scaleFactorY/3;
				screenRenderInfo.lowerLeftX += (oldWidth - (screenRenderInfo.displayWidth /
					screenRenderInfo.scaleFactorX)) / 2.0;
				screenRenderInfo.lowerLeftY += (oldHeight - (screenRenderInfo.displayHeight /
					screenRenderInfo.scaleFactorY)) / 2.0;
			}
			break;
		case ZOOM_FIT : /* Zoom Fit */
			render_zoom_to_fit_display (&screenRenderInfo);
			break;
		case ZOOM_SET : /*explicit scale set by user */
			screenRenderInfo.scaleFactorX = scaleFactor;
			screenRenderInfo.scaleFactorY = scaleFactor;
			screenRenderInfo.lowerLeftX += (oldWidth - (screenRenderInfo.displayWidth /
				screenRenderInfo.scaleFactorX)) / 2.0;
			screenRenderInfo.lowerLeftY += (oldHeight - (screenRenderInfo.displayHeight /
				screenRenderInfo.scaleFactorY)) / 2.0;
		      break;
		default :
			GERB_MESSAGE("Illegal zoom direction %d\n", zoomType);
	}

	if (zoomType == ZOOM_IN_CMOUSE || zoomType == ZOOM_OUT_CMOUSE) {
		/* make sure the mouse is still pointing at the point calculated earlier */
		screenRenderInfo.lowerLeftX = mouseCoordinateX - mouseX / screenRenderInfo.scaleFactorX;
		screenRenderInfo.lowerLeftY = mouseCoordinateY - (screenRenderInfo.displayHeight - mouseY) /
			screenRenderInfo.scaleFactorY;
	}

	render_refresh_rendered_image_on_screen();
	return;
}


/* --------------------------------------------------------- */
/** Will determine the outline of the zoomed regions.
 * In case region to be zoomed is too small (which correspondes
 * e.g. to a double click) it is interpreted as a right-click 
 * and will be used to identify a part from the CURRENT selection, 
 * which is drawn on screen*/
void
render_calculate_zoom_from_outline(GtkWidget *widget, GdkEventButton *event)
{
	int x1, y1, x2, y2, dx, dy;	/* Zoom outline (UR and LL corners) */
	double centerPointX, centerPointY;
	int half_x, half_y;		/* cache for half window dimensions */

	x1 = MIN(screen.start_x, event->x);
	y1 = MIN(screen.start_y, event->y);
	x2 = MAX(screen.start_x, event->x);
	y2 = MAX(screen.start_y, event->y);
	dx = x2-x1;
	dy = y2-y1;

	if ((dx >= 4) && (dy >= 4)) {
		if (screen.centered_outline_zoom) {
			/* Centered outline mode */
			x1 = screen.start_x - dx;
			y1 = screen.start_y - dy;
			dx *= 2;
			dy *= 2;
		}
		half_x = (x1+x2)/2;
		half_y = (y1+y2)/2;
		centerPointX = half_x/screenRenderInfo.scaleFactorX + screenRenderInfo.lowerLeftX;
		centerPointY = (screenRenderInfo.displayHeight - half_y)/screenRenderInfo.scaleFactorY +
				screenRenderInfo.lowerLeftY;
		
		screenRenderInfo.scaleFactorX *= MIN(((double)screenRenderInfo.displayWidth / dx),
							((double)screenRenderInfo.displayHeight / dy));
		screenRenderInfo.scaleFactorY = screenRenderInfo.scaleFactorX;
		screenRenderInfo.lowerLeftX = centerPointX - (screenRenderInfo.displayWidth /
					2.0 / screenRenderInfo.scaleFactorX);
		screenRenderInfo.lowerLeftY = centerPointY - (screenRenderInfo.displayHeight /
					2.0 / screenRenderInfo.scaleFactorY);				
	}
	render_refresh_rendered_image_on_screen();
}

void
render_draw_selection_box_outline(void) {
	GdkGC *gc;
	GdkGCValues values;
	GdkGCValuesMask values_mask;
	gint x1, y1, x2, y2, dx, dy;

	memset(&values, 0, sizeof(values));
	values.function = GDK_XOR;
	if (!screen.zoom_outline_color.pixel)
	 	gdk_colormap_alloc_color(gdk_colormap_get_system(), &screen.zoom_outline_color, FALSE, TRUE);
	values.foreground = screen.zoom_outline_color;
	values_mask = GDK_GC_FUNCTION | GDK_GC_FOREGROUND;
	gc = gdk_gc_new_with_values(screen.drawing_area->window, &values, values_mask);
	
	x1 = MIN(screen.start_x, screen.last_x);
	y1 = MIN(screen.start_y, screen.last_y);
	x2 = MAX(screen.start_x, screen.last_x);
	y2 = MAX(screen.start_y, screen.last_y);
	dx = x2-x1;
	dy = y2-y1;

	gdk_draw_rectangle(screen.drawing_area->window, gc, FALSE, x1, y1, dx, dy);
	gdk_gc_unref(gc);
}

/* --------------------------------------------------------- */
void
render_draw_zoom_outline(gboolean centered)
{
	GdkGC *gc;
	GdkGCValues values;
	GdkGCValuesMask values_mask;
	gint x1, y1, x2, y2, dx, dy;

	memset(&values, 0, sizeof(values));
	values.function = GDK_XOR;
	if (!screen.zoom_outline_color.pixel)
	 	gdk_colormap_alloc_color(gdk_colormap_get_system(), &screen.zoom_outline_color, FALSE, TRUE);
	values.foreground = screen.zoom_outline_color;
	values_mask = GDK_GC_FUNCTION | GDK_GC_FOREGROUND;
	gc = gdk_gc_new_with_values(screen.drawing_area->window, &values, values_mask);
	
	x1 = MIN(screen.start_x, screen.last_x);
	y1 = MIN(screen.start_y, screen.last_y);
	x2 = MAX(screen.start_x, screen.last_x);
	y2 = MAX(screen.start_y, screen.last_y);
	dx = x2-x1;
	dy = y2-y1;

	if (centered) {
		/* Centered outline mode */
		x1 = screen.start_x - dx;
		y1 = screen.start_y - dy;
		dx *= 2;
		dy *= 2;
		x2 = x1+dx;
		y2 = y1+dy;
	}

	gdk_draw_rectangle(screen.drawing_area->window, gc, FALSE, x1, y1, dx, dy);
	gdk_gc_unref(gc);

	/* Draw actual zoom area in dashed lines */
	memset(&values, 0, sizeof(values));
	values.function = GDK_XOR;
	values.foreground = screen.zoom_outline_color;
	values.line_style = GDK_LINE_ON_OFF_DASH;
	values_mask = GDK_GC_FUNCTION | GDK_GC_FOREGROUND | GDK_GC_LINE_STYLE;
	gc = gdk_gc_new_with_values(screen.drawing_area->window, &values,
				values_mask);
	
	if ((dy == 0) || ((double)dx/dy > (double)screen.drawing_area->allocation.width/
				screen.drawing_area->allocation.height)) {
		dy = dx * (double)screen.drawing_area->allocation.height/
			screen.drawing_area->allocation.width;
	} 
	else {
		dx = dy * (double)screen.drawing_area->allocation.width/
			screen.drawing_area->allocation.height;
	}

	gdk_draw_rectangle(screen.drawing_area->window, gc, FALSE, (x1+x2-dx)/2,
		(y1+y2-dy)/2, dx, dy);

	gdk_gc_unref(gc);
}


/** Displays a measured distance graphically on screen and in statusbar.
    activated when using SHIFT and mouse dragged to measure distances\n
    under win32 graphical annotations are currently disabled (GTK 2.47)*/
void
render_draw_measure_distance(void)
{
#if !defined (__MINGW32__) 

	GdkGC *gc;
	GdkGCValues values;
	GdkGCValuesMask values_mask;
	GdkFont *font;
#endif   
	gint x1, y1, x2, y2;
	double dx, dy;

#if !defined (__MINGW32__) /*taken out because of different drawing behaviour under win32 resulting in a smear */
	memset(&values, 0, sizeof(values));
	values.function = GDK_XOR;
	if (!screen.dist_measure_color.pixel)
	 	gdk_colormap_alloc_color(gdk_colormap_get_system(), &screen.dist_measure_color, FALSE, TRUE);
	values.foreground = screen.dist_measure_color;
	values_mask = GDK_GC_FUNCTION | GDK_GC_FOREGROUND;
	gc = gdk_gc_new_with_values(screen.drawing_area->window, &values,
				values_mask);
	font = gdk_font_load(setup.dist_fontname);
#endif
	x1 = MIN(screen.start_x, screen.last_x);
	y1 = MIN(screen.start_y, screen.last_y);
	x2 = MAX(screen.start_x, screen.last_x);
	y2 = MAX(screen.start_y, screen.last_y);
	dx = (x2 - x1)/ screenRenderInfo.scaleFactorX;
	dy = (y2 - y1)/ screenRenderInfo.scaleFactorY;
    
#if !defined (__MINGW32__)
	gdk_draw_line(screen.drawing_area->window, gc, screen.start_x,
		  screen.start_y, screen.last_x, screen.last_y);
	if (font == NULL) {
		GERB_MESSAGE("Failed to load font '%s'\n", setup.dist_fontname);
	} 
	else {
#endif
		screen.win.lastMeasuredX = dx;
		screen.win.lastMeasuredY = dy;
		callbacks_update_statusbar_measured_distance (dx, dy);
#if !defined (__MINGW32__)
	}
	gdk_gc_unref(gc);
#endif     
} /* draw_measure_distance */

void
render_translate_to_fit_display (gerbv_render_info_t *renderInfo) {
	double x1=HUGE_VAL,y1=HUGE_VAL;
	int i;
	gerb_image_info_t *info;
	
	for(i = 0; i < screen.max_files; i++) {
		if ((screen.file[i]) && (screen.file[i]->isVisible)){
			info = screen.file[i]->image->info;

			/* cairo info already has offset calculated into min/max */
			x1 = MIN(x1, info->min_x);
			y1 = MIN(y1, info->min_y);
		}
	}
	renderInfo->lowerLeftX = x1;
	renderInfo->lowerLeftY = y1;
}

/* ------------------------------------------------------------------ */
void
render_get_boundingbox(gerbv_render_size_t *boundingbox)
{
	double x1=HUGE_VAL,y1=HUGE_VAL, x2=-HUGE_VAL,y2=-HUGE_VAL;
	int i;
	gerb_image_info_t *info;

	for(i = 0; i < screen.max_files; i++) {
		if ((screen.file[i]) && (screen.file[i]->isVisible)){
			info = screen.file[i]->image->info;
			/* 
			* Find the biggest image and use as a size reference
			*/
#ifdef RENDER_USING_GDK
			x1 = MIN(x1, info->min_x + info->offsetA);
			y1 = MIN(y1, info->min_y + info->offsetB);
			x2 = MAX(x2, info->max_x + info->offsetA);
			y2 = MAX(y2, info->max_y + info->offsetB);
#else
			/* cairo info already has offset calculated into min/max */
			x1 = MIN(x1, info->min_x);
			y1 = MIN(y1, info->min_y);
			x2 = MAX(x2, info->max_x);
			y2 = MAX(y2, info->max_y);
#endif
		}
	}
	boundingbox->left    = x1;
	boundingbox->right   = x2;
	boundingbox->top    = y1;
	boundingbox->bottom = y2;
}

/* ------------------------------------------------------------------ */
void
render_zoom_to_fit_display (gerbv_render_info_t *renderInfo) {
	gerbv_render_size_t bb;
	double width, height;
	double x_scale, y_scale;

	/* Grab maximal width and height of all layers */
        render_get_boundingbox(&bb);
	width = bb.right - bb.left;
	height = bb.bottom - bb.top;
	/* add in a 5% buffer around the drawing */
	width *= 1.05;
	height *=1.05;

	/* if the values aren't sane (probably we have no models loaded), then
	   put in some defaults */
	if ((width < 0.01) && (height < 0.01)) {
		renderInfo->lowerLeftX = 0.0;
		renderInfo->lowerLeftY = 0.0;
		renderInfo->scaleFactorX = 200;
		renderInfo->scaleFactorY = 200;
		return;
	}
	/*
	* Calculate scale for both x axis and y axis
	*/
	x_scale = renderInfo->displayWidth / width;
	y_scale = renderInfo->displayHeight / height;
	/*
	* Take the scale that fits both directions with some extra checks
	*/
	renderInfo->scaleFactorX = MIN(x_scale, y_scale);
	renderInfo->scaleFactorY = renderInfo->scaleFactorX;
	if (renderInfo->scaleFactorX < 1){
	    renderInfo->scaleFactorX = 1;
	    renderInfo->scaleFactorY = 1;
	}
	renderInfo->lowerLeftX = ((bb.left + bb.right) / 2.0) -
		((double) renderInfo->displayWidth / 2.0 / renderInfo->scaleFactorX);
	renderInfo->lowerLeftY = ((bb.top + bb.bottom) / 2.0) -
		((double) renderInfo->displayHeight / 2.0 / renderInfo->scaleFactorY);
	return;
}

void render_selection_layer (void){
#ifndef RENDER_USING_GDK
	cairo_t *cr;
	
	if (screen.selectionRenderData) 
		cairo_surface_destroy ((cairo_surface_t *) screen.selectionRenderData);
	screen.selectionRenderData = 
		(gpointer) cairo_surface_create_similar ((cairo_surface_t *)screen.windowSurface,
		CAIRO_CONTENT_COLOR_ALPHA, screenRenderInfo.displayWidth,
		screenRenderInfo.displayHeight);
	if (screen.selectionInfo.type != EMPTY) {
		cr= cairo_create(screen.selectionRenderData);
		render_cairo_set_scale_translation(cr, &screenRenderInfo);
		cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1);
		/* for now, assume everything in the selection buffer is from one image */
		gerb_image_t *matchImage;
		int j;
		if (screen.selectionInfo.selectedNodeArray->len > 0) {
			gerb_selection_item_t sItem = g_array_index (screen.selectionInfo.selectedNodeArray,
					gerb_selection_item_t, 0);
			matchImage = (gerb_image_t *) sItem.image;	
			dprintf("    .... calling render_image_to_cairo_target on selection layer...\n");
			for(j = screen.max_files-1; j >= 0; j--) {
				if ((screen.file[j]) && (screen.file[j]->image == matchImage)) {
					draw_image_to_cairo_target (cr, screen.file[j]->image,
						screen.file[j]->transform.inverted,
						1.0/MAX(screenRenderInfo.scaleFactorX,
						screenRenderInfo.scaleFactorY),
						DRAW_SELECTIONS, &screen.selectionInfo);
				}
			}
		}
		cairo_destroy (cr);
	}
#endif
}

void render_refresh_rendered_image_on_screen (void) {
	GdkCursor *cursor;
	
	dprintf("----> Entering redraw_pixmap...\n");
	cursor = gdk_cursor_new(GDK_WATCH);
	gdk_window_set_cursor(GDK_WINDOW(screen.drawing_area->window), cursor);
	gdk_cursor_destroy(cursor);

	if (screenRenderInfo.renderType < 2){
	    if (screen.pixmap) 
		gdk_pixmap_unref(screen.pixmap);
	    screen.pixmap = gdk_pixmap_new(screen.drawing_area->window, screenRenderInfo.displayWidth,
	    screenRenderInfo.displayHeight, -1);
	    render_to_pixmap_using_gdk (screen.pixmap, &screenRenderInfo);	
	    dprintf("<---- leaving redraw_pixmap.\n");
	}
#ifndef RENDER_USING_GDK
	else{
	    int i;
	    dprintf("    .... Now try rendering the drawing using cairo .... \n");
	    /* 
	     * This now allows drawing several layers on top of each other.
	     * Higher layer numbers have higher priority in the Z-order.
	     */
	    for(i = screen.max_files-1; i >= 0; i--) {
		if (screen.file[i]) {
		    cairo_t *cr;
		    if (screen.file[i]->privateRenderData) 
			cairo_surface_destroy ((cairo_surface_t *) screen.file[i]->privateRenderData);
		    screen.file[i]->privateRenderData = 
			(gpointer) cairo_surface_create_similar ((cairo_surface_t *)screen.windowSurface,
			CAIRO_CONTENT_COLOR_ALPHA, screenRenderInfo.displayWidth,
			screenRenderInfo.displayHeight);
		    cr= cairo_create(screen.file[i]->privateRenderData );
		    render_layer_to_cairo_target (cr, screen.file[i], &screenRenderInfo);
		    dprintf("    .... calling render_image_to_cairo_target on layer %d...\n", i);			
		    cairo_destroy (cr);
		}
	    }
	    /* render the selection layer */
	    render_selection_layer();
	    
	    render_recreate_composite_surface ();
	}
#endif
	/* remove watch cursor and switch back to normal cursor */
	callbacks_switch_to_correct_cursor ();
	callbacks_force_expose_event_for_screen();
}

#ifndef RENDER_USING_GDK
gint
render_create_cairo_buffer_surface () {
	if (screen.bufferSurface) {
		cairo_surface_destroy (screen.bufferSurface);
		screen.bufferSurface = NULL;
	}
	if (!screen.windowSurface)
		return 0;

	screen.bufferSurface= cairo_surface_create_similar ((cairo_surface_t *)screen.windowSurface,
	                                    CAIRO_CONTENT_COLOR, screenRenderInfo.displayWidth,
	                                    screenRenderInfo.displayHeight);
	return 1;
}

void
render_clear_selection_buffer (void){
	g_array_remove_range (screen.selectionInfo.selectedNodeArray, 0,
			screen.selectionInfo.selectedNodeArray->len);
	screen.selectionInfo.type = EMPTY;
}

void
render_find_selected_objects_and_refresh_display (gint activeFileIndex, gboolean eraseOldSelection){
	/* clear the old selection array if desired */
	if ((eraseOldSelection)&&(screen.selectionInfo.selectedNodeArray->len))
		g_array_remove_range (screen.selectionInfo.selectedNodeArray, 0,
			screen.selectionInfo.selectedNodeArray->len);

	/* make sure we have a bufferSurface...if we start up in FAST mode, we may not
	   have one yet, but we need it for selections */
	if (!render_create_cairo_buffer_surface())
		return;

	/* call draw_image... passing the FILL_SELECTION mode to just search for
	   nets which match the selection, and fill the selection buffer with them */
	cairo_t *cr= cairo_create(screen.bufferSurface);	
	render_cairo_set_scale_translation(cr,&screenRenderInfo);
	draw_image_to_cairo_target (cr, screen.file[activeFileIndex]->image, screen.file[activeFileIndex]->transform.inverted,
		1.0/MAX(screenRenderInfo.scaleFactorX, screenRenderInfo.scaleFactorY),
		FIND_SELECTIONS, &screen.selectionInfo);
	cairo_destroy (cr);
	/* if the selection array is empty, switch the "mode" to empty to make it
	   easier to check if it is holding anything */
	if (!screen.selectionInfo.selectedNodeArray->len)
		screen.selectionInfo.type = EMPTY;
	/* re-render the selection buffer layer */
	if (screenRenderInfo.renderType < 2){
		render_refresh_rendered_image_on_screen ();
	}
	else {
		render_selection_layer();
		render_recreate_composite_surface ();
		callbacks_force_expose_event_for_screen();
	}
}

void
render_fill_selection_buffer_from_mouse_click (gint mouseX, gint mouseY, gint activeFileIndex,
		gboolean eraseOldSelection) {
	screen.selectionInfo.lowerLeftX = mouseX;
	screen.selectionInfo.lowerLeftY = mouseY;
	/* no need to populate the upperright coordinates for a point_click */
	screen.selectionInfo.type = POINT_CLICK;
	render_find_selected_objects_and_refresh_display (activeFileIndex, eraseOldSelection);
}

void
render_fill_selection_buffer_from_mouse_drag (gint corner1X, gint corner1Y,
	gint corner2X, gint corner2Y, gint activeFileIndex, gboolean eraseOldSelection) {
	/* figure out the lower left corner of the box */
	screen.selectionInfo.lowerLeftX = MIN(corner1X, corner2X);
	screen.selectionInfo.lowerLeftY = MIN(corner1Y, corner2Y);
	/* figure out the upper right corner of the box */
	screen.selectionInfo.upperRightX = MAX(corner1X, corner2X);
	screen.selectionInfo.upperRightY = MAX(corner1Y, corner2Y);
	
	screen.selectionInfo.type = DRAG_BOX;
	render_find_selected_objects_and_refresh_display (activeFileIndex, eraseOldSelection);
}

void render_all_layers_to_cairo_target_for_vector_output (cairo_t *cr, gerbv_render_info_t *renderInfo) {
	int i;
	render_cairo_set_scale_translation(cr, renderInfo);
	/* don't paint background for vector output, since it isn't needed */
	for(i = screen.max_files-1; i >= 0; i--) {
		if (screen.file[i] && screen.file[i]->isVisible) {

		    render_layer_to_cairo_target_without_transforming(cr, screen.file[i], renderInfo);
		}
	}
}

void render_all_layers_to_cairo_target (cairo_t *cr, gerbv_render_info_t *renderInfo) {
	int i;
	/* fill the background with the appropriate color */
	cairo_set_source_rgba (cr, (double) screen.background.red/G_MAXUINT16,
		(double) screen.background.green/G_MAXUINT16,
		(double) screen.background.blue/G_MAXUINT16, 1);
	cairo_paint (cr);
	for(i = screen.max_files-1; i >= 0; i--) {
		if (screen.file[i] && screen.file[i]->isVisible) {
			cairo_push_group (cr);
			render_layer_to_cairo_target (cr, screen.file[i], renderInfo);
			cairo_pop_group_to_source (cr);
			cairo_paint_with_alpha (cr, (double) screen.file[i]->alpha/G_MAXUINT16);
		}
	}
}

void render_layer_to_cairo_target (cairo_t *cr, gerbv_fileinfo_t *fileInfo,
						gerbv_render_info_t *renderInfo) {
	render_cairo_set_scale_translation(cr, renderInfo);
	render_layer_to_cairo_target_without_transforming(cr, fileInfo, renderInfo);
}

void render_cairo_set_scale_translation(cairo_t *cr, gerbv_render_info_t *renderInfo){
	gdouble translateX, translateY;
	
	translateX = (renderInfo->lowerLeftX * renderInfo->scaleFactorX);
	translateY = (renderInfo->lowerLeftY * renderInfo->scaleFactorY);
	
	/* renderTypes 0 and 1 use GDK rendering, so we shouldn't have made it
	   this far */
	if (renderInfo->renderType == 2) {
		cairo_set_tolerance (cr, 1.5);
		cairo_set_antialias (cr, CAIRO_ANTIALIAS_NONE);
	}
	else if (renderInfo->renderType == 3) {
		cairo_set_tolerance (cr, 1);
		/* disable ALL anti-aliasing for now due to the way cairo is rendering
		   ground planes from PCB output */
		cairo_set_antialias (cr, CAIRO_ANTIALIAS_DEFAULT);
	}

	/* translate the draw area before drawing.  We must translate the whole
	   drawing down an additional displayHeight to account for the negative
	   y flip done later */
	cairo_translate (cr, -translateX, translateY + renderInfo->displayHeight);
	/* scale the drawing by the specified scale factor (inverting y since
		cairo y axis points down) */
	cairo_scale (cr, renderInfo->scaleFactorX, -renderInfo->scaleFactorY);
}

void
render_layer_to_cairo_target_without_transforming(cairo_t *cr, gerbv_fileinfo_t *fileInfo, gerbv_render_info_t *renderInfo ) {
	cairo_set_source_rgba (cr, (double) fileInfo->color.red/G_MAXUINT16,
		(double) fileInfo->color.green/G_MAXUINT16,
		(double) fileInfo->color.blue/G_MAXUINT16, 1);
	
	draw_image_to_cairo_target (cr, fileInfo->image, fileInfo->transform.inverted,
		1.0/MAX(renderInfo->scaleFactorX, renderInfo->scaleFactorY), DRAW_IMAGE, NULL);
}

void render_recreate_composite_surface () {
	gint i;
	
	if (!render_create_cairo_buffer_surface())
		return;

	cairo_t *cr= cairo_create(screen.bufferSurface);
	/* fill the background with the appropriate color */
	cairo_set_source_rgba (cr, (double) screen.background.red/G_MAXUINT16,
		(double) screen.background.green/G_MAXUINT16,
		(double) screen.background.blue/G_MAXUINT16, 1);
	cairo_paint (cr);
	
	for(i = screen.max_files-1; i >= 0; i--) {
		if (screen.file[i] && screen.file[i]->isVisible) {
			cairo_set_source_surface (cr, (cairo_surface_t *) screen.file[i]->privateRenderData,
			                              0, 0);
			/* ignore alpha if we are in high-speed render mode */
			if (((double) screen.file[i]->alpha < 65535)&&(screenRenderInfo.renderType != 1)) {
				cairo_paint_with_alpha(cr,(double) screen.file[i]->alpha/G_MAXUINT16);
			}
			else {
				cairo_paint (cr);
			}
		}
	}
	/* render the selection layer at the end */
	if (screen.selectionInfo.type != EMPTY) {
		cairo_set_source_surface (cr, (cairo_surface_t *) screen.selectionRenderData,
			                              0, 0);
		cairo_paint_with_alpha (cr,1.0);
	}
	cairo_destroy (cr);
}

void render_project_to_cairo_target (cairo_t *cr) {
	/* fill the background with the appropriate color */
	cairo_set_source_rgba (cr, (double) screen.background.red/G_MAXUINT16,
		(double) screen.background.green/G_MAXUINT16,
		(double) screen.background.blue/G_MAXUINT16, 1);
	cairo_paint (cr);

	cairo_set_source_surface (cr, (cairo_surface_t *) screen.bufferSurface, 0 , 0);
                                                   
	cairo_paint (cr);
}
#endif  /* RENDER_USING_GDK */


void
render_to_pixmap_using_gdk (GdkPixmap *pixmap, gerbv_render_info_t *renderInfo){
	GdkGC *gc = gdk_gc_new(pixmap);
	GdkPixmap *colorStamp, *clipmask;
	int i;
	
	/* 
	 * Remove old pixmap, allocate a new one, draw the background.
	 */
	if (!screen.background.pixel)
	 	gdk_colormap_alloc_color(gdk_colormap_get_system(), &screen.background, FALSE, TRUE);
	gdk_gc_set_foreground(gc, &screen.background);
	gdk_draw_rectangle(pixmap, gc, TRUE, 0, 0, -1, -1);

	/*
	 * Allocate the pixmap and the clipmask (a one pixel pixmap)
	 */
	colorStamp = gdk_pixmap_new(pixmap, renderInfo->displayWidth,
						renderInfo->displayHeight, -1);
	clipmask = gdk_pixmap_new(NULL, renderInfo->displayWidth,
						renderInfo->displayHeight, 1);
							
	/* 
	* This now allows drawing several layers on top of each other.
	* Higher layer numbers have higher priority in the Z-order. 
	*/
	for(i = screen.max_files-1; i >= 0; i--) {
		if (screen.file[i] && screen.file[i]->isVisible) {
			enum polarity_t polarity;

			if (screen.file[i]->transform.inverted) {
				if (screen.file[i]->image->info->polarity == POSITIVE)
					polarity = NEGATIVE;
				else
					polarity = POSITIVE;
			} else {
				polarity = screen.file[i]->image->info->polarity;
			}

			/*
			* Fill up image with all the foreground color. Excess pixels
			* will be removed by clipmask.
			*/
			if (!screen.file[i]->color.pixel)
	 			gdk_colormap_alloc_color(gdk_colormap_get_system(), &screen.file[i]->color, FALSE, TRUE);
			gdk_gc_set_foreground(gc, &screen.file[i]->color);
			
			/* switch back to regular draw function for the initial
			   bitmap clear */
			gdk_gc_set_function(gc, GDK_COPY);
			gdk_draw_rectangle(colorStamp, gc, TRUE, 0, 0, -1, -1);
			
			if (renderInfo->renderType == 0) {
				gdk_gc_set_function(gc, GDK_COPY);
			}
			else if (renderInfo->renderType == 1) {
				gdk_gc_set_function(gc, GDK_XOR);
			}
			/*
			* Translation is to get it inside the allocated pixmap,
			* which is not always centered perfectly for GTK/X.
			*/
			dprintf("  .... calling image2pixmap on image %d...\n", i);
			// Dirty scaling solution when using GDK; simply use scaling factor for x-axis, ignore y-axis
			draw_gdk_image_to_pixmap(&clipmask, screen.file[i]->image,
				renderInfo->scaleFactorX, -(renderInfo->lowerLeftX * renderInfo->scaleFactorX),
				(renderInfo->lowerLeftY * renderInfo->scaleFactorY) + renderInfo->displayHeight,
				polarity, DRAW_IMAGE, NULL);

			/* 
			* Set clipmask and draw the clipped out image onto the
			* screen pixmap. Afterwards we remove the clipmask, else
			* it will screw things up when run this loop again.
			*/
			gdk_gc_set_clip_mask(gc, clipmask);
			gdk_gc_set_clip_origin(gc, 0, 0);
			gdk_draw_drawable(pixmap, gc, colorStamp, 0, 0, 0, 0, -1, -1);
			gdk_gc_set_clip_mask(gc, NULL);
		}
	}
	/* render the selection group to the top of the output */
	if (screen.selectionInfo.type != EMPTY) {
		if (!screen.selection_color.pixel)
	 		gdk_colormap_alloc_color(gdk_colormap_get_system(), &screen.selection_color, FALSE, TRUE);
	 		
		gdk_gc_set_foreground(gc, &screen.selection_color);
		gdk_gc_set_function(gc, GDK_COPY);
		gdk_draw_rectangle(colorStamp, gc, TRUE, 0, 0, -1, -1);
		
		/* for now, assume everything in the selection buffer is from one image */
		gerb_image_t *matchImage;
		int j;
		if (screen.selectionInfo.selectedNodeArray->len > 0) {
			gerb_selection_item_t sItem = g_array_index (screen.selectionInfo.selectedNodeArray,
					gerb_selection_item_t, 0);
			matchImage = (gerb_image_t *) sItem.image;	

			for(j = screen.max_files-1; j >= 0; j--) {
				if ((screen.file[j]) && (screen.file[j]->image == matchImage)) {
					draw_gdk_image_to_pixmap(&clipmask, screen.file[j]->image,
						renderInfo->scaleFactorX, -(renderInfo->lowerLeftX * renderInfo->scaleFactorX),
						(renderInfo->lowerLeftY * renderInfo->scaleFactorY) + renderInfo->displayHeight,
						POSITIVE, DRAW_SELECTIONS, &screen.selectionInfo);
				}
			}
			gdk_gc_set_clip_mask(gc, clipmask);
			gdk_gc_set_clip_origin(gc, 0, 0);
			gdk_draw_drawable(pixmap, gc, colorStamp, 0, 0, 0, 0, -1, -1);
			gdk_gc_set_clip_mask(gc, NULL);
		}
	}

	gdk_pixmap_unref(colorStamp);
	gdk_pixmap_unref(clipmask);
	gdk_gc_unref(gc);
}


/* ------------------------------------------------------------------ */
/* Fill out the gerber statistics table */
gerb_stats_t *
generate_gerber_analysis(void)
{
    int i;
    gerb_stats_t *stats;
    gerb_stats_t *instats;

    stats = gerb_stats_new();

    /* Loop through open layers and compile statistics */
    for(i = screen.max_files-1; i >= 0; i--) {
	if (screen.file[i] && 
	    screen.file[i]->isVisible &&
	    (screen.file[i]->image->layertype == GERBER) ) {
	    instats = screen.file[i]->image->gerb_stats;
	    gerb_stats_add_layer(stats, instats, i+1);
	}
    }
    
    return stats;
}


/* ------------------------------------------------------------------ */
/* Fill out the drill file statistics table */
drill_stats_t *
generate_drill_analysis(void)
{
    int i;
    drill_stats_t *stats;
    drill_stats_t *instats;

    stats = drill_stats_new();

    /* Loop through open layers and compile statistics */
    for(i = screen.max_files-1; i >= 0; i--) {
	if (screen.file[i] && 
	    screen.file[i]->isVisible &&
	    (screen.file[i]->image->layertype == DRILL) ) {
	    instats = screen.file[i]->image->drill_stats;
	    /* add this batch of stats.  Send the layer 
	     * index for error reporting */
	    drill_stats_add_layer(stats, instats, i+1);
	}
    }
    
    return stats;
}


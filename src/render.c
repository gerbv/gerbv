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
  #include "draw.h"
  #include <cairo-xlib.h>
  #include <cairo.h>
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

void
render_zoom_display (gint zoomType, gdouble scaleFactor, gdouble mouseX, gdouble mouseY) {
	double us_midx, us_midy;	/* unscaled translation for screen center */
	int half_w, half_h;		/* cache for half window dimensions */
	gdouble mouseCoordinateX = 0.0;
	gdouble mouseCoordinateY = 0.0;
	double oldWidth, oldHeight;

	half_w = screenRenderInfo.displayWidth / 2;
	half_h = screenRenderInfo.displayHeight / 2;

	oldWidth = screenRenderInfo.displayWidth / screenRenderInfo.scaleFactor;
	oldHeight = screenRenderInfo.displayHeight / screenRenderInfo.scaleFactor;
	
	if (zoomType == ZOOM_IN_CMOUSE || zoomType == ZOOM_OUT_CMOUSE) {
		/* calculate what user coordinate the mouse is pointing at */
		mouseCoordinateX = mouseX / screenRenderInfo.scaleFactor + screenRenderInfo.lowerLeftX;
		mouseCoordinateY = (screenRenderInfo.displayHeight - mouseY) /
			screenRenderInfo.scaleFactor + screenRenderInfo.lowerLeftY;
	}

	us_midx = screenRenderInfo.lowerLeftX + (screenRenderInfo.displayWidth / 2.0 )/
			screenRenderInfo.scaleFactor;
	us_midy = screenRenderInfo.lowerLeftY + (screenRenderInfo.displayHeight / 2.0 )/
			screenRenderInfo.scaleFactor;		

	switch(zoomType) {
		case ZOOM_IN : /* Zoom In */
		case ZOOM_IN_CMOUSE : /* Zoom In Around Mouse Pointer */
			screenRenderInfo.scaleFactor += screenRenderInfo.scaleFactor/3;
			screenRenderInfo.lowerLeftX += (oldWidth - (screenRenderInfo.displayWidth /
				screenRenderInfo.scaleFactor)) / 2.0;
			screenRenderInfo.lowerLeftY += (oldHeight - (screenRenderInfo.displayHeight /
				screenRenderInfo.scaleFactor)) / 2.0;	
			break;
		case ZOOM_OUT :  /* Zoom Out */
		case ZOOM_OUT_CMOUSE : /* Zoom Out Around Mouse Pointer */
			if (screenRenderInfo.scaleFactor > 10) {
				screenRenderInfo.scaleFactor -= screenRenderInfo.scaleFactor/3;
				screenRenderInfo.lowerLeftX += (oldWidth - (screenRenderInfo.displayWidth /
					screenRenderInfo.scaleFactor)) / 2.0;
				screenRenderInfo.lowerLeftY += (oldHeight - (screenRenderInfo.displayHeight /
					screenRenderInfo.scaleFactor)) / 2.0;
			}
			break;
		case ZOOM_FIT : /* Zoom Fit */
			render_zoom_to_fit_display (&screenRenderInfo);
			break;
		case ZOOM_SET : /*explicit scale set by user */
			screenRenderInfo.scaleFactor = scaleFactor;
			screenRenderInfo.lowerLeftX += (oldWidth - (screenRenderInfo.displayWidth /
				screenRenderInfo.scaleFactor)) / 2.0;
			screenRenderInfo.lowerLeftY += (oldHeight - (screenRenderInfo.displayHeight /
				screenRenderInfo.scaleFactor)) / 2.0;
		      break;
		default :
			GERB_MESSAGE("Illegal zoom direction %d\n", zoomType);
	}

	if (zoomType == ZOOM_IN_CMOUSE || zoomType == ZOOM_OUT_CMOUSE) {
		/* make sure the mouse is still pointing at the point calculated earlier */
		screenRenderInfo.lowerLeftX = mouseCoordinateX - mouseX / screenRenderInfo.scaleFactor;
		screenRenderInfo.lowerLeftY = mouseCoordinateY - (screenRenderInfo.displayHeight - mouseY) /
			screenRenderInfo.scaleFactor;
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
		
		centerPointX = half_x/screenRenderInfo.scaleFactor + screenRenderInfo.lowerLeftX;
		centerPointY = (screenRenderInfo.displayHeight - half_y)/screenRenderInfo.scaleFactor +
				screenRenderInfo.lowerLeftY;
		
		screenRenderInfo.scaleFactor *= MIN(((double)screenRenderInfo.displayWidth / dx),
							((double)screenRenderInfo.displayHeight / dy));
		screenRenderInfo.lowerLeftX = centerPointX - (screenRenderInfo.displayWidth /
					2.0 / screenRenderInfo.scaleFactor);
		screenRenderInfo.lowerLeftY = centerPointY - (screenRenderInfo.displayHeight /
					2.0 / screenRenderInfo.scaleFactor);				
	}
	render_refresh_rendered_image_on_screen();
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
	values.foreground = screen.dist_measure_color;
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
	double delta, dx, dy;

	if (screen.state != MEASURE)
		return;
#if !defined (__MINGW32__) /*taken out because of different drawing behaviour under win32 resulting in a smear */
	memset(&values, 0, sizeof(values));
	values.function = GDK_XOR;
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

	dx = (x2 - x1)/ screenRenderInfo.scaleFactor;
	dy = (y2 - y1)/ screenRenderInfo.scaleFactor;
	delta = sqrt(dx*dx + dy*dy); /* Pythagoras */
    
#if !defined (__MINGW32__)
	gdk_draw_line(screen.drawing_area->window, gc, screen.start_x,
		  screen.start_y, screen.last_x, screen.last_y);
	if (font == NULL) {
		GERB_MESSAGE("Failed to load font '%s'\n", setup.dist_fontname);
	} 
	else {
#endif
		/*
		 * Update statusbar
		 */
		if (screen.unit == GERBV_MILS) {
		    snprintf(screen.statusbar.diststr, MAX_DISTLEN,
			     "Measured distance: %7.1f mils (%7.1f X, %7.1f Y)",
			     COORD2MILS(delta), COORD2MILS(dx), COORD2MILS(dy));
		} 
		else if (screen.unit == GERBV_MMS) {
		    snprintf(screen.statusbar.diststr, MAX_DISTLEN,
			     "Measured distance: %7.1f mms (%7.1f X, %7.1f Y)",
			     COORD2MMS(delta), COORD2MMS(dx), COORD2MMS(dy));
		}
		else {
		    snprintf(screen.statusbar.diststr, MAX_DISTLEN,
			     "Measured distance: %3.4f inches (%3.4f X, %3.4f Y)",
			     COORD2MILS(delta) / 1000.0, COORD2MILS(dx) / 1000.0,
			     COORD2MILS(dy) / 1000.0);
		}
		callbacks_update_statusbar();

#if !defined (__MINGW32__)
	}
	gdk_gc_unref(gc);
#endif     
} /* draw_measure_distance */


/* ------------------------------------------------------------------ */
void
render_zoom_to_fit_display (gerbv_render_info_t *renderInfo) {
	double max_width = LONG_MIN, max_height = LONG_MIN,x1=HUGE_VAL,y1=HUGE_VAL,
	x2=-HUGE_VAL,y2=-HUGE_VAL;
	double x_scale, y_scale;
	int i;
	gerb_image_info_t *info;

	for(i = 0; i < MAX_FILES; i++) {
		if ((screen.file[i]) && (screen.file[i]->isVisible)){
			info = screen.file[i]->image->info;
			/* 
			* Find the biggest image and use as a size reference
			*/
			x1 = MIN(x1, info->min_x + info->offsetA);
			y1 = MIN(y1, info->min_y + info->offsetB);
			x2 = MAX(x2, info->max_x + info->offsetA);
			y2 = MAX(y2, info->max_y + info->offsetB);
		}
	}

	/* add in a 5% buffer around the drawing */
	max_width = (x2 - x1)*1.05;
	max_height = (y2 - y1)*1.05;

	/* if the values aren't sane (probably we have no models loaded), then
	   put in some defaults */
	if ((max_width < 0.01) || (max_height < 0.01)) {
		renderInfo->lowerLeftX = 0.0;
		renderInfo->lowerLeftY = 0.0;
		renderInfo->scaleFactor = 50;
		return;
	}
	/*
	* Calculate scale for both x axis and y axis
	*/
	x_scale = renderInfo->displayWidth / max_width;
	y_scale = renderInfo->displayHeight / max_height;

	/*
	* Take the scale that fits both directions with some extra checks
	*/
	renderInfo->scaleFactor = MIN(x_scale, y_scale);
	if (renderInfo->scaleFactor < 1)
		renderInfo->scaleFactor = 1;

	renderInfo->lowerLeftX = ((x1 + x2) / 2.0) -
		((double) renderInfo->displayWidth / 2.0 / renderInfo->scaleFactor);
	renderInfo->lowerLeftY = ((y1 + y2) / 2.0) -
		((double) renderInfo->displayHeight / 2.0 / renderInfo->scaleFactor);
	return;
}

void render_refresh_rendered_image_on_screen (void) {
#ifdef RENDER_USING_GDK
	GdkWindow *window;

	dprintf("----> Entering redraw_pixmap...\n");

	window = gtk_widget_get_parent_window(screen.drawing_area);
	/* This might be lengthy, show that we're busy by changing the pointer */
	if (window) {
		GdkCursor *cursor;

		cursor = gdk_cursor_new(GDK_WATCH);
		gdk_window_set_cursor(window, cursor);
		gdk_cursor_destroy(cursor);
	}

	if (screen.pixmap) 
	    gdk_pixmap_unref(screen.pixmap);
	screen.pixmap = gdk_pixmap_new(screen.drawing_area->window, screenRenderInfo.displayWidth,
				screenRenderInfo.displayHeight, -1);
	render_to_pixmap_using_gdk (screen.pixmap, &screenRenderInfo);
	
	/* Return default pointer shape */
	if (window) {
		gdk_window_set_cursor(window, GERBV_DEF_CURSOR);
	}
	dprintf("<---- leaving redraw_pixmap.\n");
#else
	int i;
/*
	GTimer* profileTimer = g_timer_new ();
*/
	dprintf("    .... Now try rendering the drawing using cairo .... \n");

	/* 
	* This now allows drawing several layers on top of each other.
	* Higher layer numbers have higher priority in the Z-order. 
	*/
	for(i = 0; i < MAX_FILES; i++) {
		if (screen.file[i] && screen.file[i]->isVisible) {
			cairo_t *cr;

			if (screen.file[i]->privateRenderData) 
				cairo_surface_destroy ((cairo_surface_t *) screen.file[i]->privateRenderData);

			screen.file[i]->privateRenderData = 
				(gpointer) cairo_surface_create_similar ((cairo_surface_t *)screen.windowSurface,
				CAIRO_CONTENT_COLOR_ALPHA, screenRenderInfo.displayWidth, screenRenderInfo.displayHeight);

			cr= cairo_create(screen.file[i]->privateRenderData );
			render_layer_to_cairo_target (cr, screen.file[i], &screenRenderInfo);
			dprintf("    .... calling render_image_to_cairo_target on layer %d...\n", i);			
			cairo_destroy (cr);
		}
	}
	render_recreate_composite_surface ();

/*
	g_timer_stop (profileTimer);
	gulong timerMicros;

	gdouble timerSeconds = g_timer_elapsed (profileTimer, &timerMicros);
	g_warning ("Timer: %f seconds, %ld micros\n",timerSeconds,timerMicros);
	g_timer_destroy (profileTimer);  
*/


#endif
	callbacks_force_expose_event_for_screen();
}

#ifndef RENDER_USING_GDK
void render_all_layers_to_cairo_target_for_vector_output (cairo_t *cr, gerbv_render_info_t *renderInfo) {
	int i;
	
	/* don't paint background for vector output, since it isn't needed */
	for(i = 0; i < MAX_FILES; i++) {
		if (screen.file[i] && screen.file[i]->isVisible) {
			/* since compositing layers properly destroys the vector data,
			   we just draw right on top of the last layer (producing errors
			   in cases where something was drawn with the "clear" aperture state */
			render_layer_to_cairo_target (cr, screen.file[i], renderInfo);
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
	
	for(i = 0; i < MAX_FILES; i++) {
		if (screen.file[i] && screen.file[i]->isVisible) {
			cairo_push_group (cr);
			render_layer_to_cairo_target (cr, screen.file[i], renderInfo);
			cairo_pop_group_to_source (cr);
			cairo_paint_with_alpha (cr, screen.file[i]->alpha);		
		}
	}
}

void render_layer_to_cairo_target (cairo_t *cr, gerbv_fileinfo_t *fileInfo,
						gerbv_render_info_t *renderInfo) {
	gdouble translateX, translateY;
	
	translateX = (renderInfo->lowerLeftX * renderInfo->scaleFactor);
	translateY = (renderInfo->lowerLeftY * renderInfo->scaleFactor);
			
	cairo_set_tolerance (cr, renderInfo->renderQuality);

	/* translate the draw area before drawing.  We must translate the whole
	   drawing down an additional displayHeight to account for the negative
	   y flip done later */
	cairo_translate (cr, -translateX, translateY + renderInfo->displayHeight);
	/* scale the drawing by the specified scale factor (inverting y since
		cairo y axis points down) */
	cairo_scale (cr, renderInfo->scaleFactor, -renderInfo->scaleFactor);

	cairo_set_source_rgba (cr, (double) fileInfo->color.red/G_MAXUINT16,
		(double) fileInfo->color.green/G_MAXUINT16,
		(double) fileInfo->color.blue/G_MAXUINT16, 1);

	draw_image_to_cairo_target (cr, fileInfo->image);
}

void render_recreate_composite_surface () {
	gint i;
	
	if (screen.bufferSurface) {
		cairo_surface_destroy (screen.bufferSurface);
		screen.bufferSurface = NULL;
	}
	if (!screen.windowSurface)
		return;

	screen.bufferSurface= cairo_surface_create_similar ((cairo_surface_t *)screen.windowSurface,
	                                    CAIRO_CONTENT_COLOR, screenRenderInfo.displayWidth,
	                                    screenRenderInfo.displayHeight);

	cairo_t *cr= cairo_create(screen.bufferSurface);

	for(i = 0; i < MAX_FILES; i++) {
		if (screen.file[i] && screen.file[i]->isVisible) {
			cairo_set_source_surface (cr, (cairo_surface_t *) screen.file[i]->privateRenderData,
			                              0, 0);

			if ((double) screen.file[i]->alpha < 65535) {
				cairo_paint_with_alpha(cr,(double) screen.file[i]->alpha/G_MAXUINT16);
			}
			else {
				cairo_paint (cr);
			}    
		}
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
#endif

#ifdef RENDER_USING_GDK
void
render_to_pixmap_using_gdk (GdkPixmap *pixmap, gerbv_render_info_t *renderInfo){
	GdkGC *gc = gdk_gc_new(pixmap);
	GdkPixmap *colorStamp, *clipmask;
	int i;
	
	/* 
	 * Remove old pixmap, allocate a new one, draw the background.
	 */

	gdk_gc_set_foreground(gc, &screen.background);
	gdk_draw_rectangle(pixmap, gc, TRUE, 0, 0, -1, -1);

	/*
	 * Allocate the pixmap and the clipmask (a one pixel pixmap)
	 */
	colorStamp = gdk_pixmap_new(pixmap, renderInfo->displayWidth,
						renderInfo->displayHeight, -1);
	clipmask = gdk_pixmap_new(NULL, renderInfo->displayWidth,
						renderInfo->displayHeight, 1);
							
	//gdk_gc_set_function(gc, screen.si_func);

	/* 
	* This now allows drawing several layers on top of each other.
	* Higher layer numbers have higher priority in the Z-order. 
	*/
	for(i = 0; i < MAX_FILES; i++) {
		if (screen.file[i] && screen.file[i]->isVisible) {
			enum polarity_t polarity;

			if (screen.file[i]->inverted) {
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
			gdk_gc_set_foreground(gc, &screen.file[i]->color);
			gdk_draw_rectangle(colorStamp, gc, TRUE, 0, 0, -1, -1);

			/*
			* Translation is to get it inside the allocated pixmap,
			* which is not always centered perfectly for GTK/X.
			*/
			dprintf("  .... calling image2pixmap on image %d...\n", i);
			image2pixmap(&clipmask, screen.file[i]->image,
				renderInfo->scaleFactor, -(renderInfo->lowerLeftX * renderInfo->scaleFactor),
				(renderInfo->lowerLeftY * renderInfo->scaleFactor) + renderInfo->displayHeight,
				polarity);

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

	gdk_pixmap_unref(colorStamp);
	gdk_pixmap_unref(clipmask);
	gdk_gc_unref(gc);
}
#endif

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
    for(i = 0; i < MAX_FILES; i++) {
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
    for(i = 0; i < MAX_FILES; i++) {
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


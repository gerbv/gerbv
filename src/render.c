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

#ifdef RENDER_USING_GDK
  #include "draw-gdk.h"
#else
  #include "draw.h"
  #include <cairo-xlib.h>
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


/* ------------------------------------------------------------------ */
/* Invalidate state, free up pixmaps if necessary */
void
invalidate_redraw_state(struct gerbv_redraw_state_t *state)
{
    if (state->valid) {
	state->valid = 0;
	/* Free up pixmaps */
	if (state->curr_pixmap) {
	    gdk_pixmap_unref(state->curr_pixmap);
	}
	if (state->clipmask) {
	    gdk_pixmap_unref(state->clipmask);
	}
    }
} /* invalidate_redraw_state() */


/* ------------------------------------------------------------------ */
/*
 * idle_redraw_pixmap_active is true when idle_redraw_pixmap
 * is called, during this time we are not allowed to call the
 * gtk_idle_xxx functions.
 */
gboolean idle_redraw_pixmap_active = FALSE;
gboolean idle_redraw_pixmap(gpointer data);

void
start_idle_redraw_pixmap(GtkWidget *data)
{
    if (!idle_redraw_pixmap_active) {
	gtk_idle_add(idle_redraw_pixmap, (gpointer) data);
	idle_redraw_pixmap_active = TRUE;
    }
} /* start_idle_redraw_pixmap */


/* ------------------------------------------------------------------ */
void
stop_idle_redraw_pixmap(GtkWidget *data)
{
    if (!idle_redraw_pixmap_active) {
	gtk_idle_remove_by_data ((gpointer)data);
	idle_redraw_pixmap_active = FALSE;
    }
} /* stop_idle_redraw_pixmap */

/* ------------------------------------------------------------------ */
/**Properly Redraw zoomed image.
 * On idle callback to ensure a zoomed image is properly redrawn
 */
gboolean
idle_redraw_pixmap(gpointer data)
{
    gboolean retval;

    idle_redraw_pixmap_active = TRUE;
    if (redraw_pixmap((GtkWidget *) data, FALSE)) {
	retval = TRUE;
    } else {
	retval = FALSE;
    }
    idle_redraw_pixmap_active = FALSE;
    return retval;
} /* idle_redraw_pixmap */


/* ------------------------------------------------------------------ */
void
autoscale(void)
{
    double max_width = LONG_MIN, max_height = LONG_MIN;
    double x_scale, y_scale;
    int i;
    gerb_image_info_t *info;

    if (screen.drawing_area == NULL)
	return;

    screen.gerber_bbox.x1 = HUGE_VAL;
    screen.gerber_bbox.y1 = HUGE_VAL;
    screen.gerber_bbox.x2 = -HUGE_VAL;
    screen.gerber_bbox.y2 = -HUGE_VAL;

    for(i = 0; i < MAX_FILES; i++) {
    /*check if not only screen.file[] exists, but also if it is not a search and select layer*/
        if ((screen.file[i]) && (screen.file[i]->image != NULL)){
	  
	  info = screen.file[i]->image->info;
	  /* 
	   * Find the biggest image and use as a size reference
	   */
	  screen.gerber_bbox.x1 = MIN(screen.gerber_bbox.x1,
				      info->min_x +
				      info->offset_a_in);
	  screen.gerber_bbox.y1 = MIN(screen.gerber_bbox.y1,
				      info->min_y +
				      info->offset_b_in);
	  screen.gerber_bbox.x2 = MAX(screen.gerber_bbox.x2,
				      info->max_x +
				      info->offset_a_in);
	  screen.gerber_bbox.y2 = MAX(screen.gerber_bbox.y2,
				      info->max_y +
				      info->offset_b_in);
	}
    }

    max_width = screen.gerber_bbox.x2 - screen.gerber_bbox.x1;
    max_height = screen.gerber_bbox.y2 - screen.gerber_bbox.y1;

    /*
     * Calculate scale for both x axis and y axis
     */
    x_scale = screen.drawing_area->allocation.width / max_width;
    y_scale = screen.drawing_area->allocation.height / max_height;

    /*
     * Take the scale that fits both directions with some extra checks
     */
    screen.transf->scale = (int)ceil(MIN(x_scale, y_scale));
    if (screen.transf->scale < 1)
	screen.transf->scale = 1;
    if (screen.transf->scale > 10)
	screen.transf->scale = floor(screen.transf->scale / 10) * 10;

    /*
     * Calculate translation
     */
	screen.trans_y = -(int)((double)((screen.drawing_area->allocation.height-screen.transf->scale*(max_height))/2.0));
	screen.trans_x = -(int)((double)((screen.drawing_area->allocation.width-screen.transf->scale*(max_width))/2.0));
    

    /* Initialize clipping bbox to contain entire image */
    screen.clip_bbox.x1 = -screen.trans_x/(double)screen.transf->scale;
    screen.clip_bbox.y1 = -screen.trans_y/(double)screen.transf->scale;
    screen.clip_bbox.x2 = screen.gerber_bbox.x2-screen.gerber_bbox.x1;
    screen.clip_bbox.y2 = screen.gerber_bbox.y2-screen.gerber_bbox.y1;
    screen.off_x = 0;
    screen.off_y = 0;

    return;
} /* autoscale */




/* ------------------------------------------------------------------ */
/* Create a new backing pixmap of the appropriate size */
gint
redraw_pixmap(GtkWidget *widget, int restart)
{
    int i;
    double dmax_x = LONG_MIN, dmax_y = LONG_MIN;
    double dmin_x = LONG_MAX, dmin_y = LONG_MAX;
    GdkGC *gc = gdk_gc_new(widget->window);
    GdkRectangle update_rect;
    int file_loaded = 0;
    GdkWindow *window;
    int retval = TRUE;
    struct gerbv_redraw_state_t state;

    /* Must initialize at least this value in state */
    state.valid = 0;

    dprintf("----> Entering redraw_pixmap...\n");
    
    window = gtk_widget_get_parent_window(widget);
    /* This might be lengthy, show that we're busy by changing the pointer */
    if (window) {
	GdkCursor *cursor;
	
	cursor = gdk_cursor_new(GDK_WATCH);
	gdk_window_set_cursor(window, cursor);
	gdk_cursor_destroy(cursor);
    }
    
    /* Stop the idle-function if we are not within an idle-call */
   /* if (state.valid) {
	stop_idle_redraw_pixmap(widget);
   } */
    retval = FALSE;
    
    /* Called first when opening window and then when resizing window */
    for(i = 0; i < MAX_FILES; i++) {
	if (screen.file[i]) {
	    file_loaded++;
	}
    }
    
    /*
     * Setup scale etc first time we load a file
     */
    if (file_loaded && screen.transf->scale < 0.001) {
	autoscale();
	invalidate_redraw_state(&state);
    }
    
    /*
     * Find the biggest image and use as a size reference
     */
    dmax_x = screen.gerber_bbox.x2;
    dmax_y = screen.gerber_bbox.y2;
    
    /*
     * Also find the smallest coordinates to see if we have negative
     * ones that must be compensated for.
     */
    dmin_x = screen.gerber_bbox.x1;
    dmin_y = screen.gerber_bbox.y1;
    
    /* Should we restart drawing, or try to load a saved state? */
    if (!restart && state.valid) {
	if (file_loaded != state.files_loaded) {
	    invalidate_redraw_state(&state);
	}
	
	if ((state.max_width != screen.drawing_area->allocation.width ||
	     state.max_height != screen.drawing_area->allocation.height)) {
	    invalidate_redraw_state(&state);
	}
    } else {
	invalidate_redraw_state(&state);
    }
 

    /* Check for useful data in saved state or initialise state */
    if (!state.valid) {
	int width = 0, height = 0;
	
	/*
	 * Paranoia check; size in width or height is zero
	 */
	if (!file_loaded) {
	    dprintf("   .... Oh, no!  No file found! .... \n");
	    retval = FALSE;
	    goto redraw_pixmap_end;
	}
	
	/* Clear state */
	memset(&state, 0, sizeof(state));
	
	state.files_loaded = file_loaded;

	/*
	 * Pixmap size is always size of window, no
	 * matter how the scale.
	 */
	gdk_window_get_size(widget->window, &width, &height);	
	state.max_width = width;
	state.max_height = height;

	/* 
	 * Remove old pixmap, allocate a new one, draw the background.
	 */
	if (screen.pixmap) 
	    gdk_pixmap_unref(screen.pixmap);
	screen.pixmap = gdk_pixmap_new(widget->window, state.max_width,
				       state.max_height,  -1);
	gdk_gc_set_foreground(gc, screen.background);
	gdk_draw_rectangle(screen.pixmap, gc, TRUE, 0, 0, -1, -1);
#ifdef RENDER_USING_GDK	
	/*
	 * Allocate the pixmap and the clipmask (a one pixel pixmap)
	 */
	state.curr_pixmap = gdk_pixmap_new(widget->window,
					   state.max_width,
					   state.max_height,  -1);
	state.clipmask = gdk_pixmap_new(widget->window,
					state.max_width,
					state.max_height,  1);
#endif
	state.valid = 1;
    }

    if (g_main_pending()) {
	/* return TRUE to keep this idle function active */
	dprintf("   .... Oh, no!  Found pending event to process! .... \n");
	retval = TRUE;
	start_idle_redraw_pixmap(widget);
	state.file_index = i;
	goto redraw_pixmap_end;
    }
	  
#ifdef RENDER_USING_GDK	
    dprintf("   .... Now try rendering the drawing using GDK .... \n");
    /*
     * Set superimposing function.
     */
    gdk_gc_set_function(gc, screen.si_func);


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
	     * Show progress in status bar
	     */
	    snprintf(screen.statusbar.msgstr, MAX_STATUSMSGLEN,
		     "%d %s...",
		     i, screen.file[i]->basename);
	    callbacks_update_statusbar();

	    /*
	     * Fill up image with all the foreground color. Excess pixels
	     * will be removed by clipmask.
	     */
	    gdk_gc_set_rgb_fg_color(gc, screen.file[i]->color);
	    gdk_draw_rectangle(state.curr_pixmap, gc, TRUE, 0, 0, -1, -1);

	    /*
	     * Translation is to get it inside the allocated pixmap,
	     * which is not always centered perfectly for GTK/X.
	     */
	    dprintf("  .... calling image2pixmap on image %d...\n", i);
	    image2pixmap(&(state.clipmask),
			 screen.file[i]->image, screen.transf->scale, 
			 (screen.clip_bbox.x1-dmin_x)*screen.transf->scale,
			 (screen.clip_bbox.y1+dmax_y)*screen.transf->scale,
			 polarity);

	    /* 
	     * Set clipmask and draw the clipped out image onto the
	     * screen pixmap. Afterwards we remove the clipmask, else
	     * it will screw things up when run this loop again.
	     */
	    gdk_gc_set_clip_mask(gc, state.clipmask);
	    gdk_gc_set_clip_origin(gc, 0, 0);
	    gdk_draw_pixmap(screen.pixmap, gc, state.curr_pixmap, -screen.off_x, -screen.off_y, 0, 0, -1, -1);
	    gdk_gc_set_clip_mask(gc, NULL);
	}
    }

    screen.statusbar.msgstr[0] = '\0';
    callbacks_update_statusbar();
    /* Clean up */
    state.valid = 0;
    /* Free up pixmaps */
    if (state.curr_pixmap) {
	gdk_pixmap_unref(state.curr_pixmap);
    }
    if (state.clipmask) {
	gdk_pixmap_unref(state.clipmask);
    }
    gdk_gc_unref(gc);
#else
    dprintf("    .... Now try rendering the drawing using cairo .... \n");
    cairo_t *cr;

    window = gtk_widget_get_parent_window(widget);
    /* This might be lengthy, show that we're busy by changing the pointer */
    if (window) {
	GdkCursor *cursor;
	
	cursor = gdk_cursor_new(GDK_WATCH);
	gdk_window_set_cursor(window, cursor);
	gdk_cursor_destroy(cursor);
    }
    
    /* get a cairo_t */
    cairo_surface_t* cs=cairo_image_surface_create (CAIRO_FORMAT_RGB24, state.max_width, state.max_height);
    cr= cairo_create(cs);
    
    /* speed up rendering by reducing quality by a small amount (default
       is 1.0 */
    cairo_set_tolerance (cr,1.5);
    
    /* translate the draw area before drawing */
    cairo_translate (cr,-screen.trans_x-(screen.gerber_bbox.x1*(float) screen.transf->scale),-screen.trans_y+(screen.gerber_bbox.y2*(float) screen.transf->scale));
    
    /* scale the drawing by the specified scale factor (inverting y since
     * cairo y axis points down)
     */ 
    cairo_scale (cr,(float) screen.transf->scale,-(float) screen.transf->scale);
    
    /* 
     * This now allows drawing several layers on top of each other.
     * Higher layer numbers have higher priority in the Z-order. 
     */
    for(i = 0; i < MAX_FILES; i++) {
	if (screen.file[i] && screen.file[i]->isVisible) {
	    cairo_pattern_t *testp = NULL;
	    
	    cairo_push_group   (cr);
	    
	    cairo_set_source_rgba (cr, (double) screen.file[i]->color->red/G_MAXUINT16,
				   (double) screen.file[i]->color->green/G_MAXUINT16,
				   (double) screen.file[i]->color->blue/G_MAXUINT16, 1);
	    
	    /* for now, scale the cairo context if the units are mms */
	    /* TODO: normalize all gerb_image data to mm during parsing */
	    cairo_save (cr);
	    if ((screen.file[i]->image->netlist->next)&&
		(screen.file[i]->image->netlist->next->unit==MM)) {
		cairo_scale (cr, 1.0/25.4, 1.0/25.4);
	    }
	    dprintf("    .... calling render_image_to_cairo_target on layer %d...\n", i);
	    draw_image_to_cairo_target (cr, screen.file[i]->image);
	    /* make sure we cancel any previous scaling for mm */
	    cairo_restore (cr);
	    testp = cairo_pop_group (cr);
	    if (screen.file[i]->privateRenderData) {
		cairo_pattern_destroy ((cairo_pattern_t *) screen.file[i]->privateRenderData);
	    }
	    screen.file[i]->privateRenderData = (gpointer) testp;
	}
    }
    cairo_surface_destroy (cs);
    cairo_destroy (cr);	

#endif

    update_rect.x = 0, update_rect.y = 0;
    update_rect.width =	widget->allocation.width;
    update_rect.height = widget->allocation.height;
    
    /*
     * Calls expose_event
     */
    gtk_widget_draw (widget, &update_rect);
    //gdk_window_invalidate_rect(widget->window, &widget->allocation,FALSE);
    
 redraw_pixmap_end:
    /* Return default pointer shape */
    if (window) {
	gdk_window_set_cursor(window, GERBV_DEF_CURSOR);
    }
       
    dprintf("<---- leaving redraw_pixmap.\n");
    return retval;
} /* redraw_pixmap */


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
	    instats = screen.file[i]->image->stats;
	    gerb_stats_add_layer(stats, instats);
	}
    }
    
    return stats;
}

/* ------------------------------------------------------------------ */
/* Fill out the drill file statistics table */
gerb_stats_t *
generate_drill_analysis(void)
{
    int i;
    gerb_stats_t *stats;
    gerb_stats_t *instats;

    stats = gerb_stats_new();

    /* Loop through open layers and compile statistics */
    for(i = 0; i < MAX_FILES; i++) {
	if (screen.file[i] && 
	    screen.file[i]->isVisible &&
	    (screen.file[i]->image->layertype == DRILL) ) {
	    instats = screen.file[i]->image->stats;
	    gerb_stats_add_layer(stats, instats);
	}
    }
    
    return stats;
}


#ifndef RENDER_USING_GDK
void render_project_to_cairo_target (cairo_t *cr) {
	int i;
	
	/* translate the draw area before drawing */
	cairo_translate (cr,-screen.trans_x-(screen.gerber_bbox.x1*(float) screen.transf->scale),
			 -screen.trans_y+(screen.gerber_bbox.y2*(float) screen.transf->scale));
	/* scale the drawing by the specified scale factor (inverting y since
	 * cairo y axis points down)
	 */ 
	cairo_scale (cr,(float) screen.transf->scale,-(float) screen.transf->scale);
	
	/* fill the background with the appropriate color */
	cairo_set_source_rgba (cr, (double) screen.background->red/G_MAXUINT16,
		(double) screen.background->green/G_MAXUINT16,
		(double) screen.background->blue/G_MAXUINT16, 1);
	cairo_paint (cr);

	/* 
	* This now allows drawing several layers on top of each other.
	* Higher layer numbers have higher priority in the Z-order. 
	*/
	for(i = 0; i < MAX_FILES; i++) {
		if (screen.file[i] && screen.file[i]->isVisible && screen.file[i]->privateRenderData) {
			enum polarity_t polarity;

			if (screen.file[i]->inverted) {
				if (screen.file[i]->image->info->polarity == POSITIVE)
				polarity = NEGATIVE;
				else
				polarity = POSITIVE;
			} else {
				polarity = screen.file[i]->image->info->polarity;
			}
			
                  cairo_set_source (cr,(cairo_pattern_t *)screen.file[i]->privateRenderData);
			if ((double) screen.file[i]->alpha < 65535) {				
				cairo_paint_with_alpha(cr,(double) screen.file[i]->alpha/G_MAXUINT16);
			}
			else {
				cairo_paint (cr);
			}
		}
	}
}

#endif

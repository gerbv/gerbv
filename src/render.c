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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/** \file render.c
    \brief Rendering support functions for gerbv
    \ingroup gerbv
*/

#include "gerbv.h"

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_LIBGEN_H
# include <libgen.h> /* dirname */
#endif

#include <math.h>

#include "common.h"
#include "main.h"
#include "callbacks.h"
#include "interface.h"
#include "render.h"
#include "selection.h"

#ifdef WIN32
# include <cairo-win32.h>
#elif QUARTZ
# include <cairo-quartz.h>
#else
# include <cairo-xlib.h>
#endif
#include "draw.h"

#define dprintf if(DEBUG) printf

gerbv_render_info_t screenRenderInfo;

/* ------------------------------------------------------ */
void
render_zoom_display (gint zoomType, gdouble scaleFactor, gdouble mouseX, gdouble mouseY)
{
	gdouble mouseCoordinateX = 0.0;
	gdouble mouseCoordinateY = 0.0;
	double oldWidth, oldHeight;

	oldWidth = screenRenderInfo.displayWidth / screenRenderInfo.scaleFactorX;
	oldHeight = screenRenderInfo.displayHeight / screenRenderInfo.scaleFactorY;

	if (zoomType == ZOOM_IN_CMOUSE || zoomType == ZOOM_OUT_CMOUSE) {
		/* calculate what user coordinate the mouse is pointing at */
		mouseCoordinateX = mouseX / screenRenderInfo.scaleFactorX + screenRenderInfo.lowerLeftX;
		mouseCoordinateY = (screenRenderInfo.displayHeight - mouseY) /
			screenRenderInfo.scaleFactorY + screenRenderInfo.lowerLeftY;
	}

	switch(zoomType) {
		case ZOOM_IN : /* Zoom In */
		case ZOOM_IN_CMOUSE : /* Zoom In Around Mouse Pointer */
			screenRenderInfo.scaleFactorX = MIN((gdouble)GERBV_SCALE_MAX,
					(1 + 1/3.0)*screenRenderInfo.scaleFactorX);
			screenRenderInfo.scaleFactorY = screenRenderInfo.scaleFactorX;
			screenRenderInfo.lowerLeftX += (oldWidth - (screenRenderInfo.displayWidth /
						screenRenderInfo.scaleFactorX)) / 2.0;
			screenRenderInfo.lowerLeftY += (oldHeight - (screenRenderInfo.displayHeight /
						screenRenderInfo.scaleFactorY)) / 2.0;
			break;
		case ZOOM_OUT :  /* Zoom Out */
		case ZOOM_OUT_CMOUSE : /* Zoom Out Around Mouse Pointer */
			screenRenderInfo.scaleFactorX = MAX((gdouble)GERBV_SCALE_MIN,
					(1 - 1/3.0)*screenRenderInfo.scaleFactorX);
			screenRenderInfo.scaleFactorY = screenRenderInfo.scaleFactorX;
			screenRenderInfo.lowerLeftX += (oldWidth - (screenRenderInfo.displayWidth /
						screenRenderInfo.scaleFactorX)) / 2.0;
			screenRenderInfo.lowerLeftY += (oldHeight - (screenRenderInfo.displayHeight /
						screenRenderInfo.scaleFactorY)) / 2.0;
			break;
		case ZOOM_FIT : /* Zoom Fit */
			gerbv_render_zoom_to_fit_display (mainProject, &screenRenderInfo);
			break;
		case ZOOM_SET : /*explicit scale set by user */
			screenRenderInfo.scaleFactorX =
				MIN((gdouble)GERBV_SCALE_MAX, scaleFactor);
			screenRenderInfo.scaleFactorY = screenRenderInfo.scaleFactorX;
			screenRenderInfo.lowerLeftX += (oldWidth - (screenRenderInfo.displayWidth /
						screenRenderInfo.scaleFactorX)) / 2.0;
			screenRenderInfo.lowerLeftY += (oldHeight - (screenRenderInfo.displayHeight /
						screenRenderInfo.scaleFactorY)) / 2.0;
			break;
		default :
			GERB_MESSAGE(_("Illegal zoom direction %d"), zoomType);
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

	x1 = MIN((gdouble)screen.start_x, event->x);
	y1 = MIN((gdouble)screen.start_y, event->y);
	x2 = MAX((gdouble)screen.start_x, event->x);
	y2 = MAX((gdouble)screen.start_y, event->y);
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

		screenRenderInfo.scaleFactorX *=
			MIN((double)screenRenderInfo.displayWidth / dx,
				(double)screenRenderInfo.displayHeight / dy);
		screenRenderInfo.scaleFactorX = MIN((gdouble)GERBV_SCALE_MAX,
				screenRenderInfo.scaleFactorX);
		screenRenderInfo.scaleFactorY = screenRenderInfo.scaleFactorX;
		screenRenderInfo.lowerLeftX = centerPointX - (screenRenderInfo.displayWidth /
					2.0 / screenRenderInfo.scaleFactorX);
		screenRenderInfo.lowerLeftY = centerPointY - (screenRenderInfo.displayHeight /
					2.0 / screenRenderInfo.scaleFactorY);
	}
	render_refresh_rendered_image_on_screen();
}

/* ------------------------------------------------------ */
void
render_draw_selection_box_outline(cairo_t *cr) {
	gint x1, y1, x2, y2, dx, dy;

	cairo_set_operator(cr, CAIRO_OPERATOR_DIFFERENCE);
	gdk_cairo_set_source_rgba(cr, &screen.zoom_outline_color);
	
	x1 = MIN(screen.start_x, screen.last_x);
	y1 = MIN(screen.start_y, screen.last_y);
	x2 = MAX(screen.start_x, screen.last_x);
	y2 = MAX(screen.start_y, screen.last_y);
	dx = x2-x1;
	dy = y2-y1;

	cairo_rectangle(cr, x1, y1, dx, dy);
	cairo_stroke(cr);
}

/* --------------------------------------------------------- */
void
render_draw_zoom_outline(cairo_t *cr, gboolean centered)
{
	gint x1, y1, x2, y2, dx, dy;
	GtkAllocation allocation;

	cairo_set_operator(cr, CAIRO_OPERATOR_DIFFERENCE);
	gdk_cairo_set_source_rgba(cr, &screen.zoom_outline_color);
	
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

	cairo_rectangle(cr, x1, y1, dx, dy);
	cairo_stroke(cr);

	/* Draw actual zoom area in dashed lines */
	const double on_off[] = {5, 5};
	cairo_set_dash(cr, on_off, 2, 0);

	gtk_widget_get_allocation(screen.drawing_area, &allocation);
	
	if ((dy == 0) || ((double)dx/dy > (double)allocation.width / allocation.height)) {
		dy = dx * (double)allocation.height / allocation.width;
	} 
	else {
		dx = dy * (double)allocation.width / allocation.height;
	}

	cairo_rectangle(cr, (x1+x2-dx)/2, (y1+y2-dy)/2, dx, dy);
	cairo_stroke(cr);
}

/* ------------------------------------------------------ */
/* Transforms board coordinates to screen ones */
static void
render_board2screen(gdouble *X, gdouble *Y, gdouble x, gdouble y) {
	*X = (x - screenRenderInfo.lowerLeftX) * screenRenderInfo.scaleFactorX;
	*Y = screenRenderInfo.displayHeight - (y - screenRenderInfo.lowerLeftY)
		* screenRenderInfo.scaleFactorY;
}

/* Trims the coordinates to avoid overflows in gdk_draw_line */
static void
render_trim_point(gdouble *start_x, gdouble *start_y, gdouble last_x, gdouble last_y)
{
	const gdouble max_coord = (1<<15) - 2;/* a value that causes no overflow
											 and lies out of screen */
	gdouble dx, dy;

    if (fabs (*start_x) < max_coord && fabs (*start_y) < max_coord)
		return;	

	dx = last_x - *start_x;
	dy = last_y - *start_y;

	if (*start_x < -max_coord) {
		*start_x = -max_coord;
		if (last_x > -max_coord && fabs (dx) > 0.1)
			*start_y = last_y - (last_x + max_coord) / dx * dy;
	}
	if (*start_x > max_coord) {
		*start_x = max_coord;
		if (last_x < max_coord && fabs (dx) > 0.1)
			*start_y = last_y - (last_x - max_coord) / dx * dy;
	}

	dx = last_x - *start_x;
	dy = last_y - *start_y;

	if (*start_y < -max_coord) {
		*start_y = -max_coord;
		if (last_y > -max_coord && fabs (dy) > 0.1)
			*start_x = last_x - (last_y + max_coord) / dy * dx;
	}
	if (*start_y > max_coord) {
		*start_y = max_coord;
		if (last_y < max_coord && fabs (dy) > 0.1)
			*start_x = last_x - (last_y - max_coord) / dy * dx;
	}
}

/* ------------------------------------------------------ */
/** Draws/erases measure line
 */
void
render_toggle_measure_line(cairo_t *cr)
{
	gdouble start_x, start_y, last_x, last_y;

	cairo_set_line_width(cr, 6);
	cairo_set_operator(cr, CAIRO_OPERATOR_DIFFERENCE);
	gdk_cairo_set_source_rgba(cr, &screen.zoom_outline_color);

	render_board2screen(&start_x, &start_y,
				screen.measure_start_x, screen.measure_start_y);
	render_board2screen(&last_x, &last_y,
				screen.measure_stop_x, screen.measure_stop_y);
	render_trim_point(&start_x, &start_y, last_x, last_y);
	render_trim_point(&last_x, &last_y, start_x, start_y);

	cairo_move_to(cr, start_x, start_y);
	cairo_line_to(cr, last_x, last_y);
	cairo_stroke(cr);
} /* toggle_measure_line */

/* ------------------------------------------------------ */
/** Displays a measured distance graphically on screen and in statusbar. */
void
update_statusbar_measure_distance(void)
{
	gdouble dx, dy;

	dx = fabs (screen.measure_start_x - screen.measure_stop_x);
	dy = fabs (screen.measure_start_y - screen.measure_stop_y);

	screen.measure_last_x = dx;
	screen.measure_last_y = dy;
	callbacks_update_statusbar_measured_distance (dx, dy);
}

/* ------------------------------------------------------ */
static void render_selection (void)
{
	gerbv_selection_item_t sel_item;
	gerbv_fileinfo_t *file;
	gdouble pixel_width;
	cairo_t *cr;
	int i;
	guint j;

	if (selection_length (&screen.selectionInfo) == 0)
		return;

	if (screen.selectionRenderData)
		cairo_surface_destroy (
				(cairo_surface_t *)screen.selectionRenderData);

	screen.selectionRenderData =
		(gpointer) cairo_surface_create_similar (
			(cairo_surface_t *)screen.windowSurface,
			CAIRO_CONTENT_COLOR_ALPHA,
			screenRenderInfo.displayWidth,
			screenRenderInfo.displayHeight);

	pixel_width = 1.0/MAX(screenRenderInfo.scaleFactorX,
				screenRenderInfo.scaleFactorY);

	for (i = mainProject->last_loaded; i >= 0; i--) {
		file = mainProject->file[i]; 

		if (!file ||
		(!mainProject->show_invisible_selection && !file->isVisible))
			continue;

		for (j = 0; j < selection_length (&screen.selectionInfo); j++) {
			sel_item = selection_get_item_by_index (
					&screen.selectionInfo, j);
			if (file->image != sel_item.image)
				continue;

			/* Have selected image(s) on this file, draw it */

			cr = cairo_create(screen.selectionRenderData);
			gerbv_render_cairo_set_scale_and_translation(cr,
					&screenRenderInfo);
			cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.85);
			draw_image_to_cairo_target (cr,
				file->image, pixel_width,
				DRAW_SELECTIONS, &screen.selectionInfo,
				&screenRenderInfo, TRUE,
				file->transform, TRUE);
			cairo_destroy (cr);

			break;	/* All done, go to next file */
		}
	}
}

/* ------------------------------------------------------ */
void render_refresh_rendered_image_on_screen (void) {
	GdkCursor *cursor;
	GdkWindow *window = gtk_widget_get_window(screen.drawing_area);

	dprintf("----> Entering redraw_pixmap...\n");
	cursor = gdk_cursor_new_for_display(gdk_display_get_default(), GDK_WATCH);
	gdk_window_set_cursor(GDK_WINDOW(window), cursor);
	g_object_unref(cursor);

	int i;
	dprintf("    .... Now try rendering the drawing using cairo .... \n");
	/*
	 * This now allows drawing several layers on top of each other.
	 * Higher layer numbers have higher priority in the Z-order.
	 */
	for(i = mainProject->last_loaded; i >= 0; i--) {
		if (mainProject->file[i]) {
			cairo_t *cr;
			if (mainProject->file[i]->privateRenderData) 
				cairo_surface_destroy ((cairo_surface_t *) mainProject->file[i]->privateRenderData);
			mainProject->file[i]->privateRenderData = 
					(gpointer) cairo_surface_create_similar ((cairo_surface_t *)screen.windowSurface,
							CAIRO_CONTENT_COLOR_ALPHA, screenRenderInfo.displayWidth,
							screenRenderInfo.displayHeight);
			cr= cairo_create(mainProject->file[i]->privateRenderData );
			gerbv_render_layer_to_cairo_target (cr, mainProject->file[i], &screenRenderInfo);
			dprintf("    .... calling render_image_to_cairo_target on layer %d...\n", i);			
			cairo_destroy (cr);
		}
	}

	render_recreate_composite_surface ();
	/* remove watch cursor and switch back to normal cursor */
	callbacks_switch_to_correct_cursor ();
	callbacks_force_expose_event_for_screen();
}

/* ------------------------------------------------------ */
void
render_remove_selected_objects_belonging_to_layer (
			gerbv_selection_info_t *sel_info, gerbv_image_t *image)
{
	guint i;

	for (i = 0; i < selection_length (sel_info);) {
		gerbv_selection_item_t sItem =
			selection_get_item_by_index (sel_info, i);

		if (image == (gerbv_image_t *) sItem.image)
			selection_clear_item_by_index (sel_info, i);
		else
			i++;
	}
}

/* ------------------------------------------------------ */
gint
render_create_cairo_buffer_surface () {
	if (screen.bufferSurface) {
		cairo_surface_destroy (screen.bufferSurface);
		screen.bufferSurface = NULL;
	}
	if (!screen.windowSurface)
		return 0;

	screen.bufferSurface = cairo_surface_create_similar (screen.windowSurface,
			CAIRO_CONTENT_COLOR, screenRenderInfo.displayWidth,
			screenRenderInfo.displayHeight);
	return 1;
}

/* ------------------------------------------------------ */
static void
render_find_selected_objects_and_refresh_display (gint activeFileIndex,
		enum selection_action action)
{
	enum draw_mode mode = FIND_SELECTIONS;

	/* clear the old selection array if desired */
	if ((action == SELECTION_REPLACE)
	 && (selection_length (&screen.selectionInfo) != 0))
		selection_clear (&screen.selectionInfo);

	if (action == SELECTION_TOGGLE)
		mode = FIND_SELECTIONS_TOGGLE;

	/* make sure we have a bufferSurface...if we start up in FAST mode, we may not
	   have one yet, but we need it for selections */
	if (!render_create_cairo_buffer_surface ())
		return;

	/* call draw_image... passing the FILL_SELECTION mode to just search for
	   nets which match the selection, and fill the selection buffer with them */
	cairo_t *cr = cairo_create (screen.bufferSurface);	
	gerbv_render_cairo_set_scale_and_translation (cr, &screenRenderInfo);
	draw_image_to_cairo_target (cr,
			mainProject->file[activeFileIndex]->image,
			1.0/MAX (screenRenderInfo.scaleFactorX,
				screenRenderInfo.scaleFactorY),
			mode, &screen.selectionInfo, &screenRenderInfo, TRUE,
			mainProject->file[activeFileIndex]->transform, TRUE);
	cairo_destroy (cr);

	/* re-render the selection buffer layer */
	render_recreate_composite_surface ();
	callbacks_force_expose_event_for_screen ();
}

/* ------------------------------------------------------ */
void
render_fill_selection_buffer_from_mouse_click (gint mouseX, gint mouseY,
		gint activeFileIndex, enum selection_action action)
{
	screen.selectionInfo.lowerLeftX = mouseX;
	screen.selectionInfo.lowerLeftY = mouseY;
	/* no need to populate the upperright coordinates for a point_click */
	screen.selectionInfo.type = GERBV_SELECTION_POINT_CLICK;
	render_find_selected_objects_and_refresh_display (
			activeFileIndex, action);
}

/* ------------------------------------------------------ */
void
render_fill_selection_buffer_from_mouse_drag (gint corner1X, gint corner1Y,
		gint corner2X, gint corner2Y,
		gint activeFileIndex, enum selection_action action)
{
	/* figure out the lower left corner of the box */
	screen.selectionInfo.lowerLeftX = MIN(corner1X, corner2X);
	screen.selectionInfo.lowerLeftY = MIN(corner1Y, corner2Y);
	/* figure out the upper right corner of the box */
	screen.selectionInfo.upperRightX = MAX(corner1X, corner2X);
	screen.selectionInfo.upperRightY = MAX(corner1Y, corner2Y);
	
	screen.selectionInfo.type = GERBV_SELECTION_DRAG_BOX;
	render_find_selected_objects_and_refresh_display (
			activeFileIndex, action);
}

/* ------------------------------------------------------ */
void render_recreate_composite_surface ()
{
	gint i;
	
	if (!render_create_cairo_buffer_surface())
		return;

	cairo_t *cr = cairo_create(screen.bufferSurface);
	/* fill the background with the appropriate color */
	gdk_cairo_set_source_rgba (cr, &mainProject->background);
	cairo_paint (cr);
	
	for(i = mainProject->last_loaded; i >= 0; i--) {
		if (mainProject->file[i] && mainProject->file[i]->isVisible) {
			cairo_set_source_surface (cr, (cairo_surface_t *) mainProject->file[i]->privateRenderData,
			                              0, 0);
			cairo_paint_with_alpha (cr, mainProject->file[i]->color.alpha);
		}
	}

	/* render the selection layer at the end */
	if (selection_length (&screen.selectionInfo) != 0) {
		render_selection ();
		cairo_set_source_surface (cr, (cairo_surface_t *) screen.selectionRenderData,
			                              0, 0);
		cairo_paint_with_alpha (cr, 1.0);
	}
	cairo_destroy (cr);
}

/* ------------------------------------------------------ */
void render_project_to_cairo_target (cairo_t *cr) {
	/* fill the background with the appropriate color */
	gdk_cairo_set_source_rgba (cr, &mainProject->background);
	cairo_paint (cr);

	cairo_set_source_surface (cr, screen.bufferSurface, 0 , 0);

	cairo_paint (cr);
}

void
render_free_screen_resources (void) {
	if (screen.selectionRenderData) 
		cairo_surface_destroy (screen.selectionRenderData);
	if (screen.bufferSurface)
		cairo_surface_destroy (screen.bufferSurface);
	if (screen.windowSurface)
		cairo_surface_destroy (screen.windowSurface);
}


/* ------------------------------------------------------------------ */
/*! This fills out the project's Gerber statistics table.
 *  It is called from within callbacks.c when the user
 *  asks for a Gerber report.  */
gerbv_stats_t *
generate_gerber_analysis(void)
{
	int i;
	gerbv_stats_t *stats;
	gerbv_stats_t *instats;

	/* Create new stats structure to hold report for whole project 
	* (i.e. all layers together) */
	stats = gerbv_stats_new();

	/* Loop through open layers and compile statistics by accumulating reports from each layer */
	for (i = 0; i <= mainProject->last_loaded; i++) {
		if (mainProject->file[i] && mainProject->file[i]->isVisible &&
				(mainProject->file[i]->image->layertype == GERBV_LAYERTYPE_RS274X) ) {
			instats = mainProject->file[i]->image->gerbv_stats;
			gerbv_stats_add_layer(stats, instats, i+1);
		}
	}
	return stats;
}


/* ------------------------------------------------------------------ */
/*! This fills out the project's Drill statistics table.
 *  It is called from within callbacks.c when the user
 *  asks for a Drill report.  */
gerbv_drill_stats_t *
generate_drill_analysis(void)
{
	int i;
	gerbv_drill_stats_t *stats;
	gerbv_drill_stats_t *instats;

	stats = gerbv_drill_stats_new();

	/* Loop through open layers and compile statistics by accumulating reports from each layer */
	for(i = mainProject->last_loaded; i >= 0; i--) {
		if (mainProject->file[i] && 
				mainProject->file[i]->isVisible &&
				(mainProject->file[i]->image->layertype == GERBV_LAYERTYPE_DRILL) ) {
			instats = mainProject->file[i]->image->drill_stats;
			/* add this batch of stats.  Send the layer 
			* index for error reporting */
			gerbv_drill_stats_add_layer(stats, instats, i+1);
		}
	}
	return stats;
}


/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2000-2001 Stefan Petersen (spe@stacken.kth.se)
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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>  /* ceil(), atan2() */

#include <gtk/gtk.h>

#include "draw.h"

#ifndef err
#define err(errcode, a...) \
     do { \
           fprintf(stderr, ##a); \
           exit(errcode);\
     } while (0)
#endif

#undef round
#define round(x) floor((double)(x) + 0.5)

#define MAX_POLYGON_POINTS 50

/*
 * Draws a circle _centered_ at x,y with diameter dia
 */
static void 
gerbv_draw_circle(GdkPixmap *pixmap, GdkGC *gc, 
		  gint filled, gint x, gint y, gint dia)
{
    static const gint full_circle = 23360;
    gint real_x = x - dia / 2;
    gint real_y = y - dia / 2;
    
    gdk_draw_arc(pixmap, gc, filled, real_x, real_y, dia, dia, 0, full_circle);
    
    return;
} /* gerbv_draw_circle */


/*
 * Draws a rectangle _centered_ at x,y with sides x_side, y_side
 */
static void 
gerbv_draw_rectangle(GdkPixmap *pixmap, GdkGC *gc, 
		     gint filled, gint x, gint y, gint x_side, gint y_side)
{
    
    gint real_x = x - x_side / 2;
    gint real_y = y - y_side / 2;
    
    gdk_draw_rectangle(pixmap, gc, filled, real_x, real_y, x_side, y_side);
    
    return;
} /* gerbv_draw_rectangle */


/*
 * Draws an oval _centered_ at x,y with x axle x_axle and y axle y_axle
 */ 
static void
gerbv_draw_oval(GdkPixmap *pixmap, GdkGC *gc, 
		gint filled, gint x, gint y, gint x_axle, gint y_axle)
{
    static const gint full_circle = 23360;
    gint real_x = x - x_axle / 2;
    gint real_y = y - y_axle / 2;
    
    gdk_draw_arc(pixmap, gc, filled, real_x, real_y, x_axle, y_axle, 0, 
		 full_circle);

    return;
} /* gerbv_draw_oval */


/*
 * Draws an arc 
 * Draws an arc _centered_ at x,y
 * direction:  0 counterclockwise, 1 clockwise
 */
static void
gerbv_draw_arc(GdkPixmap *pixmap, GdkGC *gc,
	       int x, int y,
	       int width, int height,
	       int angle1, int angle2)
{
    gint real_x = x - width / 2;
    gint real_y = y - height / 2;

    gdk_draw_arc(pixmap, gc, FALSE, real_x, real_y, width, height, 
		 angle1 * 64, (angle2 - angle1) * 64);
    
    return;
} /* gerbv_draw_arc */


/*
 * Convert a gerber image to a GTK pixmap to be displayed
 */
int
image2pixmap(GdkPixmap **pixmap, struct gerb_image *image, 
	     int scale, double trans_x, double trans_y,
	     enum polarity_t polarity, 
	     GdkColor *fg_color, GdkColor *bg_color, GdkColor *err_color)
{
    GdkGC *line_gc = gdk_gc_new(*pixmap);
    GdkGC *err_gc = gdk_gc_new(*pixmap);
    struct gerb_net *net;
    gint x1, y1, x2, y2;
    int p1, p2;
    int width = 0, height = 0;
    int cp_x = 0, cp_y = 0;
    GdkPoint points[MAX_POLYGON_POINTS]; /* XXX Size */
    int nuf_points = 0;


    if (image == NULL || image->netlist == NULL) 
	    return 0;
    
    if (polarity == NEGATIVE) 
	/* Black */
	gdk_gc_set_foreground(line_gc, bg_color);
    else
	/* Current line color */
	gdk_gc_set_foreground(line_gc, fg_color);
    
    for (net = image->netlist->next ; net != NULL; net = net->next) {
	
	/*
	 * Scale points with window scaling and translate them
	 */
	x1 = (int)round((image->info->offset_a + net->start_x) * scale +
			trans_x);
	y1 = (int)round((image->info->offset_b - net->start_y) * scale +
			trans_y);
	x2 = (int)round((image->info->offset_a + net->stop_x) * scale +
			trans_x);
	y2 = (int)round((image->info->offset_b - net->stop_y) * scale +
			trans_y);

	/* 
	 * If circle segment, scale and translate that one too
	 */
	if (net->cirseg) {
	    width = (int)round(net->cirseg->width * scale);
	    height = (int)round(net->cirseg->height * scale);
	    cp_x = (int)round((image->info->offset_a + net->cirseg->cp_x) *
			      scale + trans_x);
	    cp_y = (int)round((image->info->offset_b - net->cirseg->cp_y) *
			      scale + trans_y);
	}

	/*
	 * Polygon Area Fill routines
	 */
	switch (net->interpolation) {
	case PAREA_START :
	    nuf_points = 0;
	    continue;
	case PAREA_FILL :
	    points[nuf_points].x = x2;
	    points[nuf_points].y = y2;
	    if (nuf_points > MAX_POLYGON_POINTS)
		fprintf(stderr, "Too many corners on polygon area\n");
	    else
		nuf_points++;
	    continue;
	case PAREA_END :
	    gdk_draw_polygon(*pixmap, line_gc, 1, points, nuf_points);
	    continue;
	default :
	    break;
	}

	if (image->aperture[net->aperture] == NULL) {
	    fprintf(stderr, "Aperture [%d] is not defined\n", net->aperture);
	    continue;
	}

	/*
	 * "Normal" aperture drawing routines
	 */
	switch (net->aperture_state) {
	case ON :
	    p1 = (int)round(image->aperture[net->aperture]->parameter[0] * scale);
	    gdk_gc_set_line_attributes(line_gc, p1, 
				       GDK_LINE_SOLID, 
				       GDK_CAP_ROUND, 
				       GDK_JOIN_MITER);
	    switch (net->interpolation) {
	    case LINEARx10 :
	    case LINEARx01 :
	    case LINEARx001 :
		fprintf(stderr, "Linear != x1\n");
		gdk_gc_set_foreground(err_gc, err_color);
		gdk_gc_set_line_attributes(err_gc, p1, 
					   GDK_LINE_SOLID, 
					   GDK_CAP_ROUND, 
					   GDK_JOIN_MITER);
		gdk_draw_line(*pixmap, err_gc, x1, y1, x2, y2);
		break;
	    case LINEARx1 :
		gdk_draw_line(*pixmap, line_gc, x1, y1, x2, y2);
		break;
		
	    case MQ_CW_CIRCULAR :
	    case MQ_CCW_CIRCULAR :
	    case CW_CIRCULAR :
	    case CCW_CIRCULAR :
		gerbv_draw_arc(*pixmap, line_gc, cp_x, cp_y, width, height, 
			       net->cirseg->angle1, net->cirseg->angle2);
		break;		
	    default :
		
	    }
	    break;
	case OFF :
	    break;
	case FLASH :
	    p1 = (int)round(image->aperture[net->aperture]->parameter[0] * scale);
	    p2 = (int)round(image->aperture[net->aperture]->parameter[1] * scale);
	    
	    switch (image->aperture[net->aperture]->type) {
	    case CIRCLE :
		gerbv_draw_circle(*pixmap, line_gc, TRUE, x2, y2, p1);
		break;
	    case RECTANGLE:
		gerbv_draw_rectangle(*pixmap, line_gc, TRUE, x2, y2, p1, p2);
		break;
	    case OVAL :
		gerbv_draw_oval(*pixmap, line_gc, TRUE, x2, y2, p1, p2);
		break;
	    case POLYGON :
		fprintf(stderr, "Warning! Very bad at drawing polygons.\n");
		gerbv_draw_circle(*pixmap, line_gc, TRUE, x2, y2, p1);
		break;
	    case MACRO :
		gdk_gc_set_foreground(err_gc, err_color);
		gerbv_draw_circle(*pixmap, err_gc, TRUE, x2, y2, scale/20);
		break;
	    default :
		err(1, "Unknown aperture type\n");
	    }
	    break;
	default :
	    err(1, "Unknown aperture state\n");
	}
    }
    
    return 1;

} /* image2pixmap */

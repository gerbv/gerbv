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
	     int scale, gint trans_x, gint trans_y, enum polarity_t polarity, 
	     GdkColor *fg_color, GdkColor *bg_color)
{
    GdkGC *line_gc = gdk_gc_new(*pixmap);
    GdkColor err_color;
    GdkColor other_color;
    GdkGC *err_gc = gdk_gc_new(*pixmap);
    GdkGC *other_gc = gdk_gc_new(*pixmap);
    GdkColormap *colormap = gdk_colormap_get_system();
    struct gerb_net *net;
    gint x1, y1, x2, y2, i, j;
    int p1, p2;
    double x_scale, y_scale;
    int width = 0, height = 0;
    int cp_x = 0, cp_y = 0;
    
    
    if (image == NULL || image->netlist == NULL) 
	return 0;
    
    gdk_color_parse("red1", &err_color);
    gdk_colormap_alloc_color(colormap, &err_color, FALSE, TRUE);
    gdk_gc_set_foreground(err_gc, &err_color);
    
    gdk_color_parse("green", &other_color);
    gdk_colormap_alloc_color(colormap, &other_color, FALSE, TRUE);
    gdk_gc_set_foreground(other_gc, &other_color);
    
    if (polarity == NEGATIVE) 
	/* Black */
	gdk_gc_set_foreground(line_gc, bg_color);
    else
	/* Current line color */
	gdk_gc_set_foreground(line_gc, fg_color);
    
    for (net = image->netlist->next ; net != NULL; net = net->next) {
	if (image->aperture[net->aperture] == NULL) {
	    fprintf(stderr, "Aperture [%d] is not defined\n", net->aperture);
	    continue;
	}
	
	x1 = (int)ceil((image->info->offset_a + net->start_x) * scale);
	y1 = (int)ceil((image->info->offset_b + 2.5 - net->start_y) * scale);
	x2 = (int)ceil((image->info->offset_a + net->stop_x) * scale);
	y2 = (int)ceil((image->info->offset_b + 2.5 - net->stop_y)  * scale);

	x1 = x1 + trans_x;
	y1 = y1 + trans_y;
	x2 = x2 + trans_x;
	y2 = y2 + trans_y;

	if (net->cirseg) {
	    width = (int)ceil(net->cirseg->width * scale);
	    height = (int)ceil(net->cirseg->height * scale);
	    cp_x = (int)ceil((image->info->offset_a + net->cirseg->cp_x) * scale);
	    cp_y = (int)ceil((image->info->offset_b + 2.5 - net->cirseg->cp_y) * scale);
	    cp_x = cp_x + trans_x;
	    cp_y = cp_y + trans_y;
	}
	
	switch (net->aperture_state) {
	case ON :
	    p1 = (int)(image->aperture[net->aperture]->parameter[0] * scale);
	    gdk_gc_set_line_attributes(line_gc, p1, 
				       GDK_LINE_SOLID, 
				       GDK_CAP_ROUND, 
				       GDK_JOIN_MITER);
	    gdk_gc_set_line_attributes(err_gc, p1, 
				       GDK_LINE_SOLID, 
				       GDK_CAP_ROUND, 
				       GDK_JOIN_MITER);
	    gdk_gc_set_line_attributes(other_gc, p1, 
				       GDK_LINE_SOLID, 
				       GDK_CAP_ROUND, 
				       GDK_JOIN_MITER);
	    switch (net->interpolation) {
	    case LINEARx10 :
	    case LINEARx01 :
	    case LINEARx001 :
		fprintf(stderr, "Linear != x1\n");
		gdk_draw_line(*pixmap, err_gc, x1, y1, x2, y2);
		break;
	    case LINEARx1 :
		gdk_draw_line(*pixmap, line_gc, x1, y1, x2, y2);
		break;
		
	    case MQ_CW_CIRCULAR :
		fprintf(stderr, "Multi quadrant cw arc\n");
		/*gerbv_draw_arc(*pixmap, err_gc, 1, x1, y1, x2, y2, i, j);*/
		gdk_draw_line(*pixmap, err_gc, x1, y1, x2, y2);
		break;
	    case CW_CIRCULAR :
#ifdef ARC_DEBUG
		gerbv_draw_arc(*pixmap, other_gc, cp_x, cp_y, width, height, 
			       net->cirseg->angle1, net->cirseg->angle2);
#else
		gerbv_draw_arc(*pixmap, line_gc, cp_x, cp_y, width, height, 
			       net->cirseg->angle1, net->cirseg->angle2);
#endif
		break;
		
	    case MQ_CCW_CIRCULAR :
		fprintf(stderr, "Multi quadrant ccw arc\n");
		gdk_draw_line(*pixmap, err_gc, x1, y1, x2, y2);
		/*gerbv_draw_arc(*pixmap, err_gc, 0, x1, y1, x2, y2, i, j);*/
		break;
	    case CCW_CIRCULAR :
#ifdef ARC_DEBUG
		gerbv_draw_arc(*pixmap, err_gc, cp_x, cp_y, width, height, 
			       net->cirseg->angle1, net->cirseg->angle2);
#else
		gerbv_draw_arc(*pixmap, line_gc, cp_x, cp_y, width, height, 
			       net->cirseg->angle1, net->cirseg->angle2);
#endif
		break;
		
	    default :
		
	    }
	    break;
	case OFF :
	    break;
	case FLASH :
	    p1 = (int)(image->aperture[net->aperture]->parameter[0] * scale);
	    p2 = (int)(image->aperture[net->aperture]->parameter[1] * scale);
	    
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

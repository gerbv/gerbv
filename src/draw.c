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
 * Draws an oval _centered_ at x,y with x axel x_axel and y axel y_axel
 */ 
static void
gerbv_draw_oval(GdkPixmap *pixmap, GdkGC *gc, 
		gint filled, gint x, gint y, gint x_axel, gint y_axel)
{
    static const gint full_circle = 23360;
    gint real_x = x - x_axel / 2;
    gint real_y = y - y_axel / 2;
    
    gdk_draw_arc(pixmap, gc, filled, real_x, real_y, x_axel, y_axel, 0, 
		 full_circle);
    
    return;
} /* gerbv_draw_oval */


/*
 * Draws an arc 
 * direction:  0 counterclockwise, 1 clockwise
 */
static void
gerbv_draw_arc(GdkPixmap *pixmap, GdkGC *gc, int cw, 
	       gint start_x, gint start_y, 
	       gint stop_x, gint stop_y, 
	       gint dist_x, gint dist_y)
{
    int quadrant = 0;
    gint angle1 = 0;
    gint angle2 = 0;
    gint real_x = 0;
    gint real_y = 0;
    gint width  = 0;
    gint height = 0;
    gint arc_cp_x = 0;
    gint arc_cp_y = 0;
    gint d1x, d1y, d2x, d2y;
    double alfa, beta;
    
    /*
     * Quadrant detection (based on ccw, coverted below if cw)
     *    ---->X
     *    !
     *   \!/
     *  Y V
     */
    if (start_x > stop_x)
	/* 1st and 2nd quadrant */
	if (start_y > stop_y)
	    quadrant = 1;
	else
	    quadrant = 2;
    else
	/* 3rd and 4th quadrant */
	if (start_y < stop_y)
	    quadrant = 3;
	else
	    quadrant = 4;

    /* 
     * If clockwise, rotate quadrant
     */
    if (cw) {
	switch (quadrant) {
	case 1 : 
	    quadrant = 3;
	    break;
	case 2 : 
	    quadrant = 4;
	    break;
	case 3 : 
	    quadrant = 1;
	    break;
	case 4 : 
	    quadrant = 2;
	    break;
	default : 
	    err(1, "Unknow quadrant value while converting to cw\n");
	}
    }

    /*
     * Calculate arc center point
     */
    switch (quadrant) {
    case 1 :
	arc_cp_x = start_x - dist_x;
	arc_cp_y = start_y + dist_y;
	break;
    case 2 :
	arc_cp_x = start_x + dist_x;
	arc_cp_y = start_y + dist_y;
	break;
    case 3 : 
	arc_cp_x = start_x + dist_x;
	arc_cp_y = start_y - dist_y;
	break;
    case 4 :
	arc_cp_x = start_x - dist_x;
	arc_cp_y = start_y - dist_y;
	break;
    default :
	err(1, "Strange quadrant : %d\n", quadrant);
    }
    
    /*
     * Some good values 
     */
#define DIFF(a, b) ((a > b) ? a - b : b - a)
    d1x = DIFF(start_x, arc_cp_x);
    d1y = DIFF(start_y, arc_cp_y);
    d2x = DIFF(stop_x, arc_cp_x);
    d2y = DIFF(stop_y, arc_cp_y);
    
    alfa = atan2((double)d1y, (double)d1x);
    beta = atan2((double)d2y, (double)d2x);
    
    /*
     * Avoid divide by zero when sin(0) = 0 and cos(90) = 0
     */
    width  = alfa < beta ? 2 * (d1x / cos(alfa)) : 2 * (d2x / cos(beta));
    height = alfa > beta ? 2 * (d1y / sin(alfa)) : 2 * (d2y / sin(beta));
    if (alfa < 0.000001 && beta < 0.000001) height = 0;
    
    real_x = arc_cp_x - width / 2;
    real_y = arc_cp_y - height / 2;

#define RAD2DEG(a) (int)ceil(a * 180 / M_PI) 
    
    switch (quadrant) {
    case 1 :
	angle1 = RAD2DEG(alfa);
	angle2 = RAD2DEG(beta);
	break;
    case 2 :
	angle1 = 180 - RAD2DEG(alfa);
	angle2 = 180 - RAD2DEG(beta);
	break;
    case 3 : 
	angle1 = 180 + RAD2DEG(alfa);
	angle2 = 180 + RAD2DEG(beta);
	break;
    case 4 :
	angle1 = 360 - RAD2DEG(alfa);
	angle2 = 360 - RAD2DEG(beta);
	break;
    default :
	err(1, "Strange quadrant : %d\n", quadrant);
    }
    
    if (width < 0)
	fprintf(stderr, "Negative width [%d] in quadrant %d [%f][%f]\n", 
		width, quadrant, alfa, beta);
    
    if (height < 0)
	fprintf(stderr, "Negative height [%d] in quadrant %d [%d][%d]\n", 
		height, quadrant, RAD2DEG(alfa), RAD2DEG(beta));
    
    gdk_draw_arc(pixmap, gc, FALSE, real_x, real_y, 
		 width, height, 
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
	i  = (int)ceil(net->arc_start_x * scale);
	j  = (int)ceil(net->arc_start_y * scale);
	
	x1 = x1 + trans_x;
	y1 = y1 + trans_y;
	x2 = x2 + trans_x;
	y2 = y2 + trans_y;
	
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
		gerbv_draw_arc(*pixmap, other_gc, 1, x1, y1, x2, y2, i, j);
#else
		gerbv_draw_arc(*pixmap, line_gc, 1, x1, y1, x2, y2, i, j);
#endif
		break;
		
	    case MQ_CCW_CIRCULAR :
		fprintf(stderr, "Multi quadrant ccw arc\n");
		gdk_draw_line(*pixmap, err_gc, x1, y1, x2, y2);
		/*gerbv_draw_arc(*pixmap, err_gc, 0, x1, y1, x2, y2, i, j);*/
		break;
	    case CCW_CIRCULAR :
#ifdef ARC_DEBUG
		gerbv_draw_arc(*pixmap, err_gc, 0, x1, y1, x2, y2, i, j);
#else
		gerbv_draw_arc(*pixmap, line_gc, 0, x1, y1, x2, y2, i, j);
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

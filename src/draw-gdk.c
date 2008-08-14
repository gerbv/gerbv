/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2000-2003 Stefan Petersen (spe@stacken.kth.se)
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

/** \file draw-gdk.c
    \brief GDK rendering functions
    \ingroup libgerbv
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>  /* ceil(), atan2() */

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gtk/gtk.h>
#include "gerbv.h"
#include "draw-gdk.h"

#undef round
#define round(x) ceil((double)(x))

#define dprintf if(DEBUG) printf

/*
 * If you want to rotate a
 * column vector v by t degrees using matrix M, use
 *
 *   M = {{cos t, -sin t}, {sin t, cos t}} in M*v.
 *
 * From comp.graphics.algorithms Frequently Asked Questions
 *
 * Due reverse defintion of X-axis in GTK you have to negate
 * angels.
 *
 */
static GdkPoint 
rotate_point(GdkPoint point, int angle)
{
    double sint, cost;
    GdkPoint returned;
    
    if (angle == 0)
	return point;

    sint = sin(-(double)angle * M_PI / 180.0);
    cost = cos(-(double)angle * M_PI / 180.0);
    
    returned.x = (int)round(cost * (double)point.x - sint * (double)point.y);
    returned.y = (int)round(sint * (double)point.x + cost * (double)point.y);
    
    return returned;
}


/*
 * Aperture macro primitive 1 (Circle)
 */
static void
gerbv_gdk_draw_prim1(GdkPixmap *pixmap, GdkGC *gc, double *p, 
		     double scale, gint x, gint y)
{
    const int exposure_idx = 0;
    const int diameter_idx = 1;
    const int x_offset_idx = 2;
    const int y_offset_idx = 3;
    const gint full_circle = 23360;
    GdkGC *local_gc = gdk_gc_new(pixmap);
    gint dia    = round(fabs(p[diameter_idx] * scale));
    gint real_x = x - dia / 2;
    gint real_y = y - dia / 2;
    GdkColor color;

    gdk_gc_copy(local_gc, gc);

    real_x += (int)(p[x_offset_idx] * (double)scale);
    real_y -= (int)(p[y_offset_idx] * (double)scale);

    /* Exposure */
    if (p[exposure_idx] == 0.0) {
	color.pixel = 0;
	gdk_gc_set_foreground(local_gc, &color);
    }

    gdk_gc_set_line_attributes(local_gc, 
			       1, /* outline always 1 pixels */
			       GDK_LINE_SOLID, 
			       GDK_CAP_BUTT, 
			       GDK_JOIN_MITER);

    /* 
     * A filled circle 
     */
    gdk_draw_arc(pixmap, local_gc, 1, real_x, real_y, dia, dia, 
		 0, full_circle);

    gdk_gc_unref(local_gc);

    return;
} /* gerbv_gdk_draw_prim1 */


/*
 * Aperture macro primitive 4 (outline)
 * - Start point is not included in number of points.
 * - Outline is 1 pixel.
 */
static void
gerbv_gdk_draw_prim4(GdkPixmap *pixmap, GdkGC *gc, double *p, 
		     double scale, gint x, gint y)
{
    const int exposure_idx = 0;
    const int nuf_points_idx = 1;
    const int first_x_idx = 2;
    const int first_y_idx = 3;
    const int rotext_idx = 4;
    GdkGC *local_gc = gdk_gc_new(pixmap);
    int nuf_points, point;
    double rotation;
    GdkPoint *points;
    GdkColor color;

    /* Include start point */
    nuf_points = (int)p[nuf_points_idx] + 1;
    points = (GdkPoint *)g_malloc(sizeof(GdkPoint) * nuf_points);
    if (!points) {
	g_free(points);
	return;
    }

    rotation = p[(nuf_points - 1) * 2 + rotext_idx];
    for (point = 0; point < nuf_points; point++) {
	points[point].x = (int)round(scale * p[point * 2 + first_x_idx]);
	points[point].y = -(int)round(scale * p[point * 2 + first_y_idx]);
	if (rotation != 0.0)
	    points[point] = rotate_point(points[point], rotation);
	points[point].x += x;
	points[point].y += y;
    }

    gdk_gc_copy(local_gc, gc);

    /* Exposure */
    if (p[exposure_idx] == 0.0) {
	color.pixel = 0;
	gdk_gc_set_foreground(local_gc, &color);
    }

    gdk_gc_set_line_attributes(local_gc, 
			       1, /* outline always 1 pixels */
			       GDK_LINE_SOLID, 
			       GDK_CAP_BUTT, 
			       GDK_JOIN_MITER);
    gdk_draw_polygon(pixmap, local_gc, 1, points, nuf_points);

    g_free(points);

    gdk_gc_unref(local_gc);

    return;
} /* gerbv_gdk_draw_prim4 */


/*
 * Aperture macro primitive 5 (polygon)
 */
static void
gerbv_gdk_draw_prim5(GdkPixmap *pixmap, GdkGC *gc, double *p, 
		     double scale, gint x, gint y)
{
    const int exposure_idx = 0;
    const int nuf_vertices_idx = 1;
    const int center_x_idx = 2;
    const int center_y_idx = 3;
    const int diameter_idx = 4;
    const int rotation_idx = 5;
    int nuf_vertices, i;
    double vertex, tick, rotation, radius;
    GdkPoint *points;
    GdkGC *local_gc = gdk_gc_new(pixmap);
    GdkColor color;

    nuf_vertices = (int)p[nuf_vertices_idx];
    points = (GdkPoint *)g_malloc(sizeof(GdkPoint) * nuf_vertices);
    if (!points) {
	g_free(points);
	return;
    }

    gdk_gc_copy(local_gc, gc);

    /* Exposure */
    if (p[exposure_idx] == 0.0) {
	color.pixel = 0;
	gdk_gc_set_foreground(local_gc, &color);
    }

    tick = 2 * M_PI / (double)nuf_vertices;
    rotation = -p[rotation_idx] * M_PI / 180.0;
    radius = p[diameter_idx] / 2.0;
    for (i = 0; i < nuf_vertices; i++) {
	vertex =  tick * (double)i + rotation;
	points[i].x = (int)round(scale * radius * cos(vertex)) + x +
	    p[center_x_idx];
	points[i].y = (int)round(scale * radius * sin(vertex)) + y +
	    p[center_y_idx];
    }

    gdk_draw_polygon(pixmap, local_gc, 1, points, nuf_vertices);

    gdk_gc_unref(local_gc);

    g_free(points);
    return;
} /* gerbv_gdk_draw_prim5 */


/*
 * Doesn't handle and explicit x,y yet
 * Questions:
 *  - is "gap" distance between edges of circles or distance between
 *    center of line of circle?
 */
static void
gerbv_gdk_draw_prim6(GdkPixmap *pixmap, GdkGC *gc, double *p, 
		 double scale, gint x, gint y)
{
    const int outside_dia_idx = 2;
    const int ci_thickness_idx = 3;
    const int gap_idx = 4;
    const int nuf_circles_idx = 5;
    const int ch_thickness_idx = 6;
    const int ch_length_idx = 7;
    const int rotation_idx = 8;
    GdkGC *local_gc = gdk_gc_new(pixmap);
    double real_dia;
    double real_gap;
    int circle;
    GdkPoint crosshair[4];
    int point;

    gdk_gc_copy(local_gc, gc);
    gdk_gc_set_line_attributes(local_gc, 
			       (int)round(scale * p[ci_thickness_idx]),
			       GDK_LINE_SOLID, 
			       GDK_CAP_BUTT, 
			       GDK_JOIN_MITER);

    real_dia = p[outside_dia_idx] -  p[ci_thickness_idx] / 2.0;
    real_gap = p[gap_idx] + p[ci_thickness_idx];

    for (circle = 0; circle != (int)p[nuf_circles_idx];  circle++) {
	/* 
	 * Non filled circle 
	 */
	const gint full_circle = 23360;
	gint dia = (real_dia - real_gap * circle) * scale;
	gdk_draw_arc(pixmap, local_gc, 0, x - dia / 2, y - dia / 2, 
		     dia, dia, 0, full_circle);
			  
    }

    /*
     * Cross Hair 
     */
    memset(crosshair, 0, sizeof(GdkPoint) * 4);
    crosshair[0].x = (int)((p[ch_length_idx] / 2.0) * scale);
    /*crosshair[0].y = 0;*/
    crosshair[1].x = -crosshair[0].x;
    /*crosshair[1].y = 0;*/
    /*crosshair[2].x = 0;*/
    crosshair[2].y = crosshair[0].x;
    /*crosshair[3].x = 0;*/
    crosshair[3].y = -crosshair[0].x;

    gdk_gc_set_line_attributes(local_gc, 
			       (int)round(scale * p[ch_thickness_idx]),
			       GDK_LINE_SOLID, 
			       GDK_CAP_BUTT, 
			       GDK_JOIN_MITER);

    for (point = 0; point < 4; point++) {
	crosshair[point] = rotate_point(crosshair[point], 
					p[rotation_idx]);
	crosshair[point].x += x;
	crosshair[point].y += y;
    }
    gdk_draw_line(pixmap, local_gc, 
		  crosshair[0].x, crosshair[0].y, 
		  crosshair[1].x, crosshair[1].y);
    gdk_draw_line(pixmap, local_gc, 
		  crosshair[2].x, crosshair[2].y, 
		  crosshair[3].x, crosshair[3].y);

    gdk_gc_unref(local_gc);

    return;
} /* gerbv_gdk_draw_prim6 */


static void
gerbv_gdk_draw_prim7(GdkPixmap *pixmap, GdkGC *gc, double *p, 
		 double scale, gint x, gint y)
{
    const int outside_dia_idx = 2;
    const int inside_dia_idx = 3;
    const int ch_thickness_idx = 4;
    const int rotation_idx = 5;
    const gint full_circle = 23360;
    GdkGCValues gc_val;
    int diameter, i;
    GdkGC *local_gc = gdk_gc_new(pixmap);
    GdkPoint point[4];
    double ci_thickness = (p[outside_dia_idx] - 
			   p[inside_dia_idx]) / 2.0;

    gdk_gc_copy(local_gc, gc);
    gdk_gc_set_line_attributes(local_gc, 
			       (int)round(scale * ci_thickness),
			       GDK_LINE_SOLID, 
			       GDK_CAP_BUTT, 
			       GDK_JOIN_MITER);

    /* 
     * Non filled circle 
     */
    diameter = (p[inside_dia_idx] + ci_thickness) * scale;
    gdk_draw_arc(pixmap, local_gc, 0, x - diameter / 2, y - diameter / 2, 
		 diameter, diameter, 0, full_circle);

    /*
     * Cross hair
     */ 
    /* Calculate the end points of the crosshair */    
    /* GDK doesn't always remove all of the circle (round of error probably)
       I extend the crosshair line with 2 (one pixel in each end) to make 
       sure all of the circle is removed with the crosshair */
    for (i = 0; i < 4; i++) {
	point[i].x = round((p[outside_dia_idx] / 2.0) * scale) + 2;
	point[i].y = 0;
	point[i] = rotate_point(point[i], p[rotation_idx] + 90 * i);
	point[i].x += x;
	point[i].y += y;
    }

    gdk_gc_set_line_attributes(local_gc, 
			       (int)round(scale * p[ch_thickness_idx]),
			       GDK_LINE_SOLID, 
			       GDK_CAP_BUTT, 
			       GDK_JOIN_MITER);

    /* The cross hair should "cut out" parts of the circle, hence inverse */
    gdk_gc_get_values(local_gc, &gc_val);
    if (gc_val.foreground.pixel == 1)
	gc_val.foreground.pixel = 0;
    else
	gc_val.foreground.pixel = 1;
    gdk_gc_set_foreground(local_gc, &(gc_val.foreground));

    /* Draw the actual cross */
    gdk_draw_line(pixmap, local_gc, 
		  point[0].x, point[0].y, point[2].x, point[2].y);
    gdk_draw_line(pixmap, local_gc,
		  point[1].x, point[1].y, point[3].x, point[3].y);

    gdk_gc_unref(local_gc);

    return;
} /* gerbv_gdk_draw_prim7 */


/*
 * Doesn't handle and explicit x,y yet
 */
static void
gerbv_gdk_draw_prim20(GdkPixmap *pixmap, GdkGC *gc, double *p, 
		  double scale, gint x, gint y)
{
    const int exposure_idx = 0;
    const int linewidth_idx = 1;
    const int start_x_idx = 2;
    const int start_y_idx = 3;
    const int end_x_idx = 4;
    const int end_y_idx = 5;
    const int rotation_idx = 6;
    const int nuf_points = 2;
    GdkGC *local_gc = gdk_gc_new(pixmap);
    GdkPoint points[nuf_points];
    GdkColor color;
    int i;

    gdk_gc_copy(local_gc, gc);

    /* Exposure */
    if (p[exposure_idx] == 0.0) {
	color.pixel = 0;
	gdk_gc_set_foreground(local_gc, &color);
    }

    gdk_gc_set_line_attributes(local_gc, 
			       (int)round(scale * p[linewidth_idx]),
			       GDK_LINE_SOLID, 
			       GDK_CAP_BUTT, 
			       GDK_JOIN_MITER);

    points[0].x = (p[start_x_idx] * scale);
    points[0].y = (p[start_y_idx] * scale);
    points[1].x = (p[end_x_idx] * scale);
    points[1].y = (p[end_y_idx] * scale);

    for (i = 0; i < nuf_points; i++) {
	points[i] = rotate_point(points[i], -p[rotation_idx]);
	points[i].x = x + points[i].x;
	points[i].y = y - points[i].y;
    }

    gdk_draw_line(pixmap, local_gc, 
		  points[0].x, points[0].y, 
		  points[1].x, points[1].y);
    
    gdk_gc_unref(local_gc);

    return;
} /* gerbv_gdk_draw_prim20 */


static void
gerbv_gdk_draw_prim21(GdkPixmap *pixmap, GdkGC *gc, double *p, 
		  double scale, gint x, gint y)
{
    const int exposure_idx = 0;
    const int width_idx = 1;
    const int height_idx = 2;
    const int exp_x_idx = 3;
    const int exp_y_idx = 4;
    const int rotation_idx = 5;
    const int nuf_points = 4;
    GdkPoint points[nuf_points];
    GdkColor color;
    GdkGC *local_gc = gdk_gc_new(pixmap);
    int half_width, half_height;
    int i;

    half_width = (int)round(p[width_idx] * scale / 2.0);
    half_height =(int)round(p[height_idx] * scale / 2.0);

    points[0].x = half_width;
    points[0].y = half_height;

    points[1].x = half_width;
    points[1].y = -half_height;

    points[2].x = -half_width;
    points[2].y = -half_height;

    points[3].x = -half_width;
    points[3].y = half_height;

    for (i = 0; i < nuf_points; i++) {
	points[i] = rotate_point(points[i], p[rotation_idx]);
	points[i].x += (x + (int)(p[exp_x_idx] * scale));
	points[i].y += (y - (int)(p[exp_y_idx] * scale));
    }

    gdk_gc_copy(local_gc, gc);

    /* Exposure */
    if (p[exposure_idx] == 0.0) {
	color.pixel = 0;
	gdk_gc_set_foreground(local_gc, &color);
    }

    gdk_draw_polygon(pixmap, local_gc, 1, points, nuf_points);

    gdk_gc_unref(local_gc);

    return;
} /* gerbv_gdk_draw_prim21 */


/*
 * Doesn't handle explicit x,y yet
 */
static void
gerbv_gdk_draw_prim22(GdkPixmap *pixmap, GdkGC *gc, double *p, 
		  double scale, gint x, gint y)
{
    const int exposure_idx = 0;
    const int width_idx = 1;
    const int height_idx = 2;
    const int x_lower_left_idx = 3;
    const int y_lower_left_idx = 4;
    const int rotation_idx = 5;
    const int nuf_points = 4;
    GdkPoint points[nuf_points];
    GdkGC *local_gc = gdk_gc_new(pixmap);
    GdkColor color;
    int i;

    points[0].x = (int)round(p[x_lower_left_idx] * scale);
    points[0].y = (int)round(p[y_lower_left_idx] * scale);

    points[1].x = (int)round((p[x_lower_left_idx] + p[width_idx])
			     * scale);
    points[1].y = (int)round(p[y_lower_left_idx] * scale);

    points[2].x = (int)round((p[x_lower_left_idx]  + p[width_idx])
			     * scale);
    points[2].y = (int)round((p[y_lower_left_idx]  - p[height_idx])
			     * scale);

    points[3].x = (int)round(p[x_lower_left_idx] * scale);
    points[3].y = (int)round((p[y_lower_left_idx] - p[height_idx])
			     * scale);

    for (i = 0; i < nuf_points; i++) {
	points[i] = rotate_point(points[i], p[rotation_idx]);
	points[i].x += x;
	points[i].y += y;
    }
    
    gdk_gc_copy(local_gc, gc);

    /* Exposure */
    if (p[exposure_idx] == 0.0) {
	color.pixel = 0;
	gdk_gc_set_foreground(local_gc, &color);
    }

    gdk_draw_polygon(pixmap, local_gc, 1, points, nuf_points);

    gdk_gc_unref(local_gc);

    return;
} /* gerbv_gdk_draw_prim22 */


static void 
gerbv_gdk_draw_amacro(GdkPixmap *pixmap, GdkGC *gc, 
		      gerbv_simplified_amacro_t *s, double scale, 
		      gint x, gint y)
{
    gerbv_simplified_amacro_t *ls = s;

    dprintf("Drawing simplified aperture macros:\n");
    while (ls != NULL) {

	switch (ls->type) {
	case GERBV_APTYPE_MACRO_CIRCLE:
	    gerbv_gdk_draw_prim1(pixmap, gc, ls->parameter, scale, x, y);
	    dprintf("  Circle\n");
	    break;
	case GERBV_APTYPE_MACRO_OUTLINE:
	    gerbv_gdk_draw_prim4(pixmap, gc, ls->parameter, scale, x, y);
	    dprintf("  Outline\n");
	    break;
	case GERBV_APTYPE_MACRO_POLYGON:
	    gerbv_gdk_draw_prim5(pixmap, gc, ls->parameter, scale, x, y);
	    dprintf("  Polygon\n");
	    break;
	case GERBV_APTYPE_MACRO_MOIRE:
	    gerbv_gdk_draw_prim6(pixmap, gc, ls->parameter, scale, x, y);
	    dprintf("  Moiré\n");
	    break;
	case GERBV_APTYPE_MACRO_THERMAL:
	    gerbv_gdk_draw_prim7(pixmap, gc, ls->parameter, scale, x, y);
	    dprintf("  Thermal\n");
	    break;
	case GERBV_APTYPE_MACRO_LINE20:
	    gerbv_gdk_draw_prim20(pixmap, gc, ls->parameter, scale, x, y);
	    dprintf("  Line 20\n");
	    break;
	case GERBV_APTYPE_MACRO_LINE21:
	    gerbv_gdk_draw_prim21(pixmap, gc, ls->parameter, scale, x, y);
	    dprintf("  Line 21\n");
	    break;
	case GERBV_APTYPE_MACRO_LINE22:
	    gerbv_gdk_draw_prim22(pixmap, gc, ls->parameter, scale, x, y);
	    dprintf("  Line 22\n");
	    break;
	default:
	    GERB_FATAL_ERROR("Unknown simplified aperture macro");
	}

	ls = ls->next;
    }

} /* gerbv_gdk_draw_amacro */


/*
 * Draws a circle _centered_ at x,y with diameter dia
 */
static void 
gerbv_gdk_draw_circle(GdkPixmap *pixmap, GdkGC *gc, 
		  gint filled, gint x, gint y, gint dia)
{
    static const gint full_circle = 23360;
    gint real_x = x - dia / 2;
    gint real_y = y - dia / 2;
    
    gdk_draw_arc(pixmap, gc, filled, real_x, real_y, dia, dia, 0, full_circle);
    
    return;
} /* gerbv_gdk_draw_circle */


/*
 * Draws a rectangle _centered_ at x,y with sides x_side, y_side
 */
static void 
gerbv_gdk_draw_rectangle(GdkPixmap *pixmap, GdkGC *gc, 
		     gint filled, gint x, gint y, gint x_side, gint y_side)
{
    
    gint real_x = x - x_side / 2;
    gint real_y = y - y_side / 2;
    
    gdk_draw_rectangle(pixmap, gc, filled, real_x, real_y, x_side, y_side);
    
    return;
} /* gerbv_gdk_draw_rectangle */


/*
 * Draws an oval _centered_ at x,y with x axis x_axis and y axis y_axis
 */ 
static void
gerbv_gdk_draw_oval(GdkPixmap *pixmap, GdkGC *gc, 
		gint filled, gint x, gint y, gint x_axis, gint y_axis)
{
    gint delta = 0;
    GdkGC *local_gc = gdk_gc_new(pixmap);

    gdk_gc_copy(local_gc, gc);

    if (x_axis > y_axis) {
	/* Draw in x axis */
	delta = x_axis / 2 - y_axis / 2;
	gdk_gc_set_line_attributes(local_gc, y_axis, 
				   GDK_LINE_SOLID, 
				   GDK_CAP_ROUND, 
				   GDK_JOIN_MITER);
	gdk_draw_line(pixmap, local_gc, x - delta, y, x + delta, y);
    } else {
	/* Draw in y axis */
	delta = y_axis / 2 - x_axis / 2;
	gdk_gc_set_line_attributes(local_gc, x_axis, 
				   GDK_LINE_SOLID, 
				   GDK_CAP_ROUND, 
				   GDK_JOIN_MITER);
	gdk_draw_line(pixmap, local_gc, x, y - delta, x, y + delta);
    }

    gdk_gc_unref(local_gc);

    return;
} /* gerbv_gdk_draw_oval */


/*
 * Draws an arc 
 * Draws an arc _centered_ at x,y
 * direction:  0 counterclockwise, 1 clockwise
 */
static void
gerbv_gdk_draw_arc(GdkPixmap *pixmap, GdkGC *gc,
	       int x, int y,
	       int width, int height,
	       double angle1, double angle2)
{
    gint real_x = x - width / 2;
    gint real_y = y - height / 2;

    gdk_draw_arc(pixmap, gc, FALSE, real_x, real_y, width, height, 
		 (gint)(angle1 * 64.0), (gint)(angle2 - angle1) * 64.0);
    
    return;
} /* gerbv_gdk_draw_arc */


/*
 * Convert a gerber image to a GDK clip mask to be used when creating pixmap
 */
int
draw_gdk_image_to_pixmap(GdkPixmap **pixmap, gerbv_image_t *image, 
	     double scale, double trans_x, double trans_y,
	     gerbv_polarity_t polarity, gchar drawMode, gerbv_selection_info_t *selectionInfo)
{
    GdkGC *gc = gdk_gc_new(*pixmap);
    GdkGC *pgc = gdk_gc_new(*pixmap);
    GdkGCValues gc_values;
    struct gerbv_net *net, *polygonStartNet=NULL;
    gint x1, y1, x2, y2;
    int p1, p2, p3;
    int cir_width = 0, cir_height = 0;
    int cp_x = 0, cp_y = 0;
    GdkPoint *points = NULL;
    int curr_point_idx = 0;
    int in_parea_fill = 0;
    double unit_scale;
    GdkColor transparent, opaque;
    int steps,i,pointArraySize=0;
    double angleDiff;

    if (image == NULL || image->netlist == NULL) {
	/*
	 * Destroy GCs before exiting
	 */
	gdk_gc_unref(gc);
	gdk_gc_unref(pgc);
	
	return 0;
    }
    
    /* Set up the two "colors" we have */
    opaque.pixel = 0; /* opaque will not let color through */
    transparent.pixel = 1; /* transparent will let color through */ 

    /*
     * Clear clipmask and set draw color depending image on image polarity
     */
    if (polarity == GERBV_POLARITY_NEGATIVE) {
	gdk_gc_set_foreground(gc, &transparent);
	gdk_draw_rectangle(*pixmap, gc, TRUE, 0, 0, -1, -1);
	gdk_gc_set_foreground(gc, &opaque);
    } else {
	gdk_gc_set_foreground(gc, &opaque);
	gdk_draw_rectangle(*pixmap, gc, TRUE, 0, 0, -1, -1);
	gdk_gc_set_foreground(gc, &transparent);
    }

    for (net = image->netlist->next ; net != NULL; net = net->next) {
      int repeat_X=1, repeat_Y=1;
      double repeat_dist_X=0.0, repeat_dist_Y=0.0;
      int repeat_i, repeat_j;

	/*
	 * If step_and_repeat (%SR%) used, repeat the drawing;
	 */
	repeat_X = net->layer->stepAndRepeat.X;
	repeat_Y = net->layer->stepAndRepeat.Y;
	repeat_dist_X = net->layer->stepAndRepeat.dist_X;
	repeat_dist_Y = net->layer->stepAndRepeat.dist_Y;
	
	if (drawMode == DRAW_SELECTIONS) {
		/* this flag makes sure we don't draw any unintentional polygons...
		   if we've successfully entered a polygon (the first net matches, and
		   we don't want to check the nets inside the polygon) then
		   polygonStartNet will be set */
		if (!polygonStartNet) {
			int i;
			gboolean foundNet = FALSE;
			
			for (i=0; i<selectionInfo->selectedNodeArray->len; i++){
				gerbv_selection_item_t sItem = g_array_index (selectionInfo->selectedNodeArray,
					gerbv_selection_item_t, i);
				if (sItem.net == net)
					foundNet = TRUE;
			}
			if (!foundNet)
				continue;
		}
	}
	
      for(repeat_i = 0; repeat_i < repeat_X; repeat_i++) {
	for(repeat_j = 0; repeat_j < repeat_Y; repeat_j++) {
	  double sr_x = repeat_i * repeat_dist_X;
	  double sr_y = repeat_j * repeat_dist_Y;
	
      unit_scale = scale;

	/*
	 * Scale points with window scaling and translate them
	 */
	x1 = (int)round((image->info->offsetA + net->start_x + sr_x) * unit_scale +
			trans_x);
	y1 = (int)round((-image->info->offsetB - net->start_y - sr_y) * unit_scale +
			trans_y);
	x2 = (int)round((image->info->offsetA + net->stop_x + sr_x) * unit_scale +
			trans_x);
	y2 = (int)round((-image->info->offsetB - net->stop_y - sr_y) * unit_scale +
			trans_y);

	/* 
	 * If circle segment, scale and translate that one too
	 */
	if (net->cirseg) {
	    cir_width = (int)round(net->cirseg->width * unit_scale);
	    cir_height = (int)round(net->cirseg->height * unit_scale);
	    cp_x = (int)round((image->info->offsetA + net->cirseg->cp_x) *
			      unit_scale + trans_x);
	    cp_y = (int)round((image->info->offsetB - net->cirseg->cp_y) *
			      unit_scale + trans_y);
	}

	/*
	 * Set GdkFunction depending on if this (gerber) layer is inverted
	 * and allow for the photoplot being negative.
	 */
	gdk_gc_set_function(gc, GDK_COPY);
	if ((net->layer->polarity == GERBV_POLARITY_CLEAR) != (polarity == GERBV_POLARITY_NEGATIVE))
	    gdk_gc_set_foreground(gc, &opaque);
	else
	    gdk_gc_set_foreground(gc, &transparent);

	/*
	 * Polygon Area Fill routines
	 */
	switch (net->interpolation) {
	case GERBV_INTERPOLATION_PAREA_START :
	    points = NULL;
	    /* save the first net in the polygon as the "ID" net pointer
	       in case we are saving this net to the selection array */
	    polygonStartNet = net;
	    curr_point_idx = 0;
	    pointArraySize = 0;
	    in_parea_fill = 1;
	    continue;
	case GERBV_INTERPOLATION_PAREA_END :
	    gdk_gc_copy(pgc, gc); 
	    gdk_gc_set_line_attributes(pgc, 1, 
				       GDK_LINE_SOLID, 
				       GDK_CAP_PROJECTING, 
				       GDK_JOIN_MITER);
	    gdk_draw_polygon(*pixmap, pgc, 1, points, curr_point_idx);
	    g_free(points);
	    points = NULL;
	    in_parea_fill = 0;
	    polygonStartNet = NULL;
	    continue;
	/* make sure we completely skip over any deleted nodes */
	case GERBV_INTERPOLATION_DELETED:
	    continue;
	default :
	    break;
	}

	if (in_parea_fill) {
	    switch (net->interpolation) {
	    case GERBV_INTERPOLATION_x10 :
	    case GERBV_INTERPOLATION_LINEARx01 :
	    case GERBV_INTERPOLATION_LINEARx001 :
	    case GERBV_INTERPOLATION_LINEARx1 :
		if (pointArraySize < (curr_point_idx + 1)) {
		    points = (GdkPoint *)g_realloc(points,sizeof(GdkPoint) *  (curr_point_idx + 1));
		    pointArraySize = (curr_point_idx + 1);
		}
		points[curr_point_idx].x = x2;
		points[curr_point_idx].y = y2;
		curr_point_idx++;
		break;
	    case GERBV_INTERPOLATION_CW_CIRCULAR :
	    case GERBV_INTERPOLATION_CCW :
		/* we need to chop up the arc into small lines for rendering
		   with GDK */
		angleDiff = net->cirseg->angle2 - net->cirseg->angle1;
		steps = (int) abs(angleDiff);
		if (pointArraySize < (curr_point_idx + steps)) {
		    points = (GdkPoint *)g_realloc(points,sizeof(GdkPoint) *  (curr_point_idx + steps));
		    pointArraySize = (curr_point_idx + steps);
		}
		for (i=0; i<steps; i++){
		    points[curr_point_idx].x = cp_x + cir_width / 2.0 * cos ((net->cirseg->angle1 +
									      (angleDiff * i) / steps)*M_PI/180);
		    points[curr_point_idx].y = cp_y - cir_width / 2.0 * sin ((net->cirseg->angle1 +
									      (angleDiff * i) / steps)*M_PI/180);
		    curr_point_idx++;
		}
		break;
	    default:
		break;
	    }
	    continue;
	}

	/*
	 * If aperture state is off we allow use of undefined apertures.
	 * This happens when gerber files starts, but hasn't decided on 
	 * which aperture to use.
	 */
	if (image->aperture[net->aperture] == NULL) {
	  /* Commenting this out since it gets emitted every time you click on the screen 
	     if (net->aperture_state != GERBV_APERTURE_STATE_OFF)
	     GERB_MESSAGE("Aperture D%d is not defined\n", net->aperture);
	  */
	    continue;
	}
	
	switch (net->aperture_state) {
	case GERBV_APERTURE_STATE_ON :
	    p1 = (int)round(image->aperture[net->aperture]->parameter[0] * unit_scale);
	    if (image->aperture[net->aperture]->type == GERBV_APTYPE_RECTANGLE)
		gdk_gc_set_line_attributes(gc, p1, 
					   GDK_LINE_SOLID, 
					   GDK_CAP_PROJECTING, 
					   GDK_JOIN_MITER);
	    else
		gdk_gc_set_line_attributes(gc, p1, 
					   GDK_LINE_SOLID, 
					   GDK_CAP_ROUND, 
					   GDK_JOIN_MITER);
	    
	    switch (net->interpolation) {
	    case GERBV_INTERPOLATION_x10 :
	    case GERBV_INTERPOLATION_LINEARx01 :
	    case GERBV_INTERPOLATION_LINEARx001 :
		GERB_MESSAGE("Linear != x1\n");
		gdk_gc_set_line_attributes(gc, p1, 
					   GDK_LINE_ON_OFF_DASH, 
					   GDK_CAP_ROUND, 
					   GDK_JOIN_MITER);
		gdk_draw_line(*pixmap, gc, x1, y1, x2, y2);
		gdk_gc_set_line_attributes(gc, p1, 
					   GDK_LINE_SOLID,
					   GDK_CAP_ROUND, 
					   GDK_JOIN_MITER);
		break;
	    case GERBV_INTERPOLATION_LINEARx1 :
		if (image->aperture[net->aperture]->type != GERBV_APTYPE_RECTANGLE)
		    gdk_draw_line(*pixmap, gc, x1, y1, x2, y2);
		else {
		    gint dx, dy;
		    GdkPoint poly[6];
		    
		    dx = (int)round(image->aperture[net->aperture]->parameter[0]
				    * unit_scale / 2);
		    dy = (int)round(image->aperture[net->aperture]->parameter[1]
				    * unit_scale / 2);
		    if(x1 > x2) dx = -dx;
		    if(y1 > y2) dy = -dy;
		    poly[0].x = x1 - dx; poly[0].y = y1 - dy;
		    poly[1].x = x1 - dx; poly[1].y = y1 + dy;
		    poly[2].x = x2 - dx; poly[2].y = y2 + dy;
		    poly[3].x = x2 + dx; poly[3].y = y2 + dy;
		    poly[4].x = x2 + dx; poly[4].y = y2 - dy;
		    poly[5].x = x1 + dx; poly[5].y = y1 - dy;
		    gdk_draw_polygon(*pixmap, gc, 1, poly, 6);
		}
 		break;
	    case GERBV_INTERPOLATION_CW_CIRCULAR :
	    case GERBV_INTERPOLATION_CCW :
		gerbv_gdk_draw_arc(*pixmap, gc, cp_x, cp_y, cir_width, cir_height, 
				   net->cirseg->angle1, net->cirseg->angle2);
		break;
	    default :
		break;
	    }
	    break;
	case GERBV_APERTURE_STATE_OFF :
	    break;
	case GERBV_APERTURE_STATE_FLASH :
	    p1 = (int)round(image->aperture[net->aperture]->parameter[0] * unit_scale);
	    p2 = (int)round(image->aperture[net->aperture]->parameter[1] * unit_scale);
	    p3 = (int)round(image->aperture[net->aperture]->parameter[2] * unit_scale);
	    
	    switch (image->aperture[net->aperture]->type) {
	    case GERBV_APTYPE_CIRCLE :
		gerbv_gdk_draw_circle(*pixmap, gc, TRUE, x2, y2, p1);
		/*
		 * If circle has an inner diameter we must remove
		 * that part of the circle to make a hole in it.
		 * We should actually support square holes too,
		 * but due to laziness I don't.
		 */
		if (p2) {
		    //if (p3) GERB_COMPILE_WARNING("Should be a square hole in this aperture.\n");
		    gdk_gc_get_values(gc, &gc_values);
		    if (gc_values.foreground.pixel == opaque.pixel) {
			gdk_gc_set_foreground(gc, &transparent);
			gerbv_gdk_draw_circle(*pixmap, gc, TRUE, x2, y2, p2);
			gdk_gc_set_foreground(gc, &opaque);
		    } else {
			gdk_gc_set_foreground(gc, &opaque);
			gerbv_gdk_draw_circle(*pixmap, gc, TRUE, x2, y2, p2);
			gdk_gc_set_foreground(gc, &transparent);
		    }
		}

		break;
	    case GERBV_APTYPE_RECTANGLE:
		gerbv_gdk_draw_rectangle(*pixmap, gc, TRUE, x2, y2, p1, p2);
		break;
	    case GERBV_APTYPE_OVAL :
		gerbv_gdk_draw_oval(*pixmap, gc, TRUE, x2, y2, p1, p2);
		break;
	    case GERBV_APTYPE_POLYGON :
		//GERB_COMPILE_WARNING("Very bad at drawing polygons.\n");
		gerbv_gdk_draw_circle(*pixmap, gc, TRUE, x2, y2, p1);
		break;
	    case GERBV_APTYPE_MACRO :
		gerbv_gdk_draw_amacro(*pixmap, gc, 
				      image->aperture[net->aperture]->simplified,
				      unit_scale, x2, y2);
		break;
	    default :
		GERB_MESSAGE("Unknown aperture type\n");
		return 0;
	    }
	    break;
	default :
	    GERB_MESSAGE("Unknown aperture state\n");
	    return 0;
	}
    }
      }
    }
    /*
     * Destroy GCs before exiting
     */
    gdk_gc_unref(gc);
    gdk_gc_unref(pgc);
    
    return 1;

} /* image2pixmap */



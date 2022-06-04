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
#include "common.h"

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
rotate_point(GdkPoint point, double angle)
{
    double sint, cost;
    GdkPoint returned;
    
    if (angle == 0.0)
	return point;

    sint = sin(DEG2RAD(-angle));
    cost = cos(DEG2RAD(-angle));
    
    returned.x = lround(cost*point.x - sint*point.y);
    returned.y = lround(sint*point.x + cost*point.y);
    
    return returned;
}


/*
 * Aperture macro primitive 1 (Circle)
 */
static void
gerbv_gdk_draw_prim1(GdkPixmap *pixmap, GdkGC *gc, gerbv_simplified_amacro_t *s, 
		     double scale, gint x, gint y)
{
    const int exposure_idx = 0;
    const int diameter_idx = 1;
    const int x_offset_idx = 2;
    const int y_offset_idx = 3;
    const gint full_circle = 23360;
    GdkGC *local_gc = gdk_gc_new(pixmap);
    gint dia    = round(fabs(s->parameter[diameter_idx] * scale));
    gint real_x = x - dia / 2;
    gint real_y = y - dia / 2;
    GdkColor color;

    gdk_gc_copy(local_gc, gc);

    real_x += (int)(s->parameter[x_offset_idx] * (double)scale);
    real_y -= (int)(s->parameter[y_offset_idx] * (double)scale);

    /* Exposure */
    if (s->parameter[exposure_idx] == 0.0) {
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
gerbv_gdk_draw_prim4(GdkPixmap *pixmap, GdkGC *gc, gerbv_simplified_amacro_t *s, 
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
    nuf_points = (int)s->parameter[nuf_points_idx] + 1;
    points = g_new(GdkPoint, nuf_points);
    if (!points) {
	g_free(points);
	return;
    }

    rotation = s->parameter[(nuf_points - 1) * 2 + rotext_idx];
    for (point = 0; point < nuf_points; point++) {
	points[point].x = (int)round(scale * s->parameter[point * 2 + first_x_idx]);
	points[point].y = -(int)round(scale * s->parameter[point * 2 + first_y_idx]);
	if (rotation != 0.0)
	    points[point] = rotate_point(points[point], rotation);
	points[point].x += x;
	points[point].y += y;
    }

    gdk_gc_copy(local_gc, gc);

    /* Exposure */
    if (s->parameter[exposure_idx] == 0.0) {
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
gerbv_gdk_draw_prim5(GdkPixmap *pixmap, GdkGC *gc, gerbv_simplified_amacro_t *s, 
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

    nuf_vertices = (int)s->parameter[nuf_vertices_idx];
    points = g_new(GdkPoint, nuf_vertices);
    if (!points) {
	g_free(points);
	return;
    }

    gdk_gc_copy(local_gc, gc);

    /* Exposure */
    if (s->parameter[exposure_idx] == 0.0) {
	color.pixel = 0;
	gdk_gc_set_foreground(local_gc, &color);
    }

    tick = 2 * M_PI / (double)nuf_vertices;
    rotation = DEG2RAD(-s->parameter[rotation_idx]);
    radius = s->parameter[diameter_idx] / 2.0;
    for (i = 0; i < nuf_vertices; i++) {
	vertex =  tick * (double)i + rotation;
	points[i].x = (int)round(scale * (radius * cos(vertex) + s->parameter[center_x_idx])) + x;
	points[i].y = (int)round(scale * (radius * sin(vertex) - s->parameter[center_y_idx])) + y;
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
gerbv_gdk_draw_prim6(GdkPixmap *pixmap, GdkGC *gc, gerbv_simplified_amacro_t *s, 
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
    double real_dia_diff;
    int circle;
    GdkPoint crosshair[4];
    int point;

    gdk_gc_copy(local_gc, gc);
    gdk_gc_set_line_attributes(local_gc, 
			       (int)round(scale * s->parameter[ci_thickness_idx]),
			       GDK_LINE_SOLID, 
			       GDK_CAP_BUTT, 
			       GDK_JOIN_MITER);

    real_dia = s->parameter[outside_dia_idx] -  s->parameter[ci_thickness_idx] / 2.0;
    real_dia_diff = 2*(s->parameter[gap_idx] + s->parameter[ci_thickness_idx]);

    for (circle = 0; circle != (int)s->parameter[nuf_circles_idx];  circle++) {
	/* 
	 * Non filled circle 
	 */
	const gint full_circle = 23360;
	gint dia = (real_dia - real_dia_diff * circle) * scale;
	if (dia >= 0){
		gdk_draw_arc(pixmap, local_gc, 0, x - dia / 2, y - dia / 2, 
				dia, dia, 0, full_circle);
	}
    }

    /*
     * Cross Hair 
     */
    memset(crosshair, 0, sizeof(GdkPoint) * 4);
    crosshair[0].x = (int)((s->parameter[ch_length_idx] / 2.0) * scale);
    /*crosshair[0].y = 0;*/
    crosshair[1].x = -crosshair[0].x;
    /*crosshair[1].y = 0;*/
    /*crosshair[2].x = 0;*/
    crosshair[2].y = crosshair[0].x;
    /*crosshair[3].x = 0;*/
    crosshair[3].y = -crosshair[0].x;

    gdk_gc_set_line_attributes(local_gc, 
			       (int)round(scale * s->parameter[ch_thickness_idx]),
			       GDK_LINE_SOLID, 
			       GDK_CAP_BUTT, 
			       GDK_JOIN_MITER);

    for (point = 0; point < 4; point++) {
	crosshair[point] = rotate_point(crosshair[point], 
					s->parameter[rotation_idx]);
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
gerbv_gdk_draw_prim7(GdkPixmap *pixmap, GdkGC *gc, gerbv_simplified_amacro_t *s, 
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
    double ci_thickness = (s->parameter[outside_dia_idx] - 
			   s->parameter[inside_dia_idx]) / 2.0;

    gdk_gc_copy(local_gc, gc);
    gdk_gc_set_line_attributes(local_gc, 
			       (int)round(scale * ci_thickness),
			       GDK_LINE_SOLID, 
			       GDK_CAP_BUTT, 
			       GDK_JOIN_MITER);

    /* 
     * Non filled circle 
     */
    diameter = (s->parameter[inside_dia_idx] + ci_thickness) * scale;
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
	point[i].x = round((s->parameter[outside_dia_idx] / 2.0) * scale) + 2;
	point[i].y = 0;
	point[i] = rotate_point(point[i], s->parameter[rotation_idx] + 90 * i);
	point[i].x += x;
	point[i].y += y;
    }

    gdk_gc_set_line_attributes(local_gc, 
			       (int)round(scale * s->parameter[ch_thickness_idx]),
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
gerbv_gdk_draw_prim20(GdkPixmap *pixmap, GdkGC *gc, gerbv_simplified_amacro_t *s, 
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
    if (s->parameter[exposure_idx] == 0.0) {
	color.pixel = 0;
	gdk_gc_set_foreground(local_gc, &color);
    }

    gdk_gc_set_line_attributes(local_gc, 
			       (int)round(scale * s->parameter[linewidth_idx]),
			       GDK_LINE_SOLID, 
			       GDK_CAP_BUTT, 
			       GDK_JOIN_MITER);

    points[0].x = (s->parameter[start_x_idx] * scale);
    points[0].y = (s->parameter[start_y_idx] * scale);
    points[1].x = (s->parameter[end_x_idx] * scale);
    points[1].y = (s->parameter[end_y_idx] * scale);

    for (i = 0; i < nuf_points; i++) {
	points[i] = rotate_point(points[i], -s->parameter[rotation_idx]);
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
gerbv_gdk_draw_prim21(GdkPixmap *pixmap, GdkGC *gc, gerbv_simplified_amacro_t *s, 
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

    half_width = (int)round(s->parameter[width_idx] * scale / 2.0);
    half_height =(int)round(s->parameter[height_idx] * scale / 2.0);

    points[0].x = half_width;
    points[0].y = half_height;

    points[1].x = half_width;
    points[1].y = -half_height;

    points[2].x = -half_width;
    points[2].y = -half_height;

    points[3].x = -half_width;
    points[3].y = half_height;

    for (i = 0; i < nuf_points; i++) {
	points[i].x += (int)(s->parameter[exp_x_idx] * scale);
	points[i].y -= (int)(s->parameter[exp_y_idx] * scale);
	points[i] = rotate_point(points[i], s->parameter[rotation_idx]);
	points[i].x += x;
	points[i].y += y;
    }

    gdk_gc_copy(local_gc, gc);

    /* Exposure */
    if (s->parameter[exposure_idx] == 0.0) {
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
gerbv_gdk_draw_prim22(GdkPixmap *pixmap, GdkGC *gc, gerbv_simplified_amacro_t *s, 
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

    points[0].x = (int)round(s->parameter[x_lower_left_idx] * scale);
    points[0].y = (int)round(s->parameter[y_lower_left_idx] * scale);

    points[1].x = (int)round((s->parameter[x_lower_left_idx] + s->parameter[width_idx])
			     * scale);
    points[1].y = (int)round(s->parameter[y_lower_left_idx] * scale);

    points[2].x = (int)round((s->parameter[x_lower_left_idx]  + s->parameter[width_idx])
			     * scale);
    points[2].y = (int)round((s->parameter[y_lower_left_idx]  + s->parameter[height_idx])
			     * scale);

    points[3].x = (int)round(s->parameter[x_lower_left_idx] * scale);
    points[3].y = (int)round((s->parameter[y_lower_left_idx] + s->parameter[height_idx])
			     * scale);

    for (i = 0; i < nuf_points; i++) {
	points[i] = rotate_point(points[i], -s->parameter[rotation_idx]);
	points[i].x = x + points[i].x;
	points[i].y = y - points[i].y;
    }

    gdk_gc_copy(local_gc, gc);

    /* Exposure */
    if (s->parameter[exposure_idx] == 0.0) {
	color.pixel = 0;
	gdk_gc_set_foreground(local_gc, &color);
    }

    gdk_draw_polygon(pixmap, local_gc, 1, points, nuf_points);

    gdk_gc_unref(local_gc);

    return;
} /* gerbv_gdk_draw_prim22 */

/* Keep this array in order and without holes */
static void (*dgk_draw_amacro_funcs[])(GdkPixmap *, GdkGC *, gerbv_simplified_amacro_t *, 
		     double, gint, gint) = {
	[GERBV_APTYPE_MACRO_CIRCLE]  = &gerbv_gdk_draw_prim1,
	[GERBV_APTYPE_MACRO_OUTLINE] = &gerbv_gdk_draw_prim4,
	[GERBV_APTYPE_MACRO_POLYGON] = &gerbv_gdk_draw_prim5,
	[GERBV_APTYPE_MACRO_MOIRE]   = &gerbv_gdk_draw_prim6,
	[GERBV_APTYPE_MACRO_THERMAL] = &gerbv_gdk_draw_prim7,
	[GERBV_APTYPE_MACRO_LINE20]  = &gerbv_gdk_draw_prim20,
	[GERBV_APTYPE_MACRO_LINE21]  = &gerbv_gdk_draw_prim21,
	[GERBV_APTYPE_MACRO_LINE22]  = &gerbv_gdk_draw_prim22,
};

static inline void 
gerbv_gdk_draw_amacro(GdkPixmap *pixmap, GdkGC *gc, 
		      gerbv_simplified_amacro_t *s, double scale, 
		      gint x, gint y)
{
	dprintf("%s(): drawing simplified aperture macros:\n", __func__);

	while (s != NULL) {
		if (s->type >= GERBV_APTYPE_MACRO_CIRCLE
		 && s->type <= GERBV_APTYPE_MACRO_LINE22) {
			dgk_draw_amacro_funcs[s->type](pixmap, gc,
					s, scale, x, y);
			dprintf("  %s\n", gerbv_aperture_type_name(s->type));
		} else {
			GERB_FATAL_ERROR(
				_("Unknown simplified aperture macro type %d"),
				s->type);
		}

		s = s->next;
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
		int filled, gint x, gint y, gint x_side, gint y_side,
		double angle_deg)
{
	int i;
	GdkPoint points[4];

	points[0].x = -(x_side >> 1);
	points[0].y = -(y_side >> 1);
	points[1].x = x_side >> 1;
	points[1].y = points[0].y;
	points[2].x = points[1].x;
	points[2].y = y_side >> 1;
	points[3].x = points[0].x;
	points[3].y = points[2].y;

	for (i = 0; i < 4; i++) {
		points[i] = rotate_point(points[i], angle_deg);
		points[i].x += x;
		points[i].y += y;
	}

	gdk_draw_polygon(pixmap, gc, filled, points, 4);

	return;
} /* gerbv_gdk_draw_rectangle */


/*
 * Draws an oval _centered_ at x,y with x axis x_axis and y axis y_axis
 */ 
static void
gerbv_gdk_draw_oval(GdkPixmap *pixmap, GdkGC *gc,
		int filled, gint x, gint y, gint x_axis, gint y_axis,
		double angle_deg)
{
	gint width;
	GdkPoint points[2];
	GdkGC *local_gc = gdk_gc_new(pixmap);

	gdk_gc_copy(local_gc, gc);

	if (x_axis > y_axis) {
		/* Draw in x axis */
		width = y_axis;

		points[0].x = -(x_axis >> 1) + (y_axis >> 1);
		points[0].y = 0;
		points[1].x =  (x_axis >> 1) - (y_axis >> 1);
		points[1].y = 0;
	} else {
		/* Draw in y axis */
		width = x_axis;

		points[0].x = 0;
		points[0].y = -(y_axis >> 1) + (x_axis >> 1);
		points[1].x = 0;
		points[1].y =  (y_axis >> 1) - (x_axis >> 1);
	}

	points[0] = rotate_point(points[0], angle_deg);
	points[0].x += x;
	points[0].y += y;
	points[1] = rotate_point(points[1], angle_deg);
	points[1].x += x;
	points[1].y += y;

	gdk_gc_set_line_attributes(local_gc, width,
			GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_MITER);
	gdk_draw_line(pixmap, local_gc,
			points[0].x, points[0].y,
			points[1].x, points[1].y);

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
		round(64*fmod(angle1, 360)), round(64*(angle2 - angle1)));
} /* gerbv_gdk_draw_arc */

void
draw_gdk_render_polygon_object (gerbv_net_t *oldNet, gerbv_image_t *image, double sr_x, double sr_y,
			cairo_matrix_t *fullMatrix, cairo_matrix_t *scaleMatrix, GdkGC *gc, GdkGC *pgc,
			GdkPixmap **pixmap) {
	gerbv_net_t *currentNet;
	gint x2,y2,cp_x=0,cp_y=0,cir_width=0;
	GdkPoint *points = NULL;
	unsigned int pointArraySize, curr_point_idx;
	int steps,i;
	gdouble angleDiff, tempX, tempY;

	/* save the first net in the polygon as the "ID" net pointer
	in case we are saving this net to the selection array */
	curr_point_idx = 0;
	pointArraySize = 0;

	for (currentNet = oldNet->next; currentNet!=NULL; currentNet = currentNet->next){	
		tempX = currentNet->stop_x + sr_x;
		tempY = currentNet->stop_y + sr_y;
		cairo_matrix_transform_point (fullMatrix, &tempX, &tempY);
		x2 = (int)round(tempX);
		y2 = (int)round(tempY);
		
		/* 
		* If circle segment, scale and translate that one too
		*/
		if (currentNet->cirseg) {
			tempX = currentNet->cirseg->width;
			tempY = currentNet->cirseg->height;
			cairo_matrix_transform_point (scaleMatrix, &tempX, &tempY);

			/* Variables can be negative after transformation */
			tempX = fabs(tempX);
			cir_width = (int)round(tempX);

			tempX = currentNet->cirseg->cp_x + sr_x;
			tempY = currentNet->cirseg->cp_y + sr_y;
			cairo_matrix_transform_point (fullMatrix, &tempX, &tempY);
			cp_x = (int)round(tempX);
			cp_y = (int)round(tempY);
		}

		switch (currentNet->interpolation) {
			case GERBV_INTERPOLATION_LINEARx1 :
			case GERBV_INTERPOLATION_LINEARx10 :
			case GERBV_INTERPOLATION_LINEARx01 :
			case GERBV_INTERPOLATION_LINEARx001 :
				if (pointArraySize < (curr_point_idx + 1)) {
					pointArraySize = curr_point_idx + 1;
					points = (GdkPoint *)g_realloc(points,
							pointArraySize*sizeof(GdkPoint));
				}
				points[curr_point_idx].x = x2;
				points[curr_point_idx].y = y2;
				curr_point_idx++;
				break;
			case GERBV_INTERPOLATION_CW_CIRCULAR :
			case GERBV_INTERPOLATION_CCW_CIRCULAR :
				/* we need to chop up the arc into small lines for rendering
				with GDK */
				angleDiff = currentNet->cirseg->angle2 - currentNet->cirseg->angle1;
				steps = (int) abs(angleDiff);
				if (pointArraySize < (curr_point_idx + steps)) {
					pointArraySize = curr_point_idx + steps;
					points = (GdkPoint *)g_realloc(points,
							pointArraySize*sizeof(GdkPoint));
				}
				for (i=0; i<steps; i++){
					points[curr_point_idx].x = cp_x + cir_width / 2.0 * cos (DEG2RAD(currentNet->cirseg->angle1 +
									      (angleDiff * i) / steps));
					points[curr_point_idx].y = cp_y - cir_width / 2.0 * sin (DEG2RAD(currentNet->cirseg->angle1 +
									      (angleDiff * i) / steps));
					curr_point_idx++;
				}
				break;
			case GERBV_INTERPOLATION_PAREA_END :
				gdk_gc_copy(pgc, gc); 
				gdk_gc_set_line_attributes(pgc, 1, 
				       GDK_LINE_SOLID, 
				       GDK_CAP_PROJECTING, 
				       GDK_JOIN_MITER);
				gdk_draw_polygon(*pixmap, pgc, 1, points, curr_point_idx);
				g_free(points);
				points = NULL;
				return;
			default:
				GERB_COMPILE_WARNING(
					_("Skipped interpolation type %d"),
					currentNet->interpolation);
				break;
		}
	}
	return;
}

void
draw_gdk_apply_netstate_transformation  (cairo_matrix_t *fullMatrix, cairo_matrix_t *scaleMatrix,
	gerbv_netstate_t *state) {
	/* apply scale factor */
	cairo_matrix_scale (fullMatrix, state->scaleA, state->scaleB);
	cairo_matrix_scale (scaleMatrix, state->scaleA, state->scaleB);
	/* apply offset */
	cairo_matrix_translate (fullMatrix, state->offsetA, state->offsetB);
	/* apply mirror */
	switch (state->mirrorState) {
		case GERBV_MIRROR_STATE_FLIPA:
			cairo_matrix_scale (fullMatrix, -1, 1);
			cairo_matrix_scale (scaleMatrix, -1, 1);
			break;
		case GERBV_MIRROR_STATE_FLIPB:
			cairo_matrix_scale (fullMatrix, 1, -1);
			cairo_matrix_scale (scaleMatrix, -1, 1);
			break;
		case GERBV_MIRROR_STATE_FLIPAB:
			cairo_matrix_scale (fullMatrix, -1, -1);
			cairo_matrix_scale (scaleMatrix, -1, 1);
			break;
		default:
			break;
	}
	/* finally, apply axis select */
	if (state->axisSelect == GERBV_AXIS_SELECT_SWAPAB) {
		/* we do this by rotating 270 (counterclockwise, then mirroring
		   the Y axis */
		cairo_matrix_rotate (fullMatrix, M_PI + M_PI_2);
		cairo_matrix_scale (fullMatrix, 1, -1);
	}
}

static void
draw_gdk_cross (GdkPixmap *pixmap, GdkGC *gc, gint xc, gint yc, gint r)
{
	gdk_gc_set_line_attributes (gc, 1, GDK_LINE_SOLID,
			GDK_CAP_BUTT, GDK_JOIN_MITER);
	gdk_draw_line (pixmap, gc, xc - r, yc, xc + r, yc);
	gdk_draw_line (pixmap, gc, xc, yc - r, xc, yc + r);
}

/*
 * Convert a gerber image to a GDK clip mask to be used when creating pixmap
 */
int
draw_gdk_image_to_pixmap(GdkPixmap **pixmap, gerbv_image_t *image, 
	     double scale, double trans_x, double trans_y,
	     enum draw_mode drawMode,
	     gerbv_selection_info_t *selectionInfo, gerbv_render_info_t *renderInfo,
	     gerbv_user_transformation_t transform)
{
	const int hole_cross_inc_px = 8;
	GdkGC *gc = gdk_gc_new(*pixmap);
	GdkGC *pgc = gdk_gc_new(*pixmap);
	GdkGCValues gc_values;
	struct gerbv_net *net;
	gerbv_netstate_t *oldState;
	gerbv_layer_t *oldLayer;
	gint x1, y1, x2, y2;
	glong xlong1, ylong1, xlong2, ylong2;
	int p1, p2;
	int cir_width = 0, cir_height = 0;
	int cp_x = 0, cp_y = 0;
	GdkColor transparent, opaque;
	gerbv_polarity_t polarity;
	gdouble tempX, tempY, r;
	gdouble minX=0,minY=0,maxX=0,maxY=0;

	if (image == NULL || image->netlist == NULL) {
		gdk_gc_unref(gc);
		gdk_gc_unref(pgc);

		return 0;
	}

	if (transform.inverted) {
		if (image->info->polarity == GERBV_POLARITY_POSITIVE)
			polarity = GERBV_POLARITY_NEGATIVE;
		else
			polarity = GERBV_POLARITY_POSITIVE;
	} else {
		polarity = image->info->polarity;
	}
	if (drawMode == DRAW_SELECTIONS)
		polarity = GERBV_POLARITY_POSITIVE;

	gboolean useOptimizations = TRUE;
	// if the user is using any transformations for this layer, then don't bother using rendering
	//   optimizations
	if (fabs(transform.translateX) > GERBV_PRECISION_LINEAR_INCH
	||  fabs(transform.translateY) > GERBV_PRECISION_LINEAR_INCH
	||  fabs(transform.scaleX - 1) > GERBV_PRECISION_LINEAR_INCH 
	||  fabs(transform.scaleY - 1) > GERBV_PRECISION_LINEAR_INCH
	||  fabs(transform.rotation) > GERBV_PRECISION_ANGLE_RAD
	||  transform.mirrorAroundX || transform.mirrorAroundY)
		useOptimizations = FALSE;

	// calculate the transformation matrix for the user_transformation options
	cairo_matrix_t fullMatrix, scaleMatrix;
	cairo_matrix_init (&fullMatrix, 1, 0, 0, 1, 0, 0);
	cairo_matrix_init (&scaleMatrix, 1, 0, 0, 1, 0, 0);

	cairo_matrix_translate (&fullMatrix, trans_x, trans_y);
	cairo_matrix_scale (&fullMatrix, scale, scale);
	cairo_matrix_scale (&scaleMatrix, scale, scale);
	/* offset image */

	cairo_matrix_translate (&fullMatrix, transform.translateX, -1*transform.translateY);
	// don't use mirroring for the scale matrix
	gdouble scaleX = transform.scaleX;
	gdouble scaleY = -1*transform.scaleY;
	cairo_matrix_scale (&scaleMatrix, scaleX, -1*scaleY);
	if (transform.mirrorAroundX)
		scaleY *= -1;
	if (transform.mirrorAroundY)
		scaleX *= -1;

	cairo_matrix_scale (&fullMatrix, scaleX, scaleY);
	/* do image rotation */
	cairo_matrix_rotate (&fullMatrix, transform.rotation);
	//cairo_matrix_rotate (&scaleMatrix, transform.rotation);

	/* do image rotation */
	cairo_matrix_rotate (&fullMatrix, image->info->imageRotation);

	if (useOptimizations) {
		minX = renderInfo->lowerLeftX;
		minY = renderInfo->lowerLeftY;
		maxX = renderInfo->lowerLeftX + (renderInfo->displayWidth /
					renderInfo->scaleFactorX);
		maxY = renderInfo->lowerLeftY + (renderInfo->displayHeight /
					renderInfo->scaleFactorY);
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
	oldLayer = image->layers;
	oldState = image->states;
	for (net = image->netlist->next ; net != NULL; net = gerbv_image_return_next_renderable_object(net)) {
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

		/* check if this is a new netstate */
		if (net->state != oldState){
			/* it's a new state, so recalculate the new transformation matrix
			   for it */
			draw_gdk_apply_netstate_transformation (&fullMatrix, &scaleMatrix, net->state);
			oldState = net->state;	
		}
		/* check if this is a new layer */
		/* for now, only do layer rotations in GDK rendering */
		if (net->layer != oldLayer){
			cairo_matrix_rotate (&fullMatrix, net->layer->rotation);
			oldLayer = net->layer;
		}

		if (drawMode == DRAW_SELECTIONS) {
			gboolean foundNet = FALSE;
			gerbv_selection_item_t sItem;
			
			for (guint i = 0; i < selectionInfo->selectedNodeArray->len; i++) {
				sItem = g_array_index (selectionInfo->selectedNodeArray,
						gerbv_selection_item_t, i);
				if (sItem.net == net) {
					foundNet = TRUE;
					break;
				}
			}
			if (!foundNet)
				continue;
		}

		for(repeat_i = 0; repeat_i < repeat_X; repeat_i++) {
		for(repeat_j = 0; repeat_j < repeat_Y; repeat_j++) {
		  double sr_x = repeat_i * repeat_dist_X;
		  double sr_y = repeat_j * repeat_dist_Y;
			
			if ((useOptimizations)&&((net->boundingBox.right+sr_x < minX)
					|| (net->boundingBox.left+sr_x > maxX)
					|| (net->boundingBox.top+sr_y < minY)
					|| (net->boundingBox.bottom+sr_y > maxY))) {
				continue;
			}

		/* 
		 * If circle segment, scale and translate that one too
		 */
		if (net->cirseg) {
		    tempX = net->cirseg->width;
		    tempY = net->cirseg->height;
		    cairo_matrix_transform_point (&scaleMatrix, &tempX, &tempY);

		    /* Variables can be negative after transformation */
		    tempX = fabs(tempX);
		    tempY = fabs(tempY);
		    cir_width =  (int)round(tempX);
		    cir_height = (int)round(tempY);
		    
		    tempX = net->cirseg->cp_x;
		    tempY = net->cirseg->cp_y;
		    cairo_matrix_transform_point (&fullMatrix, &tempX, &tempY);
		    cp_x = (int)round(tempX);
		    cp_y = (int)round(tempY);
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
		    draw_gdk_render_polygon_object (net,image,sr_x,sr_y,&fullMatrix,
		    	&scaleMatrix,gc,pgc,pixmap);
		    continue;
		/* make sure we completely skip over any deleted nodes */
		case GERBV_INTERPOLATION_DELETED:
		    continue;
		default :
		    break;
		}


		/*
		 * If aperture state is off we allow use of undefined apertures.
		 * This happens when gerber files starts, but hasn't decided on 
		 * which aperture to use.
		 */
		if (image->aperture[net->aperture] == NULL) {
		  /* Commenting this out since it gets emitted every time you click on the screen 
		     if (net->aperture_state != GERBV_APERTURE_STATE_OFF)
		     GERB_MESSAGE("Aperture D%d is not defined", net->aperture);
		  */
		    continue;
		}

		/*
		 * Scale points with window scaling and translate them
		 */
		tempX = net->start_x + sr_x;
		tempY = net->start_y + sr_y;
		cairo_matrix_transform_point (&fullMatrix, &tempX, &tempY);
		xlong1 = (int)round(tempX);
		ylong1 = (int)round(tempY);

		tempX = net->stop_x + sr_x;
		tempY = net->stop_y + sr_y;
		cairo_matrix_transform_point (&fullMatrix, &tempX, &tempY);
		xlong2 = (int)round(tempX);
		ylong2 = (int)round(tempY);

		/* if the object is way outside our view window, just skip over it in order
		   to eliminate some GDK clipping problems at high zoom levels */
		if ((xlong1 < -10000) && (xlong2 < -10000))
			continue;
		if ((ylong1 < -10000) && (ylong2 < -10000))
			continue;
		if ((xlong1 > 10000) && (xlong2 > 10000))
			continue;
		if ((ylong1 > 10000) && (ylong2 > 10000))
			continue;

		if (xlong1 > G_MAXINT) x1 = G_MAXINT;
		else if (xlong1 < G_MININT) x1 = G_MININT;
		else x1 = (int)xlong1;

		if (xlong2 > G_MAXINT) x2 = G_MAXINT;
		else if (xlong2 < G_MININT) x2 = G_MININT;
		else x2 = (int)xlong2;

		if (ylong1 > G_MAXINT) y1 = G_MAXINT;
		else if (ylong1 < G_MININT) y1 = G_MININT;
		else y1 = (int)ylong1;

		if (ylong2 > G_MAXINT) y2 = G_MAXINT;
		else if (ylong2 < G_MININT) y2 = G_MININT;
		else y2 = (int)ylong2;

		switch (net->aperture_state) {
		case GERBV_APERTURE_STATE_ON :
		    tempX = image->aperture[net->aperture]->parameter[0];
		    cairo_matrix_transform_point (&scaleMatrix, &tempX, &tempY);

		    /* Variables can be negative after transformation */
		    tempX = fabs(tempX);
		    p1 = (int)round(tempX);

		    gdk_gc_set_line_attributes(gc, p1, GDK_LINE_SOLID,
				    (image->aperture[net->aperture]->type == GERBV_APTYPE_RECTANGLE)?
						GDK_CAP_PROJECTING: GDK_CAP_ROUND,
				    GDK_JOIN_MITER);
		    
		    switch (net->interpolation) {
		    case GERBV_INTERPOLATION_LINEARx10 :
		    case GERBV_INTERPOLATION_LINEARx01 :
		    case GERBV_INTERPOLATION_LINEARx001 :
			GERB_MESSAGE(_("Linear != x1"));
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
			if (image->aperture[net->aperture]->type != GERBV_APTYPE_RECTANGLE) {
				gdk_draw_line(*pixmap, gc, x1, y1, x2, y2);

				if (renderInfo->show_cross_on_drill_holes
				&& image->layertype == GERBV_LAYERTYPE_DRILL) {
					/* Draw crosses on drill slot start and end */
					r = p1/2.0 + hole_cross_inc_px;
					draw_gdk_cross(*pixmap, gc, x1, y1, r);
					draw_gdk_cross(*pixmap, gc, x2, y2, r);
				}
			    break;
			}

			gint dx, dy;
			GdkPoint poly[6];

			tempX = image->aperture[net->aperture]->parameter[0]/2;
			tempY = image->aperture[net->aperture]->parameter[1]/2;
			cairo_matrix_transform_point (&scaleMatrix, &tempX, &tempY);
			dx = (int)round(tempX);
			dy = (int)round(tempY);

			if(x1 > x2) dx = -dx;
			if(y1 > y2) dy = -dy;
			poly[0].x = x1 - dx; poly[0].y = y1 - dy;
			poly[1].x = x1 - dx; poly[1].y = y1 + dy;
			poly[2].x = x2 - dx; poly[2].y = y2 + dy;
			poly[3].x = x2 + dx; poly[3].y = y2 + dy;
			poly[4].x = x2 + dx; poly[4].y = y2 - dy;
			poly[5].x = x1 + dx; poly[5].y = y1 - dy;
			gdk_draw_polygon(*pixmap, gc, 1, poly, 6);
			break;

		    case GERBV_INTERPOLATION_CW_CIRCULAR :
		    case GERBV_INTERPOLATION_CCW_CIRCULAR :
			gerbv_gdk_draw_arc(*pixmap, gc, cp_x, cp_y,
				cir_width, cir_height,
				net->cirseg->angle1 +
					RAD2DEG(transform.rotation),
				net->cirseg->angle2 +
					RAD2DEG(transform.rotation));
			break;
		    default :
			GERB_COMPILE_WARNING(
				_("Skipped interpolation type %d"),
				net->interpolation);
			break;
		    }
		    break;
		case GERBV_APERTURE_STATE_OFF :
		    break;
		case GERBV_APERTURE_STATE_FLASH :
		    tempX = image->aperture[net->aperture]->parameter[0];
		    tempY = image->aperture[net->aperture]->parameter[1];
		    cairo_matrix_transform_point (&scaleMatrix, &tempX, &tempY);

		    /* Variables can be negative after transformation */
		    tempX = fabs(tempX);
		    tempY = fabs(tempY);
		    p1 = (int)round(tempX);
		    p2 = (int)round(tempY);
		    
		    switch (image->aperture[net->aperture]->type) {
		    case GERBV_APTYPE_CIRCLE :
			gerbv_gdk_draw_circle(*pixmap, gc, TRUE, x2, y2, p1);

			if (renderInfo->show_cross_on_drill_holes
			&& image->layertype == GERBV_LAYERTYPE_DRILL) {
				r = p1/2.0 + hole_cross_inc_px;
				draw_gdk_cross(*pixmap, gc, x2, y2, r);
			}

			/*
			 * If circle has an inner diameter we must remove
			 * that part of the circle to make a hole in it.
			 * We should actually support square holes too,
			 * but due to laziness I don't.
			 */
			if (p2) {
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
			gerbv_gdk_draw_rectangle(*pixmap, gc, TRUE,
				x2, y2, p1, p2, RAD2DEG(transform.rotation +
					image->info->imageRotation));
			break;
		    case GERBV_APTYPE_OVAL :
			gerbv_gdk_draw_oval(*pixmap, gc, TRUE,
				x2, y2, p1, p2, RAD2DEG(transform.rotation +
					image->info->imageRotation));
			break;
		    case GERBV_APTYPE_POLYGON :
			/* TODO: gdk_draw_polygon() */
			gerbv_gdk_draw_circle(*pixmap, gc, TRUE, x2, y2, p1);
			break;
		    case GERBV_APTYPE_MACRO :
			/* TODO: check line22 and others */
			gerbv_gdk_draw_amacro(*pixmap, gc, 
					      image->aperture[net->aperture]->simplified,
					      scale, x2, y2);
			break;
		    default :
			GERB_MESSAGE(_("Unknown aperture type %d"),
					image->aperture[net->aperture]->type);
			return 0;
		    }
		    break;
		default :
		    GERB_MESSAGE(_("Unknown aperture state %d"),
				net->aperture_state);
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



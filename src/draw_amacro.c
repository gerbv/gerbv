/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2000-2002 Stefan Petersen (spe@stacken.kth.se)
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
#include <math.h> /* M_PI */

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gtk/gtk.h>

#include "draw_amacro.h"

#undef round
#define round(x) floor((double)(x) + 0.5)


/*
 * Stack declarations and operations to be used by the simple engine that
 * executes the parsed aperture macros.
 */
typedef struct {
    double *stack;
    int sp;
} stack_t;


static stack_t *
new_stack(unsigned int nuf_push)
{
    const int extra_stack_size = 10;
    stack_t *s;

    s = (stack_t *)malloc(sizeof(stack_t));
    if (!s) {
	free(s);
	return NULL;
    }
    memset(s, 0, sizeof(stack_t));

    s->stack = (double *)malloc(sizeof(double) * (nuf_push + extra_stack_size));
    if (!s->stack) {
	free(s->stack);
	return NULL;
    }

    memset(s->stack, 0, sizeof(double) * (nuf_push + extra_stack_size));
    s->sp = 0;

    return s;
} /* new_stack */


static void
free_stack(stack_t *s)
{
    if (s && s->stack)
	free(s->stack);

    if (s)
	free(s);

    return;
} /* free_stack */


static void
push(stack_t *s, double val)
{
    s->stack[s->sp++] = val;
    return;
} /* push */


static double
pop(stack_t *s)
{
    return s->stack[--s->sp];
} /* pop */


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
 * Doesn't handle explicit x,y yet
 */
static void
gerbv_draw_prim1(GdkPixmap *pixmap, GdkGC *gc, stack_t *s, int scale,
		 gint x, gint y)
{
    const int exposure_idx = 0;
    const int diameter_idx = 1;
    const gint full_circle = 23360;
    GdkGC *local_gc = gdk_gc_new(pixmap);
    gint dia    = round(fabs(s->stack[diameter_idx] * scale));
    gint real_x = x - dia / 2;
    gint real_y = y - dia / 2;
    GdkColor color;

    gdk_gc_copy(local_gc, gc);

    /* Exposure */
    if (s->stack[exposure_idx] == 0.0) {
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
} /* gerbv_draw_prim1 */


/*
 * Doesn't handle explicit x,y yet
 * Questions:
 *  - should start point be included in number of points?
 *  - how thick is the outline?
 */
static void
gerbv_draw_prim4(GdkPixmap *pixmap, GdkGC *gc, stack_t *s, int scale,
		 gint x, gint y)
{
    const int exposure_idx = 0;
    const int nuf_points_idx = 1;
    const int first_x_idx = 2;
    const int first_y_idx = 3;
    const int rotext_idx = 4;
    GdkGC *local_gc = gdk_gc_new(pixmap);
    int nuf_points, point, closed_shape;
    double rotation;
    GdkPoint *points;
    GdkColor color;


    nuf_points = (int)s->stack[nuf_points_idx];
    points = (GdkPoint *)malloc(sizeof(GdkPoint) * nuf_points);
    if (!points) {
	free(points);
	return;
    }

    /*
     * Closed (ie filled as I interpret it) shape if first and last point
     * are the same.
     */
    closed_shape = 
	(fabs(s->stack[first_x_idx] - s->stack[nuf_points * 2 + first_x_idx]) < 0.0001) &&
	(fabs(s->stack[first_y_idx] - s->stack[nuf_points * 2 + first_y_idx]) < 0.0001);

    rotation = s->stack[nuf_points * 2 + rotext_idx];
    for (point = 0; point < nuf_points; point++) {
	points[point].x = (int)round(scale * s->stack[point * 2 + first_x_idx]);
	points[point].y = -(int)round(scale * s->stack[point * 2 + first_y_idx]);
	if (rotation > 0.1)
	    points[point] = rotate_point(points[point], rotation);
	points[point].x += x;
	points[point].y += y;
    }

    gdk_gc_copy(local_gc, gc);

    /* Exposure */
    if (s->stack[exposure_idx] == 0.0) {
	color.pixel = 0;
	gdk_gc_set_foreground(local_gc, &color);
    }

    gdk_gc_set_line_attributes(local_gc, 
			       1, /* outline always 1 pixels */
			       GDK_LINE_SOLID, 
			       GDK_CAP_BUTT, 
			       GDK_JOIN_MITER);
    gdk_draw_polygon(pixmap, local_gc, closed_shape, points, nuf_points);

    free(points);

    gdk_gc_unref(local_gc);

    return;
} /* gerbv_draw_prim4 */


/*
 * Doesn't handle explicit x,y yet
 */
static void
gerbv_draw_prim5(GdkPixmap *pixmap, GdkGC *gc, stack_t *s, int scale,
		 gint x, gint y)
{
    const int exposure_idx = 0;
    const int nuf_vertices_idx = 1;
    const int diameter_idx = 4;
    const int rotation_idx = 5;
    int nuf_vertices, i;
    double vertex, tick, rotation, radius;
    GdkPoint *points;
    GdkGC *local_gc = gdk_gc_new(pixmap);
    GdkColor color;

    if (s->sp != 6)
	return;

    nuf_vertices = (int)s->stack[nuf_vertices_idx];
    points = (GdkPoint *)malloc(sizeof(GdkPoint) * nuf_vertices);
    if (!points) {
	free(points);
	return;
    }

    /* Exposure */
    if (s->stack[exposure_idx] == 0.0) {
	color.pixel = 0;
	gdk_gc_set_foreground(local_gc, &color);
    }

    tick = 2 * M_PI / (double)nuf_vertices;
    rotation = -s->stack[rotation_idx] * M_PI / 180.0;
    radius = s->stack[diameter_idx] / 2.0;
    for (i = 0; i < nuf_vertices; i++) {
	vertex =  tick * (double)i + rotation;
	points[i].x = (int)round(scale * radius * cos(vertex)) + x;
	points[i].y = (int)round(scale * radius * sin(vertex)) + y;
    }

    gdk_draw_polygon(pixmap, gc, 1, points, nuf_vertices);

    gdk_gc_unref(local_gc);

    free(points);
    return;
} /* gerbv_draw_prim5 */


/*
 * Doesn't handle and explicit x,y yet
 * Questions:
 *  - is "gap" distance between edges of circles or distance between
 *    center of line of circle?
 */
static void
gerbv_draw_prim6(GdkPixmap *pixmap, GdkGC *gc, stack_t *s, int scale,
		 gint x, gint y)
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
			       (int)round(scale * s->stack[ci_thickness_idx]),
			       GDK_LINE_SOLID, 
			       GDK_CAP_BUTT, 
			       GDK_JOIN_MITER);

    real_dia = s->stack[outside_dia_idx] -  s->stack[ci_thickness_idx] / 2.0;
    real_gap = s->stack[gap_idx] + s->stack[ci_thickness_idx];

    for (circle = 0; circle != (int)s->stack[nuf_circles_idx];  circle++) {
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
    crosshair[0].x = (int)((s->stack[ch_length_idx] / 2.0) * scale);
    /*crosshair[0].y = 0;*/
    crosshair[1].x = -crosshair[0].x;
    /*crosshair[1].y = 0;*/
    /*crosshair[2].x = 0;*/
    crosshair[2].y = crosshair[0].x;
    /*crosshair[3].x = 0;*/
    crosshair[3].y = -crosshair[0].x;

    gdk_gc_set_line_attributes(local_gc, 
			       (int)round(scale * s->stack[ch_thickness_idx]),
			       GDK_LINE_SOLID, 
			       GDK_CAP_BUTT, 
			       GDK_JOIN_MITER);

    for (point = 0; point < 4; point++) {
	crosshair[point] = rotate_point(crosshair[point], 
					s->stack[rotation_idx]);
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
} /* gerbv_draw_prim6 */


static void
gerbv_draw_prim7(GdkPixmap *pixmap, GdkGC *gc, stack_t *s, int scale,
		 gint x, gint y)
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
    double ci_thickness = (s->stack[outside_dia_idx] - 
			s->stack[inside_dia_idx]) / 2.0;

    gdk_gc_copy(local_gc, gc);
    gdk_gc_set_line_attributes(local_gc, 
			       (int)round(scale * ci_thickness),
			       GDK_LINE_SOLID, 
			       GDK_CAP_BUTT, 
			       GDK_JOIN_MITER);

    /* 
     * Non filled circle 
     */
    diameter = (s->stack[inside_dia_idx] + ci_thickness) * scale;
    gdk_draw_arc(pixmap, local_gc, 0, x - diameter / 2, y - diameter / 2, 
		 diameter, diameter, 0, full_circle);

    /*
     * Cross hair
     */ 
    /* Calculate the end points of the crosshair */
    for (i = 0; i < 4; i++) {
	point[i].x = round((s->stack[outside_dia_idx] / 2.0) * scale);
	point[i].y = 0;
	point[i] = rotate_point(point[i], s->stack[rotation_idx] + 90 * i);
	point[i].x += x;
	point[i].y += y;
    }

    /* We must "reach out" to the outer part of the circle, hence round end */
    gdk_gc_set_line_attributes(local_gc, 
			       (int)round(scale * s->stack[ch_thickness_idx]),
			       GDK_LINE_SOLID, 
			       GDK_CAP_ROUND, 
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
} /* gerbv_draw_prim7 */


/*
 * Doesn't handle and explicit x,y yet
 */
static void
gerbv_draw_prim20(GdkPixmap *pixmap, GdkGC *gc, stack_t *s, int scale,
		  gint x, gint y)
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
    if (s->stack[exposure_idx] == 0.0) {
	color.pixel = 0;
	gdk_gc_set_foreground(local_gc, &color);
    }

    gdk_gc_set_line_attributes(local_gc, 
			       (int)round(scale * s->stack[linewidth_idx]),
			       GDK_LINE_SOLID, 
			       GDK_CAP_BUTT, 
			       GDK_JOIN_MITER);

    points[0].x = (s->stack[start_x_idx] * scale);
    points[0].y = (s->stack[start_y_idx] * scale);
    points[1].x = (s->stack[end_x_idx] * scale);
    points[1].y = (s->stack[end_y_idx] * scale);

    for (i = 0; i < nuf_points; i++) {
	points[i] = rotate_point(points[i], s->stack[rotation_idx]);
	points[i].x = x + points[i].x;
	points[i].y = y - points[i].y;
    }

    gdk_draw_line(pixmap, local_gc, 
		  points[0].x, points[0].y, 
		  points[1].x, points[1].y);
    
    gdk_gc_unref(local_gc);

    return;
} /* gerbv_draw_prim20 */


/*
 * Doesn't handle explicit x,y yet
 */
static void
gerbv_draw_prim21(GdkPixmap *pixmap, GdkGC *gc, stack_t *s, int scale,
		  gint x, gint y)
{
    const int exposure_idx = 0;
    const int width_idx = 1;
    const int height_idx = 2;
    const int rotation_idx = 5;
    const int nuf_points = 4;
    GdkPoint points[nuf_points];
    GdkColor color;
    GdkGC *local_gc = gdk_gc_new(pixmap);
    int half_width, half_height;
    int i;

    half_width = (int)round(s->stack[width_idx] * scale / 2.0);
    half_height =(int)round(s->stack[height_idx] * scale / 2.0);

    points[0].x = half_width;
    points[0].y = half_height;

    points[1].x = half_width;
    points[1].y = -half_height;

    points[2].x = -half_width;
    points[2].y = -half_height;

    points[3].x = -half_width;
    points[3].y = half_height;

    for (i = 0; i < nuf_points; i++) {
	points[i] = rotate_point(points[i], s->stack[rotation_idx]);
	points[i].x += x;
	points[i].y += y;
    }

    /* Exposure */
    if (s->stack[exposure_idx] == 0.0) {
	color.pixel = 0;
	gdk_gc_set_foreground(local_gc, &color);
    }

    gdk_draw_polygon(pixmap, gc, 1, points, nuf_points);

    gdk_gc_unref(local_gc);

    return;
} /* gerbv_draw_prim21 */


/*
 * Doesn't handle explicit x,y yet
 */
static void
gerbv_draw_prim22(GdkPixmap *pixmap, GdkGC *gc, stack_t *s, int scale,
		  gint x, gint y)
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

    points[0].x = (int)round(s->stack[x_lower_left_idx] * scale);
    points[0].y = (int)round(s->stack[y_lower_left_idx] * scale);

    points[1].x = (int)round((s->stack[x_lower_left_idx] + s->stack[width_idx])
			     * scale);
    points[1].y = (int)round(s->stack[y_lower_left_idx] * scale);

    points[2].x = (int)round((s->stack[x_lower_left_idx]  + s->stack[width_idx])
			     * scale);
    points[2].y = (int)round((s->stack[y_lower_left_idx]  - s->stack[height_idx])
			     * scale);

    points[3].x = (int)round(s->stack[x_lower_left_idx] * scale);
    points[3].y = (int)round((s->stack[y_lower_left_idx] - s->stack[height_idx])
			     * scale);

    for (i = 0; i < nuf_points; i++) {
	points[i] = rotate_point(points[i], s->stack[rotation_idx]);
	points[i].x += x;
	points[i].y += y;
    }
    
    /* Exposure */
    if (s->stack[exposure_idx] == 0.0) {
	color.pixel = 0;
	gdk_gc_set_foreground(local_gc, &color);
    }

    gdk_draw_polygon(pixmap, gc, 1, points, nuf_points);

    gdk_gc_unref(local_gc);

    return;
} /* gerbv_draw_prim22 */


int
gerbv_draw_amacro(GdkPixmap *pixmap, GdkGC *gc,
		  instruction_t *program, unsigned int nuf_push,
		  double *parameters, int scale, gint x, gint y)
{
    stack_t *s = new_stack(nuf_push);
    instruction_t *ip;
    int handled = 1;
    
    for(ip = program; ip != NULL; ip = ip->next) {
	switch(ip->opcode) {
	case NOP:
	    break;
	case PUSH :
	    push(s, ip->data.fval);
	    break;
        case PPUSH :
	    push(s, parameters[ip->data.ival - 1]);
	    break;
	case ADD :
	    push(s, pop(s) + pop(s));
	    break;
	case SUB :
	    push(s, -pop(s) + pop(s));
	    break;
	case MUL :
	    push(s, pop(s) * pop(s));
	    break;
	case DIV :
	    push(s, 1 / ((pop(s) / pop(s))));
	    break;
	case PRIM :
	    /* 
	     * This handles the exposure thing in the aperture macro
	     * The exposure is always the first element on stack independent
	     * of aperture macro.
	     */
	    switch(ip->data.ival) {
	    case 1:
		gerbv_draw_prim1(pixmap, gc, s, scale, x, y);
		break;
	    case 4 :
		gerbv_draw_prim4(pixmap, gc, s, scale, x, y);
		break;
	    case 5 :
		gerbv_draw_prim5(pixmap, gc, s, scale, x, y);
		break;
	    case 6 :
		gerbv_draw_prim6(pixmap, gc, s, scale, x, y);
		break;
	    case 7 :
		gerbv_draw_prim7(pixmap, gc, s, scale, x, y);
		break;
	    case 2  :
	    case 20 :
		gerbv_draw_prim20(pixmap, gc, s, scale, x, y);
		break;
	    case 21 :
		gerbv_draw_prim21(pixmap, gc, s, scale, x, y);
		break;
	    case 22 :
		gerbv_draw_prim22(pixmap, gc, s, scale, x, y);
		break;
	    default :
		handled = 0;
	    }
	    /* 
	     * Here we reset the stack pointer. It's not general correct
	     * correct to do this, but since I know how the compiler works
	     * I can do this. The correct way to do this should be to 
	     * subtract number of used elements in each primitive operation.
	     */
	    s->sp = 0;
	    break;
	default :
	    break;
	}
    }
    free_stack(s);

    return handled;
} /* gerbv_draw_amacro */

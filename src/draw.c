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
 * Stack declarations and operations
 */
typedef struct {
    double *stack;
    int sp;
} stack_t;


stack_t *
new_stack(void)
{
    const int stack_size = 55;
    stack_t *s;

    s = (stack_t *)malloc(sizeof(stack_t));
    if (!s) {
	free(s);
	return NULL;
    }
    memset(s, 0, sizeof(stack_t));

    s->stack = (double *)malloc(sizeof(double) * stack_size);
    if (!s->stack) {
	free(s->stack);
	return NULL;
    }

    memset(s->stack, 0, sizeof(double) * stack_size);
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


#if 0
static void
print_stack(stack_t *s)
{
    int i;

    printf("%d:",s->sp);
    for (i = 0; i < s->sp; i++)
	printf("%f  ", s->stack[i]);
    printf("\n");
    
    return;
} /* print_stack */
#endif


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
GdkPoint 
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
 * Doesn't handle exposure yet and explicit x,y
 * Questions:
 *  - Empty or filled circle?
 *  - If empty, width of line?
 */
static void
gerbv_draw_prim1(GdkPixmap *pixmap, GdkGC *gc, stack_t *s, int scale,
		 gint x, gint y)
{
    const int diameter_idx = 1;
    GdkGC *local_gc = gdk_gc_new(pixmap);

    gdk_gc_copy(local_gc, gc);
    gdk_gc_set_line_attributes(local_gc, 
			       1, /* outline always 1 pixels */
			       GDK_LINE_SOLID, 
			       GDK_CAP_BUTT, 
			       GDK_JOIN_MITER);

    /* 
     * Non filled circle 
     */
    gerbv_draw_circle(pixmap, local_gc, 0, x, y,
		      s->stack[diameter_idx] * scale);

    gdk_gc_unref(local_gc);

    return;
} /* gerbv_draw_prim1 */


/*
 * Doesn't handle exposure yet and explicit x,y
 * Questions:
 *  - should start point be included in number of points?
 *  - how thick is the outline?
 */
static void
gerbv_draw_prim4(GdkPixmap *pixmap, GdkGC *gc, stack_t *s, int scale,
		 gint x, gint y)
{
    const int nuf_points_idx = 1;
    const int rotext_idx = 4;
    GdkGC *local_gc = gdk_gc_new(pixmap);
    int nuf_points, point;
    double rotation;
    GdkPoint *points;

    nuf_points = (int)s->stack[nuf_points_idx];
    points = (GdkPoint *)malloc(sizeof(GdkPoint) * nuf_points);
    if (!points) {
	free(points);
	return;
    }

    rotation = s->stack[nuf_points + rotext_idx];
    for (point = 0; point < nuf_points; point++) {
	points[point].x = (int)round(scale * s->stack[point * 2 + 2]);
	points[point].y = -(int)round(scale * s->stack[point * 2 + 3]);
	points[point] = rotate_point(points[point], rotation);
	points[point].x += x;
	points[point].y += y;
    }

    gdk_gc_copy(local_gc, gc);
    gdk_gc_set_line_attributes(local_gc, 
			       1, /* outline always 1 pixels */
			       GDK_LINE_SOLID, 
			       GDK_CAP_BUTT, 
			       GDK_JOIN_MITER);
    gdk_draw_polygon(pixmap, local_gc, 0, points, nuf_points);

    free(points);
    gdk_gc_unref(local_gc);
    return;
} /* gerbv_draw_prim4 */


/*
 * Doesn't handle exposure yet and explicit x,y
 */
static void
gerbv_draw_prim5(GdkPixmap *pixmap, GdkGC *gc, stack_t *s, int scale,
		 gint x, gint y)
{
    const int nuf_vertices_idx = 1;
    const int diameter_idx = 4;
    const int rotation_idx = 5;
    int nuf_vertices, i;
    double vertex, tick, rotation, radius;
    GdkPoint *points;

    if (s->sp != 6)
	return;

    nuf_vertices = (int)s->stack[nuf_vertices_idx];
    points = (GdkPoint *)malloc(sizeof(GdkPoint) * nuf_vertices);
    if (!points) {
	free(points);
	return;
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

    free(points);
    return;
} /* gerbv_draw_prim5 */


/*
 * Doesn't handle exposure yet and explicit x,y
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
	gerbv_draw_circle(pixmap, local_gc, 0, x, y,
			  (real_dia - real_gap * circle) * scale);
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
    double new_stack[] = 
    {s->stack[0],                      /* X */
     s->stack[1],                      /* Y */
     s->stack[outside_dia_idx],        /* Outside Diameter */
     (s->stack[outside_dia_idx] - 
      s->stack[inside_dia_idx]) / 2.0, /* Circle Line Thickness */
     0.0,                              /* Gap Between Circles */
     1.0,                              /* Number of Circles */
     s->stack[ch_thickness_idx],       /* Cross Hair Thickness */
     s->stack[outside_dia_idx],        /* Cross Hair Length */
     s->stack[rotation_idx]};          /* Rotation */
    double *old_stack;

    old_stack = s->stack;
    s->stack = new_stack;
    gerbv_draw_prim6(pixmap, gc, s, scale, x, y);
    s->stack = old_stack;

    return;
} /* gerbv_draw_prim7 */


/*
 * Doesn't handle exposure yet and explicit x,y
 */
void
gerbv_draw_prim20(GdkPixmap *pixmap, GdkGC *gc, stack_t *s, int scale,
		  gint x, gint y)
{
    const int linewidth_idx = 1;
    const int start_x_idx = 2;
    const int start_y_idx = 3;
    const int end_x_idx = 4;
    const int end_y_idx = 5;
    const int rotation_idx = 6;
    const int nuf_points = 2;
    GdkGC *local_gc = gdk_gc_new(pixmap);
    GdkPoint points[nuf_points];
    int i;

    gdk_gc_copy(local_gc, gc);

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
	points[i].x += x;
	points[i].y += y;
    }

    gdk_draw_line(pixmap, local_gc, 
		  points[0].x, points[0].y, 
		  points[1].x, points[1].y);
    
    gdk_gc_unref(local_gc);

    return;
} /* gerbv_draw_prim20 */


/*
 * Doesn't handle exposure yet and explicit x,y
 */
void
gerbv_draw_prim21(GdkPixmap *pixmap, GdkGC *gc, stack_t *s, int scale,
		  gint x, gint y)
{
    const int width_idx = 1;
    const int height_idx = 2;
    const int rotation_idx = 5;
    const int nuf_points = 4;
    GdkPoint points[nuf_points];
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

    gdk_draw_polygon(pixmap, gc, 1, points, nuf_points);

    return;
} /* gerbv_draw_prim21 */


void
gerbv_draw_prim22(GdkPixmap *pixmap, GdkGC *gc, stack_t *s, int scale,
		  gint x, gint y)
{
    const int width_idx = 1;
    const int height_idx = 2;
    const int x_lower_left_idx = 3;
    const int y_lower_left_idx = 4;
    const int rotation_idx = 5;
    const int nuf_points = 4;
    GdkPoint points[nuf_points];
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

    gdk_draw_polygon(pixmap, gc, 1, points, nuf_points);

    return;
} /* gerbv_draw_prim22 */


static int
gerbv_draw_amacro(GdkPixmap *pixmap, GdkGC *gc,
		  instruction_t *program, double *parameters, int scale,
		  gint x, gint y)
{
    stack_t *s = new_stack();
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
#if 0
		printf("Unknown/unhandled primitive %d\n", ip->data.ival);
		print_stack(s);
#endif
		handled = 0;
	    }
	    break;
	default :
	}
    }
    free_stack(s);
    return handled;
} /* gerbv_draw_amacro */


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
		/*
		 * While aperture macros are not properly implemented
		 * we do like this in the mean time.
		 */
		if (!gerbv_draw_amacro(*pixmap, line_gc, 
				       image->aperture[net->aperture]->amacro->program,
				       image->aperture[net->aperture]->parameter,
				       scale, x2, y2)) {
		    gdk_gc_set_foreground(err_gc, err_color);
		    gerbv_draw_circle(*pixmap, err_gc, TRUE, x2, y2, scale/20);
		}
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

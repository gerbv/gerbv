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


#include "draw_amacro.h"
#include "gerb_error.h"


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
static void 
rotate_point(double *x, double *y, double angle)
{
    double sint, cost;
    double _x = *x, _y = *y;
    
    if (angle == 0.0)
	return;
    
    sint = sin(angle * M_PI / 180.0);
    cost = cos(angle * M_PI / 180.0);
    
    *x = cost * _x - sint * _y;
    *y = sint * _x + cost * _y;
}



/*
 * Doesn't handle exposure yet and explicit x,y
 */
static void 
gerbv_draw_prim1 (struct gerb_render_context *ctx,
		  stack_t *s, int scale, double x, double y)
{
    const int diameter_idx = 1;
    double dia    = round(fabs(s->stack[diameter_idx] * scale));
    double real_x = x - 0.5 * dia;
    double real_y = y - 0.5 * dia;

    ctx->fill_oval(ctx, real_x, real_y, dia, dia);

} /* gerbv_draw_prim1 */


/*
 * Doesn't handle exposure yet and explicit x,y
 * Questions:
 *  - should start point be included in number of points?
 *  - how thick is the outline?
 */
static void 
gerbv_draw_prim4(struct gerb_render_context *ctx, stack_t *s, int scale,
		 double x, double y)
{
    const int nuf_points_idx = 1;
    const int first_x_idx = 2;
    const int first_y_idx = 3;
    const int rotext_idx = 4;
    int closed_shape = 0;
    int nuf_points, i;
    double rotation;
    double *points;

    nuf_points = (int)s->stack[nuf_points_idx];

    if (!(points = malloc(2 * sizeof(double) * nuf_points)))
	GERB_FATAL_ERROR("Malloc failed\n");

    /*
     * Closed (ie filled as I interpret it) shape if first and last point
     * are the same.
     */
    if (s->stack[first_x_idx] == s->stack[nuf_points * 2 + first_x_idx]
     && s->stack[first_y_idx] == s->stack[nuf_points * 2 + first_y_idx])
        closed_shape = 1;

    rotation = s->stack[nuf_points * 2 + rotext_idx];

    for (i=0; i<nuf_points; i++) {
	points[2*i]   = scale * s->stack[2 * i + first_x_idx];
	points[2*i+1] = -(scale * s->stack[2 * i + first_y_idx]);
	rotate_point(&points[2*i], &points[2*i+1], rotation);
	points[2*i]   += x;
	points[2*i+1] += y;
    }

    if (closed_shape)
	ctx->fill_polygon (ctx, points, nuf_points);
    else
	ctx->draw_linestrip (ctx, points, nuf_points);

    free(points);

} /* gerbv_draw_prim4 */


/*
 * Doesn't handle exposure yet and explicit x,y
 */
static void 
gerbv_draw_prim5(struct gerb_render_context *ctx, stack_t *s, int scale,
		 double x, double y)
{
    const int nuf_vertices_idx = 1;
    const int diameter_idx = 4;
    const int rotation_idx = 5;
    int nuf_vertices, i;
    double vertex, tick, rotation, radius;
    double *points;

    if (s->sp != 6)
	return;

    nuf_vertices = (int)s->stack[nuf_vertices_idx];

    if (!(points = malloc(2 * sizeof(double) * nuf_vertices)))
	GERB_FATAL_ERROR("Malloc failed\n");

    tick = 2 * M_PI / (double)nuf_vertices;
    rotation = -s->stack[rotation_idx] * M_PI / 180.0;
    radius = s->stack[diameter_idx] / 2.0;

    for (i=0; i<nuf_vertices; i++) {
	vertex =  tick * (double)i + rotation;
	points[2*i]   = (scale * radius * cos(vertex)) + x;
	points[2*i+1] = (scale * radius * sin(vertex)) + y;
    }

    ctx->fill_polygon (ctx, points, nuf_vertices);

    free(points);
} /* gerbv_draw_prim5 */


/*
 * Doesn't handle exposure yet and explicit x,y
 * Questions:
 *  - is "gap" distance between edges of circles or distance between
 *    center of line of circle?
 */
static void 
gerbv_draw_prim6 (struct gerb_render_context *ctx, stack_t *s, int scale,
		  double x, double y)
{
    const int outside_dia_idx = 2;
    const int ci_thickness_idx = 3;
    const int gap_idx = 4;
    const int nuf_circles_idx = 5;
    const int ch_thickness_idx = 6;
    const int ch_length_idx = 7;
    const int rotation_idx = 8;
    double real_dia;
    double real_gap;
    double crosshair[8];
    int i;

    ctx->set_line_style (ctx, scale * s->stack[ci_thickness_idx], 0, 0);

    real_dia = s->stack[outside_dia_idx] - s->stack[ci_thickness_idx] / 2.0;
    real_gap = s->stack[gap_idx] + s->stack[ci_thickness_idx];

    for (i=0; i<s->stack[nuf_circles_idx]; i++) {
	/* 
	 * Non filled circle 
	 */
	double dia = (real_dia - real_gap * i) * scale;
	ctx->draw_arc (ctx, x, y, dia, dia, 0.0, 360.0);
    }

    /*
     * Cross Hair 
     */
    crosshair[0] = 0.5 * s->stack[ch_length_idx] * scale;
    crosshair[1] = 0.0;
    crosshair[2] = -crosshair[0];
    crosshair[3] = 0.0;
    crosshair[4] = 0.0;
    crosshair[5] = crosshair[0];
    crosshair[6] = 0.0;
    crosshair[7] = -crosshair[0];

    for (i=0; i<4; i++) {
	rotate_point(&crosshair[2*i], &crosshair[2*i+1], s->stack[rotation_idx]);
	crosshair[2*i]   += x;
	crosshair[2*i+1] += y;
    }

    ctx->set_line_style (ctx, scale * s->stack[ch_thickness_idx], 0, 0);

    ctx->draw_line (ctx, crosshair[0], crosshair[1], crosshair[2], crosshair[3]);
    ctx->draw_line (ctx, crosshair[4], crosshair[5], crosshair[6], crosshair[7]);

} /* gerbv_draw_prim6 */


static void 
gerbv_draw_prim7(struct gerb_render_context *ctx, stack_t *s, int scale,
		 double x, double y)
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

    gerbv_draw_prim6(ctx, s, scale, x, y);

    s->stack = old_stack;

} /* gerbv_draw_prim7 */


/*
 * Doesn't handle exposure yet and explicit x,y
 */
static void 
gerbv_draw_prim20(struct gerb_render_context *ctx, stack_t *s, int scale,
		  double x, double y)
{
    const int linewidth_idx = 1;
    const int start_x_idx = 2;
    const int start_y_idx = 3;
    const int end_x_idx = 4;
    const int end_y_idx = 5;
    const int rotation_idx = 6;
    const int nuf_points = 2;
    double points[2*nuf_points];
    int i;

    ctx->set_line_style (ctx, scale * s->stack[linewidth_idx], 0, 0);

    points[0] = s->stack[start_x_idx] * scale;
    points[1] = s->stack[start_y_idx] * scale;
    points[2] = s->stack[end_x_idx] * scale;
    points[3] = s->stack[end_y_idx] * scale;

    for (i = 0; i < nuf_points; i++) {
	rotate_point(&points[2*i], &points[2*i+1], s->stack[rotation_idx]);
	points[2*i] = x + points[2*i];
	points[2*i+1] = y - points[2*i+1];
    }

    ctx->draw_line (ctx, points[0], points[1], points[2], points[3]);

} /* gerbv_draw_prim20 */


/*
 * Doesn't handle exposure yet and explicit x,y
 */
static void
gerbv_draw_prim21(struct gerb_render_context *ctx, stack_t *s, int scale,
		  double x, double y)
{
    const int width_idx = 1;
    const int height_idx = 2;
    const int rotation_idx = 5;
    const int nuf_points = 4;
    double points[2*nuf_points];
    int half_width, half_height;
    int i;

    half_width = s->stack[width_idx] * scale / 2.0;
    half_height = s->stack[height_idx] * scale / 2.0;

    points[0] = half_width;
    points[1] = half_height;
    points[2] = half_width;
    points[3] = -half_height;
    points[4] = -half_width;
    points[5] = -half_height;
    points[6] = -half_width;
    points[7] = half_height;

    for (i=0; i<nuf_points; i++) {
	rotate_point(&points[2*i], &points[2*i+1], s->stack[rotation_idx]);
	points[2*i]   += x;
	points[2*i+1] += y;
    }

    ctx->fill_polygon (ctx, points, nuf_points);

} /* gerbv_draw_prim21 */


static void 
gerbv_draw_prim22(struct gerb_render_context *ctx, stack_t *s, int scale,
		  double x, double y)
{
    const int width_idx = 1;
    const int height_idx = 2;
    const int x_lower_left_idx = 3;
    const int y_lower_left_idx = 4;
    const int rotation_idx = 5;
    const int nuf_points = 4;
    double points[2*nuf_points];
    int i;

    points[0] = s->stack[x_lower_left_idx] * scale;
    points[1] = s->stack[y_lower_left_idx] * scale;
    points[2] = (s->stack[x_lower_left_idx] + s->stack[width_idx]) * scale;
    points[3] = s->stack[y_lower_left_idx] * scale;
    points[4] = (s->stack[x_lower_left_idx] + s->stack[width_idx]) * scale;
    points[5] = (s->stack[y_lower_left_idx] - s->stack[height_idx]) * scale;
    points[6] = s->stack[x_lower_left_idx] * scale;
    points[7] = (s->stack[y_lower_left_idx] - s->stack[height_idx]) * scale;

    for (i=0; i<nuf_points; i++) {
	rotate_point(&points[2*i], &points[2*i+1], s->stack[rotation_idx]);
	points[2*i]   += x;
	points[2*i+1] += y;
    }

    ctx->fill_polygon (ctx, points, nuf_points);

} /* gerbv_draw_prim22 */


int 
gerbv_draw_amacro(struct gerb_render_context *ctx,
		  instruction_t *program, unsigned int nuf_push,
		  double *parameters, int scale, double x, double y)
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
	    switch(ip->data.ival) {
	    case 1:
		gerbv_draw_prim1 (ctx, s, scale, x, y);
		break;
	    case 4 :
		gerbv_draw_prim4 (ctx, s, scale, x, y);
		break;
	    case 5 :
		gerbv_draw_prim5 (ctx, s, scale, x, y);
		break;
	    case 6 :
		gerbv_draw_prim6 (ctx, s, scale, x, y);
		break;
	    case 7 :
		gerbv_draw_prim7 (ctx, s, scale, x, y);
		break;
	    case 2  :
	    case 20 :
		gerbv_draw_prim20 (ctx, s, scale, x, y);
		break;
	    case 21 :
		gerbv_draw_prim21 (ctx, s, scale, x, y);
		break;
	    case 22 :
		gerbv_draw_prim22 (ctx, s, scale, x, y);
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

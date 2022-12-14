/*
 * gEDA - GNU Electronic Design Automation
 * This is a part of gerbv
 *
 *   Copyright (C) 2022 Stephen J. Hardy
 * Based on draw.c:
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

/** \file search.c
    \brief Iterate through data structures, for searching and selection.
    \ingroup libgerbv
*/


#include <stdio.h>
#include <stdlib.h>
#include <math.h>  /* ceil(), atan2() */

#ifdef HAVE_STRING_H
# include <string.h>
#endif

#include "gerbv.h"
#include "common.h"
#include "selection.h"

#include "search.h"

#define dprintf if(DEBUG) printf

static void
_update_exposure(search_state_t *ss, gdouble exp)
{
        switch ((int)exp) {
        case 2:
                ss->clear[ss->stack] = !ss->clear[ss->stack];
                break;
        case 1:
                ss->clear[ss->stack] = TRUE;
                break;
        default:
                ss->clear[ss->stack] = FALSE;
                break;
        }
}

#define _translate(ss, dx, dy)  cairo_matrix_translate(ss->transform+ss->stack, dx, dy)
#define _scale(ss, dx, dy)  cairo_matrix_scale(ss->transform+ss->stack, dx, dy)
#define _rotate(ss, ccw_degrees)  cairo_matrix_rotate(ss->transform+ss->stack, DEG2RAD(ccw_degrees))
#define _rotate_xy(ss, dx, dy)  cairo_matrix_rotate(ss->transform+ss->stack, atan2(dy, dx))

static void
_push(search_state_t *ss)
{
        g_assert(ss->stack+1 < SEARCH_MAX_XFORM && ss->stack >= 0);
        ++ss->stack;
        ss->clear[ss->stack] = ss->clear[ss->stack-1];
        ss->transform[ss->stack] = ss->transform[ss->stack-1];
}

static void
_pop(search_state_t *ss)
{
        g_assert(ss->stack > 0);
        --ss->stack;
}

static void
_circle(search_state_t *ss, gdouble diam)
{
        // Invoke callback on circle.
        ss->dx = diam*0.5;
        ss->callback(ss, SEARCH_CIRCLE);
}

static void
_ring(search_state_t *ss, gdouble od, gdouble id)
{
        // Invoke callback on ring i.e. an outer and inner diameter.
        ss->dx = od*0.5;
        ss->dy = id*0.5;
        ss->callback(ss, SEARCH_RING);
}

static void
_rectangle(search_state_t *ss, gdouble w, gdouble h)
{
        // Lower left at origin, w along x and h along y.
        // Invoke callback on rectangle.
        ss->dx = w;
        ss->dy = h;
        ss->callback(ss, SEARCH_RECTANGLE);
}

static void
_obround(search_state_t *ss, gdouble x, gdouble hlw)
{
        // Geometry same as track (below) but different code since this is a flash.
        ss->dx = x;
        ss->hlw = hlw;
        ss->callback(ss, SEARCH_OBROUND);
}

static void
_track(search_state_t *ss, gdouble x, gdouble hlw)
{
        // Stroked track with rounded ends.  Left center at (0,0)
        // right center at (x,0).  Half linewidth is hlw.
        ss->dx = x;
        ss->hlw = hlw;
        ss->callback(ss, SEARCH_TRACK);
}


static void
_move_to(search_state_t *ss, gdouble x, gdouble y)
{
        // Unlike cairo_move_to, this obliterates any existing path.  Use for one poly at a time.
        ss->polygon->len = 1;
        ((vertex_t *)ss->polygon->data)[0].x = x;
        ((vertex_t *)ss->polygon->data)[0].y = y;
}

static void
_line_to(search_state_t *ss, gdouble x, gdouble y)
{
        guint n = ss->polygon->len;
        g_array_set_size(ss->polygon, n + 1);
        ((vertex_t *)ss->polygon->data)[n].x = x;
        ((vertex_t *)ss->polygon->data)[n].y = y;
}

static void
_polygon(search_state_t *ss)
{
        // Invoke callback on (irregular) filled polygon data in ss->polygon.
        ss->poly = (vertex_t *)ss->polygon->data;
        ss->len = ss->polygon->len;
        ss->callback(ss, SEARCH_POLYGON);
}

static void
_polygon_track(search_state_t *ss, gdouble hlw)
{
        // Invoke callback on (irregular) polygon track data in ss->polygon.
        ss->hlw = hlw;
        ss->poly = (vertex_t *)ss->polygon->data;
        ss->len = ss->polygon->len;
        ss->callback(ss, SEARCH_POLYGON);
}

static void
_regular_polygon(search_state_t *ss, gdouble circum_diam, gdouble sides)
{
        // Invoke callback on regular n-gon.  First point at (x,y) = (circum_diam/2, 0)
        // For simplicity, expand this into a normal polygon, so that callback does not have to
        // handle special case.  For anything beyond octagon or less than triangle, treat as
        // if it was a circle.
        int n = sides, i;
        gdouble r = circum_diam*0.5;
        if (n > 8 || n < 3)
                _circle(ss, circum_diam);
        _move_to(ss, r, 0.);
        for (i = 1; i < n; ++i)
                _line_to(ss, r * cos(M_PI*2.*i/n), r * sin(M_PI*2.*i/n));
        _polygon(ss);
}

static void
_arc (search_state_t *ss, gdouble cp_x, gdouble cp_y, gdouble r, gdouble a1_rad, gdouble a2_rad, gdouble dir)
{
        gdouble da = fabs(a2_rad - a1_rad);
        // Use 1/20 rad (~3 deg) polygon approximation.  OK for selection etc.
        int n = ceil(da * 20.);
        int i;
        
        da = (a2_rad - a1_rad) / n;
        for (i = 0; i < n; ++i) {
                a1_rad += da;
                _line_to(ss, cp_x + r*cos(a1_rad), cp_y + r*sin(a1_rad));
        }
}


static void
_amacro(search_state_t *ss, gerbv_simplified_amacro_t *s)
{
	gerbv_simplified_amacro_t *ls = s;

	dprintf("Drawing simplified aperture macros:\n");

	// Push the current state.
	_push(ss);

	while (ls != NULL) {

		dprintf("\t%s(): drawing %s\n", __FUNCTION__,
				gerbv_aperture_type_name(ls->type));

		switch (ls->type) {

		case GERBV_APTYPE_MACRO_CIRCLE:
			_update_exposure (ss, ls->parameter[CIRCLE_EXPOSURE]);
			_translate (ss, ls->parameter[CIRCLE_CENTER_X],
					ls->parameter[CIRCLE_CENTER_Y]);
			_circle (ss, ls->parameter[CIRCLE_DIAMETER]);
			break;

		case GERBV_APTYPE_MACRO_OUTLINE:
			_update_exposure (ss, ls->parameter[OUTLINE_EXPOSURE]);
			_rotate (ss, ls->parameter[OUTLINE_ROTATION_IDX(ls->parameter)]);
			_move_to (ss, ls->parameter[OUTLINE_FIRST_X],
					ls->parameter[OUTLINE_FIRST_Y]);

			for (int point = 1; 
			     point < 1 + (int)ls->parameter[OUTLINE_NUMBER_OF_POINTS];
			     ++point) {
				_line_to (ss, ls->parameter[OUTLINE_X_IDX_OF_POINT(point)],
					ls->parameter[OUTLINE_Y_IDX_OF_POINT(point)]);
			}

			_polygon (ss);
			break;

		case GERBV_APTYPE_MACRO_POLYGON:
			_update_exposure (ss, ls->parameter[POLYGON_EXPOSURE]);
			_translate (ss, ls->parameter[POLYGON_CENTER_X],
					ls->parameter[POLYGON_CENTER_Y]);
			_rotate (ss, ls->parameter[POLYGON_ROTATION]);
			_regular_polygon(ss,
					ls->parameter[POLYGON_DIAMETER],
					ls->parameter[POLYGON_NUMBER_OF_POINTS]);
			break;

		case GERBV_APTYPE_MACRO_MOIRE:
			_translate (ss, ls->parameter[MOIRE_CENTER_X],
					ls->parameter[MOIRE_CENTER_Y]);
			_circle (ss, ls->parameter[MOIRE_OUTSIDE_DIAMETER]);
			break;

		case GERBV_APTYPE_MACRO_THERMAL:
			_translate (ss, ls->parameter[THERMAL_CENTER_X],
					ls->parameter[THERMAL_CENTER_Y]);
			_ring (ss, ls->parameter[THERMAL_OUTSIDE_DIAMETER],
			           ls->parameter[THERMAL_INSIDE_DIAMETER]);
			break;

		case GERBV_APTYPE_MACRO_LINE20: {
		        gdouble hlw = 0.5*ls->parameter[LINE20_LINE_WIDTH];
		        gdouble dx = ls->parameter[LINE20_END_X] - ls->parameter[LINE20_START_X];
		        gdouble dy = ls->parameter[LINE20_END_Y] - ls->parameter[LINE20_START_Y];
			_update_exposure (ss, ls->parameter[LINE20_EXPOSURE]);
			//search_set_line_cap (ss, CAIRO_LINE_CAP_BUTT);
			_rotate (ss, ls->parameter[LINE20_ROTATION]);
			_translate (ss, ls->parameter[LINE20_START_X],
			                ls->parameter[LINE20_START_Y]);
			_rotate_xy (ss, dx, dy);
			_translate (ss, -hlw, 0.);
			_rectangle (ss, hypot(dx, dy), hlw*2);
			break;
                }
		case GERBV_APTYPE_MACRO_LINE21:
			_update_exposure (ss, ls->parameter[LINE21_EXPOSURE]);
			_rotate (ss, ls->parameter[LINE21_ROTATION]);
			_translate (ss, ls->parameter[LINE21_CENTER_X] + ls->parameter[LINE21_WIDTH]/2.,
					ls->parameter[LINE21_CENTER_Y] + ls->parameter[LINE21_HEIGHT]/2.);
			_rectangle (ss, ls->parameter[LINE21_WIDTH], ls->parameter[LINE21_HEIGHT]);
			break;

		case GERBV_APTYPE_MACRO_LINE22:
			_update_exposure (ss, ls->parameter[LINE22_EXPOSURE]);
			_rotate (ss, ls->parameter[LINE22_ROTATION]);
			_translate (ss,
					ls->parameter[LINE22_LOWER_LEFT_X],
					ls->parameter[LINE22_LOWER_LEFT_Y]);
			_rectangle (ss, ls->parameter[LINE22_WIDTH], ls->parameter[LINE22_HEIGHT]);
			break;

		default:
			break;
		}

                // Restore to transform on entry.
                _pop(ss);
                _push(ss);
                
		ls = ls->next;
	}
	
	_pop(ss);
}

static void
_apply_netstate_transformation (search_state_t *ss, gerbv_netstate_t *state)
{
	/* apply scale factor */
	_scale (ss, state->scaleA, state->scaleB);
	/* apply offset */
	_translate (ss, state->offsetA, state->offsetB);
	/* apply mirror */
	switch (state->mirrorState) {
	case GERBV_MIRROR_STATE_FLIPA:
		_scale (ss, -1, 1);
		break;
	case GERBV_MIRROR_STATE_FLIPB:
		_scale (ss, 1, -1);
		break;
	case GERBV_MIRROR_STATE_FLIPAB:
		_scale (ss, -1, -1);
		break;
	default:
		break;
	}
	/* finally, apply axis select */
	if (state->axisSelect == GERBV_AXIS_SELECT_SWAPAB) {
		/* we do this by rotating 270 (counterclockwise, then mirroring
		   the Y axis */
		_rotate (ss, 270.);
		_scale (ss, 1, -1);
	}
}

static void
_polygon_object (search_state_t *ss)
{
        gerbv_net_t *oldNet = ss->net;
	gerbv_net_t *currentNet;
	gboolean haveDrawnFirstFillPoint = FALSE;
	gdouble x2,y2,cp_x=0,cp_y=0;

	for (currentNet = oldNet->next; currentNet!=NULL;
			currentNet = currentNet->next) {
		x2 = currentNet->stop_x;
		y2 = currentNet->stop_y;

		/* translate circular x,y data as well */
		if (currentNet->cirseg) {
			cp_x = currentNet->cirseg->cp_x;
			cp_y = currentNet->cirseg->cp_y;
		}
		if (!haveDrawnFirstFillPoint) {
			_move_to (ss, x2, y2);
			haveDrawnFirstFillPoint=TRUE;
			continue;
		}

		switch (currentNet->interpolation) {
		case GERBV_INTERPOLATION_LINEARx1 :
		case GERBV_INTERPOLATION_LINEARx10 :
		case GERBV_INTERPOLATION_LINEARx01 :
		case GERBV_INTERPOLATION_LINEARx001 :
			_line_to (ss, x2, y2);
			break;
		case GERBV_INTERPOLATION_CW_CIRCULAR :
		case GERBV_INTERPOLATION_CCW_CIRCULAR :
			_arc (ss, cp_x, cp_y, currentNet->cirseg->width/2.0,
				DEG2RAD(currentNet->cirseg->angle1),
				DEG2RAD(currentNet->cirseg->angle2),
				currentNet->cirseg->angle2 > currentNet->cirseg->angle1 ? 1. : -1.);
			break;
		case GERBV_INTERPOLATION_PAREA_END :
			_polygon (ss);
			return;
		default :
			break;
		}
	}
}


static int
_run (search_state_t *ss)
{

	double x1, y1, x2, y2, cp_x=0, cp_y=0;
	gdouble *p, p0, p1, hlw, r, a1, a2;
	gerbv_netstate_t *oldState;
	gerbv_layer_t *oldLayer;




	/* do initial justify */
	_translate (ss, ss->image->info->imageJustifyOffsetActualA,
		 ss->image->info->imageJustifyOffsetActualB);

	/* offset ss->image */
	_translate (ss, ss->image->info->offsetA, ss->image->info->offsetB);
	/* do ss->image rotation */
	_rotate (ss, RAD2DEG(ss->image->info->imageRotation));


	/* next, push two search states to simulate the first layer and netstate
	   translations (these will be popped when another layer or netstate is
	   started */
	_push (ss);
	_push (ss);

	/* store the current layer and netstate so we know when they change */
	oldLayer = ss->image->layers;
	oldState = ss->image->states;


	for (ss->net = ss->image->netlist->next; ss->net != NULL;
			ss->net = gerbv_image_return_next_renderable_object(ss->net)) {

		/* check if this is a new layer */
		if (ss->net->layer != oldLayer){
			/* it's a new layer, so recalculate the new transformation matrix
			   for it */
			_pop (ss);
			_pop (ss);
			_push (ss);
			/* do any rotations */
			_rotate (ss, RAD2DEG(ss->net->layer->rotation));

			/* Finally, reapply old netstate transformation */
			_push (ss);
			_apply_netstate_transformation (ss, ss->net->state);
			oldLayer = ss->net->layer;
		}

		/* check if this is a new netstate */
		if (ss->net->state != oldState){
			/* pop the transformation matrix back to the "pre-state" state and
			   resave it */
			_pop (ss);
			_push (ss);
			/* it's a new state, so recalculate the new transformation matrix
			   for it */
			_apply_netstate_transformation (ss, ss->net->state);
			oldState = ss->net->state;
		}

                ss->aperture = ss->image->aperture[ss->net->aperture];

		x1 = ss->net->start_x;
		y1 = ss->net->start_y;
		x2 = ss->net->stop_x;
		y2 = ss->net->stop_y;

		/* translate circular x,y data as well */
		if (ss->net->cirseg) {
			cp_x = ss->net->cirseg->cp_x;
			cp_y = ss->net->cirseg->cp_y;
		}

		/* Polygon area fill routines */
		switch (ss->net->interpolation) {
		case GERBV_INTERPOLATION_PAREA_START :

				_polygon_object (ss);

			continue;
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
		if (!ss->aperture)
			continue;

		switch (ss->net->aperture_state) {
		case GERBV_APERTURE_STATE_ON :
			hlw = ss->aperture->parameter[0] * 0.5;

			switch (ss->net->interpolation) {
			case GERBV_INTERPOLATION_LINEARx1 :
			case GERBV_INTERPOLATION_LINEARx10 :
			case GERBV_INTERPOLATION_LINEARx01 :
			case GERBV_INTERPOLATION_LINEARx001 :
				//search_set_line_cap (ss, CAIRO_LINE_CAP_ROUND);

				switch (ss->aperture->type) {
				case GERBV_APTYPE_RECTANGLE :
				        hlw = MIN(hlw, ss->aperture->parameter[1] * 0.5);
					// Fall through...
				case GERBV_APTYPE_CIRCLE :
				        _push(ss);
				        _translate(ss, x1, y1);
				        _rotate_xy(ss, x2-x1, y2-y1);
				        _track(ss, hypot(x2-x1, y2-y1), hlw);
                                        _pop(ss);
					break;
				/* macros can only be flashed, so ignore any that might be here,
				   and X2 standard prohibits painting with polygon or oval apertures,
				   so ignore this (impossibly rare) case. */
				default:
					break;
				}
				break;
			case GERBV_INTERPOLATION_CW_CIRCULAR :
			case GERBV_INTERPOLATION_CCW_CIRCULAR :
			        r = ss->net->cirseg->width * 0.5;
			        a1 = DEG2RAD(ss->net->cirseg->angle1);
			        a2 = DEG2RAD(ss->net->cirseg->angle2);
			        _move_to(ss, cp_x + r*cos(a1), cp_y + r*sin(a2)); 
			        _arc (ss, cp_x, cp_y, r, a1, a2, a2 > a1 ? 1. : -1.);
			        _polygon_track(ss, hlw);
				break;
			default :
				break;
			}
			break;
		case GERBV_APERTURE_STATE_OFF :
			break;
		case GERBV_APERTURE_STATE_FLASH :
			p = ss->aperture->parameter;
			p0 = p[0];
			p1 = p[1];

			_push (ss);
			_translate(ss, x2, y2);

			switch (ss->aperture->type) {
			case GERBV_APTYPE_CIRCLE :
                                if (p1 > 0.)
                                        _ring(ss, p0, p1);
                                else
				        _circle(ss, p0);
				break;
			case GERBV_APTYPE_RECTANGLE :
			        _translate(ss, -p0*0.5, -p1*0.5);
				_rectangle(ss, p0, p1);
				break;
			case GERBV_APTYPE_OVAL :
			        // Treat it like a short "track" with rounded ends.
			        if (p0 >= p1) {
			                _translate(ss, (p1-p0)*0.5, 0.);
			                _obround(ss, p0-p1, p1*0.5);
			        }
			        else {
			                _translate(ss, 0, (p1-p0)*0.5);
			                _rotate(ss, -90.);
			                _obround(ss, p1-p0, p0*0.5);
			        }
				break;
			case GERBV_APTYPE_POLYGON :
			        _rotate(ss, p[2]);
				_regular_polygon(ss, p0, p1);
				break;
			case GERBV_APTYPE_MACRO :
				_amacro(ss, ss->aperture->simplified);
				break;
			default :
				break;
			}

			_pop (ss);
			break;
		default:
			break;
		}
	}

	/* restore the initial two state saves (one for layer, one for netstate)*/
	_pop (ss);
	_pop (ss);

	return 1;
}


// Returns a number
//  <0 if testpoint is to the left of line segment (linestart..lineend)
//  ==0 if on the line (to numerical precision)
//  >0 if testpoint is on the right.
static gdouble
_side_of_line(const vertex_t * linestart, const vertex_t * lineend, const vertex_t * testpoint)
{
    return ( (lineend->x - linestart->x) * (testpoint->y - linestart->y)
            - (testpoint->x -  linestart->x) * (lineend->y - linestart->y) );
}

// Test whether testpoint is inside the polygon (with n vertices), using the non-zero winding rule.
// This is an adaptation of code with the following copyright notice:
/*
// Copyright 2000 softSurfer, 2012 Dan Sunday
// This code may be freely used and modified for any purpose
// providing that this copyright notice is included with it.
// SoftSurfer makes no warranty for this code, and cannot be held
// liable for any real or imagined damage resulting from its use.
// Users of this code must verify correctness for their application.
*/
// SJH - modified for C convention, and handle closing segment differently.
static gboolean
_point_in_polygon(const vertex_t * testpoint, const vertex_t * V, unsigned n)
{
    unsigned i, j;
    int wn = 0;    // the  winding number counter

    // loop through all edges of the polygon
    for (i=0; i<n; ++i) {   // edge from V[i] to  V[i+1 mod n]
        // Set j to successor vertex.
        j = i+1;
        if (j == n)
                j = 0;
        if (V[i].y <= testpoint->y) {           // start y <= P.y
            if (V[j].y  > testpoint->y)         // an upward crossing
                 if (_side_of_line(V+i, V+j, testpoint) > 0.)  // P left of  edge
                     ++wn;                      // have  a valid up intersect
        }
        else {                                  // start y > P.y (no test needed)
            if (V[j].y  <= testpoint->y)        // a downward crossing
                 if (_side_of_line(V+i, V+j, testpoint) < 0.)  // P right of  edge
                     --wn;                      // have  a valid down intersect
        }
    }
    
    // We have the winding count, but only interested in whether it is non-zero.
    return wn != 0;
}


void point_on_line_at_parameter(vertex_t * pt, const vertex_t * p0, const vertex_t * p1, double u)
{
    pt->x = p0->x*(1.-u) + p1->x*u;
    pt->y = p0->y*(1.-u) + p1->y*u;
}
double 
_point_to_line_parameter(const vertex_t * pt, const vertex_t * p0, const vertex_t * p1)
{
    // Return parameter relative to line segment p0->p1 for point on that line closest to pt.
    // If segment smaller than 1e-4, then take distance to midpoint to avoid numerical instability.
    vertex_t d, e;
    gdouble len2;
    d.x = p1->x - p0->x;
    d.y = p1->y - p0->y;
    len2 = d.x*d.x + d.y*d.y;
    if (len2 < 1.e-8)
        return 0.5;
    e.x = pt->x - p0->x;
    e.y = pt->y - p0->y;
    return (e.x*d.x + e.y*d.y)/len2;
}
gdouble 
_distance_point_to_line_segment(const vertex_t * pt, const vertex_t * p0, const vertex_t * p1)
{
    // Distance from point pt to line segment p0->p1, and return it.
    double u = _point_to_line_parameter(pt, p0, p1);
    vertex_t s;
    if (u <= 0.)
        s = *p0;
    else if (u >= 1.)
        s = *p1;
    else
        point_on_line_at_parameter(&s, p0, p1, u);
    return hypot(pt->x - s.x, pt->y - s.y);
}

static gdouble
_distance_to_open_polygon(const vertex_t * testpoint, const vertex_t * V, unsigned n)
{
        gdouble r = HUGE_VAL, s;
        unsigned i;
        
        for (i = 0; i+1 < n; ++i) {
                s = _distance_point_to_line_segment(testpoint, V+i, V+(i+1));
                r = MIN(r, s);
        }
        return r;
}

static gdouble
_distance_to_closed_polygon(const vertex_t * testpoint, const vertex_t * V, unsigned n)
{
        gdouble r = HUGE_VAL, s;
        unsigned i, j;
        
        for (i = 0; i < n; ++i) {
                // Set j to successor vertex.
                j = i+1;
                if (j == n)
                        j = 0;
                s = _distance_point_to_line_segment(testpoint, V+i, V+j);
                r = MIN(r, s);
        }
        return r;
}

gdouble
search_distance_to_border(search_state_t * ss, search_context_t ctx, const vertex_t * vtx)
{
        // This is the meat of the distance to border callback(s).  Return signed distance
        // from vtx to the closest point on the border of the current object described
        // by ss with context.  Return HUGE_VAL for bogus.
        // First, we need to inverse transform the point.
        //TODO: efficiency could be improved if we track and cache the matrix inverse.
        cairo_matrix_t m = ss->transform[ss->stack];
        gdouble x = vtx->x;
        gdouble y = vtx->y;
        gdouble r, rx, ry;
        gboolean inside;
        vertex_t v;
        
        if (cairo_matrix_invert(&m) == CAIRO_STATUS_INVALID_MATRIX)
                return HUGE_VAL;
        cairo_matrix_transform_point(&m, &x, &y);       
        switch (ctx) {
        case SEARCH_CIRCLE:
                r = hypot(x, y);
                return r - ss->dx;
        case SEARCH_RING:
                r = hypot(x, y);
                return MAX(r - ss->dx, ss->dy - r);
        case SEARCH_RECTANGLE:
                rx = MAX(x - ss->dx, -x);
                ry = MAX(y - ss->dy, -y);
                if (rx > 0. && ry > 0.)
                        return hypot(rx, ry);
                return MAX(rx, ry);
        case SEARCH_OBROUND:
        case SEARCH_TRACK:
                if (x >= 0. && x <= ss->dx)
                        return MAX(y - ss->hlw, -ss->hlw - y);
                if (x < 0.)
                        return hypot(x, y) - ss->hlw;
                return hypot(x - ss->dx, y) - ss->hlw;
        
        // In both cases, first compute min. distance to centerline, disregarding linewidth.
        // Then, for polygon, find whether inside or outside: if inside, return -ve dist, else +ve.
        // For poly track, take abs value of dist then subtract hlw.
        // To compute CL distance, iterate all segments (plus an implicit close for polygon).
        // For each seg, update matrix (and inverse) to canonical X aligned form and recurse
        // as for rectangle, with dy=0 (thin line).
        case SEARCH_POLYGON:
                v.x = x;
                v.y = y;
                inside = _point_in_polygon(&v, ss->poly, ss->len);
                r = _distance_to_closed_polygon(&v, ss->poly, ss->len);
                return inside ? -r : r;
        case SEARCH_POLY_TRACK:
                v.x = x;
                v.y = y;
                r = _distance_to_open_polygon(&v, ss->poly, ss->len) - ss->hlw;
                return r;
        default:
                return HUGE_VAL;
        }
}

gdouble
search_distance_to_border_no_transform(search_state_t * ss, search_context_t ctx, gdouble x, gdouble y)
{
        // Same as above, but for when transform already done on x,y coordinates.
        gdouble r, rx, ry;
        gboolean inside;
        vertex_t v;
        
        switch (ctx) {
        case SEARCH_CIRCLE:
                r = hypot(x, y);
                return r - ss->dx;
        case SEARCH_RING:
                r = hypot(x, y);
                return MAX(r - ss->dx, ss->dy - r);
        case SEARCH_RECTANGLE:
                rx = MAX(x - ss->dx, -x);
                ry = MAX(y - ss->dy, -y);
                if (rx > 0. && ry > 0.)
                        return hypot(rx, ry);
                return MAX(rx, ry);
        case SEARCH_OBROUND:
        case SEARCH_TRACK:
                if (x >= 0. && x <= ss->dx)
                        return MAX(y - ss->hlw, -ss->hlw - y);
                if (x < 0.)
                        return hypot(x, y) - ss->hlw;
                return hypot(x - ss->dx, y) - ss->hlw;
        
        // In both cases, first compute min. distance to centerline, disregarding linewidth.
        // Then, for polygon, find whether inside or outside: if inside, return -ve dist, else +ve.
        // For poly track, take abs value of dist then subtract hlw.
        // To compute CL distance, iterate all segments (plus an implicit close for polygon).
        // For each seg, update matrix (and inverse) to canonical X aligned form and recurse
        // as for rectangle, with dy=0 (thin line).
        case SEARCH_POLYGON:
                v.x = x;
                v.y = y;
                inside = _point_in_polygon(&v, ss->poly, ss->len);
                r = _distance_to_closed_polygon(&v, ss->poly, ss->len);
                return inside ? -r : r;
        case SEARCH_POLY_TRACK:
                v.x = x;
                v.y = y;
                r = _distance_to_open_polygon(&v, ss->poly, ss->len) - ss->hlw;
                return r;
        default:
                return HUGE_VAL;
        }
}


/* User data used by _dist_from_border() callback

        The inclusion criteria are that the point is either within the object,
        or not beyond the "not_over" distance.
        
        When sorting the results, the absolute values are used, rather than the signed
        distances.  If we did not do that, then a large polygon area would always "win"
        if we are inside it yet far from the border (large -ve distance), even though
        we were right near a smaller object inside, like a via.
        
        So the large region is kept in contention, but "the" selection is whichever
        object has the closest visible border.
*/
typedef struct dfb
{
        GArray *        nets;           // Array of search_result_t (net pointer and dist).
        gdouble         not_over;       // Discard any farther than this.
        vertex_t        board;          // Point we are comparing with
} dfb_t;

static void
_dist_from_border(search_state_t * ss, search_context_t ctx)
{
        dfb_t * d = (dfb_t *)ss->user_data;
        gdouble dist = search_distance_to_border(ss, ctx, &d->board);
        // dist is -ve if we are inside it, so will always include enclosing objects.
        if (dist <= d->not_over) {
                search_result_t * sr = (search_result_t *)d->nets->data + d->nets->len;
                g_array_set_size(d->nets, d->nets->len+1);
                sr->net = ss->net;
                sr->dist = dist;
        }
}

static gint
_dist_from_border_compare(gconstpointer a, gconstpointer b, gpointer user_data)
{
        const search_result_t * ra = (const search_result_t *)a;
        const search_result_t * rb = (const search_result_t *)b;
        // Penalize tracks w.r.t. flashes, since normally want to focus on the pad not the track
        // leading up to it.  Use a 20 mil penalty.
        int track_penalty_a = ra->net->aperture_state == GERBV_APERTURE_STATE_FLASH ? 0 : 200;
        int track_penalty_b = rb->net->aperture_state == GERBV_APERTURE_STATE_FLASH ? 0 : 200;
        // Need to multiply the result up so it's an integer.  Use 10k so as to resolve to 0.1 mil.
        // Compare on absolute distances from the border, otherwise would always pick the
        // largest object we were most inside of, which is not useful. 
        return (gint)((fabs(ra->dist) - fabs(rb->dist))*10000.) + (track_penalty_a - track_penalty_b);
}

/******************
  Library API
  
        Traverse objects and invoke callback.  
    
*******************/




search_state_t *
search_create_search_state_for_image(gerbv_image_t * image)
{
        search_state_t * ss = (search_state_t *)g_new0(search_state_t, 1);
        ss->image = image;
        cairo_matrix_init_identity(ss->transform);      // First level
        // Allocate for 128 vertices, will expand automatically.
        ss->polygon = g_array_sized_new(FALSE, FALSE, sizeof(vertex_t), 128);
        return ss;
}

void
search_destroy_search_state(search_state_t * ss)
{
        if (ss) {
                g_array_free(ss->polygon, TRUE);
                g_free(ss);
        }
}

search_state_t *
search_init_search_state_for_image(search_state_t * ss, gerbv_image_t * image)
{
        if (!ss)
                ss = search_create_search_state_for_image(image);
        else
                ss->image = image;
        ss->stack = 0;
        ss->clear[0] = FALSE;
        cairo_matrix_init_identity(ss->transform);
        return ss;
}

search_state_t *
search_image(search_state_t * ss, gerbv_image_t * image, search_callback_t cb, gpointer user_data)
{
        ss = search_init_search_state_for_image(ss, image);
        ss->callback = cb;
        ss->user_data = user_data;
        _run(ss);        
        return ss;
}


GArray *
search_image_for_closest_to_border(gerbv_image_t * image, gdouble boardx, gdouble boardy, int nclosest, gdouble not_over)
{
        dfb_t d;
        
        search_state_t * ss = search_init_search_state_for_image(NULL, image);
        ss->callback = _dist_from_border;
        ss->user_data = &d;
        d.nets = g_array_sized_new(FALSE, FALSE, sizeof(search_result_t), 128);
        d.not_over = not_over;
        d.board.x = boardx;
        d.board.y = boardy;
        _run(ss);   
        // Sort the result by increasing border distance, then truncate to requested number of results.
        g_array_sort_with_data(d.nets, _dist_from_border_compare, ss);
        g_array_set_size(d.nets, MIN((guint)nclosest, d.nets->len));
        search_destroy_search_state(ss);
        return d.nets;
}





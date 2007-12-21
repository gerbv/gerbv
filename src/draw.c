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

#include "draw.h"
#include "gerb_error.h"
#include <cairo.h>

/*
 * Stack declarations and operations to be used by the simple engine that
 * executes the parsed aperture macros.
 */
typedef struct {
	double *stack;
	int sp;
} macro_stack_t;


static macro_stack_t *
new_stack(unsigned int nuf_push)
{
	const int extra_stack_size = 10;
	macro_stack_t *s;

	s = (macro_stack_t *)g_malloc(sizeof(macro_stack_t));
	if (!s) {
		g_free(s);
		return NULL;
	}
	memset(s, 0, sizeof(macro_stack_t));

	s->stack = (double *)g_malloc(sizeof(double) * (nuf_push + extra_stack_size));
	if (!s->stack) {
		g_free(s->stack);
		return NULL;
	}

	memset(s->stack, 0, sizeof(double) * (nuf_push + extra_stack_size));
	s->sp = 0;

	return s;
} /* new_stack */


static void
free_stack(macro_stack_t *s)
{
	if (s && s->stack)
		g_free(s->stack);

	if (s)
		g_free(s);

	return;
} /* free_stack */


static void
push(macro_stack_t *s, double val)
{
	s->stack[s->sp++] = val;
	return;
} /* push */


static double
pop(macro_stack_t *s)
{
	return s->stack[--s->sp];
} /* pop */



/*
 * Draws a circle _centered_ at x,y with diameter dia
 */
static void 
gerbv_draw_circle(cairo_t *cairoTarget, gdouble diameter)
{
	cairo_arc (cairoTarget, 0, 0, diameter/2.0, 0, 2.0*M_PI);
	return;
}

/*
 * Draws a rectangle _centered_ at x,y with sides x_side, y_side
 */
static void
gerbv_draw_rectangle(cairo_t *cairoTarget, gdouble width, gdouble height)
{
	cairo_rectangle (cairoTarget, - width / 2.0, - height / 2.0, width, height);
	return;
}

/*
 * Draws an oval _centered_ at x,y with x axis x_axis and y axis y_axis
 */ 
static void
gerbv_draw_oval(cairo_t *cairoTarget, gdouble width, gdouble height)
{
	/* cairo doesn't have a function to draw ovals, so we must
	 * draw an arc and stretch it by scaling different x and y values
	 */
	cairo_save (cairoTarget);
	cairo_scale (cairoTarget, width, height);
	gerbv_draw_circle (cairoTarget, 1);
	cairo_restore (cairoTarget);
	return;
}

/*
 * Draws an oval _centered_ at x,y with x axis x_axis and y axis y_axis
 */ 
static void
gerbv_draw_polygon(cairo_t *cairoTarget, gdouble outsideRadius,
		gdouble numberOfSides, gdouble degreesOfRotation)
{
	int i, numberOfSidesInteger = (int) numberOfSides;
	
	cairo_rotate (cairoTarget, degreesOfRotation * M_PI/180);
	cairo_move_to (cairoTarget, outsideRadius, 0);
	/* skip first point, since we've moved there already */
	for (i=1; i< (int)numberOfSidesInteger; i++){
		gdouble angle = (double) i / numberOfSidesInteger * M_PI * 2;
		cairo_line_to (cairoTarget, cos(angle) * outsideRadius,
				sin(angle) * outsideRadius);
	}
	return;
}

static void 
gerbv_draw_aperature_hole(cairo_t *cairoTarget, gdouble dimensionX, gdouble dimensionY)
{
	if (dimensionX) {
		if (dimensionY) {
			gerbv_draw_rectangle (cairoTarget, dimensionX, dimensionY);
		}
		else {
			gerbv_draw_circle (cairoTarget, dimensionX);
		}
	}
	return;
}

int
gerbv_draw_amacro(cairo_t *cairoTarget, instruction_t *program, unsigned int nuf_push,
		  gdouble *parameters)
{
	macro_stack_t *s = new_stack(nuf_push);
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
				cairo_new_path(cairoTarget);
				if (ip->data.ival == 1) {
			    		gerbv_draw_circle (cairoTarget, s->stack[CIRCLE_DIAMETER]);
			    		cairo_fill (cairoTarget);
				}
				else if (ip->data.ival == 4) {
					int pointCounter,numberOfPoints;
					numberOfPoints = (int) s->stack[OUTLINE_NUMBER_OF_POINTS];
					
					cairo_rotate (cairoTarget, s->stack[numberOfPoints * 2 + OUTLINE_ROTATION - 2] * M_PI/180);
					cairo_move_to (cairoTarget, s->stack[OUTLINE_FIRST_X], s->stack[OUTLINE_FIRST_Y]);

					for (pointCounter=0; pointCounter < numberOfPoints; pointCounter++) {
						cairo_line_to (cairoTarget, s->stack[pointCounter * 2 + OUTLINE_FIRST_X],
							s->stack[pointCounter * 2 + OUTLINE_FIRST_Y]);
					}

					/* although the gerber specs allow for an open outline,
					   I interpret it to mean the outline should be closed by the
					   rendering softare automatically, since there is no dimension
					   for line thickness.
					*/
					cairo_fill (cairoTarget);
				}
				else if (ip->data.ival == 5) {
					cairo_move_to (cairoTarget, s->stack[POLYGON_CENTER_X],
						s->stack[POLYGON_CENTER_Y]);
					gerbv_draw_polygon(cairoTarget, s->stack[POLYGON_DIAMETER] / 2.0,
						s->stack[POLYGON_NUMBER_OF_POINTS], s->stack[POLYGON_ROTATION]);
					cairo_fill (cairoTarget);
				}
				else if (ip->data.ival == 6) {
					gdouble diameter, gap;
				    	int circleIndex;
				    	
				    	cairo_rotate (cairoTarget, s->stack[MOIRE_ROTATION] * M_PI/180);
				    	diameter = s->stack[MOIRE_OUTSIDE_DIAMETER] -  s->stack[MOIRE_CIRCLE_THICKNESS];
				    	gap = s->stack[MOIRE_GAP_WIDTH] + s->stack[MOIRE_CIRCLE_THICKNESS];
				    	cairo_set_line_width (cairoTarget, s->stack[MOIRE_CIRCLE_THICKNESS]);
				    	
				    	for (circleIndex = 0; circleIndex < (int)s->stack[MOIRE_NUMBER_OF_CIRCLES];  circleIndex++) {
				    		gdouble currentDiameter = (diameter - gap * (float) circleIndex);
				    		gerbv_draw_circle (cairoTarget, currentDiameter);
				    		cairo_stroke (cairoTarget);
				    	}
				    	
				    	gdouble crosshairRadius = (s->stack[MOIRE_CROSSHAIR_LENGTH] / 2.0);
				    	
				    	cairo_set_line_width (cairoTarget, s->stack[MOIRE_CROSSHAIR_THICKNESS]);
				    	cairo_move_to (cairoTarget, -crosshairRadius, 0);
				    	cairo_line_to (cairoTarget, crosshairRadius, 0);    	
				    	cairo_move_to (cairoTarget, 0, -crosshairRadius);
				    	cairo_line_to (cairoTarget, 0, crosshairRadius);
				    	cairo_stroke (cairoTarget);
				}
				else if (ip->data.ival == 7) {
					gdouble diameter, ci_thickness;
					gint i;
					cairo_operator_t oldOperator = cairo_get_operator (cairoTarget);
					
					ci_thickness = (s->stack[THERMAL_OUTSIDE_DIAMETER] - 
						s->stack[THERMAL_INSIDE_DIAMETER]) / 2.0;
					diameter = (s->stack[THERMAL_INSIDE_DIAMETER] + ci_thickness);
					
					/* we don't want to delete any previously rendered items on
					   this layer, so we need to render the thermal to a private
					   group and then composite it on top of the existing layer */
					cairo_push_group (cairoTarget);
					/* draw non-filled circle */
					cairo_set_line_width (cairoTarget, ci_thickness);
					gerbv_draw_circle(cairoTarget, diameter);
					cairo_stroke (cairoTarget);
					/* draw crosshairs */
					cairo_set_operator (cairoTarget, CAIRO_OPERATOR_CLEAR);
					cairo_set_line_width (cairoTarget,s->stack[THERMAL_CROSSHAIR_THICKNESS]);
					cairo_set_line_cap (cairoTarget, CAIRO_LINE_CAP_BUTT);
					/* do initial rotation */
					cairo_rotate (cairoTarget, s->stack[THERMAL_ROTATION] * M_PI/180);
					for (i=0; i<4; i++) {
						cairo_move_to (cairoTarget, 0.0, 0.0);
						cairo_line_to (cairoTarget, s->stack[THERMAL_OUTSIDE_DIAMETER] / 2.0, 0.0);
						cairo_rotate (cairoTarget, 90 * M_PI/180);
					}
					cairo_stroke (cairoTarget);
					cairo_set_operator (cairoTarget, oldOperator);
					cairo_pop_group_to_source (cairoTarget);
					cairo_paint (cairoTarget);
				}
				else if ((ip->data.ival == 2)||(ip->data.ival == 20)) {
			  		cairo_rotate (cairoTarget, s->stack[LINE20_ROTATION] * M_PI/180);
			    		cairo_set_line_width (cairoTarget, s->stack[LINE20_LINE_WIDTH]);
			    		cairo_set_line_cap (cairoTarget, CAIRO_LINE_CAP_BUTT);
			    		cairo_move_to (cairoTarget, s->stack[LINE20_START_X], s->stack[LINE20_START_Y]);
				    	cairo_line_to (cairoTarget, s->stack[LINE20_END_X], s->stack[LINE20_END_Y]);
				    	cairo_stroke (cairoTarget);
				}
				else if (ip->data.ival == 21) {
			    		gdouble halfWidth, halfHeight;
			    		
			    		halfWidth = s->stack[LINE21_WIDTH] / 2.0;
			    		halfHeight = s->stack[LINE21_HEIGHT] / 2.0;
		    			cairo_rotate (cairoTarget, s->stack[LINE21_ROTATION] * M_PI/180);
		    			cairo_rectangle (cairoTarget, -halfWidth, -halfHeight,
		    				s->stack[LINE21_WIDTH], s->stack[LINE21_HEIGHT]);
		    			cairo_fill (cairoTarget);
		    		}
				else if (ip->data.ival == 22) {
			    		cairo_rotate (cairoTarget, s->stack[LINE22_ROTATION] * M_PI/180);
			    		cairo_rectangle (cairoTarget, s->stack[LINE22_LOWER_LEFT_X],
		    				s->stack[LINE22_LOWER_LEFT_Y], s->stack[LINE22_WIDTH],
		    				s->stack[LINE22_HEIGHT]);
		    			cairo_fill (cairoTarget);
				}
				else {
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


int
draw_image_to_cairo_target (cairo_t *cairoTarget, gerb_image_t *image)
{
    struct gerb_net *net;
    double x1, y1, x2, y2, cp_x=0, cp_y=0;
    int in_parea_fill = 0,drawing_parea_fill = 0;
    gdouble p1, p2, p3, p4, p5, dx, dy;
    
    for (net = image->netlist->next ; net != NULL; net = net->next) {
	int repeat_X=1, repeat_Y=1;
	double repeat_dist_X = 0, repeat_dist_Y = 0;
	int repeat_i, repeat_j;
	
	/*
	 * If step_and_repeat (%SR%) used, repeat the drawing;
	 * if step_and_repeat not used, net->step_and_repeat will be NULL, and
	 * we repeat everything just once.
	 */
	if(net->step_and_repeat != NULL){
	    repeat_X = net->step_and_repeat->X;
	    repeat_Y = net->step_and_repeat->Y;
	    repeat_dist_X = net->step_and_repeat->dist_X;
	    repeat_dist_Y = net->step_and_repeat->dist_Y;
	}
	for(repeat_i = 0; repeat_i < repeat_X; repeat_i++) {
	    for(repeat_j = 0; repeat_j < repeat_Y; repeat_j++) {
		double sr_x = repeat_i * repeat_dist_X;
		double sr_y = repeat_j * repeat_dist_Y;
		
		x1 = net->start_x + sr_x;
		y1 = net->start_y + sr_y;
		x2 = net->stop_x + sr_x;
		y2 = net->stop_y + sr_y;

		/* translate circular x,y data as well */
		if (net->cirseg) {
			cp_x = net->cirseg->cp_x + sr_x;
			cp_y = net->cirseg->cp_y + sr_y;
		}
	
		if ((net->layer_polarity == CLEAR)) {
			cairo_set_operator (cairoTarget, CAIRO_OPERATOR_CLEAR);
		}
		else {
			cairo_set_operator (cairoTarget, CAIRO_OPERATOR_OVER);
		}
	    
		/*
		* Polygon Area Fill routines
		*/
		switch (net->interpolation) {
			case PAREA_START :
				in_parea_fill = 1;
				cairo_new_path(cairoTarget);
				continue;
			case PAREA_END :
				cairo_close_path(cairoTarget);
				/* also create a thin border to make sure there are no
				 * slivers of space between adjacent polygons
				 */
				//cairo_set_line_width (cairoTarget, 0.002);
				//cairo_stroke_preserve (cairoTarget);
				cairo_fill (cairoTarget);
				in_parea_fill = 0;
				continue;
			default :
				break;
		}
		if (in_parea_fill) {
			if (!drawing_parea_fill) {
				cairo_move_to (cairoTarget, x1,y1);
				drawing_parea_fill=TRUE;
			}
			if (net->aperture_state == ON) {
				cairo_line_to (cairoTarget, x2,y2);
			}
			else {
				cairo_move_to (cairoTarget, x2,y2);
			}
			continue;
		}
	
		/*
		 * If aperture state is off we allow use of undefined apertures.
		 * This happens when gerber files starts, but hasn't decided on 
		 * which aperture to use.
		 */
		if (image->aperture[net->aperture] == NULL) {
			if (net->aperture_state != OFF)
				GERB_MESSAGE("Aperture [%d] is not defined\n", net->aperture);
			continue;
		}
		switch (net->aperture_state) {
			case ON :
				switch (net->interpolation) {
					case LINEARx10 :
					case LINEARx01 :
					case LINEARx001 :
					case LINEARx1 :
						switch (image->aperture[net->aperture]->type) {
							case CIRCLE :
								cairo_set_line_cap (cairoTarget, CAIRO_LINE_CAP_ROUND);
								cairo_set_line_width (cairoTarget, image->aperture[net->aperture]->parameter[0]);
								cairo_move_to (cairoTarget, x1,y1);
								cairo_line_to (cairoTarget, x2,y2);
								cairo_stroke (cairoTarget);
								break;
							case RECTANGLE :				
								dx = (image->aperture[net->aperture]->parameter[0]/ 2);
								dy = (image->aperture[net->aperture]->parameter[1]/ 2);
								if(x1 > x2)
									dx = -dx;
								if(y1 > y2)
									dy = -dy;
								cairo_new_path(cairoTarget);
								cairo_set_line_width (cairoTarget, image->aperture[net->aperture]->parameter[0]);
								cairo_move_to (cairoTarget, x1 - dx, y1 - dy);
								cairo_line_to (cairoTarget, x1 - dx, y1 + dy);
								cairo_line_to (cairoTarget, x2 - dx, y2 + dy);
								cairo_line_to (cairoTarget, x2 + dx, y2 + dy);
								cairo_line_to (cairoTarget, x2 + dx, y2 - dy);
								cairo_line_to (cairoTarget, x1 + dx, y1 - dy);
								cairo_fill (cairoTarget);
								break;
							/* for now, just render ovals or polygons like a circle */
							case OVAL :
							case POLYGON :
								cairo_set_line_cap (cairoTarget, CAIRO_LINE_CAP_ROUND);
								cairo_set_line_width (cairoTarget, image->aperture[net->aperture]->parameter[0]);
								cairo_move_to (cairoTarget, x1,y1);
								cairo_line_to (cairoTarget, x2,y2);
								cairo_stroke (cairoTarget);
								break;
							/* macros can only be flashed, so ignore any that might be here */
							default :
								break;
						}
						break;
					case CW_CIRCULAR :
					case CCW_CIRCULAR :
						/* cairo doesn't have a function to draw oval arcs, so we must
						 * draw an arc and stretch it by scaling different x and y values
						 */
						cairo_new_path(cairoTarget);
						cairo_set_line_width (cairoTarget, image->aperture[net->aperture]->parameter[0]);
						cairo_save (cairoTarget);
						cairo_translate(cairoTarget, cp_x, cp_y);
						cairo_scale (cairoTarget, net->cirseg->height,
							net->cirseg->width);
						if (net->cirseg->angle2 > net->cirseg->angle1) {
							cairo_arc (cairoTarget, 0.0, 0.0, 0.5, net->cirseg->angle1 * M_PI/180,
								net->cirseg->angle2 * M_PI/180);
						}
						else {
							cairo_arc_negative (cairoTarget, 0.0, 0.0, 0.5, net->cirseg->angle1 * M_PI/180,
								net->cirseg->angle2 * M_PI/180);
						}
						cairo_restore (cairoTarget);
						cairo_stroke (cairoTarget);
						break;
					default :
						break;
				}
				break;
			case OFF :
				cairo_move_to (cairoTarget, x1,y1);
				break;
			case FLASH :
				p1 = image->aperture[net->aperture]->parameter[0];
				p2 = image->aperture[net->aperture]->parameter[1];
				p3 = image->aperture[net->aperture]->parameter[2];
				p4 = image->aperture[net->aperture]->parameter[3];
				p5 = image->aperture[net->aperture]->parameter[4];

				cairo_save (cairoTarget);
				cairo_translate (cairoTarget, x2, y2);

				switch (image->aperture[net->aperture]->type) {
					case CIRCLE :
						gerbv_draw_circle(cairoTarget, p1);
						gerbv_draw_aperature_hole (cairoTarget, p2, p3);
						break;
					case RECTANGLE :
						gerbv_draw_rectangle(cairoTarget, p1, p2);
						gerbv_draw_aperature_hole (cairoTarget, p3, p4);
						break;
					case OVAL :
						gerbv_draw_oval(cairoTarget, p1, p2);
						gerbv_draw_aperature_hole (cairoTarget, p3, p4);
						break;
					case POLYGON :
						gerbv_draw_polygon(cairoTarget, p1, p2, p3);
						gerbv_draw_aperature_hole (cairoTarget, p4, p5);
						break;
					case MACRO :
						gerbv_draw_amacro(cairoTarget, 
								  image->aperture[net->aperture]->amacro->program,
								  image->aperture[net->aperture]->amacro->nuf_push,
								  image->aperture[net->aperture]->parameter);
						break;
					default :
						GERB_MESSAGE("Unknown aperture type\n");
						return 0;
				}
				/* and finally fill the path */
				cairo_fill (cairoTarget);
				cairo_restore (cairoTarget);
				break;
			default:
				GERB_MESSAGE("Unknown aperture state\n");
				return 0;
		}
	    }
	}
    }
	return 1;
}

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
#include "draw-gdk.h"
#include "gerb_error.h"
#include <cairo.h>

#define dprintf if(DEBUG) printf
static void
draw_check_if_object_is_in_selected_area (cairo_t *cairoTarget, gboolean isStroke,
		gerb_selection_info_t *selectionInfo, gerb_image_t *image, struct gerb_net *net){
	gdouble corner1X,corner1Y,corner2X,corner2Y;

	corner1X = selectionInfo->lowerLeftX;
	corner1Y = selectionInfo->lowerLeftY;
	corner2X = selectionInfo->upperRightX;
	corner2Y = selectionInfo->upperRightY;

	/* calculate the coordinate of the user's click in the current
	   transformation matrix */
	cairo_device_to_user  (cairoTarget, &corner1X, &corner1Y);
	cairo_device_to_user  (cairoTarget, &corner2X, &corner2Y);
	if (selectionInfo->type == POINT_CLICK) {
		/* use the cairo in_fill routine to see if the point is within the
		   drawn area */
		if ((isStroke && cairo_in_stroke (cairoTarget, corner1X, corner1Y)) ||
			(!isStroke && cairo_in_fill (cairoTarget, corner1X, corner1Y))) {
			/* add the net to the selection array */
			gerb_selection_item_t sItem = {image, net};
			g_array_append_val (selectionInfo->selectedNodeArray, sItem);
		}
	}
	else if (selectionInfo->type == DRAG_BOX) {
		gdouble x1,x2,y1,y2;
		gdouble minX,minY,maxX,maxY;
		
		/* we can't assume the "lowerleft" corner is actually in the lower left,
		   since the cairo transformation matrix may be mirrored,etc */
		minX = MIN(corner1X,corner2X);
		maxX = MAX(corner1X,corner2X);
		minY = MIN(corner1Y,corner2Y);
		maxY = MAX(corner1Y,corner2Y);
		if (isStroke)
			cairo_stroke_extents (cairoTarget, &x1, &y1, &x2, &y2);
		else
			cairo_fill_extents (cairoTarget, &x1, &y1, &x2, &y2);
		
		if ((minX < x1) && (minY < y1) && (maxX > x2) && (maxY > y2)) {
			/* add the net to the selection array */
			gerb_selection_item_t sItem = {image, net};
			g_array_append_val (selectionInfo->selectedNodeArray, sItem); 
		}
	}
	/* clear the path, since we didn't actually draw it and cairo
		 doesn't reset it after the previous calls */
	cairo_new_path (cairoTarget);
}

static void
draw_fill (cairo_t *cairoTarget, gchar drawMode, gerb_selection_info_t *selectionInfo,
		gerb_image_t *image, struct gerb_net *net){
	if ((drawMode == DRAW_IMAGE) || (drawMode == DRAW_SELECTIONS))
		cairo_fill (cairoTarget);
	else
		draw_check_if_object_is_in_selected_area (cairoTarget, FALSE,
			selectionInfo, image, net);
}

static void
draw_stroke (cairo_t *cairoTarget, gchar drawMode, gerb_selection_info_t *selectionInfo,
		gerb_image_t *image, struct gerb_net *net){
	if ((drawMode == DRAW_IMAGE) || (drawMode == DRAW_SELECTIONS))
		cairo_stroke (cairoTarget);
	else
		draw_check_if_object_is_in_selected_area (cairoTarget, TRUE,
			selectionInfo, image, net);
}

/*
 * Draws a circle _centered_ at x,y with diameter dia
 */
static void 
gerbv_draw_circle(cairo_t *cairoTarget, gdouble diameter)
{
    cairo_arc (cairoTarget, 0.0, 0.0, diameter/2.0, 0, 2.0*M_PI);
    return;
} /* gerbv_draw_circle */


/*
 * Draws a rectangle _centered_ at x,y with sides x_side, y_side
 */
static void
gerbv_draw_rectangle(cairo_t *cairoTarget, gdouble width, gdouble height)
{
    cairo_rectangle (cairoTarget, - width / 2.0, - height / 2.0, width, height);
    return;
} /* gerbv_draw_rectangle */


/*
 * Draws an oblong _centered_ at x,y with x axis x_axis and y axis y_axis
 */ 
static void
gerbv_draw_oblong(cairo_t *cairoTarget, gdouble width, gdouble height)
{
    /*
    cairo_new_path (cairoTarget);
    cairo_set_line_cap (cairoTarget, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_width (cairoTarget, height);
    cairo_move_to (cairoTarget, -width/2.0 + height/2.0, 0);
    cairo_line_to (cairoTarget, width/2.0 - height/2.0, 0);
    draw_stroke (cairoTarget, drawMode, selectionInfo);
    */
    /* cairo doesn't have a function to draw ovals, so we must
     * draw an arc and stretch it by scaling different x and y values
     */
    cairo_save (cairoTarget);
    cairo_scale (cairoTarget, width, height);
    gerbv_draw_circle (cairoTarget, 1);
    cairo_restore (cairoTarget);
    return;
} /* gerbv_draw_oval */


static void
gerbv_draw_polygon(cairo_t *cairoTarget, gdouble outsideDiameter,
		   gdouble numberOfSides, gdouble degreesOfRotation)
{
    int i, numberOfSidesInteger = (int) numberOfSides;
    
    cairo_rotate(cairoTarget, degreesOfRotation * M_PI/180);
    cairo_move_to(cairoTarget, outsideDiameter / 2.0, 0);
    /* skip first point, since we've moved there already */
    /* include last point, since we may be drawing an aperture hole next
       and cairo may not correctly close the path itself */
    for (i = 1; i <= (int)numberOfSidesInteger; i++){
	gdouble angle = (double) i / numberOfSidesInteger * M_PI * 2.0;
	cairo_line_to (cairoTarget, cos(angle) * outsideDiameter / 2.0,
		       sin(angle) * outsideDiameter / 2.0);
    }
    return;
} /* gerbv_draw_polygon */


static void 
gerbv_draw_aperature_hole(cairo_t *cairoTarget, gdouble dimensionX, gdouble dimensionY)
{
    if (dimensionX) {
	if (dimensionY) {
	    gerbv_draw_rectangle (cairoTarget, dimensionX, dimensionY);
	} else {
	    gerbv_draw_circle (cairoTarget, dimensionX);
	}
    }
    return;
} /* gerbv_draw_aperature_hole */

gboolean
draw_update_macro_exposure (cairo_t *cairoTarget, cairo_operator_t clearOperator,
		cairo_operator_t darkOperator, gdouble exposureSetting){

	if (exposureSetting == 0.0) {
		cairo_set_operator (cairoTarget, clearOperator);
	}
	else if (exposureSetting == 1.0) {
		cairo_set_operator (cairoTarget, darkOperator);
	}
	else if (exposureSetting == 2.0) {
		/* reverse current exposure setting */
		cairo_operator_t currentOperator = cairo_get_operator (cairoTarget);
		if (currentOperator == clearOperator) {
			cairo_set_operator (cairoTarget, darkOperator);
		}
		else {
			cairo_set_operator (cairoTarget, clearOperator);
		}
	}
	return TRUE;
}

	    
int
gerbv_draw_amacro(cairo_t *cairoTarget, cairo_operator_t clearOperator,
	cairo_operator_t darkOperator, gerb_simplified_amacro_t *s,
	gint usesClearPrimative, gchar drawMode, gerb_selection_info_t *selectionInfo,
	gerb_image_t *image, struct gerb_net *net)
{
    int handled = 1;  
    gerb_simplified_amacro_t *ls = s;

    dprintf("Drawing simplified aperture macros:\n");
    if (usesClearPrimative)
    	cairo_push_group (cairoTarget);
    while (ls != NULL) {
	    /* 
	     * This handles the exposure thing in the aperture macro
	     * The exposure is always the first element on stack independent
	     * of aperture macro.
	     */
	    cairo_save (cairoTarget);
	    cairo_new_path(cairoTarget);
	    cairo_operator_t oldOperator = cairo_get_operator (cairoTarget);

	    if (ls->type == MACRO_CIRCLE) {
	    	
	      if (draw_update_macro_exposure (cairoTarget, clearOperator, 
	      		darkOperator, ls->parameter[CIRCLE_EXPOSURE])){
		    	cairo_translate (cairoTarget, ls->parameter[CIRCLE_CENTER_X],
				       ls->parameter[CIRCLE_CENTER_Y]);
			
			gerbv_draw_circle (cairoTarget, ls->parameter[CIRCLE_DIAMETER]);
			draw_fill (cairoTarget, drawMode, selectionInfo, image, net);
		}
	    } else if (ls->type == MACRO_OUTLINE) {
		int pointCounter,numberOfPoints;
		/* Number of points parameter seems to not include the start point,
		 * so we add one to include the start point.
		 */
		numberOfPoints = (int) ls->parameter[OUTLINE_NUMBER_OF_POINTS] + 1;
		
		if (draw_update_macro_exposure (cairoTarget, clearOperator, 
					darkOperator, ls->parameter[OUTLINE_EXPOSURE])){
			cairo_rotate (cairoTarget, ls->parameter[(numberOfPoints - 1) * 2 + OUTLINE_ROTATION] * M_PI/180.0);
			cairo_move_to (cairoTarget, ls->parameter[OUTLINE_FIRST_X], ls->parameter[OUTLINE_FIRST_Y]);
			
			for (pointCounter=0; pointCounter < numberOfPoints; pointCounter++) {
			    cairo_line_to (cairoTarget, ls->parameter[pointCounter * 2 + OUTLINE_FIRST_X],
					   ls->parameter[pointCounter * 2 + OUTLINE_FIRST_Y]);
			}
			/* although the gerber specs allow for an open outline,
			   I interpret it to mean the outline should be closed by the
			   rendering softare automatically, since there is no dimension
			   for line thickness.
			*/
			draw_fill (cairoTarget, drawMode, selectionInfo, image, net);
		}
	    } else if (ls->type == MACRO_POLYGON) {
	      if (draw_update_macro_exposure (cairoTarget, clearOperator, 
	      			darkOperator, ls->parameter[POLYGON_EXPOSURE])){
			cairo_translate (cairoTarget, ls->parameter[POLYGON_CENTER_X],
				       ls->parameter[POLYGON_CENTER_Y]);
			gerbv_draw_polygon(cairoTarget, ls->parameter[POLYGON_DIAMETER],
					   ls->parameter[POLYGON_NUMBER_OF_POINTS], ls->parameter[POLYGON_ROTATION]);
			draw_fill (cairoTarget, drawMode, selectionInfo, image, net);
		}
	    } else if (ls->type == MACRO_MOIRE) {
		gdouble diameter, gap;
		int circleIndex;
		
		cairo_translate (cairoTarget, ls->parameter[MOIRE_CENTER_X],
			       ls->parameter[MOIRE_CENTER_Y]);
		cairo_rotate (cairoTarget, ls->parameter[MOIRE_ROTATION] * M_PI/180);
		diameter = ls->parameter[MOIRE_OUTSIDE_DIAMETER] -  ls->parameter[MOIRE_CIRCLE_THICKNESS];
		gap = ls->parameter[MOIRE_GAP_WIDTH] + ls->parameter[MOIRE_CIRCLE_THICKNESS];
		cairo_set_line_width (cairoTarget, ls->parameter[MOIRE_CIRCLE_THICKNESS]);
		
		for (circleIndex = 0; circleIndex < (int)ls->parameter[MOIRE_NUMBER_OF_CIRCLES];  circleIndex++) {
		    gdouble currentDiameter = (diameter - gap * (float) circleIndex);
		    gerbv_draw_circle (cairoTarget, currentDiameter);
		    draw_stroke (cairoTarget, drawMode, selectionInfo, image, net);
		}
		
		gdouble crosshairRadius = (ls->parameter[MOIRE_CROSSHAIR_LENGTH] / 2.0);
		
		cairo_set_line_width (cairoTarget, ls->parameter[MOIRE_CROSSHAIR_THICKNESS]);
		cairo_move_to (cairoTarget, -crosshairRadius, 0);
		cairo_line_to (cairoTarget, crosshairRadius, 0);    	
		cairo_move_to (cairoTarget, 0, -crosshairRadius);
		cairo_line_to (cairoTarget, 0, crosshairRadius);
		draw_stroke (cairoTarget, drawMode, selectionInfo, image, net);
	    } else if (ls->type == MACRO_THERMAL) {
		gint i;
		gdouble startAngle1, startAngle2, endAngle1, endAngle2;
		
		cairo_translate (cairoTarget, ls->parameter[THERMAL_CENTER_X],
			       ls->parameter[THERMAL_CENTER_Y]);
		cairo_rotate (cairoTarget, ls->parameter[THERMAL_ROTATION] * M_PI/180.0);
		startAngle1 = atan (ls->parameter[THERMAL_CROSSHAIR_THICKNESS]/ls->parameter[THERMAL_INSIDE_DIAMETER]);
		endAngle1 = M_PI/2 - startAngle1;
		endAngle2 = atan (ls->parameter[THERMAL_CROSSHAIR_THICKNESS]/ls->parameter[THERMAL_OUTSIDE_DIAMETER]);
		startAngle2 = M_PI/2 - endAngle2;
		for (i = 0; i < 4; i++) {
			cairo_arc (cairoTarget, 0, 0, ls->parameter[THERMAL_INSIDE_DIAMETER]/2.0, startAngle1, endAngle1);
			cairo_rel_line_to (cairoTarget, 0, ls->parameter[THERMAL_CROSSHAIR_THICKNESS]);
			cairo_arc_negative (cairoTarget, 0, 0, ls->parameter[THERMAL_OUTSIDE_DIAMETER]/2.0,
				startAngle2, endAngle2);
			cairo_rel_line_to (cairoTarget, -ls->parameter[THERMAL_CROSSHAIR_THICKNESS],0);
			draw_fill (cairoTarget, drawMode, selectionInfo, image, net);
			cairo_rotate (cairoTarget, 90 * M_PI/180);
		}
	    } else if (ls->type == MACRO_LINE20) {
	      if (draw_update_macro_exposure (cairoTarget, clearOperator, 
	      			darkOperator, ls->parameter[LINE20_EXPOSURE])){
			cairo_set_line_width (cairoTarget, ls->parameter[LINE20_LINE_WIDTH]);
			cairo_set_line_cap (cairoTarget, CAIRO_LINE_CAP_BUTT);
			cairo_rotate (cairoTarget, ls->parameter[LINE20_ROTATION] * M_PI/180.0);
			cairo_move_to (cairoTarget, ls->parameter[LINE20_START_X], ls->parameter[LINE20_START_Y]);
			cairo_line_to (cairoTarget, ls->parameter[LINE20_END_X], ls->parameter[LINE20_END_Y]);
			draw_stroke (cairoTarget, drawMode, selectionInfo, image, net);
		}
	    } else if (ls->type == MACRO_LINE21) {
		gdouble halfWidth, halfHeight;
		
		if (draw_update_macro_exposure (cairoTarget, clearOperator,
						darkOperator, ls->parameter[LINE22_EXPOSURE])){
			halfWidth = ls->parameter[LINE21_WIDTH] / 2.0;
			halfHeight = ls->parameter[LINE21_HEIGHT] / 2.0;
			cairo_translate (cairoTarget, ls->parameter[LINE21_CENTER_X], ls->parameter[LINE21_CENTER_Y]);
			cairo_rotate (cairoTarget, ls->parameter[LINE21_ROTATION] * M_PI/180.0);
			cairo_rectangle (cairoTarget, -halfWidth, -halfHeight,
					 ls->parameter[LINE21_WIDTH], ls->parameter[LINE21_HEIGHT]);
			draw_fill (cairoTarget, drawMode, selectionInfo, image, net);
		}	
	    } else if (ls->type == MACRO_LINE22) {
	    	gdouble halfWidth, halfHeight;
		
		if (draw_update_macro_exposure (cairoTarget, clearOperator,
					darkOperator, ls->parameter[LINE22_EXPOSURE])){
			halfWidth = ls->parameter[LINE22_WIDTH] / 2.0;
			halfHeight = ls->parameter[LINE22_HEIGHT] / 2.0;
			cairo_translate (cairoTarget, ls->parameter[LINE22_LOWER_LEFT_X],
					ls->parameter[LINE22_LOWER_LEFT_Y]);
			cairo_rotate (cairoTarget, ls->parameter[LINE22_ROTATION] * M_PI/180.0);
			cairo_rectangle (cairoTarget, 0, 0,
					ls->parameter[LINE22_WIDTH], ls->parameter[LINE22_HEIGHT]);
			draw_fill (cairoTarget, drawMode, selectionInfo, image, net);
		}
	    } else {
		handled = 0;
	    }
	    cairo_set_operator (cairoTarget, oldOperator);
	    cairo_restore (cairoTarget);
	    ls = ls->next;
    }
    if (usesClearPrimative) {
    	cairo_pop_group_to_source (cairoTarget);
      cairo_paint (cairoTarget);
    }
    return handled;
} /* gerbv_draw_amacro */


void
draw_apply_netstate_transformation (cairo_t *cairoTarget, gerb_netstate_t *state) 
{
	/* apply scale factor */
	cairo_scale (cairoTarget, state->scaleA, state->scaleB);
	/* apply offset */
	cairo_translate (cairoTarget, state->offsetA, state->offsetB);
	/* apply mirror */
	switch (state->mirrorState) {
		case FLIPA:
			cairo_scale (cairoTarget, -1, 1);
			break;
		case FLIPB:
			cairo_scale (cairoTarget, 1, -1);
			break;
		case FLIPAB:
			cairo_scale (cairoTarget, -1, -1);
			break;
		default:
			break;
	}
	/* finally, apply axis select */
	if (state->axisSelect == SWAPAB) {
		/* we do this by rotating 270 (counterclockwise, then mirroring
		   the Y axis */
		cairo_rotate (cairoTarget, 3 * M_PI / 2);
		cairo_scale (cairoTarget, 1, -1);
	}
}

int
draw_image_to_cairo_target (cairo_t *cairoTarget, gerb_image_t *image,
					gboolean invertLayer, gdouble pixelWidth,
					gchar drawMode, gerb_selection_info_t *selectionInfo)
{
	struct gerb_net *net, *polygonStartNet=NULL;
	double x1, y1, x2, y2, cp_x=0, cp_y=0;
	int in_parea_fill = 0,haveDrawnFirstFillPoint = 0;
	gdouble p1, p2, p3, p4, p5, dx, dy;
	gerb_netstate_t *oldState;
	gerb_layer_t *oldLayer;
	int repeat_X=1, repeat_Y=1;
	double repeat_dist_X = 0, repeat_dist_Y = 0;
	int repeat_i, repeat_j;
	cairo_operator_t drawOperatorClear, drawOperatorDark;
	gboolean invertPolarity = FALSE;
	
    /* do initial justify */
	cairo_translate (cairoTarget, image->info->imageJustifyOffsetActualA,
		 image->info->imageJustifyOffsetActualB);

    /* set the fill rule so aperture holes are cleared correctly */	 
    cairo_set_fill_rule (cairoTarget, CAIRO_FILL_RULE_EVEN_ODD);
    /* offset image */
    cairo_translate (cairoTarget, image->info->offsetA, image->info->offsetB);
    /* do image rotation */
    cairo_rotate (cairoTarget, image->info->imageRotation);
    /* load in polarity operators depending on the image polarity */
    invertPolarity = invertLayer;
    if (image->info->polarity == NEGATIVE)
    	invertPolarity = !invertPolarity;
    if (invertPolarity) {
    	drawOperatorClear = CAIRO_OPERATOR_OVER;
    	drawOperatorDark = CAIRO_OPERATOR_CLEAR;
    	cairo_set_operator (cairoTarget, CAIRO_OPERATOR_OVER);
    	cairo_paint (cairoTarget);
    	cairo_set_operator (cairoTarget, CAIRO_OPERATOR_CLEAR);
    }
    else {
      drawOperatorClear = CAIRO_OPERATOR_CLEAR;
    	drawOperatorDark = CAIRO_OPERATOR_OVER;
    }
    /* next, push two cairo states to simulate the first layer and netstate
       translations (these will be popped when another layer or netstate is
       started */

    cairo_save (cairoTarget);
    cairo_save (cairoTarget);
    /* store the current layer and netstate so we know when they change */
    oldLayer = image->layers;
    oldState = image->states;

    for (net = image->netlist->next ; net != NULL; net = net->next) {

	/* check if this is a new layer */
	if (net->layer != oldLayer){
		/* it's a new layer, so recalculate the new transformation matrix
		   for it */
		cairo_restore (cairoTarget);
		cairo_restore (cairoTarget);
		cairo_save (cairoTarget);
		/* do any rotations */
		cairo_rotate (cairoTarget, net->layer->rotation);
		/* handle the layer polarity */
		if ((net->layer->polarity == CLEAR)) {
			cairo_set_operator (cairoTarget, drawOperatorClear);
		}
		else {
			cairo_set_operator (cairoTarget, drawOperatorDark);
		}
		/* check for changes to step and repeat */
		repeat_X = net->layer->stepAndRepeat.X;
		repeat_Y = net->layer->stepAndRepeat.Y;
		repeat_dist_X = net->layer->stepAndRepeat.dist_X;
		repeat_dist_Y = net->layer->stepAndRepeat.dist_Y;
		/* draw any knockout areas */
		if (net->layer->knockout.firstInstance == TRUE) {
			cairo_operator_t oldOperator = cairo_get_operator (cairoTarget);
			if (net->layer->knockout.polarity == CLEAR) {
				cairo_set_operator (cairoTarget, drawOperatorClear);
			}
			else {
				cairo_set_operator (cairoTarget, drawOperatorDark);
			}
			cairo_new_path (cairoTarget);
			cairo_rectangle (cairoTarget, net->layer->knockout.lowerLeftX - net->layer->knockout.border,
					net->layer->knockout.lowerLeftY - net->layer->knockout.border,
					net->layer->knockout.width + (net->layer->knockout.border*2),
					net->layer->knockout.height + (net->layer->knockout.border*2));
			draw_fill (cairoTarget, drawMode, selectionInfo, image, net);
			cairo_set_operator (cairoTarget, oldOperator);
		}
		/* finally, reapply old netstate transformation */
		cairo_save (cairoTarget);
		draw_apply_netstate_transformation (cairoTarget, net->state);
		oldLayer = net->layer;
	}
	/* check if this is a new netstate */
	if (net->state != oldState){
		/* pop the transformation matrix back to the "pre-state" state and
		   resave it */
		cairo_restore (cairoTarget);
		cairo_save (cairoTarget);
		/* it's a new state, so recalculate the new transformation matrix
		   for it */
		draw_apply_netstate_transformation (cairoTarget, net->state);
		oldState = net->state;	
	}
	/* if we are only drawing from the selection buffer, search if this net is
	   in the buffer */
	if (drawMode == DRAW_SELECTIONS) {
		/* this flag makes sure we don't draw any unintentional polygons...
		   if we've successfully entered a polygon (the first net matches, and
		   we don't want to check the nets inside the polygon) then
		   polygonStartNet will be set */
		if (!polygonStartNet) {
			int i;
			gboolean foundNet = FALSE;
			
			for (i=0; i<selectionInfo->selectedNodeArray->len; i++){
				gerb_selection_item_t sItem = g_array_index (selectionInfo->selectedNodeArray,
					gerb_selection_item_t, i);
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
		
		x1 = net->start_x + sr_x;
		y1 = net->start_y + sr_y;
		x2 = net->stop_x + sr_x;
		y2 = net->stop_y + sr_y;
           
		/* translate circular x,y data as well */
		if (net->cirseg) {
			cp_x = net->cirseg->cp_x + sr_x;
			cp_y = net->cirseg->cp_y + sr_y;
		}
		
		/* render any labels attached to this net */
		/* NOTE: this is currently only used on PNP files, so we may
		   make some assumptions here... */
		if (net->label) {
			cairo_set_font_size (cairoTarget, 0.05);
			cairo_save (cairoTarget);
			
			cairo_move_to (cairoTarget, x1, y1);
			cairo_scale (cairoTarget, 1, -1);
			cairo_show_text (cairoTarget, net->label->str);
			cairo_restore (cairoTarget);
		}
		/*
		* Polygon Area Fill routines
		*/
		switch (net->interpolation) {
			case PAREA_START :
				in_parea_fill = 1;
				haveDrawnFirstFillPoint = FALSE;
				/* save the first net in the polygon as the "ID" net pointer
				   in case we are saving this net to the selection array */
				polygonStartNet = net;
				cairo_new_path(cairoTarget);
				continue;
			case PAREA_END :
				cairo_close_path(cairoTarget);
				/* turn off anti-aliasing for polygons, since it shows seams
				   with adjacent polygons (usually on PCB ground planes) */
				cairo_antialias_t oldAlias = cairo_get_antialias (cairoTarget);
				cairo_set_antialias (cairoTarget, CAIRO_ANTIALIAS_NONE);
				draw_fill (cairoTarget, drawMode, selectionInfo, image, polygonStartNet);
				cairo_set_antialias (cairoTarget, oldAlias);
				in_parea_fill = 0;
				polygonStartNet = NULL;
				continue;
			/* make sure we completely skip over any deleted nodes */
			case DELETED:
				continue;
			default :
				break;
		}
		if (in_parea_fill) {
			if (!haveDrawnFirstFillPoint) {
				cairo_move_to (cairoTarget, x2,y2);
				haveDrawnFirstFillPoint=TRUE;
				continue;
			}
			switch (net->interpolation) {
				case LINEARx10 :
				case LINEARx01 :
				case LINEARx001 :
				case LINEARx1 :
					cairo_line_to (cairoTarget, x2,y2);
					break;
				case CW_CIRCULAR :
				case CCW_CIRCULAR :
					if (net->cirseg->angle2 > net->cirseg->angle1) {
						cairo_arc (cairoTarget, cp_x, cp_y, net->cirseg->width/2.0,
							net->cirseg->angle1 * M_PI/180,net->cirseg->angle2 * M_PI/180);
					}
					else {
						cairo_arc_negative (cairoTarget, cp_x, cp_y, net->cirseg->width/2.0,
							net->cirseg->angle1 * M_PI/180,net->cirseg->angle2 * M_PI/180);
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
			if (net->aperture_state != OFF)
				GERB_MESSAGE("Aperture [%d] is not defined\n", net->aperture);
			continue;
		}
		switch (net->aperture_state) {
			case ON :
				/* if the aperture width is truly 0, then render as a 1 pixel width
				   line.  0 diameter apertures are used by some programs to draw labels,
				   etc, and they are rendered by other programs as 1 pixel wide */
				/* NOTE: also, make sure all lines are at least 1 pixel wide, so they
				   always show up at low zoom levels */
				if (image->aperture[net->aperture]->parameter[0] > pixelWidth)
					cairo_set_line_width (cairoTarget, image->aperture[net->aperture]->parameter[0]);
				else
					cairo_set_line_width (cairoTarget, pixelWidth);
				switch (net->interpolation) {
					case LINEARx10 :
					case LINEARx01 :
					case LINEARx001 :
					case LINEARx1 :
						cairo_set_line_cap (cairoTarget, CAIRO_LINE_CAP_ROUND);
						switch (image->aperture[net->aperture]->type) {
							case CIRCLE :
								cairo_move_to (cairoTarget, x1,y1);
								cairo_line_to (cairoTarget, x2,y2);
								draw_stroke (cairoTarget, drawMode, selectionInfo, image, net);
								break;
							case RECTANGLE :				
								dx = (image->aperture[net->aperture]->parameter[0]/ 2);
								dy = (image->aperture[net->aperture]->parameter[1]/ 2);
								if(x1 > x2)
									dx = -dx;
								if(y1 > y2)
									dy = -dy;
								cairo_new_path(cairoTarget);
								cairo_move_to (cairoTarget, x1 - dx, y1 - dy);
								cairo_line_to (cairoTarget, x1 - dx, y1 + dy);
								cairo_line_to (cairoTarget, x2 - dx, y2 + dy);
								cairo_line_to (cairoTarget, x2 + dx, y2 + dy);
								cairo_line_to (cairoTarget, x2 + dx, y2 - dy);
								cairo_line_to (cairoTarget, x1 + dx, y1 - dy);
								draw_fill (cairoTarget, drawMode, selectionInfo, image, net);
								break;
							/* for now, just render ovals or polygons like a circle */
							case OVAL :
							case POLYGON :
								cairo_move_to (cairoTarget, x1,y1);
								cairo_line_to (cairoTarget, x2,y2);
								draw_stroke (cairoTarget, drawMode, selectionInfo, image, net);
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
						if (image->aperture[net->aperture]->type == RECTANGLE) {
							cairo_set_line_cap (cairoTarget, CAIRO_LINE_CAP_SQUARE);
						}
						else {
							cairo_set_line_cap (cairoTarget, CAIRO_LINE_CAP_ROUND);
						}
						cairo_save (cairoTarget);
						cairo_translate(cairoTarget, cp_x, cp_y);
						cairo_scale (cairoTarget, net->cirseg->width, net->cirseg->height);
						if (net->cirseg->angle2 > net->cirseg->angle1) {
							cairo_arc (cairoTarget, 0.0, 0.0, 0.5, net->cirseg->angle1 * M_PI/180,
								net->cirseg->angle2 * M_PI/180);
						}
						else {
							cairo_arc_negative (cairoTarget, 0.0, 0.0, 0.5, net->cirseg->angle1 * M_PI/180,
								net->cirseg->angle2 * M_PI/180);
						}
						cairo_restore (cairoTarget);
						draw_stroke (cairoTarget, drawMode, selectionInfo, image, net);
						break;
					default :
						break;
				}
				break;
			case OFF :
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
						gerbv_draw_oblong(cairoTarget, p1, p2);
						gerbv_draw_aperature_hole (cairoTarget, p3, p4);
						break;
					case POLYGON :
						gerbv_draw_polygon(cairoTarget, p1, p2, p3);
						gerbv_draw_aperature_hole (cairoTarget, p4, p5);
						break;
					case MACRO :
						gerbv_draw_amacro(cairoTarget, drawOperatorClear, drawOperatorDark,
								  image->aperture[net->aperture]->simplified,
								  (int) image->aperture[net->aperture]->parameter[0],
								  drawMode, selectionInfo, image, net);
						break;   
					default :
						GERB_MESSAGE("Unknown aperture type\n");
						return 0;
				}
				/* and finally fill the path */
				draw_fill (cairoTarget, drawMode, selectionInfo, image, net);
				cairo_restore (cairoTarget);
				break;
			default:
				GERB_MESSAGE("Unknown aperture state\n");
				return 0;
		}
	    }
	}
    }

    /* restore the initial two state saves (one for layer, one for netstate)*/
    cairo_restore (cairoTarget);
    cairo_restore (cairoTarget);

    return 1;
}



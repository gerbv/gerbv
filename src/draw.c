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

#include <math.h>
#include <stdlib.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "draw.h"
#include "draw_amacro.h"
#include "gerb_error.h"


/*
 * Render data contained of gerb_image struct and compose it into the
 * framebuffer using the specified color and transparency.
 *
 * if image is NULL we'll just clear the framebuffer to prepare a new
 * render pass.
 * 
 */ 
int 
gerb_render_image(struct gerb_render_context *ctx,
		  struct gerb_image *image, 
		  int scale, double trans_x, double trans_y)
{
    struct gerb_net *net;
    double x1, y1, x2, y2;
    double p1, p2;
    double cir_width = 0, cir_height = 0;
    double cp_x = 0, cp_y = 0;
    double *points = NULL;
    int curr_point_idx = 0;
    int in_parea_fill = 0;
    double unit_scale;

    if (image == NULL)
	goto compose;

    /*
     * Clear background depending image on image polarity
     */
    if (image->info->polarity == NEGATIVE)
	ctx->clear (ctx, CLEAR);
    else
	ctx->clear (ctx, DARK);

    for (net = image->netlist->next ; net != NULL; net = net->next) {
	
	if (net->unit == MM) 
	    unit_scale = scale / 25.4;
	else 
	    unit_scale = scale;

	/*
	 * Scale points with window scaling and translate them
	 */
	x1 = (image->info->offset_a + net->start_x) * unit_scale + trans_x;
	y1 = (image->info->offset_b - net->start_y) * unit_scale + trans_y;
	x2 = (image->info->offset_a + net->stop_x) * unit_scale + trans_x;
	y2 = (image->info->offset_b - net->stop_y) * unit_scale + trans_y;

	/* 
	 * If circle segment, scale and translate that one too
	 */
	if (net->cirseg) {
	    cir_width = net->cirseg->width * unit_scale;
	    cir_height = net->cirseg->height * unit_scale;
	    cp_x = (image->info->offset_a + net->cirseg->cp_x) *
			      unit_scale + trans_x;
	    cp_y = (image->info->offset_b - net->cirseg->cp_y) *
			      unit_scale + trans_y;
	}

	/*
	 * if this (gerber) layer is inverted
	 */
	if (net->layer_polarity == CLEAR) {
	    if (image->info->polarity == NEGATIVE)
	    	ctx->set_color (ctx, CLEAR);
	    else
	    	ctx->set_color (ctx, DARK);
	} else {
	    if (image->info->polarity == NEGATIVE)
	    	ctx->set_color (ctx, DARK);
	    else
	    	ctx->set_color (ctx, CLEAR);
	}

	/*
	 * Polygon Area Fill routines
	 */
	switch (net->interpolation) {
	case PAREA_START :
	    if (!(points = malloc (2 * sizeof(double) *  net->nuf_pcorners)))
		GERB_FATAL_ERROR("Malloc failed\n");
	    memset(points, 0, 2 * sizeof(double) *  net->nuf_pcorners);
	    curr_point_idx = 0;
	    in_parea_fill = 1;
	    continue;
	case PAREA_END :
	    ctx->fill_polygon (ctx, points, curr_point_idx);
	    free(points);
	    points = NULL;
	    in_parea_fill = 0;
	    continue;
	default :
	    break;
	}

	if (in_parea_fill) {
	    points[2*curr_point_idx]   = x2;
	    points[2*curr_point_idx+1] = y2;
	    curr_point_idx++;
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

	/*
	 * "Normal" aperture drawing routines
	 */
	if (image->aperture[net->aperture]->unit == MM)
	    unit_scale = scale / 25.4;
	else
	    unit_scale = scale;

	switch (net->aperture_state) {
	case ON :
	    p1 = image->aperture[net->aperture]->parameter[0] * unit_scale;

	    /*
	     * Rectangular apertures should draw lines with projecting end.
	     */
	    if ((image->aperture[net->aperture]->type == RECTANGLE))
		ctx->set_line_style (ctx, p1, 0, 1);
	    else
		ctx->set_line_style (ctx, p1, 0, 0);

	    switch (net->interpolation) {
	    case LINEARx10 :
	    case LINEARx01 :
	    case LINEARx001 :
		GERB_MESSAGE("Linear != x1\n");
		ctx->set_line_style (ctx, p1, 1, 0);
		ctx->draw_line (ctx, x1, y1, x2, y2);
		ctx->set_line_style (ctx, p1, 0, 0);
		break;
	    case LINEARx1:
		ctx->draw_line (ctx, x1, y1, x2, y2);
		break;
	    case CW_CIRCULAR:
	    case CCW_CIRCULAR:
		ctx->draw_arc (ctx, cp_x-cir_width/2.0, cp_y-cir_height/2.0,
			       cir_width, cir_height,
			       net->cirseg->angle1,
			       net->cirseg->angle2 - net->cirseg->angle1);
		break;		
	    default :
		break;
	    }
	    break;
	case OFF :
	    break;
	case FLASH :
	    p1 = image->aperture[net->aperture]->parameter[0] * unit_scale;
	    p2 = image->aperture[net->aperture]->parameter[1] * unit_scale;
	    
	    switch (image->aperture[net->aperture]->type) {
	    case CIRCLE:
		ctx->fill_oval (ctx, x2-p1/2, y2-p1/2, p1, p1);
		break;
	    case RECTANGLE:
		ctx->fill_rectangle (ctx, x2-p1/2, y2-p2/2, p1, p2);
		break;
	    case OVAL :
		ctx->fill_oval (ctx, x2-p1/2, y2-p2/2, p1, p2);
		break;
	    case POLYGON :
		GERB_COMPILE_WARNING("Very bad at drawing polygons.\n");
		ctx->fill_oval (ctx, x2-p1/2, y2-p1/2, p1, p1);
		break;
	    case MACRO :
		gerbv_draw_amacro(ctx, 
				  image->aperture[net->aperture]->amacro->program,
				  image->aperture[net->aperture]->amacro->nuf_push,
				  image->aperture[net->aperture]->parameter,
				  unit_scale, x2, y2);
		break;
	    default :
		GERB_MESSAGE("Unknown aperture type\n");
		return -1;
	    }
	    break;
	default :
	    GERB_MESSAGE("Unknown aperture state\n");
	    return -1;
	}
    }

compose:
    ctx->compose (ctx);

    return 0;
} /* gerb_render_image */


void
gerb_render_set_viewport(struct gerb_render_context *ctx,
			 int width, int height)
{
    ctx->set_viewport (ctx, width, height);
} /* gerb_render_set_viewport */


void 
gerb_render_clear_bg(struct gerb_render_context *ctx,
		     const unsigned char bg_rgba[4])
{
    ctx->clear_composition (ctx, bg_rgba);
} /* gerb_render_clear_bg */


void 
gerb_render_set_color(struct gerb_render_context *ctx,
		      const unsigned char rgba [4])
{
    ctx->set_layer_color (ctx, rgba);
} /* gerb_render_set_color */


void 
gerb_render_show(struct gerb_render_context *ctx,
		 double offset_x, double offset_y)
{
    ctx->blit (ctx, offset_x, offset_y);
} /* gerb_render_show */


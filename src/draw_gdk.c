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

#include <math.h>
#include <stdlib.h>

#include "gerb_error.h"
#include "draw_gdk.h"


#undef round
#define round(x) ceil((double)(x))


struct gerb_gdk_context {
	struct gerb_render_context ctx;
	GdkDrawable *dst_drawable;
	GdkGC *dst_drawable_gc;
	GdkPixmap *clipmask;
	GdkGC *clipmask_gc;
	GdkPixmap *pixmap;
	GdkGC *pixmap_gc;
	unsigned char current_rgba [4];
};


static void 
gerb_gdk_set_layer_color(struct gerb_render_context *ctx,
			 const unsigned char rgba[4])
{
    struct gerb_gdk_context *gctx = (struct gerb_gdk_context*) ctx;
    
    gctx->current_rgba[0] = rgba[0];
    gctx->current_rgba[1] = rgba[1];
    gctx->current_rgba[2] = rgba[2];
    gctx->current_rgba[3] = rgba[3];
} /* gerb_gdk_set_layer_color */


static void 
gerb_gdk_set_viewport(struct gerb_render_context *ctx, int w, int h)
{
    struct gerb_gdk_context *gctx = (struct gerb_gdk_context*) ctx;
    
    if (gctx->pixmap)
	gdk_pixmap_unref(gctx->pixmap);
    
    if (gctx->clipmask)
	gdk_pixmap_unref(gctx->clipmask);
    
    if (gctx->dst_drawable_gc)
	gdk_gc_unref(gctx->dst_drawable_gc);
    
    if (gctx->pixmap_gc)
	gdk_gc_unref(gctx->pixmap_gc);
    
    if (gctx->clipmask_gc)
	gdk_gc_unref(gctx->clipmask_gc);
    
    gctx->pixmap   = gdk_pixmap_new(gctx->dst_drawable, w, h, -1);
    gctx->clipmask = gdk_pixmap_new(gctx->dst_drawable, w, h, 1);
    
    gctx->dst_drawable_gc = gdk_gc_new (gctx->dst_drawable);
    gctx->pixmap_gc = gdk_gc_new (gctx->pixmap);
    gctx->clipmask_gc = gdk_gc_new (gctx->clipmask);
} /* gerb_gdk_set_viewport */


static void 
gerb_gdk_clear(struct gerb_render_context *ctx, enum polarity_t polarity)
{
    struct gerb_gdk_context *gctx = (struct gerb_gdk_context*) ctx;
    GdkColor c;
    GdkGC *local_gc = gdk_gc_new(gctx->clipmask);
    
    c.pixel = (polarity == CLEAR) ? 1 : 0;
    
    gdk_gc_copy(local_gc, gctx->clipmask_gc);
    gdk_gc_set_foreground(local_gc, &c);
    gdk_draw_rectangle(gctx->clipmask, local_gc, TRUE, 0, 0, -1, -1);
    gdk_gc_unref(local_gc);
} /* gerb_gdk_clear */


static void 
gerb_gdk_draw_line(struct gerb_render_context *ctx,
		   double x1, double y1, double x2, double y2)
{
    struct gerb_gdk_context *gctx = (struct gerb_gdk_context*) ctx;
    
    gdk_draw_line (gctx->clipmask, gctx->clipmask_gc,
		   round(x1), round(y1), round(x2), round(y2));
} /* gerb_gdk_draw_line */


static void 
gerb_gdk_draw_linestrip(struct gerb_render_context *ctx,
			double *xy, int n)
{
    struct gerb_gdk_context *gctx = (struct gerb_gdk_context*) ctx;
    int i;
    
    for (i=0; i<n-1; i++)
	gdk_draw_line (gctx->clipmask, gctx->clipmask_gc,
		       round(xy[2*i]), round(xy[2*i+1]),
		       round(xy[2*i+2]), round(xy[2*i+3]));
} /* gerb_gdk_draw_linestrip */


static void 
gerb_gdk_draw_arc(struct gerb_render_context *ctx,
		  double x, double y, double rx, double ry,
		  double phi_start, double phi_end)
{
    struct gerb_gdk_context *gctx = (struct gerb_gdk_context*) ctx;
    
    gdk_draw_arc (gctx->clipmask, gctx->clipmask_gc, 0,
		  round(x), round(y), round(rx), round(ry),
		  phi_start * 64.0, phi_end * 64.0);
} /* gerb_gdk_draw_arc */


static void 
gerb_gdk_fill_polygon(struct gerb_render_context *ctx, double *xy, int n)
{
    struct gerb_gdk_context *gctx = (struct gerb_gdk_context*) ctx;
    GdkPoint *points;
    int i;
    
    if (!(points = malloc (2 * n * sizeof(GdkPoint))))
	GERB_FATAL_ERROR("Malloc failed\n");
    
    for (i=0; i<n; i++) {
	points[i].x = round(xy[2*i]);
	points[i].y = round(xy[2*i+1]);
    }
    
    gdk_draw_polygon(gctx->clipmask, gctx->clipmask_gc, 1, points, n);
    
    free (points);
} /* gerb_gdk_fill_polygon */


static void 
gerb_gdk_fill_rectangle(struct gerb_render_context *ctx,
			double x, double y, double w, double h)
{
    struct gerb_gdk_context *gctx = (struct gerb_gdk_context*) ctx;
    
    gdk_draw_rectangle (gctx->clipmask, gctx->clipmask_gc, 1,
			round(x), round(y), round(w), round(h));
} /* gerb_gdk_fill_rectangle */ 


static void 
gerb_gdk_fill_oval(struct gerb_render_context *ctx,
		   double x, double y, double rx, double ry)
{
    struct gerb_gdk_context *gctx = (struct gerb_gdk_context*) ctx;
    
    gdk_draw_arc (gctx->clipmask, gctx->clipmask_gc, 1,
		  round(x), round(y), round(rx), round(ry),
		  360 * 64, 360 * 64);
} /* gerb_gdk_fill_oval */


static void 
gerb_gdk_set_color(struct gerb_render_context *ctx,
		    enum polarity_t polarity)
{
    struct gerb_gdk_context *gctx = (struct gerb_gdk_context*) ctx;
    GdkColor c;
    
    c.pixel = (polarity == CLEAR) ? 1 : 0;
    
    gdk_gc_set_foreground(gctx->clipmask_gc, &c);
} /* gerb_gdk_set_color */


static void 
gerb_gdk_set_line_style(struct gerb_render_context *ctx,
			double width, int dashed)
{
    struct gerb_gdk_context *gctx = (struct gerb_gdk_context*) ctx;
    
    gdk_gc_set_line_attributes (gctx->clipmask_gc, round(width),
				dashed ? GDK_LINE_ON_OFF_DASH : GDK_LINE_SOLID,
				GDK_CAP_ROUND,
				GDK_JOIN_MITER);
} /* gerb_gdk_set_line_style */


static void 
gerb_gdk_clear_composition(struct gerb_render_context *ctx,
			   const unsigned char rgba[4])
{
    struct gerb_gdk_context *gctx = (struct gerb_gdk_context*) ctx;
    GdkGC *local_gc = gdk_gc_new(gctx->pixmap);
    unsigned char r = rgba[0];
    unsigned char g = rgba[1];
    unsigned char b = rgba[2];
    GdkColor c;
    
    c.pixel = 0;
    c.red   = (r << 8) | r;
    c.green = (g << 8) | g;
    c.blue  = (b << 8) | b;
    
    gdk_colormap_alloc_color(gdk_colormap_get_system(), &c, FALSE, TRUE);
    
    gdk_gc_copy(local_gc, gctx->pixmap_gc);
    gdk_gc_set_foreground(local_gc, &c);
    gdk_draw_rectangle(gctx->pixmap, local_gc, TRUE, 0, 0, -1, -1);
    gdk_gc_unref (local_gc);
} /* gerb_gdk_clear_composition */


static void 
gerb_gdk_compose(struct gerb_render_context *ctx)
{
    struct gerb_gdk_context *gctx = (struct gerb_gdk_context*) ctx;
    unsigned char r = gctx->current_rgba[0];
    unsigned char g = gctx->current_rgba[1];
    unsigned char b = gctx->current_rgba[2];
    GdkColor c;
    
    c.pixel = 0;
    c.red   = (r << 8) | r;
    c.green = (g << 8) | g;
    c.blue  = (b << 8) | b;
    
    gdk_colormap_alloc_color(gdk_colormap_get_system(), &c, FALSE, TRUE);
    gdk_gc_set_foreground(gctx->pixmap_gc, &c);
    
    /*
     * Set clipmask and draw a colored rectangle using the clipmask 
     * as stencil. Afterwards we remove the clipmask, else
     * it will screw things up when run here again.
     */
    gdk_gc_set_clip_mask(gctx->pixmap_gc, gctx->clipmask);
    gdk_gc_set_clip_origin(gctx->pixmap_gc, 0, 0);
    gdk_draw_rectangle(gctx->pixmap, gctx->pixmap_gc, TRUE, 0, 0, -1, -1);
    gdk_gc_set_clip_mask(gctx->pixmap_gc, NULL);
} /* gerb_gdk_compose */


static void 
gerb_gdk_blit(struct gerb_render_context *ctx,
	      double offset_x, double offset_y)
{
    struct gerb_gdk_context *gctx = (struct gerb_gdk_context*) ctx;
    
    gdk_draw_pixmap (gctx->dst_drawable, gctx->dst_drawable_gc,
		     gctx->pixmap,
		     round(offset_x), round(offset_y), 0, 0, -1, -1);
} /* gerb_gdk_blit */


struct gerb_render_context* 
gerb_create_gdk_render_context(GdkDrawable *dest)
{
    struct gerb_gdk_context *gctx;
    
    if (!(gctx = malloc(sizeof(struct gerb_gdk_context))))
	GERB_FATAL_ERROR("Malloc failed\n");
    
    gctx->ctx.set_layer_color   = gerb_gdk_set_layer_color;
    gctx->ctx.set_viewport      = gerb_gdk_set_viewport;
    gctx->ctx.clear             = gerb_gdk_clear;
    gctx->ctx.draw_line         = gerb_gdk_draw_line;
    gctx->ctx.draw_linestrip    = gerb_gdk_draw_linestrip;
    gctx->ctx.draw_arc          = gerb_gdk_draw_arc;
    gctx->ctx.fill_polygon      = gerb_gdk_fill_polygon;
    gctx->ctx.fill_rectangle    = gerb_gdk_fill_rectangle;
    gctx->ctx.fill_oval         = gerb_gdk_fill_oval;
    gctx->ctx.set_color         = gerb_gdk_set_color;
    gctx->ctx.set_line_style    = gerb_gdk_set_line_style;
    gctx->ctx.compose           = gerb_gdk_compose;
    gctx->ctx.clear_composition = gerb_gdk_clear_composition;
    gctx->ctx.blit              = gerb_gdk_blit;
    
    gctx->dst_drawable          = dest;
    gctx->dst_drawable_gc       = NULL;
    gctx->pixmap                = NULL;
    gctx->pixmap_gc             = NULL;
    gctx->clipmask              = NULL;
    gctx->clipmask_gc           = NULL;
    
    return (struct gerb_render_context*) gctx;
} /* gerb_create_gdk_render_context */

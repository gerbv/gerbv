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

#ifndef DRAW_H
#define DRAW_H

#include "gerber.h"


struct gerb_render_context {
    void (*set_layer_color) (struct gerb_render_context *ctx, const unsigned char rgba [4]);
    void (*set_viewport) (struct gerb_render_context *ctx, int w, int h);
    void (*clear) (struct gerb_render_context *ctx, enum polarity_t color);
    void (*draw_line) (struct gerb_render_context *ctx, double x1, double y1, double x2, double y2);
    void (*draw_linestrip) (struct gerb_render_context *ctx, double *xy, int n);
    void (*draw_arc) (struct gerb_render_context *ctx, double cp_x, double cp_yy, double dx, double dy, double phi_start, double phi_delta);
    void (*fill_polygon) (struct gerb_render_context *ctx, double *xy, int n);
    void (*fill_rectangle) (struct gerb_render_context *ctx, double x, double y, double w, double h);
    void (*fill_oval) (struct gerb_render_context *ctx, double x, double y, double rx, double ry);
    void (*set_color) (struct gerb_render_context *ctx, enum polarity_t color);
    void (*set_line_style) (struct gerb_render_context *ctx, double width, int dashed, int cap_projecting);
    void (*clear_composition) (struct gerb_render_context *ctx, const unsigned char clear_rgba [4]);
    void (*compose) (struct gerb_render_context *ctx);
    void (*blit) (struct gerb_render_context *ctx, double offset_x, double offset_y);
};



/*
 * Convert a gerber image to a GDK clip mask to be used when creating pixmap
 */
extern
int gerb_render_image (struct gerb_render_context *ctx,
		       struct gerb_image *image, 
		       int scale, double trans_x, double trans_y);

extern
void gerb_render_set_viewport (struct gerb_render_context *ctx,
			       int width, int height);

extern
void gerb_render_clear_bg (struct gerb_render_context *ctx,
			   const unsigned char bg_rgba[4]);

extern
void gerb_render_set_color (struct gerb_render_context *ctx,
			    const unsigned char rgba[4]);

extern
void gerb_render_show (struct gerb_render_context *ctx,
		       double offset_x, double offset_y);

#endif /* DRAW_H */


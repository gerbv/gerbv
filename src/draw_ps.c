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
#include "draw_ps.h"


#undef round
#define round(x) (int)ceil((double)(x))


struct gerb_ps_context {
    struct gerb_render_context ctx;
    char *filename;
    FILE *fd;
    unsigned char current_rgba [4];
    double current_linewidth;
    int cap_projecting;
    int dashed;
};


static void 
gerb_ps_set_layer_color (struct gerb_render_context *ctx,
			 const unsigned char rgba[4])
{
    struct gerb_ps_context *gctx = (struct gerb_ps_context*) ctx;
    
    gctx->current_rgba[0] = rgba[0];
    gctx->current_rgba[1] = rgba[1];
    gctx->current_rgba[2] = rgba[2];
    gctx->current_rgba[3] = rgba[3];
} /* gerb_ps_set_color */


static void 
gerb_ps_set_viewport (struct gerb_render_context *ctx, int w, int h)
{
    struct gerb_ps_context *gctx = (struct gerb_ps_context*) ctx;
    FILE *fd = gctx->fd;

    /*
     * XXX Would [max|min][x|y] be better here?? 
     * XXX We must probably translate this so (min_x, min_y) is (0,0)
     */
    fprintf(fd, "gsave 72 setlinewidth newpath\n") ;
    fprintf(fd, "%d %d moveto\n",w,h);
    fprintf(fd, "0 %d lineto\n", h);
    fprintf(fd, "0 0  lineto\n");
    fprintf(fd, "%d 0 lineto\n", w);
    fprintf(fd, "closepath White fill grestore\n");

} /* gerb_ps_set_viewport */


static void 
gerb_ps_clear (struct gerb_render_context *ctx, enum polarity_t polarity)
{
    struct gerb_ps_context *gctx = (struct gerb_ps_context*) ctx;

    fprintf(gctx->fd, "%%gerb_ps_clear not implemented\n");
} /* gerb_ps_clear */


static void 
gerb_ps_draw_line(struct gerb_render_context *ctx,
		  double x1, double y1, double x2, double y2)
{
    struct gerb_ps_context *gctx = (struct gerb_ps_context*) ctx;
    
    fprintf(gctx->fd, "%.3f %.3f moveto %.3f %.3f lineto\n", x1, y1, x2, y2);

} /* gerb_ps_draw_line */


static void 
gerb_ps_draw_linestrip(struct gerb_render_context *ctx, double *xy, int n)
{
    struct gerb_ps_context *gctx = (struct gerb_ps_context*) ctx;
    int i;
    
    for (i=0; i<n-1; i++)
	fprintf(gctx->fd, "%.3f %.3f moveto %.3f %.3f lineto\n", 
		xy[2*i], xy[2*i+1],
		xy[2*i+2], xy[2*i+3]);

} /* gerb_ps_draw_linestrip */


static void 
gerb_ps_draw_arc(struct gerb_render_context *ctx,
		 double x, double y, double rx, double ry,
		 double phi_start, double phi_end)
{
    struct gerb_ps_context *gctx = (struct gerb_ps_context*) ctx;
    double cp_x, cp_y;

    cp_x = x + rx/2.0, cp_y = y + ry/2.0;

    fprintf(gctx->fd,"%.3f %.3f moveto ", 
	    cp_x + rx/2.0*cos(phi_start*M_PI/180.0), 
	    cp_y + ry/2.0*sin(phi_start*M_PI/180.0));
    
    fprintf(gctx->fd,"%.3f %.3f %.3f %.3f %.3f arc %% %.3f %.3f SUCKS!\n",
	    cp_x, cp_y, /* arc center point */
	    rx/2.0,
	    phi_start,  phi_start+phi_end, phi_start, phi_end);

} /* gerb_ps_draw_arc */


static void 
gerb_ps_fill_polygon(struct gerb_render_context *ctx, double *xy, int n)
{
    struct gerb_ps_context *gctx = (struct gerb_ps_context*) ctx;
    int i;

    fprintf(gctx->fd, "gsave newpath %.3f %.3f moveto \n", xy[0], xy[1]);
    for (i=1; i < n; i++)
	fprintf(gctx->fd, "%.3f %.3f lineto\n", xy[2*i], xy[2*i+1]);
    fprintf(gctx->fd, "closepath Black fill grestore\n");

} /* gerb_ps_fill_polygon */


static void 
gerb_ps_fill_rectangle (struct gerb_render_context *ctx,
			double x, double y, double w, double h)
{
    struct gerb_ps_context *gctx = (struct gerb_ps_context*) ctx;

    fprintf(gctx->fd, "%.3f %.3f %.3f %.3f rectangle\n", x, y, w, h);

} /* gerb_ps_fill_rectangle */


static void 
gerb_ps_fill_oval(struct gerb_render_context *ctx,
		  double x, double y, double rx, double ry)
{
    struct gerb_ps_context *gctx = (struct gerb_ps_context*) ctx;

    if (rx != ry) {
	fprintf(gctx->fd, "gsave 1 setlinewidth ");
	fprintf(gctx->fd, "%.3f %.3f %.3f %.3f 0 360 ellipse fill grestore\n", 
		x + rx/2.0, y + ry/2.0, rx/2.0, ry/2.0);
    } else
	fprintf(gctx->fd, "%.3f %.3f 0.000 %.3f circle\n", 
		x + rx/2.0, y + ry/2.0, rx);

} /* gerb_ps_fill_oval */


static void 
gerb_ps_set_color (struct gerb_render_context *ctx,
		   enum polarity_t polarity)
{
    struct gerb_ps_context *gctx = (struct gerb_ps_context*) ctx;

    fprintf(gctx->fd, "%%No color ");    

    switch (polarity) {
    case POSITIVE:
	fprintf(gctx->fd, "POSITIVE\n");
	break;
    case NEGATIVE:
	fprintf(gctx->fd, "NEGATIVE\n");
	break;
    case DARK:
	fprintf(gctx->fd, "DARK\n");
	break;
    case CLEAR:
	fprintf(gctx->fd, "CLEAR\n");
	break;
    default:
	fprintf(gctx->fd, "none\n");
    }

} /* gerb_ps_set_color */


static void 
gerb_ps_set_line_style (struct gerb_render_context *ctx,
			double width, int dashed, int cap_projecting)
{
    struct gerb_ps_context *gctx = (struct gerb_ps_context*) ctx;

    if (gctx->current_linewidth != width) {
	fprintf(gctx->fd, "stroke\n");
	fprintf(gctx->fd, "%f setlinewidth \n", width );
	gctx->current_linewidth = width;
    }

    if (dashed && !gctx->dashed) {
	fprintf(gctx->fd, "[4 4] setdash\n");
	gctx->dashed = 1;
    }
    if (!dashed && gctx->dashed) {
	fprintf(gctx->fd, "[] 0 setdash\n");
	gctx->dashed = 0;
    }

    if(cap_projecting && !gctx->cap_projecting) {
	fprintf(gctx->fd, "2 setlinecap\n");
	gctx->cap_projecting = 1;
    } 
    if(!cap_projecting && gctx->cap_projecting) {
	fprintf(gctx->fd, "1 setlinecap\n");
	gctx->cap_projecting = 0;
    }

} /* gerb_ps_set_line_style */


static void 
gerb_ps_clear_composition (struct gerb_render_context *ctx,
			   const unsigned char rgba[4])
{
    struct gerb_ps_context *gctx = (struct gerb_ps_context*) ctx;

    fprintf(gctx->fd, "%%Clear composition not implemented\n");

} /* gerb_ps_clear_composition */


static void 
gerb_ps_compose(struct gerb_render_context *ctx)
{
	struct gerb_ps_context *gctx = (struct gerb_ps_context*) ctx;

	fprintf(gctx->fd, "stroke\n");
} /* gerb_ps_compose */


static void 
gerb_ps_blit(struct gerb_render_context *ctx,
	     double offset_x, double offset_y)
{
	struct gerb_ps_context *gctx = (struct gerb_ps_context*) ctx;
#if 0
	fprintf(gctx->fd, "/Times-Roman findfont\n");
	fprintf(gctx->fd, "32 scalefont\n");
	fprintf(gctx->fd, "setfont\n");
	fprintf(gctx->fd, "%% -2 inch 0 inch translate\n");
	fprintf(gctx->fd, "-45 rotate\n");
	fprintf(gctx->fd, "2 1 scale\n");
	fprintf(gctx->fd, "newpath\n");
	fprintf(gctx->fd, "0 0 moveto\n");
	fprintf(gctx->fd, "(BUGLY) true charpath\n");
	fprintf(gctx->fd, "0.5 setlinewidth\n");
	fprintf(gctx->fd, "0.4 setgray\n");
	fprintf(gctx->fd, "stroke\n");
#endif
	fprintf(gctx->fd, "showpage\n");
	fclose(gctx->fd);
} /* gerb_ps_blit */



struct gerb_render_context* 
gerb_create_ps_render_context(char *filename)
{
	struct gerb_ps_context *gctx;
	FILE *fd;

	if (!(gctx = malloc(sizeof(struct gerb_ps_context))))
		GERB_FATAL_ERROR("Malloc failed\n");

	if ((fd = fopen(filename, "w")) == NULL)
		GERB_FATAL_ERROR("Couldn't open %s\n", filename);
	
	gctx->ctx.set_layer_color   = gerb_ps_set_layer_color;
	gctx->ctx.set_viewport      = gerb_ps_set_viewport;
	gctx->ctx.clear             = gerb_ps_clear;
	gctx->ctx.draw_line         = gerb_ps_draw_line;
	gctx->ctx.draw_linestrip    = gerb_ps_draw_linestrip;
	gctx->ctx.draw_arc          = gerb_ps_draw_arc;
	gctx->ctx.fill_polygon      = gerb_ps_fill_polygon;
	gctx->ctx.fill_rectangle    = gerb_ps_fill_rectangle;
	gctx->ctx.fill_oval         = gerb_ps_fill_oval;
	gctx->ctx.set_color         = gerb_ps_set_color;
	gctx->ctx.set_line_style    = gerb_ps_set_line_style;
	gctx->ctx.compose           = gerb_ps_compose;
	gctx->ctx.clear_composition = gerb_ps_clear_composition;
	gctx->ctx.blit              = gerb_ps_blit;

	gctx->filename              = filename;
	gctx->fd                    = fd;
	gctx->current_linewidth     = 0.0;
	gctx->cap_projecting        = 0;
	gctx->dashed                = 0;

	fprintf(fd, "%%!PS-Adobe-2.0\n");
	fprintf(fd, "%%%%Creator: gerbv\n\n");
	fprintf(fd, "/inch {72 mul} def\n");
	fprintf(fd, "/mm {25.4 div inch} def\n");

	fprintf(fd, "1 inch 1 inch translate 90 rotate\n");

#if 1
	fprintf(fd, "/Black {0 setgray} def\n");
        fprintf(fd, "/White {1 setgray} def\n");
#else
        fprintf(fd, "/Black {1 setgray} def %% Polarity reversed\n");
        fprintf(fd, "/White {0 setgray} def\n");
#endif
	fprintf(fd, "/rectangle { %% x y xl yl\n");
	fprintf(fd, "    gsave\n");
	fprintf(fd, "    newpath\n");
	fprintf(fd, "    1 setlinewidth\n");
	fprintf(fd, "    3 index 2 index 2 div sub\n");
	fprintf(fd, "    3 index 2 index 2 div add moveto\n");
	fprintf(fd, "    1 index 0 rlineto\n");         /*  -> */
	fprintf(fd, "    dup -1 mul 0 exch rlineto\n"); /* \!/ */
	fprintf(fd, "    1 index -1 mul 0 rlineto\n");  /* <-  */
	fprintf(fd, "    dup 0 exch rlineto\n");        /* /!\ */
	fprintf(fd, "    pop pop pop pop closepath  Black fill\n");
	fprintf(fd, "    grestore\n");
	fprintf(fd, "} def\n");

	/* Stolen from the ps "blue book" */
	fprintf(fd, "/ellipsedict 8 dict def \n");
	fprintf(fd, "ellipsedict /mtrx matrix put\n");
	fprintf(fd, "/ellipse\n");
	fprintf(fd, " { ellipsedict begin\n");
	fprintf(fd, "   /endangle exch def\n");
	fprintf(fd, "   /startangle exch def\n");
	fprintf(fd, "   /yrad exch def\n");
	fprintf(fd, "   /xrad exch def\n");
	fprintf(fd, "   /y exch def\n");
	fprintf(fd, "   /x exch def\n");
	fprintf(fd, "   x y moveto\n"); /* Added */
	fprintf(fd, "   /savematrix mtrx currentmatrix def\n");
	fprintf(fd, "   x y translate\n");
	fprintf(fd, "   xrad yrad scale\n");
	fprintf(fd, "   0 0 1 startangle endangle arc\n");
	fprintf(fd, "   savematrix setmatrix\n");
	fprintf(fd, "  end\n");
	fprintf(fd, "} def\n");

	fprintf(fd, "/circle { %% x y id od\n");
	fprintf(fd, "    gsave\n");
	fprintf(fd, "    3 index 3 index moveto\n");
	fprintf(fd, "    3 index 3 index 3 2 roll %% Fix arguments\n");
	fprintf(fd, "    2 div %% d given, need r\n");
	fprintf(fd, "    0 360 arc Black fill %% outer\n");
	fprintf(fd, "    2 div %% d given, need r\n");
	fprintf(fd, "    0 360 arc White fill %%inner\n");
	fprintf(fd, "grestore\n");
	fprintf(fd, "} def\n");

	fprintf(fd, "\n1 setlinecap\n");

	return (struct gerb_render_context*) gctx;
} /* gerb_create_ps_render_context */

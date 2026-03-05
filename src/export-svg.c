/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2026 Source Parts contributors
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

/** \file export-svg.c
    \brief Optimized SVG export that writes SVG directly from gerbv data structures,
           bypassing Cairo's SVG surface for dramatically smaller output files.
    \ingroup libgerbv
*/

#include "gerbv.h"
#include "common.h"
#include "export-svg.h"

#include <math.h>
#include <string.h>
#include <float.h>
#include <glib/gstdio.h>

/* ------------------------------------------------------------------ */
/*  Coordinate formatting                                              */
/* ------------------------------------------------------------------ */

/** Format a coordinate value with reduced precision and stripped trailing zeros */
static void
svg_fmt_coord (GString *out, gdouble val)
{
	gchar buf[32];

	g_snprintf (buf, sizeof (buf), "%.*f", SVG_COORD_PRECISION, val);

	/* Strip trailing zeros after decimal point */
	if (strchr (buf, '.')) {
		gchar *end = buf + strlen (buf) - 1;

		while (*end == '0')
			*end-- = '\0';

		if (*end == '.')
			*end = '\0';
	}

	g_string_append (out, buf);
}

/** Append "x,y" formatted coordinate pair */
static void
svg_fmt_xy (GString *out, gdouble x, gdouble y)
{
	svg_fmt_coord (out, x);
	g_string_append_c (out, ',');
	svg_fmt_coord (out, y);
}

/* ------------------------------------------------------------------ */
/*  Indentation helpers                                                */
/* ------------------------------------------------------------------ */

static void
svg_indent (GString *out, gint level)
{
	for (gint i = 0; i < level; i++)
		g_string_append (out, "  ");
}

/* ------------------------------------------------------------------ */
/*  Style registry                                                     */
/* ------------------------------------------------------------------ */

/**
 * Find or create a style class for the given fill color.
 * Returns the class name (e.g. "c0").
 */
static const gchar *
svg_style_register_fill (svg_writer_ctx_t *ctx,
		gdouble r, gdouble g, gdouble b)
{
	svg_style_registry_t *reg = &ctx->styles;

	for (gint i = 0; i < reg->count; i++) {
		svg_style_class_t *sc = &reg->classes[i];

		if (sc->has_fill && !sc->has_stroke
		&&  fabs (sc->fill_r - r) < 0.001
		&&  fabs (sc->fill_g - g) < 0.001
		&&  fabs (sc->fill_b - b) < 0.001)
			return sc->class_name;
	}

	if (reg->count >= SVG_MAX_STYLES)
		return "c0";

	svg_style_class_t *sc = &reg->classes[reg->count];
	sc->fill_r = r;
	sc->fill_g = g;
	sc->fill_b = b;
	sc->has_fill = TRUE;
	sc->has_stroke = FALSE;
	g_snprintf (sc->class_name, sizeof (sc->class_name), "c%d", reg->count);
	reg->count++;

	return sc->class_name;
}

/**
 * Find or create a style class for a stroke with given color, width, and linecap.
 */
static const gchar *
svg_style_register_stroke (svg_writer_ctx_t *ctx,
		gdouble r, gdouble g, gdouble b,
		gdouble width, gint linecap)
{
	svg_style_registry_t *reg = &ctx->styles;

	for (gint i = 0; i < reg->count; i++) {
		svg_style_class_t *sc = &reg->classes[i];

		if (sc->has_stroke
		&&  fabs (sc->stroke_r - r) < 0.001
		&&  fabs (sc->stroke_g - g) < 0.001
		&&  fabs (sc->stroke_b - b) < 0.001
		&&  fabs (sc->stroke_width - width) < 0.0001
		&&  sc->stroke_linecap == linecap
		&&  sc->has_fill == FALSE)
			return sc->class_name;
	}

	if (reg->count >= SVG_MAX_STYLES)
		return "c0";

	svg_style_class_t *sc = &reg->classes[reg->count];
	sc->stroke_r = r;
	sc->stroke_g = g;
	sc->stroke_b = b;
	sc->stroke_width = width;
	sc->stroke_linecap = linecap;
	sc->has_fill = FALSE;
	sc->has_stroke = TRUE;
	g_snprintf (sc->class_name, sizeof (sc->class_name), "c%d", reg->count);
	reg->count++;

	return sc->class_name;
}

/** Emit the <style> block into defs */
static void
svg_emit_style_block (svg_writer_ctx_t *ctx)
{
	svg_style_registry_t *reg = &ctx->styles;
	static const gchar *cap_names[] = { "butt", "round", "square" };

	if (reg->count == 0)
		return;

	g_string_append (ctx->defs, "  <style>\n");

	for (gint i = 0; i < reg->count; i++) {
		svg_style_class_t *sc = &reg->classes[i];

		g_string_append_printf (ctx->defs, "    .%s{", sc->class_name);

		if (sc->has_fill) {
			gint ri = (gint)(sc->fill_r * 255 + 0.5);
			gint gi = (gint)(sc->fill_g * 255 + 0.5);
			gint bi = (gint)(sc->fill_b * 255 + 0.5);

			g_string_append_printf (ctx->defs,
				"fill:rgb(%d,%d,%d);", ri, gi, bi);
		} else {
			g_string_append (ctx->defs, "fill:none;");
		}

		if (sc->has_stroke) {
			gint ri = (gint)(sc->stroke_r * 255 + 0.5);
			gint gi = (gint)(sc->stroke_g * 255 + 0.5);
			gint bi = (gint)(sc->stroke_b * 255 + 0.5);
			gint cap = CLAMP (sc->stroke_linecap, 0, 2);

			g_string_append_printf (ctx->defs,
				"stroke:rgb(%d,%d,%d);", ri, gi, bi);
			g_string_append (ctx->defs, "stroke-width:");
			svg_fmt_coord (ctx->defs, sc->stroke_width);
			g_string_append_printf (ctx->defs,
				";stroke-linecap:%s;", cap_names[cap]);
		}

		g_string_append (ctx->defs, "}\n");
	}

	g_string_append (ctx->defs, "  </style>\n");
}

/* ------------------------------------------------------------------ */
/*  Macro primitive rendering into SVG                                 */
/* ------------------------------------------------------------------ */

/**
 * Render all simplified macro primitives for an aperture flash.
 * Instead of defining a reusable symbol (macro shapes vary per-flash
 * due to parameterization), we inline the shapes at the flash point.
 */
static void
svg_draw_amacro (svg_writer_ctx_t *ctx, const gchar *fill_class,
		gerbv_simplified_amacro_t *s, gint usesClearPrimitive)
{
	gerbv_simplified_amacro_t *ls = s;

	while (ls != NULL) {
		gboolean isClear = FALSE;

		switch (ls->type) {
		case GERBV_APTYPE_MACRO_CIRCLE: {
			isClear = (ls->parameter[CIRCLE_EXPOSURE] == 0.0);
			const gchar *cls = isClear ?
				svg_style_register_fill (ctx,
					ctx->bg_r, ctx->bg_g, ctx->bg_b) :
				fill_class;
			gdouble cx = ls->parameter[CIRCLE_CENTER_X];
			gdouble cy = ls->parameter[CIRCLE_CENTER_Y];
			gdouble dia = ls->parameter[CIRCLE_DIAMETER];
			gdouble rot = ls->parameter[CIRCLE_ROTATION];

			if (fabs (rot) > 0.001) {
				svg_indent (ctx->body, ctx->indent);
				g_string_append_printf (ctx->body,
					"<g transform=\"rotate(");
				svg_fmt_coord (ctx->body, rot);
				g_string_append (ctx->body, ")\">\n");
				ctx->indent++;
			}

			svg_indent (ctx->body, ctx->indent);
			g_string_append_printf (ctx->body,
				"<circle class=\"%s\" cx=\"", cls);
			svg_fmt_coord (ctx->body, cx);
			g_string_append (ctx->body, "\" cy=\"");
			svg_fmt_coord (ctx->body, cy);
			g_string_append (ctx->body, "\" r=\"");
			svg_fmt_coord (ctx->body, dia / 2.0);
			g_string_append (ctx->body, "\"/>\n");

			if (fabs (rot) > 0.001) {
				ctx->indent--;
				svg_indent (ctx->body, ctx->indent);
				g_string_append (ctx->body, "</g>\n");
			}
			break;
		}
		case GERBV_APTYPE_MACRO_OUTLINE: {
			isClear = (ls->parameter[OUTLINE_EXPOSURE] == 0.0);
			const gchar *cls = isClear ?
				svg_style_register_fill (ctx,
					ctx->bg_r, ctx->bg_g, ctx->bg_b) :
				fill_class;
			gdouble rot = ls->parameter[
				OUTLINE_ROTATION_IDX(ls->parameter)];
			gint npoints = (gint)ls->parameter[
				OUTLINE_NUMBER_OF_POINTS];

			if (fabs (rot) > 0.001) {
				svg_indent (ctx->body, ctx->indent);
				g_string_append (ctx->body,
					"<g transform=\"rotate(");
				svg_fmt_coord (ctx->body, rot);
				g_string_append (ctx->body, ")\">\n");
				ctx->indent++;
			}

			svg_indent (ctx->body, ctx->indent);
			g_string_append_printf (ctx->body,
				"<polygon class=\"%s\" points=\"", cls);

			for (gint pt = 0; pt <= npoints; pt++) {
				if (pt > 0)
					g_string_append_c (ctx->body, ' ');

				svg_fmt_xy (ctx->body,
					ls->parameter[OUTLINE_X_IDX_OF_POINT(pt)],
					ls->parameter[OUTLINE_Y_IDX_OF_POINT(pt)]);
			}

			g_string_append (ctx->body, "\"/>\n");

			if (fabs (rot) > 0.001) {
				ctx->indent--;
				svg_indent (ctx->body, ctx->indent);
				g_string_append (ctx->body, "</g>\n");
			}
			break;
		}
		case GERBV_APTYPE_MACRO_POLYGON: {
			isClear = (ls->parameter[POLYGON_EXPOSURE] == 0.0);
			const gchar *cls = isClear ?
				svg_style_register_fill (ctx,
					ctx->bg_r, ctx->bg_g, ctx->bg_b) :
				fill_class;
			gdouble cx = ls->parameter[POLYGON_CENTER_X];
			gdouble cy = ls->parameter[POLYGON_CENTER_Y];
			gdouble dia = ls->parameter[POLYGON_DIAMETER];
			gint nsides = (gint)ls->parameter[POLYGON_NUMBER_OF_POINTS];
			gdouble rot = ls->parameter[POLYGON_ROTATION];

			svg_indent (ctx->body, ctx->indent);
			g_string_append_printf (ctx->body,
				"<polygon class=\"%s\" points=\"", cls);

			for (gint i = 0; i < nsides; i++) {
				gdouble angle = DEG2RAD(rot) +
					((gdouble)i) * 2.0 * M_PI / nsides;
				gdouble px = cx + cos (angle) * dia / 2.0;
				gdouble py = cy + sin (angle) * dia / 2.0;

				if (i > 0)
					g_string_append_c (ctx->body, ' ');

				svg_fmt_xy (ctx->body, px, py);
			}

			g_string_append (ctx->body, "\"/>\n");
			break;
		}
		case GERBV_APTYPE_MACRO_MOIRE: {
			gdouble cx = ls->parameter[MOIRE_CENTER_X];
			gdouble cy = ls->parameter[MOIRE_CENTER_Y];
			gdouble rot = ls->parameter[MOIRE_ROTATION];
			gdouble outsideDia = ls->parameter[MOIRE_OUTSIDE_DIAMETER];
			gdouble thickness = ls->parameter[MOIRE_CIRCLE_THICKNESS];
			gdouble gap = ls->parameter[MOIRE_GAP_WIDTH];
			gint numCircles = (gint)ls->parameter[MOIRE_NUMBER_OF_CIRCLES];
			gdouble crossThick = ls->parameter[MOIRE_CROSSHAIR_THICKNESS];
			gdouble crossLen = ls->parameter[MOIRE_CROSSHAIR_LENGTH];

			/* Register stroke style for concentric circles */
			const gchar *circle_cls = svg_style_register_stroke (
				ctx, ctx->bg_r, ctx->bg_g, ctx->bg_b,
				thickness, 0);

			svg_indent (ctx->body, ctx->indent);
			g_string_append (ctx->body,
				"<g transform=\"translate(");
			svg_fmt_xy (ctx->body, cx, cy);

			if (fabs (rot) > 0.001) {
				g_string_append (ctx->body, ") rotate(");
				svg_fmt_coord (ctx->body, rot);
			}

			g_string_append (ctx->body, ")\">\n");
			ctx->indent++;

			/* Concentric ring strokes */
			gdouble diameter = outsideDia - thickness;
			gdouble diaDiff = 2.0 * (gap + thickness);

			for (gint c = 0; c < numCircles; c++) {
				gdouble dia = diameter - diaDiff * c;

				if (dia <= 0)
					continue;

				svg_indent (ctx->body, ctx->indent);
				g_string_append_printf (ctx->body,
					"<circle class=\"%s\" cx=\"0\" cy=\"0\" r=\"",
					circle_cls);
				svg_fmt_coord (ctx->body, dia / 2.0);
				g_string_append (ctx->body, "\"/>\n");
			}

			/* Crosshair lines */
			const gchar *cross_cls = svg_style_register_stroke (
				ctx, ctx->bg_r, ctx->bg_g, ctx->bg_b,
				crossThick, 0);
			gdouble halfCross = crossLen / 2.0;

			svg_indent (ctx->body, ctx->indent);
			g_string_append_printf (ctx->body,
				"<line class=\"%s\" x1=\"", cross_cls);
			svg_fmt_coord (ctx->body, -halfCross);
			g_string_append (ctx->body, "\" y1=\"0\" x2=\"");
			svg_fmt_coord (ctx->body, halfCross);
			g_string_append (ctx->body, "\" y2=\"0\"/>\n");

			svg_indent (ctx->body, ctx->indent);
			g_string_append_printf (ctx->body,
				"<line class=\"%s\" x1=\"0\" y1=\"", cross_cls);
			svg_fmt_coord (ctx->body, -halfCross);
			g_string_append (ctx->body, "\" x2=\"0\" y2=\"");
			svg_fmt_coord (ctx->body, halfCross);
			g_string_append (ctx->body, "\"/>\n");

			ctx->indent--;
			svg_indent (ctx->body, ctx->indent);
			g_string_append (ctx->body, "</g>\n");
			break;
		}
		case GERBV_APTYPE_MACRO_THERMAL: {
			gdouble cx = ls->parameter[THERMAL_CENTER_X];
			gdouble cy = ls->parameter[THERMAL_CENTER_Y];
			gdouble outerDia = ls->parameter[THERMAL_OUTSIDE_DIAMETER];
			gdouble innerDia = ls->parameter[THERMAL_INSIDE_DIAMETER];
			gdouble crossThick = ls->parameter[THERMAL_CROSSHAIR_THICKNESS];
			gdouble rot = ls->parameter[THERMAL_ROTATION];

			gdouble startAngle1 = asin (crossThick / innerDia);
			gdouble endAngle1 = M_PI_2 - startAngle1;
			gdouble endAngle2 = asin (crossThick / outerDia);
			gdouble startAngle2 = M_PI_2 - endAngle2;

			svg_indent (ctx->body, ctx->indent);
			g_string_append (ctx->body,
				"<g transform=\"translate(");
			svg_fmt_xy (ctx->body, cx, cy);

			if (fabs (rot) > 0.001) {
				g_string_append (ctx->body, ") rotate(");
				svg_fmt_coord (ctx->body, rot);
			}

			g_string_append_printf (ctx->body,
				")\" class=\"%s\">\n", fill_class);
			ctx->indent++;

			gdouble ri = innerDia / 2.0;
			gdouble ro = outerDia / 2.0;

			for (gint q = 0; q < 4; q++) {
				gdouble qrot = q * M_PI_2;
				/* Inner arc start */
				gdouble ix1 = ri * cos (startAngle1 + qrot);
				gdouble iy1 = ri * sin (startAngle1 + qrot);
				/* Inner arc end */
				gdouble ix2 = ri * cos (endAngle1 + qrot);
				gdouble iy2 = ri * sin (endAngle1 + qrot);
				/* Outer arc start (from endAngle perspective) */
				gdouble ox1 = ro * cos (startAngle2 + qrot);
				gdouble oy1 = ro * sin (startAngle2 + qrot);
				/* Outer arc end */
				gdouble ox2 = ro * cos (endAngle2 + qrot);
				gdouble oy2 = ro * sin (endAngle2 + qrot);

				svg_indent (ctx->body, ctx->indent);
				g_string_append (ctx->body, "<path d=\"M ");
				svg_fmt_xy (ctx->body, ix1, iy1);
				g_string_append (ctx->body, " A ");
				svg_fmt_coord (ctx->body, ri);
				g_string_append_c (ctx->body, ',');
				svg_fmt_coord (ctx->body, ri);
				g_string_append (ctx->body, " 0 0,1 ");
				svg_fmt_xy (ctx->body, ix2, iy2);
				g_string_append (ctx->body, " L ");
				svg_fmt_xy (ctx->body, ox1, oy1);
				g_string_append (ctx->body, " A ");
				svg_fmt_coord (ctx->body, ro);
				g_string_append_c (ctx->body, ',');
				svg_fmt_coord (ctx->body, ro);
				g_string_append (ctx->body, " 0 0,0 ");
				svg_fmt_xy (ctx->body, ox2, oy2);
				g_string_append (ctx->body, " Z\"/>\n");
			}

			ctx->indent--;
			svg_indent (ctx->body, ctx->indent);
			g_string_append (ctx->body, "</g>\n");
			break;
		}
		case GERBV_APTYPE_MACRO_LINE20: {
			isClear = (ls->parameter[LINE20_EXPOSURE] == 0.0);
			const gchar *cls = isClear ?
				svg_style_register_stroke (ctx,
					ctx->bg_r, ctx->bg_g, ctx->bg_b,
					ls->parameter[LINE20_LINE_WIDTH], 0) :
				svg_style_register_stroke (ctx, 0, 0, 0,
					ls->parameter[LINE20_LINE_WIDTH], 0);
			gdouble rot = ls->parameter[LINE20_ROTATION];

			if (fabs (rot) > 0.001) {
				svg_indent (ctx->body, ctx->indent);
				g_string_append (ctx->body,
					"<g transform=\"rotate(");
				svg_fmt_coord (ctx->body, rot);
				g_string_append (ctx->body, ")\">\n");
				ctx->indent++;
			}

			svg_indent (ctx->body, ctx->indent);
			g_string_append_printf (ctx->body,
				"<line class=\"%s\" x1=\"", cls);
			svg_fmt_coord (ctx->body,
				ls->parameter[LINE20_START_X]);
			g_string_append (ctx->body, "\" y1=\"");
			svg_fmt_coord (ctx->body,
				ls->parameter[LINE20_START_Y]);
			g_string_append (ctx->body, "\" x2=\"");
			svg_fmt_coord (ctx->body,
				ls->parameter[LINE20_END_X]);
			g_string_append (ctx->body, "\" y2=\"");
			svg_fmt_coord (ctx->body,
				ls->parameter[LINE20_END_Y]);
			g_string_append (ctx->body, "\"/>\n");

			if (fabs (rot) > 0.001) {
				ctx->indent--;
				svg_indent (ctx->body, ctx->indent);
				g_string_append (ctx->body, "</g>\n");
			}
			break;
		}
		case GERBV_APTYPE_MACRO_LINE21: {
			isClear = (ls->parameter[LINE21_EXPOSURE] == 0.0);
			const gchar *cls = isClear ?
				svg_style_register_fill (ctx,
					ctx->bg_r, ctx->bg_g, ctx->bg_b) :
				fill_class;
			gdouble w = ls->parameter[LINE21_WIDTH];
			gdouble h = ls->parameter[LINE21_HEIGHT];
			gdouble cx = ls->parameter[LINE21_CENTER_X];
			gdouble cy = ls->parameter[LINE21_CENTER_Y];
			gdouble rot = ls->parameter[LINE21_ROTATION];

			if (fabs (rot) > 0.001) {
				svg_indent (ctx->body, ctx->indent);
				g_string_append (ctx->body,
					"<g transform=\"rotate(");
				svg_fmt_coord (ctx->body, rot);
				g_string_append (ctx->body, ")\">\n");
				ctx->indent++;
			}

			svg_indent (ctx->body, ctx->indent);
			g_string_append_printf (ctx->body,
				"<rect class=\"%s\" x=\"", cls);
			svg_fmt_coord (ctx->body, cx - w / 2.0);
			g_string_append (ctx->body, "\" y=\"");
			svg_fmt_coord (ctx->body, cy - h / 2.0);
			g_string_append (ctx->body, "\" width=\"");
			svg_fmt_coord (ctx->body, w);
			g_string_append (ctx->body, "\" height=\"");
			svg_fmt_coord (ctx->body, h);
			g_string_append (ctx->body, "\"/>\n");

			if (fabs (rot) > 0.001) {
				ctx->indent--;
				svg_indent (ctx->body, ctx->indent);
				g_string_append (ctx->body, "</g>\n");
			}
			break;
		}
		case GERBV_APTYPE_MACRO_LINE22: {
			isClear = (ls->parameter[LINE22_EXPOSURE] == 0.0);
			const gchar *cls = isClear ?
				svg_style_register_fill (ctx,
					ctx->bg_r, ctx->bg_g, ctx->bg_b) :
				fill_class;
			gdouble w = ls->parameter[LINE22_WIDTH];
			gdouble h = ls->parameter[LINE22_HEIGHT];
			gdouble llx = ls->parameter[LINE22_LOWER_LEFT_X];
			gdouble lly = ls->parameter[LINE22_LOWER_LEFT_Y];
			gdouble rot = ls->parameter[LINE22_ROTATION];

			if (fabs (rot) > 0.001) {
				svg_indent (ctx->body, ctx->indent);
				g_string_append (ctx->body,
					"<g transform=\"rotate(");
				svg_fmt_coord (ctx->body, rot);
				g_string_append (ctx->body, ")\">\n");
				ctx->indent++;
			}

			svg_indent (ctx->body, ctx->indent);
			g_string_append_printf (ctx->body,
				"<rect class=\"%s\" x=\"", cls);
			svg_fmt_coord (ctx->body, llx);
			g_string_append (ctx->body, "\" y=\"");
			svg_fmt_coord (ctx->body, lly);
			g_string_append (ctx->body, "\" width=\"");
			svg_fmt_coord (ctx->body, w);
			g_string_append (ctx->body, "\" height=\"");
			svg_fmt_coord (ctx->body, h);
			g_string_append (ctx->body, "\"/>\n");

			if (fabs (rot) > 0.001) {
				ctx->indent--;
				svg_indent (ctx->body, ctx->indent);
				g_string_append (ctx->body, "</g>\n");
			}
			break;
		}
		default:
			break;
		}

		ls = ls->next;
	}
}

/* ------------------------------------------------------------------ */
/*  Polygon area fill rendering                                        */
/* ------------------------------------------------------------------ */

/**
 * Render a polygon area fill (PAREA_START ... PAREA_END) into SVG.
 * Walks the net chain from oldNet->next until PAREA_END, building
 * a single <path> element.
 */
static void
svg_render_polygon_area (svg_writer_ctx_t *ctx, gerbv_net_t *oldNet,
		gdouble sr_x, gdouble sr_y, const gchar *fill_class,
		gboolean isClear)
{
	gerbv_net_t *currentNet;
	gboolean haveFirst = FALSE;
	const gchar *cls;

	if (isClear)
		cls = svg_style_register_fill (ctx,
			ctx->bg_r, ctx->bg_g, ctx->bg_b);
	else
		cls = fill_class;

	svg_indent (ctx->body, ctx->indent);
	g_string_append_printf (ctx->body, "<path class=\"%s\" d=\"", cls);

	for (currentNet = oldNet->next; currentNet != NULL;
			currentNet = currentNet->next) {
		gdouble x2 = currentNet->stop_x + sr_x;
		gdouble y2 = currentNet->stop_y + sr_y;

		if (!haveFirst) {
			g_string_append (ctx->body, "M ");
			svg_fmt_xy (ctx->body, x2, y2);
			haveFirst = TRUE;
			continue;
		}

		switch (currentNet->interpolation) {
		case GERBV_INTERPOLATION_LINEARx1:
		case GERBV_INTERPOLATION_LINEARx10:
		case GERBV_INTERPOLATION_LINEARx01:
		case GERBV_INTERPOLATION_LINEARx001:
			g_string_append (ctx->body, " L ");
			svg_fmt_xy (ctx->body, x2, y2);
			break;

		case GERBV_INTERPOLATION_CW_CIRCULAR:
		case GERBV_INTERPOLATION_CCW_CIRCULAR: {
			if (!currentNet->cirseg)
				break;

			gdouble cp_x = currentNet->cirseg->cp_x + sr_x;
			gdouble cp_y = currentNet->cirseg->cp_y + sr_y;
			gdouble rx = currentNet->cirseg->width / 2.0;
			gdouble ry = currentNet->cirseg->height / 2.0;
			gdouble a1 = currentNet->cirseg->angle1;
			gdouble a2 = currentNet->cirseg->angle2;

			/* Determine arc direction */
			gint sweep;
			gint large_arc;

			if (a2 > a1) {
				/* cairo_arc (positive direction) */
				gdouble delta = a2 - a1;
				sweep = 1;
				large_arc = (delta > 180.0) ? 1 : 0;
			} else {
				/* cairo_arc_negative */
				gdouble delta = a1 - a2;
				sweep = 0;
				large_arc = (delta > 180.0) ? 1 : 0;
			}

			/* Endpoint (computed from cirseg center + angle2) */
			gdouble ex = cp_x + rx * cos (DEG2RAD(a2));
			gdouble ey = cp_y + ry * sin (DEG2RAD(a2));

			g_string_append (ctx->body, " A ");
			svg_fmt_coord (ctx->body, rx);
			g_string_append_c (ctx->body, ',');
			svg_fmt_coord (ctx->body, ry);
			g_string_append_printf (ctx->body,
				" 0 %d,%d ", large_arc, sweep);
			svg_fmt_xy (ctx->body, ex, ey);
			break;
		}
		case GERBV_INTERPOLATION_PAREA_END:
			g_string_append (ctx->body, " Z\"/>\n");
			return;

		default:
			break;
		}
	}

	/* Shouldn't reach here, but close the path just in case */
	g_string_append (ctx->body, "\"/>\n");
}

/* ------------------------------------------------------------------ */
/*  Netstate transform                                                 */
/* ------------------------------------------------------------------ */

/** Build a transform attribute string for a netstate */
static gchar *
svg_netstate_transform (gerbv_netstate_t *state)
{
	GString *t = g_string_new (NULL);
	gboolean hasTransform = FALSE;

	if (fabs (state->scaleA - 1.0) > 0.0001 ||
	    fabs (state->scaleB - 1.0) > 0.0001) {
		g_string_append (t, "scale(");
		svg_fmt_coord (t, state->scaleA);
		g_string_append_c (t, ',');
		svg_fmt_coord (t, state->scaleB);
		g_string_append (t, ") ");
		hasTransform = TRUE;
	}

	if (fabs (state->offsetA) > 0.0001 ||
	    fabs (state->offsetB) > 0.0001) {
		g_string_append (t, "translate(");
		svg_fmt_coord (t, state->offsetA);
		g_string_append_c (t, ',');
		svg_fmt_coord (t, state->offsetB);
		g_string_append (t, ") ");
		hasTransform = TRUE;
	}

	switch (state->mirrorState) {
	case GERBV_MIRROR_STATE_FLIPA:
		g_string_append (t, "scale(-1,1) ");
		hasTransform = TRUE;
		break;
	case GERBV_MIRROR_STATE_FLIPB:
		g_string_append (t, "scale(1,-1) ");
		hasTransform = TRUE;
		break;
	case GERBV_MIRROR_STATE_FLIPAB:
		g_string_append (t, "scale(-1,-1) ");
		hasTransform = TRUE;
		break;
	default:
		break;
	}

	if (state->axisSelect == GERBV_AXIS_SELECT_SWAPAB) {
		g_string_append (t, "rotate(270) scale(1,-1) ");
		hasTransform = TRUE;
	}

	if (!hasTransform) {
		g_string_free (t, TRUE);
		return NULL;
	}

	return g_string_free (t, FALSE);
}

/* ------------------------------------------------------------------ */
/*  Layer rendering                                                    */
/* ------------------------------------------------------------------ */

/**
 * Render a single image (layer) to the SVG body.
 * This replicates the logic of draw_image_to_cairo_target() but
 * outputs SVG elements instead of Cairo drawing commands.
 */
static void
svg_render_layer (svg_writer_ctx_t *ctx, gerbv_image_t *image,
		gerbv_user_transformation_t transform,
		gdouble color_r, gdouble color_g, gdouble color_b)
{
	gerbv_net_t *net;
	gerbv_netstate_t *oldState;
	gerbv_layer_t *oldLayer;
	gboolean invertPolarity = FALSE;
	gdouble scaleX = transform.scaleX;
	gdouble scaleY = transform.scaleY;
	gint stateGroupOpen = 0;
	gint layerGroupOpen = 0;

	/* The fill class for this layer's color */
	const gchar *darkFillClass = svg_style_register_fill (ctx,
		color_r, color_g, color_b);
	const gchar *clearFillClass = svg_style_register_fill (ctx,
		ctx->bg_r, ctx->bg_g, ctx->bg_b);
	const gchar *currentFillClass;

	if (transform.mirrorAroundX)
		scaleY *= -1;
	if (transform.mirrorAroundY)
		scaleX *= -1;

	/* Emit user transform group */
	{
		GString *userTransform = g_string_new (NULL);

		if (fabs (transform.translateX) > 0.0001 ||
		    fabs (transform.translateY) > 0.0001) {
			g_string_append (userTransform, "translate(");
			svg_fmt_xy (userTransform, transform.translateX,
				transform.translateY);
			g_string_append (userTransform, ") ");
		}

		if (fabs (scaleX - 1.0) > 0.0001 ||
		    fabs (scaleY - 1.0) > 0.0001) {
			g_string_append (userTransform, "scale(");
			svg_fmt_xy (userTransform, scaleX, scaleY);
			g_string_append (userTransform, ") ");
		}

		if (fabs (transform.rotation) > 0.0001) {
			g_string_append (userTransform, "rotate(");
			svg_fmt_coord (userTransform,
				transform.rotation * 180.0 / M_PI);
			g_string_append (userTransform, ") ");
		}

		svg_indent (ctx->body, ctx->indent);
		if (userTransform->len > 0) {
			g_string_append_printf (ctx->body,
				"<g transform=\"%s\" fill-rule=\"evenodd\">\n",
				userTransform->str);
		} else {
			g_string_append (ctx->body,
				"<g fill-rule=\"evenodd\">\n");
		}

		g_string_free (userTransform, TRUE);
	}
	ctx->indent++;

	/* Image justify offset */
	if (fabs (image->info->imageJustifyOffsetActualA) > 0.0001 ||
	    fabs (image->info->imageJustifyOffsetActualB) > 0.0001) {
		svg_indent (ctx->body, ctx->indent);
		g_string_append (ctx->body, "<g transform=\"translate(");
		svg_fmt_xy (ctx->body,
			image->info->imageJustifyOffsetActualA,
			image->info->imageJustifyOffsetActualB);
		g_string_append (ctx->body, ")\">\n");
		ctx->indent++;
	}

	/* Image offset */
	if (fabs (image->info->offsetA) > 0.0001 ||
	    fabs (image->info->offsetB) > 0.0001) {
		svg_indent (ctx->body, ctx->indent);
		g_string_append (ctx->body, "<g transform=\"translate(");
		svg_fmt_xy (ctx->body, image->info->offsetA, image->info->offsetB);
		g_string_append (ctx->body, ")\">\n");
		ctx->indent++;
	}

	/* Image rotation */
	if (fabs (image->info->imageRotation) > 0.0001) {
		svg_indent (ctx->body, ctx->indent);
		g_string_append (ctx->body, "<g transform=\"rotate(");
		svg_fmt_coord (ctx->body,
			image->info->imageRotation * 180.0 / M_PI);
		g_string_append (ctx->body, ")\">\n");
		ctx->indent++;
	}

	/* Polarity setup */
	invertPolarity = transform.inverted;
	if (image->info->polarity == GERBV_POLARITY_NEGATIVE)
		invertPolarity = !invertPolarity;

	if (invertPolarity) {
		/* In inverted mode: "dark" draws background, "clear" draws foreground */
		currentFillClass = clearFillClass;
		/* Paint the entire area with the fill color first */
		/* This is tricky in SVG without knowing bounds, but we can use a large rect */
		/* We'll handle this with a background rect in the layer group */
	} else {
		currentFillClass = darkFillClass;
	}

	/* Open layer and state groups (simulating the two cairo_save() calls) */
	svg_indent (ctx->body, ctx->indent);
	g_string_append (ctx->body, "<g>\n");
	ctx->indent++;
	layerGroupOpen = 1;

	svg_indent (ctx->body, ctx->indent);
	g_string_append (ctx->body, "<g>\n");
	ctx->indent++;
	stateGroupOpen = 1;

	oldLayer = image->layers;
	oldState = image->states;

	for (net = image->netlist->next; net != NULL;
			net = gerbv_image_return_next_renderable_object (net)) {

		/* Check for new layer */
		if (net->layer != oldLayer) {
			/* Close state and layer groups */
			if (stateGroupOpen) {
				ctx->indent--;
				svg_indent (ctx->body, ctx->indent);
				g_string_append (ctx->body, "</g>\n");
			}
			if (layerGroupOpen) {
				ctx->indent--;
				svg_indent (ctx->body, ctx->indent);
				g_string_append (ctx->body, "</g>\n");
			}

			/* Open new layer group with rotation */
			svg_indent (ctx->body, ctx->indent);
			if (fabs (net->layer->rotation) > 0.0001) {
				g_string_append (ctx->body,
					"<g transform=\"rotate(");
				svg_fmt_coord (ctx->body,
					net->layer->rotation * 180.0 / M_PI);
				g_string_append (ctx->body, ")\">\n");
			} else {
				g_string_append (ctx->body, "<g>\n");
			}
			ctx->indent++;
			layerGroupOpen = 1;

			/* Update polarity based on layer */
			if ((net->layer->polarity == GERBV_POLARITY_CLEAR) ^ invertPolarity) {
				currentFillClass = clearFillClass;
			} else {
				currentFillClass = darkFillClass;
			}

			/* Handle knockout areas */
			gerbv_knockout_t *ko = &net->layer->knockout;
			if (ko->firstInstance == TRUE) {
				const gchar *koClass;

				if (ko->polarity == GERBV_POLARITY_CLEAR) {
					koClass = clearFillClass;
				} else {
					koClass = darkFillClass;
				}

				/* For vector export fix: clear polarity uses bg color */
				if ((net->layer->polarity == GERBV_POLARITY_CLEAR) ^ invertPolarity) {
					/* Draw knockout with clear=bg workaround */
					if (ko->polarity == GERBV_POLARITY_CLEAR) {
						koClass = svg_style_register_fill (ctx,
							ctx->bg_r, ctx->bg_g, ctx->bg_b);
					}
				}

				svg_indent (ctx->body, ctx->indent);
				g_string_append_printf (ctx->body,
					"<rect class=\"%s\" x=\"", koClass);
				svg_fmt_coord (ctx->body, ko->lowerLeftX - ko->border);
				g_string_append (ctx->body, "\" y=\"");
				svg_fmt_coord (ctx->body, ko->lowerLeftY - ko->border);
				g_string_append (ctx->body, "\" width=\"");
				svg_fmt_coord (ctx->body, ko->width + 2 * ko->border);
				g_string_append (ctx->body, "\" height=\"");
				svg_fmt_coord (ctx->body, ko->height + 2 * ko->border);
				g_string_append (ctx->body, "\"/>\n");
			}

			/* Open new state group */
			gchar *stateTransform = svg_netstate_transform (net->state);
			svg_indent (ctx->body, ctx->indent);
			if (stateTransform) {
				g_string_append_printf (ctx->body,
					"<g transform=\"%s\">\n", stateTransform);
				g_free (stateTransform);
			} else {
				g_string_append (ctx->body, "<g>\n");
			}
			ctx->indent++;
			stateGroupOpen = 1;

			oldLayer = net->layer;
		}

		/* Check for new netstate */
		if (net->state != oldState) {
			/* Close and reopen state group */
			if (stateGroupOpen) {
				ctx->indent--;
				svg_indent (ctx->body, ctx->indent);
				g_string_append (ctx->body, "</g>\n");
			}

			gchar *stateTransform = svg_netstate_transform (net->state);
			svg_indent (ctx->body, ctx->indent);
			if (stateTransform) {
				g_string_append_printf (ctx->body,
					"<g transform=\"%s\">\n", stateTransform);
				g_free (stateTransform);
			} else {
				g_string_append (ctx->body, "<g>\n");
			}
			ctx->indent++;
			stateGroupOpen = 1;

			oldState = net->state;
		}

		/* Step and repeat */
		gerbv_step_and_repeat_t *sr = &net->layer->stepAndRepeat;

		for (gint ix = 0; ix < sr->X; ix++) {
			for (gint iy = 0; iy < sr->Y; iy++) {
				gdouble sr_x = ix * sr->dist_X;
				gdouble sr_y = iy * sr->dist_Y;

				gdouble x1 = net->start_x + sr_x;
				gdouble y1 = net->start_y + sr_y;
				gdouble x2 = net->stop_x + sr_x;
				gdouble y2 = net->stop_y + sr_y;

				/* Determine if we're in clear mode */
				gboolean isClearOp =
					((net->layer->polarity == GERBV_POLARITY_CLEAR)
					 ^ invertPolarity);

				/* Polygon area fill */
				if (net->interpolation == GERBV_INTERPOLATION_PAREA_START) {
					svg_render_polygon_area (ctx, net,
						sr_x, sr_y, currentFillClass,
						isClearOp);
					continue;
				}

				if (net->interpolation == GERBV_INTERPOLATION_DELETED)
					continue;

				/* Check aperture exists */
				if (image->aperture[net->aperture] == NULL)
					continue;

				switch (net->aperture_state) {
				case GERBV_APERTURE_STATE_ON: {
					/* Stroke operations */
					gerbv_aperture_t *ap = image->aperture[net->aperture];
					gdouble lineWidth = ap->parameter[0];
					const gchar *strokeClass;
					gint linecap;

					switch (net->interpolation) {
					case GERBV_INTERPOLATION_LINEARx1:
					case GERBV_INTERPOLATION_LINEARx10:
					case GERBV_INTERPOLATION_LINEARx01:
					case GERBV_INTERPOLATION_LINEARx001:
						switch (ap->type) {
						case GERBV_APTYPE_CIRCLE:
							linecap = 1; /* round */
							if (isClearOp)
								strokeClass = svg_style_register_stroke (ctx,
									ctx->bg_r, ctx->bg_g, ctx->bg_b,
									lineWidth, linecap);
							else
								strokeClass = svg_style_register_stroke (ctx,
									color_r, color_g, color_b,
									lineWidth, linecap);

							svg_indent (ctx->body, ctx->indent);
							g_string_append_printf (ctx->body,
								"<line class=\"%s\" x1=\"",
								strokeClass);
							svg_fmt_coord (ctx->body, x1);
							g_string_append (ctx->body, "\" y1=\"");
							svg_fmt_coord (ctx->body, y1);
							g_string_append (ctx->body, "\" x2=\"");
							svg_fmt_coord (ctx->body, x2);
							g_string_append (ctx->body, "\" y2=\"");
							svg_fmt_coord (ctx->body, y2);
							g_string_append (ctx->body, "\"/>\n");
							break;

						case GERBV_APTYPE_RECTANGLE: {
							/* Rectangle stroke draws a filled polygon */
							gdouble dx = ap->parameter[0] / 2.0;
							gdouble dy = ap->parameter[1] / 2.0;
							const gchar *cls;

							if (x1 > x2) dx = -dx;
							if (y1 > y2) dy = -dy;

							if (isClearOp)
								cls = svg_style_register_fill (ctx,
									ctx->bg_r, ctx->bg_g, ctx->bg_b);
							else
								cls = currentFillClass;

							svg_indent (ctx->body, ctx->indent);
							g_string_append_printf (ctx->body,
								"<polygon class=\"%s\" points=\"",
								cls);
							svg_fmt_xy (ctx->body, x1 - dx, y1 - dy);
							g_string_append_c (ctx->body, ' ');
							svg_fmt_xy (ctx->body, x1 - dx, y1 + dy);
							g_string_append_c (ctx->body, ' ');
							svg_fmt_xy (ctx->body, x2 - dx, y2 + dy);
							g_string_append_c (ctx->body, ' ');
							svg_fmt_xy (ctx->body, x2 + dx, y2 + dy);
							g_string_append_c (ctx->body, ' ');
							svg_fmt_xy (ctx->body, x2 + dx, y2 - dy);
							g_string_append_c (ctx->body, ' ');
							svg_fmt_xy (ctx->body, x1 + dx, y1 - dy);
							g_string_append (ctx->body, "\"/>\n");
							break;
						}
						case GERBV_APTYPE_OVAL:
						case GERBV_APTYPE_POLYGON:
							/* Render like circle for strokes (matching draw.c behavior) */
							linecap = 1;
							if (isClearOp)
								strokeClass = svg_style_register_stroke (ctx,
									ctx->bg_r, ctx->bg_g, ctx->bg_b,
									lineWidth, linecap);
							else
								strokeClass = svg_style_register_stroke (ctx,
									color_r, color_g, color_b,
									lineWidth, linecap);

							svg_indent (ctx->body, ctx->indent);
							g_string_append_printf (ctx->body,
								"<line class=\"%s\" x1=\"",
								strokeClass);
							svg_fmt_coord (ctx->body, x1);
							g_string_append (ctx->body, "\" y1=\"");
							svg_fmt_coord (ctx->body, y1);
							g_string_append (ctx->body, "\" x2=\"");
							svg_fmt_coord (ctx->body, x2);
							g_string_append (ctx->body, "\" y2=\"");
							svg_fmt_coord (ctx->body, y2);
							g_string_append (ctx->body, "\"/>\n");
							break;

						default:
							break;
						}
						break;

					case GERBV_INTERPOLATION_CW_CIRCULAR:
					case GERBV_INTERPOLATION_CCW_CIRCULAR: {
						if (!net->cirseg)
							break;

						gdouble cp_x = net->cirseg->cp_x + sr_x;
						gdouble cp_y = net->cirseg->cp_y + sr_y;
						gdouble a1 = net->cirseg->angle1;
						gdouble a2 = net->cirseg->angle2;
						gdouble rw = net->cirseg->width / 2.0;
						gdouble rh = net->cirseg->height / 2.0;

						/* Start point from angle1 */
						gdouble sx = cp_x + rw * cos (DEG2RAD(a1));
						gdouble sy = cp_y + rh * sin (DEG2RAD(a1));
						/* End point from angle2 */
						gdouble ex = cp_x + rw * cos (DEG2RAD(a2));
						gdouble ey = cp_y + rh * sin (DEG2RAD(a2));

						gint sweep, large_arc;
						if (a2 > a1) {
							gdouble delta = a2 - a1;
							sweep = 1;
							large_arc = (delta > 180.0) ? 1 : 0;
						} else {
							gdouble delta = a1 - a2;
							sweep = 0;
							large_arc = (delta > 180.0) ? 1 : 0;
						}

						linecap = (ap->type == GERBV_APTYPE_RECTANGLE) ? 2 : 1;
						if (isClearOp)
							strokeClass = svg_style_register_stroke (ctx,
								ctx->bg_r, ctx->bg_g, ctx->bg_b,
								lineWidth, linecap);
						else
							strokeClass = svg_style_register_stroke (ctx,
								color_r, color_g, color_b,
								lineWidth, linecap);

						svg_indent (ctx->body, ctx->indent);
						g_string_append_printf (ctx->body,
							"<path class=\"%s\" d=\"M ", strokeClass);
						svg_fmt_xy (ctx->body, sx, sy);
						g_string_append (ctx->body, " A ");
						svg_fmt_coord (ctx->body, rw);
						g_string_append_c (ctx->body, ',');
						svg_fmt_coord (ctx->body, rh);
						g_string_append_printf (ctx->body,
							" 0 %d,%d ", large_arc, sweep);
						svg_fmt_xy (ctx->body, ex, ey);
						g_string_append (ctx->body, "\"/>\n");
						break;
					}
					default:
						break;
					}
					break;
				}
				case GERBV_APERTURE_STATE_OFF:
					break;

				case GERBV_APERTURE_STATE_FLASH: {
					gerbv_aperture_t *ap = image->aperture[net->aperture];
					gdouble *p = ap->parameter;
					const gchar *cls;

					if (isClearOp)
						cls = clearFillClass;
					else
						cls = currentFillClass;

					switch (ap->type) {
					case GERBV_APTYPE_CIRCLE:
						svg_indent (ctx->body, ctx->indent);
						g_string_append_printf (ctx->body,
							"<circle class=\"%s\" cx=\"", cls);
						svg_fmt_coord (ctx->body, x2);
						g_string_append (ctx->body, "\" cy=\"");
						svg_fmt_coord (ctx->body, y2);
						g_string_append (ctx->body, "\" r=\"");
						svg_fmt_coord (ctx->body, p[0] / 2.0);
						g_string_append (ctx->body, "\"/>\n");

						/* Aperture hole */
						if (p[1] > 0) {
							const gchar *holeCls = isClearOp ?
								currentFillClass : clearFillClass;

							if (p[2] > 0) {
								/* Rectangular hole */
								svg_indent (ctx->body, ctx->indent);
								g_string_append_printf (ctx->body,
									"<rect class=\"%s\" x=\"", holeCls);
								svg_fmt_coord (ctx->body, x2 - p[1] / 2.0);
								g_string_append (ctx->body, "\" y=\"");
								svg_fmt_coord (ctx->body, y2 - p[2] / 2.0);
								g_string_append (ctx->body, "\" width=\"");
								svg_fmt_coord (ctx->body, p[1]);
								g_string_append (ctx->body, "\" height=\"");
								svg_fmt_coord (ctx->body, p[2]);
								g_string_append (ctx->body, "\"/>\n");
							} else {
								/* Circular hole */
								svg_indent (ctx->body, ctx->indent);
								g_string_append_printf (ctx->body,
									"<circle class=\"%s\" cx=\"", holeCls);
								svg_fmt_coord (ctx->body, x2);
								g_string_append (ctx->body, "\" cy=\"");
								svg_fmt_coord (ctx->body, y2);
								g_string_append (ctx->body, "\" r=\"");
								svg_fmt_coord (ctx->body, p[1] / 2.0);
								g_string_append (ctx->body, "\"/>\n");
							}
						}
						break;

					case GERBV_APTYPE_RECTANGLE:
						svg_indent (ctx->body, ctx->indent);
						g_string_append_printf (ctx->body,
							"<rect class=\"%s\" x=\"", cls);
						svg_fmt_coord (ctx->body, x2 - p[0] / 2.0);
						g_string_append (ctx->body, "\" y=\"");
						svg_fmt_coord (ctx->body, y2 - p[1] / 2.0);
						g_string_append (ctx->body, "\" width=\"");
						svg_fmt_coord (ctx->body, p[0]);
						g_string_append (ctx->body, "\" height=\"");
						svg_fmt_coord (ctx->body, p[1]);
						g_string_append (ctx->body, "\"/>\n");

						/* Aperture hole */
						if (p[2] > 0) {
							const gchar *holeCls = isClearOp ?
								currentFillClass : clearFillClass;

							if (p[3] > 0) {
								svg_indent (ctx->body, ctx->indent);
								g_string_append_printf (ctx->body,
									"<rect class=\"%s\" x=\"", holeCls);
								svg_fmt_coord (ctx->body, x2 - p[2] / 2.0);
								g_string_append (ctx->body, "\" y=\"");
								svg_fmt_coord (ctx->body, y2 - p[3] / 2.0);
								g_string_append (ctx->body, "\" width=\"");
								svg_fmt_coord (ctx->body, p[2]);
								g_string_append (ctx->body, "\" height=\"");
								svg_fmt_coord (ctx->body, p[3]);
								g_string_append (ctx->body, "\"/>\n");
							} else {
								svg_indent (ctx->body, ctx->indent);
								g_string_append_printf (ctx->body,
									"<circle class=\"%s\" cx=\"", holeCls);
								svg_fmt_coord (ctx->body, x2);
								g_string_append (ctx->body, "\" cy=\"");
								svg_fmt_coord (ctx->body, y2);
								g_string_append (ctx->body, "\" r=\"");
								svg_fmt_coord (ctx->body, p[2] / 2.0);
								g_string_append (ctx->body, "\"/>\n");
							}
						}
						break;

					case GERBV_APTYPE_OVAL: {
						/* Inline oblong path at flash position */
						svg_indent (ctx->body, ctx->indent);
						g_string_append_printf (ctx->body,
							"<path class=\"%s\" d=\"", cls);

						if (p[0] < p[1]) {
							gdouble r = p[0] / 2.0;
							gdouble hs = (p[1] - p[0]) / 2.0;

							g_string_append (ctx->body, "M ");
							svg_fmt_xy (ctx->body, x2 + r, y2 + hs);
							g_string_append (ctx->body, " A ");
							svg_fmt_coord (ctx->body, r);
							g_string_append_c (ctx->body, ',');
							svg_fmt_coord (ctx->body, r);
							g_string_append (ctx->body, " 0 1,0 ");
							svg_fmt_xy (ctx->body, x2 - r, y2 + hs);
							g_string_append (ctx->body, " L ");
							svg_fmt_xy (ctx->body, x2 - r, y2 - hs);
							g_string_append (ctx->body, " A ");
							svg_fmt_coord (ctx->body, r);
							g_string_append_c (ctx->body, ',');
							svg_fmt_coord (ctx->body, r);
							g_string_append (ctx->body, " 0 1,0 ");
							svg_fmt_xy (ctx->body, x2 + r, y2 - hs);
						} else {
							gdouble r = p[1] / 2.0;
							gdouble hs = (p[0] - p[1]) / 2.0;

							g_string_append (ctx->body, "M ");
							svg_fmt_xy (ctx->body, x2 + hs, y2 + r);
							g_string_append (ctx->body, " A ");
							svg_fmt_coord (ctx->body, r);
							g_string_append_c (ctx->body, ',');
							svg_fmt_coord (ctx->body, r);
							g_string_append (ctx->body, " 0 1,0 ");
							svg_fmt_xy (ctx->body, x2 + hs, y2 - r);
							g_string_append (ctx->body, " L ");
							svg_fmt_xy (ctx->body, x2 - hs, y2 - r);
							g_string_append (ctx->body, " A ");
							svg_fmt_coord (ctx->body, r);
							g_string_append_c (ctx->body, ',');
							svg_fmt_coord (ctx->body, r);
							g_string_append (ctx->body, " 0 1,0 ");
							svg_fmt_xy (ctx->body, x2 - hs, y2 + r);
						}

						g_string_append (ctx->body, " Z\"/>\n");

						/* Aperture hole */
						if (p[2] > 0) {
							const gchar *holeCls = isClearOp ?
								currentFillClass : clearFillClass;

							if (p[3] > 0) {
								svg_indent (ctx->body, ctx->indent);
								g_string_append_printf (ctx->body,
									"<rect class=\"%s\" x=\"", holeCls);
								svg_fmt_coord (ctx->body, x2 - p[2] / 2.0);
								g_string_append (ctx->body, "\" y=\"");
								svg_fmt_coord (ctx->body, y2 - p[3] / 2.0);
								g_string_append (ctx->body, "\" width=\"");
								svg_fmt_coord (ctx->body, p[2]);
								g_string_append (ctx->body, "\" height=\"");
								svg_fmt_coord (ctx->body, p[3]);
								g_string_append (ctx->body, "\"/>\n");
							} else {
								svg_indent (ctx->body, ctx->indent);
								g_string_append_printf (ctx->body,
									"<circle class=\"%s\" cx=\"", holeCls);
								svg_fmt_coord (ctx->body, x2);
								g_string_append (ctx->body, "\" cy=\"");
								svg_fmt_coord (ctx->body, y2);
								g_string_append (ctx->body, "\" r=\"");
								svg_fmt_coord (ctx->body, p[2] / 2.0);
								g_string_append (ctx->body, "\"/>\n");
							}
						}
						break;
					}
					case GERBV_APTYPE_POLYGON: {
						gint n = (gint)p[1];
						gdouble rot = DEG2RAD(p[2]);

						svg_indent (ctx->body, ctx->indent);
						g_string_append_printf (ctx->body,
							"<polygon class=\"%s\" points=\"", cls);

						for (gint i = 0; i < n; i++) {
							gdouble angle = rot +
								((gdouble)i) * 2.0 * M_PI / n;
							gdouble px = x2 + cos (angle) * p[0] / 2.0;
							gdouble py = y2 + sin (angle) * p[0] / 2.0;

							if (i > 0)
								g_string_append_c (ctx->body, ' ');

							svg_fmt_xy (ctx->body, px, py);
						}

						g_string_append (ctx->body, "\"/>\n");

						/* Aperture hole */
						if (p[3] > 0) {
							const gchar *holeCls = isClearOp ?
								currentFillClass : clearFillClass;

							if (p[4] > 0) {
								svg_indent (ctx->body, ctx->indent);
								g_string_append_printf (ctx->body,
									"<rect class=\"%s\" x=\"", holeCls);
								svg_fmt_coord (ctx->body, x2 - p[3] / 2.0);
								g_string_append (ctx->body, "\" y=\"");
								svg_fmt_coord (ctx->body, y2 - p[4] / 2.0);
								g_string_append (ctx->body, "\" width=\"");
								svg_fmt_coord (ctx->body, p[3]);
								g_string_append (ctx->body, "\" height=\"");
								svg_fmt_coord (ctx->body, p[4]);
								g_string_append (ctx->body, "\"/>\n");
							} else {
								svg_indent (ctx->body, ctx->indent);
								g_string_append_printf (ctx->body,
									"<circle class=\"%s\" cx=\"", holeCls);
								svg_fmt_coord (ctx->body, x2);
								g_string_append (ctx->body, "\" cy=\"");
								svg_fmt_coord (ctx->body, y2);
								g_string_append (ctx->body, "\" r=\"");
								svg_fmt_coord (ctx->body, p[3] / 2.0);
								g_string_append (ctx->body, "\"/>\n");
							}
						}
						break;
					}
					case GERBV_APTYPE_MACRO: {
						/* Macro flash: translate to position and inline all primitives */
						svg_indent (ctx->body, ctx->indent);
						g_string_append (ctx->body,
							"<g transform=\"translate(");
						svg_fmt_xy (ctx->body, x2, y2);
						g_string_append (ctx->body, ")\">\n");
						ctx->indent++;

						svg_draw_amacro (ctx, currentFillClass,
							ap->simplified,
							(gint)p[0]);

						ctx->indent--;
						svg_indent (ctx->body, ctx->indent);
						g_string_append (ctx->body, "</g>\n");
						break;
					}
					default:
						break;
					}
					break;
				}
				default:
					break;
				}
			}
		}
	}

	/* Close state and layer groups */
	if (stateGroupOpen) {
		ctx->indent--;
		svg_indent (ctx->body, ctx->indent);
		g_string_append (ctx->body, "</g>\n");
	}
	if (layerGroupOpen) {
		ctx->indent--;
		svg_indent (ctx->body, ctx->indent);
		g_string_append (ctx->body, "</g>\n");
	}

	/* Close image rotation group */
	if (fabs (image->info->imageRotation) > 0.0001) {
		ctx->indent--;
		svg_indent (ctx->body, ctx->indent);
		g_string_append (ctx->body, "</g>\n");
	}

	/* Close image offset group */
	if (fabs (image->info->offsetA) > 0.0001 ||
	    fabs (image->info->offsetB) > 0.0001) {
		ctx->indent--;
		svg_indent (ctx->body, ctx->indent);
		g_string_append (ctx->body, "</g>\n");
	}

	/* Close image justify group */
	if (fabs (image->info->imageJustifyOffsetActualA) > 0.0001 ||
	    fabs (image->info->imageJustifyOffsetActualB) > 0.0001) {
		ctx->indent--;
		svg_indent (ctx->body, ctx->indent);
		g_string_append (ctx->body, "</g>\n");
	}

	/* Close user transform group */
	ctx->indent--;
	svg_indent (ctx->body, ctx->indent);
	g_string_append (ctx->body, "</g>\n");
}

/* ------------------------------------------------------------------ */
/*  Project-level rendering                                            */
/* ------------------------------------------------------------------ */

void
export_svg_render_project (gerbv_project_t *project,
		gerbv_render_info_t *renderInfo,
		const gchar *filename,
		gboolean exportLayersAsSvgLayers)
{
	svg_writer_ctx_t ctx;
	GdkColor *bg = &project->background;
	GString *svgOut;

	memset (&ctx, 0, sizeof (ctx));
	ctx.body = g_string_sized_new (65536);
	ctx.defs = g_string_sized_new (4096);
	ctx.renderInfo = renderInfo;
	ctx.indent = 1;

	/* Background color (normalized to 0..1) */
	ctx.bg_r = (gdouble)bg->red / G_MAXUINT16;
	ctx.bg_g = (gdouble)bg->green / G_MAXUINT16;
	ctx.bg_b = (gdouble)bg->blue / G_MAXUINT16;

	/* Global transform: replicate gerbv_render_cairo_set_scale_and_translation */
	gdouble translateX = renderInfo->lowerLeftX * renderInfo->scaleFactorX;
	gdouble translateY = renderInfo->lowerLeftY * renderInfo->scaleFactorY;

	/* Render each visible layer (back to front, matching Cairo path) */
	for (gint i = project->last_loaded; i >= 0; i--) {
		gerbv_fileinfo_t *fileInfo = project->file[i];

		if (fileInfo == NULL || !fileInfo->isVisible)
			continue;

		gdouble lr = (gdouble)fileInfo->color.red / G_MAXUINT16;
		gdouble lg = (gdouble)fileInfo->color.green / G_MAXUINT16;
		gdouble lb = (gdouble)fileInfo->color.blue / G_MAXUINT16;

		if (exportLayersAsSvgLayers) {
			gchar *labelEscaped = g_markup_escape_text (
				fileInfo->name ? fileInfo->name : _("Unnamed layer"), -1);

			svg_indent (ctx.body, ctx.indent);
			g_string_append_printf (ctx.body,
				"<g inkscape:groupmode=\"layer\" inkscape:label=\"%s\">\n",
				labelEscaped);
			ctx.indent++;
			g_free (labelEscaped);
		}

		svg_render_layer (&ctx, fileInfo->image,
			fileInfo->transform, lr, lg, lb);

		if (exportLayersAsSvgLayers) {
			ctx.indent--;
			svg_indent (ctx.body, ctx.indent);
			g_string_append (ctx.body, "</g>\n");
		}
	}

	/* Now assemble the final SVG document */
	/* First pass complete: all styles have been collected.
	   Emit the style block into defs. */
	svg_emit_style_block (&ctx);

	/* Add the aperture hole style */
	if (ctx.styles.count > 0) {
		/* ap-hole class uses even-odd fill for cutout holes */
	}

	svgOut = g_string_sized_new (ctx.defs->len + ctx.body->len + 1024);

	/* SVG header */
	g_string_append (svgOut, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	g_string_append (svgOut, "<svg xmlns=\"http://www.w3.org/2000/svg\"");

	if (exportLayersAsSvgLayers) {
		g_string_append (svgOut,
			" xmlns:inkscape=\"http://www.inkscape.org/namespaces/inkscape\"");
	}

	g_string_append (svgOut, " version=\"1.1\"");
	g_string_append_printf (svgOut, " width=\"%d\" height=\"%d\"",
		renderInfo->displayWidth, renderInfo->displayHeight);
	g_string_append_printf (svgOut, " viewBox=\"0 0 %d %d\">\n",
		renderInfo->displayWidth, renderInfo->displayHeight);

	/* Defs block */
	if (ctx.defs->len > 0) {
		g_string_append (svgOut, "<defs>\n");
		g_string_append (svgOut, ctx.defs->str);
		g_string_append (svgOut, "</defs>\n");
	}

	/* Background rectangle (matching existing behavior: skip pure white/black) */
	if ((bg->red != 0xffff || bg->green != 0xffff || bg->blue != 0xffff)
	 && (bg->red != 0x0000 || bg->green != 0x0000 || bg->blue != 0x0000)) {
		g_string_append_printf (svgOut,
			"<rect x=\"0\" y=\"0\" width=\"100%%\" height=\"100%%\" fill=\"rgb(%u,%u,%u)\"/>\n",
			bg->red / 257, bg->green / 257, bg->blue / 257);
	}

	/* Global coordinate transform group:
	 * translate(-llx*sx, lly*sy + dh) scale(sx, -sy)
	 * This flips Y axis (Gerber Y-up to SVG Y-down). */
	g_string_append (svgOut, "<g transform=\"translate(");
	svg_fmt_coord (svgOut, -translateX);
	g_string_append_c (svgOut, ',');
	svg_fmt_coord (svgOut, translateY + renderInfo->displayHeight);
	g_string_append (svgOut, ") scale(");
	svg_fmt_coord (svgOut, renderInfo->scaleFactorX);
	g_string_append_c (svgOut, ',');
	svg_fmt_coord (svgOut, -renderInfo->scaleFactorY);
	g_string_append (svgOut, ")\">\n");

	/* Body content */
	g_string_append (svgOut, ctx.body->str);

	/* Close global transform group */
	g_string_append (svgOut, "</g>\n");
	g_string_append (svgOut, "</svg>\n");

	/* Write to file */
	if (!g_file_set_contents (filename, svgOut->str, svgOut->len, NULL)) {
		GERB_COMPILE_ERROR (_("Exporting error to file \"%s\""), filename);
	}

	g_string_free (svgOut, TRUE);
	g_string_free (ctx.body, TRUE);
	g_string_free (ctx.defs, TRUE);
}

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

/** \file draw.c
    \brief Cairo rendering functions and the related selection calculating functions
    \ingroup libgerbv
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h> /* ceil(), atan2() */

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "gerbv.h"
#include "draw.h"
#include "common.h"
#include "selection.h"

#define dprintf \
    if (DEBUG)  \
    printf

static gboolean draw_do_vector_export_fix(cairo_t* cairoTarget, double* bg_red, double* bg_green, double* bg_blue);

/** Draw Cairo line from current coordinates.
  @param x	End of line x coordinate.
  @param y	End of line y coordinate.
  @param adjustByHalf	Increase x and y by 0.5.
  @param pixelOutput	Round x and y coordinates to pixels.
*/
void
draw_cairo_line_to(cairo_t* cairoTarget, gdouble x, gdouble y, gboolean adjustByHalf, gboolean pixelOutput) {
    if (pixelOutput) {
        cairo_user_to_device(cairoTarget, &x, &y);
        x = round(x);
        y = round(y);
        if (adjustByHalf) {
            x += 0.5;
            y += 0.5;
        }
        cairo_device_to_user(cairoTarget, &x, &y);
    }
    cairo_line_to(cairoTarget, x, y);
}

/** Move Cairo coordinates.
  @param x	Move to x coordinate.
  @param y	Move to y coordinate.
  @param oddWidth	Increase x and y by 0.5.
  @param pixelOutput	Round x and y coordinates to pixels.
*/
void
draw_cairo_move_to(cairo_t* cairoTarget, gdouble x, gdouble y, gboolean oddWidth, gboolean pixelOutput) {
    if (pixelOutput) {
        cairo_user_to_device(cairoTarget, &x, &y);
        x = round(x);
        y = round(y);
        if (oddWidth) {
            x += 0.5;
            y += 0.5;
        }
        cairo_device_to_user(cairoTarget, &x, &y);
    }
    cairo_move_to(cairoTarget, x, y);
}

/** Cairo translate user-space origin.
  @param x	The x coordinate.
  @param y	The y coordinate.
  @param pixelOutput	Round x and y coordinates to pixels.
*/
void
draw_cairo_translate_adjust(cairo_t* cairoTarget, gdouble x, gdouble y, gboolean pixelOutput) {
    if (pixelOutput) {
        cairo_user_to_device(cairoTarget, &x, &y);
        x = round(x);
        y = round(y);
        cairo_device_to_user(cairoTarget, &x, &y);
    }

    cairo_translate(cairoTarget, x, y);
}

/** Check if net is in selection buffer and possibly deselect it.
  @return TRUE if net is selected, FALSE if not selected.
  @param net	Checked net.
  @param selectionInfo	Selection buffer.
  @param remove		TRUE for deselect net.
 */
static gboolean
draw_net_is_in_selection_buffer_remove(gerbv_net_t* net, gerbv_selection_info_t* selectionInfo, gboolean remove) {
    gerbv_selection_item_t sItem;

    for (guint i = 0; i < selection_length(selectionInfo); i++) {
        sItem = selection_get_item_by_index(selectionInfo, i);
        if (sItem.net == net) {
            if (remove)
                selection_clear_item_by_index(selectionInfo, i);

            return TRUE;
        }
    }

    return FALSE;
}

static void
draw_check_if_object_is_in_selected_area(
    cairo_t* cairoTarget, gboolean isStroke, gerbv_selection_info_t* selectionInfo, gerbv_image_t* image,
    struct gerbv_net* net, enum draw_mode drawMode
) {
    gerbv_selection_item_t sItem = { image, net };
    gdouble                corner1X, corner1Y, corner2X, corner2Y;
    gdouble                x1, x2, y1, y2;
    gdouble                minX, minY, maxX, maxY;

    corner1X = selectionInfo->lowerLeftX;
    corner1Y = selectionInfo->lowerLeftY;
    corner2X = selectionInfo->upperRightX;
    corner2Y = selectionInfo->upperRightY;

    /* calculate the coordinate of the user's click in the current
       transformation matrix */
    cairo_device_to_user(cairoTarget, &corner1X, &corner1Y);
    cairo_device_to_user(cairoTarget, &corner2X, &corner2Y);

    switch (selectionInfo->type) {
        case GERBV_SELECTION_POINT_CLICK:
            /* use the cairo in_fill routine to see if the point is within the
               drawn area */
            if ((isStroke && cairo_in_stroke(cairoTarget, corner1X, corner1Y))
                || (!isStroke && cairo_in_fill(cairoTarget, corner1X, corner1Y))) {

                if (!draw_net_is_in_selection_buffer_remove(net, selectionInfo, (drawMode == FIND_SELECTIONS_TOGGLE))) {
                    selection_add_item(selectionInfo, &sItem);
                }
            }
            break;

        case GERBV_SELECTION_DRAG_BOX:
            /* we can't assume the "lowerleft" corner is actually in the lower left,
               since the cairo transformation matrix may be mirrored,etc */
            minX = MIN(corner1X, corner2X);
            maxX = MAX(corner1X, corner2X);
            minY = MIN(corner1Y, corner2Y);
            maxY = MAX(corner1Y, corner2Y);

            if (isStroke)
                cairo_stroke_extents(cairoTarget, &x1, &y1, &x2, &y2);
            else
                cairo_fill_extents(cairoTarget, &x1, &y1, &x2, &y2);

            if ((minX < x1) && (minY < y1) && (maxX > x2) && (maxY > y2)) {
                if (!draw_net_is_in_selection_buffer_remove(net, selectionInfo, (drawMode == FIND_SELECTIONS_TOGGLE))) {
                    selection_add_item(selectionInfo, &sItem);
                }
            }
            break;
        default: break;
    }
    /* clear the path, since we didn't actually draw it and cairo
         doesn't reset it after the previous calls */
    cairo_new_path(cairoTarget);
}

static void
draw_fill(
    cairo_t* cairoTarget, enum draw_mode drawMode, gerbv_selection_info_t* selectionInfo, gerbv_image_t* image,
    struct gerbv_net* net
) {
    if ((drawMode == DRAW_IMAGE) || (drawMode == DRAW_SELECTIONS))
        cairo_fill(cairoTarget);
    else
        draw_check_if_object_is_in_selected_area(cairoTarget, FALSE, selectionInfo, image, net, drawMode);
}

static void
draw_stroke(
    cairo_t* cairoTarget, enum draw_mode drawMode, gerbv_selection_info_t* selectionInfo, gerbv_image_t* image,
    struct gerbv_net* net
) {
    if ((drawMode == DRAW_IMAGE) || (drawMode == DRAW_SELECTIONS))
        cairo_stroke(cairoTarget);
    else
        draw_check_if_object_is_in_selected_area(cairoTarget, TRUE, selectionInfo, image, net, drawMode);
}

/** Draw the circle _centered_ at current Cairo coordinates.
  @param diameter	Circle diameter.
 */
static void
gerbv_draw_circle(cairo_t* cairoTarget, gdouble diameter) {
    cairo_arc(cairoTarget, 0.0, 0.0, diameter / 2.0, 0, 2.0 * M_PI);

    return;
}

/** Draw the rectangle _centered_ at current Cairo coordinates.
  @param width	Width of the rectangle.
  @param height	Height of the rectangle.
  @param pixelOutput	Round width and height to pixels.
 */
static void
gerbv_draw_rectangle(cairo_t* cairoTarget, gdouble width, gdouble height, gboolean pixelOutput) {
    if (pixelOutput) {
        cairo_user_to_device_distance(cairoTarget, &width, &height);
        width -= (int)round(width) % 2;
        height -= (int)round(height) % 2;
        cairo_device_to_user_distance(cairoTarget, &width, &height);
    }

    cairo_rectangle(cairoTarget, -width / 2.0, -height / 2.0, width, height);

    return;
}

/** Draw the oblong _centered_ at current Cairo coordinates.
  @param width	Width of the oblong.
  @param height	Height of the oblong.
 */
static void
gerbv_draw_oblong(cairo_t* cairoTarget, gdouble width, gdouble height) {
    /* --- This stuff produces a line + rounded ends --- */
    gdouble circleDiameter, strokeDistance;

    cairo_new_path(cairoTarget);
    if (width < height) {
        circleDiameter = width;
        strokeDistance = (height - width) / 2.0;
        cairo_arc(cairoTarget, 0.0, strokeDistance, circleDiameter / 2.0, 0, -M_PI);
        cairo_line_to(cairoTarget, -circleDiameter / 2.0, -strokeDistance);
        cairo_arc(cairoTarget, 0.0, -strokeDistance, circleDiameter / 2.0, -M_PI, 0);
        cairo_line_to(cairoTarget, circleDiameter / 2.0, strokeDistance);
    } else {
        circleDiameter = height;
        strokeDistance = (width - height) / 2.0;
        cairo_arc(cairoTarget, -strokeDistance, 0.0, circleDiameter / 2.0, M_PI_2, -M_PI_2);
        cairo_line_to(cairoTarget, strokeDistance, -circleDiameter / 2.0);
        cairo_arc(cairoTarget, strokeDistance, 0.0, circleDiameter / 2.0, -M_PI_2, M_PI_2);
        cairo_line_to(cairoTarget, -strokeDistance, circleDiameter / 2.0);
    }

#if 0
	/*  --- This stuff produces an oval pad --- */
	/* cairo doesn't have a function to draw ovals, so we must
	 * draw an arc and stretch it by scaling different x and y values */
	cairo_save (cairoTarget);
	cairo_scale (cairoTarget, width, height);
	gerbv_draw_circle (cairoTarget, 1);
	cairo_restore (cairoTarget);
#endif
    return;
}

static void
gerbv_draw_polygon(cairo_t* cairoTarget, gdouble outsideDiameter, gdouble numberOfSides, gdouble degreesOfRotation) {
    int i, numberOfSidesInteger = (int)numberOfSides;

    cairo_rotate(cairoTarget, DEG2RAD(degreesOfRotation));
    cairo_move_to(cairoTarget, outsideDiameter / 2.0, 0);

    /* skip first point, since we've moved there already */
    /* include last point, since we may be drawing an aperture hole next
       and cairo may not correctly close the path itself */
    for (i = 1; i <= (int)numberOfSidesInteger; i++) {
        gdouble angle = ((double)i) * M_PI * 2.0 / numberOfSidesInteger;
        cairo_line_to(cairoTarget, cos(angle) * outsideDiameter / 2.0, sin(angle) * outsideDiameter / 2.0);
    }

    return;
}

static void
gerbv_draw_aperture_hole(cairo_t* cairoTarget, gdouble dimensionX, gdouble dimensionY, gboolean pixelOutput) {
    if (dimensionX) {
        if (dimensionY)
            gerbv_draw_rectangle(cairoTarget, dimensionX, dimensionY, pixelOutput);
        else
            gerbv_draw_circle(cairoTarget, dimensionX);
    }

    return;
}

static void
draw_update_macro_exposure(
    cairo_t* cairoTarget, cairo_operator_t clearOperator, cairo_operator_t darkOperator, gdouble exposureSetting
) {

    if (exposureSetting == 0.0) {
        cairo_set_operator(cairoTarget, clearOperator);
    } else if (exposureSetting == 1.0) {
        cairo_set_operator(cairoTarget, darkOperator);
    } else if (exposureSetting == 2.0) {
        /* reverse current exposure setting */
        cairo_operator_t currentOperator = cairo_get_operator(cairoTarget);
        if (currentOperator == clearOperator) {
            cairo_set_operator(cairoTarget, darkOperator);
        } else {
            cairo_set_operator(cairoTarget, clearOperator);
        }
    }
}

static int
gerbv_draw_amacro(
    cairo_t* cairoTarget, cairo_operator_t clearOperator, cairo_operator_t darkOperator, gerbv_simplified_amacro_t* s,
    gint usesClearPrimitive, gdouble pixelWidth, enum draw_mode drawMode, gerbv_selection_info_t* selectionInfo,
    gerbv_image_t* image, struct gerbv_net* net
) {
    gerbv_simplified_amacro_t* ls = s;
    gboolean                   doVectorExportFix;
    double                     bg_r, bg_g, bg_b; /* Background color */
    int                        ret = 1;

    dprintf("Drawing simplified aperture macros:\n");

    doVectorExportFix = draw_do_vector_export_fix(cairoTarget, &bg_r, &bg_g, &bg_b);

    switch (cairo_surface_get_type(cairo_get_target(cairoTarget))) {

        case CAIRO_SURFACE_TYPE_PDF:
        case CAIRO_SURFACE_TYPE_PS:
        case CAIRO_SURFACE_TYPE_SVG:

            /* Don't limit "pixel width" in vector export */
            pixelWidth = DBL_MIN;

            break;

        default: break;
    }

    if (usesClearPrimitive)
        cairo_push_group(cairoTarget);

    while (ls != NULL) {
        /*
         * This handles the exposure thing in the aperture macro
         * The exposure is always the first element on stack independent
         * of aperture macro.
         */
        cairo_save(cairoTarget);
        cairo_new_path(cairoTarget);

        dprintf("\t%s(): drawing %s\n", __FUNCTION__, gerbv_aperture_type_name(ls->type));

        switch (ls->type) {

            case GERBV_APTYPE_MACRO_CIRCLE:
                draw_update_macro_exposure(cairoTarget, clearOperator, darkOperator, ls->parameter[CIRCLE_EXPOSURE]);
                cairo_translate(cairoTarget, ls->parameter[CIRCLE_CENTER_X], ls->parameter[CIRCLE_CENTER_Y]);
                gerbv_draw_circle(cairoTarget, ls->parameter[CIRCLE_DIAMETER]);

                if (doVectorExportFix && CAIRO_OPERATOR_CLEAR == cairo_get_operator(cairoTarget)) {
                    cairo_save(cairoTarget);
                    cairo_set_source_rgba(cairoTarget, bg_r, bg_g, bg_b, 1.0);
                    cairo_set_operator(cairoTarget, CAIRO_OPERATOR_OVER);

                    draw_fill(cairoTarget, drawMode, selectionInfo, image, net);

                    cairo_restore(cairoTarget);

                    break;
                }

                draw_fill(cairoTarget, drawMode, selectionInfo, image, net);
                break;

            case GERBV_APTYPE_MACRO_OUTLINE:
                draw_update_macro_exposure(cairoTarget, clearOperator, darkOperator, ls->parameter[OUTLINE_EXPOSURE]);
                cairo_rotate(cairoTarget, DEG2RAD(ls->parameter[OUTLINE_ROTATION_IDX(ls->parameter)]));
                cairo_move_to(cairoTarget, ls->parameter[OUTLINE_FIRST_X], ls->parameter[OUTLINE_FIRST_Y]);

                for (int point = 1; point < 1 + (int)ls->parameter[OUTLINE_NUMBER_OF_POINTS]; point++) {
                    cairo_line_to(
                        cairoTarget, ls->parameter[OUTLINE_X_IDX_OF_POINT(point)],
                        ls->parameter[OUTLINE_Y_IDX_OF_POINT(point)]
                    );
                }

                if (doVectorExportFix && CAIRO_OPERATOR_CLEAR == cairo_get_operator(cairoTarget)) {
                    cairo_save(cairoTarget);
                    cairo_set_source_rgba(cairoTarget, bg_r, bg_g, bg_b, 1.0);
                    cairo_set_operator(cairoTarget, CAIRO_OPERATOR_OVER);

                    draw_fill(cairoTarget, drawMode, selectionInfo, image, net);

                    cairo_restore(cairoTarget);

                    break;
                }

                /* Although the gerber specs allow for an open outline,
                 * I interpret it to mean the outline should be closed
                 * by the rendering softare automatically, since there
                 * is no dimension for line thickness. */
                draw_fill(cairoTarget, drawMode, selectionInfo, image, net);
                break;

            case GERBV_APTYPE_MACRO_POLYGON:
                draw_update_macro_exposure(cairoTarget, clearOperator, darkOperator, ls->parameter[POLYGON_EXPOSURE]);
                cairo_translate(cairoTarget, ls->parameter[POLYGON_CENTER_X], ls->parameter[POLYGON_CENTER_Y]);
                gerbv_draw_polygon(
                    cairoTarget, ls->parameter[POLYGON_DIAMETER], ls->parameter[POLYGON_NUMBER_OF_POINTS],
                    ls->parameter[POLYGON_ROTATION]
                );

                if (doVectorExportFix && CAIRO_OPERATOR_CLEAR == cairo_get_operator(cairoTarget)) {
                    cairo_save(cairoTarget);
                    cairo_set_source_rgba(cairoTarget, bg_r, bg_g, bg_b, 1.0);
                    cairo_set_operator(cairoTarget, CAIRO_OPERATOR_OVER);

                    draw_fill(cairoTarget, drawMode, selectionInfo, image, net);

                    cairo_restore(cairoTarget);

                    break;
                }

                draw_fill(cairoTarget, drawMode, selectionInfo, image, net);
                break;

            case GERBV_APTYPE_MACRO_MOIRE:
                {
                    gdouble diameter, diameterDifference, crosshairRadius;

                    cairo_translate(cairoTarget, ls->parameter[MOIRE_CENTER_X], ls->parameter[MOIRE_CENTER_Y]);
                    cairo_rotate(cairoTarget, DEG2RAD(ls->parameter[MOIRE_ROTATION]));
                    diameter           = ls->parameter[MOIRE_OUTSIDE_DIAMETER] - ls->parameter[MOIRE_CIRCLE_THICKNESS];
                    diameterDifference = 2 * (ls->parameter[MOIRE_GAP_WIDTH] + ls->parameter[MOIRE_CIRCLE_THICKNESS]);
                    cairo_set_line_width(cairoTarget, ls->parameter[MOIRE_CIRCLE_THICKNESS]);

                    if (doVectorExportFix && CAIRO_OPERATOR_CLEAR == cairo_get_operator(cairoTarget)) {
                        cairo_save(cairoTarget);
                        cairo_set_source_rgba(cairoTarget, bg_r, bg_g, bg_b, 1.0);
                        cairo_set_operator(cairoTarget, CAIRO_OPERATOR_OVER);
                    }

                    for (int circle = 0; circle < (int)ls->parameter[MOIRE_NUMBER_OF_CIRCLES]; circle++) {
                        gdouble dia = diameter - diameterDifference * circle;

                        if (dia <= 0) {
                            GERB_COMPILE_WARNING(
                                _("Ignoring %s "
                                  "with non positive diameter"),
                                gerbv_aperture_type_name(ls->type)
                            );
                            continue;
                        }

                        gerbv_draw_circle(cairoTarget, dia);
                        draw_stroke(cairoTarget, drawMode, selectionInfo, image, net);
                    }

                    cairo_set_line_width(cairoTarget, ls->parameter[MOIRE_CROSSHAIR_THICKNESS]);
                    crosshairRadius = ls->parameter[MOIRE_CROSSHAIR_LENGTH] / 2.0;
                    cairo_move_to(cairoTarget, -crosshairRadius, 0);
                    cairo_line_to(cairoTarget, crosshairRadius, 0);
                    cairo_move_to(cairoTarget, 0, -crosshairRadius);
                    cairo_line_to(cairoTarget, 0, crosshairRadius);

                    draw_stroke(cairoTarget, drawMode, selectionInfo, image, net);

                    if (doVectorExportFix && CAIRO_OPERATOR_CLEAR == cairo_get_operator(cairoTarget)) {
                        cairo_restore(cairoTarget);
                    }

                    break;
                }
            case GERBV_APTYPE_MACRO_THERMAL:
                {
                    gdouble startAngle1, startAngle2, endAngle1, endAngle2;

                    cairo_translate(cairoTarget, ls->parameter[THERMAL_CENTER_X], ls->parameter[THERMAL_CENTER_Y]);
                    cairo_rotate(cairoTarget, DEG2RAD(ls->parameter[THERMAL_ROTATION]));
                    startAngle1 =
                        asin(ls->parameter[THERMAL_CROSSHAIR_THICKNESS] / ls->parameter[THERMAL_INSIDE_DIAMETER]);
                    endAngle1 = M_PI_2 - startAngle1;
                    endAngle2 =
                        asin(ls->parameter[THERMAL_CROSSHAIR_THICKNESS] / ls->parameter[THERMAL_OUTSIDE_DIAMETER]);
                    startAngle2 = M_PI_2 - endAngle2;

                    if (doVectorExportFix && CAIRO_OPERATOR_CLEAR == cairo_get_operator(cairoTarget)) {
                        cairo_save(cairoTarget);
                        cairo_set_source_rgba(cairoTarget, bg_r, bg_g, bg_b, 1.0);
                        cairo_set_operator(cairoTarget, CAIRO_OPERATOR_OVER);

                        /* */

                        cairo_restore(cairoTarget);

                        break;
                    }

                    for (gint i = 0; i < 4; i++) {
                        cairo_arc(
                            cairoTarget, 0, 0, ls->parameter[THERMAL_INSIDE_DIAMETER] / 2.0, startAngle1, endAngle1
                        );
                        cairo_arc_negative(
                            cairoTarget, 0, 0, ls->parameter[THERMAL_OUTSIDE_DIAMETER] / 2.0, startAngle2, endAngle2
                        );
                        draw_fill(cairoTarget, drawMode, selectionInfo, image, net);
                        cairo_rotate(cairoTarget, M_PI_2);
                    }

                    break;
                }
            case GERBV_APTYPE_MACRO_LINE20:
                draw_update_macro_exposure(cairoTarget, clearOperator, darkOperator, ls->parameter[LINE20_EXPOSURE]);
                cairo_set_line_width(cairoTarget, MAX(ls->parameter[LINE20_LINE_WIDTH], pixelWidth));
                cairo_set_line_cap(cairoTarget, CAIRO_LINE_CAP_BUTT);
                cairo_rotate(cairoTarget, DEG2RAD(ls->parameter[LINE20_ROTATION]));
                cairo_move_to(cairoTarget, ls->parameter[LINE20_START_X], ls->parameter[LINE20_START_Y]);
                cairo_line_to(cairoTarget, ls->parameter[LINE20_END_X], ls->parameter[LINE20_END_Y]);

                if (doVectorExportFix && CAIRO_OPERATOR_CLEAR == cairo_get_operator(cairoTarget)) {
                    cairo_save(cairoTarget);
                    cairo_set_source_rgba(cairoTarget, bg_r, bg_g, bg_b, 1.0);
                    cairo_set_operator(cairoTarget, CAIRO_OPERATOR_OVER);

                    draw_stroke(cairoTarget, drawMode, selectionInfo, image, net);

                    cairo_restore(cairoTarget);

                    break;
                }

                draw_stroke(cairoTarget, drawMode, selectionInfo, image, net);
                break;

            case GERBV_APTYPE_MACRO_LINE21:
                draw_update_macro_exposure(cairoTarget, clearOperator, darkOperator, ls->parameter[LINE21_EXPOSURE]);
                cairo_rotate(cairoTarget, DEG2RAD(ls->parameter[LINE21_ROTATION]));
                cairo_translate(cairoTarget, ls->parameter[LINE21_CENTER_X], ls->parameter[LINE21_CENTER_Y]);
                cairo_rectangle(
                    cairoTarget, -MAX(ls->parameter[LINE21_WIDTH] / 2.0, pixelWidth),
                    -MAX(ls->parameter[LINE21_HEIGHT] / 2.0, pixelWidth), MAX(ls->parameter[LINE21_WIDTH], pixelWidth),
                    MAX(ls->parameter[LINE21_HEIGHT], pixelWidth)
                );
                if (doVectorExportFix && CAIRO_OPERATOR_CLEAR == cairo_get_operator(cairoTarget)) {
                    cairo_save(cairoTarget);
                    cairo_set_source_rgba(cairoTarget, bg_r, bg_g, bg_b, 1.0);
                    cairo_set_operator(cairoTarget, CAIRO_OPERATOR_OVER);

                    draw_fill(cairoTarget, drawMode, selectionInfo, image, net);

                    cairo_restore(cairoTarget);

                    break;
                }

                draw_fill(cairoTarget, drawMode, selectionInfo, image, net);
                break;

            case GERBV_APTYPE_MACRO_LINE22:
                draw_update_macro_exposure(cairoTarget, clearOperator, darkOperator, ls->parameter[LINE22_EXPOSURE]);
                cairo_rotate(cairoTarget, DEG2RAD(ls->parameter[LINE22_ROTATION]));
                cairo_translate(cairoTarget, ls->parameter[LINE22_LOWER_LEFT_X], ls->parameter[LINE22_LOWER_LEFT_Y]);
                cairo_rectangle(
                    cairoTarget, 0, 0, MAX(ls->parameter[LINE22_WIDTH], pixelWidth),
                    MAX(ls->parameter[LINE22_HEIGHT], pixelWidth)
                );

                if (doVectorExportFix && CAIRO_OPERATOR_CLEAR == cairo_get_operator(cairoTarget)) {
                    cairo_save(cairoTarget);
                    cairo_set_source_rgba(cairoTarget, bg_r, bg_g, bg_b, 1.0);
                    cairo_set_operator(cairoTarget, CAIRO_OPERATOR_OVER);

                    draw_fill(cairoTarget, drawMode, selectionInfo, image, net);

                    cairo_restore(cairoTarget);

                    break;
                }

                draw_fill(cairoTarget, drawMode, selectionInfo, image, net);
                break;

            default: GERB_COMPILE_WARNING(_("Unknown macro type: %s"), gerbv_aperture_type_name(ls->type)); ret = 0;
        }

        cairo_restore(cairoTarget);
        ls = ls->next;
    }

    if (usesClearPrimitive) {
        cairo_pop_group_to_source(cairoTarget);
        cairo_paint(cairoTarget);
    }

    return ret;
}

void
draw_apply_netstate_transformation(cairo_t* cairoTarget, gerbv_netstate_t* state) {
    /* apply scale factor */
    cairo_scale(cairoTarget, state->scaleA, state->scaleB);
    /* apply offset */
    cairo_translate(cairoTarget, state->offsetA, state->offsetB);
    /* apply mirror */
    switch (state->mirrorState) {
        case GERBV_MIRROR_STATE_FLIPA: cairo_scale(cairoTarget, -1, 1); break;
        case GERBV_MIRROR_STATE_FLIPB: cairo_scale(cairoTarget, 1, -1); break;
        case GERBV_MIRROR_STATE_FLIPAB: cairo_scale(cairoTarget, -1, -1); break;
        default: break;
    }
    /* finally, apply axis select */
    if (state->axisSelect == GERBV_AXIS_SELECT_SWAPAB) {
        /* we do this by rotating 270 (counterclockwise, then mirroring
           the Y axis */
        cairo_rotate(cairoTarget, M_PI + M_PI_2);
        cairo_scale(cairoTarget, 1, -1);
    }
}

void
draw_render_polygon_object(
    gerbv_net_t* oldNet, cairo_t* cairoTarget, gdouble sr_x, gdouble sr_y, gerbv_image_t* image,
    enum draw_mode drawMode, gerbv_selection_info_t* selectionInfo, gboolean pixelOutput
) {
    gerbv_net_t *currentNet, *polygonStartNet;
    int          haveDrawnFirstFillPoint = 0;
    gdouble      x2, y2, cp_x = 0, cp_y = 0;

    haveDrawnFirstFillPoint = FALSE;
    /* save the first net in the polygon as the "ID" net pointer
       in case we are saving this net to the selection array */
    polygonStartNet = oldNet;
    cairo_new_path(cairoTarget);

    for (currentNet = oldNet->next; currentNet != NULL; currentNet = currentNet->next) {
        x2 = currentNet->stop_x + sr_x;
        y2 = currentNet->stop_y + sr_y;

        /* translate circular x,y data as well */
        if (currentNet->cirseg) {
            cp_x = currentNet->cirseg->cp_x + sr_x;
            cp_y = currentNet->cirseg->cp_y + sr_y;
        }
        if (!haveDrawnFirstFillPoint) {
            draw_cairo_move_to(cairoTarget, x2, y2, FALSE, pixelOutput);
            haveDrawnFirstFillPoint = TRUE;
            continue;
        }

        switch (currentNet->interpolation) {
            case GERBV_INTERPOLATION_LINEARx1:
            case GERBV_INTERPOLATION_LINEARx10:
            case GERBV_INTERPOLATION_LINEARx01:
            case GERBV_INTERPOLATION_LINEARx001: draw_cairo_line_to(cairoTarget, x2, y2, FALSE, pixelOutput); break;
            case GERBV_INTERPOLATION_CW_CIRCULAR:
            case GERBV_INTERPOLATION_CCW_CIRCULAR:
                if (currentNet->cirseg->angle2 > currentNet->cirseg->angle1) {
                    cairo_arc(
                        cairoTarget, cp_x, cp_y, currentNet->cirseg->width / 2.0, DEG2RAD(currentNet->cirseg->angle1),
                        DEG2RAD(currentNet->cirseg->angle2)
                    );
                } else {
                    cairo_arc_negative(
                        cairoTarget, cp_x, cp_y, currentNet->cirseg->width / 2.0, DEG2RAD(currentNet->cirseg->angle1),
                        DEG2RAD(currentNet->cirseg->angle2)
                    );
                }
                break;
            case GERBV_INTERPOLATION_PAREA_END:
                cairo_close_path(cairoTarget);
                /* turn off anti-aliasing for polygons, since it shows seams
                   with adjacent polygons (usually on PCB ground planes) */
                cairo_antialias_t oldAlias = cairo_get_antialias(cairoTarget);
                cairo_set_antialias(cairoTarget, CAIRO_ANTIALIAS_NONE);
                draw_fill(cairoTarget, drawMode, selectionInfo, image, polygonStartNet);
                cairo_set_antialias(cairoTarget, oldAlias);
                return;
            default: break;
        }
    }
}

/** Draw Cairo cross.
  @param xc	Cross center x coordinate.
  @param yc	Cross center y coordinate.
  @param r	Cross half size.
*/
static void
draw_cairo_cross(cairo_t* cairoTarget, gdouble xc, gdouble yc, gdouble r) {
    cairo_move_to(cairoTarget, xc, yc - r);
    cairo_rel_line_to(cairoTarget, 0, 2 * r);
    cairo_move_to(cairoTarget, xc - r, yc);
    cairo_rel_line_to(cairoTarget, 2 * r, 0);
    cairo_stroke(cairoTarget);
}

static int
draw_calc_pnp_mark_coords(struct gerbv_net* start_net, double* label_x, double* label_y) {
    double            x, y;
    struct gerbv_net* net   = start_net;
    const char*       label = NULL;

    if (net && net->label)
        label = net->label->str;

    if (label == NULL)
        return 0;

    x = HUGE_VAL;
    y = -HUGE_VAL;
    do {
        if (!net->label || 0 != g_strcmp0(net->label->str, label))
            break;

        /* Search top left corner */
        if (net->boundingBox.top != HUGE_VAL) {
            /* Bounding box not calculated */
            x = MIN(x, net->boundingBox.left);
            y = MAX(y, net->boundingBox.top);
        } else {
            x = MIN(x, net->stop_x);
            y = MAX(y, net->stop_y + 0.01 / 2);
            /* 0.01 default line width */
        }
    } while (NULL != (net = gerbv_image_return_next_renderable_object(net)));

    *label_x = x;
    *label_y = y;

    return 1;
}

int
draw_image_to_cairo_target(
    cairo_t* cairoTarget, gerbv_image_t* image, gdouble pixelWidth, enum draw_mode drawMode,
    gerbv_selection_info_t* selectionInfo, gerbv_render_info_t* renderInfo, gboolean allowOptimization,
    gerbv_user_transformation_t transform, gboolean pixelOutput
) {
    const int         hole_cross_inc_px = 8;
    struct gerbv_net *net, *polygonStartNet = NULL;
    double            x1, y1, x2, y2, cp_x = 0, cp_y = 0;
    gdouble *         p, p0, p1, dx, dy, lineWidth, r;
    gerbv_netstate_t* oldState;
    gerbv_layer_t*    oldLayer;
    cairo_operator_t  drawOperatorClear, drawOperatorDark;
    gboolean          invertPolarity = FALSE, oddWidth = FALSE;
    gdouble           minX = 0, minY = 0, maxX = 0, maxY = 0;
    gdouble           criticalRadius;
    gdouble           scaleX = transform.scaleX;
    gdouble           scaleY = transform.scaleY;
    /* Keep PNP label not mirrored */
    gdouble  pnp_label_scale_x = 1, pnp_label_scale_y = -1;
    gboolean limitLineWidth = TRUE;
    gboolean displayPixel   = TRUE;
    gboolean doVectorExportFix;
    double   bg_r, bg_g, bg_b; /* Background color */

#if CAIRO_VERSION >= CAIRO_VERSION_ENCODE(1, 16, 0)
    // Fix for cairo 1.17.6 and above which sets to surface unit back to PT (default: UNIT_USER)

    cairo_surface_t* cSurface = cairo_get_target(cairoTarget);
    if (cairo_surface_get_type(cSurface) == CAIRO_SURFACE_TYPE_SVG) {
        cairo_svg_surface_set_document_unit(cSurface, CAIRO_SVG_UNIT_PT);
    }
#endif

    doVectorExportFix = draw_do_vector_export_fix(cairoTarget, &bg_r, &bg_g, &bg_b);

    /* If we are scaling the image at all, ignore the line width checks
     * since scaled up lines can still be visible */
    if ((scaleX != 1) || (scaleY != 1)) {
        limitLineWidth = FALSE;
    }

    if (transform.mirrorAroundX) {
        scaleY *= -1;
        pnp_label_scale_y = 1;
    }

    if (transform.mirrorAroundY) {
        scaleX *= -1;
        pnp_label_scale_x = -1;
    }

    cairo_translate(cairoTarget, transform.translateX, transform.translateY);
    cairo_scale(cairoTarget, scaleX, scaleY);
    cairo_rotate(cairoTarget, transform.rotation);

    gboolean useOptimizations = allowOptimization;

    /* If the user is using any transformations for this layer, then don't
     * bother using rendering optimizations */
    if (fabs(transform.translateX) > GERBV_PRECISION_LINEAR_INCH
        || fabs(transform.translateY) > GERBV_PRECISION_LINEAR_INCH
        || fabs(transform.scaleX - 1) > GERBV_PRECISION_LINEAR_INCH
        || fabs(transform.scaleY - 1) > GERBV_PRECISION_LINEAR_INCH
        || fabs(transform.rotation) > GERBV_PRECISION_ANGLE_RAD || transform.mirrorAroundX || transform.mirrorAroundY)
        useOptimizations = FALSE;

    if (useOptimizations && pixelOutput) {
        minX = renderInfo->lowerLeftX;
        minY = renderInfo->lowerLeftY;
        maxX = renderInfo->lowerLeftX + (renderInfo->displayWidth / renderInfo->scaleFactorX);
        maxY = renderInfo->lowerLeftY + (renderInfo->displayHeight / renderInfo->scaleFactorY);
    }

    /* do initial justify */
    cairo_translate(cairoTarget, image->info->imageJustifyOffsetActualA, image->info->imageJustifyOffsetActualB);

    /* set the fill rule so aperture holes are cleared correctly */
    cairo_set_fill_rule(cairoTarget, CAIRO_FILL_RULE_EVEN_ODD);
    /* offset image */
    cairo_translate(cairoTarget, image->info->offsetA, image->info->offsetB);
    /* do image rotation */
    cairo_rotate(cairoTarget, image->info->imageRotation);

    /* load in polarity operators depending on the image polarity */
    invertPolarity = transform.inverted;
    if (image->info->polarity == GERBV_POLARITY_NEGATIVE)
        invertPolarity = !invertPolarity;
    if (drawMode == DRAW_SELECTIONS)
        invertPolarity = FALSE;

    if (invertPolarity) {
        drawOperatorClear = CAIRO_OPERATOR_OVER;
        drawOperatorDark  = CAIRO_OPERATOR_CLEAR;
        cairo_set_operator(cairoTarget, CAIRO_OPERATOR_OVER);
        cairo_paint(cairoTarget);
        cairo_set_operator(cairoTarget, CAIRO_OPERATOR_CLEAR);
    } else {
        drawOperatorClear = CAIRO_OPERATOR_CLEAR;
        drawOperatorDark  = CAIRO_OPERATOR_OVER;
    }

    /* next, push two cairo states to simulate the first layer and netstate
       translations (these will be popped when another layer or netstate is
       started */
    cairo_save(cairoTarget);
    cairo_save(cairoTarget);

    /* store the current layer and netstate so we know when they change */
    oldLayer = image->layers;
    oldState = image->states;

    const char* pnp_net_label_str_prev = NULL;

    for (net = image->netlist->next; net != NULL; net = gerbv_image_return_next_renderable_object(net)) {

        /* check if this is a new layer */
        if (net->layer != oldLayer) {
            /* it's a new layer, so recalculate the new transformation matrix
               for it */
            cairo_restore(cairoTarget);
            cairo_restore(cairoTarget);
            cairo_save(cairoTarget);
            /* do any rotations */
            cairo_rotate(cairoTarget, net->layer->rotation);
            /* handle the layer polarity */
            if ((net->layer->polarity == GERBV_POLARITY_CLEAR) ^ invertPolarity) {
                cairo_set_operator(cairoTarget, CAIRO_OPERATOR_CLEAR);
                drawOperatorClear = CAIRO_OPERATOR_OVER;
                drawOperatorDark  = CAIRO_OPERATOR_CLEAR;
            } else {
                cairo_set_operator(cairoTarget, CAIRO_OPERATOR_OVER);
                drawOperatorClear = CAIRO_OPERATOR_CLEAR;
                drawOperatorDark  = CAIRO_OPERATOR_OVER;
            }

            /* Draw any knockout areas */
            gerbv_knockout_t* ko = &net->layer->knockout;
            if (ko->firstInstance == TRUE) {
                cairo_save(cairoTarget);

                if (ko->polarity == GERBV_POLARITY_CLEAR) {
                    cairo_set_operator(cairoTarget, drawOperatorClear);
                } else {
                    cairo_set_operator(cairoTarget, drawOperatorDark);
                }

                if (doVectorExportFix && CAIRO_OPERATOR_CLEAR == cairo_get_operator(cairoTarget)) {

                    cairo_set_operator(cairoTarget, CAIRO_OPERATOR_OVER);
                    cairo_set_source_rgba(cairoTarget, bg_r, bg_g, bg_b, 1.0);
                }

                cairo_new_path(cairoTarget);
                cairo_rectangle(
                    cairoTarget, ko->lowerLeftX - ko->border, ko->lowerLeftY - ko->border, ko->width + 2 * ko->border,
                    ko->height + 2 * ko->border
                );
                draw_fill(cairoTarget, drawMode, selectionInfo, image, net);

                cairo_restore(cairoTarget);
            }

            /* Finally, reapply old netstate transformation */
            cairo_save(cairoTarget);
            draw_apply_netstate_transformation(cairoTarget, net->state);
            oldLayer = net->layer;
        }

        /* check if this is a new netstate */
        if (net->state != oldState) {
            /* pop the transformation matrix back to the "pre-state" state and
               resave it */
            cairo_restore(cairoTarget);
            cairo_save(cairoTarget);
            /* it's a new state, so recalculate the new transformation matrix
               for it */
            draw_apply_netstate_transformation(cairoTarget, net->state);
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
                if (!draw_net_is_in_selection_buffer_remove(net, selectionInfo, FALSE))
                    continue;
            }
        }

        /* Render any labels attached to this net */
        /* NOTE: this is currently only used on PNP files, so we may
           make some assumptions here... */
        if (drawMode != DRAW_SELECTIONS && net->label
            && (image->layertype == GERBV_LAYERTYPE_PICKANDPLACE_TOP
                || image->layertype == GERBV_LAYERTYPE_PICKANDPLACE_BOT)
            && g_strcmp0(net->label->str, pnp_net_label_str_prev)) {

            double mark_x, mark_y;

            /* Add PNP text label only one time per
             * net and if it is not selected. */
            pnp_net_label_str_prev = net->label->str;

            if (draw_calc_pnp_mark_coords(net, &mark_x, &mark_y)) {
                cairo_save(cairoTarget);

                cairo_set_font_size(cairoTarget, 0.05);
                cairo_move_to(cairoTarget, mark_x, mark_y);
                cairo_scale(cairoTarget, pnp_label_scale_x, pnp_label_scale_y);
                cairo_show_text(cairoTarget, net->label->str);

                cairo_restore(cairoTarget);
            }
        }

        /* step and repeat */
        gerbv_step_and_repeat_t* sr = &net->layer->stepAndRepeat;
        int                      ix, iy;
        for (ix = 0; ix < sr->X; ix++) {
            for (iy = 0; iy < sr->Y; iy++) {
                double sr_x = ix * sr->dist_X;
                double sr_y = iy * sr->dist_Y;

                if (useOptimizations && pixelOutput
                    && ((net->boundingBox.right + sr_x < minX) || (net->boundingBox.left + sr_x > maxX)
                        || (net->boundingBox.top + sr_y < minY) || (net->boundingBox.bottom + sr_y > maxY))) {
                    continue;
                }

                x1 = net->start_x + sr_x;
                y1 = net->start_y + sr_y;
                x2 = net->stop_x + sr_x;
                y2 = net->stop_y + sr_y;

                /* translate circular x,y data as well */
                if (net->cirseg) {
                    cp_x = net->cirseg->cp_x + sr_x;
                    cp_y = net->cirseg->cp_y + sr_y;
                }

                /* Polygon area fill routines */
                switch (net->interpolation) {
                    case GERBV_INTERPOLATION_PAREA_START:

                        if (doVectorExportFix && CAIRO_OPERATOR_CLEAR == cairo_get_operator(cairoTarget)) {

                            cairo_save(cairoTarget);

                            cairo_set_operator(cairoTarget, CAIRO_OPERATOR_OVER);
                            cairo_set_source_rgba(cairoTarget, bg_r, bg_g, bg_b, 1.0);

                            draw_render_polygon_object(
                                net, cairoTarget, sr_x, sr_y, image, drawMode, selectionInfo, pixelOutput
                            );

                            cairo_restore(cairoTarget);
                        } else {
                            draw_render_polygon_object(
                                net, cairoTarget, sr_x, sr_y, image, drawMode, selectionInfo, pixelOutput
                            );
                        }

                        continue;
                    case GERBV_INTERPOLATION_DELETED: continue;
                    default: break;
                }

                /*
                 * If aperture state is off we allow use of undefined apertures.
                 * This happens when gerber files starts, but hasn't decided on
                 * which aperture to use.
                 */
                if (image->aperture[net->aperture] == NULL)
                    continue;

                switch (net->aperture_state) {
                    case GERBV_APERTURE_STATE_ON:
                        /* if the aperture width is truly 0, then render as a 1 pixel width
                           line.  0 diameter apertures are used by some programs to draw labels,
                           etc, and they are rendered by other programs as 1 pixel wide */
                        /* NOTE: also, make sure all lines are at least 1 pixel wide, so they
                           always show up at low zoom levels */

                        if (limitLineWidth
                            && ((image->aperture[net->aperture]->parameter[0] < pixelWidth) && (pixelOutput)))
                            criticalRadius = pixelWidth / 2.0;
                        else
                            criticalRadius = image->aperture[net->aperture]->parameter[0] / 2.0;
                        lineWidth = criticalRadius * 2.0;
                        // convert to a pixel integer
                        cairo_user_to_device_distance(cairoTarget, &lineWidth, &x1);
                        if (pixelOutput) {
                            lineWidth = round(lineWidth);
                            if ((int)lineWidth % 2) {
                                oddWidth = TRUE;
                            } else {
                                oddWidth = FALSE;
                            }
                        }
                        cairo_device_to_user_distance(cairoTarget, &lineWidth, &x1);
                        cairo_set_line_width(cairoTarget, lineWidth);

                        switch (net->interpolation) {
                            case GERBV_INTERPOLATION_LINEARx1:
                            case GERBV_INTERPOLATION_LINEARx10:
                            case GERBV_INTERPOLATION_LINEARx01:
                            case GERBV_INTERPOLATION_LINEARx001:
                                cairo_set_line_cap(cairoTarget, CAIRO_LINE_CAP_ROUND);

                                /* weed out any lines that are
                                 * obviously not going to
                                 * render on the visible screen */
                                switch (image->aperture[net->aperture]->type) {
                                    case GERBV_APTYPE_CIRCLE:
                                        if (renderInfo->show_cross_on_drill_holes
                                            && image->layertype == GERBV_LAYERTYPE_DRILL) {
                                            /* Draw center crosses on slot hole */
                                            cairo_set_line_width(cairoTarget, pixelWidth);
                                            cairo_set_line_cap(cairoTarget, CAIRO_LINE_CAP_SQUARE);
                                            r = image->aperture[net->aperture]->parameter[0] / 2.0
                                              + hole_cross_inc_px * pixelWidth;
                                            draw_cairo_cross(cairoTarget, x1, y1, r);
                                            draw_cairo_cross(cairoTarget, x2, y2, r);
                                            cairo_set_line_cap(cairoTarget, CAIRO_LINE_CAP_ROUND);
                                            cairo_set_line_width(cairoTarget, lineWidth);
                                        }

                                        draw_cairo_move_to(cairoTarget, x1, y1, oddWidth, pixelOutput);
                                        draw_cairo_line_to(cairoTarget, x2, y2, oddWidth, pixelOutput);

                                        if (doVectorExportFix
                                            && CAIRO_OPERATOR_CLEAR == cairo_get_operator(cairoTarget)) {
                                            cairo_save(cairoTarget);
                                            cairo_set_source_rgba(cairoTarget, bg_r, bg_g, bg_b, 1.0);
                                            cairo_set_operator(cairoTarget, CAIRO_OPERATOR_OVER);

                                            draw_stroke(cairoTarget, drawMode, selectionInfo, image, net);

                                            cairo_restore(cairoTarget);
                                        } else {
                                            draw_stroke(cairoTarget, drawMode, selectionInfo, image, net);
                                        }

                                        break;
                                    case GERBV_APTYPE_RECTANGLE:
                                        dx = image->aperture[net->aperture]->parameter[0] / 2;
                                        dy = image->aperture[net->aperture]->parameter[1] / 2;
                                        if (x1 > x2)
                                            dx = -dx;
                                        if (y1 > y2)
                                            dy = -dy;
                                        cairo_new_path(cairoTarget);
                                        draw_cairo_move_to(cairoTarget, x1 - dx, y1 - dy, FALSE, pixelOutput);
                                        draw_cairo_line_to(cairoTarget, x1 - dx, y1 + dy, FALSE, pixelOutput);
                                        draw_cairo_line_to(cairoTarget, x2 - dx, y2 + dy, FALSE, pixelOutput);
                                        draw_cairo_line_to(cairoTarget, x2 + dx, y2 + dy, FALSE, pixelOutput);
                                        draw_cairo_line_to(cairoTarget, x2 + dx, y2 - dy, FALSE, pixelOutput);
                                        draw_cairo_line_to(cairoTarget, x1 + dx, y1 - dy, FALSE, pixelOutput);
                                        draw_fill(cairoTarget, drawMode, selectionInfo, image, net);
                                        break;
                                    /* TODO: for now, just render ovals or polygons like a circle */
                                    case GERBV_APTYPE_OVAL:
                                    case GERBV_APTYPE_POLYGON:
                                        draw_cairo_move_to(cairoTarget, x1, y1, oddWidth, pixelOutput);
                                        draw_cairo_line_to(cairoTarget, x2, y2, oddWidth, pixelOutput);
                                        draw_stroke(cairoTarget, drawMode, selectionInfo, image, net);
                                        break;
                                    /* macros can only be flashed, so ignore any that might be here */
                                    default:
                                        GERB_COMPILE_WARNING(
                                            _("Unknown aperture type: %s"),
                                            _(gerbv_aperture_type_name(image->aperture[net->aperture]->type))
                                        );
                                        break;
                                }
                                break;
                            case GERBV_INTERPOLATION_CW_CIRCULAR:
                            case GERBV_INTERPOLATION_CCW_CIRCULAR:
                                /* cairo doesn't have a function to draw oval arcs, so we must
                                 * draw an arc and stretch it by scaling different x and y values
                                 */
                                cairo_new_path(cairoTarget);
                                if (image->aperture[net->aperture]->type == GERBV_APTYPE_RECTANGLE) {
                                    cairo_set_line_cap(cairoTarget, CAIRO_LINE_CAP_SQUARE);
                                } else {
                                    cairo_set_line_cap(cairoTarget, CAIRO_LINE_CAP_ROUND);
                                }
                                cairo_save(cairoTarget);
                                cairo_translate(cairoTarget, cp_x, cp_y);
                                cairo_scale(cairoTarget, net->cirseg->width, net->cirseg->height);
                                if (net->cirseg->angle2 > net->cirseg->angle1) {
                                    cairo_arc(
                                        cairoTarget, 0.0, 0.0, 0.5, DEG2RAD(net->cirseg->angle1),
                                        DEG2RAD(net->cirseg->angle2)
                                    );
                                } else {
                                    cairo_arc_negative(
                                        cairoTarget, 0.0, 0.0, 0.5, DEG2RAD(net->cirseg->angle1),
                                        DEG2RAD(net->cirseg->angle2)
                                    );
                                }
                                cairo_restore(cairoTarget);
                                draw_stroke(cairoTarget, drawMode, selectionInfo, image, net);
                                break;
                            default:
                                GERB_COMPILE_WARNING(
                                    _("Unknown interpolation type: %s"), _(gerbv_interpolation_name(net->interpolation))
                                );
                                break;
                        }
                        break;
                    case GERBV_APERTURE_STATE_OFF: break;
                    case GERBV_APERTURE_STATE_FLASH:
                        p = image->aperture[net->aperture]->parameter;

                        cairo_save(cairoTarget);
                        draw_cairo_translate_adjust(cairoTarget, x2, y2, pixelOutput);

                        switch (image->aperture[net->aperture]->type) {
                            case GERBV_APTYPE_CIRCLE:
                                if (renderInfo->show_cross_on_drill_holes
                                    && image->layertype == GERBV_LAYERTYPE_DRILL) {
                                    /* Draw center cross on drill hole */
                                    cairo_set_line_width(cairoTarget, pixelWidth);
                                    cairo_set_line_cap(cairoTarget, CAIRO_LINE_CAP_SQUARE);
                                    r = p[0] / 2.0 + hole_cross_inc_px * pixelWidth;
                                    draw_cairo_cross(cairoTarget, 0, 0, r);
                                    cairo_set_line_width(cairoTarget, lineWidth);
                                    cairo_set_line_cap(cairoTarget, CAIRO_LINE_CAP_ROUND);
                                }

                                gerbv_draw_circle(cairoTarget, p[0]);
                                gerbv_draw_aperture_hole(cairoTarget, p[1], p[2], pixelOutput);
                                break;
                            case GERBV_APTYPE_RECTANGLE:
                                // some CAD programs use very thin flashed rectangles to compose
                                //	logos/images, so we must make sure those display here
                                displayPixel = pixelOutput;
                                p0           = p[0];
                                p1           = p[1];
                                if (limitLineWidth && (p[0] < pixelWidth) && pixelOutput) {
                                    p0           = pixelWidth;
                                    displayPixel = FALSE;
                                }
                                if (limitLineWidth && (p[1] < pixelWidth) && pixelOutput) {
                                    p1           = pixelWidth;
                                    displayPixel = FALSE;
                                }
                                gerbv_draw_rectangle(cairoTarget, p0, p1, displayPixel);
                                gerbv_draw_aperture_hole(cairoTarget, p[2], p[3], displayPixel);
                                break;
                            case GERBV_APTYPE_OVAL:
                                gerbv_draw_oblong(cairoTarget, p[0], p[1]);
                                gerbv_draw_aperture_hole(cairoTarget, p[2], p[3], pixelOutput);
                                break;
                            case GERBV_APTYPE_POLYGON:
                                gerbv_draw_polygon(cairoTarget, p[0], p[1], p[2]);
                                gerbv_draw_aperture_hole(cairoTarget, p[3], p[4], pixelOutput);
                                break;
                            case GERBV_APTYPE_MACRO:
                                /* TODO: to do it properly for vector export (doVectorExportFix) draw all
                                 * macros with some vector library with logical operators */
                                gerbv_draw_amacro(
                                    cairoTarget, drawOperatorClear, drawOperatorDark,
                                    image->aperture[net->aperture]->simplified, (gint)p[0], pixelWidth, drawMode,
                                    selectionInfo, image, net
                                );
                                break;
                            default:
                                GERB_COMPILE_WARNING(
                                    _("Unknown aperture type: %s"),
                                    _(gerbv_aperture_type_name(image->aperture[net->aperture]->type))
                                );
                                return 0;
                        }

                        /* And finally fill the path */
                        if (doVectorExportFix && CAIRO_OPERATOR_CLEAR == cairo_get_operator(cairoTarget)) {
                            cairo_set_source_rgba(cairoTarget, bg_r, bg_g, bg_b, 1.0);
                            cairo_set_operator(cairoTarget, CAIRO_OPERATOR_OVER);
                        }

                        draw_fill(cairoTarget, drawMode, selectionInfo, image, net);
                        cairo_restore(cairoTarget);
                        break;
                    default:
                        GERB_COMPILE_WARNING(
                            _("Unknown aperture state: %s"), _(gerbv_aperture_type_name(net->aperture_state))
                        );

                        return 0;
                }
            }
        }
    }

    /* restore the initial two state saves (one for layer, one for netstate)*/
    cairo_restore(cairoTarget);
    cairo_restore(cairoTarget);

    return 1;
}

/* Check if Cairo target require the vector export fix to be done and if so try
 * to retrieve background color. */
static gboolean
draw_do_vector_export_fix(cairo_t* cairoTarget, double* bg_red, double* bg_green, double* bg_blue) {
    /* Cairo library produce _raster_ output if polygon is cleared by
     * negative aperture or other polygon. A workaround is to draw over
     * with background color instead of clearing. Drawback is: there won't
     * be any see thru negative opening if two layers printed one on the
     * another. */

    switch (cairo_surface_get_type(cairo_get_target(cairoTarget))) {

        case CAIRO_SURFACE_TYPE_PDF:
        case CAIRO_SURFACE_TYPE_PS:
        case CAIRO_SURFACE_TYPE_SVG:
            {
                double *p0, *p1, *p2;

                /* Get background color from cairo user data to emulate clear
                 * operator */
                p0 = cairo_get_user_data(cairoTarget, (cairo_user_data_key_t*)0);
                p1 = cairo_get_user_data(cairoTarget, (cairo_user_data_key_t*)1);
                p2 = cairo_get_user_data(cairoTarget, (cairo_user_data_key_t*)2);

                if (p0 != NULL && p1 != NULL && p2 != NULL) {
                    *bg_red   = *p0;
                    *bg_green = *p1;
                    *bg_blue  = *p2;
                } else {
                    *bg_red = *bg_green = *bg_blue = 1.0;
                }

                break;
            }

        default: return FALSE;
    }

    return TRUE;
}

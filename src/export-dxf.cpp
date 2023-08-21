/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of Gerbv.
 *
 *   Copyright (C) 2014 Sergey Alyoshin
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

/** \file export-dxf.cpp
    \brief Functions for exporting Gerbv images to DXF files
    \ingroup libgerbv
*/

#include "gerbv.h"

#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <dxflib/dl_dxf.h>

#include "common.h"

/* dxflib version difference */
#ifndef DL_STRGRP_END
#define DL_STRGRP_END STRGRP_END
#endif
#ifndef DL_ATTFLAGS_CODE
#define DL_ATTFLAGS_CODE ATTFLAGS_CODE
#endif
#ifndef DL_TXTHI_CODE
#define DL_TXTHI_CODE TXTHI_CODE
#endif
#ifndef DL_TXT_STYLE_CODE
#define DL_TXT_STYLE_CODE TXT_STYLE_CODE
#endif
#ifndef DL_FIRST_XCOORD_CODE
#define DL_FIRST_XCOORD_CODE FIRST_XCOORD_CODE
#endif
#ifndef DL_FIRST_YCOORD_CODE
#define DL_FIRST_YCOORD_CODE FIRST_YCOORD_CODE
#endif
#ifndef DL_CLOSED_PLINE
#define DL_CLOSED_PLINE CLOSED_PLINE
#endif

enum insunits {
    INSUNITS_NONE = 0,
    INSUNITS_INCH,
    INSUNITS_FEET,
    INSUNITS_MILE,
    INSUNITS_MM,
    INSUNITS_CM,
    INSUNITS_M,
    INSUNITS_KM,
    INSUNITS_MICROINCH,
    INSUNITS_MIL,
    /* ... and more ... */
};

extern "C" {
gboolean
gerbv_export_dxf_file_from_image(const gchar* file_name, gerbv_image_t* input_img, gerbv_user_transformation_t* trans) {
    DL_Codes::version exportVersion = DL_Codes::AC1015;
    DL_Dxf*           dxf           = new DL_Dxf();
    DL_WriterA*       dw;
    gerbv_aperture_t* apert;
    gerbv_image_t*    img;
    gerbv_net_t*      net;
    GArray*           apert_tab;
    double            x[4], y[4], r, dx, dy, nom;
    unsigned int      i;

    dw = dxf->out(file_name, exportVersion);

    if (dw == NULL) {
        GERB_MESSAGE(_("Can't open file for writing: %s"), file_name);
        return FALSE;
    }

    /* Output decimals as dots for all locales */
    setlocale(LC_NUMERIC, "C");

    /* Duplicate the image, cleaning it in the process */
    img = gerbv_image_duplicate_image(input_img, trans);

    dxf->writeHeader(*dw);

    dw->dxfString(DL_STRGRP_END, "$INSUNITS");
    dw->dxfInt(DL_ATTFLAGS_CODE, INSUNITS_INCH);

    dw->dxfString(DL_STRGRP_END, "$DIMEXE");
    dw->dxfReal(DL_TXTHI_CODE, 1.25);

    dw->dxfString(DL_STRGRP_END, "$TEXTSTYLE");
    dw->dxfString(DL_TXT_STYLE_CODE, "Standard");

    /* TODO ? */
    dw->dxfString(DL_STRGRP_END, "$LIMMIN");
    dw->dxfReal(DL_FIRST_XCOORD_CODE, 0.0);
    dw->dxfReal(DL_FIRST_YCOORD_CODE, 0.0);

    dw->sectionEnd();

    dw->sectionTables();
    dxf->writeVPort(*dw);

    /* Line types */
#if (DL_VERSION_MAJOR == 3)
    dw->tableLinetypes(1);
    dxf->writeLinetype(*dw, DL_LinetypeData("ByLayer", "ByLayer", 0, 0, 0));
#else
    dw->tableLineTypes(1);
    dxf->writeLineType(*dw, DL_LineTypeData("ByLayer", 0));
#endif

    dw->tableEnd();

    /* Layers */
    dw->tableLayers(1); /* One layer */

#if (DL_VERSION_MAJOR == 3)
    dxf->writeLayer(
        *dw, DL_LayerData("0", 0),
        DL_Attributes(
            "", DL_Codes::black, /* Color */
            10,                  /* Width */
            "Continuous", 1
        )
    ); /* Line style and scale */
#else
    dxf->writeLayer(
        *dw, DL_LayerData("0", 0),
        DL_Attributes(
            "", DL_Codes::black, /* Color */
            10,                  /* Width */
            "Continuous"
        )
    ); /* Line style */
#endif

    dw->tableEnd();

#if (DL_VERSION_MAJOR == 3)
    dxf->writeStyle(*dw, DL_StyleData("Standard", 0, 2.5, 1.0, 0.0, 0, 2.5, "txt", ""));
#else
    dxf->writeStyle(*dw);
#endif

    dxf->writeView(*dw);
    dxf->writeUcs(*dw);
    dw->tableAppid(1);
    dw->tableAppidEntry(0x12);
    dw->dxfString(2, "ACAD");
    dw->dxfInt(DL_ATTFLAGS_CODE, 0);
    dw->tableEnd();
    dw->sectionEnd();

    /* All entities */
    dw->sectionEntities();

#if (DL_VERSION_MAJOR == 3)
    DL_Attributes* attr = new DL_Attributes("0", 0, -1, "ByLayer", 1.0);
#else
    DL_Attributes* attr = new DL_Attributes("0", 0, -1, "ByLayer");
#endif

    for (net = img->netlist; net != NULL; net = net->next) {
        apert = img->aperture[net->aperture];
        if (!apert)
            continue;

        if (net->interpolation == GERBV_INTERPOLATION_PAREA_START) {
            dxf->writePolyline(*dw, DL_PolylineData(1, 0, 0, DL_CLOSED_PLINE), *attr);

            net = net->next;

            while (net != NULL && net->interpolation != GERBV_INTERPOLATION_PAREA_END) {
                if (net->aperture_state == GERBV_APERTURE_STATE_ON) {
                    dxf->writeVertex(*dw, DL_VertexData(COORD2INS(net->stop_x), COORD2INS(net->stop_y), 0, 0));
                }
                net = net->next;
            }

            dxf->writePolylineEnd(*dw);

            continue;
        }

        switch (net->aperture_state) {
            case GERBV_APERTURE_STATE_FLASH:
                switch (apert->type) {
                    case GERBV_APTYPE_CIRCLE:
                        x[0] = net->stop_x;
                        y[0] = net->stop_y;
                        r    = apert->parameter[0] / 2;
                        dxf->writeCircle(*dw, DL_CircleData(x[0], y[0], 0.0, r), *attr);
                        break;
                    case GERBV_APTYPE_RECTANGLE:
                        x[0] = net->stop_x + apert->parameter[0] / 2;
                        y[0] = net->stop_y + apert->parameter[1] / 2;
                        x[1] = x[0];
                        y[1] = y[0] - apert->parameter[1];
                        x[2] = x[1] - apert->parameter[0];
                        y[2] = y[1];
                        x[3] = x[2];
                        y[3] = y[0];
                        dxf->writePolyline(*dw, DL_PolylineData(4, 0, 0, DL_CLOSED_PLINE), *attr);
                        for (i = 0; i < 4; i++)
                            dxf->writeVertex(*dw, DL_VertexData(x[i], y[i], 0, 0));
                        dxf->writePolylineEnd(*dw);
                        break;
                    case GERBV_APTYPE_OVAL:
                        if (apert->parameter[0] > apert->parameter[1]) {
                            /* Horizontal oval */
                            r  = apert->parameter[1] / 2;
                            dx = apert->parameter[0] / 2 - r;

                            x[0] = net->stop_x - dx;
                            y[0] = net->stop_y + r;
                            x[1] = net->stop_x + dx;
                            y[1] = y[0];
                            x[2] = x[1];
                            y[2] = net->stop_y - r;
                            x[3] = x[0];
                            y[3] = y[2];
                        } else {
                            /* Vertical oval */
                            r  = apert->parameter[0] / 2;
                            dy = apert->parameter[1] / 2 - r;

                            x[0] = net->stop_x - r;
                            y[0] = net->stop_y - dy;
                            x[1] = x[0];
                            y[1] = net->stop_y + dy;
                            x[2] = net->stop_x + r;
                            y[2] = y[1];
                            x[3] = x[2];
                            y[3] = y[0];
                        }

                        dxf->writePolyline(*dw, DL_PolylineData(4, 0, 0, DL_CLOSED_PLINE), *attr);
                        dxf->writeVertex(*dw, DL_VertexData(x[3], y[3], 0, -1));
                        dxf->writeVertex(*dw, DL_VertexData(x[0], y[0], 0, 0));
                        dxf->writeVertex(*dw, DL_VertexData(x[1], y[1], 0, -1));
                        dxf->writeVertex(*dw, DL_VertexData(x[2], y[2], 0, 0));

                        dxf->writePolylineEnd(*dw);
                        break;
                    case GERBV_APTYPE_MACRO:
                    default:
                        /* TODO: other GERBV_APTYPE_ */
                        GERB_COMPILE_WARNING(
                            "%s:%d: aperture type %d is "
                            "not yet supported",
                            __func__, __LINE__, apert->type
                        );
                        break;
                }
                break;
            case GERBV_APERTURE_STATE_ON:
                /* Line or cut slot in drill file */
                switch (apert->type) {
                    case GERBV_APTYPE_CIRCLE:
                        /* Calculate perpendicular vector */
                        dx  = net->stop_x - net->start_x;
                        dy  = net->stop_y - net->start_y;
                        nom = apert->parameter[0] / (2 * sqrt(dx * dx + dy * dy));
                        dx  = dx * nom;
                        dy  = dy * nom;

                        /* Line 1 of 2 */
                        x[0] = net->stop_x + dy;
                        y[0] = net->stop_y - dx;
                        x[1] = net->start_x + dy;
                        y[1] = net->start_y - dx;

                        /* Line 2 of 2 */
                        x[2] = net->start_x - dy;
                        y[2] = net->start_y + dx;
                        x[3] = net->stop_x - dy;
                        y[3] = net->stop_y + dx;

                        dxf->writePolyline(*dw, DL_PolylineData(4, 0, 0, DL_CLOSED_PLINE), *attr);
                        dxf->writeVertex(*dw, DL_VertexData(x[3], y[3], 0, -1));
                        dxf->writeVertex(*dw, DL_VertexData(x[0], y[0], 0, 0));
                        dxf->writeVertex(*dw, DL_VertexData(x[1], y[1], 0, -1));
                        dxf->writeVertex(*dw, DL_VertexData(x[2], y[2], 0, 0));

                        dxf->writePolylineEnd(*dw);
                        break;
                    default:
                        GERB_COMPILE_WARNING(
                            "%s:%d: aperture type %d is "
                            "not yet supported",
                            __func__, __LINE__, apert->type
                        );
                        break;
                }
                break;
            default: break;
        }
    }

    gerbv_destroy_image(img);

    dw->sectionEnd();

    dxf->writeObjects(*dw);
    dxf->writeObjectsEnd(*dw);
    dw->dxfEOF();
    dw->close();

    delete attr;
    delete dw;
    delete dxf;

    setlocale(LC_NUMERIC, ""); /* Return to the default locale */

    return TRUE;
}
} /* extern "C" */

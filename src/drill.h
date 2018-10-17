/*
 * gEDA - GNU Electronic Design Automation
 * drill.h
 * Copyright (C) 2000-2001 Stefan Petersen (spe@stacken.kth.se)
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

/** \file drill.h
    \brief Header info for the Excellon drill parsing functions
    \ingroup libgerbv
*/

#ifndef DRILL_H
#define DRILL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <glib.h>
#include "gerb_image.h"
#include "gerb_file.h"

#define TOOL_MIN 1  /* T00 code is reserved for unload tool command */
#define TOOL_MAX 9999

gerbv_image_t *parse_drillfile(gerb_file_t *fd, gerbv_HID_Attribute *attr_list,
				int n_attr, int reload);
gboolean drill_file_p(gerb_file_t *fd, gboolean *returnFoundBinary);

/* NOTE: keep drill_g_code_t in actual G code order. */
typedef enum {
	DRILL_G_UNKNOWN = -1,

	DRILL_G_ROUT = 0, /* Route mode */
	DRILL_G_LINEARMOVE, /* Linear (straight line) mode */
	DRILL_G_CWMOVE,  /* Circular CW mode */
	DRILL_G_CCWMOVE, /* Circular CCW mode */
	DRILL_G_VARIABLEDWELL, /* Variable dwell */
	DRILL_G_DRILL, /* Drill mode */

	DRILL_G_OVERRIDETOOLSPEED = 7, /* Override current tool feed or speed */

	DRILL_G_ROUTCIRCLE = 32, /* Routed circle canned cycle CW */
	DRILL_G_ROUTCIRCLECCW,   /* Routed circle canned cycle CCW */
	DRILL_G_VISTOOL, /* Select vision tool */
	DRILL_G_VISSINGLEPOINTOFFSET, /* Single point vision offset */
	DRILL_G_VISMULTIPOINTTRANS, /* Multipoint vision translation */
	DRILL_G_VISCANCEL, /* Cancel vision translation or offset */
	DRILL_G_VISCORRHOLEDRILL, /* Vision corrected single hole drilling */
	DRILL_G_VISAUTOCALIBRATION, /* Vision system autocalibration */
	DRILL_G_CUTTERCOMPOFF, /* Cutter compensation off */
	DRILL_G_CUTTERCOMPLEFT, /* Cutter compensation left */
	DRILL_G_CUTTERCOMPRIGHT, /* Cutter compensation right */

	DRILL_G_VISSINGLEPOINTOFFSETREL = 45, /* Single point vision offset
						 relative to G35 or G36 */
	DRILL_G_VISMULTIPOINTTRANSREL, /* Multipoint vision translation
					  relative to G35 or G36 */
	DRILL_G_VISCANCELREL, /* Cancel vision translation or offset
				 from G45 or G46 */
	DRILL_G_VISCORRHOLEDRILLREL, /* Vision corrected single hole drilling
					relative to G35 or G36 */
	DRILL_G_PACKDIP2 = 81,/* Dual in line package, same to G82 in Format2 */
	DRILL_G_PACKDIP, /* Dual in line package */
	DRILL_G_PACK8PINL, /* Eight pin L pack */
	DRILL_G_CIRLE, /* Canned circle */
	DRILL_G_SLOT, /* Canned slot */

	DRILL_G_ROUTSLOT = 87, /* Routed slot canned cycle */

	DRILL_G_ABSOLUTE = 90, /* Absolute input mode */
	DRILL_G_INCREMENTAL, /* Incremental input mode */

	DRILL_G_ZEROSET = 93, /* Sets work zero relative to absolute zero */
} drill_g_code_t;

/* NOTE: keep drill_m_code_t in actual M code order. */
typedef enum {
	DRILL_M_UNKNOWN = -1,

	DRILL_M_END = 0,
	DRILL_M_PATTERNEND,
	DRILL_M_REPEATPATTERNOFFSET,

	DRILL_M_STOPOPTIONAL = 6,

	DRILL_M_SANDREND = 8,	/* Step and repeat */
	DRILL_M_STOPINSPECTION,

	DRILL_M_ZAXISROUTEPOSITIONDEPTHCTRL = 14,
	DRILL_M_ZAXISROUTEPOSITION,
	DRILL_M_RETRACTCLAMPING,
	DRILL_M_RETRACTNOCLAMPING,
	DRILL_M_TOOLTIPCHECK,

	DRILL_M_PATTERN = 25,
	DRILL_M_ENDREWIND = 30,
	DRILL_M_MESSAGELONG = 45,

	DRILL_M_MESSAGE = 47,
	DRILL_M_HEADER,

	DRILL_M_VISANDRPATTERN = 50,	/* Visual step and repeat */
	DRILL_M_VISANDRPATTERNREWIND,
	DRILL_M_VISANDRPATTERNOFFSETCOUNTERCTRL,

	DRILL_M_REFSCALING = 60,	/* Reference scaling */
	DRILL_M_REFSCALINGEND,
	DRILL_M_PECKDRILLING,
	DRILL_M_PECKDRILLINGEND,

	DRILL_M_SWAPAXIS = 70,
	DRILL_M_METRIC,
	DRILL_M_IMPERIAL,

	DRILL_M_MIRRORX = 80,
	DRILL_M_MIRRORY = 90,
	DRILL_M_HEADEREND = 95,

	DRILL_M_CANNEDTEXTX = 97,
	DRILL_M_CANNEDTEXTY,
	DRILL_M_USERDEFPATTERN,
} drill_m_code_t;

const char *drill_g_code_name(drill_g_code_t g_code);
const char *drill_m_code_name(drill_m_code_t m_code);

#ifdef __cplusplus
}
#endif

#endif /* DRILL_H */

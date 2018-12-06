/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2014 Florian Hirschberg
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

/** \file export-isel-drill.c
    \brief Functions for exporting gerbv images to ISEL Automation
           NCP file format with drill commands for CNC machines
    \ingroup libgerbv
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <math.h>

#include <glib/gstdio.h>
#include "gerbv.h"
#include "common.h"

/* DEBUG printing.  #define DEBUG 1 in config.h to use this fcn. */
#define dprintf if(DEBUG) printf
#define round(x) floor(x+0.5)

gboolean
gerbv_export_isel_drill_file_from_image (const gchar *filename, gerbv_image_t *inputImage,
		gerbv_user_transformation_t *transform) {
	FILE *fd;
	GArray *apertureTable = g_array_new(FALSE,FALSE,sizeof(int));
	gerbv_net_t *currentNet;

	/* force gerbv to output decimals as dots (not commas for other locales) */
	setlocale(LC_NUMERIC, "C");

	if ((fd = g_fopen(filename, "w")) == NULL) {
		GERB_COMPILE_ERROR(_("Can't open file for writing: %s"),
				filename);

		return FALSE;
	}

	/* duplicate the image, cleaning it in the process */
	gerbv_image_t *image = gerbv_image_duplicate_image (inputImage, transform);

	/* write header info */
	fprintf(fd,
		"IMF_PBL_V1.0\r\n"
		"\r\n"
		"; Please change this parameters to your needs\r\n"
		"; Don't forget to change the drill depth to\r\n"
		"; your PCB thickness\r\n"
		"\r\n"
		"; The normal movement velocity in 1/1000 mm/s\r\n"
		"VEL 5000\r\n"
		"\r\n"
		"; The fast movement velocity in 1/1000 mm/s\r\n"
		"FASTVEL 10000\r\n"
		"; The safety height in mm over the PCB for movement\r\n"
		"\r\n"
		"DRILLDEF S2000\r\n"
		"\r\n"
		"; Drill velocity of downwards movement in 1/1000 mm/s\r\n"
		"\r\n"
		"DRILLDEF V1000\r\n"
		"\r\n"
		"; The drill depth in 1/1000 mm\r\n"
		"\r\n"
		"DRILLDEF D1800 ; 1.5mm material + 0.3mm break through\r\n"
		"\r\n"
		"; DO NOT CHANGE THESE PARAMETERS!!\r\n"
		"\r\n"
		"DRILLDEF C1 P0\r\n"
		"\r\n");

	/* define all apertures */
	gerbv_aperture_t *currentAperture;
	gint i;

	/* the image should already have been cleaned by a duplicate_image call, so we can safely
	   assume the aperture range is correct */
	for (i=APERTURE_MIN; i<APERTURE_MAX; i++) {
		currentAperture = image->aperture[i];

		if (!currentAperture)
			continue;

		switch (currentAperture->type) {
		case GERBV_APTYPE_CIRCLE:
			/* add the "approved" aperture to our valid list */
			fprintf(fd, "; TOOL %d - Diameter %1.3f mm\r\n",
				i + 1,
				COORD2MMS(currentAperture->parameter[0]));
			g_array_append_val (apertureTable, i);

			break;
		default:
			break;
		}
	}

	/* write rest of image */

	for (i=0; i<apertureTable->len; i++) {
		int currentAperture=g_array_index (apertureTable, int, i);

		/* write tool change */
		fprintf(fd, "GETTOOL %d\r\n",currentAperture+1);

		/* run through all nets and look for drills using this aperture */
		for (currentNet = image->netlist; currentNet;
				currentNet = currentNet->next) {

			if (currentNet->aperture != currentAperture)
				continue;

			switch (currentNet->aperture_state) {
			case GERBV_APERTURE_STATE_FLASH: {
				long xVal,yVal;

				xVal = (long) round(COORD2MMS(currentNet->stop_x)*1e3);
				yVal = (long) round(COORD2MMS(currentNet->stop_y)*1e3);
				fprintf(fd, "DRILL X%06ld Y%06ld\r\n",
						xVal, yVal);
				break;
			}
			default:
				GERB_COMPILE_WARNING(
					_("Skipped to export of unsupported state %d "
					"interpolation \"%s\""),
					currentNet->aperture_state,
					gerbv_interpolation_name(
						currentNet->interpolation));
			}
		}
	}
	g_array_free (apertureTable, TRUE);
	/* write footer */
	fprintf(fd, "PROGEND\r\n");
	gerbv_destroy_image (image);
	fclose(fd);

	/* return to the default locale */
	setlocale(LC_NUMERIC, "");
	return TRUE;
}

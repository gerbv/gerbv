/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2008 Julian Lamb
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

/** \file export-drill.c
    \brief Functions for exporting gerbv images to Excellon drill files
    \ingroup libgerbv
*/

#include "gerbv.h"

#include <math.h>
#include <glib/gstdio.h>

#include "common.h"

#define dprintf if(DEBUG) printf

#define round(x) floor(x+0.5)

gboolean
gerbv_export_drill_file_from_image (const gchar *filename, gerbv_image_t *inputImage,
		gerbv_user_transformation_t *transform) {
	FILE *fd;
	GArray *apertureTable = g_array_new(FALSE, FALSE, sizeof(int));
	gerbv_net_t *net;
	
	/* force gerbv to output decimals as dots (not commas for other locales) */
	setlocale(LC_NUMERIC, "C");
	
	if ((fd = g_fopen(filename, "w")) == NULL) {
		GERB_COMPILE_ERROR( _("Can't open file for writing: %s"),
				filename);
		return FALSE;
	}
	
	/* duplicate the image, cleaning it in the process */
	gerbv_image_t *image = gerbv_image_duplicate_image (inputImage, transform);
	
	/* write header info */
	fprintf(fd, "M48\n");
	fprintf(fd, "INCH,TZ\n");

	/* define all apertures */
	gerbv_aperture_t *aperture;
	gint i;

	/* the image should already have been cleaned by a duplicate_image call, so we can safely
	   assume the aperture range is correct */
	for (i = APERTURE_MIN; i < APERTURE_MAX; i++) {
		aperture = image->aperture[i];
		
		if (!aperture)
			continue;

		switch (aperture->type) {
		case GERBV_APTYPE_CIRCLE:
			fprintf(fd, "T%dC%1.3f\n", i, aperture->parameter[0]);
			/* add the "approved" aperture to our valid list */
			g_array_append_val(apertureTable, i);
			break;
		default:
			break;
		}
	}
	
	fprintf(fd, "%%\n");
	/* write rest of image */
	
	for (i = 0; i < apertureTable->len; i++) {
		int aperture_idx = g_array_index(apertureTable, int, i);
		
		/* write tool change */
		fprintf(fd, "T%d\n", aperture_idx);
		
		/* run through all nets and look for drills using this aperture */
		for (net = image->netlist; net; net = net->next) {
			if (net->aperture != aperture_idx)
				continue;

			switch (net->aperture_state) {
			case GERBV_APERTURE_STATE_FLASH:
				fprintf(fd, "X%06ldY%06ld\n",
					(long)round(net->stop_x * 10000.0),
					(long)round(net->stop_y * 10000.0));
				break;
			case GERBV_APERTURE_STATE_ON:	/* Cut slot */
				fprintf(fd, "X%06ldY%06ldG85X%06ldY%06ld\n",
					(long)round(net->start_x * 10000.0),
					(long)round(net->start_y * 10000.0),
					(long)round(net->stop_x * 10000.0),
					(long)round(net->stop_y * 10000.0));
				break;
			default:
				break;
			}
		}
	}
	g_array_free (apertureTable, TRUE);

	/* write footer */
	fprintf(fd, "M30\n\n");
	gerbv_destroy_image(image);
	fclose(fd);
	
	/* return to the default locale */
	setlocale(LC_NUMERIC, "");
	return TRUE;
}

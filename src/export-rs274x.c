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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include "gerber.h"
#include <export-rs274x.h>

/* DEBUG printing.  #define DEBUG 1 in config.h to use this fcn. */
#define dprintf if(DEBUG) printf

void
export_rs274x_write_apertures (FILE *fd, gerb_image_t *image) {
	gerb_aperture_t *currentAperture;
	gint numberOfRequiredParameters,numberOfOptionalParameters,i,j;
		
	for (i=APERTURE_MIN; i<APERTURE_MAX; i++) {
		gboolean validAperture=TRUE;
		
		currentAperture = image->aperture[i];
		
		if (!currentAperture)
			continue;
		
		switch (currentAperture->type) {
			case CIRCLE:
				fprintf(fd, "%%ADD%d",i);
				fprintf(fd, "C,");
				numberOfRequiredParameters = 1;
				numberOfOptionalParameters = 2;
				break;
			case RECTANGLE:
				fprintf(fd, "%%ADD%d",i);
				fprintf(fd, "R,");
				numberOfRequiredParameters = 2;
				numberOfOptionalParameters = 2;
				break;
			case OVAL:
				fprintf(fd, "%%ADD%d",i);
				fprintf(fd, "O,");
				numberOfRequiredParameters = 2;
				numberOfOptionalParameters = 2;
				break;
			case POLYGON:
				fprintf(fd, "%%ADD%d",i);
				fprintf(fd, "P,");
				numberOfRequiredParameters = 2;
				numberOfOptionalParameters = 3;
				break;
			default:
				validAperture=FALSE;
				break;
		}
		if (!validAperture)
			continue;
		/* write the parameter list */
		for (j=0; j<(numberOfRequiredParameters + numberOfOptionalParameters); j++) {
			if ((j < numberOfRequiredParameters) || (currentAperture->parameter[j] != 0)) {
				/* print the "X" character to separate the parameters */
				if (j>0)
					fprintf(fd, "X");
				fprintf(fd, "%.4f",currentAperture->parameter[j]);
			}
		}
		fprintf(fd, "*%%\n");
	}
}

gboolean
export_rs274x_file_from_image (gchar *filename, gerb_image_t *image) {
	FILE *fd;

	if ((fd = g_fopen(filename, "w")) == NULL) {
		GERB_MESSAGE("Can't open file for writing: %s\n", filename);
		return FALSE;
	}
	/* write header info */
	fprintf(fd, "G04 Created by Gerber Viewer*\n");
	fprintf(fd, "G04 --Header info--*\n");
	fprintf(fd, "%%MOIN*%%\n");
	fprintf(fd, "%%FSLAX23Y23*%%\n");
	
	/* define all apertures */
	fprintf(fd, "G04 --Define apertures--*\n");
	export_rs274x_write_apertures (fd, image);
	
	/* write rest of image */
	fprintf(fd, "G04 --Start main section--*\n");
	gint currentAperture = 0;
	gerb_net_t *currentNet;
	
	for (currentNet = image->netlist; currentNet; currentNet = currentNet->next){
		/* check for "layer" changes (RS274X commands) */
		
		/* check for new "netstate" (more RS274X commands) */
		
		/* check for tool changes */
		if (currentNet->aperture != currentAperture) {
			fprintf(fd, "G54D%02d*\n",currentNet->aperture);
			currentAperture = currentNet->aperture;
		}
		long xVal,yVal,endX,endY,centerX,centerY;;
		switch (currentNet->interpolation) {
			case LINEARx10 :
			case LINEARx01 :
			case LINEARx001 :
			case LINEARx1 :
				xVal = (long) (currentNet->stop_x * 1000.0);
				yVal = (long) (currentNet->stop_y * 1000.0);
				fprintf(fd, "G01X%05ldY%05ld",xVal,yVal);
				/* and finally, write the esposure value */
				if (currentNet->aperture_state == OFF)
					fprintf(fd, "D02*\n");
				else if (currentNet->aperture_state == ON)
					fprintf(fd, "D01*\n");
				else
					fprintf(fd, "D03*\n");
				break;
			case CW_CIRCULAR :
			case CCW_CIRCULAR :
				// FIXME: we need to reorganize polygon fills so that
				//	circular paths aren't chopped up into lines inside
				//	the gerb_image
				if (!currentNet->cirseg)
					break;
				centerX= (long) ((currentNet->cirseg->cp_x - currentNet->start_x) * 1000.0);
				centerY= (long) ((currentNet->cirseg->cp_y - currentNet->start_y) * 1000.0);
				endX = (long) (currentNet->stop_x * 1000.0);
				endY = (long) (currentNet->stop_y * 1000.0);
				/* always use multi-quadrant, since it's much easier to export */
				/*  and most all software should support it */
				fprintf(fd, "G75*\n");
				/* figure out clockwise or c-clockwise */
				if (currentNet->cirseg->angle2 > currentNet->cirseg->angle1)
					fprintf(fd, "G03");
				else
					fprintf(fd, "G02");
				/* don't write the I and J values if the exposure is off */
				if (currentNet->aperture_state == ON)
					fprintf(fd, "X%05ldY%05ldI%05ldJ%05ld",endX,endY,centerX,centerY);
				else
					fprintf(fd, "X%05ldY%05ld",endX,endY);
				/* and finally, write the esposure value */
				if (currentNet->aperture_state == OFF)
					fprintf(fd, "D02*\n");
				else if (currentNet->aperture_state == ON)
					fprintf(fd, "D01*\n");
				else
					fprintf(fd, "D03*\n");
				break;
			case PAREA_START:
				fprintf(fd, "G36*");
				break;
			case PAREA_END:
				fprintf(fd, "G37*");
				break;
			default:
				break;
		}
	}
	
	/* write footer */
	fprintf(fd, "G04 --Footer info--*\n");
	fprintf(fd, "M02*\n");

	fclose(fd);
	return TRUE;
}

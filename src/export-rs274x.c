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

/** \file export-rs274x.c
    \brief Functions to export an image to a RS274X file
    \ingroup libgerbv
*/


#include "gerbv.h"

#include <math.h>
#include <ctype.h>
#include <glib/gstdio.h>

#include "common.h"
#include "gerber.h"
#include "x2attr.h"

#define dprintf if(DEBUG) printf

#define round(x) floor(x+0.5)

// User data passed to x2_attr_foreach callback, and numeric output formatter.
typedef struct export_user_data
{
        int std_version;                // RS274-Xn standard (currently, 1 or 2)
        FILE *fd;                       // File to which we are writing
        x2attr_type_t type;             // Attribute type ('F','A' or 'O')
        struct x2attr_dict * tracker;   // Dictionary used to maintain current value of all attribs output.
                                        // This is so we don't dumbly repeat the same key,value pairs.
                                        // A single table suffices for all types, since the standard mandates
                                        // unique keys regardless of type.
        int decimals;                   // Number of fractional decimal places
        int digits;                     // Total decimal digits (max)
        gboolean lzc;                   // Whether to do leading zero compression
        gdouble multiplier;             // Multiplier for converting float to int.  This should include the factor
                                        // 10**decimals, and an additional factor of 25.4 for mm output.
        char xystr[128];                // Work area for string output
        char ijstr[128];                // Work area for string output
} export_user_data_t;

static void 
_dump_attribs(unsigned index, const char * key, const char * value, gpointer user_data)
{
        export_user_data_t * xud = (export_user_data_t *)user_data;
        const char * newval;
        
        // Keys and values are assumed to be valid for direct output.  This will be the case if
        // they are unchanged after being imported from the original file.  If there are any
        // programmatic additions or modifications, then the caller is responsible for
        // assuring validity (e.g. '*' chars must be escaped to '\u002A').
        
        // To assist in generating correct output, if the first char of the key is invalid for a
        // legitimate attribute name (i.e. is not alpha, underscore, dot or dollar ascii) then the
        // attribute is not exported.  So programmatic attributes which are not intended for
        // export should have their keys prefixed with a digit, space or most any punctuation.
        if (!isalpha(key[0]) && key[0] != '.' && key[0] != '_' && key[0] != '$')
                return;
        // .MD5 is special in that it needs to read the entire file in canonical format, up to
        // just before the M02.
        //TODO: make use of GChecksum.
        if (!strcmp(key, ".MD5"))
                return;
        newval = _x2attr_value_changed(xud->tracker, key, value);
        if (newval) {
                char type = (char)xud->type;
                if (xud->type == X2ATTR_OBJECT && index==1)
                        // Special case for object attributes.  There will be a chained state containing
                        // aperture attributes behind the object attributes.  So change the output type.
                        type = 'A';
                fprintf(xud->fd, "%%T%c%s%s%s*%%\n", type, key, *newval ? "," : "", newval);
        }
}

static char *
_format_pair(export_user_data_t * xud, gdouble x, gdouble y, gboolean ij)
{
        // Return a properly formatted XY (or IJ) pair.
        char a = ij ? 'I' : 'X';
        char b = ij ? 'J' : 'Y';
        char * p = ij ? xud->ijstr : xud->xystr;
        
        if (xud->lzc)
                sprintf(p, "%c%ld%c%ld", 
                        a,
                        (long)round(x*xud->multiplier), 
                        b,
                        (long)round(y*xud->multiplier));
        else
                sprintf(p, "%c%0*ld%c%0*ld", 
                        a,
                        xud->digits, (long)round(x*xud->multiplier), 
                        b,
                        xud->digits, (long)round(y*xud->multiplier));
        return p;
}


static void
export_rs274x_write_macro (FILE *fd, gerbv_aperture_t *currentAperture,
			gint apertureNumber, gdouble tounits) {
	gerbv_simplified_amacro_t *ls = currentAperture->simplified;

	/* write the macro portion first */
	fprintf(fd, "%%AMMACRO%d*\n",apertureNumber);
	while (ls != NULL) {
		if (ls->type == GERBV_APTYPE_MACRO_CIRCLE) {
			fprintf(fd, "1,%d,%f,%f,%f*\n",(int) ls->parameter[CIRCLE_EXPOSURE],
				ls->parameter[CIRCLE_DIAMETER]*tounits,ls->parameter[CIRCLE_CENTER_X]*tounits,
				ls->parameter[CIRCLE_CENTER_Y]*tounits);
		}
		else if (ls->type == GERBV_APTYPE_MACRO_OUTLINE) {
			int pointCounter;
			int numberOfPoints = (int) ls->parameter[OUTLINE_NUMBER_OF_POINTS];
			
			/* for Flatcam no new line after this 3 digits */
			fprintf(fd, "4,%d,%d,",(int) ls->parameter[OUTLINE_EXPOSURE],
				numberOfPoints);
			/* add 1 point for the starting point here */
			for (pointCounter=0; pointCounter <= numberOfPoints; pointCounter++) {
			    fprintf(fd, "%f,%f,",ls->parameter[pointCounter * 2 + OUTLINE_FIRST_X]*tounits,
					   ls->parameter[pointCounter * 2 + OUTLINE_FIRST_Y]*tounits);
			}
			fprintf(fd, "%f*\n",ls->parameter[pointCounter * 2 + OUTLINE_FIRST_X]);
		}
		else if (ls->type == GERBV_APTYPE_MACRO_POLYGON) {
			fprintf(fd, "5,%d,%d,%f,%f,%f,%f*\n",(int) ls->parameter[POLYGON_EXPOSURE],
				(int) ls->parameter[POLYGON_NUMBER_OF_POINTS],
				ls->parameter[POLYGON_CENTER_X]*tounits,ls->parameter[POLYGON_CENTER_Y]*tounits,
				ls->parameter[POLYGON_DIAMETER]*tounits,ls->parameter[POLYGON_ROTATION]);
		}
		else if (ls->type == GERBV_APTYPE_MACRO_MOIRE) {
			fprintf(fd, "6,%f,%f,%f,%f,%f,%d,%f,%f,%f*\n",ls->parameter[MOIRE_CENTER_X]*tounits,
				ls->parameter[MOIRE_CENTER_Y]*tounits,ls->parameter[MOIRE_OUTSIDE_DIAMETER]*tounits,
				ls->parameter[MOIRE_CIRCLE_THICKNESS]*tounits,ls->parameter[MOIRE_GAP_WIDTH]*tounits,
				(int) ls->parameter[MOIRE_NUMBER_OF_CIRCLES],ls->parameter[MOIRE_CROSSHAIR_THICKNESS]*tounits,
				ls->parameter[MOIRE_CROSSHAIR_LENGTH]*tounits,ls->parameter[MOIRE_ROTATION]);
		}
		else if (ls->type == GERBV_APTYPE_MACRO_THERMAL) {
			fprintf(fd, "7,%f,%f,%f,%f,%f,%f*\n",ls->parameter[THERMAL_CENTER_X]*tounits,
				ls->parameter[THERMAL_CENTER_Y]*tounits,ls->parameter[THERMAL_OUTSIDE_DIAMETER]*tounits,
				ls->parameter[THERMAL_INSIDE_DIAMETER]*tounits,ls->parameter[THERMAL_CROSSHAIR_THICKNESS]*tounits,
				ls->parameter[THERMAL_ROTATION]);
		}
		else if (ls->type == GERBV_APTYPE_MACRO_LINE20) {
			fprintf(fd, "20,%d,%f,%f,%f,%f,%f,%f*\n",(int) ls->parameter[LINE20_EXPOSURE],
				ls->parameter[LINE20_LINE_WIDTH]*tounits,ls->parameter[LINE20_START_X]*tounits,
				ls->parameter[LINE20_START_Y]*tounits,ls->parameter[LINE20_END_X]*tounits,
				ls->parameter[LINE20_END_Y]*tounits,ls->parameter[LINE20_ROTATION]);
		}
		else if (ls->type == GERBV_APTYPE_MACRO_LINE21) {
			fprintf(fd, "21,%d,%f,%f,%f,%f,%f*\n",(int) ls->parameter[LINE21_EXPOSURE],
				ls->parameter[LINE21_WIDTH]*tounits,ls->parameter[LINE21_HEIGHT]*tounits,
				ls->parameter[LINE21_CENTER_X]*tounits,ls->parameter[LINE21_CENTER_Y]*tounits,
				ls->parameter[LINE21_ROTATION]);
		}
		else if (ls->type == GERBV_APTYPE_MACRO_LINE22) {
			fprintf(fd, "22,%d,%f,%f,%f,%f,%f*\n",(int) ls->parameter[LINE22_EXPOSURE],
				ls->parameter[LINE22_WIDTH]*tounits,ls->parameter[LINE22_HEIGHT]*tounits,
				ls->parameter[LINE22_LOWER_LEFT_X]*tounits,ls->parameter[LINE22_LOWER_LEFT_Y]*tounits,
				ls->parameter[LINE22_ROTATION]);
		}
		ls = ls->next;
	}
	fprintf(fd, "%%\n");
	/* and finally create an aperture definition to use the macro */
	fprintf(fd, "%%ADD%dMACRO%d*%%\n",apertureNumber,apertureNumber);
}

static void
export_rs274x_write_apertures (FILE *fd, gerbv_image_t *image, gdouble tounits, export_user_data_t * xud) {
	gerbv_aperture_t *currentAperture;
	gint numberOfRequiredParameters=0,numberOfOptionalParameters=0,i,j;
	
	/* the image should already have been cleaned by a duplicate_image call, so we can safely
	   assume the aperture range is correct */
	for (i=APERTURE_MIN; i<APERTURE_MAX; i++) {
		gboolean writeAperture=TRUE;
		gdouble units[APERTURE_PARAMETERS_MAX];
		
		// Assume all units are linear and scale inch<->mm.
		for (j = 0; j < APERTURE_PARAMETERS_MAX; ++j)
		        units[j] = tounits;
		
		currentAperture = image->aperture[i];
		
		if (!currentAperture)
			continue;

                if (xud->std_version > 1) {
	                xud->type = X2ATTR_APERTURE;
	                x2attr_foreach_aperture_attr(currentAperture, _dump_attribs, xud);
	        }
		
		switch (currentAperture->type) {
			case GERBV_APTYPE_CIRCLE:
				fprintf(fd, "%%ADD%d",i);
				fprintf(fd, "C,");
				numberOfRequiredParameters = 1;
				numberOfOptionalParameters = 2;
				break;
			case GERBV_APTYPE_RECTANGLE:
				fprintf(fd, "%%ADD%d",i);
				fprintf(fd, "R,");
				numberOfRequiredParameters = 2;
				numberOfOptionalParameters = 2;
				break;
			case GERBV_APTYPE_OVAL:
				fprintf(fd, "%%ADD%d",i);
				fprintf(fd, "O,");
				numberOfRequiredParameters = 2;
				numberOfOptionalParameters = 2;
				break;
			case GERBV_APTYPE_POLYGON:
				fprintf(fd, "%%ADD%d",i);
				fprintf(fd, "P,");
				numberOfRequiredParameters = 2;
				numberOfOptionalParameters = 3;
				units[1] = 1.;  // #vertices is a number
				units[2] = 1.;  // rotation is an angle
				break;
			case GERBV_APTYPE_MACRO:
				export_rs274x_write_macro (fd, currentAperture, i, tounits);
				writeAperture=FALSE;
				break;
			default:
				writeAperture=FALSE;
				break;
		}
		if (writeAperture) {
			/* write the parameter list */
			for (j=0; j<(numberOfRequiredParameters + numberOfOptionalParameters); j++) {
				if ((j < numberOfRequiredParameters) || (currentAperture->parameter[j] != 0)) {
					/* print the "X" character to separate the parameters */
					if (j>0)
						fprintf(fd, "X");
					fprintf(fd, "%.4f",currentAperture->parameter[j]*units[j]);
				}
			}
			fprintf(fd, "*%%\n");
		}
	}
}

static void
export_rs274x_write_layer_change (gerbv_layer_t *oldLayer, gerbv_layer_t *newLayer, FILE *fd) {
	if (oldLayer->polarity != newLayer->polarity) {
		/* polarity changed */
		if ((newLayer->polarity == GERBV_POLARITY_CLEAR))
			fprintf(fd, "%%LPC*%%\n");
		else
			fprintf(fd, "%%LPD*%%\n");
	}
}

static void
export_rs274x_write_state_change (gerbv_netstate_t *oldState, gerbv_netstate_t *newState, FILE *fd) {


}



// Common export routine.
// std_version is 1 (for rs274-x), 2 (for rs274-x2) others TBD.
// Main diff is the X2 writes out file, aperture and object attributes.  These may be from the original
// file read in, or from a -x file read in and augmented with IPC-D-356 data, attributes added
// by the application, etc.
static gboolean
_export(int std_version
                , const gchar *filename
		, gerbv_image_t *inputImage
		, gerbv_user_transformation_t *transform
		, int decimals
		, int digits
		, gboolean metric
                )
{
	double decimal_coeff;
	double tounits;
	FILE *fd;
	gerbv_netstate_t *oldState;
	gerbv_layer_t *oldLayer;
	gboolean insidePolygon=FALSE;
	gerbv_user_transformation_t *thisTransform;
	gboolean x2 = std_version == 2;
	export_user_data_t xud;
	const int maxdigits = sizeof(long) > 4 ? 12 : 9;
        
	// Enforce some sanity.  Current standard recommends using mm with 6 decimals (nanometer) since
	// that encompases all reasonable inch resolutions exactly, and reduces possibility of numeric
	// instability changing region topology. 
	if (decimals < (metric ? 2 : 3))
	        decimals = metric ? 2 : 3;
	if (decimals > (metric ? 6 : 7))
	        decimals = (metric ? 6 : 7);
	if (decimals > digits)
	        digits = decimals;
	if (digits > maxdigits)
	        digits = maxdigits;
	
	tounits = metric ? 25.4 : 1.;      
	decimal_coeff = pow(10., decimals) * tounits;

	// force gerbv to output decimals as dots (not commas for other locales)
	setlocale(LC_NUMERIC, "C");

	if (transform != NULL) {
		thisTransform = transform;
	} else {
		static gerbv_user_transformation_t identityTransform =
						{0,0,1,1,0,FALSE,FALSE,FALSE};
		thisTransform = &identityTransform;
	}
	if ((fd = g_fopen(filename, "w")) == NULL) {
		GERB_COMPILE_ERROR(_("Can't open file for writing: %s"),
				filename);
		return FALSE;
	}
	
	xud.fd = fd;
	xud.tracker = _x2attr_new_dict();
	xud.decimals = decimals;
	xud.digits = digits;
	xud.multiplier = decimal_coeff;
	xud.std_version = std_version;
	
	/* duplicate the image, cleaning it in the process */
	gerbv_image_t *image = gerbv_image_duplicate_image (inputImage, thisTransform);
	
	/* write header info */
	if (x2) {
	        // Write all file attributes first, to the header.
	        //TODO: MD5 checksum goes at end - not implemented and suppressed for now.
	        xud.type = X2ATTR_FILE;
	        x2attr_foreach_image_attr(image, _dump_attribs, &xud);
	}
        fprintf(fd, "G04 This is an RS-274x file exported by *\n");
        fprintf(fd, "G04 gerbv version %s *\n",VERSION);
        fprintf(fd, "G04 More information is available about gerbv at *\n");
        fprintf(fd, "G04 https://gerbv.github.io/ *\n");
        fprintf(fd, "G04 --End of header info--*\n");

	fprintf(fd, metric ? "%%MOMM*%%\n" : "%%MOIN*%%\n");
	fprintf(fd, "%%FSLAX%d%dY%d%d*%%\n", digits-decimals, decimals, digits-decimals, decimals);

        // We just output FSLA which means leading zero suppression.
	xud.lzc = TRUE;
	
	/* check the image info struct for any non-default settings */
	/* image offset */
	if ((image->info->offsetA > 0.0) || (image->info->offsetB > 0.0))
		fprintf(fd, "%%IOA%fB%f*%%\n",image->info->offsetA*tounits,image->info->offsetB*tounits);
	/* image polarity */
	if (image->info->polarity == GERBV_POLARITY_CLEAR)
		fprintf(fd, "%%IPNEG*%%\n");
	else
		fprintf(fd, "%%IPPOS*%%\n");
	/* image name */
	if (image->info->name)
		fprintf(fd, "%%IN%s*%%\n",image->info->name);
	/* plotter film */
	if (image->info->plotterFilm)
		fprintf(fd, "%%PF%s*%%\n",image->info->plotterFilm);

	/* image rotation */
	if ((image->info->imageRotation != 0.0)
	     ||  (thisTransform->rotation != 0.0))
		fprintf(fd, "%%IR%d*%%\n",
		        (int)round(RAD2DEG(image->info->imageRotation))%360);

	if ((image->info->imageJustifyTypeA != GERBV_JUSTIFY_NOJUSTIFY)
	||  (image->info->imageJustifyTypeB != GERBV_JUSTIFY_NOJUSTIFY)) {
		fprintf(fd, "%%IJA");
		if (image->info->imageJustifyTypeA == GERBV_JUSTIFY_CENTERJUSTIFY)
			fprintf(fd, "C");
		else 
			fprintf(fd, "%.4f",image->info->imageJustifyOffsetA*tounits);
		fprintf(fd, "B");
		if (image->info->imageJustifyTypeB == GERBV_JUSTIFY_CENTERJUSTIFY)
			fprintf(fd, "C");
		else 
			fprintf(fd, "%.4f",image->info->imageJustifyOffsetB*tounits);
		fprintf(fd, "*%%\n");

	}
	/* handle scale user orientation transforms */
	if (fabs(thisTransform->scaleX - 1) > GERBV_PRECISION_LINEAR_INCH
	||  fabs(thisTransform->scaleY - 1) > GERBV_PRECISION_LINEAR_INCH) {
		fprintf(fd, "%%SFA%.4fB%.4f*%%\n",thisTransform->scaleX,thisTransform->scaleY);
	}
	/* handle mirror image user orientation transform */
	if ((thisTransform->mirrorAroundX)||(thisTransform->mirrorAroundY)) {
		fprintf(fd, "%%MIA%dB%d*%%\n",thisTransform->mirrorAroundY,thisTransform->mirrorAroundX);
	}
	
	/* define all apertures */
	fprintf(fd, "G04 --Define apertures--*\n");
	export_rs274x_write_apertures (fd, image, tounits, &xud);
	
	/* write rest of image */
	fprintf(fd, "G04 --Start main section--*\n");
	gint currentAperture = 0;
	gerbv_net_t *currentNet;
	
        xud.type = X2ATTR_OBJECT;
	oldLayer = image->layers;
	oldState = image->states;
	/* skip the first net, since it's always zero due to the way we parse things */
	for (currentNet = image->netlist->next; currentNet; currentNet = currentNet->next){
		/* check for "layer" changes (RS274X commands) */
		if (currentNet->layer != oldLayer)
			export_rs274x_write_layer_change (oldLayer, currentNet->layer, fd);
		
		/* check for new "netstate" (more RS274X commands) */
		if (currentNet->state != oldState)
			export_rs274x_write_state_change (oldState, currentNet->state, fd);
		
		/* check for tool changes */
		/* also, make sure the aperture number is a valid one, since sometimes
		   the loaded file may refer to invalid apertures */
		if ((currentNet->aperture != currentAperture)&&
			(image->aperture[currentNet->aperture] != NULL)) {
			fprintf(fd, "%sD%02d*\n",
			        x2 ? "" : "G54",        // G54 unnecessary in modern standards
			        currentNet->aperture);
			currentAperture = currentNet->aperture;
		}
		
		oldLayer = currentNet->layer;
		oldState = currentNet->state;

	        if (x2 && !insidePolygon)
	                x2attr_foreach_net_attr(currentNet, _dump_attribs, &xud);
		
		switch (currentNet->interpolation) {
			case GERBV_INTERPOLATION_LINEARx1 :
			case GERBV_INTERPOLATION_LINEARx10 :
			case GERBV_INTERPOLATION_LINEARx01 :
			case GERBV_INTERPOLATION_LINEARx001 :
				/* see if we need to write an "aperture off" line to get
				   the pen to the right start point */
				if ((!insidePolygon) && (currentNet->aperture_state == GERBV_APERTURE_STATE_ON))
					fprintf(fd, "G01%sD02*\n",_format_pair(&xud, currentNet->start_x, currentNet->start_y, FALSE));
				fprintf(fd, "G01%s",_format_pair(&xud, currentNet->stop_x, currentNet->stop_y, FALSE));
				/* and finally, write the esposure value */
				if (currentNet->aperture_state == GERBV_APERTURE_STATE_OFF)
					fprintf(fd, "D02*\n");
				else if (currentNet->aperture_state == GERBV_APERTURE_STATE_ON)
					fprintf(fd, "D01*\n");
				else
					fprintf(fd, "D03*\n");
				break;
			case GERBV_INTERPOLATION_CW_CIRCULAR :
			case GERBV_INTERPOLATION_CCW_CIRCULAR :
				/* see if we need to write an "aperture off" line to get
				   the pen to the right start point */
				if ((!insidePolygon) && (currentNet->aperture_state == GERBV_APERTURE_STATE_ON))
					fprintf(fd, "G01%sD02*\n",_format_pair(&xud, currentNet->start_x, currentNet->start_y, FALSE));
				
				/* always use multi-quadrant, since it's much easier to export */
				/*  and most all software should support it */
				fprintf(fd, "G75*\n");

				if (currentNet->interpolation == GERBV_INTERPOLATION_CW_CIRCULAR)
					fprintf(fd, "G02");	/* Clockwise */
				else
					fprintf(fd, "G03");	/* Counter clockwise */

				/* don't write the I and J values if the exposure is off */
				if (currentNet->aperture_state == GERBV_APERTURE_STATE_ON)
					fprintf(fd, "%s%s",
					        _format_pair(&xud, currentNet->stop_x, currentNet->stop_y, FALSE),
					        _format_pair(&xud, currentNet->cirseg->cp_x - currentNet->start_x, 
					                           currentNet->cirseg->cp_y - currentNet->start_y, TRUE));
				else
					fprintf(fd, "%s",_format_pair(&xud, currentNet->stop_x, currentNet->stop_y, FALSE));
				/* and finally, write the exposure value */
				if (currentNet->aperture_state == GERBV_APERTURE_STATE_OFF)
					fprintf(fd, "D02*\n");
				else if (currentNet->aperture_state == GERBV_APERTURE_STATE_ON)
					fprintf(fd, "D01*\n");
				else
					fprintf(fd, "D03*\n");
				break;
			case GERBV_INTERPOLATION_PAREA_START:
				fprintf(fd, "G36*\n");
				insidePolygon = TRUE;
				break;
			case GERBV_INTERPOLATION_PAREA_END:
				fprintf(fd, "G37*\n");
				insidePolygon = FALSE;
				break;
			default:
				break;
		}
	}
	
	fprintf(fd, "M02*\n");
	
	_x2attr_destroy_dict(xud.tracker);
	gerbv_destroy_image (image);
	fclose(fd);
	
	// return to the default locale
	setlocale(LC_NUMERIC, "");
	return TRUE;
        
}


gboolean
gerbv_export_rs274x_file_from_image (const gchar *filename, gerbv_image_t *inputImage,
		gerbv_user_transformation_t *transform)
{
        // Default export.  If there are any file attributes attached, assume X2, else X1.
        // attrs will only be non-null if there were any %TF*% defs in the input file,
        // or any were set programmatically.  (If only TA or TO defs, and no TF, then will
        // only export as X1 and no attributes will be output).
        // If X2, then export in recommended metric (up to 10m at nm resolution hence 46)
        // Otherwise, use inch with 36 (100 inch at 1uin), for backward compat.
        if (inputImage->attrs)
                return _export(2
                        , filename
		        , inputImage
		        , transform
		        , 6
		        , 10
		        , TRUE //mm
                        );
        else
                return _export(1
                        , filename
		        , inputImage
		        , transform
		        , 6
		        , 9
		        , FALSE //inch
                        );
}

gboolean
gerbv_export_rs274x2_file_from_image (const gchar *filename
		, gerbv_image_t *inputImage
		, gerbv_user_transformation_t *transform
		, int decimals
		, int digits
		, gboolean metric
                )
{
        // Explicit X2 export.
        return _export(2
                , filename
	        , inputImage
	        , transform
	        , decimals
	        , digits
	        , metric
                );
}


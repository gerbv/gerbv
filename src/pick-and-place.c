/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2000-2003 Stefan Petersen (spe@stacken.kth.se)
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


/** @file pick-and-place.c
    @contains the pick and place parser and renderer
 */ 

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <sys/stat.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <locale.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif /* HAVE_GETOPT_H */

#include <pango/pango.h>

#include <assert.h>

#include "gerber.h"
#include "drill.h"
#include "gerb_error.h"
#include "draw.h"
#include "color.h"
#include "gerbv_screen.h"
#include "log.h"
#include "setup.h"
#include "project.h"
#include "csv.h"
#include "pick-and-place.h"
/* CHECKME - here gi18n is disabled */
#define _(String) (String)

#define ST_START     1
#define ST_COLLECT   2
#define ST_TAILSPACE 3
#define ST_END_QUOTE 4
#define istspace iswspace

//! Parses a string representing float number with a unit, default is mm
/** @param char a string to be screened for unit
    @return a correctly converted double */
double pick_and_place_get_float_unit(char *str)
{
	double x = 0.0;
	char unit[41];
	/* float, optional space, optional unit mm,cm,in,mil */
	sscanf(str, "%lf %40s", &x, unit);
	if(strstr(unit,"in")) {
		x *= 25.4;
	} else if(strstr(unit,"mil")) {
		x *= 0.0254;
	} else if(strstr(unit, "cm")) {
		x *= 10.0;
	}
	return x;
}/*get_float_unit*/


/** search a string for a delimiter.
 Must occur at least n times. */
int pick_and_place_screen_for_delimiter(char *str, int n)
{
	char *ptr;
	char delimiter[4] = "|,;:";
	int counter[4];
	int idx, idx_max = 0;

	memset(counter, 0, sizeof(counter));
	for(ptr=str; *ptr; ptr++) {
		switch(*ptr) {
			case '|':
				idx = 0;
				break;
			case ',':
				idx = 1;
				break;
			case ';':
				idx = 2;
				break;
			case ':':
				idx = 3;
				break;
			default:
				continue;
				break;
		}
		counter[idx]++;
		if(counter[idx]>counter[idx_max]) {
		idx_max = idx;
		}
	}
	if (counter[idx_max] > n) {
		return (unsigned char) delimiter[idx_max];
	} else {
		return -1;
	}
}/*pnp_screen_for_delimiter*/


/**Parses the PNP data.
   two lists are filled with the row data.\n One for the scrollable list in the search and select parts interface, the other one a mere two columned list, which drives the autocompletion when entering a search.\n
   It also tries to determine the shape of a part and sets  pnp_state->shape accordingly which will be used when drawing the selections as an overlay on screen.
   @return the initial node of the pnp_state netlist
 */

GArray *
pick_and_place_parse_file(gerb_file_t *fd)
{
	PnpPartData      pnpPartData;
	int               line_counter = 0;
	int               ret;
	char             *row[12];
	//int               delimiter=-1;
	char              buf[MAXL+2], buf0[MAXL+2];
	double            tmp_x, tmp_y;
	gerb_transf_t    *tr_rot = gerb_transf_new();
	GArray 	*pnpParseDataArray = g_array_new (FALSE, FALSE, sizeof(PnpPartData));
	gboolean foundValidDataRow = FALSE;
    
	/* added by t.motylewski@bfad.de
	* many locales redefine "." as "," and so on, so sscanf has problems when
	* reading Pick and Place files using %f format */
	setlocale(LC_NUMERIC, "C" );

	while ( fgets(buf, MAXL, fd->fd) != NULL ) {
		int len = strlen(buf)-1;

		line_counter += 1;/*next line*/
		if(line_counter<2) {
			// TODO in principle column names could be read and interpreted
			continue; // skip the first line with names of columns
		}
		if(len >=0 && buf[len] == '\n') {
			buf[len--] = 0;
		}
		if(len >=0 && buf[len] == '\r') {
			buf[len--] = 0;
		}
		if (len <= 11 )  {  //lets check a minimum length of 11
			continue;
		}
		if ((len > 0) && (buf[0] == '%')) {
			continue;
		}
		// abort if we see a G04 code
		if ((len > 4) && (strncmp(buf,"G04 ",4)==0)) {
			g_array_free (pnpParseDataArray, TRUE);
			return NULL;  
		}
		/* this accepts file both with and without quotes */
		/* if (!pnp_state) { //we are in first line
		if ((delimiter = pnp_screen_for_delimiter(buf, 8)) < 0) {
		continue;
		}
		}   */ 
		ret = csv_row_parse(buf, MAXL,  buf0, MAXL, row, 11, ',',   CSV_QUOTES);
		if (ret > 0) {
			foundValidDataRow = TRUE;
		}
		//printf("direct:%s, %s, %s, %s, %s, %s, %s, %s, %s, %s,  %s, ret %d\n", row[0], row[1], row[2],row[3], row[4], row[5], row[6], row[7], row[8], row[9], row[10], ret);       

		if (row[0] && row[8]) { // here could be some better check for the syntax
			int i_length=0, i_width = 0;
			sprintf (pnpPartData.designator, "%.*s", sizeof(pnpPartData.designator)-1, row[0]);
			sprintf (pnpPartData.footprint, "%.*s", sizeof(pnpPartData.footprint)-1, row[1]);
			sprintf (pnpPartData.layer, "%.*s", sizeof(pnpPartData.layer)-1, row[8]);
			if (row[10] != NULL) {
				if ( ! g_utf8_validate(row[10], -1, NULL)) {
					gchar * str = g_convert(row[10], strlen(row[10]), "UTF-8", "ISO-8859-1",
					NULL, NULL, NULL);
					// I have not decided yet whether it is better to use always
					// "ISO-8859-1" or current locale.
					// str = g_locale_to_utf8(row[10], -1, NULL, NULL, NULL);
					sprintf (pnpPartData.comment, "%.*s", sizeof(pnpPartData.comment)-1, str);
					g_free(str);
				}
				else {
					sprintf (pnpPartData.comment, "%.*s", sizeof(pnpPartData.comment)-1, row[10]);
				}
			}
			/*
			gchar* g_convert(const gchar *str, gssize len, const gchar *to_codeset, const gchar *from_codeset, gsize *bytes_read, gsize *bytes_written, GError **error);
			*/
			pnpPartData.mid_x = pick_and_place_get_float_unit(row[2]);
			pnpPartData.mid_y = pick_and_place_get_float_unit(row[3]);
			pnpPartData.ref_x = pick_and_place_get_float_unit(row[4]);
			pnpPartData.ref_y = pick_and_place_get_float_unit(row[5]);
			pnpPartData.pad_x = pick_and_place_get_float_unit(row[6]);
			pnpPartData.pad_y = pick_and_place_get_float_unit(row[7]);
			sscanf(row[9], "%lf", &pnpPartData.rotation); // no units, always deg

			if(sscanf(pnpPartData.footprint, "%02d%02d", &i_length, &i_width) == 2) {
				// parse footprints like 0805 or 1206
				pnpPartData.length = 0.254 * i_length;
				pnpPartData.width = 0.254 * i_width;
				pnpPartData.shape = PART_SHAPE_RECTANGLE;
			} 
			else { 
				gerb_transf_reset(tr_rot);
				gerb_transf_rotate(tr_rot, -pnpPartData.rotation * M_PI/180);/* rotate it back to get dimensions */
				gerb_transf_apply( pnpPartData.pad_x -  pnpPartData.mid_x, 
				pnpPartData.pad_y - pnpPartData.mid_y, tr_rot, 
				&tmp_x, &tmp_y);
				if ((fabs(tmp_y) > fabs(tmp_x/100)) && (fabs(tmp_x) > fabs(tmp_y/100))){
					pnpPartData.length = 2 * tmp_x;/* get dimensions*/
					pnpPartData.width = 2 * tmp_y;
					pnpPartData.shape = PART_SHAPE_STD;
				} 
				else {
					pnpPartData.length = 0.0;
					pnpPartData.width = 0.0;
					pnpPartData.shape = PART_SHAPE_UNKNOWN;
				}
			}  
		}
		g_array_append_val (pnpParseDataArray, pnpPartData);
	}   
	gerb_transf_free(tr_rot);
	fd->ptr=0;
	rewind(fd->fd);

	if (!foundValidDataRow) {
		// this doesn't look like a valid PNP file, so return error
		g_array_free (pnpParseDataArray, TRUE);
		return NULL;    
	}     
    return pnpParseDataArray;
}


/*
 * Check if the file looks like a valid pick-and-place file
 * by checking for at least 4 commas
 * Returns 1 if it is, 0 if not.
 */
gboolean
pick_and_place_check_file_type (gerb_file_t *fd) {
	GArray *parsedPickAndPlaceData = pick_and_place_parse_file (fd);
	//g_warning ("ggg1 %d",parsedPickAndPlaceData);
	if (parsedPickAndPlaceData == NULL) {
		return FALSE;	
	}
	else {
		g_array_free (parsedPickAndPlaceData, TRUE);
		return TRUE;
	}
}

gerb_image_t *
pick_and_place_convert_pnp_data_to_image (GArray *parsedPickAndPlaceData) {
	gerb_image_t *image = NULL;
	gerb_net_t *curr_net = NULL;
	float radius;
	int i;
	gerb_transf_t *tr_rot = gerb_transf_new();

	image = new_gerb_image(image);
	if (image == NULL) {
		GERB_FATAL_ERROR("malloc image failed\n");
	}
	
	image->format = (gerb_format_t *)malloc(sizeof(gerb_format_t));
	if (image->format == NULL) {
		GERB_FATAL_ERROR("malloc format failed\n");
	}
	memset((void *)image->format, 0, sizeof(gerb_format_t));

	curr_net = image->netlist;
	image->info->min_x = -1;
	image->info->min_y = -1;
	image->info->max_x = 5;
	image->info->max_y = 5;
	image->info->scale_factor_A = 1.0;
	image->info->scale_factor_B = 1.0;
	image->info->offset_a = 0.0;
	image->info->offset_b = 0.0;
	image->info->step_and_repeat.X = 1.0;
	image->info->step_and_repeat.Y = 1.0;
	image->info->step_and_repeat.dist_X = 0.0;
	image->info->step_and_repeat.dist_Y = 0.0;

	image->aperture[0] = (gerb_aperture_t *)malloc(sizeof(gerb_aperture_t));
	memset((void *) image->aperture[0], 0, sizeof(gerb_aperture_t));
	image->aperture[0]->type = CIRCLE;
	image->aperture[0]->amacro = NULL;
	image->aperture[0]->parameter[0] = 0.4;
	image->aperture[0]->parameter[1] = 0.0;
	image->aperture[0]->parameter[2] = 0.0;
	image->aperture[0]->parameter[3] = 0.0;
	image->aperture[0]->parameter[4] = 0.0;
	image->aperture[0]->nuf_parameters = 1;
	image->aperture[0]->unit = MM;

	//g_warning ("length is %d",parsedPickAndPlaceData->len);
	for (i = 0; i < parsedPickAndPlaceData->len; i++) {
		PnpPartData partData = g_array_index(parsedPickAndPlaceData, PnpPartData, i);
                                  
		curr_net->next = (gerb_net_t *)malloc(sizeof(gerb_net_t));
		curr_net = curr_net->next;
		assert(curr_net);
		memset((void *)curr_net, 0, sizeof(gerb_net_t));
		partData.rotation *= M_PI/180; /* convert deg to rad */
		
		if ((partData.shape == PART_SHAPE_RECTANGLE) ||
			(partData.shape == PART_SHAPE_STD)) {
			// TODO: draw rectangle length x width taking into account rotation or pad x,y
			gerb_transf_reset(tr_rot);
			gerb_transf_rotate(tr_rot, partData.rotation);
			gerb_transf_shift(tr_rot, partData.mid_x, partData.mid_y);

			gerb_transf_apply(partData.length/2, partData.width/2, tr_rot, 
			      &curr_net->start_x, &curr_net->start_y);
			gerb_transf_apply(-partData.length/2, partData.width/2, tr_rot, 
			      &curr_net->stop_x, &curr_net->stop_y);

			curr_net->aperture = 0;
			curr_net->layer_polarity = POSITIVE;
			curr_net->unit = MM;
			curr_net->aperture_state = ON;
			curr_net->interpolation = LINEARx1;

			curr_net->next = (gerb_net_t *)malloc(sizeof(gerb_net_t));
			curr_net = curr_net->next;
			assert(curr_net);
			memset((void *)curr_net, 0, sizeof(gerb_net_t));

			gerb_transf_apply(-partData.length/2, partData.width/2, tr_rot, 
			      &curr_net->start_x, &curr_net->start_y);
			gerb_transf_apply(-partData.length/2, -partData.width/2, tr_rot, 
			      &curr_net->stop_x, &curr_net->stop_y);
			            
			curr_net->aperture = 0;
			curr_net->layer_polarity = POSITIVE;
			curr_net->unit = MM;
			curr_net->aperture_state = ON;
			curr_net->interpolation = LINEARx1;

			curr_net->next = (gerb_net_t *)malloc(sizeof(gerb_net_t));
			curr_net = curr_net->next;
			assert(curr_net);
			memset((void *)curr_net, 0, sizeof(gerb_net_t));

			gerb_transf_apply(-partData.length/2, -partData.width/2, tr_rot, 
			      &curr_net->start_x, &curr_net->start_y);
			gerb_transf_apply(partData.length/2, -partData.width/2, tr_rot, 
			      &curr_net->stop_x, &curr_net->stop_y);
			            
			curr_net->aperture = 0;
			curr_net->layer_polarity = POSITIVE;
			curr_net->unit = MM;
			curr_net->aperture_state = ON;
			curr_net->interpolation = LINEARx1;

			curr_net->next = (gerb_net_t *)malloc(sizeof(gerb_net_t));
			curr_net = curr_net->next;
			assert(curr_net);
			memset((void *)curr_net, 0, sizeof(gerb_net_t));

			gerb_transf_apply(partData.length/2, -partData.width/2, tr_rot, 
			      &curr_net->start_x, &curr_net->start_y);
			gerb_transf_apply(partData.length/2, partData.width/2, tr_rot, 
			      &curr_net->stop_x, &curr_net->stop_y);
			      
			curr_net->aperture = 0;
			curr_net->layer_polarity = POSITIVE;
			curr_net->unit = MM;
			curr_net->aperture_state = ON;
			curr_net->interpolation = LINEARx1;

			curr_net->next = (gerb_net_t *)malloc(sizeof(gerb_net_t));
			curr_net = curr_net->next;
			assert(curr_net);
			memset((void *)curr_net, 0, sizeof(gerb_net_t));
            
			if (partData.shape == PART_SHAPE_RECTANGLE) {
				gerb_transf_apply(partData.length/4, -partData.width/2, tr_rot, 
				      &curr_net->start_x, &curr_net->start_y);
				gerb_transf_apply(partData.length/4, partData.width/2, tr_rot, 
				      &curr_net->stop_x, &curr_net->stop_y);
			} else {
				gerb_transf_apply(partData.length/4, partData.width/2, tr_rot, 
				      &curr_net->start_x, &curr_net->start_y);
				gerb_transf_apply(partData.length/4, partData.width/4, tr_rot, 
				      &curr_net->stop_x, &curr_net->stop_y);
				      
				curr_net->aperture = 0;
				curr_net->layer_polarity = POSITIVE;
				curr_net->unit = MM;
				curr_net->aperture_state = ON;
				curr_net->interpolation = LINEARx1;

				curr_net->next = (gerb_net_t *)malloc(sizeof(gerb_net_t));
				curr_net = curr_net->next;
				assert(curr_net);
				memset((void *)curr_net, 0, sizeof(gerb_net_t));
				gerb_transf_apply(partData.length/2, partData.width/4, tr_rot, 
				      &curr_net->start_x, &curr_net->start_y);
				gerb_transf_apply(partData.length/4, partData.width/4, tr_rot, 
				      &curr_net->stop_x, &curr_net->stop_y);     
			}

	            curr_net->aperture = 0;
	            curr_net->layer_polarity = POSITIVE;
	            curr_net->unit = MM;
	            curr_net->aperture_state = ON;
	            curr_net->interpolation = LINEARx1;
            }
		else {
			curr_net->start_x = partData.mid_x;
			curr_net->start_y = partData.mid_y;
			curr_net->stop_x = partData.pad_x;
			curr_net->stop_y = partData.pad_y;

			curr_net->aperture = 0;
			curr_net->layer_polarity = POSITIVE;
			curr_net->unit = MM;
			curr_net->aperture_state = ON;
			curr_net->interpolation = LINEARx1;

			curr_net->next = (gerb_net_t *)malloc(sizeof(gerb_net_t));
			curr_net = curr_net->next;
			assert(curr_net);
			memset((void *)curr_net, 0, sizeof(gerb_net_t));


			curr_net->start_x = partData.mid_x;
			curr_net->start_y = partData.mid_y;
			curr_net->stop_x = partData.pad_x;
			curr_net->stop_y = partData.pad_y;

			curr_net->aperture = 0;
			curr_net->layer_polarity = POSITIVE;
			curr_net->unit = MM;
			curr_net->aperture_state = ON;
			curr_net->interpolation = CW_CIRCULAR;
			curr_net->cirseg = (gerb_cirseg_t *)malloc(sizeof(gerb_cirseg_t));
			memset((void *)curr_net->cirseg, 0, sizeof(gerb_cirseg_t));
			curr_net->cirseg->angle1 = 0.0;
			curr_net->cirseg->angle2 = 360.0;
			curr_net->cirseg->cp_x = partData.mid_x;
			curr_net->cirseg->cp_y = partData.mid_y;
			radius = sqrt((partData.pad_x-partData.mid_x)*(partData.pad_x-partData.mid_x) +
				 (partData.pad_y-partData.mid_y)*(partData.pad_y-partData.mid_y));
			curr_net->cirseg->width = 2*radius; /* fabs(pad_x-mid_x) */
			curr_net->cirseg->height = 2*radius;
		}
	}
	curr_net->next = NULL;
	    
	gerb_transf_free(tr_rot);
	g_array_free (parsedPickAndPlaceData, TRUE);
	return image;
}


/**will actually create the layer set in the dialog containing all selected parts.
  *this is called whenever enter is pressed, Mark button is clicked or right-click/double click was recognised on a single part.
  *parts are differentiated between by using the PART_SHAPE_ identifier set in\n 
  *parse_pnp
  *@see parse_pnp()
  */
gerb_image_t *
pick_and_place_parse_file_to_image(gerb_file_t *fd) {
	gerb_image_t *image;

	GArray *parsedPickAndPlaceData = pick_and_place_parse_file (fd);
	image = pick_and_place_convert_pnp_data_to_image (parsedPickAndPlaceData);
	return image;
}

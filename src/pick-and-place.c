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



/** Allocates a new pnp_state structure. 
   First of a netlist of type pnp_state_t */
pnp_state_t *
new_pnp_state()
{
    pnp_state_t *pnp_state;

    pnp_state = (pnp_state_t *)malloc(sizeof(pnp_state_t));
    if (pnp_state == NULL) {
	free(pnp_state);
	return NULL;
    }

    memset(pnp_state, 0, sizeof(pnp_state));
    
    return pnp_state;
} /* new_pnp_state */



//! Parses a string representing float number with a unit, default is mm
/** @param char a string to be screened for unit
    @return a correctly converted double */
double get_float_unit(char *str)
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
int pnp_screen_for_delimiter(char *str, int n)
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

pnp_state_t *parse_pnp(gerb_file_t *fd)
{
    pnp_state_t      *pnp_state = NULL;
    pnp_state_t      *pnp_state0 = NULL;
    int               line_counter = 0;
    int               ret;
    char             *row[12];
    int               delimiter=-1;
    char              buf[MAXL+2], buf0[MAXL+2];
    double            tmp_x, tmp_y;
    gerb_transf_t    *tr_rot = gerb_transf_new();
    
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
        if(len >=0 && buf[len] == '\n')
            buf[len--] = 0;
        if(len >=0 && buf[len] == '\r')
            buf[len--] = 0;
        if (len <= 11 )  {  //lets check a minimum length of 11
	    continue;
	}
	/* this accepts file both with and without quotes */
        if (!pnp_state) { //we are in first line
            if ((delimiter = pnp_screen_for_delimiter(buf, 8)) < 0) {
            continue;
            }
        }    
        ret = csv_row_parse(buf, MAXL,  buf0, MAXL, row, 11, delimiter,   CSV_QUOTES);
        // printf("direct:%s, %s, %s, %s, %s, %s, %s, %s, %s, %s,  %s, ret %d\n", row[0], row[1], row[2],row[3], row[4], row[5], row[6], row[7], row[8], row[9], row[10], ret);       
        
        if(pnp_state) {
            pnp_state = pnp_state->next = new_pnp_state();
        } else {
            // first element
            pnp_state0 = pnp_state = new_pnp_state();
        }
   
        if (row[0] && row[8]) { // here could be some better check for the syntax
	    int i_length=0, i_width = 0;
            memset  (pnp_state->designator, 0, sizeof(pnp_state->designator));
            sprintf (pnp_state->designator, "%.*s", sizeof(pnp_state->designator)-1, row[0]);
            memset  (pnp_state->footprint, 0, sizeof(pnp_state->footprint));
            sprintf (pnp_state->footprint, "%.*s", sizeof(pnp_state->footprint)-1, row[1]);
            memset  (pnp_state->layer, 0, sizeof(pnp_state->layer));
            sprintf (pnp_state->layer, "%.*s", sizeof(pnp_state->layer)-1, row[8]);
            memset  (pnp_state->comment, 0, sizeof(pnp_state->comment));
            if (row[10] != NULL) {
                memset  (pnp_state->comment, 0, sizeof(pnp_state->comment));
                if ( ! g_utf8_validate(row[10], -1, NULL)) {
		    gchar * str = g_convert(row[10], strlen(row[10]), "UTF-8", "ISO-8859-1",
                        NULL, NULL, NULL);
		    // I have not decided yet whether it is better to use always
		    // "ISO-8859-1" or current locale.
		    // str = g_locale_to_utf8(row[10], -1, NULL, NULL, NULL);
                    sprintf (pnp_state->comment, "%.*s", sizeof(pnp_state->comment)-1, str);
		    g_free(str);
		} else 
                    sprintf (pnp_state->comment, "%.*s", sizeof(pnp_state->comment)-1, row[10]);
            }
    /*
    gchar* g_convert(const gchar *str, gssize len, const gchar *to_codeset, const gchar *from_codeset, gsize *bytes_read, gsize *bytes_written, GError **error);
    */
            pnp_state->mid_x = get_float_unit(row[2]);
	        pnp_state->mid_y = get_float_unit(row[3]);
	        pnp_state->ref_x = get_float_unit(row[4]);
	        pnp_state->ref_y = get_float_unit(row[5]);
	        pnp_state->pad_x = get_float_unit(row[6]);
	        pnp_state->pad_y = get_float_unit(row[7]);
	        sscanf(row[9], "%lf", &pnp_state->rotation); // no units, always deg
            
	        if(sscanf(pnp_state->footprint, "%02d%02d", &i_length, &i_width) == 2) {
		        // parse footprints like 0805 or 1206
		        pnp_state->length = 0.254 * i_length;
                pnp_state->width = 0.254 * i_width;
		        pnp_state->shape = PART_SHAPE_RECTANGLE;
	        } else { 
                gerb_transf_reset(tr_rot);
                gerb_transf_rotate(tr_rot, -pnp_state->rotation * M_PI/180);/* rotate it back to get dimensions */
                gerb_transf_apply( pnp_state->pad_x -  pnp_state->mid_x, 
                     pnp_state->pad_y - pnp_state->mid_y, tr_rot, 
                     &tmp_x, &tmp_y);
                if ((fabs(tmp_y) > fabs(tmp_x/100)) && (fabs(tmp_x) > fabs(tmp_y/100))){
                    pnp_state->length = 2 * tmp_x;/* get dimensions*/
                    pnp_state->width = 2 * tmp_y;
                    pnp_state->shape = PART_SHAPE_STD;
                    
                } else {
                    pnp_state->length = 0.0;
                    pnp_state->width = 0.0;
                    pnp_state->shape = PART_SHAPE_UNKNOWN;
                }
            }    

        }    
    }   
    gerb_transf_free(tr_rot);       
    return pnp_state0; /* return the FIRST list node */
}

/**Frees a pnp_state_t netlist completely.*/
void 
free_pnp_state(pnp_state_t *pnp_state)
{
    pnp_state_t *pnp_next;
   
    while (pnp_state != NULL) {
	pnp_next = pnp_state->next;
        free(pnp_state);
        pnp_state = pnp_next;
    }
	
    return;
} /* free_pnp_state */


/*
 * Check if the file looks like a valid pick-and-place file
 * by checking for at least 4 commas
 * Returns 1 if it is, 0 if not.
 */
gboolean
pick_and_place_check_file_type(gerb_file_t *fd) {
	pnp_state_t *parsedPickAndPlaceData = parse_pnp (fd);

	if (parsedPickAndPlaceData == NULL) {
		return FALSE;	
	}
	else {
		free_pnp_state (parsedPickAndPlaceData);
		return TRUE;
	}
}


/**will actually create the layer set in the dialog containing all selected parts.
  *this is called whenever enter is pressed, Mark button is clicked or right-click/double click was recognised on a single part.
  *parts are differentiated between by using the PART_SHAPE_ identifier set in\n 
  *parse_pnp
  *@see parse_pnp()
  */
gerb_image_t *
pick_and_place_parse_file(gerb_file_t *fd) {
	gerb_image_t *image = NULL;
	gerb_net_t *curr_net = NULL;
	double x_scale = 1, y_scale = 1;

	image = new_gerb_image(image);
	if (image == NULL) {
		GERB_FATAL_ERROR("malloc image failed\n");
	}
	curr_net = image->netlist;

	if (image && image->format ){
		x_scale = pow(10.0, (double)image->format->x_dec);
		y_scale = pow(10.0, (double)image->format->y_dec);
	}
	//pnp_state_t  *parsed_PNP_data = parse_pnp(fd);                       
	return image;
}

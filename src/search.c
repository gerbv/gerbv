/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2004 Juergen Haas (juergenhaas@gmx.net)
 *
 * $Id$
 *	                     by Juergen Haas (juergenhaas@gmx.net)
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
#include <config.h>
#endif

#ifdef USE_GTK2
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <errno.h>
#include <locale.h>


#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "search_file.h"
#include "gerb_error.h"
#include "search.h"
#include "search_gui.h"
#include "csv.h"



/*
 * Allocates a new pnp_state structure
 */
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



/** parse a string representing float number with a unit, default is mm */
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


/*
 * Parses the PNP data
 */

pnp_state_t *parse_pnp(pnp_file_t *fd)
{
    pnp_state_t      *pnp_state = NULL;
    pnp_state_t      *pnp_state0 = NULL;
    int               line_counter = 0;
    int               ret;
    char             *row[12];
    int               delimiter=-1;
    char              buf[MAXL+2], buf0[MAXL+2];
    
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
		        pnp_state->length = 0.0254 * i_length;
                pnp_state->width = 0.0254 * i_width;
		        pnp_state->shape = PART_SHAPE_RECTANGLE;
	        } else {
		        pnp_state->length = 0.0;
		        pnp_state->width = 0.0;
		        pnp_state->shape = PART_SHAPE_UNKNOWN;
            }

            gtk_list_store_append (GTK_LIST_STORE(fd->model), &interface.iter); 
            gtk_list_store_set (GTK_LIST_STORE(fd->model), &interface.iter,
                COLUMN_DESIGNATOR, pnp_state->designator, 
	        COLUMN_footprint, pnp_state->footprint,
	        COLUMN_mid_x, pnp_state->mid_x,
	        COLUMN_mid_y, pnp_state->mid_y ,
	        COLUMN_ref_x, pnp_state->ref_x,
	        COLUMN_ref_y, pnp_state->ref_y,
	        COLUMN_pad_x, pnp_state->pad_x,
	        COLUMN_pad_y, pnp_state->pad_y,
            COLUMN_LAYER, pnp_state->layer,
            COLUMN_rotation, pnp_state->rotation,
		    COLUMN_length, pnp_state->length,
		    COLUMN_width, pnp_state->width,
		    COLUMN_shape, pnp_state->shape,
            COLUMN_COMMENT, pnp_state->comment,
	        COLUMN_NO_FILES_FOUND, FALSE,
	        -1);
                        
            gtk_list_store_append (GTK_LIST_STORE(completion_model), &interface.iter); 
            gtk_list_store_set (GTK_LIST_STORE(completion_model), &interface.iter,
	        0, pnp_state->designator, 
	        1, pnp_state->comment,
                -1);
 
        //     pnp_state->next = new_pnp_state();
        //     pnp_state = pnp_state->next;
        }    
    }   
           
    return pnp_state0; /* return the FIRST list node */
}


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

#endif

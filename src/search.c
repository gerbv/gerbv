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



static void
eat_line_pnp(pnp_file_t *fd)
{
    int read = fgetc(fd->fd);
    
    while(read != 10 && read != 13) {
	if (read == EOF) return;
	read = fgetc(fd->fd);
    }
} /* eat_line_pnp */


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


char pnp_screen_for_delimiter(char str[MAXL+2])
{
    char *rest_of_line, tmp_buf[MAXL+2];
    char delimiter[4] = "|,;:";
    int counter, idx0;   
    
    
    memset  (tmp_buf, 0, sizeof(tmp_buf));
    sprintf (tmp_buf, "%.*s", sizeof(tmp_buf)-1, str);
    for (idx0 = 0; idx0 <4; idx0++) {
        counter = 0;
        printf("idx0 %i\n", idx0);
        if (strchr(tmp_buf, delimiter[idx0]) != NULL) {

            rest_of_line = strrchr(tmp_buf, delimiter[idx0]);       
            while (rest_of_line  != NULL) {
                if (rest_of_line != 0)		/* If STRCHR returns an address for '|' then */
                    *rest_of_line = '\0';		/* stick a null character at that address and so shorten the original line. */
                counter++;    
                rest_of_line = strrchr(tmp_buf, delimiter[idx0]);
            }
            if (counter > 5) {
                printf("delimiter is: %c", delimiter[idx0]);
                return delimiter[idx0];
            }
   
        printf("it is in there:%p", strrchr(tmp_buf, delimiter[idx0]));    
        }        
    }
   
    return '?';/* does not seem to be a csv-style file*/

}/*pnp_screen_for_delimiter*/


/*
 * Parses the PNP data
 */

pnp_state_t *parse_pnp(pnp_file_t *fd)
{
    pnp_state_t      *pnp_state = NULL;
    int               line_counter = 0;
    int               ret;
  //  GtkTreeSelection *tmp_sel;
    char             *row[12], delimiter;
    char              buf[MAXL+2], buf0[MAXL+2];
    
    /* added by t.motylewski@bfad.de
     * many locales redefine "." as "," and so on, so sscanf has problems when
     * reading Pick and Place files using %f format */

    setlocale(LC_NUMERIC, "C" );
     
    pnp_state = new_pnp_state();
    parsed_PNP_data = pnp_state;/*Global storage, for data reload, e.g. if search_window was destroyed*/

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
        if ((delimiter = pnp_screen_for_delimiter(buf)) == '?') {
            continue;
        }
        ret = csv_row_parse(buf, MAXL,  buf0, MAXL, row, 11, delimiter,   CSV_QUOTES);
        printf("direct:%s, %s, %s, %s, %s, %s, %s, %s, %s, %s,  %s, ret %d\n", row[0], row[1], row[2],row[3], row[4], row[5], row[6], row[7], row[8], row[9], row[10], ret);       
   
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
	        1, g_locale_to_utf8(pnp_state->comment, -1, NULL, NULL, NULL), -1);
 
             pnp_state->next = new_pnp_state();
             pnp_state = pnp_state->next;
        }    
    }   

 
   
#if 0 
#ifdef HAVE_SYS_MMAN_H
    char buf[MAXL+1];
    
    while (fgets(buf, MAXL, fd->fd) != NULL) {
    if(buf[strlen(buf)-1] == '\n')
        buf[strlen(buf)-1] = 0;
    if(buf[strlen(buf)-1] == '\r')
        buf[strlen(buf)-1] = 0;
   
    ret = sscanf(buf, "%100[^,],%100[^,],%lfmm,%lfmm,%lfmm,%lfmm,%lfmm,%lfmm,%100[^,],%lf,%100[^\n]",
        pnp_state->designator, pnp_state->footprint, &pnp_state->mid_x, &pnp_state->mid_y, &pnp_state->ref_x, &pnp_state->ref_y, &pnp_state->pad_x, &pnp_state->pad_y,
        pnp_state->layer, &pnp_state->rotation, pnp_state->comment);
    /*fprintf(stderr,"layer:%s: ", pnp_state->layer);
    fprintf(stderr,"mid_x:%f: ",pnp_state-> mid_x);
    fprintf(stderr,"comment:%s:", pnp_state->comment);*/
    
    if(ret<11) {
        GERB_MESSAGE("wrong line format(%d): %s\n", ret, buf);
        continue;
    } 
    eat_line_pnp(fd);
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
                        COLUMN_COMMENT, g_locale_to_utf8(pnp_state->comment, -1, NULL, NULL, NULL),
			COLUMN_NO_FILES_FOUND, FALSE,
			-1);
                        
     gtk_list_store_append (GTK_LIST_STORE(completion_model), &interface.iter); 
     gtk_list_store_set (GTK_LIST_STORE(completion_model), &interface.iter,
			0, pnp_state->designator, 
		        1, g_locale_to_utf8(pnp_state->comment, -1, NULL, NULL, NULL), -1);
 
    line_counter += 1;/*next line*/
    pnp_state->next = new_pnp_state();
    pnp_state = pnp_state->next;
    }   
#else
#if defined (__MINGW32__)
    char buf[MAXL];
 
  while (fgets(buf, MAXL, fd->fd) != NULL) {

    if(buf[strlen(buf)-1] == '\n')
        buf[strlen(buf)-1] = 0;
    if(buf[strlen(buf)-1] == '\r')
        buf[strlen(buf)-1] = 0;
   
    ret = sscanf(buf, "%100[^,],%100[^,],%lfmm,%lfmm,%lfmm,%lfmm,%lfmm,%lfmm,%100[^,],%lf,%100[^X]",
        pnp_state->designator, pnp_state->footprint, &pnp_state->mid_x, &pnp_state->mid_y, &pnp_state->ref_x, &pnp_state->ref_y, &pnp_state->pad_x, &pnp_state->pad_y,
        pnp_state->layer, &pnp_state->rotation, pnp_state->comment);
    /*fprintf(stderr,"layer:%s: ", pnp_state->layer);
    fprintf(stderr,"mid_x:%f: ",pnp_state-> mid_x);
    fprintf(stderr,"comment:%s:", pnp_state->comment);*/
    
    if(ret<11) {
        GERB_MESSAGE("wrong line format: %s\n", buf);
        continue;
    }
    
    eat_line_pnp(fd);
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
                        COLUMN_COMMENT, g_locale_to_utf8(pnp_state->comment, -1, NULL, NULL, NULL),
			COLUMN_NO_FILES_FOUND, FALSE,
			-1);
                        
     gtk_list_store_append (GTK_LIST_STORE(completion_model), &interface.iter); 
     gtk_list_store_set (GTK_LIST_STORE(completion_model), &interface.iter,
			0, pnp_state->designator, 
		        1, g_locale_to_utf8(pnp_state->comment, -1, NULL, NULL, NULL), -1);

    
    line_counter += 1;/*next line*/
    pnp_state->next = new_pnp_state();
    pnp_state = pnp_state->next;
    }             
#endif
#endif
#endif /*#if 0*/
    pnp_state->next = NULL;
    /* create list of all entries as a Glist for convenient use later
     * (uparrow movements in assembly mode) 
    tmp_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(interface.tree));
    gtk_tree_selection_select_all (tmp_sel);
    interface.PNP_entries_list = gtk_tree_selection_get_selected_rows
                                            (GTK_TREE_SELECTION(tmp_sel),
                                             (GtkTreeModel **)&interface.model);
    gtk_tree_selection_unselect_all (tmp_sel); */                                            
    return pnp_state;
}


void 
free_pnp_state(pnp_state_t *pnp_state)
{
    pnp_state_t *pnp1, *pnp2;
   
    
    pnp1 = pnp_state;
    while (pnp1 != NULL) {
//	free(pnp1->designator);
//	pnp1->designator = NULL;
	
	pnp2 = pnp1;
	pnp1 = pnp1->next;
	free(pnp2);
	pnp2 = NULL;
    }
	
    return;
} /* free_pnp_state */

#endif

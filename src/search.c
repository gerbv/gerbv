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


#include <gtk/gtk.h>
#include <gdk/gdk.h>


#include "search_file.h"
#include "gerb_error.h"
#include "search.h"
#include "search_gui.h"
#include "csv.h"
//#define  G_STR_DELIMITERS       "_-|> <.:;,`\n`"


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
    int read = pnp_fgetc(fd);
    
    while(read != 10 && read != 13) {
	if (read == EOF) return;
	read = pnp_fgetc(fd);
    }
} /* eat_line_pnp */


/*
 * Parses the PNP data
 */

pnp_state_t *parse_pnp(pnp_file_t *fd)
{
    pnp_state_t      *pnp_state = NULL;
    int               line_counter = 1;
    int               ret;
    GtkTreeSelection *tmp_sel;
   // char        *row[10];
  //  char        buf[1024];
    
     
    pnp_state = new_pnp_state();
    parsed_PNP_data = pnp_state;/*Global storage, for data reload, e.g. if search_window was destroyed*/
   /* while  (ret = csv_row_fread(fopen("/home/juergen/MINGW/gerbv/PickPlaceforaim02.easy.csv", "r"),  buf, 1024, row, 11, ',', CSV_QUOTES)  > 0 ) {
    printf("%s, %s, %s,%s, %s, %s,%s, %s, %s,%s, %s, \n", row[0], row[1], row[2],row[3], row[4], row[5], row[6], row[7], row[8], row[9], row[10]);       
   }
   while  ((ret = csv_row_fread(fopen("etc/passwd", "r"),  buf, 1024, row, 10, ':', CSV_TRIM))  > 0 ) {
    printf("ret:%i >%s %s \n", ret, row[0], row[2]);       
   }
   
   while  ((ret = csv_row_parse(fd->data, 1024,  buf, 1024, row, 1024, ',', CSV_QUOTES))  > 0 ) {
    printf("direct:%s, %s, %s,%s, %s, %s,%s, %s, %s,%s, %s, \n", row[0], row[1], row[2],row[3], row[4], row[5], row[6], row[7], row[8], row[9], row[10]);       
   }*/
  
#ifdef HAVE_SYS_MMAN_H
    char buf[MAXL+1];
    
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
    } else {
         GERB_MESSAGE("\n");
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
    } else {
         GERB_MESSAGE("\n");
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

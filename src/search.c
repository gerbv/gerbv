/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2000-2003 Stefan Petersen (spe@stacken.kth.se)
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
#include "search.h"
#include "search_gui.h"


/*
 * Allocates a new pnp_state structure
 */
static pnp_state_t *
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
    pnp_state_t *pnp_state, *ip = NULL;
    int primitive = 0;
    char *end;
    int line_counter = 1;
    char buf[MAXL+1];
    int ret;
    
     
    pnp_state = new_pnp_state();

    /*
     * We have to skip the first line
     */
    /*if (line_counter == 1) {
    eat_line_pnp(fd);	
     line_counter += 1;
    }*/
   

    while (fgets(buf, MAXL, fd->fd) != NULL) {
    
    if(buf[strlen(buf)-1] == '\n')
        buf[strlen(buf)-1] = 0;
    if(buf[strlen(buf)-1] == '\r')
        buf[strlen(buf)-1] = 0;
   
    ret = sscanf(buf, "%100[^,],%100[^,],%lfmm,%lfmm,%lfmm,%lfmm,%lfmm,%lfmm,%100[^,],%lf,%100[^X]",
        pnp_state->designator, pnp_state->footprint, &pnp_state->mid_x, &pnp_state->mid_y, &pnp_state->ref_x, &pnp_state->ref_y, &pnp_state->pad_x, &pnp_state->pad_y,
        pnp_state->layer, &pnp_state->rotation, pnp_state->comment);
/*    fprintf(stderr,"layer:%s: ", layer);
    fprintf(stderr,"mid_x:%f: ", mid_x);
    fprintf(stderr,"comment:%s:", comment);*/
    if(ret<11) {
        fprintf(stderr, "wrong line: %s\n", buf);
        continue;
    } else {
        fprintf(stderr,"\n");
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
                            COLUMN_COMMENT, pnp_state->comment,
			    COLUMN_NO_FILES_FOUND, FALSE,
			    -1);

/*                           
 * COLUMN_COMMENT, g_locale_from_utf8(pnp_state->comment, -1, NULL, NULL, NULL),
*/
    
    line_counter += 1;/*next line*/
    pnp_state->next = new_pnp_state();
    pnp_state = pnp_state->next;
    }
    return pnp_state;
}


void 
free_pnp_state(pnp_state_t *pnp_state)
{
 /*   pnp_state_t *pnp1, *pnp2;FIX ME
   
    
    pnp1 = pnp_state;
    while (pnp1 != NULL) {
	free(pnp1->designator);
	pnp1->designator = NULL;
	
	pnp2 = pnp1;
	pnp1 = pnp1->next;
	free(pnp2);
	pnp2 = NULL;
    }
*/	
    return;
} /* free_pnp_state */


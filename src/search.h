/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2004 Juergen Haas (juergenhaas@gmx.net)
 *
 * $Id$
 *  
 *                                      Juergen H. (juergenhaas@gmx.net) 
 *                                      and Tomasz M. (T.Motylewski@bfad.de)
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

#ifndef SEARCH_H
#define SEARCH_H

#ifdef USE_GTK2
#include <stdio.h>
#include "search_file.h"
#include "gerbv_screen.h"
#include "search_cb.h"

/* maximum size of string*/
#define MAXL 200

enum e_footprint {
    PART_SHAPE_UNKNOWN = 0, /// drawn as circle with line
    PART_SHAPE_RECTANGLE = 1 /// rectangle with one side marked
};

typedef struct pnp_state {
    
    char     designator[MAXL];
    char     footprint[MAXL];
    double   mid_x;
    double   mid_y;
    double   ref_x;
    double   ref_y;
    double   pad_x;
    double   pad_y;
    char     layer[MAXL]; /*T is top B is bottom*/
    double   rotation;
    char     comment[MAXL];    
    int      shape;
    double   width;
    double   length;
  

    gerbv_unit_t unit;/*unused at the moment*/

    unsigned int nuf_push;  /* Nuf pushes to estimate stack size */
    struct pnp_state *next;
    } pnp_state_t;
    
extern pnp_state_t *parsed_PNP_data;
    
    
    
/*CHECK ME: introduce pnp_state.state and pnp_state.model
    typedef struct pnp_state {
    
        typedef struct state {
        char     designator[MAXL];
        char     footprint[MAXL];
        double   mid_x;
        double   mid_y;
        double   ref_x;
        double   ref_y;
        double   pad_x;
        double   pad_y;
        char     layer[MAXL]; T is top B is bottom
        double   rotation;
        char     comment[MAXL];    

        gerbv_unit_t unit;unused at the moment

        unsigned int nuf_push;  Nuf pushes to estimate stack size 
        struct pnp_state *next;
        } state;
    GtkListStore *model;
    } pnp_state_t;
*/

pnp_state_t *new_pnp_state();
double get_float_unit(char *str);
int pnp_screen_for_delimiter(char *str, int n);
pnp_state_t *parse_pnp(pnp_file_t *fd);
void free_pnp_state(pnp_state_t *pnp_state);

#endif /* USE_GTK2 */
#endif /* SEARCH_H */

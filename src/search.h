/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2000-2002 Stefan Petersen (spe@stacken.kth.se)
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

#include <stdio.h>
#include "search_file.h"
#include "gerbv_screen.h"

/// maximum size of string
#define MAXL 200

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

    gerbv_unit_t unit;/*unused at the moment*/

    unsigned int nuf_push;  /* Nuf pushes to estimate stack size */
    struct pnp_state *next;
} pnp_state_t;


pnp_state_t *parse_pnp(pnp_file_t *fd);
void free_pnp_state(pnp_state_t *pnp_state);


#endif /* SEARCH_H */

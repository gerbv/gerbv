/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2004 Tomasz Motylewski (T.Motylewski@bfad.de)
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
 
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */


#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "gerb_transf.h"

void gerb_transf_free(gerb_transf_t* transf)
{
    free(transf);
    
}/*gerb_transf_t*/


gerb_transf_t* gerb_transf_new(void)
{
    gerb_transf_t *transf;
    
    transf = malloc(sizeof(gerb_transf_t));
    gerb_transf_reset(transf);
    return transf;
    

} /*gerb_transf_new*/

void gerb_transf_reset(gerb_transf_t* transf)
{
    memset(transf,0,sizeof(gerb_transf_t));
    
    transf->r_mat[0][0] = transf->r_mat[1][1] = 1.0; /*off-diagonals 0 diagonals 1 */
    transf->r_mat[1][0] = transf->r_mat[0][1] = 0.0;
    transf->scale = 1.0;
    transf->offset[0] = transf->offset[1] = 0.0;
    
} /*gerb_transf_reset*/

void gerb_transf_apply(double x, double y, gerb_transf_t* transf, double *out_x, double *out_y)
{

//    x += transf->offset[0];
//    y += transf->offset[1];
    *out_x = (x * transf->r_mat[0][0] + y * transf->r_mat[0][1]) * transf->scale;
    *out_y = (x * transf->r_mat[1][0] + y * transf->r_mat[1][1]) * transf->scale;
    *out_x += transf->offset[0];
    *out_y += transf->offset[1];
    
    
}/*gerb_transf_apply*/

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
 
/*! @file gerb_transf.c
    @brief handles all translation and rotation operations for drawing on screen. */ 
 
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <glib.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "gerb_transf.h"

void gerb_transf_free(gerbv_transf_t* transf)
{
    g_free(transf);
    
}/*gerbv_transf_t*/


gerbv_transf_t* gerb_transf_new(void)
{
    gerbv_transf_t *transf;
    
    transf = g_malloc(sizeof(gerbv_transf_t));
    gerb_transf_reset(transf);
    return transf;
    

} /*gerb_transf_new*/

void gerb_transf_reset(gerbv_transf_t* transf)
{
    memset(transf,0,sizeof(gerbv_transf_t));
    
    transf->r_mat[0][0] = transf->r_mat[1][1] = 1.0; /*off-diagonals 0 diagonals 1 */
    transf->r_mat[1][0] = transf->r_mat[0][1] = 0.0;
    transf->scale = 1.0;
    transf->offset[0] = transf->offset[1] = 0.0;
    
} /*gerb_transf_reset*/

//!Rotation
/*! append rotation to transformation.
@param transf transformation to be modified
@param angle in rad (counterclockwise rotation) */

void gerb_transf_rotate(gerbv_transf_t* transf, double angle)
{
    double m[2][2]; 
    double s = sin(angle), c = cos(angle);
      
    memcpy(m, transf->r_mat, sizeof(m));    
    transf->r_mat[0][0] = c * m[0][0] - s * m[1][0];
    transf->r_mat[0][1] = c * m[0][1] - s * m[1][1];
    transf->r_mat[1][0] = s * m[0][0] + c * m[1][0];
    transf->r_mat[1][1] = s * m[0][1] + c * m[1][1];
//    transf->offset[0] = transf->offset[1] = 0.0; CHECK ME
    
} /*gerb_transf_rotate*/

//!Translation
/*! append translation to transformation.
@param transf transformation to be modified
@param shift_x translation in x direction
@param shift_y translation in y direction */

void gerb_transf_shift(gerbv_transf_t* transf, double shift_x, double shift_y)
{
      
    transf->offset[0] += shift_x;
    transf->offset[1] += shift_y;
            
} /*gerb_transf_shift*/

void gerb_transf_apply(double x, double y, gerbv_transf_t* transf, double *out_x, double *out_y)
{

//    x += transf->offset[0];
//    y += transf->offset[1];
    *out_x = (x * transf->r_mat[0][0] + y * transf->r_mat[0][1]) * transf->scale;
    *out_y = (x * transf->r_mat[1][0] + y * transf->r_mat[1][1]) * transf->scale;
    *out_x += transf->offset[0];
    *out_y += transf->offset[1];
    
    
}/*gerb_transf_apply*/

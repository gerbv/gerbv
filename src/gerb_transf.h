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
 
 
#ifndef GERB_TRANSF_H
#define GERB_TRANSF_H


/** Contains the transformation matrix, the scale and the offset on screen.
   linear transformation applies first shift by offset [0]=x, [1]=y,
   then multiplication by r_mat and scale */
typedef struct gerbv_transf {
    double r_mat[2][2];
    double scale;
    double offset[2];
} gerbv_transf_t;


gerbv_transf_t* gerb_transf_new(void);

void gerb_transf_free(gerbv_transf_t* transf);

void gerb_transf_reset(gerbv_transf_t* transf);

void gerb_transf_rotate(gerbv_transf_t* transf, double angle);

void gerb_transf_shift(gerbv_transf_t* transf, double shift_x, double shift_y);
   
void gerb_transf_apply(double x, double y, gerbv_transf_t* transf, double *out_x, double *out_y);

#endif




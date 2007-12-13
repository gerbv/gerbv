/*
 * gEDA - GNU Electronic Design Automation
 * gerb_stats.h -- a part of gerbv.
 *
 *   Copyright (C) 2007 Stuart Brorson (sdb@cloud9.net)
 *
 * $Id$
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

#ifndef GERB_STATS_H
#define GERB_STATS_H


typedef struct {
    int G0;
    int G1;
    int G2;
    int G3;
    int G4;
    int G10;
    int G11;
    int G12;
    int G36;
    int G37;
    int G54;
    int G55;
    int G70;
    int G71;
    int G74;
    int G75;
    int G90;
    int G91;
    int G_unknown;

    int D1;
    int D2;
    int D3;
/*    GHashTable *D_user_defined; */
    int D_unknown;
    int D_error;

    int M0;
    int M1;
    int M2;
    int M_unknown;

    int X;
    int Y;
    int I;
    int J;

    /* Must include % RS-274 codes */
    int star;
    int unknown;


} gerb_stats_t;

typedef struct {
    int comment;
    int F;

    int G00;
    int G01;
    int G02;
    int G03;
    int G05;
    int G90;
    int G91;
    int G93;
    int G_unknown;

    int M00;
    int M01;
    int M18;
    int M25;
    int M30;
    int M31;
    int M45;
    int M47;
    int M48;
    int M71;
    int M72;
    int M95;
    int M97;
    int M98;
    int M_unknown;

    int unknown;

} drill_stats_t;

gerb_stats_t * gerb_stats_new(void);
void gerb_stats_add_layer(gerb_stats_t *accum_stats, 
			  gerb_stats_t *input_stats);

drill_stats_t * drill_stats_new(void);
void drill_stats_add_layer(drill_stats_t *accum_stats, 
			   drill_stats_t *input_stats);

#endif /* GERB_STATS_H */

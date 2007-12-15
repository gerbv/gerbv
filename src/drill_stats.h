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

#ifndef DRILL_STATS_H
#define DRILL_STATS_H

#include <gdk/gdk.h>  /* This imports gboolean type */


typedef struct drill_list {
    int drill_num;
    double drill_size;
    char *drill_unit;
    int drill_count;
    struct drill_list *next;
} drill_list_t;

typedef struct {
    int layer_count;

    drill_list_t *drill_list;
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


/* ===================  Prototypes ================ */
drill_stats_t * drill_stats_new(void);
void drill_stats_add_layer(drill_stats_t *accum_stats, 
			   drill_stats_t *input_stats);
gboolean drill_stats_in_drill_list(drill_list_t *drill_list, int drill_num);
drill_list_t *drill_stats_new_drill_list(void);
void drill_stats_add_to_drill_list(drill_list_t *drill_list_in,
				   int drill_num_in, double drill_size_in,
				   char *drill_unit_in);
void drill_stats_increment_drill_counter(drill_list_t *drill_list_in,
					 int drill_num_in);
void drill_stats_add_to_drill_counter(drill_list_t *drill_list_in,
				      int drill_num_in,
				      int increment);


#endif /* DRILL_STATS_H */

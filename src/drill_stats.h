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

/** \file drill_stats.h
    \brief Header info to the statistics generating functions for Excellon drill files
    \ingroup libgerbv
*/

#ifndef DRILL_STATS_H
#define DRILL_STATS_H

#include <gdk/gdk.h>      /* This imports gboolean type */


/* ===================  Prototypes ================ */

gboolean drill_stats_in_drill_list(gerbv_drill_list_t *drill_list, int drill_num);
gerbv_drill_list_t *gerbv_drill_stats_new_drill_list(void);
void drill_stats_add_to_drill_list(gerbv_drill_list_t *drill_list_in,
				   int drill_num_in, double drill_size_in,
				   char *drill_unit_in);
void drill_stats_modify_drill_list(gerbv_drill_list_t *drill_list_in,
				   int drill_num_in, double drill_size_in,
				   char *drill_unit_in);
void drill_stats_increment_drill_counter(gerbv_drill_list_t *drill_list_in,
					 int drill_num_in);
void drill_stats_add_to_drill_counter(gerbv_drill_list_t *drill_list_in,
				      int drill_num_in,
				      int increment);
gerbv_error_list_t *gerbv_drill_stats_new_error_list(void);
void drill_stats_add_error(gerbv_error_list_t *error_list_in,
			   int layer, const char *error_text, 
			   gerbv_message_type_t type);

#endif /* DRILL_STATS_H */

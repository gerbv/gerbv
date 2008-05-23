/*
 * gEDA - GNU Electronic Design Automation
 * gerbv_stats.h -- a part of gerbv.
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

/** \file gerb_stats.h
    \brief Header info for the statistics generating functions for RS274X files
    \ingroup libgerbv
*/

#ifndef gerb_stats_H
#define gerb_stats_H



/* ===================  Prototypes ================ */
gerbv_error_list_t *gerbv_stats_new_error_list(void);
void gerbv_stats_add_error(gerbv_error_list_t *error_list_in,
                           int layer, const char *error_text,
                           gerbv_message_type_t type);

gerbv_aperture_list_t *gerbv_stats_new_aperture_list(void);
void gerbv_stats_add_aperture(gerbv_aperture_list_t *aperture_list_in,
			     int layer, int number, gerbv_aperture_type_t type,
			     double parameter[5]);
void gerbv_stats_add_to_D_list(gerbv_aperture_list_t *D_list_in,
			      int number);
int gerbv_stats_increment_D_list_count(gerbv_aperture_list_t *D_list_in,
				       int number, 
				       int count,
				       gerbv_error_list_t *error); 

#endif /* gerb_stats_H */

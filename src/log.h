/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2000-2002 Stefan Petersen (spe@stacken.kth.se)
 *
 * $Id$
 *
 * Contributed by Dino Ghilardi <dino.ghilardi@ieee.org> 
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

#ifndef LOG_H
#define LOG_H

#include <gtk/gtk.h>

void
gerbv_console_log_handler(const gchar *log_domain,
			  GLogLevelFlags log_level,
			  const gchar *message, 
			  gpointer user_data);


void
gerbv_gtk_log_handler(const gchar *log_domain,
		      GLogLevelFlags log_level,
		      const gchar *message, gpointer user_data);



#endif /* LOG_H */

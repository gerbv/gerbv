/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2004 Juergen Haas (juergenhaas@gmx.net)
 *
 * $Id$
 * 
 *                                      Juergen H. (juergenhaas@gmx.net) and
 *                                      Tomasz M. (T.Motylewski@bfad.de)
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
 
 
#ifndef _SEARCH_CB_H
#define _SEARCH_CB_H
#ifdef USE_GTK2
#include "search.h"
#include "search_file.h"


void load_pnp_file_popup(GtkWidget *widget, gpointer data);
int open_pnp(char *filename, int idx, int reload);
#endif
#endif /* USE_GTK2 */

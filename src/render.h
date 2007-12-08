/*
 * gEDA - GNU Electronic Design Automation
 *
 * render.h -- this file is a part of gerbv.
 *
 * $Id$
 *
 *   Copyright (C) 2007 Stuart Brorson (SDB@cloud9.net)
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

#include "gerber.h"

/* Keep redraw state when preempting to process certain events */
struct gerbv_redraw_state_t {
    int valid;			/* Set to nonzero when data is valid */
    int file_index;
    GdkPixmap *curr_pixmap;
    GdkPixmap *clipmask;
    int max_width;
    int max_height;
    int files_loaded;
};

gerb_stats_t *generate_gerber_analysis(void);


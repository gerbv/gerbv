/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2000-2002 Stefan Petersen (spe@stacken.kth.se)
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

#ifndef DRAW_GDK_H
#define DRAW_GDK_H

#include <gdk/gdk.h>
#include "gerber.h"
#include "gerb_image.h"

/* Default mouse cursor. Perhaps redefine this to a variable later? */
#define GERBV_DEF_CURSOR	NULL

/*
 * Convert a gerber image to a GDK clip mask to be used when creating pixmap
 */
int image2pixmap(GdkPixmap **pixmap, gerb_image_t *image, 
		 double scale, double trans_x, double trans_y,
		 enum polarity_t polarity);

#endif /* DRAW_GDK_H */


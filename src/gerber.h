/*
 * gEDA - GNU Electronic Design Automation
 * This files is a part of gerbv.
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

#ifndef GERBER_H
#define GERBER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <glib.h>

#include "gerb_file.h"

/*
 * parse gerber file pointed to by fd
 */
gerbv_image_t *parse_gerb(gerb_file_t *fd, gchar *directoryPath);
gboolean gerber_is_rs274x_p(gerb_file_t *fd, gboolean *returnFoundBinary);
gboolean gerber_is_rs274d_p(gerb_file_t *fd);
gerbv_net_t *
gerber_create_new_net (gerbv_net_t *currentNet, gerbv_layer_t *layer, gerbv_netstate_t *state);

gboolean
gerber_create_new_aperture (gerbv_image_t *image, int *indexNumber,
		enum aperture_t apertureType, gdouble parameter1, gdouble parameter2);
#ifdef __cplusplus
}
#endif

#endif /* GERBER_H */

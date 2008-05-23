/*
 * gEDA - GNU Electronic Design Automation
 * drill.h
 * Copyright (C) 2000-2001 Stefan Petersen (spe@stacken.kth.se)
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

/** \file drill.h
    \brief Header infor for the Excellon drill parsing functions
    \ingroup libgerbv
*/

#ifndef DRILL_H
#define DRILL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <glib.h>
#include "gerb_image.h"
#include "gerb_file.h"

#define TOOL_MIN 1  /* T00 code is reserved for unload tool command */
#define TOOL_MAX 9999

gerbv_image_t *parse_drillfile(gerb_file_t *fd, gerbv_HID_Attribute *attr_list, 
			      int n_attr, int reload);
gboolean
drill_file_p(gerb_file_t *fd, gboolean *returnFoundBinary);

#ifdef __cplusplus
}
#endif

#endif /* DRILL_H */

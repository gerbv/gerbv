/*
 * gEDA - GNU Electronic Design Automation
 * This files is a part of gerbv.
 *
 *   Copyright (C) 2000-2001 Stefan Petersen (spe@stacken.kth.se)
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
#include "gerb_image.h"
#include "gerb_file.h"


#define EAGLECAD_KLUDGE 

/*
 * parse gerber file pointed to by fd
 */
gerb_image_t *parse_gerb(gerb_file_t *fd);

/*
 * Check that the parsed gerber image is complete.
 * Returned errorcodes are:
 * 0: No problems
 * 1: Missing netlist
 * 2: Missing format
 * 4: Missing apertures
 * 8: Missing info
 * It could be any of above or'ed together
 */
enum gerb_verify_error { MISSING_NETLIST = 1,
			 MISSING_FORMAT = 2, 
			 MISSING_APERTURES = 4,
			 MISSING_INFO = 8 };

enum gerb_verify_error verify_gerb(gerb_image_t *image);


#ifdef __cplusplus
}
#endif

#endif /* GERBER_H */

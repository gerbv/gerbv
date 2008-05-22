/*
 *                            COPYRIGHT
 *
 * gEDA - GNU Electronic Design Automation
 * This is a part of gerbv
 *
 *  Copyright (C) 2008 Dan McMahill
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 * it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

/** \file attribute.h
    \brief Dynamic GUI window creation header info
*/

#ifndef ATTRIBUTE_H
#define ATTRIBUTE_H

#ifdef __cplusplus
extern "C" {
#endif

/* 
 * The following typedef's are taken directly from src/hid.h in the
 * pcb project.  The names are kept the same to make it easier to
 * compare to pcb's sources.
 */
    
int
attribute_interface_dialog (HID_Attribute *, int, HID_Attr_Val *, 
			    const char *,const char *);

void
attribute_merge (HID_Attribute *, int, HID_Attribute *, int);

#ifdef __cplusplus
}
#endif

#endif /*  ATTRIBUTE_H */

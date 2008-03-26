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
    
    
/* Used for HID attributes (exporting and printing, mostly).
   HA_boolean uses int_value, HA_enum sets int_value to the index and
   str_value to the enumeration string.  HID_Label just shows the
   default str_value.  HID_Mixed is a real_value followed by an enum,
   like 0.5in or 100mm. 
*/
typedef struct
{
    int int_value;
    char *str_value;
    double real_value;
} HID_Attr_Val;

typedef struct
{
    char *name;
    char *help_text;
    enum
	{ HID_Label, HID_Integer, HID_Real, HID_String,
	  HID_Boolean, HID_Enum, HID_Mixed, HID_Path
	} type;
    int min_val, max_val;	/* for integer and real */
    HID_Attr_Val default_val;	/* Also actual value for global attributes.  */
    const char **enumerations;
    /* If set, this is used for global attributes (i.e. those set
       statically with REGISTER_ATTRIBUTES below) instead of changing
       the default_val.  Note that a HID_Mixed attribute must specify a
       pointer to HID_Attr_Val here, and HID_Boolean assumes this is
       "char *" so the value should be initialized to zero, and may be
       set to non-zero (not always one).  */
    void *value;
    int hash; /* for detecting changes. */
} HID_Attribute;


int
interface_attribute_dialog (HID_Attribute *, int, HID_Attr_Val *, 
			    const char *,const char *);

void
attribute_merge (HID_Attribute *, int, HID_Attribute *, int);

#ifdef __cplusplus
}
#endif

#endif /*  ATTRIBUTE_H */

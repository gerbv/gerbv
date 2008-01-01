/*
 * gEDA - GNU Electronic Design Automation
 * This files is a part of gerbv.
 *
 *   Copyright (C) 2007-2008 Stuart Brorson (sdb@cloud9.net)
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

#ifndef GERB_APERTURE_H
#define GERB_APERTURE_H
	
#define APERTURE_MIN 10
#define APERTURE_MAX 9999

#define APERTURE_PARAMETERS_MAX 10

#include "amacro.h"

enum aperture_t {CIRCLE, RECTANGLE, OVAL, POLYGON, MACRO};
enum aperture_state_t {OFF, ON, FLASH};
enum unit_t {INCH, MM, UNIT_UNSPECIFIED};

typedef struct gerb_aperture {
    enum aperture_t type;
    amacro_t *amacro;
    double parameter[APERTURE_PARAMETERS_MAX];
    int nuf_parameters;
    enum unit_t unit;
} gerb_aperture_t;


#endif

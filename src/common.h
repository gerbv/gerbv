/*
 * $Id$
 *
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2007 Dan McMahill
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
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

/** \file common.h
    \brief Contains basic defines for debug messages
    \ingroup libgerbv
*/

#ifndef __COMMON_H__
#define __COMMON_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef __GNUC__
#define __FUNCTION1(a,b) a ":" #b
#define __FUNCTION2(a,b) __FUNCTION1(a,b)
#define __FUNCTION__ __FUNCTION2(__FILE__,__LINE__)
#endif

#endif /* __COMMON_H__ */


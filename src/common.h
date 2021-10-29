/*
 * gEDA - GNU Electronic Design Automation
 * common.h -- This file is a part of gerbv
 *
 * Copyright (C) 2007 Dan McMahill
 * Copyright (C) 2007-2020 sourceforge contributors
 * Copyright (C) 2020-2021 github contributors
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

/** \file common.h
    \brief Contains basic defines
    \ingroup libgerbv
*/

#ifndef GERBV_COMMON_H
#define GERBV_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef __GNUC__
#define __FUNCTION1(a,b) a ":" #b
#define __FUNCTION2(a,b) __FUNCTION1(a,b)
#define __FUNCTION__ __FUNCTION2(__FILE__,__LINE__)
#endif

#include "locale.h"
#include "gettext.h"
#define _(str) gettext(str)
#ifdef ENABLE_NLS
# ifdef gettext_noop
#  define N_(str) gettext_noop(str)
# else
#  define N_(str) (str)
# endif
#else
# define N_(str) (str)
#endif

#ifdef __cplusplus
}
#endif

#endif /* GERBV_COMMON_H */

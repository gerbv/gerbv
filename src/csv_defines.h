/*
 * gEDA - GNU Electronic Design Automation
 * csv_defines.h -- This file is a part of gerbv
 *
 * Copyright (C) 2000-2002 Stefan Petersen (spe@stacken.kth.se)
 * Copyright (C) 2002-2020 sourceforge contributors
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

/** \file csv_defines.h
    \brief Sets up internal definitions for handling csv-style files
    \ingroup libgerbv
*/

#ifndef GERBV_DEFINES_H
#define GERBV_DEFINES_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__sparc__)

#define HAVE_ENCDEC 0
#define HAVE_STRDUP 1
#define HAVE_STRNLEN 0
#define HAVE_EXPAT 0
#define HAVE_MBSTATE 0
#define HAVE_WCWIDTH 1
#define HAVE_SNPRINTF 1
#define HAVE_VSNPRINTF 1
#define HAVE_VARMACRO 1
#define HAVE_LANGINFO 1

#elif defined(_WIN32)

#define HAVE_ENCDEC 0
#define HAVE_STRDUP 1

#ifndef HAVE_STRNLEN
# define HAVE_STRNLEN 0
#endif

#define HAVE_EXPAT 0
#define HAVE_MBSTATE 0
#define HAVE_WCWIDTH 0

#ifndef HAVE_SNPRINTF
# define HAVE_SNPRINTF 0
#endif

#define HAVE_VSNPRINTF 0
#define HAVE_VARMACRO 0
#define HAVE_LANGINFO 0

#else

#define HAVE_ENCDEC 0
#define HAVE_STRDUP 1
#define HAVE_STRNLEN 1
#define HAVE_EXPAT 0
#define HAVE_MBSTATE 1
#define HAVE_WCWIDTH 1
#define HAVE_SNPRINTF 1
#define HAVE_VSNPRINTF 0
#define HAVE_VARMACRO 1
#define HAVE_LANGINFO 1

#endif

#ifdef __GNUC__
#define UNUSED __attribute__ ((unused))
#else
#define UNUSED
#endif

#ifdef __cplusplus
}
#endif

#endif /* GERBV_DEFINES_H */

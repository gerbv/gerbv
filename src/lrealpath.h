/*
 * gEDA - GNU Electronic Design Automation
 * lrealpath.h -- This file is a part of gerbv
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

/** \file lrealpath.h
    \brief Header info for 
    \ingroup gerbv
 */ 

#ifndef GERBV_LREALPATH_H
#define GERBV_LREALPATH_H

#ifdef __cplusplus
extern "C" {
#endif

/* A well-defined realpath () that is always compiled in.  */
char *lrealpath (const char *);

#ifdef __cplusplus
}
#endif

#endif /* GERBV_LREALPATH_H */

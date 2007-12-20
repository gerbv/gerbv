/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2000-2003 Stefan Petersen (spe@stacken.kth.se)
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

#ifndef GERB_ERROR_H
#define GERB_ERROR_H
#include <glib.h>

enum error_type_t {FATAL, ERROR, WARNING, NOTE};

typedef struct error_list_t {
    int layer;
    char *error_text;
    enum error_type_t type;
    struct error_list_t *next;
} error_list_t;


#define GERB_FATAL_ERROR(t...) g_log(NULL, G_LOG_LEVEL_ERROR, ##t);
#define GERB_COMPILE_ERROR(t...)  g_log(NULL, G_LOG_LEVEL_CRITICAL, ##t);
#define GERB_COMPILE_WARNING(t...)  g_log(NULL, G_LOG_LEVEL_WARNING, ##t);
#define GERB_MESSAGE(t...)  g_log(NULL, G_LOG_LEVEL_MESSAGE, ##t);

#endif /* GERB_ERROR_H */

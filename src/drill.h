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

#ifndef DRILL_H
#define DRILL_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef err
#define err(errcode, a...) \
     do { \
           fprintf(stderr, ##a); \
           exit(errcode);\
     } while (0)
#endif

#define TOOL_MIN 1
#define TOOL_MAX 999

/* I couldn't possibly code without these */
#undef TRUE
#define TRUE 1
#undef FALSE
#define FALSE 0

#undef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#undef min
#define min(a,b) ((a) < (b) ? (a) : (b))

#include "gerber.h"

enum drill_m_code_t {DRILL_M_UNKNOWN, DRILL_M_NOT_IMPLEMENTED, DRILL_M_END,
		     DRILL_M_ENDOFPATTERN, DRILL_M_HEADER, DRILL_M_METRIC, 
		     DRILL_M_IMPERIAL, DRILL_M_FILENAME};

typedef struct drill_state {
    double curr_x;
    double curr_y;
    int current_tool;
} drill_state_t;

gerb_image_t *parse_drillfile(FILE *fd);

#ifdef __cplusplus
}
#endif

#endif /* DRILL_H */

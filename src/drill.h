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
#define TOOL_TO_APERTURE_OFFSET (APERTURE_MIN - TOOL_MIN + 1)

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

enum tool_state_t {TOOL_NOT_DEFINED, TOOL_DEFINED};
enum drill_file_type_t {UNKNOWN, EXCELLON};
enum drill_m_code_t {DRILL_M_UNKNOWN, DRILL_M_NOT_IMPLEMENTED, DRILL_M_END,
		     DRILL_M_ENDOFPATTERN, DRILL_M_HEADER, DRILL_M_METRIC, 
		     DRILL_M_IMPERIAL, DRILL_M_FILENAME};

typedef struct drill_hole {
    double x;
    double y;
    int tool_num;
    struct drill_hole *next;
} drill_hole_t;

typedef struct drill_tool {
/*    int tool_num; */
    double diameter;
    enum unit_t unit;
/*    enum tool_state_t state; */
/*    struct drill_tool *next; */
} drill_tool_t;

typedef struct drill_format {
    enum unit_t unit;
    enum drill_file_type_t file_type;
    enum omit_zeros_t omit_zeros;
    enum coordinate_t coordinate;
    int x_int;
    int x_dec;
    int y_int;
    int y_dec;
} drill_format_t;

typedef struct drill_image_info {
    double min_x;
    double min_y;
    double max_x;
    double max_y;
    double offset_a;
    double offset_b;
} drill_image_info_t;

typedef struct drill_state {
    double curr_x;
    double curr_y;
    int current_tool;
} drill_state_t;

typedef struct drill_image {
    drill_image_info_t *info;
    drill_format_t *format;
    drill_tool_t *tools[TOOL_MAX - TOOL_MIN];
    drill_hole_t *holes;
} drill_image_t;

gerb_image_t *parse_drillfile(FILE *fd);
void free_drill_image(drill_image_t *image);

#ifdef __cplusplus
}
#endif

#endif /* DRILL_H */

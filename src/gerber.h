/*
 * gEDA - GNU Electronic Design Automation
 * This files is a part of gerbv.
 *
 *   Copyright (C) 2000-2001 Stefan Petersen (spe@stacken.kth.se)
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

#ifndef GERBER_H
#define GERBER_H

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
	
	
#define APERTURE_MIN 10
#define APERTURE_MAX 999
	
enum aperture_state {OFF, ON, FLASH};
enum aperture_t {CIRCLE, RECTANGLE, OVAL, POLYGON, MACRO};
enum unit_t {INCH, MM};
enum polarity_t {POSITIVE, NEGATIVE};
enum omit_zeros_t {LEADING, TRAILING};
enum coordinate_t {ABSOLUTE, INCREMENTAL};
enum interpolation_t {LINEARx1, LINEARx10, LINEARx01, LINEARx001, 
		      CW_CIRCULAR, CCW_CIRCULAR, 
		      MQ_CW_CIRCULAR, MQ_CCW_CIRCULAR};

struct gerb_net {
    double start_x;
    double start_y;
    double stop_x;
    double stop_y;
    double arc_start_x;
    double arc_start_y;
    int aperture;
    enum aperture_state aperture_state;
    enum interpolation_t interpolation;
    struct gerb_net *next;
};


struct gerb_aperture {
    enum aperture_t type;
    double parameter[5];
    int nuf_parameters;
};


struct gerb_format {
    enum omit_zeros_t omit_zeros;
    enum coordinate_t coordinate;
    int x_int;
    int x_dec;
    int y_int;
    int y_dec;
};
	
	
struct gerb_image_info {
    enum unit_t unit;
    enum polarity_t polarity;
    double min_x;
    double min_y;
    double max_x;
    double max_y;
    double offset_a;
    double offset_b;
};

#ifdef LAYER
struct gerb_layer {
    struct gerb_net *netlist;
    struct gerb_layer *next;
};
#endif

struct gerb_image {
    struct gerb_aperture *aperture[APERTURE_MAX - APERTURE_MIN];
    struct gerb_format *format;
    struct gerb_image_info *info;
#ifdef LAYER
    int nuf_layers;
    struct gerb_layer *layer;
#else
    struct gerb_net *netlist; /* XXX til I get energy to implement layers */
#endif
};


struct gerb_state {
    int curr_x;
    int curr_y;
    int prev_x;
    int prev_y;
    int arc_start_x;
    int arc_start_y;
    int curr_aperture;
    enum aperture_state aperture_state;
    enum interpolation_t interpolation;
};
    
struct gerb_image *parse_gerb(FILE *fd);
    
void free_gerb_image(struct gerb_image *image);

#ifdef __cplusplus
}
#endif

#endif /* GERBER_H */

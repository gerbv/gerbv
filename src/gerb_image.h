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

#ifndef GERB_IMAGE_H
#define GERB_IMAGE_H

#include "attribute.h"
#include "gerb_aperture.h"
#include "gerb_transf.h"
#include "gerb_stats.h"
#include "drill_stats.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <glib.h> /* To pick up gpointer */
 
enum polarity_t {POSITIVE, NEGATIVE, DARK, CLEAR};
enum omit_zeros_t {LEADING, TRAILING, EXPLICIT, ZEROS_UNSPECIFIED};
enum coordinate_t {ABSOLUTE, INCREMENTAL};
enum interpolation_t {LINEARx1, LINEARx10, LINEARx01, LINEARx001, 
		      CW_CIRCULAR, CCW_CIRCULAR, PAREA_START, PAREA_END, DELETED};
enum encoding_t {NONE, ASCII, EBCDIC, BCD, ISO_ASCII, EIA };
enum layertype_t {GERBER, DRILL, PICK_AND_PLACE};
enum knockout_t {NOKNOCKOUT, FIXEDKNOCK, BORDER};
enum mirror_state_t {NOMIRROR, FLIPA, FLIPB, FLIPAB};
enum axis_select_t {NOSELECT, SWAPAB};
enum image_justify_type_t {NOJUSTIFY, LOWERLEFT, CENTERJUSTIFY};

enum selection_t {EMPTY, POINT_CLICK, DRAG_BOX};

typedef struct {
	gpointer image;
	gpointer net;
} gerb_selection_item_t;

typedef struct {
	enum selection_t type;
	gdouble lowerLeftX;
	gdouble lowerLeftY;
	gdouble upperRightX;
	gdouble upperRightY;
	GArray *selectedNodeArray;
} gerb_selection_info_t;

typedef struct {
    gdouble translateX;
    gdouble translateY;
    gdouble scaleX;
    gdouble scaleY;
    gboolean inverted;
} gerb_user_transformations_t;

typedef struct gerb_cirseg {
    double cp_x;
    double cp_y;
    double width;  /* of oval */
    double height; /* of oval */
    double angle1;
    double angle2;
} gerb_cirseg_t;

typedef struct gerb_step_and_repeat { /* SR parameters */
    int X;
    int Y;
    double dist_X;
    double dist_Y;
} gerb_step_and_repeat_t;
 
typedef struct {
    gboolean firstInstance;
    enum knockout_t type;
    enum polarity_t polarity;
    gdouble lowerLeftX;
    gdouble lowerLeftY;
    gdouble width;
    gdouble height;
    gdouble border;
} gerb_knockout_t;
 
typedef struct {
    gerb_step_and_repeat_t stepAndRepeat;
    gerb_knockout_t knockout;
    gdouble rotation;
    enum polarity_t polarity; 
    gchar *name;
    gpointer next;
} gerb_layer_t;

typedef struct {
    enum axis_select_t axisSelect;
    enum mirror_state_t mirrorState;
    enum unit_t unit;
    gdouble offsetA;
    gdouble offsetB;
    gdouble scaleA;
    gdouble scaleB;
    gpointer next;
} gerb_netstate_t;

typedef struct gerb_net {
    double start_x;
    double start_y;
    double stop_x;
    double stop_y;
    int aperture;
    enum aperture_state_t aperture_state;
    enum interpolation_t interpolation;
    struct gerb_cirseg *cirseg;
    struct gerb_net *next;
    GString *label;
    gerb_layer_t *layer;
    gerb_netstate_t *state;
} gerb_net_t;

typedef struct gerb_format {
    enum omit_zeros_t omit_zeros;
    enum coordinate_t coordinate;
    int x_int;
    int x_dec;
    int y_int;
    int y_dec;
    int lim_seqno; /* Length limit for codes of sequence number */
    int lim_gf;    /* Length limit for codes of general function */
    int lim_pf;    /* Length limit for codes of plot function */
    int lim_mf;    /* Length limit for codes of miscellaneous function */
} gerb_format_t;
	
	
typedef struct gerb_image_info {
    char *name;
    enum polarity_t polarity;
    double min_x; /* Always in inches */
    double min_y;
    double max_x;
    double max_y;
    double offsetA;
    double offsetB;
    enum encoding_t encoding;
    double imageRotation;
    enum image_justify_type_t imageJustifyTypeA;
    enum image_justify_type_t imageJustifyTypeB;
    gdouble imageJustifyOffsetA;
    gdouble imageJustifyOffsetB;
    gdouble imageJustifyOffsetActualA;
    gdouble imageJustifyOffsetActualB;
    gchar *plotterFilm;

    /* Descriptive string for the type of file (rs274-x, drill, etc)
     * that this is
     */
    gchar *type;

    /* Attribute list that is used to hold all sorts of information
     * about how the layer is to be parsed.
    */ 
    HID_Attribute *attr_list;
    int n_attr;
} gerb_image_info_t;

typedef struct {
    enum layertype_t layertype;
    gerb_aperture_t *aperture[APERTURE_MAX];
    gerb_layer_t *layers;
    gerb_netstate_t *states;
    amacro_t *amacro;
    gerb_format_t *format;
    gerb_image_info_t *info;
    gerb_net_t *netlist;
    gerb_transf_t *transf;
    gerb_stats_t *gerb_stats;
    drill_stats_t *drill_stats;
} gerb_image_t;


/*
 * Function prototypes
 */
gerb_image_t *new_gerb_image(gerb_image_t *image, const gchar *type);
void free_gerb_image(gerb_image_t *image);

/*
 * Check that the parsed gerber image is complete.
 * Returned errorcodes are:
 * 0: No problems
 * 1: Missing netlist
 * 2: Missing format
 * 4: Missing apertures
 * 8: Missing info
 * It could be any of above or'ed together
 */
typedef enum { 
    GERB_IMAGE_OK = 0,
    GERB_IMAGE_MISSING_NETLIST = 1,
    GERB_IMAGE_MISSING_FORMAT = 2, 
    GERB_IMAGE_MISSING_APERTURES = 4,
    GERB_IMAGE_MISSING_INFO = 8,
} gerb_verify_error_t;

gerb_verify_error_t gerb_image_verify(gerb_image_t const* image);

/* Dumps a written version of image to stdout */
void gerb_image_dump(gerb_image_t const* image);

gerb_layer_t *
gerb_image_return_new_layer (gerb_layer_t *previousLayer);

gerb_netstate_t *
gerb_image_return_new_netstate (gerb_netstate_t *previousState);

void
gerb_image_copy_image (gerb_image_t *sourceImage, gerb_user_transformations_t *transform, gerb_image_t *destinationImage);

gerb_image_t *
gerb_image_duplicate_image (gerb_image_t *sourceImage, gerb_user_transformations_t *transform);

void
gerb_image_delete_selected_nets (gerb_image_t *sourceImage, GArray *selectedNodeArray);

gboolean
gerb_image_reduce_area_of_selected_objects (GArray *selectionArray, gdouble areaReduction, gint paneRows,
		gint paneColumns);

gboolean
gerb_image_move_selected_objects (GArray *selectionArray, gdouble translationX,
		gdouble translationY);

#ifdef __cplusplus
}
#endif

#endif /* GERB_IMAGE_H */

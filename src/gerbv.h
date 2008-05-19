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

/** \file gerbv.h
    \brief This is the main header file for the libgerbv library
*/

//! \example example1.c
//! \example example2.c
//! \example example3.c
//! \example example4.c
//! \example example5.c

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

/* Used for HID attributes (exporting and printing, mostly).
   HA_boolean uses int_value, HA_enum sets int_value to the index and
   str_value to the enumeration string.  HID_Label just shows the
   default str_value.  HID_Mixed is a real_value followed by an enum,
   like 0.5in or 100mm. 
*/
typedef struct
{
    int int_value;
    char *str_value;
    double real_value;
} HID_Attr_Val;

typedef struct
{
    char *name;
    char *help_text;
    enum
	{ HID_Label, HID_Integer, HID_Real, HID_String,
	  HID_Boolean, HID_Enum, HID_Mixed, HID_Path
	} type;
    int min_val, max_val;	/* for integer and real */
    HID_Attr_Val default_val;	/* Also actual value for global attributes.  */
    const char **enumerations;
    /* If set, this is used for global attributes (i.e. those set
       statically with REGISTER_ATTRIBUTES below) instead of changing
       the default_val.  Note that a HID_Mixed attribute must specify a
       pointer to HID_Attr_Val here, and HID_Boolean assumes this is
       "char *" so the value should be initialized to zero, and may be
       set to non-zero (not always one).  */
    void *value;
    int hash; /* for detecting changes. */
} HID_Attribute;

/* from old gerb_error */
enum error_type_t {FATAL, GRB_ERROR, WARNING, NOTE};

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

/* from old amacro.h */
enum opcodes {NOP,      /* No Operation */
	      PUSH,     /* Ordinary stack operations. Uses float */
	      PPUSH,    /* Parameter onto stack, 1 is first parameter and 
			    so on (compare gerber $1, $2 and so on */ 
	      PPOP,     /* Data on stack popped onto parameter register.
			   First parameter popped from stack is which register
			   to store data and second parameter popped is actual
			   data. */
	      ADD, SUB, /* Mathematical operations */
	      MUL, DIV, 
	      PRIM};    /* Defines what primitive to draw. Inparameters
			   should be on the stack. */


typedef struct instruction {
    enum opcodes opcode;
    union {
	int ival;
	float fval;
    } data;
    struct instruction *next;
} instruction_t;

typedef struct amacro {
    char *name;
    instruction_t *program;
    unsigned int nuf_push;  /* Nuf pushes in program to estimate stack size */
    struct amacro *next;
} amacro_t;


/* from old gerb_aperture.h header file */
#define APERTURE_MIN 10
#define APERTURE_MAX 9999

/*
 * Maximum number of aperture parameters is set by the outline aperture
 * macro. There (p. 28) is defined up to 50 points in polygon.
 * So 50 points with x and y plus two for holding extra data gives...
 */
#define APERTURE_PARAMETERS_MAX 102

enum aperture_t {APERTURE_NONE, CIRCLE, RECTANGLE, OVAL, POLYGON, MACRO, 
		 MACRO_CIRCLE, MACRO_OUTLINE, MACRO_POLYGON, MACRO_MOIRE, 
		 MACRO_THERMAL, MACRO_LINE20, MACRO_LINE21, MACRO_LINE22};
enum aperture_state_t {OFF, ON, FLASH};
enum unit_t {INCH, MM, UNIT_UNSPECIFIED};

typedef struct gerbv_simplified_amacro {
    enum aperture_t type;
    double parameter[APERTURE_PARAMETERS_MAX];
    struct gerbv_simplified_amacro *next;
} gerbv_simplified_amacro_t;

typedef struct gerbv_aperture {
    enum aperture_t type;
    amacro_t *amacro;
    gerbv_simplified_amacro_t *simplified;
    double parameter[APERTURE_PARAMETERS_MAX];
    int nuf_parameters;
    enum unit_t unit;
} gerbv_aperture_t;

/* from old gerbv_stats.h */
/* the gerb_aperture_list is used to keep track of 
 * apertures used in stats reporting */
typedef struct gerbv_aperture_list_t {
    int number;
    int layer;
    int count;
    enum aperture_t type;
    double parameter[5];
    struct gerbv_aperture_list_t *next;
} gerbv_aperture_list_t;


typedef struct {
    struct error_list_t *error_list;
    struct gerbv_aperture_list_t *aperture_list;
    struct gerbv_aperture_list_t *D_code_list;

    int layer_count;
    int G0;
    int G1;
    int G2;
    int G3;
    int G4;
    int G10;
    int G11;
    int G12;
    int G36;
    int G37;
    int G54;
    int G55;
    int G70;
    int G71;
    int G74;
    int G75;
    int G90;
    int G91;
    int G_unknown;

    int D1;
    int D2;
    int D3;
/*    GHashTable *D_user_defined; */
    int D_unknown;
    int D_error;

    int M0;
    int M1;
    int M2;
    int M_unknown;

    int X;
    int Y;
    int I;
    int J;

    /* Must include % RS-274 codes */
    int star;
    int unknown;

} gerbv_stats_t;

/* from old drill_stats.h */
typedef struct drill_list {
    int drill_num;
    double drill_size;
    char *drill_unit;
    int drill_count;
    struct drill_list *next;
} drill_list_t;

typedef struct {
    int layer_count;
    
    error_list_t *error_list;
    drill_list_t *drill_list;
    int comment;
    int F;

    int G00;
    int G01;
    int G02;
    int G03;
    int G04;
    int G05;
    int G90;
    int G91;
    int G93;
    int G_unknown;

    int M00;
    int M01;
    int M18;
    int M25;
    int M30;
    int M31;
    int M45;
    int M47;
    int M48;
    int M71;
    int M72;
    int M95;
    int M97;
    int M98;
    int M_unknown;

    int unknown;

    char *detect;

} drill_stats_t;

/* from gerb_image.h */
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
} gerbv_selection_item_t;

typedef struct {
	enum selection_t type;
	gdouble lowerLeftX;
	gdouble lowerLeftY;
	gdouble upperRightX;
	gdouble upperRightY;
	GArray *selectedNodeArray;
} gerbv_selection_info_t;

/*!  Stores image transformation information, used to modify the rendered
position/scale/etc of an image. */
typedef struct {
    gdouble translateX; /*!< the X translation (in inches) */
    gdouble translateY; /*!< the Y translation (in inches) */
    gdouble scaleX; /*!< the X scale factor (1.0 is default) */
    gdouble scaleY; /*!< the Y scale factor (1.0 is default) */
    gboolean inverted; /*!< TRUE if the image should be rendered "inverted" (light is dark and vice versa) */
} gerbv_user_transformation_t;

typedef struct gerbv_cirseg {
    double cp_x;
    double cp_y;
    double width;  /* of oval */
    double height; /* of oval */
    double angle1;
    double angle2;
} gerbv_cirseg_t;

typedef struct gerbv_step_and_repeat { /* SR parameters */
    int X;
    int Y;
    double dist_X;
    double dist_Y;
} gerbv_step_and_repeat_t;
 
typedef struct {
    gboolean firstInstance;
    enum knockout_t type;
    enum polarity_t polarity;
    gdouble lowerLeftX;
    gdouble lowerLeftY;
    gdouble width;
    gdouble height;
    gdouble border;
} gerbv_knockout_t;
 
typedef struct {
    gerbv_step_and_repeat_t stepAndRepeat;
    gerbv_knockout_t knockout;
    gdouble rotation;
    enum polarity_t polarity; 
    gchar *name;
    gpointer next;
} gerbv_layer_t;

typedef struct {
    enum axis_select_t axisSelect;
    enum mirror_state_t mirrorState;
    enum unit_t unit;
    gdouble offsetA;
    gdouble offsetB;
    gdouble scaleA;
    gdouble scaleB;
    gpointer next;
} gerbv_netstate_t;

/*!  The structure used to hold a geometric entity (line/polygon/etc)*/
typedef struct gerbv_net {
    double start_x; /*!< the X coordinate of the start point */
    double start_y; /*!< the Y coordinate of the start point */
    double stop_x; /*!< the X coordinate of the end point */
    double stop_y; /*!< the Y coordinate of the end point */
    int aperture; /*!< the index of the aperture used for this entity */
    enum aperture_state_t aperture_state; /*!< the state of the aperture tool (on/off/etc) */
    enum interpolation_t interpolation; /*!< the path interpolation method (linear/etc) */
    struct gerbv_cirseg *cirseg; /*!< a special struct used to hold circular path data */
    struct gerbv_net *next; /*!< the next net in the array */
    GString *label; /*!< a label string for this net */
    gerbv_layer_t *layer; /*!< the RS274X layer this net belongs to */
    gerbv_netstate_t *state; /*!< the RS274X state this net belongs to */
} gerbv_net_t;

typedef struct gerbv_format {
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
} gerbv_format_t;
	
	
typedef struct gerbv_image_info {
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
} gerbv_image_info_t;

/*!  The structure used to hold a layer (RS274X, drill, or pick-and-place data) */
typedef struct {
    enum layertype_t layertype; /*!< the type of layer (RS274X, drill, or pick-and-place) */
    gerbv_aperture_t *aperture[APERTURE_MAX]; /*!< an array with all apertures used */
    gerbv_layer_t *layers; /*!< an array of all RS274X layers used (only used in RS274X types) */
    gerbv_netstate_t *states; /*!< an array of all RS274X states used (only used in RS274X types) */
    amacro_t *amacro; /*!< an array of all macros used (only used in RS274X types) */
    gerbv_format_t *format; /*!< formatting info */
    gerbv_image_info_t *info; /*!< miscellaneous info regarding the layer such as overall size, etc */
    gerbv_net_t *netlist; /*!< an array of all geometric entities in the layer */
    gerbv_stats_t *gerbv_stats; /*!< RS274X statistics for the layer */
    drill_stats_t *drill_stats;  /*!< Excellon drill statistics for the layer */
} gerbv_image_t;

//! Allocate a new gerbv_image structure
//! \return the newly created image
gerbv_image_t *gerbv_create_image(gerbv_image_t *image, /*!< the old image to free or NULL */
		const gchar *type /*!< the type of image to create */
);

//! Free an image structure
void gerbv_destroy_image(gerbv_image_t *image /*!< the image to free */
);

//! Copy an image into an existing image, effectively merging the two together
void
gerbv_image_copy_image (gerbv_image_t *sourceImage, /*!< the source image */
	gerbv_user_transformation_t *transform, /*!< the transformation to apply to the new image, or NULL for none */
	gerbv_image_t *destinationImage /*!< the destination image to copy to */
);

//! Duplicate an existing image and return the new copy
//! \return the newly created image
gerbv_image_t *
gerbv_image_duplicate_image (gerbv_image_t *sourceImage, /*!< the source image */
	gerbv_user_transformation_t *transform /*!< the transformation to apply to the new image, or NULL for none */
);

//! Delete a net in an existing image
void
gerbv_image_delete_net (gerbv_net_t *currentNet /*!< the net to delete */
);

void
gerbv_image_delete_selected_nets (gerbv_image_t *sourceImage, GArray *selectedNodeArray);

gboolean
gerbv_image_reduce_area_of_selected_objects (GArray *selectionArray, gdouble areaReduction, gint paneRows,
		gint paneColumns, gdouble paneSeparation);

gboolean
gerbv_image_move_selected_objects (GArray *selectionArray, gdouble translationX,
		gdouble translationY);

/* from old gerbv_screen.h */
#define INITIAL_SCALE 200
#define MAX_ERRMSGLEN 25
#define MAX_COORDLEN 28
#define MAX_DISTLEN 90
#define MAX_STATUSMSGLEN (MAX_ERRMSGLEN+MAX_COORDLEN+MAX_DISTLEN)

/* Macros to convert between unscaled gerber coordinates and other units */
/* XXX NOTE: Currently unscaled units are assumed as inch, this is not
   XXX necessarily true for all files */
#define COORD2MILS(c) ((c)*1000.0)
#define COORD2MMS(c) ((c)*25.4)

typedef enum {NORMAL, IN_MOVE, IN_ZOOM_OUTLINE, IN_MEASURE, ALT_PRESSED,
		IN_SELECTION_DRAG, SCROLLBAR} gerbv_state_t;
typedef enum {POINTER, PAN, ZOOM, MEASURE} gerbv_tool_t;
typedef enum {GERBV_MILS, GERBV_MMS, GERBV_INS} gerbv_unit_t;

typedef struct {
    gerbv_image_t *image;
    GdkColor color;
    guint16 alpha;
    gboolean isVisible;
    gpointer privateRenderData;
    gchar *fullPathname; /* this should be the full pathname to the file */
    gchar *name;
    gerbv_user_transformation_t transform;
} gerbv_fileinfo_t;

typedef struct {
	double x1, y1;
	double x2, y2;
} gerbv_bbox_t;

/*!  The top-level structure used in libgerbv.  A gerbv_project_t groups together
any number of layers, while keeping track of other basic paramters needed for rendering */
typedef struct {
  GdkColor  background; /*!< the background color used for rendering */
  int max_files; /*!< the current number of fileinfos in the file array */
  gerbv_fileinfo_t **file; /*!< the array for holding the child fileinfos */
  int curr_index; /*!< the index of the currently active fileinfo */
  int last_loaded; /*!< the number of fileinfos currently in the project */
  int renderType; /*!< the type of renderer to use */
  gboolean project_dirty;   /*!< TRUE if changes have been made since last save */ 
  gboolean check_before_delete;  /*!< TRUE to ask before deleting objects */
  gchar *path; /*!< the default path to load new files from */
  gchar *execpath;    /*!< the path to executed version of gerbv */
  gchar *project;     /*!< the default name for the private project file */
} gerbv_project_t;

typedef enum {ZOOM_IN, ZOOM_OUT, ZOOM_FIT, ZOOM_IN_CMOUSE, ZOOM_OUT_CMOUSE, ZOOM_SET } gerbv_zoom_dir_t;
typedef struct {
    gerbv_zoom_dir_t z_dir;
    GdkEventButton *z_event;
    int scale;
} gerbv_zoom_data_t;

//! Create a new project structure and initialize some important variables
gerbv_project_t *
gerbv_create_project (void);

//! Free a project and all related variables
void
gerbv_destroy_project (gerbv_project_t *gerbvProject /*!< the project to destroy */
);

//! Open a file, parse the contents, and add a new layer to an existing project
void 
gerbv_open_layer_from_filename (
	gerbv_project_t *gerbvProject, /*!< the existing project to add the new layer to */
	gchar *filename /*!< the full pathname of the file to be parsed */
);

//! Free a fileinfo structure
void
gerbv_destroy_fileinfo (gerbv_fileinfo_t *fileInfo /*!< the fileinfo to free */
);

gboolean 
gerbv_save_layer_from_index(gerbv_project_t *gerbvProject, gint index, gchar *filename);

int
gerbv_revert_file(gerbv_project_t *gerbvProject, int idx);

void 
gerbv_revert_all_files(gerbv_project_t *gerbvProject);

void 
gerbv_unload_layer(gerbv_project_t *gerbvProject, int index);

void 
gerbv_unload_all_layers (gerbv_project_t *gerbvProject);

void 
gerbv_change_layer_order(gerbv_project_t *gerbvProject, gint oldPosition, gint newPosition);

gint
gerbv_add_parsed_image_to_project (gerbv_project_t *gerbvProject, gerbv_image_t *parsed_image,
			gchar *filename, gchar *baseName, int idx, int reload);
int
gerbv_open_image(gerbv_project_t *gerbvProject, char *filename, int idx, int reload,
		HID_Attribute *fattr, int n_fattr, gboolean forceLoadFile);
		
/* Notice that the pixel field is used for alpha in this case */
typedef struct{
    unsigned char red;
    unsigned char green;
    unsigned char blue;
    unsigned char alpha;
}LayerColor;

/* old render.h stuff */
typedef struct {
	gdouble scaleFactorX;
	gdouble scaleFactorY;
	gdouble lowerLeftX;
	gdouble lowerLeftY;
	gint renderType; /* 0 is default */
	gint displayWidth;
	gint displayHeight;
} gerbv_render_info_t;

typedef struct {
    double left;
    double right;
    double top;
    double bottom;
} gerbv_render_size_t;

void
gerbv_render_get_boundingbox(gerbv_project_t *gerbvProject, gerbv_render_size_t *boundingbox);

void
gerbv_render_zoom_to_fit_display (gerbv_project_t *gerbvProject, gerbv_render_info_t *renderInfo);

void
gerbv_render_translate_to_fit_display (gerbv_project_t *gerbvProject, gerbv_render_info_t *renderInfo);

void
gerbv_render_to_pixmap_using_gdk (gerbv_project_t *gerbvProject, GdkPixmap *pixmap,
		gerbv_render_info_t *renderInfo, gerbv_selection_info_t *selectionInfo,
		GdkColor *selectionColor);

#ifndef RENDER_USING_GDK
void
gerbv_render_all_layers_to_cairo_target_for_vector_output (gerbv_project_t *gerbvProject,
		cairo_t *cr, gerbv_render_info_t *renderInfo);

void
gerbv_render_all_layers_to_cairo_target (gerbv_project_t *gerbvProject, cairo_t *cr,
			gerbv_render_info_t *renderInfo);

void
gerbv_render_layer_to_cairo_target (cairo_t *cr, gerbv_fileinfo_t *fileInfo,
						gerbv_render_info_t *renderInfo);
						
void
gerbv_render_cairo_set_scale_and_translation(cairo_t *cr, gerbv_render_info_t *renderInfo);

void
gerbv_render_layer_to_cairo_target_without_transforming(cairo_t *cr, gerbv_fileinfo_t *fileInfo, gerbv_render_info_t *renderInfo );
#endif

/* from old tooltable.h */
double GetToolDiameter_Inches(int toolNumber);
int ProcessToolsFile(const char *toolFileName);

//! Render a project to a PNG file, autoscaling the layers to fit inside the specified image dimensions
void
gerbv_export_png_file_from_project_autoscaled (
		gerbv_project_t *gerbvProject, /*!< the project to render */
		int widthInPixels, /*!< the width of the rendered picture (in pixels) */
		int heightInPixels, /*!< the height of the rendered picture (in pixels) */
		gchar const* filename  /*!< the filename for the exported PNG file */
);

//! Render a project to a PNG file using user-specified render info
void
gerbv_export_png_file_from_project (
		gerbv_project_t *gerbvProject, /*!< the project to render */
		gerbv_render_info_t *renderInfo, /*!< the render settings for the rendered image */
		gchar const* filename /*!< the filename for the exported PNG file */
);

//! Render a project to a PDF file, autoscaling the layers to fit inside the specified image dimensions
void
gerbv_export_pdf_file_from_project_autoscaled (
		gerbv_project_t *gerbvProject, /*!< the project to render */
		int widthInPixels, /*!< the width of the rendered picture (in pixels) */
		int heightInPixels, /*!< the height of the rendered picture (in pixels) */
		gchar const* filename  /*!< the filename for the exported PDF file */
);

//! Render a project to a PDF file using user-specified render info
void
gerbv_export_pdf_file_from_project (
		gerbv_project_t *gerbvProject, /*!< the project to render */
		gerbv_render_info_t *renderInfo, /*!< the render settings for the rendered image */
		gchar const* filename /*!< the filename for the exported PDF file */
);

//! Render a project to a Postscript file, autoscaling the layers to fit inside the specified image dimensions
void
gerbv_export_postscript_file_from_project_autoscaled (
		gerbv_project_t *gerbvProject, /*!< the project to render */
		int widthInPixels, /*!< the width of the rendered picture (in pixels) */
		int heightInPixels, /*!< the height of the rendered picture (in pixels) */
		gchar const* filename  /*!< the filename for the exported Postscript file */
);

//! Render a project to a Postscript file using user-specified render info
void
gerbv_export_postscript_file_from_project (
		gerbv_project_t *gerbvProject, /*!< the project to render */
		gerbv_render_info_t *renderInfo, /*!< the render settings for the rendered image */
		gchar const* filename /*!< the filename for the exported Postscript file */
);

//! Render a project to a SVG file, autoscaling the layers to fit inside the specified image dimensions
void
gerbv_export_svg_file_from_project_autoscaled (
		gerbv_project_t *gerbvProject, /*!< the project to render */
		int widthInPixels, /*!< the width of the rendered picture (in pixels) */
		int heightInPixels, /*!< the height of the rendered picture (in pixels) */
		gchar const* filename  /*!< the filename for the exported   file */
);

//! Render a project to a   file using user-specified render info
void
gerbv_export_svg_file_from_project (
		gerbv_project_t *gerbvProject, /*!< the project to render */
		gerbv_render_info_t *renderInfo, /*!< the render settings for the rendered image */
		gchar const* filename /*!< the filename for the exported   file */
);

//! Parse a RS274X file and return the parsed image
//! \return the new gerbv_image_t, or NULL if not successful
gerbv_image_t *
gerbv_create_rs274x_image_from_filename (gchar *filename /*!< the filename of the file to be parsed*/
);

//! Export an image to a new file in RS274X format
//! \return TRUE if successful, or FALSE if not
gboolean
gerbv_export_rs274x_file_from_image (gchar *filename, /*!< the filename for the new file */
		gerbv_image_t *image /*!< the image to export */
);

//! Export an image to a new file in Excellon drill format
//! \return TRUE if successful, or FALSE if not
gboolean
gerbv_export_drill_file_from_image (gchar *filename, /*!< the filename for the new file */
		gerbv_image_t *image /*!< the image to export */
);

//! Draw a line on the specified image
void
gerbv_image_create_line_object (gerbv_image_t *image, /*!< the image to draw to */
		gdouble startX, /*!< the starting X coordinate */
		gdouble startY, /*!< the starting Y coordinate */
		gdouble endX, /*!< the ending X coordinate */
		gdouble endY, /*!< the ending Y coordinate */
		gdouble lineWidth, /*!< the width of the line to draw */
		enum aperture_t apertureType  /*!< the type of aperture to use (e.g. CIRCLE) */
);

//! Draw an arc on the specified image
void
gerbv_image_create_arc_object (gerbv_image_t *image, /*!< the image to draw to */
		gdouble centerX, /*!< the center X coordinate */
		gdouble centerY, /*!< the center Y coordinate */
		gdouble radius, /*!< the arc radius */
		gdouble startAngle, /*!< the start angle (in CCW degrees) */
		gdouble endAngle, /*!< the start angle (in CCW degrees) */
		gdouble lineWidth, /*!< the width of the line to draw */
		enum aperture_t apertureType  /*!< the type of aperture to use (e.g. CIRCLE) */
);
		
//! Draw a filled rectangle on the specified image	
void
gerbv_image_create_rectangle_object (gerbv_image_t *image, /*!< the image to draw to */
		gdouble coordinateX, /*!< the X coordinate of the lower left corner */
		gdouble coordinateY, /*!< the Y coordinate of the lower left corner */
		gdouble width, /*!< the width of the drawn rectangle */
		gdouble height /*!< the height of the drawn rectangle */
);
			
/* from drill and gerb stats headers */
drill_stats_t * drill_stats_new(void);
void drill_stats_add_layer(drill_stats_t *accum_stats, 
			   drill_stats_t *input_stats, int this_layer);

gerbv_stats_t * gerbv_stats_new(void);
void gerbv_stats_add_layer(gerbv_stats_t *accum_stats, 
			  gerbv_stats_t *input_stats,
			  int this_layer);


/*
 * gEDA - GNU Electronic Design Automation
 * This files is a part of gerbv.
 *
 *   Copyright (C) 2000-2001 Stefan Petersen (spe@stacken.kth.se)
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
 
/** maximum size of strings. */
#define MAXL 200

enum e_footprint {
    PART_SHAPE_UNKNOWN = 0, /* drawn as circle with line*/
    PART_SHAPE_RECTANGLE = 1, /* rectangle with one side marked*/
    PART_SHAPE_STD = 2 /* rectangle with one corner marked*/
};

/** captures all pick and place data columns.
 which were read successfully in from a pick and place file.\n
  There is only one pnp file at any time valid, for the whole project. Loading a second pnp file results in complete loss of previous pick_and_place data.\n
  
  typedef struct pnp_state {\n
    
    char     designator[MAXL];\n
    char     footprint[MAXL];\n
    double   mid_x;\n
    double   mid_y;\n
    double   ref_x;\n
    double   ref_y;\n
    double   pad_x;\n
    double   pad_y;\n
    char     layer[MAXL]; T is top B is bottom\n
    double   rotation;\n
    char     comment[MAXL];    \n
    int      shape;\n
    double   width;\n
    double   length;\n

    unsigned int nuf_push;   Nuf pushes to estimate stack size \n
    struct pnp_state *next;\n
    } pnp_state_t;\n
  
*/
typedef struct pnp_state {
    
    char     designator[MAXL];
    char     footprint[MAXL];
    double   mid_x;
    double   mid_y;
    double   ref_x;
    double   ref_y;
    double   pad_x;
    double   pad_y;
    char     layer[MAXL]; /*T is top B is bottom*/
    double   rotation;
    char     comment[MAXL];    
    int      shape;
    double   width;
    double   length;

    unsigned int nuf_push;  /* Nuf pushes to estimate stack size */
    struct pnp_state *next;
    } pnp_state_t;

/** captures all pick and place data in a netlist style.
    Is intended to be also used e.g. for reloading files*/        
extern pnp_state_t *parsed_PNP_data;

pnp_state_t *new_pnp_state();
double get_float_unit(char *str);
int pnp_screen_for_delimiter(char *str, int n);
pnp_state_t *parse_pnp(gerb_file_t *fd);
void free_pnp_state(pnp_state_t *pnp_state);

gerb_image_t *
pick_and_place_parse_file(gerb_file_t *fd);

gboolean
pick_and_place_check_file_type(gerb_file_t *fd);

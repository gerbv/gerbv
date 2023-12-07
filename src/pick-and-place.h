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

/** \file pick-and-place.h
    \brief Header info for the PNP (pick-and-place) parsing functions
    \ingroup libgerbv
*/

#ifndef GERBV_LAYERTYPE_PICKANDPLACE_H
#define GERBV_LAYERTYPE_PICKANDPLACE_H

#define MAXL 200

typedef struct gerb_transf {
    double r_mat[2][2];
    double scale;
    double offset[2];
} gerbv_transf_t;

enum e_footprint {
    PART_SHAPE_UNKNOWN   = 0, /* drawn as circle with line*/
    PART_SHAPE_RECTANGLE = 1, /* rectangle with one side marked*/
    PART_SHAPE_STD       = 2  /* rectangle with one corner marked*/
};

typedef struct {
    double       mid_x;
    double       mid_y;
    double       ref_x;
    double       ref_y;
    double       pad_x;
    double       pad_y;
    double       rotation;
    int          shape;
    double       width;
    double       length;
    unsigned int nuf_push; /* Nuf pushes to estimate stack size */
    unsigned int placed:1;

    char         value[50];
    char         designator[16];
    char         footprint[30];
    char         layer[8]; /*T is top B is bottom*/
    char         comment[MAXL];
} PnpPartData;


gerbv_image_t* pick_and_place_parse_file_to_image(gerb_file_t* fd);


enum pnp_file_type {
	PNP_FILE_UNKNOWN = 0,
	PNP_FILE_CSV,
	PNP_FILE_PARTLIST_EAGLE,
	PNP_FILES
};

void pick_and_place_parse_file_to_images(gerb_file_t* fd, struct pnp_manual_dev *, enum pnp_file_type,
		gerbv_image_t** topImage, gerbv_image_t** bottomImage);

int pick_and_place_check_file_type(gerb_file_t* fd, gboolean* returnFoundBinary);

struct pnp_manual_dev; // defined in pick-and-place.c, private

struct pnp_pub_context {
	GArray* tb_part_list;  // array of PnpPartData, top & bot common
	GPtrArray* top_part_list; // list of pointers to PnpPartData + n, sorted by value
	GPtrArray* bot_part_list; // list of pointers to PnpPartData + n, sorted by value

	gerbv_image_t* top_image;
	gerbv_image_t* bot_image;

	int tb_pl_ref_count;
	unsigned int center_ch_color; // rgb 888
	struct {
		unsigned int rcvd_brdloc;
		unsigned int rcvd_pnploc;
		unsigned int rcvd_invalid;
	} stats;
};

enum pnp_events {
	PNP_EV_NONE = 0,
	PNP_EV_BRDLOC,
	PNP_EV_EN_CAL12_MENU,

};

struct pnp_event_data {
	enum pnp_events evc;
	union {
		struct { // PNP_EV_BRDLOC
			double board_x, board_y; // [inches]
		} loc;

		int args[3]; // [0]: PNP_EV_EN_CAL12_MENU, ..
	};
	struct pnp_manual_dev *mdev;
};

typedef void (*pnp_remote_event_fn)(void *arg, struct pnp_event_data *);

struct pnp_manual_dev *pick_and_place_mdev_init(char *, pnp_remote_event_fn, void *arg);
enum pnp_mdev_opcodes {
	MDEV_CAL_OFF = -1,
	MDEV_CAL_SAV_REFLOC1 = 1,
	MDEV_CAL_SAV_REFLOC2,

	MDEV_REDRAW
};
int pick_and_place_mdev_ctl(struct pnp_manual_dev *, double board_x, double board_y, enum pnp_mdev_opcodes opcode);
struct pnp_pub_context *pick_and_place_mdev2ctx(struct pnp_manual_dev *);

void pick_and_place_mdev_free(struct pnp_manual_dev **);

#endif /* GERBV_LAYERTYPE_PICKANDPLACE_H */

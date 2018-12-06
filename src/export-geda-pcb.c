/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of Gerbv.
 *
 *   Copyright (C) 2014 Sergey Alyoshin
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

/** \file export-geda-pcb.c
    \brief Functions for exporting Gerbv images to gEDA PCB files
    \ingroup libgerbv
*/

#include "gerbv.h"
#include "common.h"

#include <glib/gstdio.h>

static void
write_line(FILE *fd, gerbv_net_t *net, double thick,
		double dx_p, double dy_m, const char *sflags)
{
	dx_p = COORD2MILS(dx_p);
	dy_m = COORD2MILS(dy_m);

	fprintf(fd, "\tLine[%.2fmil %.2fmil %.2fmil %.2fmil "
			"%.2fmil %.2fmil \"%s\"]\n",
			dx_p + COORD2MILS(net->stop_x),
			dy_m - COORD2MILS(net->stop_y),
			dx_p + COORD2MILS(net->start_x),
			dy_m - COORD2MILS(net->start_y),
			COORD2MILS(thick), 100.0, sflags);
}

static void
write_element_with_pad(FILE *fd, gerbv_net_t *net, double thick,
		double dx_p, double dy_m, const char *sflags)
{
	double xc, yc;
	static unsigned int element_num = 1;

	dx_p = COORD2MILS(dx_p);
	dy_m = COORD2MILS(dy_m);

	xc = COORD2MILS(net->stop_x + net->start_x)/2;
	yc = COORD2MILS(net->stop_y + net->start_y)/2;

	fprintf(fd, "Element[\"\" \"\" \"pad%d\" \"\" "
			"%.2fmil %.2fmil 0mil 0mil 0 100 \"\"]\n(\n",
			element_num++, dx_p + xc, dy_m - yc);
	fprintf(fd, "\tPad[%.2fmil %.2fmil %.2fmil %.2fmil "
			"%.2fmil 0mil %.2fmil "
			"\"%s\" \"%s\" \"%s\"]\n)\n",
			xc - COORD2MILS(net->stop_x),
			yc - COORD2MILS(net->stop_y),
			xc - COORD2MILS(net->start_x),
			yc - COORD2MILS(net->start_y),
			COORD2MILS(thick),	/* Thickness */
			COORD2MILS(thick),	/* Mask */
			"1",			/* Name */
			"1",			/* Number */
			sflags);		/* String flags */
}

static void
write_polygon(FILE *fd, gerbv_net_t *net,
		double dx_p, double dy_m, const char *sflags)
{
	dx_p = COORD2MILS(dx_p);
	dy_m = COORD2MILS(dy_m);

	fprintf(fd, "\tPolygon(\"%s\")\n\t(", sflags);
	net = net->next;

	unsigned int i = 0;
	while (net != NULL
		&& net->interpolation != GERBV_INTERPOLATION_PAREA_END) {
		if (net->aperture_state == GERBV_APERTURE_STATE_ON) {
			fprintf(fd, "%s[%.2fmil %.2fmil] ",
				!(i%5)? "\n\t\t": "",
				dx_p + COORD2MILS(net->stop_x),
				dy_m - COORD2MILS(net->stop_y));
				i++;
		}
		net = net->next;
	}

	fprintf(fd, "\n\t)\n");
}

gboolean
gerbv_export_geda_pcb_file_from_image(const gchar *file_name,
		gerbv_image_t *input_img,
		gerbv_user_transformation_t *trans)
{
	gerbv_aperture_t *apert;
	gerbv_image_t *img;
	gerbv_net_t *net;
	double dx_p, dy_m;
	double thick, len;
	FILE *fd;

	if ((fd = g_fopen(file_name, "w")) == NULL) {
		GERB_MESSAGE(_("Can't open file for writing: %s"), file_name);
		return FALSE;
	}

	/* Output decimals as dots for all locales */
	setlocale(LC_NUMERIC, "C");

	/* Duplicate the image, cleaning it in the process */
	img = gerbv_image_duplicate_image(input_img, trans);

	/* Header */
	fputs("# Generated with gerbv\n\n", fd);
	fputs("FileVersion[20091103]\n", fd);

	dx_p = (img->info->max_x - img->info->min_x) - img->info->min_x;
	dy_m = 2*(img->info->max_y - img->info->min_y) + img->info->min_y;

	/* Make board size is 3 times more than Gerber size */
	fprintf(fd, "PCB[\"%s\" %.2fmil %.2fmil]\n",
			img->info->name,
			3*COORD2MILS(img->info->max_x - img->info->min_x),
			3*COORD2MILS(img->info->max_y - img->info->min_y));

	fputs("Grid[1000.000000 0.0000 0.0000 0]\n", fd);

	/* Write all apertures as elements with single pad, before layer
	 * definition */
	for (net = img->netlist; net != NULL; net = net->next) {
		apert = img->aperture[net->aperture];
		if (!apert)
			continue;

		/* Skip polygon, it will be written in layer section */
		if (net->interpolation == GERBV_INTERPOLATION_PAREA_START) {
			/* Skip to the end for polygon */
			do {
				net = net->next;
			} while (net != NULL && net->interpolation !=
					GERBV_INTERPOLATION_PAREA_END);
			continue;
		}

		switch (net->aperture_state) {
		case GERBV_APERTURE_STATE_FLASH:
			switch (apert->type) {
			case GERBV_APTYPE_CIRCLE:
				/* Set start to stop coords for Circle flash */
				net->start_x = net->stop_x;
				net->start_y = net->stop_y;
				write_element_with_pad(fd, net,
						apert->parameter[0],
						dx_p, dy_m, "");
				break;
			case GERBV_APTYPE_OVAL:
			case GERBV_APTYPE_RECTANGLE:

				if (apert->parameter[0] > apert->parameter[1]) {
					/* Horizontal */
					len = apert->parameter[0];
					thick = apert->parameter[1];

					net->start_x = net->stop_x - len/2 +
								thick/2;
					net->stop_x += len/2 - thick/2;
					net->start_y = net->stop_y;
				} else {
					/* Vertical */
					len = apert->parameter[1];
					thick = apert->parameter[0];

					net->start_x = net->stop_x;
					net->start_y = net->stop_y - len/2 +
								thick/2;
					net->stop_y += len/2 - thick/2;
				}

				write_element_with_pad(fd, net, thick,
					dx_p, dy_m,
					(apert->type == GERBV_APTYPE_RECTANGLE)?
						"square": "");
				break;
			default:
/* TODO */
				GERB_COMPILE_WARNING(
					"%s:%d: aperture type %d is "
					"not yet supported",
					__func__, __LINE__,
					apert->type);
				break;
			}
			break;
		case GERBV_APERTURE_STATE_ON:
			/* Will be done in layer section */
			break;
		default:
/* TODO */
			GERB_COMPILE_WARNING(
					"%s:%d: aperture type %d is "
					"not yet supported",
					__func__, __LINE__,
					apert->type);
			break;
		}
	}

	/* Write all lines in layer definition */
	fputs("Layer(1 \"top\")\n(\n", fd);

	for (net = img->netlist; net != NULL; net = net->next) {
		apert = img->aperture[net->aperture];
		if (!apert)
			continue;

		if (net->interpolation == GERBV_INTERPOLATION_PAREA_START) {
			write_polygon(fd, net, dx_p, dy_m, "clearpoly");

			/* Skip to the end for polygon */
			do {
				net = net->next;
			} while (net != NULL && net->interpolation !=
					GERBV_INTERPOLATION_PAREA_END);
			continue;
		}

		switch (net->aperture_state) {
		case GERBV_APERTURE_STATE_FLASH:
			/* Already done in elements section */
			break;

		case GERBV_APERTURE_STATE_ON:
			/* Trace (or cut slot in drill file) */
			switch (apert->type) {
			case GERBV_APTYPE_CIRCLE:
				write_line(fd, net, apert->parameter[0],
						dx_p, dy_m, "clearline");
				break;
			default:
				GERB_COMPILE_WARNING(
						"%s:%d: aperture type %d is "
						"not yet supported",
						__func__, __LINE__,
						apert->type);
				break;
			}
			break;
		default:
			GERB_COMPILE_WARNING(
					"%s:%d: aperture state %d type %d is "
					"not yet supported",
					__func__, __LINE__,
					net->aperture_state, apert->type);
			break;
		}
	}

	fputs(")\n", fd);	/* End of Layer 1 */

	/* Necessary layer */
	fputs("Layer(7 \"outline\")\n(\n)\n", fd);

	gerbv_destroy_image (img);
	fclose(fd);

	setlocale(LC_NUMERIC, "");	/* Return to the default locale */

	return TRUE;
}

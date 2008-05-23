/*
 * gEDA - GNU Electronic Design Automation
 * This files is a part of gerbv.
 *
 *   Copyright (C) 2000-2002 Stefan Petersen (spe@stacken.kth.se)
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

/** \file amacro.h
    \brief Aperture macro parsing header info
    \ingroup libgerbv
*/

#ifndef AMACRO_H
#define AMACRO_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Parses the definition of an aperture macro
 */
gerbv_amacro_t *parse_aperture_macro(gerb_file_t *fd);

/*
 * Frees amacro struct completly
 */
void free_amacro(gerbv_amacro_t *amacro);

/*
 * Print out parsed aperture macro. For debugging purpose.
 */
void print_program(gerbv_amacro_t *amacro);


#ifdef __cplusplus
}
#endif

#endif /* AMACRO_H */

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

#ifndef AMACRO_H
#define AMACRO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "gerb_file.h"


enum opcodes {NOP,      /* No Operation */
	      PUSH,     /* Ordinary stack operations. Uses float */
	      PPUSH,    /* Parameter onto stack, 1 is first parameter and 
			    so on (compare gerber $1, $2 and so on */ 
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


/*
 * Parses the definition of an aperture macro
 */
amacro_t *parse_aperture_macro(gerb_file_t *fd);

/*
 * Frees amacro struct completly
 */
void free_amacro(amacro_t *amacro);

/*
 * Print out parsed aperture macro. For debugging purpose.
 */
void print_program(amacro_t *amacro);


#ifdef __cplusplus
}
#endif

#endif /* AMACRO_H */

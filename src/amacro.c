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

/** \file amacro.c
    \brief Aperture macro parsing functions
    \ingroup libgerbv
*/

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "gerbv.h"
#include "gerb_file.h"
#include "amacro.h"

/*
 * Allocates a new instruction structure
 */
static gerbv_instruction_t *
new_instruction(void)
{
    gerbv_instruction_t *instruction;

    instruction = (gerbv_instruction_t *)malloc(sizeof(gerbv_instruction_t));
    if (instruction == NULL) {
	free(instruction);
	return NULL;
    }

    memset(instruction, 0, sizeof(gerbv_instruction_t));
    
    return instruction;
} /* new_instruction */


/*
 * Allocates a new amacro structure
 */
static gerbv_amacro_t *
new_amacro(void)
{
    gerbv_amacro_t *amacro;

    amacro = (gerbv_amacro_t *)malloc(sizeof(gerbv_amacro_t));
    if (amacro == NULL) {
	free(amacro);
	return NULL;
    }

    memset(amacro, 0, sizeof(gerbv_amacro_t));
    
    return amacro;
} /* new_amacro */


/*
 * Parses the definition of an aperture macro
 */
gerbv_amacro_t *
parse_aperture_macro(gerb_file_t *fd)
{
    gerbv_amacro_t *amacro;
    gerbv_instruction_t *ip = NULL;
    int primitive = 0, c, found_primitive = 0;
    gerbv_opcodes_t math_op = GERBV_OPCODE_NOP;
    int comma = 0; /* Just read an operator (one of '*+X/) */
    int neg = 0; /* negative numbers succeding , */
    unsigned char continueLoop = 1;
    int equate = 0;

    amacro = new_amacro();

    /*
     * Get macroname
     */
    amacro->name = gerb_fgetstring(fd, '*');
    c = gerb_fgetc(fd);	/* skip '*' */
    if (c == EOF) {
	continueLoop = 0;
    }
    
    /*
     * Since I'm lazy I have a dummy head. Therefore the first 
     * instruction in all programs will be NOP.
     */
    amacro->program = new_instruction();
    ip = amacro->program;
    
    while(continueLoop) {
	
	c = gerb_fgetc(fd);
	switch (c) {
	case '$':
	    if (found_primitive) {
		ip->next = new_instruction(); /* XXX Check return value */
		ip = ip->next;
		ip->opcode = GERBV_OPCODE_PPUSH;
		amacro->nuf_push++;
		ip->data.ival = gerb_fgetint(fd, NULL);
		comma = 0;
	    } else {
		equate = gerb_fgetint(fd, NULL);
	    }
	    break;
	case '*':
	    if (math_op != GERBV_OPCODE_NOP) {
		ip->next = new_instruction(); /* XXX Check return value */
		ip = ip->next;
		ip->opcode = math_op;
		math_op = GERBV_OPCODE_NOP;
	    }
	    /*
	     * Check is due to some gerber files has spurious empty lines.
	     * (EagleCad of course).
	     */
	    if (found_primitive) {
		ip->next = new_instruction(); /* XXX Check return value */
		ip = ip->next;
		if (equate) {
		    ip->opcode = GERBV_OPCODE_PPOP;
		    ip->data.ival = equate;
		} else {
		    ip->opcode = GERBV_OPCODE_PRIM;
		    ip->data.ival = primitive;
		}
		equate = 0;
		primitive = 0;
		found_primitive = 0;
	    }
	    break;
	case '=':
	    if (equate) {
		found_primitive = 1;
	    }
	    break;
	case ',':
	    if (!found_primitive) {
		found_primitive = 1;
		break;
	    }
	    if (math_op != GERBV_OPCODE_NOP) {
		ip->next = new_instruction(); /* XXX Check return value */
		ip = ip->next;
		ip->opcode = math_op;
		math_op = GERBV_OPCODE_NOP;
	    }
	    comma = 1;
	    break;
	case '+':
	    if (math_op != GERBV_OPCODE_NOP) {
		ip->next = new_instruction(); /* XXX Check return value */
		ip = ip->next;
		ip->opcode = math_op;
	    }
	    math_op = GERBV_OPCODE_ADD;
	    comma = 1;
	    break;
	case '-':
	    if (comma) {
		neg = 1;
		comma = 0;
		break;
	    }
	    if (math_op != GERBV_OPCODE_NOP) {
		ip->next = new_instruction(); /* XXX Check return value */
		ip = ip->next;
		ip->opcode = math_op;
	    }
	    math_op = GERBV_OPCODE_SUB;
	    break;
	case '/':
	    if (math_op != GERBV_OPCODE_NOP) {
		ip->next = new_instruction(); /* XXX Check return value */
		ip = ip->next;
		ip->opcode = math_op;
	    }
	    math_op = GERBV_OPCODE_DIV;
	    comma = 1;
	    break;
	case 'X':
	case 'x':
	    if (math_op != GERBV_OPCODE_NOP) {
		ip->next = new_instruction(); /* XXX Check return value */
		ip = ip->next;
		ip->opcode = math_op;
	    }
	    math_op = GERBV_OPCODE_MUL;
	    comma = 1;
	    break;
	case '0':
	    /*
	     * Comments in aperture macros are a definition starting with
	     * zero and ends with a '*'
	     */
	    if (!found_primitive && (primitive == 0)) {
		/* Comment continues 'til next *, just throw it away */
		gerb_fgetstring(fd, '*');
		c = gerb_fgetc(fd); /* Read the '*' */
		break;
	    }
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	    /* 
	     * First number in an aperture macro describes the primitive
	     * as a numerical value
	     */
	    if (!found_primitive) {
		primitive = (primitive * 10) + (c - '0');
		break;
	    }
	    (void)gerb_ungetc(fd);
	    ip->next = new_instruction(); /* XXX Check return value */
	    ip = ip->next;
	    ip->opcode = GERBV_OPCODE_PUSH;
	    amacro->nuf_push++;
	    ip->data.fval = gerb_fgetdouble(fd);
	    if (neg) 
		ip->data.fval = -ip->data.fval;
	    neg = 0;
	    comma = 0;
	    break;
	case '%':
	    gerb_ungetc(fd);  /* Must return with % first in string
				 since the main parser needs it */
	    return amacro;
	default :
	    /* Whitespace */
	    break;
	}
	if (c == EOF) {
	    continueLoop = 0;
	}
    }
    free (amacro);
    return NULL;
}


void 
free_amacro(gerbv_amacro_t *amacro)
{
    gerbv_amacro_t *am1, *am2;
    gerbv_instruction_t *instr1, *instr2;
    
    am1 = amacro;
    while (am1 != NULL) {
	free(am1->name);
	am1->name = NULL;

	instr1 = am1->program;
	while (instr1 != NULL) {
	    instr2 = instr1;
	    instr1 = instr1->next;
	    free(instr2);
	    instr2 = NULL;
	}

	am2 = am1;
	am1 = am1->next;
	free(am2);
	am2 = NULL;
    }
	
    return;
} /* free_amacro */


void 
print_program(gerbv_amacro_t *amacro)
{
    gerbv_instruction_t *ip;

    printf("Macroname [%s] :\n", amacro->name);
    for (ip = amacro->program ; ip != NULL; ip = ip->next) {
	switch(ip->opcode) {
	case GERBV_OPCODE_NOP:
	    printf(" NOP\n");
	    break;
	case GERBV_OPCODE_PUSH: 
	    printf(" PUSH %f\n", ip->data.fval);
	    break;
	case GERBV_OPCODE_PPOP:
	    printf(" PPOP %d\n", ip->data.ival);
	    break;
	case GERBV_OPCODE_PPUSH:
	    printf(" PPUSH %d\n", ip->data.ival);
	    break;
	case GERBV_OPCODE_ADD:
	    printf(" ADD\n");
	    break;
	case GERBV_OPCODE_SUB:
	    printf(" SUB\n");
	    break;
	case GERBV_OPCODE_MUL:
	    printf(" MUL\n");
	    break;
	case GERBV_OPCODE_DIV:
	    printf(" DIV\n");
	    break;
	case GERBV_OPCODE_PRIM:
	    printf(" PRIM %d\n", ip->data.ival);
	    break;
	default :
	    printf("  ERROR!\n");
	    break;
	}
	fflush(stdout);
    }
} /* print_program */

/*
 * tooltable.c
 * Copyright (C) 2004 dmitri (at) users.sourceforge.net
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

/** \file tooltable.c
    \brief Tool file parsing functions
    \ingroup libgerbv
*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN_TOOL_NUMBER         1       /* T01 */
#define MAX_TOOL_NUMBER         99      /* T99 */

static int have_tools_file = 0;
static double tools[1+MAX_TOOL_NUMBER];

static void 
ProcessToolLine(const char *cp)
{
    const char *cp0 = cp;
    int toolNumber;
    double toolDia;
    
    if (cp == NULL)
        return;
    
    /* Skip leading spaces if there are some */
    while (isspace((int) *cp)) {
        if (*(++cp) == '\0')
            return;
    }
    
    if (*cp != 'T') {
        fprintf(stderr, "*** WARNING: Strange tool \"%s\" ignored.\n", cp0);
        return;
    }
    if ((!isdigit((int) cp[1])) || (!isdigit((int) cp[2]))) {
        fprintf(stderr, "*** WARNING: No tool number in \"%s\".\n", cp0);
        return;
    }
    do {
        char tnb[3];
        tnb[0] = cp[1];
        tnb[1] = cp[2];
        tnb[2] = '\0';
        toolNumber = atoi(tnb);
        if ((toolNumber < MIN_TOOL_NUMBER) || (toolNumber > MAX_TOOL_NUMBER)) {
            fprintf(stderr, "*** WARNING: Can't parse tool number in \"%s\".\n", cp0);
            return;
        }
    } while (0);
    
    cp += 3; /* Skip Tnn */

    /* Skip following spaces if there are some */
    while (isspace((int) *cp)) {
        if (*(++cp) == '\0')
            return;
    }

    /* The rest of the line is supposed to be the tool diameter in inches. */
    toolDia = atof(cp);

    if (toolDia <= 0) {
        fprintf(stderr, "*** WARNING: Tool T%02d diameter is impossible.\n", toolNumber);
        return;
    }
    if (toolDia < 0.001) {
        fprintf(stderr, "*** WARNING: Tool T%02d diameter is very small - "
                "are you sure?\n", toolNumber);
    }
    
    if (tools[toolNumber] != 0) {
        fprintf(stderr, "*** ERROR: Tool T%02d is already defined.\n", toolNumber);
        fprintf(stderr, "*** Exiting because this is a HOLD error at any board house.\n");
        exit(1);
        return;
    }
    
    tools[toolNumber] = toolDia;
} /* ProcessToolLine */


int 
gerbv_process_tools_file(const char *tf)
{
    FILE *f;
    char buf[80];
    
    have_tools_file = 0;
    memset(tools, 0, sizeof(tools));
    
    if (tf == NULL)
        return 0;
    
    f = fopen(tf, "r");
    if (f == NULL) {
        fprintf(stderr, "*** ERROR: Failed to open file \"%s\" to read.\n", tf);
        return 0;
    }
    while (!feof(f)) {
        memset(buf, 0, sizeof(buf));
        if (NULL == fgets(buf, sizeof(buf)-1, f))
            break;
        ProcessToolLine(buf);
    }
    fclose(f);
    have_tools_file = 1;
    return 1;
} /* gerbv_process_tools_file */


double 
gerbv_get_tool_diameter(int toolNumber)
{
    if (!have_tools_file)
        return 0;
    if ((toolNumber < MIN_TOOL_NUMBER) || (toolNumber > MAX_TOOL_NUMBER))
        return 0;
    return tools[toolNumber];
} /* gerbv_get_tool_diameter */

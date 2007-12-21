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

#include <glib.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */
#include <stdlib.h>
#ifdef HAVE_STRING_H
#include "string.h"
#endif /* HAVE_STRING_H */

#include "setup.h"

/* The place this is set */

setup_t setup;

void 
setup_init(void)
{
    char *fontstring;

    memset((void *)&setup, 0, sizeof(setup_t));

    fontstring = (char *)g_malloc(strlen(DEF_DISTFONTNAME) + 1);
    if (fontstring == NULL)
	return;
    memset((void *)fontstring, 0, strlen(DEF_DISTFONTNAME) + 1);
    strncpy(fontstring, DEF_DISTFONTNAME, strlen(DEF_DISTFONTNAME));
    setup.dist_fontname = fontstring;

    fontstring = (char *)g_malloc(strlen(DEF_STATUSFONTNAME) + 1);
    if (fontstring == NULL)
	return;
    memset((void *)fontstring, 0, strlen(DEF_STATUSFONTNAME) + 1);
    strncpy(fontstring, DEF_STATUSFONTNAME, strlen(DEF_STATUSFONTNAME));
    setup.status_fontname = fontstring;

}


void
setup_destroy(void)
{
    if (setup.dist_fontname)
	g_free(setup.dist_fontname);
    if (setup.status_fontname)
	g_free(setup.status_fontname);
}

        /*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2004 Juergen Haas (juergenhaas@gmx.net)
 *
 * $Id$
 *	                      by Juergen Haas (juergenhaas@gmx.net)
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef USE_GTK2

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /*HAVE_UNISTD_H*/

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif /*HAVE_SYS_MMAN_H*/

#include "gerb_error.h"
#include "search_file.h"
#include "search_gui.h"

GtkListStore *completion_model;

pnp_file_t *
pnp_fopen(char *filename)
{

    pnp_file_t *fd;
    struct stat statinfo;
    int filenr;
    
    fd = (pnp_file_t *)malloc(sizeof(pnp_file_t));
    memset(fd,0,sizeof(pnp_file_t));
    if (fd == NULL) {
	return NULL;
    }
    fd->fd = fopen(filename, "r");
    if (fd->fd == NULL) {
        GERB_MESSAGE("FD was NULL");
	return NULL;
    }

    filenr = fileno(fd->fd);
    fstat(filenr, &statinfo);
    if (!S_ISREG(statinfo.st_mode)) {
	errno = EISDIR;
	return NULL;
    }
    if ((int)statinfo.st_size == 0) {
	errno = EIO; /* More compatible with the world outside Linux */
	return NULL;
    }

    fd->model = interface.model;

    return fd;
} /* pnp_fopen */

void
pnp_fclose(pnp_file_t *fd)
{
    if(fd == NULL)
        return;
    fclose(fd->fd);
    free(fd);
    
    return;
} /* pnp_fclose */

#endif /* USE_GTK2 */

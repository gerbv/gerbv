        /*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2000-2003 Stefan Petersen (spe@stacken.kth.se)
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
    
#ifdef HAVE_SYS_MMAN_H
    fd = (pnp_file_t *)malloc(sizeof(pnp_file_t));
    memset(fd,0,sizeof(pnp_file_t));
    if (fd == NULL) {
	return NULL;
    }
     fd->fd = fopen(filename, "r");
    if (fd->fd == NULL) {
	return NULL;
    }

    fd->ptr = 0;
    fd->fileno = fileno(fd->fd);
    fstat(fd->fileno, &statinfo);
    if (!S_ISREG(statinfo.st_mode)) {
	errno = EISDIR;
	return NULL;
    }
    if ((int)statinfo.st_size == 0) {
	errno = EIO; /* More compatible with the world outside Linux */
	return NULL;
    }
    fd->datalen = (int)statinfo.st_size;
    fd->data = (char *)mmap(0, statinfo.st_size, PROT_READ, MAP_PRIVATE, 
			    fd->fileno, 0);
    if(fd->data == MAP_FAILED) {
        fclose(fd->fd);
	    free(fd);
	    fd = NULL;
    }

    fd->model = interface.model;
    
#else
#if defined (__MINGW32__)

    fd = (pnp_file_t *)malloc(sizeof(pnp_file_t));
    memset(fd, 0, sizeof(pnp_file_t));
    if (fd == NULL) {
        GERB_MESSAGE("FD was NULL");
	return NULL;
    }
    
    fd->fd = fopen(filename, "r");
    if (fd->fd == NULL) {
        return NULL;
    }
    fd->ptr = 0;
    fd->fileno = fileno(fd->fd);
    fstat(fd->fileno, &statinfo);
    if (!S_ISREG(statinfo.st_mode)) {
	errno = EISDIR;
	return NULL;
    }
    if ((int)statinfo.st_size == 0) {
	errno = EIO; /* More compatible with the world outside Linux */
	return NULL;
    }
    fd->datalen = (int)statinfo.st_size;
    fd->data = calloc(1, statinfo.st_size + 1);
    if (fd->data == NULL) {
        fclose(fd->fd);
	free(fd);
	fd = NULL;
    }
   // fread((void*)fd->data, 1, statinfo.st_size, fd->fd);
    fd->model  =    interface.model;

  
   
    #endif   
#endif
    return fd;
} /* pnp_fopen */


int
pnp_fgetc(pnp_file_t *fd)
{

    if (fd->ptr > fd->datalen || fd->datalen == 0)
	return EOF;

    return (int) fd->data[fd->ptr++];
} /* pnp_fgetc */


int
pnp_fgetint(pnp_file_t *fd)
{
    long int result;
    char *end;
    
    result = strtol(fd->data + fd->ptr, &end, 10);
    fd->ptr = end - fd->data;

    return (int)result;
} /* pnp_fgetint */


double
pnp_fgetdouble(pnp_file_t *fd)
{
    double result;
    char *end;
    
    result = strtod(fd->data + fd->ptr, &end);
    fd->ptr = end - fd->data;

    return result;
} /* pnp_fgetdouble */


char *
pnp_fgetstring(pnp_file_t *fd, char term)
{
    char *strend;
    char *newstr;
    int len;
    
    strend = strchr(fd->data + fd->ptr, term);
    if (strend == NULL)
	return NULL;

    len = strend - (fd->data + fd->ptr);

    newstr = (char *)malloc(len + 1);
    if (newstr == NULL)
	return NULL;
    strncpy(newstr, fd->data + fd->ptr, len);
    newstr[len] = '\0';
    fd->ptr += len;

    return newstr;
} /* pnp_fgetstring */


void 
pnp_ungetc(pnp_file_t *fd)
{
    if (fd->ptr)
	fd->ptr--;

    return;
} /* pnp_ungetc */


void
pnp_fclose(pnp_file_t *fd)
{
#ifdef HAVE_MMAN_H
    munmap(fd->data, fd->datalen);
#endif    
    fclose(fd->fd);
    free(fd);
    
    return;
} /* pnp_fclose */


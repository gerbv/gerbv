/*
 * gEDA - GNU Electronic Design Automation
 * This file is a part of gerbv.
 *
 *   Copyright (C) 2000-2001 Stefan Petersen (spe@stacken.kth.se)
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/mman.h>

#include "gerb_file.h"

gerb_file_t *
gerb_fopen(char *filename)
{
    gerb_file_t *fd;
    struct stat statinfo;

    fd = (gerb_file_t *)malloc(sizeof(gerb_file_t));
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
    fd->datalen = (int)statinfo.st_size;
    fd->data = (char *)mmap(0, statinfo.st_size, PROT_READ, MAP_PRIVATE, 
			    fd->fileno, 0);
    if(fd->data == MAP_FAILED) {
	/* Failed to memory map file.
	   Probable cause is that it is a directory. */
	fclose(fd->fd);
	free(fd);
	fd = NULL;
    }

    return fd;
}


char 
gerb_fgetc(gerb_file_t *fd)
{
    char data;

    if (fd->ptr > fd->datalen || fd->datalen == 0)
	return EOF;
    data = fd->data[fd->ptr++];

    return data;
}


int
gerb_fgetint(gerb_file_t *fd)
{
    long int result;
    char *end;
    
    result = strtol(fd->data + fd->ptr, &end, 10);
    fd->ptr = end - fd->data;

    return (int)result;
}


double
gerb_fgetdouble(gerb_file_t *fd)
{
    double result;
    char *end;
    
    result = strtod(fd->data + fd->ptr, &end);
    fd->ptr = end - fd->data;

    return result;
}


char *
gerb_fgetstring(gerb_file_t *fd, char term)
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
}


void 
gerb_ungetc(gerb_file_t *fd)
{
    if (fd->ptr)
	fd->ptr--;

    return;
}


void
gerb_fclose(gerb_file_t *fd)
{
    munmap(fd->data, fd->datalen);
    fclose(fd->fd);
    free(fd);
    
    return;
}

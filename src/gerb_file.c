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

#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/mman.h>

#include "fileio.h"

gerb_file_t *
gerb_fopen(char *filename)
{
    gerb_file_t *fd;
    struct stat statinfo;

    fd = (gerb_file_t *)malloc(sizeof(gerb_file_t));
    if (fd == NULL) {
	perror("gerb_fopen");
	return NULL;
    }

    fd->fd = fopen(filename, "r");
    if (fd == NULL) {
	perror("gerb_fopen");
	return NULL;
    }

    fd->fileno = fileno(fd->fd);
    fstat(fd->fileno, &statinfo);
    fd->datalen = (int)statinfo.st_size;
    fd->data = (char *)mmap(0, statinfo.st_size, PROT_READ, MAP_PRIVATE, 
			    fd->fileno, 0);
    fd->ptr = 0;

    return fd;
}


char 
gerb_fgetc(gerb_file_t *fd)
{
    char data;

    if (fd->ptr > fd->datalen)
	return EOF;
    data = fd->data[fd->ptr++];

    return data;
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

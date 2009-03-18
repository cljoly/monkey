/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*  Monkey HTTP Daemon
 *  ------------------
 *  Copyright (C) 2001-2008, Eduardo Silva P.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>

#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <unistd.h>
#include <signal.h>
#include <sys/sendfile.h>

#include <time.h>

#include "monkey.h"
#include "memory.h"
#include "utils.h"
#include "str.h"
#include "config.h"
#include "chars.h"
#include "socket.h"
#include "clock.h"

int SendFile(int socket, struct request *sr)
{
	long int nbytes=0;

	nbytes = sendfile(socket, sr->fd_file, &sr->bytes_offset,
			sr->bytes_to_send);

	if (nbytes == -1) {
		fprintf(stderr, "error from sendfile: %s\n", strerror(errno));
		return -1;
	}
	else
	{
		sr->bytes_to_send-=nbytes;
	}
	return sr->bytes_to_send;
}

/* It's a valid directory ? */
int CheckDir(char *pathfile)
{
	struct stat path;

	if(stat(pathfile,&path)==-1)
		return -1;
		
	if(!(path.st_mode & S_IFDIR))
		return -1;
		
	return 0;
}

int CheckFile(char *pathfile)
{
	struct stat path;

	if(stat(pathfile,&path)==-1)
		return -1;
		
	if(!(path.st_mode & S_IFREG))
		return -1;
		
	return 0;
}

/* Devuelve la fecha para enviarla 
 en el header */
mk_pointer PutDate_string(time_t date)
{
	int n, size=50;
        mk_pointer date_gmt;
	struct tm *gmt_tm;
	
        mk_pointer_reset(&date_gmt);

	if(date==0){
		if ( (date = time(NULL)) == -1 ){
                        return date_gmt;
		}
	}

        date_gmt.data = mk_mem_malloc(size);
	gmt_tm	= (struct tm *) gmtime(&date);
	n = strftime(date_gmt.data, size-1,  GMT_DATEFORMAT, gmt_tm);
	date_gmt.data[n] = '\0';
	date_gmt.len = n;

        return date_gmt;
}

time_t PutDate_unix(char *date)
{
	time_t new_unix_time;
	struct tm t_data;
	
	if(!strptime(date, GMT_DATEFORMAT, (struct tm *) &t_data)){
		return -1;
	}

	new_unix_time = mktime((struct tm *) &t_data);

	return (new_unix_time);
}

int mk_buffer_cat(mk_pointer *p, char *buf1, char *buf2){

        int len1, len2;

        len1 = strlen(buf1);
        len2 = strlen(buf2);

        /* alloc space */
        p->data = (char *) mk_mem_malloc(len1+len2+1);

        /* copy data */
        strncpy(p->data, buf1, len1);
        strncpy(p->data+len1, buf2, len2);
        p->data[len1+len2]='\0';

        /* assign len */
        p->len = len1+len2;

        return 0;
}

char *m_build_buffer(char **buffer, unsigned long *len, const char *format, ...)
{
	va_list	ap;
	int length;
	char *ptr;
	static size_t _mem_alloc = 64;
	size_t alloc = 0;
	
	/* *buffer *must* be an empty/NULL buffer */

	*buffer = (char *) mk_mem_malloc(_mem_alloc);
	if(!*buffer)
	{
		return NULL;
	}
	alloc = _mem_alloc;
	
	va_start(ap, format);
	length = vsnprintf(*buffer, alloc, format, ap);
	
	if(length >= alloc) {
		ptr = realloc(*buffer, length + 1);
		if(!ptr) {
			va_end(ap);
			return NULL;
		}
		*buffer = ptr;
		alloc = length + 1;
		length = vsnprintf(*buffer, alloc, format, ap);
	}
	va_end(ap);

	if(length<0){
		return NULL;
	}

	ptr = *buffer;
	ptr[length] = '\0';
	*len = length;
	
	return *buffer;
}

/* Run current process in background mode (daemon, evil Monkey >:) */
int mk_utils_set_daemon()
{
	 switch (fork())
  	  {
		case 0 : break;
		case -1: exit(1); break; /* Error */
		default: exit(0); /* Success */
	  };

	  setsid(); /* Create new session */
	  fclose(stdin); /* close screen outputs */
	  fclose(stderr);
	  fclose(stdout);

	return 0;
}


char *get_real_string(mk_pointer uri){

        int i, hex_result, aux_char;
        int buf_idx=0;
        char *buf;
        char hex[3];

	if((i = mk_string_char_search(uri.data, '%', uri.len))<0)
	{
		return NULL;
	}

        buf = mk_mem_malloc_z(uri.len);


        if(i>0){
                strncpy(buf, uri.data, i);
                buf_idx = i;
        }

        while(i<uri.len)
        {
                if(uri.data[i]=='%' && i+2<uri.len){
                        memset(hex, '\0', sizeof(hex));
                        strncpy(hex, uri.data+i+1,2);
                        hex[2]='\0';

			if((hex_result=hex2int(hex))<=127){
				buf[buf_idx]=toascii(hex_result);
			}
			else {
				if((aux_char=get_char(hex_result))!=-1){
					buf[buf_idx]=aux_char;
				}
				else{
					mk_mem_free(buf);
					return NULL;
				}
			}
                        i+=2;
                }
                else{
                        buf[buf_idx] = uri.data[i];
                }
                i++;
                buf_idx++;
        }        
        buf[buf_idx]='\0';

        return (char *) buf;
}

void mk_utils_toupper(char *string)
{
        int i, len;
        
        len = strlen(string);
        for(i=0; i<len; i++)
        {
                string[i] = toupper(string[i]);
        }
}

mk_pointer mk_utils_int2mkp(int n)
{
        mk_pointer p;
        char *buf;
        unsigned long len;
        int size = 32;

        buf = mk_mem_malloc(size);
        len = snprintf(buf, 32, "%i", n);

        p.data = buf;
        p.len = len;

        return p;
}


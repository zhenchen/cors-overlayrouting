/*
** Copyright (C) 2006 Li Tang <tangli99@gmail.com>
**  
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**  
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**  
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**  
*/

#ifndef __UTILS_DEBUG_H
#define __UTILS_DEBUG_H

#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define DEBUG_ENABLED
#define DEBUG_LEVEL 1
// #define NBMTR_INTEST
// #define RLDVR_INTEST
// #define RSLTR_INTEST

#define CHOP(str) \
    if (str[strlen(str)-1]=='\n' || str[strlen(str)-1]=='\r') \
	str[strlen(str)-1]='\0';

#ifdef DEBUG_ENABLED
#define DEBUG_PRINT(level, msg, ... ) \
    if (level <= DEBUG_LEVEL) { \
        time_t tmp = time(NULL); \
        char * ctmp = ctime(&tmp); \
        CHOP(ctmp); \
        printf("[%s: Line:%d in File:%s] ", ctmp, __LINE__, __FILE__); \
        printf(msg, ## __VA_ARGS__); \
    }

#else
#define DEBUG_PRINT(level, msg, ... )
#endif

#endif

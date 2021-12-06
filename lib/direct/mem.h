/*
   This file is part of DirectFB.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
*/

#ifndef __DIRECT__MEM_H__
#define __DIRECT__MEM_H__

#include <direct/os/mem.h>

/**********************************************************************************************************************/

void DIRECT_API  direct_print_memleaks( void );

void DIRECT_API *direct_dbg_malloc    ( const char *file,
                                        int         line,
                                        const char *func,
                                        size_t      bytes );

void DIRECT_API *direct_dbg_calloc    ( const char *file,
                                        int         line,
                                        const char *func,
                                        size_t      count,
                                        size_t      bytes );

void DIRECT_API *direct_dbg_realloc   ( const char *file,
                                        int         line,
                                        const char *func,
                                        const char *what,
                                        void       *mem,
                                        size_t      bytes );

char DIRECT_API *direct_dbg_strdup    ( const char *file,
                                        int         line,
                                        const char *func,
                                        const char *str );

void DIRECT_API  direct_dbg_free      ( const char *file,
                                        int         line,
                                        const char *func,
                                        const char *what,
                                        void       *mem );

/**********************************************************************************************************************/

#if DIRECT_BUILD_DEBUG || defined(DIRECT_ENABLE_DEBUG)

#define D_MALLOC(bytes)       direct_dbg_malloc ( __FILE__, __LINE__, __FUNCTION__, bytes )
#define D_CALLOC(count,bytes) direct_dbg_calloc ( __FILE__, __LINE__, __FUNCTION__, count, bytes )
#define D_REALLOC(mem,bytes)  direct_dbg_realloc( __FILE__, __LINE__, __FUNCTION__, #mem, mem, bytes )
#define D_STRDUP(str)         direct_dbg_strdup ( __FILE__, __LINE__, __FUNCTION__, str )
#define D_FREE(mem)           direct_dbg_free   ( __FILE__, __LINE__, __FUNCTION__, #mem, mem )

#else /* DIRECT_BUILD_DEBUG || defined(DIRECT_ENABLE_DEBUG) */

#define D_MALLOC              direct_malloc
#define D_CALLOC              direct_calloc
#define D_REALLOC             direct_realloc
#define D_STRDUP              direct_strdup
#define D_FREE                direct_free

#endif /* DIRECT_BUILD_DEBUG || defined(DIRECT_ENABLE_DEBUG) */

void __D_mem_init  ( void );
void __D_mem_deinit( void );

#endif

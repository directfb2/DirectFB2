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

#ifndef __FUSION__SHMALLOC_H__
#define __FUSION__SHMALLOC_H__

#include <fusion/types.h>

/**********************************************************************************************************************/

#if FUSION_BUILD_MULTI

void FUSION_API  fusion_print_madvise     ( void );

void FUSION_API  fusion_print_memleaks    ( FusionSHMPoolShared *pool );

#endif /* FUSION_BUILD_MULTI */

void FUSION_API *fusion_dbg_shmalloc      ( FusionSHMPoolShared *pool,
                                            const char          *file,
                                            int                  line,
                                            const char          *func,
                                            size_t               size );

void FUSION_API *fusion_dbg_shcalloc      ( FusionSHMPoolShared *pool,
                                            const char          *file,
                                            int                  line,
                                            const char          *func,
                                            size_t               nmemb,
                                            size_t               size );

void FUSION_API *fusion_dbg_shrealloc     ( FusionSHMPoolShared *pool,
                                            const char          *file,
                                            int                  line,
                                            const char          *func,
                                            const char          *what,
                                            void                *ptr,
                                            size_t               size );

char FUSION_API *fusion_dbg_shstrdup      ( FusionSHMPoolShared *pool,
                                            const char          *file,
                                            int                  line,
                                            const char          *func,
                                            const char          *string );

void FUSION_API  fusion_dbg_shfree        ( FusionSHMPoolShared *pool,
                                            const char          *file,
                                            int                  line,
                                            const char          *func,
                                            const char          *what,
                                            void                *ptr );

void FUSION_API *fusion_shmalloc          ( FusionSHMPoolShared *pool,
                                            size_t               size );

void FUSION_API *fusion_shcalloc          ( FusionSHMPoolShared *pool,
                                            size_t               nmemb,
                                            size_t               size );

void FUSION_API *fusion_shrealloc         ( FusionSHMPoolShared *pool,
                                            void                *ptr,
                                            size_t               size );

char FUSION_API *fusion_shstrdup          ( FusionSHMPoolShared *pool,
                                            const char          *string );

void FUSION_API  fusion_shfree            ( FusionSHMPoolShared *pool,
                                            void                *ptr );

/**********************************************************************************************************************/

#if DIRECT_BUILD_DEBUG || defined(DIRECT_ENABLE_DEBUG)

#define SHMALLOC(pool,bytes)       fusion_dbg_shmalloc ( pool, __FILE__, __LINE__, __FUNCTION__, bytes )
#define SHCALLOC(pool,count,bytes) fusion_dbg_shcalloc ( pool, __FILE__, __LINE__, __FUNCTION__, count, bytes )
#define SHREALLOC(pool,mem,bytes)  fusion_dbg_shrealloc( pool, __FILE__, __LINE__, __FUNCTION__, #mem, mem, bytes )
#define SHSTRDUP(pool,string)      fusion_dbg_shstrdup ( pool, __FILE__, __LINE__, __FUNCTION__, string )
#define SHFREE(pool,mem)           fusion_dbg_shfree   ( pool, __FILE__, __LINE__, __FUNCTION__, #mem, mem )

#else /* DIRECT_BUILD_DEBUG || defined(DIRECT_ENABLE_DEBUG) */

#define SHMALLOC                   fusion_shmalloc
#define SHCALLOC                   fusion_shcalloc
#define SHREALLOC                  fusion_shrealloc
#define SHSTRDUP                   fusion_shstrdup
#define SHFREE                     fusion_shfree

#endif /* DIRECT_BUILD_DEBUG || defined(DIRECT_ENABLE_DEBUG) */

#endif

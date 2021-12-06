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

#ifndef __DIRECT__MAP_H__
#define __DIRECT__MAP_H__

#include <direct/types.h>

/**********************************************************************************************************************/

typedef bool                    (*DirectMapCompareFunc) ( DirectMap *map, const void *key, void *object, void *ctx );

typedef unsigned int            (*DirectMapHashFunc)    ( DirectMap *map, const void *key, void *ctx );

typedef DirectEnumerationResult (*DirectMapIteratorFunc)( DirectMap *map, void *object, void *ctx );

/**********************************************************************************************************************/

DirectResult DIRECT_API  direct_map_create ( unsigned int            initial_size,
                                             DirectMapCompareFunc    compare_func,
                                             DirectMapHashFunc       hash_func,
                                             void                   *ctx,
                                             DirectMap             **ret_map );

void         DIRECT_API  direct_map_destroy( DirectMap              *map );

DirectResult DIRECT_API  direct_map_insert ( DirectMap              *map,
                                             const void             *key,
                                             void                   *object );

DirectResult DIRECT_API  direct_map_remove ( DirectMap              *map,
                                             const void             *key );

void         DIRECT_API *direct_map_lookup ( DirectMap              *map,
                                             const void             *key );

void         DIRECT_API  direct_map_iterate( DirectMap              *map,
                                             DirectMapIteratorFunc   func,
                                             void                   *ctx );

#endif

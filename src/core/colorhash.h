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

#ifndef __CORE__COLORHASH_H__
#define __CORE__COLORHASH_H__

#include <core/coretypes.h>
#include <direct/mutex.h>

/**********************************************************************************************************************/

typedef struct {
     unsigned int pixel;
     unsigned int index;
     u32          palette_id;
} Colorhash;

typedef struct {
     int magic;
} DFBColorHashCoreShared;

typedef struct {
     int                     magic;

     CoreDFB                *core;

     DFBColorHashCoreShared *shared;

     Colorhash              *hash;
     DirectMutex             hash_lock;
} DFBColorHashCore;

/**********************************************************************************************************************/

unsigned int dfb_colorhash_lookup    ( DFBColorHashCore *core,
                                       CorePalette      *palette,
                                       u8                r,
                                       u8                g,
                                       u8                b,
                                       u8                a );

void         dfb_colorhash_invalidate( DFBColorHashCore *core,
                                       CorePalette      *palette );

#endif

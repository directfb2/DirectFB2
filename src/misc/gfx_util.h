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

#ifndef __MISC__GFX_UTIL_H__
#define __MISC__GFX_UTIL_H__

#include <core/coretypes.h>

/**********************************************************************************************************************/

void dfb_copy_buffer_32 ( u32             *src,
                          void            *dst,
                          int              dpitch,
                          DFBRectangle    *drect,
                          CoreSurface     *dst_surface,
                          const DFBRegion *dst_clip );

void dfb_scale_linear_32( u32             *src,
                          int              sw,
                          int              sh,
                          void            *dst,
                          int              dpitch,
                          DFBRectangle    *drect,
                          CoreSurface     *dst_surface,
                          const DFBRegion *dst_clip );

#endif

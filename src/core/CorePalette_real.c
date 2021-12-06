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

#include <core/CorePalette.h>
#include <core/palette.h>
#include <direct/memcpy.h>
#include <gfx/convert.h>

D_DEBUG_DOMAIN( DirectFB_CorePalette, "DirectFB/CorePalette", "DirectFB CorePalette" );

/**********************************************************************************************************************/

DFBResult
IPalette_Real__SetEntries( CorePalette    *obj,
                           const DFBColor *colors,
                           u32             num,
                           u32             offset )
{
     D_DEBUG_AT( DirectFB_CorePalette, "%s( %p )\n", __FUNCTION__, obj );

     D_ASSERT( colors != NULL );

     if (offset + num > obj->num_entries)
          return DFB_INVARG;

     if (num) {
          direct_memcpy( obj->entries + offset, colors, num * sizeof(DFBColor) );

          for (u32 i = offset; i < offset + num; i++) {
               obj->entries_yuv[i].a = obj->entries[i].a;

               RGB_TO_YCBCR( obj->entries[i].r, obj->entries[i].g, obj->entries[i].b,
                             obj->entries_yuv[i].y, obj->entries_yuv[i].u, obj->entries_yuv[i].v );
          }

          dfb_palette_update( obj, offset, offset + num - 1 );
     }

     return DFB_OK;
}

DFBResult
IPalette_Real__SetEntriesYUV( CorePalette       *obj,
                              const DFBColorYUV *colors,
                              u32                num,
                              u32                offset )
{
     D_DEBUG_AT( DirectFB_CorePalette, "%s( %p )\n", __FUNCTION__, obj );

     D_ASSERT( colors != NULL );

     if (offset + num > obj->num_entries)
          return DFB_INVARG;

     if (num) {
          direct_memcpy( obj->entries_yuv + offset, colors, num * sizeof(DFBColorYUV) );

          for (u32 i = offset; i < offset + num; i++) {
               obj->entries[i].a = obj->entries_yuv[i].a;

               YCBCR_TO_RGB( obj->entries_yuv[i].y, obj->entries_yuv[i].u, obj->entries_yuv[i].v,
                             obj->entries[i].r, obj->entries[i].g, obj->entries[i].b );
          }

          dfb_palette_update( obj, offset, offset + num - 1 );
     }

     return DFB_OK;
}

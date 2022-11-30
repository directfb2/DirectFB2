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

#ifndef __CORE__PALETTE_H__
#define __CORE__PALETTE_H__

#include <core/coretypes.h>
#include <fusion/object.h>

/**********************************************************************************************************************/

struct __DFB_CorePalette {
     FusionObject          object;
     int                   magic;

     unsigned int          num_entries;
     DFBColor             *entries;
     DFBColorYUV          *entries_yuv;
     DFBSurfaceColorSpace  colorspace;

     FusionSHMPoolShared  *shmpool;

     FusionCall           call;
};

/**********************************************************************************************************************/

typedef enum {
     CPNF_ENTRIES = 0x00000001,
     CPNF_DESTROY = 0x00000002
} CorePaletteNotificationFlags;

typedef struct {
     CorePaletteNotificationFlags  flags;
     CorePalette                  *palette;
     int                           first;
     int                           last;
} CorePaletteNotification;

/**********************************************************************************************************************/

/*
 * Creates a pool of palette objects.
 */
FusionObjectPool *dfb_palette_pool_create        ( const FusionWorld *world );

/*
 * Generates dfb_palette_ref(), dfb_palette_attach() etc.
 */
FUSION_OBJECT_METHODS( CorePalette, dfb_palette )

typedef enum {
     DFB_SURFACE_PALETTE_LISTENER = 0x00000000
} DFB_PALETTE_GLOBALS;

/**********************************************************************************************************************/

DFBResult    dfb_palette_create             ( CoreDFB               *core,
                                              unsigned int           size,
                                              DFBSurfaceColorSpace   colorspace,
                                              CorePalette          **ret_palette );

void         dfb_palette_generate_rgb332_map( CorePalette           *palette );

void         dfb_palette_generate_rgb121_map( CorePalette           *palette );

unsigned int dfb_palette_search             ( CorePalette           *palette,
                                              u8                     r,
                                              u8                     g,
                                              u8                     b,
                                              u8                     a );

void         dfb_palette_update             ( CorePalette           *palette,
                                              int                    first,
                                              int                    last );

bool         dfb_palette_equal              ( CorePalette           *palette1,
                                              CorePalette           *palette2 );

#endif

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

#include <core/CoreLayerRegion.h>
#include <core/layer_region.h>

D_DEBUG_DOMAIN( DirectFB_CoreLayerRegion, "DirectFB/CoreLayerRegion", "DirectFB CoreLayerRegion" );

/**********************************************************************************************************************/

DFBResult
ILayerRegion_Real__GetSurface( CoreLayerRegion  *obj,
                               CoreSurface     **ret_surface )
{
     D_DEBUG_AT( DirectFB_CoreLayerRegion, "%s( %p )\n", __FUNCTION__, obj );

     return dfb_layer_region_get_surface( obj, ret_surface );
}

DFBResult
ILayerRegion_Real__FlipUpdate2( CoreLayerRegion     *obj,
                                const DFBRegion     *left_update,
                                const DFBRegion     *right_update,
                                DFBSurfaceFlipFlags  flags,
                                u32                  flip_count,
                                s64                  pts )
{
     D_DEBUG_AT( DirectFB_CoreLayerRegion, "%s( %p, flags 0x%08x, flip_count %u, pts %lld )\n", __FUNCTION__,
                 obj, flags, flip_count, (long long) pts );

     return dfb_layer_region_flip_update2( obj, left_update, right_update, flags, flip_count, pts );
}

DFBResult
ILayerRegion_Real__SetSurface( CoreLayerRegion *obj,
                               CoreSurface     *surface )
{
     D_DEBUG_AT( DirectFB_CoreLayerRegion, "%s( %p, surface %p )\n", __FUNCTION__, obj, surface );

     D_ASSERT( surface != NULL );

     dfb_layer_region_set_surface( obj, surface, true );

     return DFB_OK;
}

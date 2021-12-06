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

#include <core/CoreSurfaceClient.h>
#include <core/surface.h>
#include <core/surface_client.h>

D_DEBUG_DOMAIN( DirectFB_CoreSurfaceClient, "DirectFB/CoreSurfaceClient", "DirectFB CoreSurfaceClient" );

/**********************************************************************************************************************/

DFBResult
ISurfaceClient_Real__FrameAck( CoreSurfaceClient *obj,
                               u32                flip_count )
{
     CoreSurface *surface;

     D_DEBUG_AT( DirectFB_CoreSurfaceClient, "%s( %p, count %u ) <- old count %u\n", __FUNCTION__,
                 obj, flip_count, obj->flip_count );

     D_MAGIC_ASSERT( obj->surface, CoreSurface );

     surface = obj->surface;

     D_DEBUG_AT( DirectFB_CoreSurfaceClient, "  -> surface %p (id %u)\n", surface, surface->object.id );

     dfb_surface_lock( surface );

     obj->flip_count = flip_count;

     dfb_surface_check_acks( surface );

     dfb_surface_unlock( surface );

     return DFB_OK;
}

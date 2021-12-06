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

#include <core/CoreSurfaceAllocation.h>
#include <core/core.h>
#include <core/surface_allocation.h>
#include <core/surface_buffer.h>

D_DEBUG_DOMAIN( DirectFB_CoreSurfaceAllocation, "DirectFB/CoreSurfaceAllocation", "DirectFB CoreSurfaceAllocation" );

/**********************************************************************************************************************/

DFBResult
ISurfaceAllocation_Real__Updated( CoreSurfaceAllocation *obj,
                                  const DFBBox          *updates,
                                  u32                    num_updates )
{
     DFBResult          ret;
     CoreSurfaceBuffer *buffer;

     D_DEBUG_AT( DirectFB_CoreSurfaceAllocation, "%s( %p, updates %p, num %u )\n", __FUNCTION__,
                 obj, updates, num_updates );

     ret = fusion_object_get( core_dfb->shared->surface_buffer_pool, obj->buffer_id, (FusionObject**) &buffer );
     if (ret && ret != DFB_DEAD)
          return ret;

     if (ret == DFB_DEAD) {
          D_DEBUG_AT( DirectFB_CoreSurfaceAllocation, "  -> dead object!\n" );
     }
     else {
          CoreSurface *surface;

          ret = fusion_object_get( core_dfb->shared->surface_pool, buffer->surface_id, (FusionObject**) &surface );
          if (ret && ret != DFB_DEAD) {
               dfb_surface_buffer_unref( buffer );
               return ret;
          }

          if (ret == DFB_DEAD) {
               D_DEBUG_AT( DirectFB_CoreSurfaceAllocation, "  -> dead object!\n" );
          }
          else {
               dfb_surface_lock( surface );

               if (obj->buffer) {
                    D_ASSERT( obj->buffer == buffer );

                    D_DEBUG_AT( DirectFB_CoreSurfaceAllocation, "  <- buffer  %p\n", buffer );
                    D_DEBUG_AT( DirectFB_CoreSurfaceAllocation, "  <- written %p\n", buffer->written );
                    D_DEBUG_AT( DirectFB_CoreSurfaceAllocation, "  <- read    %p\n", buffer->read );
                    D_DEBUG_AT( DirectFB_CoreSurfaceAllocation, "  <- serial  %lu (this %lu)\n", buffer->serial.value,
                                obj->serial.value );

                    direct_serial_increase( &buffer->serial );

                    direct_serial_copy( &obj->serial, &buffer->serial );

                    buffer->written = obj;
                    buffer->read    = NULL;

                    D_DEBUG_AT( DirectFB_CoreSurfaceAllocation, "  -> serial  %lu\n", buffer->serial.value );
               }
               else {
                    D_DEBUG_AT( DirectFB_CoreSurfaceAllocation, "  -> already decoupled!\n" );
               }

               dfb_surface_unlock( surface );

               dfb_surface_unref( surface );
          }

          dfb_surface_buffer_unref( buffer );
     }

     return DFB_OK;
}

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

#include <core/CoreSlave.h>
#include <core/core.h>
#include <core/surface_allocation.h>
#include <core/surface_buffer.h>
#include <core/surface_core.h>
#include <core/surface_pool_bridge.h>

D_DEBUG_DOMAIN( PreAlloc_Bridge, "Core/PreAlloc/Bridge", "DirectFB Core PreAlloc Surface Pool Bridge" );

/**********************************************************************************************************************/

typedef struct {
     CoreSurfacePool *shared_pool;
     CoreSurfacePool *prealloc_pool;
} preallocPoolBridgeData;

/**********************************************************************************************************************/

static int
preallocPoolBridgeDataSize()
{
     return sizeof(preallocPoolBridgeData);
}

static DFBResult
preallocInitPoolBridge( CoreDFB                          *core,
                        CoreSurfacePoolBridge            *bridge,
                        void                             *bridge_data,
                        void                             *bridge_local,
                        void                             *ctx,
                        CoreSurfacePoolBridgeDescription *ret_desc )
{
     preallocPoolBridgeData *data = bridge_data;
     DFBSurfaceCore         *sc   = ctx;
     DFBSurfaceCoreShared   *shared;

     D_DEBUG_AT( PreAlloc_Bridge, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( bridge, CoreSurfacePoolBridge );
     D_ASSERT( bridge_data != NULL );
     D_ASSERT( sc != NULL );
     D_ASSERT( sc->shared != NULL );
     D_ASSERT( ret_desc != NULL );

     shared = sc->shared;

     ret_desc->caps = CSPBCAPS_NONE;

     snprintf( ret_desc->name, DFB_SURFACE_POOL_BRIDGE_DESC_NAME_LENGTH, "PreAlloc Pool Bridge" );

     data->shared_pool   = shared->surface_pool;
     data->prealloc_pool = shared->prealloc_pool;

     return DFB_OK;
}

static DFBResult
preallocJoinPoolBridge( CoreDFB               *core,
                        CoreSurfacePoolBridge *bridge,
                        void                  *bridge_data,
                        void                  *bridge_local,
                        void                  *ctx )
{
     D_DEBUG_AT( PreAlloc_Bridge, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( bridge, CoreSurfacePoolBridge );
     D_ASSERT( bridge_data != NULL );
     D_ASSERT( ctx != NULL );

     return DFB_OK;
}

static DFBResult
preallocDestroyPoolBridge( CoreSurfacePoolBridge *bridge,
                           void                  *bridge_data,
                           void                  *bridge_local )
{
     D_DEBUG_AT( PreAlloc_Bridge, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( bridge, CoreSurfacePoolBridge );

     return DFB_OK;
}

static DFBResult
preallocLeavePoolBridge( CoreSurfacePoolBridge *bridge,
                         void                  *bridge_data,
                         void                  *bridge_local )
{
     D_DEBUG_AT( PreAlloc_Bridge, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( bridge, CoreSurfacePoolBridge );

     return DFB_OK;
}

static DFBResult
preallocCheckTransfer( CoreSurfacePoolBridge *bridge,
                       void                  *bridge_data,
                       void                  *bridge_local,
                       CoreSurfaceBuffer     *buffer,
                       CoreSurfaceAllocation *from,
                       CoreSurfaceAllocation *to )
{
     preallocPoolBridgeData *data = bridge_data;

     D_DEBUG_AT( PreAlloc_Bridge, "%s()\n", __FUNCTION__ );

     D_ASSERT( bridge_data != NULL );

     if (!Core_Resource_GetSlave( buffer->surface->object.identity )) {
          D_DEBUG_AT( PreAlloc_Bridge, "  -> no slave\n" );
          return DFB_NOIMPL;
     }

     if (from->pool == data->prealloc_pool) {
          D_DEBUG_AT( PreAlloc_Bridge, "  -> from preallocated\n" );
          return DFB_OK;
     }

     if (to->pool == data->prealloc_pool) {
          D_DEBUG_AT( PreAlloc_Bridge, "  -> to preallocated\n" );
          return DFB_OK;
     }

     return DFB_UNSUPPORTED;
}

static DFBResult
prealloc_transfer_locked( CoreSurface             *surface,
                          CoreSurfacePoolTransfer *transfer,
                          CoreSurfaceAllocation   *locked,
                          CoreSurfaceAccessFlags   flags,
                          CoreSlave               *slave )
{
     DFBResult             ret;
     CoreSurfaceBufferLock lock;
     int                   index;
     int                   i, y;

     D_MAGIC_ASSERT( surface, CoreSurface );
     D_ASSERT( transfer != NULL );

     CORE_SURFACE_ALLOCATION_ASSERT( locked );

     index = dfb_surface_buffer_index( locked->buffer );

     D_DEBUG_AT( PreAlloc_Bridge, "%s()\n", __FUNCTION__ );

     D_DEBUG_AT( PreAlloc_Bridge, "  -> transfer locked %s Fusion ID %lu (index %d)\n",
                 (flags & CSAF_WRITE) ? "from" : "to", surface->object.identity, index );

     dfb_surface_buffer_lock_init( &lock, CSAID_CPU, flags );

     ret = dfb_surface_pool_lock( locked->pool, locked, &lock );
     if (ret) {
          dfb_surface_buffer_lock_deinit( &lock );
          return ret;
     }

     for (i = 0; i < transfer->num_rects; i++) {
          const DFBRectangle *rect   = &transfer->rects[i];
          int                 offset = DFB_BYTES_PER_LINE( surface->config.format, rect->x );
          int                 length = DFB_BYTES_PER_LINE( surface->config.format, rect->w );

          for (y = 0; y < rect->h; y++) {
               if (flags & CSAF_WRITE)
                    ret = CoreSlave_GetData( slave,
                                             surface->config.preallocated[index].addr +
                                             (rect->y + y) * surface->config.preallocated[index].pitch + offset, length,
                                             lock.addr + (rect->y + y) * lock.pitch + offset );
               else
                    ret = CoreSlave_PutData( slave,
                                             surface->config.preallocated[index].addr +
                                             (rect->y + y) * surface->config.preallocated[index].pitch + offset, length,
                                             lock.addr + (rect->y + y) * lock.pitch + offset );
               if (ret)
                    break;
          }

          if (ret)
               break;
     }

     dfb_surface_pool_unlock( locked->pool, locked, &lock );

     dfb_surface_buffer_lock_deinit( &lock );

     return ret;
}

static DFBResult
prealloc_transfer_readwrite( CoreSurface             *surface,
                             CoreSurfacePoolTransfer *transfer,
                             CoreSurfaceAllocation   *allocation,
                             CoreSurfaceAccessFlags   flags,
                             CoreSlave               *slave )
{
     DFBResult ret = DFB_OK;
     int       index;
     int       i, y;

     D_MAGIC_ASSERT( surface, CoreSurface );
     D_ASSERT( transfer != NULL );

     CORE_SURFACE_ALLOCATION_ASSERT( allocation );

     index = dfb_surface_buffer_index( allocation->buffer );

     D_DEBUG_AT( PreAlloc_Bridge, "%s()\n", __FUNCTION__ );

     D_DEBUG_AT( PreAlloc_Bridge, "  -> transfer read/write %s Fusion ID %lu (index %d)\n",
                 (flags & CSAF_WRITE) ? "from" : "to", surface->object.identity, index );

     for (i = 0; i < transfer->num_rects; i++) {
          const DFBRectangle *rect   = &transfer->rects[i];
          int                 offset = DFB_BYTES_PER_LINE( surface->config.format, rect->x );
          int                 length = DFB_BYTES_PER_LINE( surface->config.format, rect->w );
          u8                 *temp   = alloca( length );

          for (y = 0; y < rect->h; y++) {
               DFBRectangle lrect = { rect->x, rect->y + y, rect->w, 1 };

               if (flags & CSAF_WRITE) {
                    ret = CoreSlave_GetData( slave,
                                             surface->config.preallocated[index].addr +
                                             (rect->y + y) * surface->config.preallocated[index].pitch + offset, length,
                                             temp );
                    if (ret)
                         break;

                    ret = dfb_surface_pool_write( allocation->pool, allocation, temp, length, &lrect );
               }
               else {
                    ret = dfb_surface_pool_read( allocation->pool, allocation, temp, length, &lrect );
                    if (ret)
                         break;

                    ret = CoreSlave_PutData( slave,
                                             surface->config.preallocated[index].addr +
                                             (rect->y + y) * surface->config.preallocated[index].pitch + offset, length,
                                             temp );
               }

               if (ret)
                    break;
          }

          if (ret)
               break;
     }

     return ret;
}

static DFBResult
preallocStartTransfer( CoreSurfacePoolBridge   *bridge,
                       void                    *bridge_data,
                       void                    *bridge_local,
                       CoreSurfacePoolTransfer *transfer,
                       void                    *transfer_data )
{
     DFBResult               ret   = DFB_BUG;
     preallocPoolBridgeData *data  = bridge_data;
     CoreSurfaceAllocation  *from  = transfer->from;
     CoreSurfaceAllocation  *to    = transfer->to;
     CoreSurface            *surface;
     CoreSlave              *slave;

     D_DEBUG_AT( PreAlloc_Bridge, "%s()\n", __FUNCTION__ );

     D_ASSERT( bridge_data != NULL );
     D_ASSERT( transfer != NULL );
     D_MAGIC_ASSERT( transfer->buffer, CoreSurfaceBuffer );
     D_MAGIC_ASSERT( transfer->buffer->surface, CoreSurface );

     surface = transfer->buffer->surface;

     slave = Core_Resource_GetSlave( surface->object.identity );
     if (!slave) {
          D_WARN( "no slave object for id %lu", surface->object.identity );
          return DFB_NOIMPL;
     }

     if (from->pool == data->prealloc_pool) {
          if (to->pool->desc.access[CSAID_CPU] & CSAF_WRITE)
               ret = prealloc_transfer_locked( surface, transfer, to, CSAF_WRITE, slave );
          else
               ret = prealloc_transfer_readwrite( surface, transfer, to, CSAF_WRITE, slave );
     }
     else if (to->pool == data->prealloc_pool) {
          if (from->pool->desc.access[CSAID_CPU] & CSAF_READ)
               ret = prealloc_transfer_locked( surface, transfer, from, CSAF_READ, slave );
          else
               ret = prealloc_transfer_readwrite( surface, transfer, from, CSAF_READ, slave );
     }

     return ret;
}

static DFBResult
preallocFinishTransfer( CoreSurfacePoolBridge   *bridge,
                        void                    *bridge_data,
                        void                    *bridge_local,
                        CoreSurfacePoolTransfer *transfer,
                        void                    *transfer_data )
{
     D_DEBUG_AT( PreAlloc_Bridge, "%s()\n", __FUNCTION__ );

     D_ASSERT( bridge_data != NULL );

     return DFB_OK;
}

const SurfacePoolBridgeFuncs preallocSurfacePoolBridgeFuncs = {
     .PoolBridgeDataSize = preallocPoolBridgeDataSize,
     .InitPoolBridge     = preallocInitPoolBridge,
     .JoinPoolBridge     = preallocJoinPoolBridge,
     .DestroyPoolBridge  = preallocDestroyPoolBridge,
     .LeavePoolBridge    = preallocLeavePoolBridge,
     .CheckTransfer      = preallocCheckTransfer,
     .StartTransfer      = preallocStartTransfer,
     .FinishTransfer     = preallocFinishTransfer,
};

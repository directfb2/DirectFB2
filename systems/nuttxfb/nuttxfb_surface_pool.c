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

#include <core/surface_buffer.h>
#include <core/surface_pool.h>

#include "nuttxfb_system.h"

D_DEBUG_DOMAIN( NuttXFB_Surfaces, "NuttXFB/Surfaces", "NuttXFB Surface Pool" );
D_DEBUG_DOMAIN( NuttXFB_SurfLock, "NuttXFB/SurfLock", "NuttXFB Surface Pool Locks" );

/**********************************************************************************************************************/

typedef struct {
     int          magic;

     NuttXFBData *nuttxfb;
} NuttXFBPoolLocalData;

/**********************************************************************************************************************/

static int
nuttxfbPoolLocalDataSize( void )
{
     return sizeof(NuttXFBPoolLocalData);
}

static DFBResult
nuttxfbInitPool( CoreDFB                    *core,
                 CoreSurfacePool            *pool,
                 void                       *pool_data,
                 void                       *pool_local,
                 void                       *system_data,
                 CoreSurfacePoolDescription *ret_desc )
{
     NuttXFBPoolLocalData *local   = pool_local;
     NuttXFBData          *nuttxfb = system_data;

     D_DEBUG_AT( NuttXFB_Surfaces, "%s()\n", __FUNCTION__ );

     D_ASSERT( core != NULL );
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( local != NULL );
     D_ASSERT( nuttxfb != NULL );
     D_ASSERT( ret_desc != NULL );

     ret_desc->caps                 = CSPCAPS_PHYSICAL | CSPCAPS_VIRTUAL;
     ret_desc->access[CSAID_CPU]    = CSAF_READ | CSAF_WRITE | CSAF_SHARED;
     ret_desc->access[CSAID_LAYER0] = CSAF_READ;
     ret_desc->types                = CSTF_LAYER | CSTF_WINDOW | CSTF_CURSOR | CSTF_FONT | CSTF_SHARED | CSTF_EXTERNAL;
     ret_desc->priority             = CSPP_DEFAULT;

     snprintf( ret_desc->name, DFB_SURFACE_POOL_DESC_NAME_LENGTH, "NuttXFB Surface Pool" );

     local->nuttxfb = nuttxfb;

     D_MAGIC_SET( local, NuttXFBPoolLocalData );

     return DFB_OK;
}

static DFBResult
nuttxfbJoinPool( CoreDFB         *core,
                 CoreSurfacePool *pool,
                 void            *pool_data,
                 void            *pool_local,
                 void            *system_data )
{
     NuttXFBPoolLocalData *local = pool_local;
     NuttXFBData          *nuttxfb = system_data;

     D_DEBUG_AT( NuttXFB_Surfaces, "%s()\n", __FUNCTION__ );

     D_ASSERT( core != NULL );
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( local != NULL );
     D_ASSERT( nuttxfb != NULL );

     local->nuttxfb = nuttxfb;

     D_MAGIC_SET( local, NuttXFBPoolLocalData );

     return DFB_OK;
}

static DFBResult
nuttxfbDestroyPool( CoreSurfacePool *pool,
                    void            *pool_data,
                    void            *pool_local )
{
     NuttXFBPoolLocalData *local = pool_local;

     D_UNUSED_P( local );

     D_DEBUG_AT( NuttXFB_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( local, NuttXFBPoolLocalData );

     D_MAGIC_CLEAR( local );

     return DFB_OK;
}

static DFBResult
nuttxfbLeavePool( CoreSurfacePool *pool,
                  void            *pool_data,
                  void            *pool_local )
{
     NuttXFBPoolLocalData *local = pool_local;

     D_UNUSED_P( local );

     D_DEBUG_AT( NuttXFB_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( local, NuttXFBPoolLocalData );

     D_MAGIC_CLEAR( local );

     return DFB_OK;
}

static DFBResult
nuttxfbAllocateBuffer( CoreSurfacePool       *pool,
                       void                  *pool_data,
                       void                  *pool_local,
                       CoreSurfaceBuffer     *buffer,
                       CoreSurfaceAllocation *allocation,
                       void                  *alloc_data )
{
     D_DEBUG_AT( NuttXFB_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     return DFB_OK;
}

static DFBResult
nuttxfbDeallocateBuffer( CoreSurfacePool       *pool,
                         void                  *pool_data,
                         void                  *pool_local,
                         CoreSurfaceBuffer     *buffer,
                         CoreSurfaceAllocation *allocation,
                         void                  *alloc_data )
{
     D_DEBUG_AT( NuttXFB_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     return DFB_OK;
}

static DFBResult
nuttxfbLock( CoreSurfacePool       *pool,
             void                  *pool_data,
             void                  *pool_local,
             CoreSurfaceAllocation *allocation,
             void                  *alloc_data,
             CoreSurfaceBufferLock *lock )
{
     NuttXFBPoolLocalData *local = pool_local;
     NuttXFBData          *nuttxfb;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( local, NuttXFBPoolLocalData );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( NuttXFB_SurfLock, "%s( %p, %p )\n", __FUNCTION__, allocation, lock->buffer );

     nuttxfb = local->nuttxfb;
     D_ASSERT( nuttxfb != NULL );

     lock->pitch  = nuttxfb->planeinfo->stride;
     lock->offset = ~0;
     lock->addr   = nuttxfb->addr;
     lock->phys   = (unsigned long) nuttxfb->planeinfo->fbmem;

     D_DEBUG_AT( NuttXFB_SurfLock, "  -> offset %lu, pitch %u, addr %p, phys 0x%08lx\n",
                 lock->offset, lock->pitch, lock->addr, lock->phys );

     return DFB_OK;
}

static DFBResult
nuttxfbUnlock( CoreSurfacePool       *pool,
               void                  *pool_data,
               void                  *pool_local,
               CoreSurfaceAllocation *allocation,
               void                  *alloc_data,
               CoreSurfaceBufferLock *lock )
{
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( NuttXFB_SurfLock, "%s( %p, %p )\n", __FUNCTION__, allocation, lock->buffer );

     return DFB_OK;
}

const SurfacePoolFuncs nuttxfbSurfacePoolFuncs = {
     .PoolLocalDataSize = nuttxfbPoolLocalDataSize,
     .InitPool          = nuttxfbInitPool,
     .JoinPool          = nuttxfbJoinPool,
     .DestroyPool       = nuttxfbDestroyPool,
     .LeavePool         = nuttxfbLeavePool,
     .AllocateBuffer    = nuttxfbAllocateBuffer,
     .DeallocateBuffer  = nuttxfbDeallocateBuffer,
     .Lock              = nuttxfbLock,
     .Unlock            = nuttxfbUnlock,
};

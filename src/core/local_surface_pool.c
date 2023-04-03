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

#include <core/surface_allocation.h>
#include <core/surface_buffer.h>
#include <core/surface_pool.h>
#include <core/system.h>
#include <misc/conf.h>

D_DEBUG_DOMAIN( Core_Local, "Core/Local", "DirectFB Core Local Surface Pool" );

/**********************************************************************************************************************/

typedef struct {
     int   magic;
     void *addr;
     int   pitch;
     int   size;
} LocalAllocationData;

/**********************************************************************************************************************/

static int
localAllocationDataSize( void )
{
     return sizeof(LocalAllocationData);
}

static DFBResult
localInitPool( CoreDFB                    *core,
               CoreSurfacePool            *pool,
               void                       *pool_data,
               void                       *pool_local,
               void                       *system_data,
               CoreSurfacePoolDescription *ret_desc )
{
     D_DEBUG_AT( Core_Local, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( ret_desc != NULL );

     ret_desc->caps              = CSPCAPS_VIRTUAL;
     ret_desc->access[CSAID_CPU] = CSAF_READ | CSAF_WRITE | CSAF_SHARED;
     ret_desc->types             = CSTF_LAYER | CSTF_WINDOW | CSTF_CURSOR | CSTF_FONT | CSTF_SHARED | CSTF_INTERNAL;
     ret_desc->priority          = CSPP_DEFAULT;

     if (dfb_system_caps() & CSCAPS_SYSMEM_EXTERNAL)
          ret_desc->types |= CSTF_EXTERNAL;

     snprintf( ret_desc->name, DFB_SURFACE_POOL_DESC_NAME_LENGTH, "System Memory" );

     return DFB_OK;
}

static DFBResult
localJoinPool( CoreDFB         *core,
               CoreSurfacePool *pool,
               void            *pool_data,
               void            *pool_local,
               void            *system_data )
{
     D_DEBUG_AT( Core_Local, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     return DFB_OK;
}

static DFBResult
localDestroyPool( CoreSurfacePool *pool,
                  void            *pool_data,
                  void            *pool_local )
{
     D_DEBUG_AT( Core_Local, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     return DFB_OK;
}

static DFBResult
localLeavePool( CoreSurfacePool *pool,
                void            *pool_data,
                void            *pool_local )
{
     D_DEBUG_AT( Core_Local, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     return DFB_OK;
}

static DFBResult
localAllocateBuffer( CoreSurfacePool       *pool,
                     void                  *pool_data,
                     void                  *pool_local,
                     CoreSurfaceBuffer     *buffer,
                     CoreSurfaceAllocation *allocation,
                     void                  *alloc_data )
{
     CoreSurface         *surface;
     LocalAllocationData *alloc = alloc_data;

     D_DEBUG_AT( Core_Local, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_MAGIC_ASSERT( buffer->surface, CoreSurface );
     D_ASSERT( alloc != NULL );

     surface = buffer->surface;

     /* Create aligned local system surface buffer if both base address and pitch are non-zero. */
     if (dfb_config->system_surface_align_base && dfb_config->system_surface_align_pitch) {
          /* Make sure base address and pitch are a positive power of two. */
          D_ASSERT( dfb_config->system_surface_align_base >= 4 );
          D_ASSERT( !(dfb_config->system_surface_align_base & (dfb_config->system_surface_align_base - 1)) );
          D_ASSERT( dfb_config->system_surface_align_pitch >= 2 );
          D_ASSERT( !(dfb_config->system_surface_align_pitch & (dfb_config->system_surface_align_pitch - 1)) );

          dfb_surface_calc_buffer_size( surface, dfb_config->system_surface_align_pitch, 0,
                                        &alloc->pitch, &alloc->size );

          /* posix_memalign() function requires base alignment to actually be at least four. */
          int err = posix_memalign( &alloc->addr, dfb_config->system_surface_align_base, alloc->size );
          if (err) {
              D_ERROR( "Core/Local: Error from posix_memalign with base alignment %u\n",
                       dfb_config->system_surface_align_base );
              return DFB_FAILURE;
          }
     }
     /* Create un-aligned local system surface buffer. */
     else {
          dfb_surface_calc_buffer_size( surface, 8, 0, &alloc->pitch, &alloc->size );

          alloc->addr = D_MALLOC( alloc->size );
          if (!alloc->addr)
               return D_OOM();
     }

     D_MAGIC_SET( alloc, LocalAllocationData );

     allocation->flags = CSALF_VOLATILE;
     allocation->size  = alloc->size;

     return DFB_OK;
}

static DFBResult
localDeallocateBuffer( CoreSurfacePool       *pool,
                       void                  *pool_data,
                       void                  *pool_local,
                       CoreSurfaceBuffer     *buffer,
                       CoreSurfaceAllocation *allocation,
                       void                  *alloc_data )
{
     LocalAllocationData *alloc = alloc_data;

     D_DEBUG_AT( Core_Local, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( alloc, LocalAllocationData );

     if (dfb_config->system_surface_align_base && dfb_config->system_surface_align_pitch)
          /* This was allocated by posix_memalign() function and requires free(). */
          free( alloc->addr );
     else
          D_FREE( alloc->addr );

     D_MAGIC_CLEAR( alloc );

     return DFB_OK;
}

static DFBResult
localLock( CoreSurfacePool       *pool,
           void                  *pool_data,
           void                  *pool_local,
           CoreSurfaceAllocation *allocation,
           void                  *alloc_data,
           CoreSurfaceBufferLock *lock )
{
     LocalAllocationData *alloc = alloc_data;

     D_DEBUG_AT( Core_Local, "%s() <- size %d\n", __FUNCTION__, alloc->size );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );
     D_MAGIC_ASSERT( alloc, LocalAllocationData );

     lock->addr  = alloc->addr;
     lock->pitch = alloc->pitch;

     return DFB_OK;
}

static DFBResult
localUnlock( CoreSurfacePool       *pool,
             void                  *pool_data,
             void                  *pool_local,
             CoreSurfaceAllocation *allocation,
             void                  *alloc_data,
             CoreSurfaceBufferLock *lock )
{
     LocalAllocationData *alloc = alloc_data;

     D_DEBUG_AT( Core_Local, "%s()\n", __FUNCTION__ );

     D_UNUSED_P( alloc );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );
     D_MAGIC_ASSERT( alloc, LocalAllocationData );

     return DFB_OK;
}

const SurfacePoolFuncs localSurfacePoolFuncs = {
     .AllocationDataSize = localAllocationDataSize,
     .InitPool           = localInitPool,
     .JoinPool           = localJoinPool,
     .DestroyPool        = localDestroyPool,
     .LeavePool          = localLeavePool,
     .AllocateBuffer     = localAllocateBuffer,
     .DeallocateBuffer   = localDeallocateBuffer,
     .Lock               = localLock,
     .Unlock             = localUnlock
};

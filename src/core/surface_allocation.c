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
#include <core/gfxcard.h>
#include <core/surface_allocation.h>
#include <core/surface_buffer.h>
#include <core/surface_pool_bridge.h>
#include <direct/memcpy.h>
#include <directfb_util.h>
#include <fusion/shmalloc.h>

D_DEBUG_DOMAIN( Core_SurfAllocation, "Core/SurfAllocation", "DirectFB Core Surface Allocation" );

/**********************************************************************************************************************/

static void
surface_allocation_destructor( FusionObject *object,
                               bool          zombie,
                               void         *ctx )
{
     CoreSurfaceAllocation *allocation = (CoreSurfaceAllocation*) object;

     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );

     D_DEBUG_AT( Core_SurfAllocation, "Destroying allocation %p (%d%s)\n",
                 allocation, allocation->size, zombie ? " ZOMBIE" : "" );

     CoreSurfaceAllocation_Deinit_Dispatch( &allocation->call );

     if (!D_FLAGS_IS_SET( allocation->flags, CSALF_INITIALIZING )) {
          if (allocation->surface)
               dfb_surface_lock( allocation->surface );

          CORE_SURFACE_ALLOCATION_ASSERT( allocation );

          dfb_surface_pool_deallocate( allocation->pool, allocation );

          if (allocation->surface)
               dfb_surface_unlock( allocation->surface );
     }

     if (allocation->data)
          SHFREE( allocation->pool->shmpool, allocation->data );

     direct_serial_deinit( &allocation->serial );

     D_MAGIC_CLEAR( allocation );

     /* Destroy the object. */
     fusion_object_destroy( object );
}

FusionObjectPool *
dfb_surface_allocation_pool_create( const FusionWorld *world )
{
     FusionObjectPool *pool;

     pool = fusion_object_pool_create( "Surface Allocation Pool",
                                       sizeof(CoreSurfaceAllocation), sizeof(CoreSurfaceAllocationNotification),
                                       surface_allocation_destructor, NULL, world );

     return pool;
}

/**********************************************************************************************************************/

DFBResult
dfb_surface_allocation_create( CoreDFB                *core,
                               CoreSurfaceBuffer      *buffer,
                               CoreSurfacePool        *pool,
                               CoreSurfaceAllocation **ret_allocation )
{
     CoreSurface           *surface;
     CoreSurfaceAllocation *allocation;

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_MAGIC_ASSERT( buffer->surface, CoreSurface );
     D_ASSERT( pool != NULL );
     D_ASSERT( ret_allocation != NULL );

     D_DEBUG_AT( Core_SurfAllocation, "%s( %dx%d %s )\n", __FUNCTION__,
                 buffer->config.size.w, buffer->config.size.h, dfb_pixelformat_name( buffer->config.format ) );

     surface = buffer->surface;

     allocation = dfb_core_create_surface_allocation( core );
     if (!allocation)
          return DFB_FUSION;

     allocation->buffer      = buffer;
     allocation->surface     = surface;
     allocation->pool        = pool;
     allocation->flags       = CSALF_INITIALIZING;
     allocation->access      = pool->desc.access;
     allocation->config      = buffer->config;
     allocation->type        = buffer->type;
     allocation->resource_id = buffer->resource_id;
     allocation->index       = buffer->index;
     allocation->buffer_id   = buffer->object.id;

     if (pool->alloc_data_size) {
          allocation->data = SHCALLOC( pool->shmpool, 1, pool->alloc_data_size );
          if (!allocation->data) {
               fusion_object_destroy( &allocation->object );
               return D_OOSHM();
          }
     }

     direct_serial_init( &allocation->serial );

     fusion_ref_add_permissions( &allocation->object.ref, 0, FUSION_REF_PERMIT_REF_UNREF_LOCAL );

     CoreSurfaceAllocation_Init_Dispatch( core, allocation, &allocation->call );

     D_MAGIC_SET( allocation, CoreSurfaceAllocation );

     /* Activate object. */
     fusion_object_activate( &allocation->object );

     /* Return the new allocation. */
     *ret_allocation = allocation;

     D_DEBUG_AT( Core_SurfAllocation, "  -> %p\n", allocation );

     return DFB_OK;
}

DFBResult
dfb_surface_allocation_decouple( CoreSurfaceAllocation *allocation )
{
     int                    i;
     int                    locks;
     CoreSurfaceBuffer     *buffer;
     CoreSurfaceAllocation *alloc;

     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( allocation->buffer, CoreSurfaceBuffer );
     D_MAGIC_ASSERT( allocation->surface, CoreSurface );
     D_ASSERT( allocation->surface == allocation->buffer->surface );

     D_DEBUG_AT( Core_SurfAllocation, "%s( %p )\n", __FUNCTION__, allocation );

     buffer = allocation->buffer;

     /* Indicate that this surface buffer pool allocation is about to be destroyed. */
     dfb_surface_pool_notify( allocation->surface, buffer, allocation, CSNF_BUFFER_ALLOCATION_DESTROY );

     allocation->buffer  = NULL;
     allocation->surface = NULL;

     fusion_vector_remove( &buffer->allocs, fusion_vector_index_of( &buffer->allocs, allocation ) );

     locks = dfb_surface_allocation_locks( allocation );
     if (!locks) {
          if (allocation->accessed[CSAID_GPU] & (CSAF_READ | CSAF_WRITE))
               /* Wait for the operation to finish. */
               dfb_gfxcard_wait_serial( &allocation->gfx_serial );

          dfb_surface_pool_deallocate( allocation->pool, allocation );
     }

     /* Reset 'read' allocation pointer of buffer. */
     if (buffer->read == allocation)
          buffer->read = NULL;

     /* Update 'written' allocation pointer of buffer. */
     if (buffer->written == allocation) {
          /* Reset pointer first. */
          buffer->written = NULL;

          /* Iterate through remaining allocations. */
          fusion_vector_foreach (alloc, i, buffer->allocs) {
               CORE_SURFACE_ALLOCATION_ASSERT( alloc );

               /* Check if allocation is up to date and set it as 'written' allocation. */
               if (direct_serial_check( &alloc->serial, &buffer->serial )) {
                    buffer->written = alloc;
                    break;
               }
          }
     }

     dfb_surface_allocation_unlink( &allocation );

     return DFB_OK;
}

static void
transfer_buffer( const CoreSurfaceConfig *config,
                 const char              *src,
                 char                    *dst,
                 int                      srcpitch,
                 int                      dstpitch )
{
     int i;

     D_DEBUG_AT( Core_SurfAllocation, "%s( %p, %p [%d] -> %p [%d] ) <- %d\n", __FUNCTION__,
                 config, src, srcpitch, dst, dstpitch, config->size.h );

     D_ASSERT( src != NULL );
     D_ASSERT( dst != NULL );
     D_ASSERT( srcpitch > 0 );
     D_ASSERT( dstpitch > 0 );
     D_ASSERT( srcpitch >= DFB_BYTES_PER_LINE( config->format, config->size.w ) );
     D_ASSERT( dstpitch >= DFB_BYTES_PER_LINE( config->format, config->size.w ) );

     for (i = 0; i < config->size.h; i++) {
          direct_memcpy( dst, src, DFB_BYTES_PER_LINE( config->format, config->size.w ) );
          src += srcpitch;
          dst += dstpitch;
     }

     switch (config->format) {
          case DSPF_I420:
          case DSPF_YV12:
               for (i = 0; i < config->size.h; i++) {
                    direct_memcpy( dst, src, DFB_BYTES_PER_LINE( config->format, config->size.w / 2 ) );
                    src += srcpitch / 2;
                    dst += dstpitch / 2;
               }
               break;

          case DSPF_Y42B:
          case DSPF_YV16:
               for (i = 0; i < config->size.h * 2; i++) {
                    direct_memcpy( dst, src, DFB_BYTES_PER_LINE( config->format, config->size.w / 2 ) );
                    src += srcpitch / 2;
                    dst += dstpitch / 2;
               }
               break;

          case DSPF_NV12:
          case DSPF_NV21:
               for (i = 0; i < config->size.h / 2; i++) {
                    direct_memcpy( dst, src, DFB_BYTES_PER_LINE( config->format, config->size.w ) );
                    src += srcpitch;
                    dst += dstpitch;
               }
               break;

          case DSPF_NV16:
          case DSPF_NV61:
               for (i = 0; i < config->size.h; i++) {
                    direct_memcpy( dst, src, DFB_BYTES_PER_LINE( config->format, config->size.w ) );
                    src += srcpitch;
                    dst += dstpitch;
               }
               break;

          case DSPF_YUV444P:
               for (i = 0; i < config->size.h * 2; i++) {
                    direct_memcpy( dst, src, DFB_BYTES_PER_LINE( config->format, config->size.w ) );
                    src += srcpitch;
                    dst += dstpitch;
               }
               break;

          default:
               break;
     }
}

static DFBResult
allocation_update_copy( CoreSurfaceAllocation *allocation,
                        CoreSurfaceAllocation *source )
{
     DFBResult             ret;
     CoreSurfaceBufferLock src;
     CoreSurfaceBufferLock dst;

     D_DEBUG_AT( Core_SurfAllocation, "%s( %p )\n", __FUNCTION__, allocation );

     D_ASSERT( allocation != source );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( source, CoreSurfaceAllocation );

     /* Lock the source allocation. */
     dfb_surface_buffer_lock_init( &src, CSAID_CPU, CSAF_READ );

     dfb_surface_pool_prelock( source->pool, source, CSAID_CPU, CSAF_READ );

     ret = dfb_surface_pool_lock( source->pool, source, &src );
     if (ret) {
          D_DERROR( ret, "Core/SurfAllocation: Could not lock source for transfer!\n" );
          dfb_surface_buffer_lock_deinit( &src );
          return ret;
     }

     /* Lock the destination allocation. */
     dfb_surface_buffer_lock_init( &dst, CSAID_CPU, CSAF_WRITE );

     dfb_surface_pool_prelock( allocation->pool, allocation, CSAID_CPU, CSAF_WRITE );

     allocation->accessed[CSAID_CPU] |= CSAF_WRITE;
     source->accessed[CSAID_CPU]     |= CSAF_READ;

     ret = dfb_surface_pool_lock( allocation->pool, allocation, &dst );
     if (ret) {
          D_DERROR( ret, "Core/SurfAllocation: Could not lock destination for transfer!\n" );
          dfb_surface_pool_unlock( source->pool, source, &src );
          dfb_surface_buffer_lock_deinit( &dst );
          dfb_surface_buffer_lock_deinit( &src );
          return ret;
     }

     transfer_buffer( &allocation->config, (char*) src.addr, (char*) dst.addr, src.pitch, dst.pitch );

     dfb_surface_pool_unlock( allocation->pool, allocation, &dst );
     dfb_surface_pool_unlock( source->pool, source, &src );

     dfb_surface_buffer_lock_deinit( &dst );
     dfb_surface_buffer_lock_deinit( &src );

     return DFB_OK;
}

static DFBResult
allocation_update_write( CoreSurfaceAllocation *allocation,
                         CoreSurfaceAllocation *source )
{
     DFBResult             ret;
     CoreSurfaceBufferLock src;

     D_DEBUG_AT( Core_SurfAllocation, "%s( %p )\n", __FUNCTION__, allocation );

     D_ASSERT( allocation != source );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( source, CoreSurfaceAllocation );

     /* Lock the source allocation. */
     dfb_surface_buffer_lock_init( &src, CSAID_CPU, CSAF_READ );

     dfb_surface_pool_prelock( source->pool, source, CSAID_CPU, CSAF_READ );

     source->accessed[CSAID_CPU] |= source->accessed[CSAID_CPU];

     ret = dfb_surface_pool_lock( source->pool, source, &src );
     if (ret) {
          D_DERROR( ret, "Core/SurfAllocation: Could not lock source for transfer!\n" );
          dfb_surface_buffer_lock_deinit( &src );
          return ret;
     }

     /* Write to the destination allocation. */
     ret = dfb_surface_pool_write( allocation->pool, allocation, (char*) src.addr, src.pitch, NULL );
     if (ret)
          D_DERROR( ret, "Core/SurfAllocation: Could not write from destination allocation!\n" );

     dfb_surface_pool_unlock( source->pool, source, &src );

     dfb_surface_buffer_lock_deinit( &src );

     return ret;
}

static DFBResult
allocation_update_read( CoreSurfaceAllocation *allocation,
                        CoreSurfaceAllocation *source )
{
     DFBResult             ret;
     CoreSurfaceBufferLock dst;

     D_DEBUG_AT( Core_SurfAllocation, "%s( %p )\n", __FUNCTION__, allocation );

     D_ASSERT( allocation != source );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( source, CoreSurfaceAllocation );

     /* Lock the destination allocation. */
     dfb_surface_buffer_lock_init( &dst, CSAID_CPU, CSAF_WRITE );

     dfb_surface_pool_prelock( allocation->pool, allocation, CSAID_CPU, CSAF_WRITE );

     allocation->accessed[CSAID_CPU] |= CSAF_READ;

     ret = dfb_surface_pool_lock( allocation->pool, allocation, &dst );
     if (ret) {
          D_DERROR( ret, "Core/SurfAllocation: Could not lock destination for transfer!\n" );
          dfb_surface_buffer_lock_deinit( &dst );
          return ret;
     }

     /* Read from the source allocation. */
     ret = dfb_surface_pool_read( source->pool, source, dst.addr, dst.pitch, NULL );
     if (ret)
          D_DERROR( ret, "Core/SurfAllocation: Could not read from source allocation!\n" );

     dfb_surface_pool_unlock( allocation->pool, allocation, &dst );

     dfb_surface_buffer_lock_deinit( &dst );

     return ret;
}

DFBResult
dfb_surface_allocation_update( CoreSurfaceAllocation  *allocation,
                               CoreSurfaceAccessFlags  access )
{
     DFBResult              ret;
     int                    i;
     CoreSurfaceAllocation *alloc;
     CoreSurfaceBuffer     *buffer;

     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( allocation->buffer, CoreSurfaceBuffer );
     D_FLAGS_ASSERT( access, CSAF_ALL );

     D_DEBUG_AT( Core_SurfAllocation, "%s( %p )\n", __FUNCTION__, allocation );

     buffer = allocation->buffer;

     D_MAGIC_ASSERT( buffer->surface, CoreSurface );
     FUSION_SKIRMISH_ASSERT( &buffer->surface->lock );

     if (direct_serial_update( &allocation->serial, &buffer->serial ) && buffer->written) {
          CoreSurfaceAllocation *source = buffer->written;

          D_ASSUME( allocation != source );

          D_DEBUG_AT( Core_SurfAllocation, "  -> alloc/written buffer %p/%p\n", allocation->buffer, source->buffer );

          D_MAGIC_ASSERT( source, CoreSurfaceAllocation );
          D_ASSERT( source->buffer == allocation->buffer );

          D_DEBUG_AT( Core_SurfAllocation, "  -> updating allocation %p from %p...\n", allocation, source );

          ret = dfb_surface_pool_bridges_transfer( buffer, source, allocation, NULL, 0 );
          if (ret) {
               if ((source->access[CSAID_CPU] & CSAF_READ) && (allocation->access[CSAID_CPU] & CSAF_WRITE))
                    ret = allocation_update_copy( allocation, source );
               else if (source->access[CSAID_CPU] & CSAF_READ)
                    ret = allocation_update_write( allocation, source );
               else if (allocation->access[CSAID_CPU] & CSAF_WRITE)
                    ret = allocation_update_read( allocation, source );
               else {
                    D_WARN( "allocation update: '%s' -> '%s'", source->pool->desc.name, allocation->pool->desc.name );
                    D_UNIMPLEMENTED();
                    ret = DFB_UNSUPPORTED;
               }
          }

          if (ret) {
               D_DERROR( ret, "Core/SurfAllocation: Updating allocation failed!\n" );
               return ret;
          }
     }

     if (access & CSAF_WRITE) {
          D_DEBUG_AT( Core_SurfAllocation, "  -> increasing serial...\n" );

          direct_serial_increase( &buffer->serial );

          direct_serial_copy( &allocation->serial, &buffer->serial );

          buffer->written = allocation;
          buffer->read    = NULL;

          /* Zap volatile allocations (freed when no longer up to date). */
          fusion_vector_foreach (alloc, i, buffer->allocs) {
               D_MAGIC_ASSERT( alloc, CoreSurfaceAllocation );

               if (alloc != allocation && (alloc->flags & CSALF_VOLATILE)) {
                    dfb_surface_allocation_decouple( alloc );
                    i--;
               }
          }
     }
     else
          buffer->read = allocation;

     /* Zap all other allocations. */
     if (dfb_config->thrifty_surface_buffers) {
          buffer->written = buffer->read = allocation;

          fusion_vector_foreach (alloc, i, buffer->allocs) {
               D_MAGIC_ASSERT( alloc, CoreSurfaceAllocation );

               /* Don't zap preallocated which would not really free up memory, but just loose the handle. */
               if (alloc != allocation && !(alloc->flags & (CSALF_PREALLOCATED | CSALF_MUCKOUT))) {
                    dfb_surface_allocation_decouple( alloc );
                    i--;
               }
          }
     }

     return DFB_OK;
}

DFBResult
dfb_surface_allocation_dump( CoreSurfaceAllocation *allocation,
                             const char            *directory,
                             const char            *prefix,
                             bool                   raw )
{
     DFBResult        ret = DFB_OK;
     CoreSurfacePool *pool;

     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( allocation->pool, CoreSurfacePool );
     D_ASSERT( directory != NULL );

     D_DEBUG_AT( Core_SurfAllocation, "%s( %p, '%s', '%s' )\n", __FUNCTION__, allocation, directory, prefix );

     pool = allocation->pool;

     if (D_FLAGS_IS_SET( pool->desc.caps, CSPCAPS_READ )) {
          int   pitch;
          int   size;
          void *buf;

          dfb_surface_calc_buffer_size( allocation->surface, 4, 1, &pitch, &size );

          buf = D_MALLOC( size );
          if (!buf)
               return D_OOM();

          ret = dfb_surface_pool_read( pool, allocation, buf, pitch, NULL );
          if (ret == DFB_OK)
               ret = dfb_surface_buffer_dump_type_locked2( allocation->buffer, directory, prefix, raw, buf, pitch );

          D_FREE( buf );
     }
     else {
          CoreSurfaceBufferLock lock;

          dfb_surface_buffer_lock_init( &lock, CSAID_CPU, CSAF_READ );

          /* Lock the surface buffer, get the data pointer and pitch. */
          ret = dfb_surface_pool_lock( pool, allocation, &lock );
          if (ret) {
               dfb_surface_buffer_lock_deinit( &lock );
               return ret;
          }

          ret = dfb_surface_buffer_dump_type_locked( allocation->buffer, directory, prefix, raw, &lock );

          /* Unlock the surface buffer. */
          dfb_surface_pool_unlock( allocation->pool, allocation, &lock );

          dfb_surface_buffer_lock_deinit( &lock );
     }

     return ret;
}

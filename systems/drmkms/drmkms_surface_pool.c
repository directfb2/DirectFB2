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

#include <core/core.h>
#include <core/gfxcard.h>
#include <core/surface_allocation.h>
#include <core/surface_buffer.h>
#include <core/surface_pool.h>
#include <direct/hash.h>
#include <drm_fourcc.h>

#include "drmkms_system.h"

D_DEBUG_DOMAIN( DRMKMS_Surfaces, "DRMKMS/Surfaces", "DRM/KMS Surface Pool" );
D_DEBUG_DOMAIN( DRMKMS_SurfLock, "DRMKMS/SurfLock", "DRM/KMS Surface Pool Locks" );

/**********************************************************************************************************************/

struct kms_bo {
     struct kms_driver *kms;
     void              *ptr;
     size_t             size;
     size_t             offset;
     size_t             pitch;
     unsigned int       handle;
};

typedef struct {
     int magic;
} DRMKMSPoolData;

typedef struct {
     int          magic;

     DRMKMSData  *drmkms;

     CoreDFB     *core;

     DirectHash  *hash;
     DirectMutex  lock;
} DRMKMSPoolLocalData;

typedef struct {
     int            magic;

     struct kms_bo *bo;

     unsigned int   handle;
     unsigned int   pitch;
     int            size;

     int            prime_fd;

     uint32_t       name;

     uint32_t       fb_id;

     void          *addr;
} DRMKMSAllocationData;

typedef struct {
     int                  magic;

     DRMKMSPoolLocalData *pool_local;

     FusionObjectID       alloc_id;

     struct kms_bo        bo;

     void                *addr;

     Reaction             reaction;
} DRMKMSAllocationLocalData;

/**********************************************************************************************************************/

static int
drmkmsPoolDataSize()
{
     return sizeof(DRMKMSPoolData);
}

static int
drmkmsPoolLocalDataSize()
{
     return sizeof(DRMKMSPoolLocalData);
}

static int
drmkmsAllocationDataSize()
{
     return sizeof(DRMKMSAllocationData);
}

static DFBResult
drmkmsInitPool( CoreDFB                    *core,
                CoreSurfacePool            *pool,
                void                       *pool_data,
                void                       *pool_local,
                void                       *system_data,
                CoreSurfacePoolDescription *ret_desc )
{
     DRMKMSPoolData      *data   = pool_data;
     DRMKMSPoolLocalData *local  = pool_local;
     DRMKMSData          *drmkms = system_data;

     D_UNUSED_P( data );

     D_DEBUG_AT( DRMKMS_Surfaces, "%s()\n", __FUNCTION__ );

     D_ASSERT( core != NULL );
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( data != NULL );
     D_ASSERT( local != NULL );
     D_ASSERT( drmkms != NULL );
     D_ASSERT( drmkms->shared != NULL );
     D_ASSERT( ret_desc != NULL );

     ret_desc->caps              = CSPCAPS_VIRTUAL;
     ret_desc->access[CSAID_CPU] = CSAF_READ | CSAF_WRITE | CSAF_SHARED;
     ret_desc->access[CSAID_GPU] = CSAF_READ | CSAF_WRITE | CSAF_SHARED;
     ret_desc->types             = CSTF_LAYER | CSTF_WINDOW | CSTF_CURSOR | CSTF_FONT | CSTF_SHARED | CSTF_EXTERNAL;
     ret_desc->priority          = CSPP_ULTIMATE;

     /* For hardware layers. */
     ret_desc->access[CSAID_LAYER0]  = CSAF_READ;
     ret_desc->access[CSAID_LAYER1]  = CSAF_READ;
     ret_desc->access[CSAID_LAYER2]  = CSAF_READ;
     ret_desc->access[CSAID_LAYER3]  = CSAF_READ;
     ret_desc->access[CSAID_LAYER4]  = CSAF_READ;
     ret_desc->access[CSAID_LAYER5]  = CSAF_READ;
     ret_desc->access[CSAID_LAYER6]  = CSAF_READ;
     ret_desc->access[CSAID_LAYER7]  = CSAF_READ;
     ret_desc->access[CSAID_LAYER8]  = CSAF_READ;
     ret_desc->access[CSAID_LAYER9]  = CSAF_READ;
     ret_desc->access[CSAID_LAYER10] = CSAF_READ;
     ret_desc->access[CSAID_LAYER11] = CSAF_READ;
     ret_desc->access[CSAID_LAYER12] = CSAF_READ;
     ret_desc->access[CSAID_LAYER13] = CSAF_READ;
     ret_desc->access[CSAID_LAYER14] = CSAF_READ;
     ret_desc->access[CSAID_LAYER15] = CSAF_READ;

     snprintf( ret_desc->name, DFB_SURFACE_POOL_DESC_NAME_LENGTH, "DRMKMS" );

     local->drmkms = drmkms;
     local->core   = core;

     direct_hash_create( 17, &local->hash );
     direct_mutex_init( &local->lock );

     D_MAGIC_SET( data, DRMKMSPoolData );
     D_MAGIC_SET( local, DRMKMSPoolLocalData );

     return DFB_OK;
}

static DFBResult
drmkmsJoinPool( CoreDFB         *core,
                CoreSurfacePool *pool,
                void            *pool_data,
                void            *pool_local,
                void            *system_data )
{
     DRMKMSPoolData      *data  = pool_data;
     DRMKMSPoolLocalData *local = pool_local;
     DRMKMSData          *drmkms  = system_data;

     D_UNUSED_P( data );

     D_DEBUG_AT( DRMKMS_Surfaces, "%s()\n", __FUNCTION__ );

     D_ASSERT( core != NULL );
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, DRMKMSPoolData );
     D_ASSERT( local != NULL );
     D_ASSERT( drmkms != NULL );
     D_ASSERT( drmkms->shared != NULL );

     local->drmkms = drmkms;
     local->core   = core;

     direct_hash_create( 17, &local->hash );
     direct_mutex_init( &local->lock );

     D_MAGIC_SET( local, DRMKMSPoolLocalData );

     return DFB_OK;
}

static DFBResult
drmkmsDestroyPool( CoreSurfacePool *pool,
                   void            *pool_data,
                   void            *pool_local )
{
     DRMKMSPoolData      *data  = pool_data;
     DRMKMSPoolLocalData *local = pool_local;

     D_UNUSED_P( data );

     D_DEBUG_AT( DRMKMS_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, DRMKMSPoolData );
     D_MAGIC_ASSERT( local, DRMKMSPoolLocalData );

     direct_mutex_deinit( &local->lock );
     direct_hash_destroy( local->hash );

     D_MAGIC_CLEAR( data );
     D_MAGIC_CLEAR( local );

     return DFB_OK;
}

static DFBResult
drmkmsLeavePool( CoreSurfacePool *pool,
                 void            *pool_data,
                 void            *pool_local )
{
     DRMKMSPoolData      *data  = pool_data;
     DRMKMSPoolLocalData *local = pool_local;

     D_UNUSED_P( data );

     D_DEBUG_AT( DRMKMS_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, DRMKMSPoolData );
     D_MAGIC_ASSERT( local, DRMKMSPoolLocalData );

     direct_mutex_deinit( &local->lock );
     direct_hash_destroy( local->hash );

     D_MAGIC_CLEAR( local );

     return DFB_OK;
}

static DFBResult
drmkmsTestConfig( CoreSurfacePool         *pool,
                  void                    *pool_data,
                  void                    *pool_local,
                  CoreSurfaceBuffer       *buffer,
                  const CoreSurfaceConfig *config )
{
     CoreSurface         *surface;
     DRMKMSPoolData      *data  = pool_data;
     DRMKMSPoolLocalData *local = pool_local;

     D_UNUSED_P( data );
     D_UNUSED_P( local );

     D_DEBUG_AT( DRMKMS_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, DRMKMSPoolData );
     D_MAGIC_ASSERT( local, DRMKMSPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_MAGIC_ASSERT( buffer->surface, CoreSurface );

     surface = buffer->surface;

     switch (surface->config.format) {
          case DSPF_ARGB:
          case DSPF_RGB32:
          case DSPF_RGB16:
          case DSPF_RGB555:
          case DSPF_ARGB1555:
          case DSPF_RGB332:
          case DSPF_RGB24:
          case DSPF_A8:
          case DSPF_UYVY:
          case DSPF_YUY2:
          case DSPF_NV12:
          case DSPF_NV21:
          case DSPF_NV16:
          case DSPF_NV61:
               break;
          default:
               D_DEBUG_AT( DRMKMS_Surfaces, "  -> unsupported pixelformat!\n" );
               return DFB_UNSUPPORTED;
     }

     return DFB_OK;
}

static DFBResult
drmkmsAllocateBuffer( CoreSurfacePool       *pool,
                      void                  *pool_data,
                      void                  *pool_local,
                      CoreSurfaceBuffer     *buffer,
                      CoreSurfaceAllocation *allocation,
                      void                  *alloc_data )
{
     DFBResult             ret;
     int                   pitch;
     int                   length;
     CoreSurface          *surface;
     DRMKMSPoolData       *data  = pool_data;
     DRMKMSPoolLocalData  *local = pool_local;
     DRMKMSAllocationData *alloc = alloc_data;
     DRMKMSData           *drmkms;
     DRMKMSDataShared     *shared;
     u32                   drm_width;
     u32                   drm_height;
     u32                   drm_format;

     D_UNUSED_P( data );

     D_DEBUG_AT( DRMKMS_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, DRMKMSPoolData );
     D_MAGIC_ASSERT( local, DRMKMSPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_MAGIC_ASSERT( buffer->surface, CoreSurface );

     drmkms = local->drmkms;
     D_ASSERT( drmkms != NULL );

     shared = drmkms->shared;
     D_ASSERT( shared != NULL );

     surface = buffer->surface;

     dfb_gfxcard_calc_buffer_size( buffer, &pitch, &length );

     drm_width  = (pitch + 3) >> 2;
     drm_height = surface->config.size.h;

     switch (surface->config.format) {
          case DSPF_ARGB:
               drm_format = DRM_FORMAT_ARGB8888;
               break;
          case DSPF_RGB32:
               drm_format = DRM_FORMAT_XRGB8888;
               break;
          case DSPF_RGB16:
               drm_format = DRM_FORMAT_RGB565;
               break;
          case DSPF_RGB555:
               drm_format = DRM_FORMAT_XRGB1555;
               break;
          case DSPF_ARGB1555:
               drm_format = DRM_FORMAT_ARGB1555;
               break;
          case DSPF_RGB332:
               drm_format = DRM_FORMAT_RGB332;
               break;
          case DSPF_RGB24:
               drm_format = DRM_FORMAT_RGB888;
               break;
          case DSPF_A8:
               drm_format = DRM_FORMAT_C8;
               break;
          case DSPF_UYVY:
               drm_format = DRM_FORMAT_UYVY;
               break;
          case DSPF_YUY2:
               drm_format = DRM_FORMAT_YUYV;
               break;
          case DSPF_NV12:
               drm_format = DRM_FORMAT_NV12;
               drm_height = (surface->config.size.h * 3 + 1) >> 1;
               break;
          case DSPF_NV21:
               drm_format = DRM_FORMAT_NV21;
               drm_height = (surface->config.size.h * 3 + 1) >> 1;
               break;
          case DSPF_NV16:
               drm_format = DRM_FORMAT_NV16;
               drm_height = surface->config.size.h << 2;
               break;
          case DSPF_NV61:
               drm_format = DRM_FORMAT_NV61;
               drm_height = surface->config.size.h << 2;
               break;

          default:
               D_ERROR( "DRMKMS/Surfaces: Unsupported pixelformat!\n" );
               ret = DFB_FAILURE;
               goto error;
     }

     const unsigned int attr[] = { KMS_BO_TYPE, KMS_BO_TYPE_SCANOUT_X8R8G8B8,
                                   KMS_WIDTH,   drm_width,
                                   KMS_HEIGHT,  drm_height,
                                   KMS_TERMINATE_PROP_LIST };

     if (kms_bo_create( drmkms->kms, attr, &alloc->bo ) < 0) {
          ret = errno2result( errno );
          D_PERROR( "DRMKMS/Surfaces: kms_bo_create() failed!\n" );
          goto error;
     }

     kms_bo_get_prop( alloc->bo, KMS_HANDLE, &alloc->handle );
     kms_bo_get_prop( alloc->bo, KMS_PITCH, &alloc->pitch );
     alloc->size = allocation->config.size.h * alloc->bo->pitch;

     D_DEBUG_AT( DRMKMS_Surfaces, "  -> bo       %p\n", alloc->bo );
     D_DEBUG_AT( DRMKMS_Surfaces, "  -> handle   %u\n", alloc->handle );
     D_DEBUG_AT( DRMKMS_Surfaces, "  -> pitch    %u\n", alloc->pitch );
     D_DEBUG_AT( DRMKMS_Surfaces, "  -> size     %d\n", alloc->size );

     alloc->prime_fd = -1;

     if (shared->use_prime_fd) {
          if (drmPrimeHandleToFD( drmkms->fd, alloc->handle, DRM_CLOEXEC, &alloc->prime_fd ) < 0) {
               ret = errno2result( errno );
               D_PERROR( "DRMKMS/Surfaces: drmPrimeHandleToFD( %u ) failed!\n", alloc->handle );
               goto error;
          }

          D_DEBUG_AT( DRMKMS_Surfaces, "  -> prime_fd %d\n", alloc->prime_fd );
     }

     if (!alloc->name) {
          struct drm_gem_flink fl;
          fl.handle = alloc->handle;
          if (drmIoctl( drmkms->fd, DRM_IOCTL_GEM_FLINK, &fl ) < 0) {
               ret = errno2result( errno );
               D_PERROR( "DRMKMS/Surfaces: DRM_IOCTL_GEM_FLINK( %u ) failed!\n", alloc->handle );
               goto error;
          }

          alloc->name = fl.name;

          D_DEBUG_AT( DRMKMS_Surfaces, "  -> name     %u\n", alloc->name );
     }

     allocation->size   = alloc->size;
     allocation->offset = alloc->prime_fd;

     if (surface->type & (CSTF_LAYER | CSTF_WINDOW) && Core_GetIdentity() == local->core->fusion_id) {
          u32 drm_bo_offsets[4] = { 0, 0, 0, 0 };
          u32 drm_bo_handles[4] = { 0, 0, 0, 0 };
          u32 drm_bo_pitches[4] = { 0, 0, 0, 0 };

          drm_bo_handles[0] = drm_bo_handles[1] = drm_bo_handles[2] = drm_bo_handles[3] = alloc->bo->handle;
          drm_bo_pitches[0] = drm_bo_pitches[1] = drm_bo_pitches[2] = drm_bo_pitches[3] = alloc->bo->pitch;
          drm_bo_offsets[1] = surface->config.size.h * alloc->pitch;

          if (drmModeAddFB2( drmkms->fd, surface->config.size.w, surface->config.size.h, drm_format,
                             drm_bo_handles, drm_bo_pitches, drm_bo_offsets, &alloc->fb_id, 0 ) < 0) {
               ret = errno2result( errno );
               D_PERROR( "DRMKMS/Surfaces: drmModeAddFB2( %u ) failed!\n", alloc->handle );
               goto error;
          }

          D_DEBUG_AT( DRMKMS_Surfaces, "  -> fb_id    %u\n", alloc->fb_id );
     }

     if (kms_bo_map( alloc->bo, &alloc->addr ) < 0) {
          ret = errno2result( errno );
          D_PERROR( "DRMKMS/Surfaces: kms_bo_map( %u ) failed!\n", alloc->handle );
          goto error;
     }

     D_DEBUG_AT( DRMKMS_Surfaces, "  -> addr     %p\n", alloc->addr );

     D_MAGIC_SET( alloc, DRMKMSAllocationData );

     return DFB_OK;

error:
     if (alloc->addr) {
          kms_bo_unmap( alloc->bo );
          alloc->addr = NULL;
     }

     if (alloc->fb_id) {
          drmModeRmFB( local->drmkms->fd, alloc->fb_id );
          alloc->fb_id = 0;
     }

     if (alloc->prime_fd != -1) {
          close( alloc->prime_fd );
          alloc->prime_fd = -1;
     }

     if (alloc->bo) {
          kms_bo_destroy( &alloc->bo );
          alloc->bo = NULL;
     }

     return ret;
}

static DFBResult
drmkmsDeallocateBuffer( CoreSurfacePool       *pool,
                        void                  *pool_data,
                        void                  *pool_local,
                        CoreSurfaceBuffer     *buffer,
                        CoreSurfaceAllocation *allocation,
                        void                  *alloc_data )
{
     DRMKMSPoolData       *data  = pool_data;
     DRMKMSPoolLocalData  *local = pool_local;
     DRMKMSAllocationData *alloc = alloc_data;

     D_UNUSED_P( data );

     D_DEBUG_AT( DRMKMS_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, DRMKMSPoolData );
     D_MAGIC_ASSERT( alloc, DRMKMSAllocationData );

     dfb_gfxcard_sync();

     D_DEBUG_AT( DRMKMS_Surfaces, "  -> bo       %p\n", alloc->bo );
     D_DEBUG_AT( DRMKMS_Surfaces, "  -> handle   %u\n", alloc->handle );
     D_DEBUG_AT( DRMKMS_Surfaces, "  -> pitch    %u\n", alloc->pitch );
     D_DEBUG_AT( DRMKMS_Surfaces, "  -> size     %d\n", alloc->size );
     D_DEBUG_AT( DRMKMS_Surfaces, "  -> prime_fd %d\n", alloc->prime_fd );
     D_DEBUG_AT( DRMKMS_Surfaces, "  -> name     %u\n", alloc->name );
     D_DEBUG_AT( DRMKMS_Surfaces, "  -> fb_id    %u\n", alloc->fb_id );
     D_DEBUG_AT( DRMKMS_Surfaces, "  -> addr     %p\n", alloc->addr );

     if (alloc->addr) {
          kms_bo_unmap( alloc->bo );
          alloc->addr = NULL;
     }

     if (alloc->fb_id) {
          drmModeRmFB( local->drmkms->fd, alloc->fb_id );
          alloc->fb_id = 0;
     }

     if (alloc->prime_fd != -1) {
          close( alloc->prime_fd );
          alloc->prime_fd = -1;
     }

     if (alloc->bo) {
          kms_bo_destroy( &alloc->bo );
          alloc->bo = NULL;
     }

     D_MAGIC_CLEAR( alloc );

     return DFB_OK;
}

static ReactionResult
drmkmsAllocationReaction( const void *msg_data,
                          void       *ctx )
{
     const CoreSurfaceAllocationNotification *notification = msg_data;
     DRMKMSAllocationLocalData               *alloc_local  = ctx;

     D_DEBUG_AT( DRMKMS_Surfaces, "%s() -> unmap bo (local)\n", __FUNCTION__ );

     D_ASSERT( notification != NULL );
     D_MAGIC_ASSERT( alloc_local, DRMKMSAllocationLocalData );

     if (notification->flags & CSANF_DEALLOCATED) {
          D_DEBUG_AT( DRMKMS_Surfaces, "  -> handle   %u\n", alloc_local->bo.handle );
          D_DEBUG_AT( DRMKMS_Surfaces, "  -> addr     %p\n", alloc_local->addr );

          struct drm_gem_close cl;
          cl.handle = alloc_local->bo.handle;
          drmIoctl( alloc_local->pool_local->drmkms->fd, DRM_IOCTL_GEM_CLOSE, &cl );

          if (alloc_local->addr)
               kms_bo_unmap( &alloc_local->bo );

          direct_mutex_lock( &alloc_local->pool_local->lock );
          direct_hash_remove( alloc_local->pool_local->hash, alloc_local->alloc_id );
          direct_mutex_unlock( &alloc_local->pool_local->lock );

          D_MAGIC_CLEAR( alloc_local );
          D_FREE( alloc_local );
     }

     return RS_OK;
}

static DFBResult
drmkmsLock( CoreSurfacePool       *pool,
            void                  *pool_data,
            void                  *pool_local,
            CoreSurfaceAllocation *allocation,
            void                  *alloc_data,
            CoreSurfaceBufferLock *lock )
{
     DRMKMSPoolLocalData  *local = pool_local;
     DRMKMSAllocationData *alloc = alloc_data;
     DRMKMSData           *drmkms;
     DRMKMSDataShared     *shared;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( local, DRMKMSPoolLocalData );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, DRMKMSAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( DRMKMS_SurfLock, "%s( %p, %p )\n", __FUNCTION__, allocation, lock->buffer );

     drmkms = local->drmkms;
     D_ASSERT( drmkms != NULL );

     shared = drmkms->shared;
     D_ASSERT( shared != NULL );

     lock->pitch  = alloc->pitch;
     lock->offset = ~0;
     lock->addr   = dfb_core_is_master( local->core ) ? alloc->addr : NULL;
     lock->phys   = 0;

     switch (lock->accessor) {
          case CSAID_LAYER0:
               lock->handle = (void*)(long) alloc->fb_id;
               D_DEBUG_AT( DRMKMS_Surfaces, "  -> primary layer buffer (handle %u)\n", alloc->fb_id );
               break;

          case CSAID_GPU:
               if (shared->use_prime_fd)
                    lock->offset = alloc->prime_fd;

               lock->handle = (void*)(long) alloc->handle;
               D_DEBUG_AT( DRMKMS_Surfaces, "  -> primary accelerator buffer (handle %u)\n", alloc->handle );
               break;

          case CSAID_CPU:
               lock->handle = (void*)(long) alloc->name;
               D_DEBUG_AT( DRMKMS_Surfaces, "  -> local processor buffer (handle %u)\n", alloc->name );

               if (!dfb_core_is_master( local->core )) {
                    DFBResult                  ret;
                    DRMKMSAllocationLocalData *alloc_local;

                    alloc_local = direct_hash_lookup( local->hash, allocation->object.id );
                    if (!alloc_local) {
                         D_DEBUG_AT( DRMKMS_Surfaces, "  -> map bo (local)\n" );

                         alloc_local = D_CALLOC( 1, sizeof(DRMKMSAllocationLocalData) );
                         if (!alloc_local)
                              return D_OOM();

                         alloc_local->pool_local = local;
                         alloc_local->alloc_id   = allocation->object.id;

                         struct drm_gem_open op;
                         op.name = alloc->name;
                         if (drmIoctl( drmkms->fd, DRM_IOCTL_GEM_OPEN, &op ) < 0) {
                              ret = errno2result( errno );
                              D_PERROR( "DRMKMS/Surfaces: DRM_IOCTL_GEM_OPEN( %u ) failed!\n", alloc->name );
                              D_FREE( alloc_local );
                              return ret;
                         }

                         D_DEBUG_AT( DRMKMS_Surfaces, "  -> name     %u => handle %u\n", alloc->name, op.handle );

                         alloc_local->bo.kms    = drmkms->kms;
                         alloc_local->bo.handle = op.handle;
                         alloc_local->bo.pitch  = alloc->pitch;
                         alloc_local->bo.size   = alloc->size;

                         if (kms_bo_map( &alloc_local->bo, &alloc_local->addr ) < 0) {
                              ret = errno2result( errno );
                              D_PERROR( "DRMKMS/Surfaces: kms_bo_map( %u ) failed!\n", alloc->handle );
                              struct drm_gem_close cl;
                              cl.handle = alloc_local->bo.handle;
                              drmIoctl( drmkms->fd, DRM_IOCTL_GEM_CLOSE, &cl );
                              D_FREE( alloc_local );
                              return ret;
                         }

                         D_DEBUG_AT( DRMKMS_Surfaces, "  -> addr     %p\n", alloc_local->addr );

                         D_MAGIC_SET( alloc_local, DRMKMSAllocationLocalData );

                         direct_hash_insert( local->hash, allocation->object.id, alloc_local );

                         dfb_surface_allocation_attach( allocation, drmkmsAllocationReaction, alloc_local,
                                                        &alloc_local->reaction );
                    }
                    else
                         D_MAGIC_ASSERT( alloc_local, DRMKMSAllocationLocalData );

                    lock->addr = alloc_local->addr;
               }
               break;

          default:
               D_BUG( "unsupported accessor %u", lock->accessor );
               break;
     }

     D_DEBUG_AT( DRMKMS_SurfLock, "  -> offset %lu, pitch %u, addr %p, phys 0x%08lx\n",
                 lock->offset, lock->pitch, lock->addr, lock->phys );

     return DFB_OK;
}

static DFBResult
drmkmsUnlock( CoreSurfacePool       *pool,
              void                  *pool_data,
              void                  *pool_local,
              CoreSurfaceAllocation *allocation,
              void                  *alloc_data,
              CoreSurfaceBufferLock *lock )
{
     DRMKMSAllocationData *alloc = alloc_data;

     D_UNUSED_P( alloc );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, DRMKMSAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( DRMKMS_SurfLock, "%s( %p, %p )\n", __FUNCTION__, allocation, lock->buffer );

     return DFB_OK;
}

const SurfacePoolFuncs drmkmsSurfacePoolFuncs = {
     .PoolDataSize       = drmkmsPoolDataSize,
     .PoolLocalDataSize  = drmkmsPoolLocalDataSize,
     .AllocationDataSize = drmkmsAllocationDataSize,
     .InitPool           = drmkmsInitPool,
     .JoinPool           = drmkmsJoinPool,
     .DestroyPool        = drmkmsDestroyPool,
     .LeavePool          = drmkmsLeavePool,
     .TestConfig         = drmkmsTestConfig,
     .AllocateBuffer     = drmkmsAllocateBuffer,
     .DeallocateBuffer   = drmkmsDeallocateBuffer,
     .Lock               = drmkmsLock,
     .Unlock             = drmkmsUnlock
};

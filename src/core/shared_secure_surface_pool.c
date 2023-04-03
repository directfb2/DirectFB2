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
#include <core/surface_allocation.h>
#include <core/surface_buffer.h>
#include <core/surface_pool.h>
#include <core/system.h>
#include <direct/filesystem.h>
#include <fusion/conf.h>
#include <fusion/fusion.h>

D_DEBUG_DOMAIN( Core_SharedSecure, "Core/SharedSecure", "DirectFB Core Shared Secure Surface Pool" );

/**********************************************************************************************************************/

typedef struct {
     char tmpfs_dir[FUSION_SHM_TMPFS_PATH_NAME_LEN + 20];
} SharedPoolData;

typedef struct {
     CoreDFB     *core;
     FusionWorld *world;
} SharedPoolLocalData;

typedef struct {
     int           pitch;
     int           size;
     DFBSurfaceID  surface_id;
     void         *master_map;
} SharedAllocationData;

/**********************************************************************************************************************/

static int
sharedSecurePoolDataSize( void )
{
     return sizeof(SharedPoolData);
}

static int
sharedSecurePoolLocalDataSize( void )
{
     return sizeof(SharedPoolLocalData);
}

static int
sharedSecureAllocationDataSize( void )
{
     return sizeof(SharedAllocationData);
}

static DFBResult
sharedSecureInitPool( CoreDFB                    *core,
                      CoreSurfacePool            *pool,
                      void                       *pool_data,
                      void                       *pool_local,
                      void                       *system_data,
                      CoreSurfacePoolDescription *ret_desc )
{
     DirectResult         ret;
     DirectDir            dir;
     DirectEntry          entry;
     SharedPoolData      *data  = pool_data;
     SharedPoolLocalData *local = pool_local;

     D_DEBUG_AT( Core_SharedSecure, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( ret_desc != NULL );

     ret_desc->caps              = CSPCAPS_VIRTUAL;
     ret_desc->access[CSAID_CPU] = CSAF_READ | CSAF_WRITE | CSAF_SHARED;
     ret_desc->types             = CSTF_LAYER | CSTF_WINDOW | CSTF_CURSOR | CSTF_FONT | CSTF_SHARED | CSTF_INTERNAL;
     ret_desc->priority          = (dfb_system_caps() & CSCAPS_PREFER_SHM) ? CSPP_PREFERED : CSPP_DEFAULT;

     if (dfb_system_caps() & CSCAPS_SYSMEM_EXTERNAL)
          ret_desc->types |= CSTF_EXTERNAL;

     snprintf( ret_desc->name, DFB_SURFACE_POOL_DESC_NAME_LENGTH, "Shared Secure Memory" );

     local->core  = core;
     local->world = dfb_core_world( core );

     snprintf( data->tmpfs_dir, sizeof(data->tmpfs_dir), "%s/dfb.%d",
               fusion_get_tmpfs( local->world ), fusion_world_index( local->world ) );

     ret = direct_dir_create( data->tmpfs_dir, 0750 );
     if (ret) {
          if (ret != DR_BUSY) {
               D_DERROR( ret, "Core/SharedSecure: Could not create '%s'!\n", data->tmpfs_dir );
               return DFB_IO;
          }

          D_DEBUG_AT( Core_SharedSecure, "  -> %s exists, cleaning up\n", data->tmpfs_dir );

          ret = direct_dir_open ( &dir, data->tmpfs_dir );
          if (ret) {
               D_DERROR( ret, "Core/SharedSecure: Could not open '%s'!\n", data->tmpfs_dir );
               return DFB_IO;
          }

          while (direct_dir_read( &dir, &entry ) == DR_OK) {
               char buf[FUSION_SHM_TMPFS_PATH_NAME_LEN + 99];

               if (!strcmp( entry.name, "." ) || !strcmp( entry.name, ".." ))
                    continue;

               snprintf( buf, sizeof(buf), "%s/%s", data->tmpfs_dir, entry.name );

               ret = direct_unlink( buf );
               if (ret) {
                    D_DERROR( ret, "Core/SharedSecure: Could not remove '%s'!\n", buf );
                    direct_dir_close( &dir );
                    return DFB_IO;
               }
          }

          direct_dir_close( &dir );
     }

     return DFB_OK;
}

static DFBResult
sharedSecureDestroyPool( CoreSurfacePool *pool,
                         void            *pool_data,
                         void            *pool_local )
{
     DirectResult    ret;
     SharedPoolData *data = pool_data;

     D_DEBUG_AT( Core_SharedSecure, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     ret = direct_dir_remove( data->tmpfs_dir );
     if (ret)
          D_DERROR( ret, "Core/SharedSecure: Could not remove '%s'!\n", data->tmpfs_dir );

     return DFB_OK;
}

static DFBResult
sharedSecureAllocateBuffer( CoreSurfacePool       *pool,
                            void                  *pool_data,
                            void                  *pool_local,
                            CoreSurfaceBuffer     *buffer,
                            CoreSurfaceAllocation *allocation,
                            void                  *alloc_data )
{
     DirectResult          ret;
     CoreSurface          *surface;
     SharedPoolData       *data  = pool_data;
     SharedAllocationData *alloc = alloc_data;
     char                  buf[FUSION_SHM_TMPFS_PATH_NAME_LEN + 99];
     DirectFile            fd;

     D_DEBUG_AT( Core_SharedSecure, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_MAGIC_ASSERT( buffer->surface, CoreSurface );

     surface = buffer->surface;

     alloc->surface_id = surface->object.id;

     dfb_surface_calc_buffer_size( surface, 8, 0, &alloc->pitch, &alloc->size );

     snprintf( buf, sizeof(buf), "%s/surface_0x%08x_shared_allocation_%p", data->tmpfs_dir, alloc->surface_id, alloc );

     ret = direct_file_open( &fd, buf, O_RDWR | O_CREAT | O_EXCL, 0660 );
     if (ret) {
          D_DERROR( ret, "Core/SharedSecure: Could not create '%s'!\n", buf );
          return DFB_IO;
     }

     if (fusion_config->shmfile_gid != -1) {
          if (direct_file_chown( &fd, -1, fusion_config->shmfile_gid ))
               D_WARN( "changing owner on %s failed... continuing on.", buf );
     }

     direct_file_chmod( &fd, 0660 );

     if (direct_file_truncate( &fd, alloc->size )) {
          D_DERROR( ret, "Core/SharedSecure: Setting file size for '%s' to %d failed!\n", buf, alloc->size );
          ret = direct_unlink( buf );
          if (ret)
               D_DERROR( ret, "Core/SharedSecure: Could not remove '%s'!\n", buf );
          return DFB_IO;
     }

     ret = direct_file_map( &fd, NULL, 0, alloc->size, DFP_READ | DFP_WRITE, &alloc->master_map );

     direct_file_close( &fd );

     if (ret) {
          D_DERROR( ret, "Core/SharedSecure: Could not mmap '%s'!\n", buf );

          ret = direct_unlink( buf );
          if (ret)
               D_DERROR( ret, "Core/SharedSecure: Could not remove '%s'!\n", buf );

          return DFB_IO;
     }

     allocation->flags = CSALF_VOLATILE;
     allocation->size  = alloc->size;

     return DFB_OK;
}

static DFBResult
sharedSecureDeallocateBuffer( CoreSurfacePool       *pool,
                              void                  *pool_data,
                              void                  *pool_local,
                              CoreSurfaceBuffer     *buffer,
                              CoreSurfaceAllocation *allocation,
                              void                  *alloc_data )
{
     DirectResult          ret;
     SharedPoolData       *data  = pool_data;
     SharedAllocationData *alloc = alloc_data;
     char                  buf[FUSION_SHM_TMPFS_PATH_NAME_LEN + 99];

     D_DEBUG_AT( Core_SharedSecure, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     snprintf( buf, sizeof(buf), "%s/surface_0x%08x_shared_allocation_%p", data->tmpfs_dir, alloc->surface_id, alloc );

     direct_file_unmap( alloc->master_map, alloc->size );

     ret = direct_unlink( buf );
     if (ret) {
          D_DERROR( ret, "Core/SharedSecure: Could not remove '%s'!\n", buf );
          return DFB_IO;
     }

     return DFB_OK;
}

static DFBResult
sharedSecureLock( CoreSurfacePool       *pool,
                  void                  *pool_data,
                  void                  *pool_local,
                  CoreSurfaceAllocation *allocation,
                  void                  *alloc_data,
                  CoreSurfaceBufferLock *lock )
{
     DirectResult          ret;
     SharedPoolData       *data  = pool_data;
     SharedAllocationData *alloc = alloc_data;
     char                  buf[FUSION_SHM_TMPFS_PATH_NAME_LEN + 99];
     DirectFile            fd;

     D_DEBUG_AT( Core_SharedSecure, "%s() <- size %d\n", __FUNCTION__, alloc->size );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     if (dfb_core_is_master( core_dfb )) {
          lock->addr = alloc->master_map;
     }
     else {
          snprintf( buf, sizeof(buf), "%s/surface_0x%08x_shared_allocation_%p",
                    data->tmpfs_dir, alloc->surface_id, alloc );

          ret = direct_file_open( &fd, buf, O_RDWR, 0 );
          if (ret) {
               D_DERROR( ret, "Core/SharedSecure: Could not open '%s'!\n", buf );
               return DFB_IO;
          }

          ret = direct_file_map( &fd, NULL, 0, alloc->size, DFP_READ | DFP_WRITE, &lock->handle );

          lock->addr = lock->handle;

          D_DEBUG_AT( Core_SharedSecure, "  -> mapped to %p\n", lock->addr );

          direct_file_close( &fd );

          if (ret) {
               D_DERROR( ret, "Core/SharedSecure: Could not mmap '%s'!\n", buf );
               return DFB_IO;
          }
     }

     lock->pitch = alloc->pitch;

     return DFB_OK;
}

static DFBResult
sharedSecureUnlock( CoreSurfacePool       *pool,
                    void                  *pool_data,
                    void                  *pool_local,
                    CoreSurfaceAllocation *allocation,
                    void                  *alloc_data,
                    CoreSurfaceBufferLock *lock )
{
     SharedAllocationData *alloc = alloc_data;

     D_DEBUG_AT( Core_SharedSecure, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     if (!dfb_core_is_master( core_dfb ))
          direct_file_unmap( lock->handle, alloc->size );

     return DFB_OK;
}

const SurfacePoolFuncs sharedSecureSurfacePoolFuncs = {
     .PoolDataSize       = sharedSecurePoolDataSize,
     .PoolLocalDataSize  = sharedSecurePoolLocalDataSize,
     .AllocationDataSize = sharedSecureAllocationDataSize,
     .InitPool           = sharedSecureInitPool,
     .DestroyPool        = sharedSecureDestroyPool,
     .AllocateBuffer     = sharedSecureAllocateBuffer,
     .DeallocateBuffer   = sharedSecureDeallocateBuffer,
     .Lock               = sharedSecureLock,
     .Unlock             = sharedSecureUnlock
};

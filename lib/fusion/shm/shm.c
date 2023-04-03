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

#include <direct/filesystem.h>
#include <fusion/conf.h>
#include <fusion/fusion_internal.h>
#include <fusion/shm/pool.h>
#include <fusion/shm/shm.h>

D_DEBUG_DOMAIN( Fusion_SHMInit, "Fusion/SHMInit", "Fusion Shared Memory Init" );

/**********************************************************************************************************************/

int
fusion_find_tmpfs( char *name,
                   int   len )
{
     int        largest = 0;
     char       buffer[1024];
     DirectFile mounts_handle;

     if (direct_file_open( &mounts_handle, "/proc/mounts", O_RDONLY, 0 ))
          return 0;

     while (!direct_file_get_string( &mounts_handle, buffer, sizeof(buffer) )) {
          char *mount_point;
          char *mount_fs;
          char *pointer = buffer;

          strsep( &pointer, " " );

          mount_point = strsep( &pointer, " " );
          mount_fs = strsep( &pointer, " " );

          if (mount_fs && mount_point && (direct_access( mount_point, W_OK ) == DR_OK) &&
             (!strcmp( mount_fs, "tmpfs" ) || !strcmp( mount_fs, "shmfs" ) || !strcmp( mount_fs, "ramfs" ))) {
               size_t size;

               if (direct_filesystem_size( mount_point, &size )) {
                    D_ERROR( "Fusion/SHMInit: Failed to get filesystem size on '%s'!\n", mount_point );
                    continue;
               }

               if (size > largest || (size == largest && !strcmp( mount_point, "/dev/shm" ))) {
                    largest = size;

                    direct_snputs( name, mount_point, len );
               }
          }
     }

     direct_file_close( &mounts_handle );

     D_DEBUG_AT( Fusion_SHMInit, "%s( %s )\n", __FUNCTION__, name );

     return largest;
}

DirectResult
fusion_shm_init( FusionWorld *world )
{
     int           i;
     int           num;
     DirectResult  ret;
     FusionSHM    *shm;

     D_MAGIC_ASSERT( world, FusionWorld );
     D_MAGIC_ASSERT( world->shared, FusionWorldShared );

     D_DEBUG_AT( Fusion_SHMInit, "%s( %p )\n", __FUNCTION__, world );

     shm = &world->shm;

     /* Initialize local data. */
     memset( shm, 0, sizeof(FusionSHM) );

     shm->world  = world;
     shm->shared = &world->shared->shm;

     /* Initialize shared data. */
     if (fusion_master( world )) {
          memset( shm->shared, 0, sizeof(FusionSHMShared) );

          if (fusion_config->tmpfs) {
               direct_snputs( shm->shared->tmpfs, fusion_config->tmpfs, FUSION_SHM_TMPFS_PATH_NAME_LEN );
          }
          else if (!fusion_find_tmpfs( shm->shared->tmpfs, FUSION_SHM_TMPFS_PATH_NAME_LEN )) {
               D_ERROR( "Fusion/SHMInit: Could not find tmpfs mount point, falling back to /dev/shm!\n" );
               snprintf( shm->shared->tmpfs, FUSION_SHM_TMPFS_PATH_NAME_LEN, "/dev/shm" );
          }

          shm->shared->world = world->shared;

          /* Initialize shared lock. */
          ret = fusion_skirmish_init2( &shm->shared->lock, "Fusion SHM", world, fusion_config->secure_fusion );
          if (ret) {
               D_DERROR( ret, "Fusion/SHMInit: Failed to create skirmish!\n" );
               return ret;
          }

          /* Initialize static pool array. */
          for (i = 0; i < FUSION_SHM_MAX_POOLS; i++)
               shm->shared->pools[i].index = i;

          D_MAGIC_SET( shm, FusionSHM );
          D_MAGIC_SET( shm->shared, FusionSHMShared );
     }
     else {
          D_MAGIC_ASSERT( shm->shared, FusionSHMShared );

          D_MAGIC_SET( shm, FusionSHM );

          for (i = 0, num = 0; i < FUSION_SHM_MAX_POOLS; i++) {
               if (shm->shared->pools[i].active) {
                    D_MAGIC_ASSERT( &shm->shared->pools[i], FusionSHMPoolShared );

                    ret = fusion_shm_pool_attach( shm, &shm->shared->pools[i] );
                    if (ret) {
                         for (--i; i >= 0; i--) {
                              if (shm->shared->pools[i].active)
                                   fusion_shm_pool_detach( shm, &shm->shared->pools[i] );
                         }

                         D_MAGIC_CLEAR( shm );

                         return ret;
                    }

                    num++;
               }
          }

          D_ASSERT( num == shm->shared->num_pools );
     }

     return DR_OK;
}

DirectResult
fusion_shm_deinit( FusionWorld *world )
{
     int           i;
     DirectResult  ret;
     FusionSHM    *shm;

     D_MAGIC_ASSERT( world, FusionWorld );

     D_DEBUG_AT( Fusion_SHMInit, "%s( %p )\n", __FUNCTION__, world );

     shm = &world->shm;

     D_MAGIC_ASSERT( shm, FusionSHM );
     D_MAGIC_ASSERT( shm->shared, FusionSHMShared );

     /* Deinitialize shared data. */
     if (fusion_master( world )) {
          ret = fusion_skirmish_prevail( &shm->shared->lock );
          if (ret)
               return ret;

          D_ASSUME( shm->shared->num_pools == 0 );

          for (i = 0; i < FUSION_SHM_MAX_POOLS; i++) {
               if (shm->shared->pools[i].active) {
                    D_MAGIC_ASSERT( &shm->shared->pools[i], FusionSHMPoolShared );
                    D_MAGIC_ASSERT( &shm->pools[i], FusionSHMPool );

                    D_WARN( "destroying remaining '%s'", shm->shared->pools[i].name );

                    fusion_shm_pool_destroy( world, &shm->shared->pools[i] );
               }
          }

          /* Destroy shared lock. */
          fusion_skirmish_destroy( &shm->shared->lock );

          D_MAGIC_CLEAR( shm->shared );
     }
     else {
          for (i = 0; i < FUSION_SHM_MAX_POOLS; i++) {
               if (shm->shared->pools[i].active) {
                    D_MAGIC_ASSERT( &shm->shared->pools[i], FusionSHMPoolShared );
                    D_MAGIC_ASSERT( &shm->pools[i], FusionSHMPool );

                    fusion_shm_pool_detach( shm, &shm->shared->pools[i] );
               }
          }
     }

     D_MAGIC_CLEAR( shm );

     return DR_OK;
}

DirectResult
fusion_shm_enum_pools( FusionWorld           *world,
                       FusionSHMPoolCallback  callback,
                       void                  *ctx )
{
     int        i;
     FusionSHM *shm;

     D_MAGIC_ASSERT( world, FusionWorld );
     D_MAGIC_ASSERT( world->shared, FusionWorldShared );
     D_ASSERT( callback != NULL );

     shm = &world->shm;

     D_MAGIC_ASSERT( shm, FusionSHM );
     D_MAGIC_ASSERT( shm->shared, FusionSHMShared );

     for (i = 0; i < FUSION_SHM_MAX_POOLS; i++) {
          if (!shm->shared->pools[i].active)
               continue;

          if (!shm->pools[i].attached) {
               D_BUG( "not attached to pool" );
               continue;
          }

          D_MAGIC_ASSERT( &shm->pools[i], FusionSHMPool );
          D_MAGIC_ASSERT( &shm->shared->pools[i], FusionSHMPoolShared );

          if (callback( &shm->pools[i], ctx ) == DENUM_CANCEL)
               break;
     }

     return DR_OK;
}

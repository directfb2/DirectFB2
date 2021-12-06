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
#include <direct/mem.h>
#include <fusion/shmalloc.h>
#include <fusion/fusion_internal.h>
#include <fusion/shm/pool.h>

#if FUSION_BUILD_KERNEL
#include <fusion/conf.h>
#else /* FUSION_BUILD_KERNEL */
#include <direct/system.h>
#endif /* FUSION_BUILD_KERNEL */

D_DEBUG_DOMAIN( Fusion_SHMPool, "Fusion/SHMPool", "Fusion Shared Memory Pool" );

/**********************************************************************************************************************/

static DirectResult init_pool    ( FusionSHM           *shm,
                                   FusionSHMPool       *pool,
                                   FusionSHMPoolShared *shared,
                                   const char          *name,
                                   unsigned int         max_size,
                                   bool                 debug );

static DirectResult join_pool    ( FusionSHM           *shm,
                                   FusionSHMPool       *pool,
                                   FusionSHMPoolShared *shared );

static void         leave_pool   ( FusionSHM           *shm,
                                   FusionSHMPool       *pool,
                                   FusionSHMPoolShared *shared );

static void         shutdown_pool( FusionSHM           *shm,
                                   FusionSHMPool       *pool,
                                   FusionSHMPoolShared *shared );

/**********************************************************************************************************************/

DirectResult
fusion_shm_pool_create( FusionWorld          *world,
                        const char           *name,
                        unsigned int          max_size,
                        bool                  debug,
                        FusionSHMPoolShared **ret_pool )
{
     int           i;
     DirectResult  ret;
     FusionSHM    *shm;

     D_MAGIC_ASSERT( world, FusionWorld );
     D_MAGIC_ASSERT( world->shared, FusionWorldShared );
     D_ASSERT( name != NULL );
     D_ASSERT( max_size > 0 );
     D_ASSERT( ret_pool != NULL );

     D_DEBUG_AT( Fusion_SHMPool, "%s( %p [%d], '%s', %u, %p, %sdebug )\n", __FUNCTION__,
                 world, world->shared->world_index, name, max_size, ret_pool, debug ? "" : "no-" );

     shm = &world->shm;

     D_MAGIC_ASSERT( shm, FusionSHM );
     D_MAGIC_ASSERT( shm->shared, FusionSHMShared );

     if (max_size < 8192) {
          D_ERROR( "Fusion/SHMPool: Maximum size (%u) should be 8192 at least!\n", max_size );
          return DR_INVARG;
     }

     ret = fusion_skirmish_prevail( &shm->shared->lock );
     if (ret)
          goto error;

     if (shm->shared->num_pools == FUSION_SHM_MAX_POOLS) {
          D_ERROR( "Fusion/SHMPool: Maximum number of pools (%d) already reached!\n", FUSION_SHM_MAX_POOLS );
          ret = DR_LIMITEXCEEDED;
          goto error;
     }

     for (i = 0; i < FUSION_SHM_MAX_POOLS; i++) {
          if (!shm->shared->pools[i].active)
               break;

          D_MAGIC_ASSERT( &shm->shared->pools[i], FusionSHMPoolShared );

          D_MAGIC_ASSUME( &shm->pools[i], FusionSHMPool );
     }

     D_ASSERT( i < FUSION_SHM_MAX_POOLS );

     D_DEBUG_AT( Fusion_SHMPool, "  -> index %d\n", i );

     memset( &shm->pools[i], 0, sizeof(FusionSHMPool) );
     memset( &shm->shared->pools[i], 0, sizeof(FusionSHMPoolShared) );

     shm->shared->pools[i].index = i;

     ret = init_pool( shm, &shm->pools[i], &shm->shared->pools[i], name, max_size, debug );
     if (ret)
          goto error;

     shm->shared->num_pools++;

     fusion_skirmish_dismiss( &shm->shared->lock );

     *ret_pool = &shm->shared->pools[i];

     D_DEBUG_AT( Fusion_SHMPool, "  -> %p\n", *ret_pool );

     return DR_OK;

error:
     fusion_skirmish_dismiss( &shm->shared->lock );

     return ret;
}

DirectResult
fusion_shm_pool_destroy( FusionWorld         *world,
                         FusionSHMPoolShared *pool )
{
     DirectResult  ret;
     FusionSHM    *shm;

     D_DEBUG_AT( Fusion_SHMPool, "%s( %p, %p )\n", __FUNCTION__, world, pool );

     D_MAGIC_ASSERT( world, FusionWorld );
     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );

     shm = &world->shm;

     D_MAGIC_ASSERT( shm, FusionSHM );
     D_MAGIC_ASSERT( shm->shared, FusionSHMShared );
     D_ASSERT( shm->shared == pool->shm );

     ret = fusion_skirmish_prevail( &shm->shared->lock );
     if (ret)
          return ret;

     ret = fusion_skirmish_prevail( &pool->lock );
     if (ret) {
          fusion_skirmish_dismiss( &shm->shared->lock );
          return ret;
     }

     D_ASSERT( pool->active );
     D_ASSERT( pool->index >= 0 );
     D_ASSERT( pool->index < FUSION_SHM_MAX_POOLS );
     D_ASSERT( pool->pool_id == shm->pools[pool->index].pool_id );
     D_ASSERT( pool == &shm->shared->pools[pool->index] );
     D_MAGIC_ASSERT( &shm->pools[pool->index], FusionSHMPool );

     shutdown_pool( shm, &shm->pools[pool->index], pool );

     shm->shared->num_pools--;

     fusion_skirmish_dismiss( &shm->shared->lock );

     return DR_OK;
}

DirectResult
fusion_shm_pool_attach( FusionSHM           *shm,
                        FusionSHMPoolShared *pool )
{
     D_DEBUG_AT( Fusion_SHMPool, "%s( %p, %p )\n", __FUNCTION__, shm, pool );

     D_MAGIC_ASSERT( shm, FusionSHM );
     D_MAGIC_ASSERT( shm->shared, FusionSHMShared );
     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( shm->shared == pool->shm );
     D_ASSERT( pool->active );
     D_ASSERT( pool->index >= 0 );
     D_ASSERT( pool->index < FUSION_SHM_MAX_POOLS );
     D_ASSERT( pool == &shm->shared->pools[pool->index] );
     D_ASSERT( !shm->pools[pool->index].attached );

     return join_pool( shm, &shm->pools[pool->index], pool );
}

DirectResult
fusion_shm_pool_detach( FusionSHM           *shm,
                        FusionSHMPoolShared *pool )
{
     D_DEBUG_AT( Fusion_SHMPool, "%s( %p, %p )\n", __FUNCTION__, shm, pool );

     D_MAGIC_ASSERT( shm, FusionSHM );
     D_MAGIC_ASSERT( shm->shared, FusionSHMShared );
     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( shm->shared == pool->shm );
     D_ASSERT( pool->active );
     D_ASSERT( pool->index >= 0 );
     D_ASSERT( pool->index < FUSION_SHM_MAX_POOLS );
     D_ASSERT( pool->pool_id == shm->pools[pool->index].pool_id );
     D_ASSERT( pool == &shm->shared->pools[pool->index] );
     D_ASSERT( shm->pools[pool->index].attached );
     D_MAGIC_ASSERT( &shm->pools[pool->index], FusionSHMPool );

     leave_pool( shm, &shm->pools[pool->index], pool );

     return DR_OK;
}

DirectResult
fusion_shm_pool_allocate( FusionSHMPoolShared  *pool,
                          int                   size,
                          bool                  clear,
                          bool                  lock,
                          void                **ret_data )
{
     DirectResult  ret;
     void         *data;

     D_DEBUG_AT( Fusion_SHMPool, "%s( %p, %d, %sclear, %p )\n", __FUNCTION__, pool, size, clear ? "" : "un", ret_data );

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( size > 0 );
     D_ASSERT( ret_data != NULL );

     if (lock) {
          ret = fusion_skirmish_prevail( &pool->lock );
          if (ret)
               return ret;
     }

     __shmalloc_brk( pool->heap, 0 );

     data = _fusion_shmalloc( pool->heap, size );
     if (!data) {
          if (lock)
               fusion_skirmish_dismiss( &pool->lock );
          return DR_NOSHAREDMEMORY;
     }

     if (clear)
          memset( data, 0, size );

     *ret_data = data;

     if (lock)
          fusion_skirmish_dismiss( &pool->lock );

     return DR_OK;
}

DirectResult
fusion_shm_pool_reallocate( FusionSHMPoolShared  *pool,
                            void                 *data,
                            int                   size,
                            bool                  lock,
                            void                **ret_data )
{
     DirectResult  ret;
     void         *new_data;

     D_DEBUG_AT( Fusion_SHMPool, "%s( %p, %p, %d, %p )\n", __FUNCTION__, pool, data, size, ret_data );

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( data != NULL );
     D_ASSERT( data >= pool->addr_base );
     D_ASSERT( data < pool->addr_base + pool->max_size );
     D_ASSERT( size > 0 );
     D_ASSERT( ret_data != NULL );

     if (lock) {
          ret = fusion_skirmish_prevail( &pool->lock );
          if (ret)
               return ret;
     }

     __shmalloc_brk( pool->heap, 0 );

     new_data = _fusion_shrealloc( pool->heap, data, size );
     if (!new_data) {
          if (lock)
               fusion_skirmish_dismiss( &pool->lock );
          return DR_NOSHAREDMEMORY;
     }

     *ret_data = new_data;

     if (lock)
          fusion_skirmish_dismiss( &pool->lock );

     return DR_OK;
}

DirectResult
fusion_shm_pool_deallocate( FusionSHMPoolShared *pool,
                            void                *data,
                            bool                 lock )
{
     DirectResult ret;

     D_DEBUG_AT( Fusion_SHMPool, "%s( %p, %p )\n", __FUNCTION__, pool, data );

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( data != NULL );
     D_ASSERT( data >= pool->addr_base );
     D_ASSERT( data < pool->addr_base + pool->max_size );

     if (lock) {
          ret = fusion_skirmish_prevail( &pool->lock );
          if (ret)
               return ret;
     }

     __shmalloc_brk( pool->heap, 0 );

     _fusion_shfree( pool->heap, data );

     if (lock)
          fusion_skirmish_dismiss( &pool->lock );

     return DR_OK;
}

/**********************************************************************************************************************/

#if FUSION_BUILD_KERNEL

static DirectResult
init_pool( FusionSHM           *shm,
           FusionSHMPool       *pool,
           FusionSHMPoolShared *shared,
           const char          *name,
           unsigned int         max_size,
           bool                 debug )
{
     DirectResult         ret;
     int                  size;
     FusionWorld         *world;
     FusionSHMPoolNew     pool_new    = { .pool_id = 0 };
     FusionSHMPoolAttach  pool_attach = { .pool_id = 0 };
     FusionEntryInfo      info;
     char                 buf[FUSION_SHM_TMPFS_PATH_NAME_LEN + 32];

     D_DEBUG_AT( Fusion_SHMPool, "%s( %p, %p, %p, '%s', %u, %sdebug )\n", __FUNCTION__,
                 shm, pool, shared, name, max_size, debug ? "" : "no-" );

     D_MAGIC_ASSERT( shm, FusionSHM );
     D_MAGIC_ASSERT( shm->shared, FusionSHMShared );
     D_MAGIC_ASSERT( shm->world, FusionWorld );
     D_ASSERT( pool != NULL );
     D_ASSERT( shared != NULL );
     D_ASSERT( name != NULL );
     D_ASSERT( max_size > sizeof(shmalloc_heap) );

     world = shm->world;

     /* Fill out information for new pool. */
     pool_new.max_size = max_size;

     pool_new.max_size += BLOCKALIGN( sizeof(shmalloc_heap) ) +
                          BLOCKALIGN( (max_size + BLOCKSIZE - 1) / BLOCKSIZE * sizeof(shmalloc_info) );

     /* Create the new pool. */
     while (ioctl( world->fusion_fd, FUSION_SHMPOOL_NEW, &pool_new )) {
          if (errno == EINTR)
               continue;

          D_PERROR( "Fusion/SHMPool: FUSION_SHMPOOL_NEW" );
          return DR_FUSION;
     }

     /* Set the pool info. */
     info.type = FT_SHMPOOL;
     info.id   = pool_new.pool_id;

     snprintf( info.name, sizeof(info.name), "%s", name );

     ioctl( world->fusion_fd, FUSION_ENTRY_SET_INFO, &info );

     fusion_entry_add_permissions( world, FT_SHMPOOL, pool_new.pool_id, 0,
                                   FUSION_SHMPOOL_ATTACH,
                                   FUSION_SHMPOOL_DETACH,
                                   0 );

     /* Set pool to attach to. */
     pool_attach.pool_id = pool_new.pool_id;

     /* Attach to the pool. */
     while (ioctl( world->fusion_fd, FUSION_SHMPOOL_ATTACH, &pool_attach )) {
          if (errno == EINTR)
               continue;

          D_PERROR( "Fusion/SHMPool: FUSION_SHMPOOL_ATTACH" );

          while (ioctl( world->fusion_fd, FUSION_SHMPOOL_DESTROY, &shared->pool_id )) {
               if (errno != EINTR) {
                    D_PERROR( "Fusion/SHMPool: FUSION_SHMPOOL_DESTROY" );
                    break;
               }
          }

          return DR_FUSION;
     }

     /* Generate filename. */
     snprintf( buf, sizeof(buf), "%s/fusion.%d.%d",
               shm->shared->tmpfs, fusion_world_index( shm->world ), pool_new.pool_id );

     /* Initialize the heap. */
     ret = __shmalloc_init_heap( shm, buf, pool_new.addr_base, max_size, &size );
     if (ret) {
          while (ioctl( world->fusion_fd, FUSION_SHMPOOL_DESTROY, &shared->pool_id )) {
               if (errno != EINTR) {
                    D_PERROR( "Fusion/SHMPool: FUSION_SHMPOOL_DESTROY" );
                    break;
               }
          }

          return ret;
     }

     /* Initialize local data. */
     pool->attached = true;
     pool->shm      = shm;
     pool->shared   = shared;
     pool->pool_id  = pool_new.pool_id;
     pool->filename = D_STRDUP( buf );

     /* Initialize shared data. */
     shared->active     = true;
     shared->debug      = debug;
     shared->shm        = shm->shared;
     shared->max_size   = pool_new.max_size;
     shared->pool_id    = pool_new.pool_id;
     shared->addr_base  = pool_new.addr_base;
     shared->heap       = pool_new.addr_base;
     shared->heap->pool = shared;

     fusion_skirmish_init2( &shared->lock, name, world, fusion_config->secure_fusion );

     D_MAGIC_SET( pool, FusionSHMPool );
     D_MAGIC_SET( shared, FusionSHMPoolShared );

     shared->name = SHSTRDUP( shared, name );

     return DR_OK;
}

static DirectResult
join_pool( FusionSHM           *shm,
           FusionSHMPool       *pool,
           FusionSHMPoolShared *shared )
{
     DirectResult         ret;
     FusionWorld         *world;
     FusionSHMPoolAttach  pool_attach = { .pool_id = 0 };
     char                 buf[FUSION_SHM_TMPFS_PATH_NAME_LEN + 32];

     D_DEBUG_AT( Fusion_SHMPool, "%s( %p, %p, %p )\n", __FUNCTION__, shm, pool, shared );

     D_MAGIC_ASSERT( shm, FusionSHM );
     D_MAGIC_ASSERT( shm->shared, FusionSHMShared );
     D_MAGIC_ASSERT( shm->world, FusionWorld );
     D_MAGIC_ASSERT( shared, FusionSHMPoolShared );

     world = shm->world;

     /* Set pool to attach to. */
     pool_attach.pool_id = shared->pool_id;

     /* Attach to the pool. */
     while (ioctl( world->fusion_fd, FUSION_SHMPOOL_ATTACH, &pool_attach )) {
          if (errno == EINTR)
               continue;

          D_PERROR( "Fusion/SHMPool: FUSION_SHMPOOL_ATTACH" );
          return DR_FUSION;
     }

     /* Generate filename. */
     snprintf( buf, sizeof(buf), "%s/fusion.%d.%d",
               shm->shared->tmpfs, fusion_world_index( shm->world ), shared->pool_id );

     /* Join the heap. */
     ret = __shmalloc_join_heap( shm, buf, pool_attach.addr_base, shared->max_size, !fusion_config->secure_fusion );
     if (ret) {
          while (ioctl( world->fusion_fd, FUSION_SHMPOOL_DETACH, &shared->pool_id )) {
               if (errno != EINTR) {
                    D_PERROR( "Fusion/SHMPool: FUSION_SHMPOOL_DETACH" );
                    break;
               }
          }

          return ret;
     }

     /* Initialize local data. */
     pool->attached = true;
     pool->shm      = shm;
     pool->shared   = shared;
     pool->pool_id  = shared->pool_id;
     pool->filename = D_STRDUP( buf );

     D_MAGIC_SET( pool, FusionSHMPool );

     return DR_OK;
}

static void
leave_pool( FusionSHM           *shm,
            FusionSHMPool       *pool,
            FusionSHMPoolShared *shared )
{
     FusionWorld *world;

     D_DEBUG_AT( Fusion_SHMPool, "%s( %p, %p, %p )\n", __FUNCTION__, shm, pool, shared );

     D_MAGIC_ASSERT( shm, FusionSHM );
     D_MAGIC_ASSERT( shm->world, FusionWorld );
     D_MAGIC_ASSERT( pool, FusionSHMPool );
     D_MAGIC_ASSERT( shared, FusionSHMPoolShared );

     world = shm->world;

     while (ioctl( world->fusion_fd, FUSION_SHMPOOL_DETACH, &shared->pool_id )) {
          if (errno != EINTR) {
               D_PERROR( "Fusion/SHMPool: FUSION_SHMPOOL_DETACH" );
               break;
          }
     }

     if (direct_file_unmap( shared->addr_base, shared->max_size ))
          D_ERROR( "Fusion/SHMPool: Could not unmap shared memory file '%s'!\n", pool->filename );

     pool->attached = false;

     D_FREE( pool->filename );

     D_MAGIC_CLEAR( pool );
}

static void
shutdown_pool( FusionSHM           *shm,
               FusionSHMPool       *pool,
               FusionSHMPoolShared *shared )
{
     FusionWorld *world;

     D_DEBUG_AT( Fusion_SHMPool, "%s( %p, %p, %p )\n", __FUNCTION__, shm, pool, shared );

     D_MAGIC_ASSERT( shm, FusionSHM );
     D_MAGIC_ASSERT( shm->world, FusionWorld );
     D_MAGIC_ASSERT( pool, FusionSHMPool );
     D_MAGIC_ASSERT( shared, FusionSHMPoolShared );

     world = shm->world;

     SHFREE( shared, shared->name );

     fusion_print_memleaks( shared );

     while (ioctl( world->fusion_fd, FUSION_SHMPOOL_DESTROY, &shared->pool_id )) {
          if (errno != EINTR) {
               D_PERROR( "Fusion/SHMPool: FUSION_SHMPOOL_DESTROY" );
               break;
          }
     }

     if (direct_file_unmap( shared->addr_base, shared->max_size ))
          D_ERROR( "Fusion/SHMPool: Could not unmap shared memory file '%s'!\n", pool->filename );

     if (direct_unlink( pool->filename ))
          D_ERROR( "Fusion/SHMPool: Could not unlink shared memory file '%s'!\n", pool->filename );

     shared->active = false;

     pool->attached = false;

     D_FREE( pool->filename );

     D_MAGIC_CLEAR( pool );

     fusion_skirmish_destroy( &shared->lock );

     D_MAGIC_CLEAR( shared );
}

void
_fusion_shmpool_process( FusionWorld          *world,
                         int                   pool_id,
                         FusionSHMPoolMessage *msg )
{
     int           i;
     DirectResult  ret;
     FusionSHM    *shm;

     D_DEBUG_AT( Fusion_SHMPool, "%s( %p, %d, %p )\n", __FUNCTION__, world, pool_id, msg );

     D_MAGIC_ASSERT( world, FusionWorld );

     shm = &world->shm;

     D_MAGIC_ASSERT( shm, FusionSHM );
     D_MAGIC_ASSERT( shm->shared, FusionSHMShared );

     ret = fusion_skirmish_prevail( &shm->shared->lock );
     if (ret)
          return;

     for (i = 0; i < FUSION_SHM_MAX_POOLS; i++) {
          if (shm->pools[i].attached) {
               D_MAGIC_ASSERT( &shm->pools[i], FusionSHMPool );

               if (shm->pools[i].pool_id == pool_id) {
                    switch (msg->type) {
                         case FSMT_REMAP:
                              break;

                         case FSMT_UNMAP:
                              D_UNIMPLEMENTED();
                              break;
                    }

                    break;
               }
          }
     }

     fusion_skirmish_dismiss( &shm->shared->lock );
}

#else /* FUSION_BUILD_KERNEL */

static DirectResult
init_pool( FusionSHM           *shm,
           FusionSHMPool       *pool,
           FusionSHMPoolShared *shared,
           const char          *name,
           unsigned int         max_size,
           bool                 debug )
{
     DirectResult  ret;
     int           size;
     long          page_size;
     int           pool_id;
     unsigned int  pool_max_size;
     void         *pool_addr_base = NULL;
     FusionWorld  *world;
     char          buf[FUSION_SHM_TMPFS_PATH_NAME_LEN + 32];

     D_DEBUG_AT( Fusion_SHMPool, "%s( %p, %p, %p, '%s', %u, %sdebug )\n", __FUNCTION__,
                 shm, pool, shared, name, max_size, debug ? "" : "non-" );

     D_MAGIC_ASSERT( shm, FusionSHM );
     D_MAGIC_ASSERT( shm->shared, FusionSHMShared );
     D_MAGIC_ASSERT( shm->world, FusionWorld );
     D_ASSERT( pool != NULL );
     D_ASSERT( shared != NULL );
     D_ASSERT( name != NULL );
     D_ASSERT( max_size > sizeof(shmalloc_heap) );

     world = shm->world;

     page_size = direct_pagesize();

     pool_id = ++world->shared->pool_ids;

     pool_max_size = max_size + BLOCKALIGN( sizeof(shmalloc_heap) ) +
                                BLOCKALIGN( (max_size + BLOCKSIZE - 1) / BLOCKSIZE * sizeof(shmalloc_info) );

     pool_addr_base = world->shared->pool_base;
     world->shared->pool_base += ((pool_max_size + page_size - 1) & ~(page_size - 1)) + page_size;
     if (world->shared->pool_base > world->shared->pool_max)
          return DR_NOSHAREDMEMORY;

     /* Generate filename. */
     snprintf( buf, sizeof(buf), "%s/fusion.%d.%d", shm->shared->tmpfs, fusion_world_index( world ), pool_id );

     /* Initialize the heap. */
     ret = __shmalloc_init_heap( shm, buf, pool_addr_base, max_size, &size );
     if (ret)
          return ret;

     /* initialize local data. */
     pool->attached = true;
     pool->shm      = shm;
     pool->shared   = shared;
     pool->pool_id  = pool_id;
     pool->filename = D_STRDUP( buf );

     /* Initialize shared data. */
     shared->active     = true;
     shared->debug      = debug;
     shared->shm        = shm->shared;
     shared->max_size   = pool_max_size;
     shared->pool_id    = pool_id;
     shared->addr_base  = pool_addr_base;
     shared->heap       = pool_addr_base;
     shared->heap->pool = shared;

     fusion_skirmish_init( &shared->lock, name, world );

     D_MAGIC_SET( pool, FusionSHMPool );
     D_MAGIC_SET( shared, FusionSHMPoolShared );

     shared->name = SHSTRDUP( shared, name );

     return DR_OK;
}

static DirectResult
join_pool( FusionSHM           *shm,
           FusionSHMPool       *pool,
           FusionSHMPoolShared *shared )
{
     DirectResult  ret;
     FusionWorld  *world;
     char          buf[FUSION_SHM_TMPFS_PATH_NAME_LEN + 32];

     D_DEBUG_AT( Fusion_SHMPool, "%s( %p, %p, %p )\n", __FUNCTION__, shm, pool, shared );

     D_MAGIC_ASSERT( shm, FusionSHM );
     D_MAGIC_ASSERT( shm->shared, FusionSHMShared );
     D_MAGIC_ASSERT( shm->world, FusionWorld );
     D_MAGIC_ASSERT( shared, FusionSHMPoolShared );

     world = shm->world;

     /* Generate filename. */
     snprintf( buf, sizeof(buf), "%s/fusion.%d.%d", shm->shared->tmpfs, fusion_world_index( world ), shared->pool_id );

     /* Join the heap. */
     ret = __shmalloc_join_heap( shm, buf, shared->addr_base, shared->max_size, true );
     if (ret)
          return ret;

     /* Initialize local data. */
     pool->attached = true;
     pool->shm      = shm;
     pool->shared   = shared;
     pool->pool_id  = shared->pool_id;
     pool->filename = D_STRDUP( buf );

     D_MAGIC_SET( pool, FusionSHMPool );

     return DR_OK;
}

static void
leave_pool( FusionSHM           *shm,
            FusionSHMPool       *pool,
            FusionSHMPoolShared *shared )
{
     D_DEBUG_AT( Fusion_SHMPool, "%s( %p, %p, %p )\n", __FUNCTION__, shm, pool, shared );

     D_MAGIC_ASSERT( shm, FusionSHM );
     D_MAGIC_ASSERT( shm->world, FusionWorld );
     D_MAGIC_ASSERT( pool, FusionSHMPool );
     D_MAGIC_ASSERT( shared, FusionSHMPoolShared );

     if (direct_file_unmap( shared->addr_base, shared->max_size ))
          D_ERROR( "Fusion/SHMPool: Could not unmap shared memory file '%s'!\n", pool->filename );

     pool->attached = false;

     D_FREE( pool->filename );

     D_MAGIC_CLEAR( pool );
}

static void
shutdown_pool( FusionSHM           *shm,
               FusionSHMPool       *pool,
               FusionSHMPoolShared *shared )
{
     D_DEBUG_AT( Fusion_SHMPool, "%s( %p, %p, %p )\n", __FUNCTION__, shm, pool, shared );

     D_MAGIC_ASSERT( shm, FusionSHM );
     D_MAGIC_ASSERT( shm->world, FusionWorld );
     D_MAGIC_ASSERT( pool, FusionSHMPool );
     D_MAGIC_ASSERT( shared, FusionSHMPoolShared );

     SHFREE( shared, shared->name );

     fusion_print_memleaks( shared );

     if (direct_file_unmap( shared->addr_base, shared->max_size ))
          D_ERROR( "Fusion/SHMPool: Could not unmap shared memory file '%s'!\n", pool->filename );

     if (direct_unlink( pool->filename ))
          D_ERROR( "Fusion/SHMPool: Could not unlink shared memory file '%s'!\n", pool->filename );

     shared->active = false;

     pool->attached = false;

     D_FREE( pool->filename );

     D_MAGIC_CLEAR( pool );

     fusion_skirmish_destroy( &shared->lock );

     D_MAGIC_CLEAR( shared );
}

#endif /* FUSION_BUILD_KERNEL */

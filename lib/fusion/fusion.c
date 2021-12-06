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

#include <direct/direct.h>
#include <direct/mem.h>
#include <direct/thread.h>
#include <fusion/conf.h>
#include <fusion/fusion_internal.h>
#include <fusion/shm/pool.h>

#if FUSION_BUILD_MULTI

#include <direct/filesystem.h>
#include <direct/map.h>
#include <direct/system.h>
#if D_DEBUG_ENABLED
#include <direct/trace.h>
#endif
#include <fusion/shmalloc.h>
#include <fusion/shm/shm.h>

#if FUSION_BUILD_KERNEL
#include <direct/memcpy.h>
#else /* FUSION_BUILD_KERNEL */
#include <fusion/hash.h>
#endif /* FUSION_BUILD_KERNEL */

#else /* FUSION_BUILD_MULTI */

#include <direct/memcpy.h>
#include <fusion/reactor.h>

#endif /* FUSION_BUILD_MULTI */

D_DEBUG_DOMAIN( Fusion_Main,          "Fusion/Main",          "Fusion High level IPC" );
D_DEBUG_DOMAIN( Fusion_Main_Dispatch, "Fusion/Main/Dispatch", "Fusion High level IPC Dispatch" );

/**********************************************************************************************************************/

#if FUSION_BUILD_MULTI

static FusionWorld *fusion_worlds[FUSION_MAX_WORLDS];
static DirectMutex  fusion_worlds_lock = DIRECT_MUTEX_INITIALIZER();

int
_fusion_fd( const FusionWorldShared *shared )
{
     int          index;
     FusionWorld *world;

     D_MAGIC_ASSERT( shared, FusionWorldShared );

     index = shared->world_index;

     D_ASSERT( index >= 0 );
     D_ASSERT( index < FUSION_MAX_WORLDS );

     world = fusion_worlds[index];

     D_MAGIC_ASSERT( world, FusionWorld );

     return world->fusion_fd;
}

FusionID
_fusion_id( const FusionWorldShared *shared )
{
     int          index;
     FusionWorld *world;

     D_MAGIC_ASSERT( shared, FusionWorldShared );

     index = shared->world_index;

     D_ASSERT( index >= 0 );
     D_ASSERT( index < FUSION_MAX_WORLDS );

     world = fusion_worlds[index];

     D_MAGIC_ASSERT( world, FusionWorld );

     return world->fusion_id;
}

FusionWorld *
_fusion_world( const FusionWorldShared *shared )
{
     int          index;
     FusionWorld *world;

     D_MAGIC_ASSERT( shared, FusionWorldShared );

     index = shared->world_index;

     D_ASSERT( index >= 0 );
     D_ASSERT( index < FUSION_MAX_WORLDS );

     world = fusion_worlds[index];

     D_MAGIC_ASSERT( world, FusionWorld );

     return world;
}

static void fusion_fork_handler_prepare( void );
static void fusion_fork_handler_parent ( void );
static void fusion_fork_handler_child  ( void );

static void
init_once()
{
     if (fusion_config->fork_handler)
          direct_thread_atfork( fusion_fork_handler_prepare, fusion_fork_handler_parent, fusion_fork_handler_child );

     fusion_print_madvise();
}

static bool
refs_map_compare( DirectMap  *map,
                  const void *key,
                  void       *object,
                  void       *ctx )
{
     const FusionRefSlaveKey *map_key   = key;
     FusionRefSlaveEntry     *map_entry = object;

     return map_key->fusion_id == map_entry->key.fusion_id && map_key->ref_id == map_entry->key.ref_id;
}

static unsigned int
refs_map_hash( DirectMap  *map,
               const void *key,
               void       *ctx )
{
     const FusionRefSlaveKey *map_key = key;

     return map_key->ref_id * 131 + map_key->fusion_id;
}

static bool
refs_map_slave_compare( DirectMap  *map,
                        const void *key,
                        void       *object,
                        void       *ctx )
{
     const int *map_key   = key;
     FusionRef *map_entry = object;

     return *map_key == map_entry->multi.id;
}

static unsigned int
refs_map_slave_hash( DirectMap  *map,
                     const void *key,
                     void       *ctx )
{
     const int *map_key = key;

     return *map_key;
}

static FusionCallHandlerResult
world_refs_call( int           caller,   /* Fusion id of the caller. */
                 int           call_arg, /* Optional call parameter. */
                 void         *call_ptr, /* Optional call parameter. */
                 void         *ctx,      /* Optional handler context. */
                 unsigned int  serial,
                 int          *ret_val )
{
     FusionWorld         *world = ctx;
     FusionRefSlaveKey    key;
     FusionRefSlaveEntry *slave;

     key.fusion_id = caller;
     key.ref_id    = call_arg;

     direct_mutex_lock( &world->refs_lock );
     slave = direct_map_lookup( world->refs_map, &key );
     direct_mutex_unlock( &world->refs_lock );

     if (!slave) {
          D_WARN( "slave (%d) ref (%d) not found", caller, call_arg );
          return FCHR_RETURN;
     }

     fusion_ref_down( slave->ref, false );

     direct_mutex_lock( &world->refs_lock );

     if (!--slave->refs) {
          direct_map_remove( world->refs_map, &key );

          D_FREE( slave );
     }

     direct_mutex_unlock( &world->refs_lock );

     return FCHR_RETURN;
}

static void *fusion_dispatch_loop( DirectThread *thread, void *arg );

static DirectOnce fusion_init_once = DIRECT_ONCE_INIT();

#if FUSION_BUILD_KERNEL

static int
fusion_try_open( const char *name1,
                 const char *name2,
                 int         flags,
                 bool        error_msg )
{
     int fd;

     fd = open( name1, flags, 660 );
     if (fd < 0) {
          if (errno != ENOENT) {
               if (error_msg)
                    D_PERROR( "Fusion/Main: Opening '%s' failed!\n", name1 );
               return -1;
          }

          fd = open( name2, flags, 660 );
          if (fd < 0 && error_msg) {
               if (errno == ENOENT)
                    D_PERROR( "Fusion/Main: Opening '%s' and '%s' failed!\n", name1, name2 );
               else
                    D_PERROR( "Fusion/Main: Opening '%s' failed!\n", name2 );
          }
     }

     return (fd < 0) ? -1 : fd;
}

static void
fusion_world_fork( FusionWorld *world )
{
     int         fd;
     char        buf1[20];
     char        buf2[20];
     FusionEnter enter;
     FusionFork  fork;

     D_MAGIC_ASSERT( world, FusionWorld );
     D_MAGIC_ASSERT( world->shared, FusionWorldShared );

     snprintf( buf1, sizeof(buf1), "/dev/fusion%d", world->shared->world_index );
     snprintf( buf2, sizeof(buf2), "/dev/fusion/%d", world->shared->world_index );

     /* Open Fusion Kernel Device. */
     fd = fusion_try_open( buf1, buf2, O_RDWR, true );
     if (fd < 0) {
          D_ERROR( "Fusion/Main: Reopening fusion device (world %d) failed!\n", world->shared->world_index );
          raise( SIGTRAP );
     }

     /* Drop "identity" when running another program. */
     if (fcntl( fd, F_SETFD, FD_CLOEXEC ) < 0)
          D_PERROR( "Fusion/Main: Setting FD_CLOEXEC flag failed!\n" );

     /* Fill enter information. */
     enter.api.major = 9;
     enter.api.minor = 0;
     enter.fusion_id = 0;

     /* Enter the fusion world. */
     while (ioctl( fd, FUSION_ENTER, &enter )) {
          if (errno != EINTR) {
               D_PERROR( "Fusion/Main: Could not reenter world '%d'!\n", world->shared->world_index );
               raise( SIGTRAP );
          }
     }

     /* Check for valid Fusion ID. */
     if (!enter.fusion_id) {
          D_ERROR( "Fusion/Main: Got no ID from FUSION_ENTER!\n" );
          raise( SIGTRAP );
     }

     D_DEBUG_AT( Fusion_Main, "  -> Fusion ID 0x%08lx\n", enter.fusion_id );

     /* Fill fork information. */
     fork.fusion_id = world->fusion_id;

     fusion_world_flush_calls( world, 1 );

     /* Fork within the fusion world. */
     while (ioctl( fd, FUSION_FORK, &fork )) {
          if (errno != EINTR) {
               D_PERROR( "Fusion/Main: Could not fork in world '%d'!\n", world->shared->world_index );
               raise( SIGTRAP );
          }
     }

     D_DEBUG_AT( Fusion_Main, "  -> Fusion ID 0x%08lx\n", fork.fusion_id );

     /* Get new fusion id back. */
     world->fusion_id = fork.fusion_id;

     /* Close old file descriptor. */
     close( world->fusion_fd );

     /* Write back new file descriptor. */
     world->fusion_fd = fd;

     D_DEBUG_AT( Fusion_Main, "  -> restarting dispatcher loop...\n" );

     /* Restart the dispatcher thread. */
     world->dispatch_loop = direct_thread_create( DTT_MESSAGING, fusion_dispatch_loop, world, "Fusion Dispatch" );
     if (!world->dispatch_loop)
          raise( SIGTRAP );
}

static void
fusion_fork_handler_prepare()
{
     int i;

     D_DEBUG_AT( Fusion_Main, "%s()\n", __FUNCTION__ );

     for (i = 0; i < FUSION_MAX_WORLDS; i++) {
          FusionWorld *world = fusion_worlds[i];

          if (!world)
               continue;

          D_MAGIC_ASSERT( world, FusionWorld );

          if (world->fork_callback)
               world->fork_callback( world->fork_action, FFS_PREPARE );
     }
}

static void
fusion_fork_handler_parent()
{
     int i;

     D_DEBUG_AT( Fusion_Main, "%s()\n", __FUNCTION__ );

     for (i = 0; i < FUSION_MAX_WORLDS; i++) {
          FusionWorld *world = fusion_worlds[i];

          if (!world)
               continue;

          D_MAGIC_ASSERT( world, FusionWorld );
          D_MAGIC_ASSERT( world->shared, FusionWorldShared );

          if (world->fork_callback)
               world->fork_callback( world->fork_action, FFS_PARENT );

          if (world->fork_action == FFA_FORK) {
               /* Increase the shared reference counter. */
               if (fusion_master( world ))
                    world->shared->refs++;
          }
     }
}

static void
fusion_fork_handler_child()
{
     int i;

     D_DEBUG_AT( Fusion_Main, "%s()\n", __FUNCTION__ );

     for (i = 0; i < FUSION_MAX_WORLDS; i++) {
          FusionWorld *world = fusion_worlds[i];

          if (!world)
               continue;

          D_MAGIC_ASSERT( world, FusionWorld );

          if (world->fork_callback)
               world->fork_callback( world->fork_action, FFS_CHILD );

          switch (world->fork_action) {
               default:
                    D_BUG( "unknown fork action %u", world->fork_action );

               case FFA_CLOSE:
                    D_DEBUG_AT( Fusion_Main, "  -> closing world %d\n", i );

                    /* Remove world from global list. */
                    fusion_worlds[i] = NULL;

                    /* Unmap shared area. */
                    direct_file_unmap( world->shared, sizeof(FusionWorldShared) );

                    /* Close Fusion Kernel Device. */
                    close( world->fusion_fd );

                    /* Free local world data. */
                    D_MAGIC_CLEAR( world );
                    D_FREE( world );

                    break;

               case FFA_FORK:
                    D_DEBUG_AT( Fusion_Main, "  -> forking in world %d\n", i );

                    fusion_world_fork( world );

                    break;
          }
     }
}

static DirectResult
map_shared_root( void               *shm_base,
                 int                 world_index,
                 bool                master,
                 FusionWorldShared **ret_shared )
{
     DirectResult   ret = DR_OK;
     DirectFile     fd;
     void          *map;
     char           tmpfs[FUSION_SHM_TMPFS_PATH_NAME_LEN];
     char           root_file[FUSION_SHM_TMPFS_PATH_NAME_LEN+32];
     int            flags = O_RDONLY;
     int            perms = DFP_READ;
     unsigned long  size = direct_page_align( sizeof(FusionWorldShared) );
     unsigned long  base = (unsigned long) shm_base + (size + direct_pagesize()) * world_index;

     if (master || !fusion_config->secure_fusion) {
          perms  |= DFP_WRITE;
          flags  = O_RDWR;
     }

     if (master)
          flags |= O_CREAT | O_TRUNC;

     if (fusion_config->tmpfs) {
          direct_snputs( tmpfs, fusion_config->tmpfs, FUSION_SHM_TMPFS_PATH_NAME_LEN );
     }
     else if (!fusion_find_tmpfs( tmpfs, FUSION_SHM_TMPFS_PATH_NAME_LEN )) {
          D_ERROR( "Fusion/Main: Could not find tmpfs mount point, falling back to /dev/shm!\n" );
          snprintf( tmpfs, FUSION_SHM_TMPFS_PATH_NAME_LEN, "/dev/shm" );
     }

     snprintf( root_file, sizeof(root_file), "%s/fusion.%d", tmpfs, world_index );

     /* Open the virtual file. */
     ret = direct_file_open( &fd, root_file, flags, 0660 );
     if (ret) {
          D_DERROR( ret, "Fusion/Main: Could not open virtual file '%s'!\n", root_file );
          return ret;
     }

     if (fusion_config->shmfile_gid != -1) {
          if (direct_file_chown( &fd, -1, fusion_config->shmfile_gid ))
               D_WARN( "changing owner on %s failed", root_file );
     }

     if (master) {
          direct_file_chmod( &fd, fusion_config->secure_fusion ? 0640 : 0660 );
          if (direct_file_truncate( &fd, size )) {
               D_DERROR( ret, "Fusion/Main: Could not truncate shared memory file '%s'!\n", root_file );
               goto out;
          }
     }

     D_DEBUG_AT( Fusion_Main, "  -> mapping shared memory file ("_ZU" bytes)\n", sizeof(FusionWorldShared) );

     /* Map shared area. */
     D_INFO( "Fusion/Main: Shared root (%d) is "_ZU" bytes, 0x%lx at 0x%lx\n",
             world_index, sizeof(FusionWorldShared), size, base );

     ret = direct_file_map( &fd, (void*) base, 0, size, perms, &map );
     if (ret) {
          D_DERROR( ret, "Fusion/Main: Mapping shared area failed!\n" );
          goto out;
     }

     *ret_shared = map;

out:
     direct_file_close( &fd );

     return ret;
}

static void *
fusion_deferred_loop( DirectThread *thread,
                      void         *arg )
{
     FusionWorld *world = arg;

     D_DEBUG_AT( Fusion_Main_Dispatch, "%s() running...\n", __FUNCTION__ );

     D_MAGIC_ASSERT( world, FusionWorld );

     direct_mutex_lock( &world->deferred.lock );

     while (world->refs) {
          DeferredCall *deferred;

          deferred = (DeferredCall*) world->deferred.list;
          if (!deferred) {
               direct_waitqueue_wait( &world->deferred.queue, &world->deferred.lock );
               continue;
          }

          direct_list_remove( &world->deferred.list, &deferred->link );

          direct_mutex_unlock( &world->deferred.lock );

          FusionReadMessage *header = &deferred->header;
          void              *data   = header + 1;

          switch (header->msg_type) {
               case FMT_SEND:
                    D_DEBUG_AT( Fusion_Main_Dispatch, "  -> FMT_SEND!\n" );
                    break;
               case FMT_CALL:
                    D_DEBUG_AT( Fusion_Main_Dispatch, "  -> FMT_CALL...\n" );
                    _fusion_call_process( world, header->msg_id, data,
                                          (header->msg_size != sizeof(FusionCallMessage)) ?
                                          data + sizeof(FusionCallMessage) : NULL );
                    break;
               case FMT_REACTOR:
                    D_DEBUG_AT( Fusion_Main_Dispatch, "  -> FMT_REACTOR...\n" );
                    _fusion_reactor_process_message( world, header->msg_id, header->msg_channel, data );
                    break;
               case FMT_SHMPOOL:
                    D_DEBUG_AT( Fusion_Main_Dispatch, "  -> FMT_SHMPOOL...\n" );
                    _fusion_shmpool_process( world, header->msg_id, data );
                    break;
               case FMT_CALL3:
                    D_DEBUG_AT( Fusion_Main_Dispatch, "  -> FMT_CALL3...\n" );
                    _fusion_call_process3( world, header->msg_id, data,
                                           (header->msg_size != sizeof(FusionCallMessage3)) ?
                                           data + sizeof(FusionCallMessage3) : NULL );
                    break;
               default:
                    D_DEBUG_AT( Fusion_Main_Dispatch, "  -> discarding message of unknown type '%u'\n",
                                header->msg_type );
                    break;
          }

          D_FREE( deferred );

          direct_mutex_lock( &world->deferred.lock );
     }

     direct_mutex_unlock( &world->deferred.lock );

     return NULL;
}

DirectResult
fusion_enter( int               world_index,
              int               abi_version,
              FusionEnterRole   role,
              FusionWorld     **ret_world )
{
     DirectResult       ret    = DR_OK;
     int                fd     = -1;
     FusionWorld       *world  = NULL;
     FusionWorldShared *shared = NULL;
     FusionEnter        enter;
     char               buf1[20];
     char               buf2[20];
     unsigned long      shm_base;

     D_DEBUG_AT( Fusion_Main, "%s( %d, %d, %p )\n", __FUNCTION__, world_index, abi_version, ret_world );

     D_ASSERT( ret_world != NULL );

     if (world_index >= FUSION_MAX_WORLDS) {
          D_ERROR( "Fusion/Main: World index %d exceeds maximum index %d!\n", world_index, FUSION_MAX_WORLDS - 1 );
          return DR_INVARG;
     }

     direct_once( &fusion_init_once, init_once );

     if (fusion_config->force_slave)
          role = FER_SLAVE;

     direct_initialize();

     direct_mutex_lock( &fusion_worlds_lock );

     if (world_index < 0) {
          if (role == FER_SLAVE) {
               D_ERROR( "Fusion/Main: Slave role and a new world (index -1) was requested!\n" );
               direct_mutex_unlock( &fusion_worlds_lock );
               return DR_INVARG;
          }

          for (world_index = 0; world_index < FUSION_MAX_WORLDS; world_index++) {
               world = fusion_worlds[world_index];
               if (world)
                    break;

               snprintf( buf1, sizeof(buf1), "/dev/fusion%d", world_index );
               snprintf( buf2, sizeof(buf2), "/dev/fusion/%d", world_index );

               /* Open Fusion Kernel Device. */
               fd = fusion_try_open( buf1, buf2, O_RDWR | O_EXCL, false );
               if (fd < 0) {
                    if (errno != EBUSY)
                         D_ERROR( "Fusion/Main: Error opening '%s' and/or '%s'!\n", buf1, buf2 );
               }
               else
                    break;
          }
     }
     else {
          world = fusion_worlds[world_index];
          if (!world) {
               int flags = O_RDWR;

               snprintf( buf1, sizeof(buf1), "/dev/fusion%d", world_index );
               snprintf( buf2, sizeof(buf2), "/dev/fusion/%d", world_index );

               if (role == FER_MASTER)
                    flags |= O_EXCL;
               else if (role == FER_SLAVE)
                    flags |= O_APPEND;

               /* Open Fusion Kernel Device. */
               fd = fusion_try_open( buf1, buf2, flags, true );
          }
     }

     /* Enter a world again. */
     if (world) {
          D_MAGIC_ASSERT( world, FusionWorld );
          D_ASSERT( world->refs > 0 );

          /* Check the role again. */
          switch (role) {
               case FER_MASTER:
                    if (world->fusion_id != FUSION_ID_MASTER) {
                         D_ERROR( "Fusion/Main: Master role requested for a world (%d), but we are already slave in!\n",
                                  world_index );
                         ret = DR_UNSUPPORTED;
                         goto error;
                    }
                    break;

               case FER_SLAVE:
                    if (world->fusion_id == FUSION_ID_MASTER) {
                         D_ERROR( "Fusion/Main: Slave role requested for a world (%d), but we are already master in!\n",
                                  world_index );
                         ret = DR_UNSUPPORTED;
                         goto error;
                    }
                    break;

               case FER_ANY:
                    break;
          }

          shared = world->shared;

          D_MAGIC_ASSERT( shared, FusionWorldShared );

          if (shared->world_abi != abi_version) {
               D_ERROR( "Fusion/Main: World ABI (%d) of world '%d' doesn't match own (%d)!\n",
                        shared->world_abi, world_index, abi_version );
               ret = DR_VERSIONMISMATCH;
               goto error;
          }

          world->refs++;

          direct_mutex_unlock( &fusion_worlds_lock );

          D_DEBUG_AT( Fusion_Main, "  -> using existing world %p [%d]\n", world, world_index );

          /* Return the world. */
          *ret_world = world;

          return DR_OK;
     }

     if (fd < 0) {
          D_ERROR( "Fusion/Main: Opening fusion device (world %d) as '%s' failed!\n",
                   world_index, role == FER_ANY ? "any" : (role == FER_MASTER ? "master" : "slave") );
          ret = DR_INIT;
          goto error;
     }

     /* Drop "identity" when running another program. */
     if (fcntl( fd, F_SETFD, FD_CLOEXEC ) < 0)
          D_PERROR( "Fusion/Main: Setting FD_CLOEXEC flag failed!\n" );

     /* Fill enter information. */
     enter.api.major = 9;
     enter.api.minor = 0;
     enter.fusion_id = 0;
     enter.secure    = fusion_config->secure_fusion;

     /* Enter the fusion world. */
     while (ioctl( fd, FUSION_ENTER, &enter )) {
          if (errno != EINTR) {
               D_PERROR( "Fusion/Main: Could not enter world '%d'!\n", world_index );
               ret = DR_INIT;
               goto error;
          }
     }

     /* Check for valid Fusion ID. */
     if (!enter.fusion_id) {
          D_ERROR( "Fusion/Main: Got no ID from FUSION_ENTER!\n" );
          ret = DR_INIT;
          goto error;
     }

     D_DEBUG_AT( Fusion_Main, "  -> Fusion ID 0x%08lx\n", enter.fusion_id );

     /* Check slave role only, master is handled by O_EXCL earlier. */
     if (role == FER_SLAVE && enter.fusion_id == FUSION_ID_MASTER) {
          D_ERROR( "Fusion/Main: Entering world '%d' as a slave failed!\n", world_index );
          ret = DR_UNSUPPORTED;
          goto error;
     }

     if (ioctl( fd, FUSION_SHM_GET_BASE, &shm_base )) {
          ret = errno2result( errno );
          D_PERROR( "Fusion/Main: FUSION_SHM_GET_BASE" );
          goto error;
     }

     /* Map shared area. */
     ret = map_shared_root( (void*) shm_base, world_index, enter.fusion_id == FUSION_ID_MASTER, &shared );
     if (ret)
          goto error;

     D_DEBUG_AT( Fusion_Main, "  -> shared area at %p, size "_ZU"\n", shared, sizeof(FusionWorldShared) );

     /* Initialize shared data. */
     if (enter.fusion_id == FUSION_ID_MASTER) {
          /* Initialize reference counter. */
          shared->refs = 1;

          /* Set ABI version. */
          shared->world_abi = abi_version;

          /* Set the world index. */
          shared->world_index = world_index;

          /* Set start time of world clock. */
          shared->start_time = direct_clock_get_time( DIRECT_CLOCK_SESSION );

          D_MAGIC_SET( shared, FusionWorldShared );
     }
     else {
          D_MAGIC_ASSERT( shared, FusionWorldShared );

          /* Check ABI version. */
          if (shared->world_abi != abi_version) {
               D_ERROR( "Fusion/Main: World ABI (%d) doesn't match own (%d)!\n",
                        shared->world_abi, abi_version );
               ret = DR_VERSIONMISMATCH;
               goto error;
          }
     }

     /* Synchronize to world clock. */
     direct_clock_set_time( DIRECT_CLOCK_SESSION, shared->start_time );

     /* Allocate local data. */
     world = D_CALLOC( 1, sizeof(FusionWorld) );
     if (!world) {
          ret = D_OOM();
          goto error;
     }

     /* Initialize local data. */
     world->refs      = 1;
     world->shared    = shared;
     world->fusion_fd = fd;
     world->fusion_id = enter.fusion_id;

     direct_mutex_init( &world->reactor_nodes_lock );

     D_MAGIC_SET( world, FusionWorld );

     fusion_worlds[world_index] = world;

     /* Initialize shared memory part. */
     ret = fusion_shm_init( world );
     if (ret)
          goto error2;

     D_DEBUG_AT( Fusion_Main, "  -> initializing other parts...\n" );

     direct_mutex_init( &world->refs_lock );

     /* Initialize other parts. */
     if (enter.fusion_id == FUSION_ID_MASTER) {
          fusion_skirmish_init2( &shared->reactor_globals, "Fusion Reactor Globals",
                                 world,fusion_config->secure_fusion );
          fusion_skirmish_init2( &shared->arenas_lock, "Fusion Arenas",
                                 world, fusion_config->secure_fusion );

          if (!fusion_config->secure_fusion) {
               fusion_skirmish_add_permissions( &shared->reactor_globals, 0,
                                                FUSION_SKIRMISH_PERMIT_PREVAIL | FUSION_SKIRMISH_PERMIT_DISMISS );
               fusion_skirmish_add_permissions( &shared->arenas_lock, 0,
                                                FUSION_SKIRMISH_PERMIT_PREVAIL | FUSION_SKIRMISH_PERMIT_DISMISS );
          }

          /* Create the main pool. */
          ret = fusion_shm_pool_create( world, "Fusion Main Pool", 0x1000000,
                                        fusion_config->debugshm, &shared->main_pool );
          if (ret)
               goto error3;

          fusion_call_init( &shared->refs_call, world_refs_call, world, world );
          fusion_call_set_name( &shared->refs_call, "world_refs" );
          fusion_call_add_permissions( &shared->refs_call, 0, FUSION_CALL_PERMIT_EXECUTE );

          direct_map_create( 37, refs_map_compare, refs_map_hash, world, &world->refs_map );
     }
     else
          direct_map_create( 37, refs_map_slave_compare, refs_map_slave_hash, world, &world->refs_map );

     D_DEBUG_AT( Fusion_Main, "  -> starting dispatcher loop...\n" );

     /* Start the dispatcher thread. */
     world->dispatch_loop = direct_thread_create( DTT_MESSAGING, fusion_dispatch_loop, world, "Fusion Dispatch" );
     if (!world->dispatch_loop) {
          ret = DR_FAILURE;
          goto error4;
     }

     direct_waitqueue_init( &world->deferred.queue );
     direct_mutex_init( &world->deferred.lock );

     /* Start the deferred thread. */
     world->deferred.thread = direct_thread_create( DTT_MESSAGING, fusion_deferred_loop, world, "Fusion Deferred" );
     if (!world->deferred.thread) {
          ret = DR_FAILURE;
          goto error4;
     }

     D_DEBUG_AT( Fusion_Main, "  -> done (%p)\n", world );

     direct_mutex_unlock( &fusion_worlds_lock );

     /* Return the fusion world. */
     *ret_world = world;

     return DR_OK;

error4:
     if (world->deferred.thread)
          direct_thread_destroy( world->deferred.thread );

     if (world->dispatch_loop)
          direct_thread_destroy( world->dispatch_loop );

     if (enter.fusion_id == FUSION_ID_MASTER)
          fusion_shm_pool_destroy( world, shared->main_pool );

error3:
     if (enter.fusion_id == FUSION_ID_MASTER) {
          fusion_skirmish_destroy( &shared->arenas_lock );
          fusion_skirmish_destroy( &shared->reactor_globals );
     }

     fusion_shm_deinit( world );

error2:
     fusion_worlds[world_index] = world;

     D_MAGIC_CLEAR( world );

     D_FREE( world );

error:
     if (shared && shared != MAP_FAILED) {
          if (enter.fusion_id == FUSION_ID_MASTER)
               D_MAGIC_CLEAR( shared );

          direct_file_unmap( shared, sizeof(FusionWorldShared) );
     }

     if (fd != -1)
          close( fd );

     direct_mutex_unlock( &fusion_worlds_lock );

     direct_shutdown();

     return ret;
}

DirectResult
fusion_world_activate( FusionWorld *world )
{
     D_DEBUG_AT( Fusion_Main, "  -> unblocking world...\n" );

     while (ioctl( world->fusion_fd, FUSION_UNBLOCK )) {
          if (errno != EINTR) {
               D_PERROR( "Fusion/Main: Could not unblock world!\n" );
               return DR_FUSION;
          }
     }

     return DR_OK;
}

DirectResult
fusion_stop_dispatcher( FusionWorld *world,
                        bool         emergency )
{
     D_DEBUG_AT( Fusion_Main_Dispatch, "%s( %semergency )\n", __FUNCTION__, emergency ? "" : "no " );

     if (!world->dispatch_loop)
          return DR_OK;

     if (!emergency) {
          fusion_sync( world );

          D_DEBUG_AT( Fusion_Main_Dispatch, "  -> locking thread...\n" );

          direct_thread_lock( world->dispatch_loop );
     }

     D_DEBUG_AT( Fusion_Main_Dispatch, "  -> locked\n" );

     world->dispatch_stop = true;

     if (!emergency) {
          D_DEBUG_AT( Fusion_Main_Dispatch, "  -> unlocking thread...\n" );

          direct_thread_unlock( world->dispatch_loop );

          fusion_sync( world );
     }

     fcntl( world->fusion_fd, F_SETFL, O_NONBLOCK );

     D_DEBUG_AT( Fusion_Main_Dispatch, "  -> finished stopping\n" );

     return DR_OK;
}

DirectResult
fusion_exit( FusionWorld *world,
             bool         emergency )
{
     DirectResult ret;

     D_DEBUG_AT( Fusion_Main, "%s( %p, %semergency )\n", __FUNCTION__, world, emergency ? "" : "no " );

     D_MAGIC_ASSERT( world, FusionWorld );
     D_MAGIC_ASSERT( world->shared, FusionWorldShared );
     D_ASSERT( world->refs > 0 );

     direct_mutex_lock( &fusion_worlds_lock );

     if (--world->refs) {
          direct_mutex_unlock( &fusion_worlds_lock );
          return DR_OK;
     }

     D_ASSUME( direct_thread_self() != world->dispatch_loop );

     if (direct_thread_self() != world->dispatch_loop) {
          int               foo;
          FusionSendMessage msg;

          /* Wake up the read loop thread. */
          msg.fusion_id = world->fusion_id;
          msg.msg_id    = 0;
          msg.msg_data  = &foo;
          msg.msg_size  = sizeof(foo);

          fusion_world_flush_calls( world, 1 );

          while (ioctl( world->fusion_fd, FUSION_SEND_MESSAGE, &msg ) < 0) {
               if (errno != EINTR) {
                    D_PERROR( "Fusion/Main: FUSION_SEND_MESSAGE" );
                    direct_thread_cancel( world->dispatch_loop );
                    break;
               }
          }

          /* Wait for its termination. */
          direct_thread_join( world->dispatch_loop );
     }

     D_ASSUME( direct_thread_self() != world->deferred.thread );

     /* Wake up the deferred call thread. */
     direct_waitqueue_signal( &world->deferred.queue );

     /* Wait for its termination. */
     direct_thread_join( world->deferred.thread );

     direct_thread_destroy( world->dispatch_loop );
     direct_thread_destroy( world->deferred.thread );

     direct_mutex_deinit( &world->deferred.lock );
     direct_waitqueue_deinit( &world->deferred.queue );

     direct_mutex_deinit( &world->refs_lock );
     direct_map_destroy( world->refs_map );

     /* Master has to deinitialize shared data. */
     if (fusion_master( world )) {
          fusion_call_destroy( &world->shared->refs_call );

          world->shared->refs--;
          if (world->shared->refs == 0) {
               fusion_skirmish_destroy( &world->shared->reactor_globals );
               fusion_skirmish_destroy( &world->shared->arenas_lock );

               fusion_shm_pool_destroy( world, world->shared->main_pool );

               /* Deinitialize shared memory. */
               fusion_shm_deinit( world );
          }
     }
     else {
          /* Leave shared memory. */
          fusion_shm_deinit( world );
     }

     /* Reset local dispatch nodes. */
     _fusion_reactor_free_all( world );

     /* Remove world from global list. */
     fusion_worlds[world->shared->world_index] = NULL;

     /* Unmap shared area. */
     if (fusion_master( world ) && world->shared->refs == 0) {
          char         tmpfs[FUSION_SHM_TMPFS_PATH_NAME_LEN];
          char         root_file[FUSION_SHM_TMPFS_PATH_NAME_LEN+32];

          if (fusion_config->tmpfs)
               direct_snputs( tmpfs, fusion_config->tmpfs, FUSION_SHM_TMPFS_PATH_NAME_LEN );
          else if (!fusion_find_tmpfs( tmpfs, FUSION_SHM_TMPFS_PATH_NAME_LEN ))
               snprintf( tmpfs, FUSION_SHM_TMPFS_PATH_NAME_LEN, "/dev/shm" );

          snprintf( root_file, sizeof(root_file), "%s/fusion.%d", tmpfs, world->shared->world_index );

          ret = direct_unlink( root_file );
          if (ret)
               D_DERROR( ret, "Fusion/Main: Could not unlink shared memory file '%s'!\n", root_file );

          D_MAGIC_CLEAR( world->shared );
     }

     direct_file_unmap( world->shared, sizeof(FusionWorldShared) );

     /* Close Fusion Kernel Device. */
     close( world->fusion_fd );

     /* Free local world data. */
     D_MAGIC_CLEAR( world );

     D_FREE( world );

     direct_mutex_unlock( &fusion_worlds_lock );

     direct_shutdown();

     return DR_OK;
}

DirectResult
fusion_kill( FusionWorld *world,
             FusionID     fusion_id,
             int          signal,
             int          timeout_ms )
{
     FusionKill param;

     D_MAGIC_ASSERT( world, FusionWorld );

     param.fusion_id  = fusion_id;
     param.signal     = signal;
     param.timeout_ms = timeout_ms;

     fusion_world_flush_calls( world, 1 );

     while (ioctl( world->fusion_fd, FUSION_KILL, &param )) {
          switch (errno) {
               case EINTR:
                    continue;
               case ETIMEDOUT:
                    return DR_TIMEOUT;
               default:
                    break;
          }

          D_PERROR( "Fusion/Main: FUSION_KILL" );

          return DR_FAILURE;
     }

     return DR_OK;
}

const char *
fusion_get_tmpfs( FusionWorld *world )
{
     D_MAGIC_ASSERT( world, FusionWorld );
     D_MAGIC_ASSERT( world->shared, FusionWorldShared );

     return world->shared->shm.tmpfs;
}

static DirectResult
defer_message( FusionWorld       *world,
               FusionReadMessage *header,
               void              *data )
{
     DeferredCall *deferred;

     deferred = D_CALLOC( 1, sizeof(DeferredCall) + header->msg_size );
     if (!deferred)
          return D_OOM();

     deferred->header = *header;

     direct_memcpy( deferred + 1, data, header->msg_size );

     direct_mutex_lock( &world->deferred.lock );

     direct_list_append( &world->deferred.list, &deferred->link );

     direct_mutex_unlock( &world->deferred.lock );

     direct_waitqueue_signal( &world->deferred.queue );

     return DR_OK;
}

DirectResult
fusion_dispatch_cleanup_add( FusionWorld                *world,
                             FusionDispatchCleanupFunc   func,
                             void                       *ctx,
                             FusionDispatchCleanup     **ret_cleanup )
{
     FusionDispatchCleanup *cleanup;

     cleanup = D_CALLOC( 1, sizeof(FusionDispatchCleanup) );
     if (!cleanup)
          return D_OOM();

     cleanup->func = func;
     cleanup->ctx  = ctx;

     direct_list_append( &world->dispatch_cleanups, &cleanup->link );

     *ret_cleanup = cleanup;

     return DR_OK;
}

DirectResult
fusion_dispatch_cleanup_remove( FusionWorld           *world,
                                FusionDispatchCleanup *cleanup )
{
     direct_list_remove( &world->dispatch_cleanups, &cleanup->link );

     D_FREE( cleanup );

     return DR_OK;
}

static void
handle_dispatch_cleanups( FusionWorld *world )
{
     FusionDispatchCleanup *cleanup, *next;

     D_DEBUG_AT( Fusion_Main_Dispatch, "%s( %p )\n", __FUNCTION__, world );

     direct_list_foreach_safe (cleanup, next, world->dispatch_cleanups) {
          if (direct_log_domain_check( &Fusion_Main_Dispatch ))
               D_DEBUG_AT( Fusion_Main_Dispatch, "  -> %s (%p)\n",
                           direct_trace_lookup_symbol_at( cleanup->func ), cleanup->ctx );

          cleanup->func( cleanup->ctx );

          D_FREE( cleanup );
     }

     D_DEBUG_AT( Fusion_Main_Dispatch, "  -> cleanups done\n" );

     world->dispatch_cleanups = NULL;
}

static DirectEnumerationResult
refs_iterate( DirectMap *map,
              void      *object,
              void      *ctx )
{
     FusionRefSlaveEntry *entry = object;

     if (entry->key.fusion_id == *((FusionID*) ctx)) {
          int i;

          for (i = 0; i < entry->refs; i++)
               fusion_ref_down( entry->ref, false );

          D_FREE( entry );

          return DENUM_REMOVE;
     }

     return DENUM_OK;
}

static void *
fusion_dispatch_loop( DirectThread *thread,
                      void         *arg )
{
     ssize_t      len      = 0;
     size_t       buf_size = FUSION_MESSAGE_SIZE * 4;
     char        *buf      = D_MALLOC( buf_size );
     FusionWorld *world    = arg;

     D_DEBUG_AT( Fusion_Main_Dispatch, "%s() running...\n", __FUNCTION__ );

     D_MAGIC_ASSERT( world, FusionWorld );

     direct_thread_lock( thread );

     while (true) {
          char *buf_p = buf;

          if (world->dispatch_stop) {
               D_DEBUG_AT( Fusion_Main_Dispatch, "  -> ignoring (dispatch_stop)\n" );

               goto out;
          }
          else {
               D_DEBUG_AT( Fusion_Main_Dispatch, "%s( world %p ) => read( "_ZU" )...\n", __FUNCTION__,
                           world, buf_size );

               direct_thread_unlock( thread );

               len = read( world->fusion_fd, buf, buf_size );

               direct_thread_lock( thread );

               if (len < 0) {
                    if (errno == EINTR)
                         continue;

                    break;
               }

               D_DEBUG_AT( Fusion_Main_Dispatch, "%s( world %p ) => got "_ZD" (of up to "_ZU")\n", __FUNCTION__,
                           world, len, buf_size );

               while (buf_p < buf + len) {
                    FusionReadMessage *header = (FusionReadMessage*) buf_p;
                    void              *data   = buf_p + sizeof(FusionReadMessage);

                    D_DEBUG_AT( Fusion_Main_Dispatch, "%s( world %p ) => %p [%ld]\n", __FUNCTION__,
                                world, header, (long) buf_p - (long) buf );

                    D_ASSERT( (buf + len - buf_p) >= sizeof(FusionReadMessage) );

                    switch (header->msg_type) {
                         case FMT_SEND:
                              D_DEBUG_AT( Fusion_Main_Dispatch, "  -> FMT_SEND!\n" );
                              break;
                         case FMT_CALL:
                              D_DEBUG_AT( Fusion_Main_Dispatch, "  -> FMT_CALL...\n" );
                              if (((FusionCallMessage*) data)->caller == 0)
                                   handle_dispatch_cleanups( world );

                              /* If the call comes from kernel space it is most likely a destructor call, defer it. */
                              if (fusion_config->defer_destructors && ((FusionCallMessage*) data)->caller == 0) {
                                   defer_message( world, header, data );
                              }
                              else {
                                   _fusion_call_process( world, header->msg_id, data,
                                                         (header->msg_size != sizeof(FusionCallMessage)) ?
                                                         data + sizeof(FusionCallMessage) : NULL );
                              }
                              break;
                         case FMT_REACTOR:
                              D_DEBUG_AT( Fusion_Main_Dispatch, "  -> FMT_REACTOR...\n" );
                              _fusion_reactor_process_message( world, header->msg_id, header->msg_channel, data );
                              break;
                         case FMT_SHMPOOL:
                              D_DEBUG_AT( Fusion_Main_Dispatch, "  -> FMT_SHMPOOL...\n" );
                              _fusion_shmpool_process( world, header->msg_id, data );
                              break;
                         case FMT_CALL3:
                              D_DEBUG_AT( Fusion_Main_Dispatch, "  -> FMT_CALL3...\n" );
                              _fusion_call_process3( world, header->msg_id, data,
                                                     (header->msg_size != sizeof(FusionCallMessage3)) ?
                                                     data + sizeof(FusionCallMessage3) : NULL );
                              break;
                         case FMT_LEAVE:
                              D_DEBUG_AT( Fusion_Main_Dispatch, "  -> FMT_LEAVE...\n" );
                              if (world->fusion_id == FUSION_ID_MASTER) {
                                   direct_mutex_lock( &world->refs_lock );
                                   direct_map_iterate( world->refs_map, refs_iterate, data );
                                   direct_mutex_unlock( &world->refs_lock );
                              }

                              if (world->leave_callback)
                                   world->leave_callback( world, *((FusionID*) data), world->leave_ctx );
                              break;
                         default:
                              D_DEBUG_AT( Fusion_Main_Dispatch, "  -> discarding message of unknown type %u\n",
                                          header->msg_type );
                              break;
                    }

                    D_DEBUG_AT( Fusion_Main_Dispatch, "  -> done\n" );

                    buf_p = data + ((header->msg_size + 3) & ~3);
               }
          }

          handle_dispatch_cleanups( world );

          if (!world->refs) {
               D_DEBUG_AT( Fusion_Main_Dispatch, "  -> good bye!\n" );
               goto out;
          }
     }

     D_PERROR( "Fusion/Main: Reading from fusion device failed!\n" );

out:
     direct_thread_unlock( thread );
     D_FREE( buf );
     return NULL;
}

DirectResult
fusion_dispatch( FusionWorld *world,
                 size_t       buf_size )
{
     ssize_t  len = 0;
     char    *buf;
     char    *buf_p;

     D_DEBUG_AT( Fusion_Main_Dispatch, "%s( world %p, buf_size "_ZU" )\n", __FUNCTION__, world, buf_size );

     D_MAGIC_ASSERT( world, FusionWorld );

     if (buf_size == 0)
          buf_size = FUSION_MESSAGE_SIZE * 4;
     else
          D_ASSUME( buf_size >= FUSION_MESSAGE_SIZE );

     buf = buf_p = D_MALLOC( buf_size );

     D_DEBUG_AT( Fusion_Main_Dispatch, "  -> dispatch => reading up to "_ZU" bytes...\n", buf_size );

     while (true) {
          len = read( world->fusion_fd, buf, buf_size );
          if (len < 0) {
               if (errno == EINTR)
                    continue;

               if (errno != EAGAIN)
                    D_PERROR( "Fusion/Main: Reading from fusion device failed!\n" );

               D_FREE( buf );
               return DR_IO;
          }

          break;
     }

     D_DEBUG_AT( Fusion_Main_Dispatch, "  -> dispatch => got "_ZD" bytes (of up to "_ZU")\n", len, buf_size );

     if (world->dispatch_loop)
          direct_thread_lock( world->dispatch_loop );

     while (buf_p < buf + len) {
          FusionReadMessage *header = (FusionReadMessage*) buf_p;
          void              *data   = buf_p + sizeof(FusionReadMessage);

          D_DEBUG_AT( Fusion_Main_Dispatch, "  -> dispatch => %p [%ld]\n", header, (long) buf_p - (long) buf );

          D_ASSERT( (buf + len - buf_p) >= sizeof(FusionReadMessage) );

          switch (header->msg_type) {
               case FMT_SEND:
                    D_DEBUG_AT( Fusion_Main_Dispatch, "  -> FMT_SEND!\n" );
                    break;
               case FMT_CALL:
                    D_DEBUG_AT( Fusion_Main_Dispatch, "  -> FMT_CALL...\n" );
                    if (((FusionCallMessage*) data)->caller == 0)
                         handle_dispatch_cleanups( world );

                    /* If the call comes from kernel space it is most likely a destructor call, defer it. */
                    if (fusion_config->defer_destructors && ((FusionCallMessage*) data)->caller == 0) {
                         defer_message( world, header, data );
                    }
                    else
                         _fusion_call_process( world, header->msg_id, data,
                                               (header->msg_size != sizeof(FusionCallMessage)) ?
                                               data + sizeof(FusionCallMessage) : NULL );
                    break;
               case FMT_REACTOR:
                    D_DEBUG_AT( Fusion_Main_Dispatch, "  -> FMT_REACTOR...\n" );
                    _fusion_reactor_process_message( world, header->msg_id, header->msg_channel, data );
                    break;
               case FMT_SHMPOOL:
                    D_DEBUG_AT( Fusion_Main_Dispatch, "  -> FMT_SHMPOOL...\n" );
                    _fusion_shmpool_process( world, header->msg_id, data );
                    break;
               case FMT_CALL3:
                    D_DEBUG_AT( Fusion_Main_Dispatch, "  -> FMT_CALL3...\n" );
                    _fusion_call_process3( world, header->msg_id, data,
                                           (header->msg_size != sizeof(FusionCallMessage3)) ?
                                           data + sizeof(FusionCallMessage3) : NULL );
                    break;
               case FMT_LEAVE:
                    D_DEBUG_AT( Fusion_Main_Dispatch, "  -> FMT_LEAVE...\n" );
                    if (world->fusion_id == FUSION_ID_MASTER) {
                         direct_mutex_lock( &world->refs_lock );
                         direct_map_iterate( world->refs_map, refs_iterate, data );
                         direct_mutex_unlock( &world->refs_lock );
                    }

                    if (world->leave_callback)
                         world->leave_callback( world, *((FusionID*) data), world->leave_ctx );
                    break;
               default:
                    D_DEBUG_AT( Fusion_Main_Dispatch, "  -> discarding message of unknown type '%u'\n",
                                header->msg_type );
                    break;
          }

          D_DEBUG_AT( Fusion_Main_Dispatch, "  -> done\n" );

          buf_p = data + ((header->msg_size + 3) & ~3);
     }

     handle_dispatch_cleanups( world );

     if (world->dispatch_loop)
          direct_thread_unlock( world->dispatch_loop );

     D_FREE( buf );

     return DR_OK;
}

DirectResult
fusion_get_fusionee_path( const FusionWorld *world,
                          FusionID           fusion_id,
                          char              *buf,
                          size_t             buf_size,
                          size_t            *ret_size )
{
     FusionGetFusioneeInfo info;
     size_t                len;

     D_ASSERT( world != NULL );
     D_ASSERT( buf != NULL );
     D_ASSERT( ret_size != NULL );

     info.fusion_id = fusion_id;

     while (ioctl( world->fusion_fd, FUSION_GET_FUSIONEE_INFO, &info ) < 0) {
          switch (errno) {
               case EINTR:
                    continue;

               default:
                    break;
          }

          D_PERROR( "Fusion/Main: FUSION_GET_FUSIONEE_INFO" );

          return DR_FUSION;
     }

     len = strlen( info.exe_file ) + 1;

     if (len > buf_size) {
          *ret_size = len;
          return DR_LIMITEXCEEDED;
     }

     direct_memcpy( buf, info.exe_file, len );

     *ret_size = len;

     return DR_OK;
}

DirectResult
fusion_get_fusionee_pid( const FusionWorld *world,
                         FusionID           fusion_id,
                         pid_t             *ret_pid )
{
     FusionGetFusioneeInfo info;

     D_ASSERT( world != NULL );
     D_ASSERT( ret_pid != NULL );

     info.fusion_id = fusion_id;

     while (ioctl( world->fusion_fd, FUSION_GET_FUSIONEE_INFO, &info ) < 0) {
          switch (errno) {
               case EINTR:
                    continue;

               default:
                    break;
          }

          D_PERROR( "Fusion/main: FUSION_GET_FUSIONEE_INFO" );

          return DR_FUSION;
     }

     *ret_pid = info.pid;

     return DR_OK;
}

DirectResult
fusion_world_set_root( FusionWorld *world,
                       void        *root )
{
     D_ASSERT( world != NULL );
     D_ASSERT( world->shared != NULL );

     if (world->fusion_id != FUSION_ID_MASTER)
          return DR_ACCESSDENIED;

     world->shared->world_root = root;

     return DR_OK;
}

void *
fusion_world_get_root( FusionWorld *world )
{
     D_ASSERT( world != NULL );
     D_ASSERT( world->shared != NULL );

     return world->shared->world_root;
}

DirectResult
fusion_sync( const FusionWorld *world )
{
     D_MAGIC_ASSERT( world, FusionWorld );

     D_DEBUG_AT( Fusion_Main, "%s( %p )\n", __FUNCTION__, world );

     D_DEBUG_AT( Fusion_Main, "  -> syncing with fusion device...\n" );

     while (ioctl( world->fusion_fd, FUSION_SYNC )) {
          switch (errno) {
               case EINTR:
                    continue;
               default:
                    break;
          }

          D_PERROR( "Fusion/Main: FUSION_SYNC" );

          return DR_FAILURE;
     }

     D_DEBUG_AT( Fusion_Main, "  -> synced\n" );

     return DR_OK;
}

#else /* FUSION_BUILD_KERNEL */

typedef struct {
     DirectLink  link;

     FusionRef  *ref;

     int         count;
} FusioneeRef;

typedef struct {
     DirectLink  link;

     FusionID    id;
     pid_t       pid;

     DirectLink *refs;
} Fusionee;

static DirectResult
_fusion_add_fusionee( FusionWorld *world,
                      FusionID     fusion_id )
{
     DirectResult  ret;
     Fusionee     *fusionee;

     D_DEBUG_AT( Fusion_Main, "%s( %p, %lu )\n", __FUNCTION__, world, fusion_id );

     D_MAGIC_ASSERT( world, FusionWorld );
     D_MAGIC_ASSERT( world->shared, FusionWorldShared );

     fusionee = SHCALLOC( world->shared->main_pool, 1, sizeof(Fusionee) );
     if (!fusionee)
          return D_OOSHM();

     fusionee->id  = fusion_id;
     fusionee->pid = direct_gettid();

     ret = fusion_skirmish_prevail( &world->shared->fusionees_lock );
     if (ret) {
          SHFREE( world->shared->main_pool, fusionee );
          return ret;
     }

     direct_list_append( &world->shared->fusionees, &fusionee->link );

     fusion_skirmish_dismiss( &world->shared->fusionees_lock );

     /* Set local pointer. */
     world->fusionee = fusionee;

     return DR_OK;
}

void
_fusion_add_local( FusionWorld *world,
                   FusionRef   *ref,
                   int          add )
{
     Fusionee    *fusionee;
     FusioneeRef *fusionee_ref;

     D_DEBUG_AT( Fusion_Main, "%s( %p, %p, %d )\n", __FUNCTION__, world, ref, add );

     D_MAGIC_ASSERT( world, FusionWorld );
     D_MAGIC_ASSERT( world->shared, FusionWorldShared );
     D_ASSERT( world->fusionee != NULL );
     D_ASSERT( ref != NULL );

     fusionee = world->fusionee;

     direct_list_foreach (fusionee_ref, fusionee->refs) {
          if (fusionee_ref->ref == ref)
               break;
     }

     if (fusionee_ref) {
          fusionee_ref->count += add;

          D_DEBUG_AT( Fusion_Main, "  -> refs = %d\n", fusionee_ref->count );

          if (fusionee_ref->count == 0) {
               direct_list_remove( &fusionee->refs, &fusionee_ref->link );

               SHFREE( world->shared->main_pool, fusionee_ref );
          }
     }
     else {
          /* Check whether we are called from _fusion_remove_fusionee(). */
          if (add <= 0)
               return;

          D_DEBUG_AT( Fusion_Main, "  -> new ref\n" );

          fusionee_ref = SHCALLOC( world->shared->main_pool, 1, sizeof(FusioneeRef) );
          if (!fusionee_ref) {
               D_OOSHM();
               return;
          }

          fusionee_ref->ref   = ref;
          fusionee_ref->count = add;

          direct_list_prepend( &fusionee->refs, &fusionee_ref->link );
     }
}

void
_fusion_check_locals( FusionWorld *world,
                      FusionRef   *ref )
{
     DirectResult  ret;
     Fusionee     *fusionee;
     FusioneeRef  *fusionee_ref, *next;
     DirectLink   *list = NULL;

     D_DEBUG_AT( Fusion_Main, "%s( %p, %p )\n", __FUNCTION__, world, ref );

     D_MAGIC_ASSERT( world, FusionWorld );
     D_MAGIC_ASSERT( world->shared, FusionWorldShared );
     D_ASSERT( ref != NULL );

     if (fusion_skirmish_prevail( &world->shared->fusionees_lock ))
          return;

     direct_list_foreach (fusionee, world->shared->fusionees) {
          if (fusionee->id == world->fusion_id)
               continue;

          direct_list_foreach (fusionee_ref, fusionee->refs) {
               if (fusionee_ref->ref == ref) {
                    ret = direct_kill( fusionee->pid, 0 );
                    if (ret == DR_NOSUCHINSTANCE) {
                         direct_list_remove( &fusionee->refs, &fusionee_ref->link );
                         direct_list_append( &list, &fusionee_ref->link );
                    }
                    break;
               }
          }
     }

     fusion_skirmish_dismiss( &world->shared->fusionees_lock );

     direct_list_foreach_safe (fusionee_ref, next, list) {
          _fusion_ref_change( ref, -fusionee_ref->count, false );

          SHFREE( world->shared->main_pool, fusionee_ref );
     }
}

void
_fusion_remove_all_locals( FusionWorld     *world,
                           const FusionRef *ref )
{
     Fusionee    *fusionee;
     FusioneeRef *fusionee_ref, *next;

     D_DEBUG_AT( Fusion_Main, "%s( %p, %p )\n", __FUNCTION__, world, ref );

     D_MAGIC_ASSERT( world, FusionWorld );
     D_MAGIC_ASSERT( world->shared, FusionWorldShared );
     D_ASSERT( ref != NULL );

     if (fusion_skirmish_prevail( &world->shared->fusionees_lock ))
          return;

     direct_list_foreach (fusionee, world->shared->fusionees) {
          direct_list_foreach_safe (fusionee_ref, next, fusionee->refs) {
               if (fusionee_ref->ref == ref) {
                    direct_list_remove( &fusionee->refs, &fusionee_ref->link );

                    SHFREE( world->shared->main_pool, fusionee_ref );
               }
          }
     }

     fusion_skirmish_dismiss( &world->shared->fusionees_lock );
}

static void
_fusion_remove_fusionee( FusionWorld *world,
                         FusionID fusion_id )
{
     Fusionee    *fusionee;
     FusioneeRef *fusionee_ref, *next;

     D_DEBUG_AT( Fusion_Main, "%s( %p, %lu )\n", __FUNCTION__, world, fusion_id );

     D_MAGIC_ASSERT( world, FusionWorld );
     D_MAGIC_ASSERT( world->shared, FusionWorldShared );

     fusion_skirmish_prevail( &world->shared->fusionees_lock );

     if (fusion_id == world->fusion_id) {
          fusionee = world->fusionee;
     }
     else {
          direct_list_foreach (fusionee, world->shared->fusionees) {
               if (fusionee->id == fusion_id)
                    break;
          }
     }

     if (!fusionee) {
          D_DEBUG_AT( Fusion_Main, "  -> fusionee %lu not found!\n", fusion_id );
          fusion_skirmish_dismiss( &world->shared->fusionees_lock );
          return;
     }

     direct_list_remove( &world->shared->fusionees, &fusionee->link );

     fusion_skirmish_dismiss( &world->shared->fusionees_lock );

     direct_list_foreach_safe (fusionee_ref, next, fusionee->refs) {
          direct_list_remove( &fusionee->refs, &fusionee_ref->link );

          _fusion_ref_change( fusionee_ref->ref, -fusionee_ref->count, false );

          SHFREE( world->shared->main_pool, fusionee_ref );
     }

     SHFREE( world->shared->main_pool, fusionee );
}

DirectResult
_fusion_send_message( int                 fd,
                      const void         *msg,
                      size_t              msg_size,
                      struct sockaddr_un *addr )
{
     socklen_t len = sizeof(struct sockaddr_un);

     D_ASSERT( msg != NULL );

     if (!addr) {
          addr = alloca( sizeof(struct sockaddr_un) );
          getsockname( fd, (struct sockaddr*) addr, &len );
     }

     while (sendto( fd, msg, msg_size, 0, (struct sockaddr*) addr, len ) < 0) {
          switch (errno) {
               case EINTR:
                    continue;
               case ECONNREFUSED:
                    return DR_DESTROYED;
               default:
                    break;
          }

          D_PERROR( "Fusion/Main: sendto() failed!\n" );

          return DR_IO;
     }

     return DR_OK;
}

DirectResult
_fusion_recv_message( int                 fd,
                      void               *msg,
                      size_t              msg_size,
                      struct sockaddr_un *addr )
{
     socklen_t len = addr ? sizeof(struct sockaddr_un) : 0;

     D_ASSERT( msg != NULL );

     while (recvfrom( fd, msg, msg_size, 0, (struct sockaddr*) addr, &len ) < 0) {
          switch (errno) {
               case EINTR:
                    continue;
               case ECONNREFUSED:
                    return DR_DESTROYED;
               default:
                    break;
          }

          D_PERROR( "Fusion/Main: recvfrom() failed!\n" );

          return DR_IO;
     }

     return DR_OK;
}

static void
fusion_fork_handler_prepare()
{
     int i;

     D_DEBUG_AT( Fusion_Main, "%s()\n", __FUNCTION__ );

     for (i = 0; i < FUSION_MAX_WORLDS; i++) {
          FusionWorld *world = fusion_worlds[i];

          if (!world)
               continue;

          D_MAGIC_ASSERT( world, FusionWorld );

          if (world->fork_callback)
               world->fork_callback( world->fork_action, FFS_PREPARE );
     }
}

static void
fusion_fork_handler_parent()
{
     int i;

     D_DEBUG_AT( Fusion_Main, "%s()\n", __FUNCTION__ );

     for (i = 0; i < FUSION_MAX_WORLDS; i++) {
          FusionWorld *world = fusion_worlds[i];

          if (!world)
               continue;

          D_MAGIC_ASSERT( world, FusionWorld );
          D_MAGIC_ASSERT( world->shared, FusionWorldShared );

          if (world->fork_callback)
               world->fork_callback( world->fork_action, FFS_PARENT );

          if (world->fork_action == FFA_FORK) {
               /* Increase the shared reference counter. */
               if (fusion_master( world ))
                    world->shared->refs++;

               /* Cancel the dispatcher to prevent conflicts. */
               direct_thread_cancel( world->dispatch_loop );
          }
     }
}

static void
fusion_fork_handler_child()
{
     int i;

     D_DEBUG_AT( Fusion_Main, "%s()\n", __FUNCTION__ );

     for (i = 0; i < FUSION_MAX_WORLDS; i++) {
          FusionWorld *world = fusion_worlds[i];

          if (!world)
               continue;

          D_MAGIC_ASSERT( world, FusionWorld );
          D_MAGIC_ASSERT( world->shared, FusionWorldShared );

          if (world->fork_callback)
               world->fork_callback( world->fork_action, FFS_CHILD );

          switch (world->fork_action) {
               default:
                    D_BUG( "unknown fork action %u", world->fork_action );

               case FFA_CLOSE:
                    D_DEBUG_AT( Fusion_Main, "  -> closing world %d\n", i );

                    /* Remove world from global list. */
                    fusion_worlds[i] = NULL;

                    /* Unmap shared area. */
                    direct_file_unmap( world->shared, sizeof(FusionWorldShared) );

                    /* Close socket. */
                    close( world->fusion_fd );

                    /* Free local world data. */
                    D_MAGIC_CLEAR( world );
                    D_FREE( world );

                    break;

               case FFA_FORK: {
                    Fusionee    *fusionee;
                    FusioneeRef *fusionee_ref;

                    D_DEBUG_AT( Fusion_Main, "  -> forking in world %d\n", i );

                    fusionee = world->fusionee;

                    D_DEBUG_AT( Fusion_Main, "  -> duplicating fusion id %lu\n", world->fusion_id );

                    fusion_skirmish_prevail( &world->shared->fusionees_lock );

                    if (_fusion_add_fusionee( world, world->fusion_id )) {
                         fusion_skirmish_dismiss( &world->shared->fusionees_lock );
                         raise( SIGTRAP );
                    }

                    D_DEBUG_AT( Fusion_Main, "  -> duplicating local refs...\n" );

                    direct_list_foreach (fusionee_ref, fusionee->refs) {
                         FusioneeRef *new_ref;

                         new_ref = SHCALLOC( world->shared->main_pool, 1, sizeof(FusioneeRef) );
                         if (!new_ref) {
                              D_OOSHM();
                              fusion_skirmish_dismiss( &world->shared->fusionees_lock );
                              raise( SIGTRAP );
                         }

                         new_ref->ref   = fusionee_ref->ref;
                         new_ref->count = fusionee_ref->count;
                         /* Avoid locking. */
                         new_ref->ref->multi.builtin.local += new_ref->count;

                         direct_list_append( &((Fusionee*) world->fusionee)->refs, &new_ref->link );
                    }

                    fusion_skirmish_dismiss( &world->shared->fusionees_lock );

                    D_DEBUG_AT( Fusion_Main, "  -> restarting dispatcher loop...\n" );

                    /* Restart the dispatcher thread. */
                    world->dispatch_loop = direct_thread_create( DTT_MESSAGING, fusion_dispatch_loop, world,
                                                                 "Fusion Dispatch" );
                    if (!world->dispatch_loop)
                         raise( SIGTRAP );

               }    break;
          }
     }
}

DirectResult
fusion_enter( int               world_index,
              int               abi_version,
              FusionEnterRole   role,
              FusionWorld     **ret_world )
{
     DirectResult        ret    = DR_OK;
     int                 fd     = -1;
     FusionID            id     = -1;
     FusionWorld        *world  = NULL;
     FusionWorldShared  *shared = MAP_FAILED;
     struct sockaddr_un  addr;
     char                buf[128];
     int                 len, err;
     int                 orig_index = world_index;

     D_DEBUG_AT( Fusion_Main, "%s( %d, %d, %p )\n", __FUNCTION__, world_index, abi_version, ret_world );

     D_ASSERT( ret_world != NULL );

     if (world_index >= FUSION_MAX_WORLDS) {
          D_ERROR( "Fusion/Main: World index %d exceeds maximum index %d!\n", world_index, FUSION_MAX_WORLDS - 1 );
          return DR_INVARG;
     }

     if (fusion_config->force_slave)
          role = FER_SLAVE;

     direct_once( &fusion_init_once, init_once );

     direct_initialize();

     direct_mutex_lock( &fusion_worlds_lock );

retry:
     world_index = orig_index;
     fd      = -1;
     id      = -1;
     world   = NULL;
     shared  = MAP_FAILED;

     fd = socket( PF_LOCAL, SOCK_RAW, 0 );
     if (fd < 0) {
          D_PERROR( "Fusion/Main: Error creating local socket!\n" );
          direct_mutex_unlock( &fusion_worlds_lock );
          return DR_IO;
     }

     /* Set close-on-exec flag. */
     if (fcntl( fd, F_SETFD, FD_CLOEXEC ) < 0)
          D_PERROR( "Fusion/Main: Setting FD_CLOEXEC flag failed!\n" );

     addr.sun_family = AF_UNIX;

     if (world_index < 0) {
          if (role == FER_SLAVE) {
               D_ERROR( "Fusion/Main: Slave role and a new world (index -1) was requested!\n" );
               direct_mutex_unlock( &fusion_worlds_lock );
               close( fd );
               return DR_INVARG;
          }

          for (world_index = 0; world_index < FUSION_MAX_WORLDS; world_index++) {
               if (fusion_worlds[world_index])
                    continue;

               len = snprintf( addr.sun_path, sizeof(addr.sun_path), "/tmp/.fusion-%d/", world_index );

               /* Make socket directory if it doesn't exits. */
               ret = direct_dir_create( addr.sun_path, 0775 );
               if (ret == DR_OK) {
                    if (fusion_config->shmfile_gid != -1)
                         direct_chown( addr.sun_path, -1, fusion_config->shmfile_gid );
               }

               snprintf( addr.sun_path + len, sizeof(addr.sun_path) - len, "%lx", (unsigned long) FUSION_ID_MASTER );

               /* Bind to address. */
               err = bind( fd, (struct sockaddr*) &addr, sizeof(addr) );
               if (err == 0) {
                    direct_chmod( addr.sun_path, 0660 );

                    /* Change group, if requested. */
                    if (fusion_config->shmfile_gid != -1)
                         direct_chown( addr.sun_path, -1, fusion_config->shmfile_gid );
                    id = FUSION_ID_MASTER;
                    break;
               }
          }
     }
     else {
          world = fusion_worlds[world_index];
          if (!world) {
               len = snprintf( addr.sun_path, sizeof(addr.sun_path), "/tmp/.fusion-%d/", world_index );

               /* Make socket directory if it doesn't exits. */
               ret = direct_dir_create( addr.sun_path, 0775 );
               if (ret == DR_OK) {
                    if (fusion_config->shmfile_gid != -1)
                         direct_chown( addr.sun_path, -1, fusion_config->shmfile_gid );
               }

               /* Check wether we are master. */
               snprintf( addr.sun_path + len, sizeof(addr.sun_path) - len, "%lx", (unsigned long) FUSION_ID_MASTER );

               err = bind( fd, (struct sockaddr*) &addr, sizeof(addr) );
               if (err < 0) {
                    if (role == FER_MASTER) {
                         D_ERROR( "Fusion/Main: Could not start session as master -> remove %s!\n", addr.sun_path );
                         ret = DR_INIT;
                         goto error;
                    }

                    /* Auto generate slave id. */
                    for (id = FUSION_ID_MASTER + 1; id < (FusionID) -1; id++) {
                         snprintf( addr.sun_path + len, sizeof(addr.sun_path) - len, "%lx", id );
                         err = bind( fd, (struct sockaddr*) &addr, sizeof(addr) );
                         if (err == 0) {
                              direct_chmod( addr.sun_path, 0660 );

                               /* Change group, if requested. */
                              if (fusion_config->shmfile_gid != -1)
                                   direct_chown( addr.sun_path, -1, fusion_config->shmfile_gid );
                              break;
                         }
                    }
               }
               else if (err == 0 && role != FER_SLAVE) {
                    direct_chmod( addr.sun_path, 0660 );

                    /* Change group, if requested. */
                    if (fusion_config->shmfile_gid != -1)
                         direct_chown( addr.sun_path, -1, fusion_config->shmfile_gid );
                    id = FUSION_ID_MASTER;
               }
          }
     }

     /* Enter a world again. */
     if (world) {
          D_MAGIC_ASSERT( world, FusionWorld );
          D_ASSERT( world->refs > 0 );

          /* Check the role again. */
          switch (role) {
               case FER_MASTER:
                    if (world->fusion_id != FUSION_ID_MASTER) {
                         D_ERROR( "Fusion/Main: Master role requested for a world (%d), but we are already slave in!\n",
                                  world_index );
                         ret = DR_UNSUPPORTED;
                         goto error;
                    }
                    break;

               case FER_SLAVE:
                    if (world->fusion_id == FUSION_ID_MASTER) {
                         D_ERROR( "Fusion/Main: Slave role requested for a world (%d), but we are already master in!\n",
                                  world_index );
                         ret = DR_UNSUPPORTED;
                         goto error;
                    }
                    break;

               case FER_ANY:
                    break;
          }

          shared = world->shared;

          D_MAGIC_ASSERT( shared, FusionWorldShared );

          if (shared->world_abi != abi_version) {
               D_ERROR( "Fusion/Main: World ABI (%d) of world '%d' doesn't match own (%d)!\n",
                        shared->world_abi, world_index, abi_version );
               ret = DR_VERSIONMISMATCH;
               goto error;
          }

          world->refs++;

          direct_mutex_unlock( &fusion_worlds_lock );

          D_DEBUG_AT( Fusion_Main, "  -> using existing world %p [%d]\n", world, world_index );

          close( fd );

          /* Return the world. */
          *ret_world = world;

          return DR_OK;
     }

     if (id == (FusionID) -1) {
          D_ERROR( "Fusion/Main: Opening fusion socket (world %d) as '%s' failed!\n",
                   world_index, role == FER_ANY ? "any" : (role == FER_MASTER ? "master" : "slave") );
          ret = DR_INIT;
          goto error;
     }

     D_DEBUG_AT( Fusion_Main, "  -> Fusion ID 0x%08lx\n", id );

     if (id == FUSION_ID_MASTER) {
          DirectFile shared_fd;

          snprintf( buf, sizeof(buf), "%s/fusion.%d.core", fusion_config->tmpfs ?: "/dev/shm", world_index );

          /* Open shared memory file. */
          ret = direct_file_open( &shared_fd, buf, O_RDWR | O_CREAT | O_TRUNC, 0660 );
          if (ret) {
               D_DERROR( ret, "Fusion/Main: Could not open shared memory file '%s'!\n", buf );
               ret = DR_INIT;
               goto error;
          }

          if (fusion_config->shmfile_gid != -1) {
               if (direct_file_chown( &shared_fd, -1, fusion_config->shmfile_gid ))
                    D_INFO( "Fusion/Main: Changing owner on '%s' failed... continuing on\n", buf );
          }

          direct_file_chmod( &shared_fd, 0660 );
          direct_file_truncate( &shared_fd, sizeof(FusionWorldShared) );

          unsigned long size = direct_page_align( sizeof(FusionWorldShared) );
          unsigned long base = (unsigned long) 0x20000000 + (size + direct_pagesize()) * world_index;

          /* Map shared area. */
          ret = direct_file_map( &shared_fd, (void*) base, 0, size, DFP_READ | DFP_WRITE, (void**) &shared );
          if (ret) {
               D_DERROR( ret, "Fusion/Main: Mapping shared area failed!\n" );
               direct_file_close( &shared_fd );
               ret = DR_INIT;
               goto error;
          }

          direct_file_close( &shared_fd );

          D_DEBUG_AT( Fusion_Main, "  -> shared area at %p, size "_ZU"\n", shared, sizeof(FusionWorldShared) );

          /* Initialize reference counter. */
          shared->refs = 1;

          /* Set ABI version. */
          shared->world_abi = abi_version;

          /* Set the world index. */
          shared->world_index = world_index;

          /* Set pool allocation base/max. */
          shared->pool_base = (void*) 0x20000000 + (size + direct_pagesize())*FUSION_MAX_WORLDS + 0x8000000*world_index;
          shared->pool_max  = shared->pool_base + 0x8000000 - 1;

          /* Set start time of world clock. */
          shared->start_time = direct_clock_get_time( DIRECT_CLOCK_SESSION );

          D_MAGIC_SET( shared, FusionWorldShared );
     }
     else {
          FusionEnter enter;
          DirectFile  shared_fd;

          /* Fill enter information. */
          enter.type      = FMT_ENTER;
          enter.fusion_id = id;

          snprintf( addr.sun_path, sizeof(addr.sun_path), "/tmp/.fusion-%d/%lx",
                    world_index, (unsigned long) FUSION_ID_MASTER );

          /* Send enter message (used to sync with the master). */
          ret = _fusion_send_message( fd, &enter, sizeof(FusionEnter), &addr );
          if (ret == DR_DESTROYED) {
               DirectDir   dir;
               DirectEntry entry;

               D_DEBUG_AT( Fusion_Main, "  -> master seems dead, cleaning up...\n" );

               snprintf( addr.sun_path, sizeof(addr.sun_path), "/tmp/.fusion-%d", world_index );

               ret = direct_dir_open( &dir, addr.sun_path );
               if (ret) {
                    D_DERROR( ret, "Fusion/Main: Error opening directory '%s' for cleanup!\n", addr.sun_path );

                    /* Unbind. */
                    socklen_t len = sizeof(addr);
                    if (getsockname( fd, (struct sockaddr*) &addr, &len ) == 0)
                         direct_unlink( addr.sun_path );

                    close( fd );

                    ret = DR_INIT;
                    goto error;
               }

               while (direct_dir_read( &dir, &entry ) == DR_OK) {
                    if (!strcmp( entry.name, "." ) || !strcmp( entry.name, ".." ))
                         continue;

                    snprintf( addr.sun_path, sizeof(addr.sun_path), "/tmp/.fusion-%d/%s", world_index, entry.name );

                    D_DEBUG_AT( Fusion_Main, "  -> removing '%s'\n", addr.sun_path );

                    ret = direct_unlink( addr.sun_path );
                    if (ret) {
                         D_DERROR( ret, "Fusion/Main: Error deleting '%s' for cleanup!\n", addr.sun_path );
                         direct_dir_close( &dir );

                         /* Unbind. */
                         socklen_t len = sizeof(addr);
                         if (getsockname( fd, (struct sockaddr*) &addr, &len ) == 0)
                              direct_unlink( addr.sun_path );

                         close( fd );

                         ret = DR_ACCESSDENIED;
                         goto error;
                    }
               }

               direct_dir_close( &dir );

               /* Unbind. */
               socklen_t len = sizeof(addr);
               if (getsockname( fd, (struct sockaddr*) &addr, &len ) == 0)
                    direct_unlink( addr.sun_path );

               close( fd );

               D_DEBUG_AT( Fusion_Main, "  -> retrying...\n" );
               goto retry;
          }

          if (ret)
               D_DERROR( ret, "Fusion/Main: Send message failed!\n" );

          if (ret == DR_OK) {
               ret = _fusion_recv_message( fd, &enter, sizeof(FusionEnter), NULL );
               if (ret)
                    D_DERROR( ret, "Fusion/Main: Receive message failed!\n" );
               if (ret == DR_OK && enter.type != FMT_ENTER) {
                    D_ERROR( "Fusion/Main: Expected message ENTER, got '%u'!\n", enter.type );
                    ret = DR_FUSION;
               }
          }

          if (ret) {
               D_ERROR( "Fusion/Main: Could not enter world '%d'!\n", world_index );
               goto error;
          }

          snprintf( buf, sizeof(buf), "%s/fusion.%d.core", fusion_config->tmpfs ?: "/dev/shm", world_index );

          /* Open shared memory file. */
          ret = direct_file_open( &shared_fd, buf, O_RDWR, 0 );
          if (ret) {
               D_DERROR( ret, "Fusion/Main: Could not open shared memory file '%s'!\n", buf );
               ret = DR_INIT;
               goto error;
          }

          unsigned long size = direct_page_align( sizeof(FusionWorldShared) );
          unsigned long base = (unsigned long) 0x20000000 + (size + direct_pagesize()) * world_index;

          /* Map shared area. */
          ret = direct_file_map( &shared_fd, (void*) base, 0, size, DFP_READ | DFP_WRITE, (void**) &shared );
          if (ret) {
               D_DERROR( ret, "Fusion/Main: Mapping shared area failed!\n" );
               direct_file_close( &shared_fd );
               ret = DR_INIT;
               goto error;
          }

          direct_file_close( &shared_fd );

          D_DEBUG_AT( Fusion_Main, "  -> shared area at %p, size "_ZU"\n", shared, sizeof(FusionWorldShared) );

          D_MAGIC_ASSERT( shared, FusionWorldShared );

          /* Check ABI version. */
          if (shared->world_abi != abi_version) {
               D_ERROR( "Fusion/Main: World ABI (%d) doesn't match own (%d)!\n",
                        shared->world_abi, abi_version );
               ret = DR_VERSIONMISMATCH;
               goto error;
          }
     }

     /* Synchronize to world clock. */
     direct_clock_set_time( DIRECT_CLOCK_SESSION, shared->start_time );

     /* Allocate local data. */
     world = D_CALLOC( 1, sizeof(FusionWorld) );
     if (!world) {
          ret = D_OOM();
          goto error;
     }

     /* Initialize local data. */
     world->refs      = 1;
     world->shared    = shared;
     world->fusion_fd = fd;
     world->fusion_id = id;

     D_MAGIC_SET( world, FusionWorld );

     fusion_worlds[world_index] = world;

     /* Initialize shared memory part. */
     ret = fusion_shm_init( world );
     if (ret)
          goto error2;

     D_DEBUG_AT( Fusion_Main, "  -> initializing other parts...\n" );

     direct_mutex_init( &world->refs_lock );

     /* Initialize other parts. */
     if (world->fusion_id == FUSION_ID_MASTER) {
          fusion_skirmish_init( &shared->arenas_lock, "Fusion Arenas", world );
          fusion_skirmish_init( &shared->reactor_globals, "Fusion Reactor Globals", world );
          fusion_skirmish_init( &shared->fusionees_lock, "Fusionees", world );

          /* Create the main pool. */
          ret = fusion_shm_pool_create( world, "Fusion Main Pool", 0x100000,
                                        fusion_config->debugshm, &shared->main_pool );
          if (ret)
               goto error3;

          fusion_hash_create( shared->main_pool, HASH_INT, HASH_PTR, 109, &shared->call_hash );

          fusion_call_init( &shared->refs_call, world_refs_call, world, world );
          fusion_call_set_name( &shared->refs_call, "world_refs" );
          fusion_call_add_permissions( &shared->refs_call, 0, FUSION_CALL_PERMIT_EXECUTE );

          direct_map_create( 37, refs_map_compare, refs_map_hash, world, &world->refs_map );
     }
     else {
          direct_map_create( 37, refs_map_slave_compare, refs_map_slave_hash, world, &world->refs_map );
     }

     /* Add ourselves to the list of fusionees. */
     ret = _fusion_add_fusionee( world, id );
     if (ret)
          goto error4;

     D_DEBUG_AT( Fusion_Main, "  -> starting dispatcher loop...\n" );

     /* Start the dispatcher thread. */
     world->dispatch_loop = direct_thread_create( DTT_MESSAGING, fusion_dispatch_loop, world, "Fusion Dispatch" );
     if (!world->dispatch_loop) {
          ret = DR_FAILURE;
          goto error5;
     }

     D_DEBUG_AT( Fusion_Main, "  -> done (%p)\n", world );

     direct_mutex_unlock( &fusion_worlds_lock );

     /* Return the fusion world. */
     *ret_world = world;

     return DR_OK;

error5:
     if (world->dispatch_loop)
          direct_thread_destroy( world->dispatch_loop );

     _fusion_remove_fusionee( world, id );

error4:
     if (world->fusion_id == FUSION_ID_MASTER)
          fusion_shm_pool_destroy( world, shared->main_pool );

error3:
     if (world->fusion_id == FUSION_ID_MASTER) {
          fusion_skirmish_destroy( &shared->arenas_lock );
          fusion_skirmish_destroy( &shared->reactor_globals );
          fusion_skirmish_destroy( &shared->fusionees_lock );
     }

     fusion_shm_deinit( world );

error2:
     fusion_worlds[world_index] = world;

     D_MAGIC_CLEAR( world );

     D_FREE( world );

error:
     if (shared != MAP_FAILED) {
          if (id == FUSION_ID_MASTER)
               D_MAGIC_CLEAR( shared );

          direct_file_unmap( shared, sizeof(FusionWorldShared) );
     }

     if (fd != -1) {
          /* Unbind. */
          socklen_t len = sizeof(addr);
          if (getsockname( fd, (struct sockaddr*) &addr, &len ) == 0)
               direct_unlink( addr.sun_path );

          close( fd );
     }

     direct_mutex_unlock( &fusion_worlds_lock );

     direct_shutdown();

     return ret;
}

DirectResult
fusion_world_activate( FusionWorld *world )
{
     return DR_OK;
}

DirectResult
fusion_stop_dispatcher( FusionWorld *world,
                        bool         emergency )
{
     if (!world->dispatch_loop)
          return DR_OK;

     if (!emergency) {
          fusion_sync( world );

          direct_thread_lock( world->dispatch_loop );
     }

     world->dispatch_stop = true;

     if (!emergency) {
          direct_thread_unlock( world->dispatch_loop );

          fusion_sync( world );
     }

     return DR_OK;
}

DirectResult
fusion_exit( FusionWorld *world,
             bool         emergency )
{
     DirectResult ret;
     int          world_index;
     bool         clear = false;

     D_DEBUG_AT( Fusion_Main, "%s( %p, %semergency )\n", __FUNCTION__, world, emergency ? "" : "no " );

     D_MAGIC_ASSERT( world, FusionWorld );
     D_MAGIC_ASSERT( world->shared, FusionWorldShared );
     D_ASSERT( world->refs > 0 );

     world_index = world->shared->world_index;

     direct_mutex_lock( &fusion_worlds_lock );

     if (--world->refs) {
          direct_mutex_unlock( &fusion_worlds_lock );
          return DR_OK;
     }

     if (!emergency) {
          FusionMessageType msg = FMT_SEND;

          /* Wakeup dispatcher. */
          if (_fusion_send_message( world->fusion_fd, &msg, sizeof(msg), NULL ))
               direct_thread_cancel( world->dispatch_loop );

          /* Wait for its termination. */
          direct_thread_join( world->dispatch_loop );
     }

     direct_thread_destroy( world->dispatch_loop );

     /* Remove ourselves from list. */
     if (!emergency || fusion_master( world )) {
          _fusion_remove_fusionee( world, world->fusion_id );
     }
     else {
          struct sockaddr_un addr;
          FusionLeave        leave;

          addr.sun_family = AF_UNIX;
          snprintf( addr.sun_path, sizeof(addr.sun_path), "/tmp/.fusion-%d/%lx",
                    world_index, (unsigned long) FUSION_ID_MASTER );

          leave.type      = FMT_LEAVE;
          leave.fusion_id = world->fusion_id;

          _fusion_send_message( world->fusion_fd, &leave, sizeof(FusionLeave), &addr );
     }

     direct_mutex_deinit( &world->refs_lock );
     direct_map_destroy( world->refs_map );

     /* Master has to deinitialize shared data. */
     if (fusion_master( world )) {
          fusion_call_destroy( &world->shared->refs_call );

          fusion_hash_destroy( world->shared->call_hash );

          world->shared->refs--;
          if (world->shared->refs == 0) {
               fusion_skirmish_destroy( &world->shared->reactor_globals );
               fusion_skirmish_destroy( &world->shared->arenas_lock );
               fusion_skirmish_destroy( &world->shared->fusionees_lock );

               fusion_shm_pool_destroy( world, world->shared->main_pool );

               /* Deinitialize shared memory. */
               fusion_shm_deinit( world );

               clear = true;
          }
     }
     else {
          /* Leave shared memory. */
          fusion_shm_deinit( world );
     }

     /* Reset local dispatch nodes. */
     _fusion_reactor_free_all( world );

     /* Remove world from global list. */
     fusion_worlds[world->shared->world_index] = NULL;

     /* Unmap shared area. */
     if (clear)
          D_MAGIC_CLEAR( world->shared );

     direct_file_unmap( world->shared, sizeof(FusionWorldShared) );

     /* Close socket. */
     close( world->fusion_fd );

     if (clear) {
          DirectDir dir;
          char      buf[128];
          int       len;

          /* Remove core shmfile. */
          snprintf( buf, sizeof(buf), "%s/fusion.%d.core", fusion_config->tmpfs ?: "/dev/shm", world_index );
          D_DEBUG_AT( Fusion_Main, "  -> removing shmfile %s\n", buf );
          direct_unlink( buf );

          /* Cleanup socket directory. */
          len = snprintf( buf, sizeof(buf), "/tmp/.fusion-%d/", world_index );
          ret = direct_dir_open( &dir, buf );
          if (ret == DR_OK) {
               DirectEntry entry;

               while (direct_dir_read( &dir, &entry ) == DR_OK) {
                    if (entry.name[0] != '.') {
                         struct stat st;

                         direct_snputs( buf + len, entry.name, sizeof(buf) - len );
                         if (stat( buf, &st ) == 0 && S_ISSOCK(st.st_mode)) {
                              D_DEBUG_AT( Fusion_Main, "  -> removing socket %s\n", buf );
                              direct_unlink( buf );
                         }
                    }
               }

               direct_dir_close( &dir );
          }
          else {
               D_DERROR( ret, "Fusion/Main: Could not open socket directory %s", buf );
          }
     }

     /* Free local world data. */
     D_MAGIC_CLEAR( world );
     D_FREE( world );

     D_DEBUG_AT( Fusion_Main, "%s( %p ) done\n", __FUNCTION__, world );

     direct_mutex_unlock( &fusion_worlds_lock );

     direct_shutdown();

     return DR_OK;
}

DirectResult
fusion_kill( FusionWorld *world,
             FusionID     fusion_id,
             int          signal,
             int          timeout_ms )
{
     DirectResult  ret;
     Fusionee     *fusionee, *next;

     D_DEBUG_AT( Fusion_Main, "%s( %p, %lu, %d, %d )\n", __FUNCTION__, world, fusion_id, signal, timeout_ms );

     D_MAGIC_ASSERT( world, FusionWorld );
     D_MAGIC_ASSERT( world->shared, FusionWorldShared );

     fusion_skirmish_prevail( &world->shared->fusionees_lock );

     direct_list_foreach_safe (fusionee, next, world->shared->fusionees) {
          if (fusion_id == 0 && fusionee->id == world->fusion_id)
               continue;

          if (fusion_id != 0 && fusionee->id != fusion_id)
               continue;

          D_DEBUG_AT( Fusion_Main, "  -> killing fusionee %lu (%d)...\n", fusionee->id, fusionee->pid );

          ret = direct_kill( fusionee->pid, signal );
          if (ret == DR_OK && timeout_ms >= 0) {
               pid_t     pid  = fusionee->pid;
               long long stop = timeout_ms ? (direct_clock_get_micros() + timeout_ms*1000) : 0;

               fusion_skirmish_dismiss( &world->shared->fusionees_lock );

               while (direct_kill( pid, 0 ) == 0) {
                    usleep( 1000 );

                    if (timeout_ms && direct_clock_get_micros() >= stop)
                         break;
               };

               fusion_skirmish_prevail( &world->shared->fusionees_lock );
          }
          else {
               if (ret == DR_NOSUCHINSTANCE) {
                    D_DEBUG_AT( Fusion_Main, " ... fusionee %lu exited without removing itself\n", fusionee->id );

                    _fusion_remove_fusionee( world, fusionee->id );
               }
               else {
                    D_DERROR( ret, "Fusion/Main: direct_kill( %d, %d ) failed!\n", fusionee->pid, signal );
               }
          }
     }

     fusion_skirmish_dismiss( &world->shared->fusionees_lock );

     return DR_OK;
}

const char *
fusion_get_tmpfs( FusionWorld *world )
{
     D_MAGIC_ASSERT( world, FusionWorld );
     D_MAGIC_ASSERT( world->shared, FusionWorldShared );

     return "/tmp";
}

DirectResult
fusion_dispatch_cleanup_add( FusionWorld                *world,
                             FusionDispatchCleanupFunc   func,
                             void                       *ctx,
                             FusionDispatchCleanup     **ret_cleanup )
{
     FusionDispatchCleanup *cleanup;

     cleanup = D_CALLOC( 1, sizeof(FusionDispatchCleanup) );
     if (!cleanup)
          return D_OOM();

     cleanup->func = func;
     cleanup->ctx  = ctx;

     direct_list_append( &world->dispatch_cleanups, &cleanup->link );

     *ret_cleanup = cleanup;

     return DR_OK;
}

DirectResult
fusion_dispatch_cleanup_remove( FusionWorld           *world,
                                FusionDispatchCleanup *cleanup )
{
     direct_list_remove( &world->dispatch_cleanups, &cleanup->link );

     D_FREE( cleanup );

     return DR_OK;
}

static void
handle_dispatch_cleanups( FusionWorld *world )
{
     FusionDispatchCleanup *cleanup, *next;

     D_DEBUG_AT( Fusion_Main_Dispatch, "%s( %p )\n", __FUNCTION__, world );

     direct_list_foreach_safe (cleanup, next, world->dispatch_cleanups) {
          if (direct_log_domain_check( &Fusion_Main_Dispatch ))
               D_DEBUG_AT( Fusion_Main_Dispatch, "  -> %s (%p)\n",
                           direct_trace_lookup_symbol_at( cleanup->func ), cleanup->ctx );

          cleanup->func( cleanup->ctx );

          D_FREE( cleanup );
     }

     D_DEBUG_AT( Fusion_Main_Dispatch, "  -> cleanups done\n" );

     world->dispatch_cleanups = NULL;
}

static DirectEnumerationResult
refs_iterate( DirectMap *map,
              void      *object,
              void      *ctx )
{
     FusionRefSlaveEntry *entry = object;

     if (entry->key.fusion_id == *((FusionID*) ctx)) {
          int i;

          for (i = 0; i < entry->refs; i++)
               fusion_ref_down( entry->ref, false );

          D_FREE( entry );

          return DENUM_REMOVE;
     }

     return DENUM_OK;
}

static void *
fusion_dispatch_loop( DirectThread *self,
                      void         *arg )
{
     FusionWorld        *world = arg;
     struct sockaddr_un  addr;
     socklen_t           addr_len = sizeof(addr);
     fd_set              set;
     char                buf[FUSION_MESSAGE_SIZE];

     D_DEBUG_AT( Fusion_Main_Dispatch, "%s() running...\n", __FUNCTION__ );

     D_MAGIC_ASSERT( world, FusionWorld );

     while (true) {
          int     err;
          ssize_t msg_size;

          FD_ZERO( &set );
          FD_SET( world->fusion_fd, &set );

          err = select( world->fusion_fd + 1, &set, NULL, NULL, NULL );
          if (err < 0) {
               switch (errno) {
                    case EINTR:
                         continue;

                    default:
                         D_PERROR( "Fusion/Main: select() failed!\n" );
                         return NULL;
               }
          }

          if (FD_ISSET( world->fusion_fd, &set ) &&
              (msg_size = recvfrom( world->fusion_fd, buf, sizeof(buf), 0, (struct sockaddr*) &addr, &addr_len )) > 0) {
               FusionMessage *msg = (FusionMessage*) buf;

               direct_thread_setcancelstate( DIRECT_THREAD_CANCEL_DISABLE );

               D_DEBUG_AT( Fusion_Main_Dispatch, "  -> message from '%s'...\n", addr.sun_path );

               direct_thread_lock( self );

               if (world->dispatch_stop) {
                    D_DEBUG_AT( Fusion_Main_Dispatch, "  -> ignoring (dispatch_stop)\n" );
               }
               else {
                    switch (msg->type) {
                         case FMT_SEND:
                              D_DEBUG_AT( Fusion_Main_Dispatch, "  -> FMT_SEND!\n" );
                              break;

                         case FMT_ENTER:
                              D_DEBUG_AT( Fusion_Main_Dispatch, "  -> FMT_ENTER...\n" );

                              if (!fusion_master( world )) {
                                   D_ERROR( "Fusion/Main/Dispatch: Got ENTER request, but we are not master!\n" );
                                   break;
                              }

                              if (msg->enter.fusion_id == world->fusion_id) {
                                   D_ERROR( "Fusion/Main/Dispatch: ENTER request received from ourselves!\n" );
                                   break;
                              }

                              _fusion_send_message( world->fusion_fd, msg, sizeof(FusionEnter), &addr );
                              break;

                         case FMT_LEAVE:
                              D_DEBUG_AT( Fusion_Main_Dispatch, "  -> FMT_LEAVE...\n" );

                              if (!fusion_master( world )) {
                                   D_ERROR( "Fusion/Main/Dispatch: Got LEAVE request, but we are not master!\n" );
                                   break;
                              }

                              if (world->fusion_id == FUSION_ID_MASTER) {
                                   direct_mutex_lock( &world->refs_lock );
                                   direct_map_iterate( world->refs_map, refs_iterate, &msg->leave.fusion_id );
                                   direct_mutex_unlock( &world->refs_lock );
                              }

                              if (msg->leave.fusion_id == world->fusion_id) {
                                   D_ERROR( "Fusion/Main/Dispatch: LEAVE request received from ourselves!\n" );
                                   break;
                              }

                              _fusion_remove_fusionee( world, msg->leave.fusion_id );
                              break;

                         case FMT_CALL:
                              D_DEBUG_AT( Fusion_Main_Dispatch, "  -> FMT_CALL...\n" );

                              if (((FusionCallMessage*) msg)->caller == 0)
                                   handle_dispatch_cleanups( world );

                              _fusion_call_process( world, msg->call.call_id, &msg->call,
                                                    (msg_size != sizeof(FusionCallMessage)) ?
                                                    (((FusionCallMessage*) msg) + 1) : NULL );
                              break;

                         case FMT_REACTOR:
                              D_DEBUG_AT( Fusion_Main_Dispatch, "  -> FMT_REACTOR...\n" );

                              _fusion_reactor_process_message( world, msg->reactor.id, msg->reactor.channel,
                                                               &buf[sizeof(FusionReactorMessage)] );

                              if (msg->reactor.ref) {
                                   fusion_ref_down( msg->reactor.ref, true );
                                   if (fusion_ref_zero_trylock( msg->reactor.ref ) == DR_OK) {
                                        fusion_ref_destroy( msg->reactor.ref );
                                        SHFREE( world->shared->main_pool, msg->reactor.ref );
                                   }
                              }
                              break;

                         default:
                              D_BUG( "unexpected message type %u", msg->type );
                              break;
                    }
               }

               handle_dispatch_cleanups( world );

               direct_thread_unlock( self );

               if (!world->refs) {
                    D_DEBUG_AT( Fusion_Main_Dispatch, "  -> good bye!\n" );
                    return NULL;
               }

               D_DEBUG_AT( Fusion_Main_Dispatch, "  -> done\n" );

               direct_thread_setcancelstate( DIRECT_THREAD_CANCEL_ENABLE );
          }
     }

     return NULL;
}

DirectResult
fusion_dispatch( FusionWorld *world,
                 size_t       buf_size )
{
     return DR_OK;
}

DirectResult
fusion_get_fusionee_path( const FusionWorld *world,
                          FusionID           fusion_id,
                          char              *buf,
                          size_t             buf_size,
                          size_t            *ret_size )
{
     D_ASSERT( world != NULL );
     D_ASSERT( buf != NULL );
     D_ASSERT( ret_size != NULL );

     *buf = '\0';
     *ret_size = 0;

     return DR_UNIMPLEMENTED;
}

DirectResult
fusion_get_fusionee_pid( const FusionWorld *world,
                         FusionID           fusion_id,
                         pid_t             *ret_pid )
{
     D_ASSERT( world != NULL );

     return DR_UNIMPLEMENTED;
}

DirectResult
fusion_world_set_root( FusionWorld *world,
                       void        *root )
{
     D_ASSERT( world != NULL );
     D_ASSERT( world->shared != NULL );

     if (world->fusion_id != FUSION_ID_MASTER)
          return DR_ACCESSDENIED;

     world->shared->world_root = root;

     return DR_OK;
}

void *
fusion_world_get_root( FusionWorld *world )
{
     D_ASSERT( world != NULL );
     D_ASSERT( world->shared != NULL );

     return world->shared->world_root;
}

DirectResult
fusion_sync( const FusionWorld *world )
{
     D_MAGIC_ASSERT( world, FusionWorld );

     D_DEBUG_AT( Fusion_Main, "%s( %p )\n", __FUNCTION__, world );

     D_DEBUG_AT( Fusion_Main, "  -> syncing with fusion device...\n" );

     D_DEBUG_AT( Fusion_Main, "  -> synced\n" );

     return DR_OK;
}

#endif /* FUSION_BUILD_KERNEL */

void
fusion_world_set_fork_action( FusionWorld      *world,
                              FusionForkAction  action )
{
     D_MAGIC_ASSERT( world, FusionWorld );

     world->fork_action = action;
}

FusionForkAction
fusion_world_get_fork_action( FusionWorld *world )
{
     D_MAGIC_ASSERT( world, FusionWorld );

     return world->fork_action;
}

void
fusion_world_set_fork_callback( FusionWorld        *world,
                                FusionForkCallback  callback )
{
     D_MAGIC_ASSERT( world, FusionWorld );

     world->fork_callback = callback;
}

void
fusion_world_set_leave_callback( FusionWorld         *world,
                                 FusionLeaveCallback  callback,
                                 void                *ctx )
{
     D_MAGIC_ASSERT( world, FusionWorld );

     world->leave_callback = callback;
     world->leave_ctx      = ctx;
}

int
fusion_world_index( const FusionWorld *world )
{
     D_MAGIC_ASSERT( world, FusionWorld );
     D_MAGIC_ASSERT( world->shared, FusionWorldShared );

     return world->shared->world_index;
}

FusionID
fusion_id( const FusionWorld *world )
{
     D_MAGIC_ASSERT( world, FusionWorld );

     return world->fusion_id;
}

bool
fusion_is_multi( const FusionWorld *world )
{
     D_MAGIC_ASSERT( world, FusionWorld );

     return true;
}

pid_t
fusion_dispatcher_tid( const FusionWorld *world )
{
     D_MAGIC_ASSERT( world, FusionWorld );

     if (world->dispatch_loop)
          return direct_thread_get_tid( world->dispatch_loop );

     return 0;
}

bool
fusion_master( const FusionWorld *world )
{
     D_MAGIC_ASSERT( world, FusionWorld );

     return world->fusion_id == FUSION_ID_MASTER;
}

bool
fusion_is_shared( FusionWorld *world,
                  const void  *ptr )
{
     int           i;
     DirectResult  ret;
     FusionSHM    *shm;

     D_MAGIC_ASSERT( world, FusionWorld );

     shm = &world->shm;

     D_MAGIC_ASSERT( shm, FusionSHM );
     D_MAGIC_ASSERT( shm->shared, FusionSHMShared );

     if (ptr >= (void*) world->shared && ptr < (void*) world->shared + sizeof(FusionWorldShared))
          return true;

     ret = fusion_skirmish_prevail( &shm->shared->lock );
     if (ret)
          return false;

     for (i = 0; i < FUSION_SHM_MAX_POOLS; i++) {
          if (shm->shared->pools[i].active) {
               FusionSHMPoolShared *pool = &shm->shared->pools[i];
               shmalloc_heap       *heap;

               D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
               D_MAGIC_ASSERT( pool->heap, shmalloc_heap );

               heap = pool->heap;

               if (ptr >= pool->addr_base && ptr < pool->addr_base + heap->size) {
                    fusion_skirmish_dismiss( &shm->shared->lock );
                    return true;
               }
          }
     }

     fusion_skirmish_dismiss( &shm->shared->lock );

     return false;
}

#else /* FUSION_BUILD_MULTI */

static void *
event_dispatcher_loop( DirectThread *thread,
                       void         *arg )
{
     const int    call_size = sizeof(FusionEventDispatcherCall);
     FusionWorld *world     = arg;

     D_DEBUG_AT( Fusion_Main_Dispatch, "%s() running...\n", __FUNCTION__ );

     D_MAGIC_ASSERT( world, FusionWorld );

     while (1) {
          FusionEventDispatcherBuffer *buf;

          direct_mutex_lock( &world->event_dispatcher_mutex );

          while (1) {
               if (!world->event_dispatcher_buffers)
                    direct_waitqueue_wait( &world->event_dispatcher_cond, &world->event_dispatcher_mutex );

               buf = (FusionEventDispatcherBuffer*) world->event_dispatcher_buffers;

               D_MAGIC_ASSERT( buf, FusionEventDispatcherBuffer );

               if (buf->can_free && buf->read_pos == buf->write_pos) {
                    direct_list_remove( &world->event_dispatcher_buffers, &buf->link );
                    direct_list_append( &world->event_dispatcher_buffers_remove, &buf->link );
                    D_DEBUG_AT( Fusion_Main_Dispatch, "Remove buffer %p free %d read %d write %d sync %d pending %d\n",
                                buf, buf->can_free, buf->read_pos, buf->write_pos, buf->sync_calls, buf->pending );
                    continue;
               }

               if (buf->read_pos >= buf->write_pos) {
                    D_DEBUG_AT( Fusion_Main_Dispatch, "Waiting buffer %p free %d read %d write %d sync %d pending %d\n",
                                buf, buf->can_free, buf->read_pos, buf->write_pos, buf->sync_calls, buf->pending );
                    direct_waitqueue_wait( &world->event_dispatcher_cond, &world->event_dispatcher_mutex );
               }

               buf = (FusionEventDispatcherBuffer*) world->event_dispatcher_buffers;

               D_MAGIC_ASSERT( buf, FusionEventDispatcherBuffer );

               if (buf->can_free && buf->read_pos == buf->write_pos) {
                    direct_list_remove( &world->event_dispatcher_buffers, &buf->link );
                    direct_list_append( &world->event_dispatcher_buffers_remove, &buf->link );
                    D_DEBUG_AT( Fusion_Main_Dispatch, "Remove buffer %p free %d read %d write %d sync %d pending %d\n",
                                buf, buf->can_free, buf->read_pos, buf->write_pos, buf->sync_calls, buf->pending );
                    continue;
               }
               break;
          }

          FusionEventDispatcherCall *msg = (FusionEventDispatcherCall*) &buf->buffer[buf->read_pos];

          D_DEBUG_AT( Fusion_Main_Dispatch, "%s() got msg %p <- arg %d, reaction %d\n", __FUNCTION__,
                      msg, msg->call_arg, msg->reaction );
          D_DEBUG_AT( Fusion_Main_Dispatch, "  -> processing buffer %p free %d read %d write %d sync %d pending %d\n",
                      buf, buf->can_free, buf->read_pos, buf->write_pos, buf->sync_calls, buf->pending );

          buf->read_pos += call_size;
          if (msg->flags & FCEF_ONEWAY)
               buf->read_pos += msg->length;

          /* Align on 4-byte boundaries. */
          buf->read_pos = (buf->read_pos + 3) & ~3;

          if (world->dispatch_stop) {
               D_DEBUG_AT( Fusion_Main_Dispatch, "  -> ignoring (dispatch_stop)\n" );
               direct_mutex_unlock( &world->event_dispatcher_mutex );
               return NULL;
          }
          else {
               direct_mutex_unlock( &world->event_dispatcher_mutex );

               if (msg->call_handler3) {
                    if (FCHR_RETAIN == msg->call_handler3( 1, msg->call_arg, msg->ptr, msg->length, msg->call_ctx, 0,
                                                           msg->ret_ptr, msg->ret_size, &msg->ret_length ))
                         D_WARN( "fusion dispatch => FCHR_RETAIN\n" );
               }
               else if (msg->call_handler) {
                    if (FCHR_RETAIN == msg->call_handler( 1, msg->call_arg, msg->ptr, msg->call_ctx, 0, &msg->ret_val ))
                         D_WARN( "fusion dispatch => FCHR_RETAIN\n" );
               }
               else if (msg->reaction == 1) {
                    FusionReactor *reactor = msg->call_ctx;
                    Reaction      *reaction, *next;

                    D_MAGIC_ASSERT( reactor, FusionReactor );

                    direct_mutex_lock( &reactor->reactions_lock );

                    direct_list_foreach_safe (reaction, next, reactor->reactions) {
                         if ((long) reaction->node_link == msg->call_arg) {
                              if (RS_REMOVE == reaction->func( msg->ptr, reaction->ctx ))
                                   direct_list_remove( &reactor->reactions, &reaction->link );
                         }
                    }

                    direct_mutex_unlock( &reactor->reactions_lock );
               }
               else if (msg->reaction == 2) {
                    FusionReactor *reactor = msg->call_ctx;

                    fusion_reactor_free( reactor );
               }
               else {
                    D_DEBUG_AT( Fusion_Main_Dispatch, "  -> good bye!\n" );
                    return NULL;
               }

               if (!(msg->flags & FCEF_ONEWAY)) {
                    direct_mutex_lock( &world->event_dispatcher_call_mutex );

                    msg->processed = 1;

                    direct_waitqueue_broadcast( &world->event_dispatcher_call_cond );

                    direct_mutex_unlock( &world->event_dispatcher_call_mutex );
               }

               direct_mutex_lock( &world->event_dispatcher_mutex );

               buf->pending--;
          }

          direct_waitqueue_signal( &world->event_dispatcher_process_cond );

          if (world->event_dispatcher_buffers_remove) {
               buf = (FusionEventDispatcherBuffer*) world->event_dispatcher_buffers_remove;

               D_MAGIC_ASSERT( buf, FusionEventDispatcherBuffer );

               if (!buf->sync_calls && !buf->pending) {
                    D_DEBUG_AT( Fusion_Main_Dispatch, "Free buffer %p free %d read %d write %d sync %d pending %d\n\n",
                                buf, buf->can_free, buf->read_pos, buf->write_pos, buf->sync_calls, buf->pending );
                    direct_list_remove( &world->event_dispatcher_buffers_remove, &buf->link );
                    D_MAGIC_CLEAR( buf );
                    D_FREE( buf );
               }
          }

          direct_mutex_unlock( &world->event_dispatcher_mutex );

          if (!world->refs) {
               D_DEBUG_AT( Fusion_Main_Dispatch, "  -> good bye!\n" );
               return NULL;
          }
     }

     D_DEBUG_AT( Fusion_Main_Dispatch, "  -> good bye!\n" );

     return NULL;
}

DirectResult
_fusion_event_dispatcher_process( FusionWorld                      *world,
                                  const FusionEventDispatcherCall  *call,
                                  FusionEventDispatcherCall       **ret )
{
     const int call_size = sizeof(FusionEventDispatcherCall);

     D_MAGIC_ASSERT( world, FusionWorld );

     direct_mutex_lock( &world->event_dispatcher_mutex );

     if (world->dispatch_stop) {
          direct_mutex_unlock( &world->event_dispatcher_mutex );
          return DR_DESTROYED;
     }

     while (call->call_handler3 && (call->flags & FCEF_ONEWAY) &&
            direct_list_count_elements_EXPENSIVE( world->event_dispatcher_buffers ) > 4) {
          direct_waitqueue_wait( &world->event_dispatcher_process_cond, &world->event_dispatcher_mutex );
     }

     if (!world->event_dispatcher_buffers) {
          FusionEventDispatcherBuffer *new_buf = D_CALLOC( 1, sizeof(FusionEventDispatcherBuffer) );
          D_MAGIC_SET( new_buf, FusionEventDispatcherBuffer );
          direct_list_append( &world->event_dispatcher_buffers, &new_buf->link );
     }

     FusionEventDispatcherBuffer *buf = direct_list_get_last( world->event_dispatcher_buffers );

     D_MAGIC_ASSERT( buf, FusionEventDispatcherBuffer );

     if (buf->write_pos + call_size + call->length > EVENT_DISPATCHER_BUFFER_LENGTH) {
          buf->can_free = 1;
          FusionEventDispatcherBuffer *new_buf = D_CALLOC( 1, sizeof(FusionEventDispatcherBuffer) );
          D_MAGIC_SET( new_buf, FusionEventDispatcherBuffer );
          direct_list_append( &world->event_dispatcher_buffers, &new_buf->link );
          buf = new_buf;
     }

     *ret = (FusionEventDispatcherCall*) &buf->buffer[buf->write_pos];

     /* Copy data and signal dispatcher. */
     direct_memcpy( *ret, call, call_size );

     buf->write_pos += call_size;

     buf->pending++;

     if (!(call->flags & FCEF_ONEWAY))
          buf->sync_calls++;

     /* Copy extra data to buffer. */
     if (call->flags & FCEF_ONEWAY && call->length) {
          (*ret)->ptr = &buf->buffer[buf->write_pos];
          direct_memcpy( (*ret)->ptr, call->ptr, call->length );
          buf->write_pos += call->length;
     }

     /* Align on 4-byte boundaries. */
     buf->write_pos = (buf->write_pos + 3) & ~3;

     direct_waitqueue_signal( &world->event_dispatcher_cond );

     direct_mutex_unlock( &world->event_dispatcher_mutex );

     if (!(call->flags & FCEF_ONEWAY)) {
          direct_mutex_lock( &world->event_dispatcher_call_mutex );

          while (!(*ret)->processed)
               direct_waitqueue_wait( &world->event_dispatcher_call_cond, &world->event_dispatcher_call_mutex );

          direct_mutex_unlock( &world->event_dispatcher_call_mutex );

          direct_mutex_lock( &world->event_dispatcher_mutex );
          buf->sync_calls--;
          direct_mutex_unlock( &world->event_dispatcher_mutex );
     }

     return DR_OK;
}

DirectResult
_fusion_event_dispatcher_process_reactions( FusionWorld   *world,
                                            FusionReactor *reactor,
                                            int            channel,
                                            void          *msg_data,
                                            int            msg_size )
{
     const int                  call_size = sizeof(FusionEventDispatcherCall);
     FusionEventDispatcherCall  msg;
     FusionEventDispatcherCall *res;

     D_MAGIC_ASSERT( world, FusionWorld );

     msg.processed = 0;
     msg.reaction = 1;
     msg.call_handler = 0;
     msg.call_handler3 = 0;
     msg.call_ctx = reactor;
     msg.flags = FCEF_ONEWAY;
     msg.call_arg = channel;
     msg.ptr = msg_data;
     msg.length = msg_size;
     msg.ret_val = 0;
     msg.ret_ptr = 0;
     msg.ret_size = 0;
     msg.ret_length = 0;

     direct_mutex_lock( &world->event_dispatcher_mutex );

     if (world->dispatch_stop) {
          direct_mutex_unlock( &world->event_dispatcher_mutex );
          return DR_DESTROYED;
     }

     if (!world->event_dispatcher_buffers) {
          FusionEventDispatcherBuffer *new_buf = D_CALLOC( 1, sizeof(FusionEventDispatcherBuffer) );
          D_MAGIC_SET( new_buf, FusionEventDispatcherBuffer );
          direct_list_append( &world->event_dispatcher_buffers, &new_buf->link );
     }

     FusionEventDispatcherBuffer *buf = direct_list_get_last( world->event_dispatcher_buffers );

     D_MAGIC_ASSERT( buf, FusionEventDispatcherBuffer );

     if (buf->write_pos + call_size + msg_size > EVENT_DISPATCHER_BUFFER_LENGTH) {
          buf->can_free = 1;
          FusionEventDispatcherBuffer *new_buf = D_CALLOC( 1, sizeof(FusionEventDispatcherBuffer) );
          D_MAGIC_SET( new_buf, FusionEventDispatcherBuffer );
          direct_list_append( &world->event_dispatcher_buffers, &new_buf->link );
          buf = new_buf;
     }

     res = (FusionEventDispatcherCall*) &buf->buffer[buf->write_pos];

     /* Copy data and signal dispatcher. */
     direct_memcpy( res, &msg, call_size );

     buf->write_pos += call_size;

     buf->pending++;

     /* Copy extra data to buffer. */
     if (msg.length) {
          res->ptr = &buf->buffer[buf->write_pos];
          direct_memcpy( res->ptr, msg.ptr, msg.length );
          buf->write_pos += msg.length;
     }

     /* Align on 4-byte boundaries. */
     buf->write_pos = (buf->write_pos + 3) & ~3;

     direct_waitqueue_signal( &world->event_dispatcher_cond );

     direct_mutex_unlock( &world->event_dispatcher_mutex );

     return DR_OK;
}

DirectResult
_fusion_event_dispatcher_process_reactor_free( FusionWorld   *world,
                                               FusionReactor *reactor )
{
     const int                  call_size = sizeof(FusionEventDispatcherCall);
     FusionEventDispatcherCall  msg;
     FusionEventDispatcherCall *res;

     D_MAGIC_ASSERT( world, FusionWorld );

     if (reactor->free)
          return DR_OK;

     reactor->free = 1;

     msg.processed = 0;
     msg.reaction = 2;
     msg.call_handler = 0;
     msg.call_handler3 = 0;
     msg.call_ctx = reactor;
     msg.flags = FCEF_ONEWAY;
     msg.call_arg = 0;
     msg.ptr = 0;
     msg.length = 0;
     msg.ret_val = 0;
     msg.ret_ptr = 0;
     msg.ret_size = 0;
     msg.ret_length = 0;

     direct_mutex_lock( &world->event_dispatcher_mutex );

     if (world->dispatch_stop) {
          direct_mutex_unlock( &world->event_dispatcher_mutex );
          return DR_DESTROYED;
     }

     if (!world->event_dispatcher_buffers) {
          FusionEventDispatcherBuffer *new_buf = D_CALLOC( 1, sizeof(FusionEventDispatcherBuffer) );
          D_MAGIC_SET( new_buf, FusionEventDispatcherBuffer );
          direct_list_append( &world->event_dispatcher_buffers, &new_buf->link );
     }

     FusionEventDispatcherBuffer *buf = direct_list_get_last( world->event_dispatcher_buffers );

     D_MAGIC_ASSERT( buf, FusionEventDispatcherBuffer );

     if (buf->write_pos + call_size > EVENT_DISPATCHER_BUFFER_LENGTH) {
          buf->can_free = 1;
          FusionEventDispatcherBuffer *new_buf = D_CALLOC( 1, sizeof(FusionEventDispatcherBuffer) );
          D_MAGIC_SET( new_buf, FusionEventDispatcherBuffer );
          direct_list_append( &world->event_dispatcher_buffers, &new_buf->link );
          buf = new_buf;
     }

     res = (FusionEventDispatcherCall*) &buf->buffer[buf->write_pos];

     /* Copy data and signal dispatcher. */
     direct_memcpy( res, &msg, call_size );

     buf->write_pos += call_size;

     buf->pending++;

     /* Align on 4-byte boundaries. */
     buf->write_pos = (buf->write_pos + 3) & ~3;

     direct_waitqueue_signal( &world->event_dispatcher_cond );

     direct_mutex_unlock( &world->event_dispatcher_mutex );

     return DR_INCOMPLETE;
}

DirectResult
fusion_enter( int               world_index,
              int               abi_version,
              FusionEnterRole   role,
              FusionWorld     **ret_world )
{
     DirectResult       ret;
     FusionWorld       *world  = NULL;
     FusionWorldShared *shared = NULL;

     D_ASSERT( ret_world != NULL );

     ret = direct_initialize();
     if (ret)
          return ret;

     world = D_CALLOC( 1, sizeof(FusionWorld) );
     if (!world) {
          ret = D_OOM();
          goto error;
     }

     shared = D_CALLOC( 1, sizeof(FusionWorldShared) );
     if (!shared) {
          ret = D_OOM();
          goto error;
     }

     world->shared = shared;

     world->fusion_id = FUSION_ID_MASTER;

     /* Create the main pool. */
     ret = fusion_shm_pool_create( world, "Fusion Main Pool", 0x100000,
                                   fusion_config->debugshm, &shared->main_pool );
     if (ret)
          goto error;

     D_MAGIC_SET( world, FusionWorld );
     D_MAGIC_SET( world->shared, FusionWorldShared );

     fusion_skirmish_init( &shared->arenas_lock, "Fusion Arenas", world );

     shared->world = world;

     direct_mutex_init( &world->event_dispatcher_mutex );
     direct_waitqueue_init( &world->event_dispatcher_cond );
     direct_waitqueue_init( &world->event_dispatcher_process_cond );
     direct_mutex_init( &world->event_dispatcher_call_mutex );
     direct_waitqueue_init( &world->event_dispatcher_call_cond );
     world->event_dispatcher_thread = direct_thread_create( DTT_MESSAGING, event_dispatcher_loop, world,
                                                            "Fusion Dispatch" );

     world->refs = 1;

     *ret_world = world;

     return DR_OK;

error:
     if (world) {
          if (world->shared)
               D_FREE( world->shared );

          D_FREE( world );
     }

     direct_shutdown();

     return ret;
}

DirectResult
fusion_world_activate( FusionWorld *world )
{
     return DR_OK;
}

DirectResult
fusion_stop_dispatcher( FusionWorld *world,
                        bool         emergency )
{
     return DR_OK;
}

DirectResult
fusion_exit( FusionWorld *world,
             bool         emergency )
{
     D_MAGIC_ASSERT( world, FusionWorld );
     D_MAGIC_ASSERT( world->shared, FusionWorldShared );

     fusion_shm_pool_destroy( world, world->shared->main_pool );

     direct_mutex_lock( &world->event_dispatcher_mutex );
     world->dispatch_stop = 1;
     direct_mutex_unlock( &world->event_dispatcher_mutex );

     direct_waitqueue_signal( &world->event_dispatcher_call_cond );
     direct_waitqueue_signal( &world->event_dispatcher_cond );

     direct_mutex_deinit( &world->event_dispatcher_mutex );
     direct_waitqueue_deinit( &world->event_dispatcher_cond );
     direct_mutex_deinit( &world->event_dispatcher_call_mutex );
     direct_waitqueue_deinit( &world->event_dispatcher_call_cond );

     D_MAGIC_CLEAR( world->shared );

     D_FREE( world->shared );

     D_MAGIC_CLEAR( world );

     D_FREE( world );

     direct_shutdown();

     return DR_OK;
}

DirectResult
fusion_kill( FusionWorld *world,
             FusionID     fusion_id,
             int          signal,
             int          timeout_ms )
{
     D_MAGIC_ASSERT( world, FusionWorld );

     world->dispatch_stop = 1;

     return DR_OK;
}

const char *
fusion_get_tmpfs( FusionWorld *world )
{
     D_MAGIC_ASSERT( world, FusionWorld );
     D_MAGIC_ASSERT( world->shared, FusionWorldShared );

     return "/tmp";
}

DirectResult
fusion_dispatch_cleanup_add( FusionWorld                *world,
                             FusionDispatchCleanupFunc   func,
                             void                       *ctx,
                             FusionDispatchCleanup     **ret_cleanup )
{
     func( ctx );

     return DR_OK;
}

DirectResult
fusion_dispatch_cleanup_remove( FusionWorld           *world,
                                FusionDispatchCleanup *cleanup )
{
     return DR_OK;
}

DirectResult
fusion_dispatch( FusionWorld *world,
                 size_t       buf_size )
{
     return DR_OK;
}

DirectResult
fusion_get_fusionee_path( const FusionWorld *world,
                          FusionID           fusion_id,
                          char              *buf,
                          size_t             buf_size,
                          size_t            *ret_size )
{
     D_ASSERT( world != NULL );
     D_ASSERT( buf != NULL );
     D_ASSERT( ret_size != NULL );

     *buf = '\0';
     *ret_size = 0;

     return DR_UNIMPLEMENTED;
}

DirectResult
fusion_get_fusionee_pid( const FusionWorld *world,
                         FusionID           fusion_id,
                         pid_t             *ret_pid )
{
     D_MAGIC_ASSERT( world, FusionWorld );

     return DR_UNIMPLEMENTED;
}

DirectResult
fusion_world_set_root( FusionWorld *world,
                       void        *root )
{
     D_ASSERT( world != NULL );
     D_ASSERT( world->shared != NULL );

     if (world->fusion_id != FUSION_ID_MASTER)
          return DR_ACCESSDENIED;

     world->shared->world_root = root;

     return DR_OK;
}

void *
fusion_world_get_root( FusionWorld *world )
{
     D_ASSERT( world != NULL );
     D_ASSERT( world->shared != NULL );

     return world->shared->world_root;
}

DirectResult
fusion_sync( const FusionWorld *world )
{
     D_MAGIC_ASSERT( world, FusionWorld );

     return DR_OK;
}

void
fusion_world_set_fork_action( FusionWorld      *world,
                              FusionForkAction  action )
{
     D_MAGIC_ASSERT( world, FusionWorld );
}

FusionForkAction
fusion_world_get_fork_action( FusionWorld *world )
{
     D_MAGIC_ASSERT( world, FusionWorld );

     return world->fork_action;
}

void
fusion_world_set_fork_callback( FusionWorld        *world,
                                FusionForkCallback  callback )
{
     D_MAGIC_ASSERT( world, FusionWorld );
}

void
fusion_world_set_leave_callback( FusionWorld         *world,
                                 FusionLeaveCallback  callback,
                                 void                *ctx )
{
     D_MAGIC_ASSERT( world, FusionWorld );
}

int
fusion_world_index( const FusionWorld *world )
{
     D_MAGIC_ASSERT( world, FusionWorld );

     return 0;
}

FusionID
fusion_id( const FusionWorld *world )
{
     D_MAGIC_ASSERT( world, FusionWorld );

     return world->fusion_id;
}

bool
fusion_is_multi( const FusionWorld *world )
{
     D_MAGIC_ASSERT( world, FusionWorld );

     return false;
}

pid_t
fusion_dispatcher_tid( const FusionWorld *world )
{
     D_MAGIC_ASSERT( world, FusionWorld );

     return direct_thread_get_tid( world->event_dispatcher_thread );
}

bool
fusion_master( const FusionWorld *world )
{
     D_MAGIC_ASSERT( world, FusionWorld );

     return true;
}

bool
fusion_is_shared( FusionWorld *world,
                  const void  *ptr )
{
     D_MAGIC_ASSERT( world, FusionWorld );

     return true;
}

#endif /* FUSION_BUILD_MULTI */

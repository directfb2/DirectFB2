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

#ifndef __FUSION__FUSION_INTERNAL_H__
#define __FUSION__FUSION_INTERNAL_H__

#include <fusion/call.h>
#include <fusion/fusion.h>
#include <fusion/ref.h>
#include <fusion/shm/shm_internal.h>

/**********************************************************************************************************************/

struct __Fusion_FusionWorldShared {
     int                  magic;

     int                  refs;            /* Increased by the master on fork(). */

     int                  world_index;

     int                  world_abi;

     long long            start_time;

     DirectLink          *arenas;
     FusionSkirmish       arenas_lock;

     FusionSkirmish       reactor_globals;

     FusionSHMShared      shm;

     FusionSHMPoolShared *main_pool;

     DirectLink          *fusionees;       /* Connected fusionees. */
     FusionSkirmish       fusionees_lock;  /* Lock for fusionees. */

     unsigned int         call_ids;        /* Generates call ids. */
     unsigned int         lock_ids;        /* Generates locks ids. */
     unsigned int         ref_ids;         /* Generates refs ids. */
     unsigned int         reactor_ids;     /* Generates reactors ids. */
     unsigned int         pool_ids;        /* Generates pools ids. */

     void                *pool_base;       /* SHM pool allocation base. */
     void                *pool_max;        /* SHM pool max address. */

     void                *world_root;

     FusionWorld         *world;

     FusionCall           refs_call;

     FusionHash          *call_hash;
};

struct __Fusion_FusionWorld {
     int                   magic;

     int                   refs;

     FusionWorldShared    *shared;

     int                   fusion_fd;
     FusionID              fusion_id;

     DirectThread         *dispatch_loop;
     bool                  dispatch_stop;

     DirectLink           *reactor_nodes;
     DirectMutex           reactor_nodes_lock;

     FusionSHM             shm;

     FusionForkAction      fork_action;
     FusionForkCallback    fork_callback;

     void                 *fusionee;

     struct {
          DirectThread    *thread;
          DirectWaitQueue  queue;
          DirectMutex      lock;
          DirectLink      *list;
     } deferred;

     FusionLeaveCallback   leave_callback;
     void                 *leave_ctx;

     DirectLink           *dispatch_cleanups;

     DirectMutex           refs_lock;
     DirectMap            *refs_map;

     DirectThread         *event_dispatcher_thread;
     DirectMutex           event_dispatcher_mutex;
     DirectWaitQueue       event_dispatcher_cond;
     DirectWaitQueue       event_dispatcher_process_cond;
     DirectLink           *event_dispatcher_buffers;
     DirectLink           *event_dispatcher_buffers_remove;
     DirectMutex           event_dispatcher_call_mutex;
     DirectWaitQueue       event_dispatcher_call_cond;
};

/**********************************************************************************************************************/

#if FUSION_BUILD_MULTI

#define FUSION_MAX_WORLDS 32

typedef struct {
     FusionID fusion_id;
     int      ref_id;
} FusionRefSlaveKey;

typedef struct {
     FusionRefSlaveKey  key;

     int                refs;

     FusionRef         *ref;
} FusionRefSlaveEntry;

typedef struct {
     int ref_id;

     int refs_catch;
     int refs_local;
} FusionRefSlaveSlaveEntry;

/*
 * from call.c
 */
void         _fusion_call_process                         ( FusionWorld                      *world,
                                                            int                               call_id,
                                                            FusionCallMessage                *call,
                                                            void                             *ptr );

/*
 * from fusion.c
 */
int          _fusion_fd                                   ( const FusionWorldShared          *shared );

FusionID     _fusion_id                                   ( const FusionWorldShared          *shared );

FusionWorld *_fusion_world                                ( const FusionWorldShared          *shared );

/*
 * from reactor.c
 */
void         _fusion_reactor_free_all                     ( FusionWorld                      *world );

void         _fusion_reactor_process_message              ( FusionWorld                      *world,
                                                            int                               reactor_id,
                                                            int                               channel,
                                                            const void                       *msg_data );

#if FUSION_BUILD_KERNEL

static __inline__ void
fusion_entry_add_permissions( const FusionWorld *world,
                              FusionType         type,
                              int                entry_id,
                              FusionID           fusion_id, ... )
{
     FusionEntryPermissions permissions;
     va_list                args;
     int                    arg;

     permissions.type        = type;
     permissions.id          = entry_id;
     permissions.fusion_id   = fusion_id;
     permissions.permissions = 0;

     va_start( args, fusion_id );

     while ((arg = va_arg( args, int )) != 0)
          FUSION_ENTRY_PERMISSIONS_ADD( permissions.permissions, arg );

     va_end( args );

     while (ioctl( world->fusion_fd, FUSION_ENTRY_ADD_PERMISSIONS, &permissions ) < 0) {
          if (errno != EINTR) {
               D_PERROR( "Fusion: FUSION_ENTRY_ADD_PERMISSIONS( type %u, id %d )\n", type, entry_id );
               break;
          }
     }
}

typedef struct {
     DirectLink        link;

     FusionReadMessage header;
} DeferredCall;

#define EXECUTE3_BIN_FLUSH_MILLIS 16

/*
 * from call.c
 */
void         _fusion_call_process3                        ( FusionWorld                      *world,
                                                            int                               call_id,
                                                            FusionCallMessage3               *msg,
                                                            void                             *ptr );

/*
 * from shm/pool.c
 */
void         _fusion_shmpool_process                      ( FusionWorld                      *world,
                                                            int                               pool_id,
                                                            FusionSHMPoolMessage             *msg );

#else /* FUSION_BUILD_KERNEL */

/*
 * from fusion.c
 */
void         _fusion_add_local                            ( FusionWorld                      *world,
                                                            FusionRef                        *ref,
                                                            int                               add );

void         _fusion_check_locals                         ( FusionWorld                      *world,
                                                            FusionRef                        *ref );

void         _fusion_remove_all_locals                    ( FusionWorld                      *world,
                                                            const FusionRef                  *ref );

DirectResult _fusion_send_message                         ( int                               fd,
                                                            const void                       *msg,
                                                            size_t                            msg_size,
                                                            struct sockaddr_un               *addr );

DirectResult _fusion_recv_message                         ( int                               fd,
                                                            void                             *msg,
                                                            size_t                            msg_size,
                                                            struct sockaddr_un               *addr );

/*
 * from ref.c
 */
DirectResult _fusion_ref_change                           ( FusionRef                        *ref,
                                                            int                               add,
                                                            bool                              global );

#endif /* FUSION_BUILD_KERNEL */

#else /* FUSION_BUILD_MULTI */

#define EVENT_DISPATCHER_BUFFER_LENGTH (FUSION_CALL_MAX_LENGTH)

typedef struct {
     DirectLink link;

     int        magic;

     char       buffer[FUSION_CALL_MAX_LENGTH];
     int        read_pos;
     int        write_pos;
     int        can_free;
     int        sync_calls;
     int        pending;
} FusionEventDispatcherBuffer;

typedef struct
{
     int                  reaction;
     FusionCallHandler    call_handler;
     FusionCallHandler3   call_handler3;
     void                *call_ctx;
     FusionCallExecFlags  flags;
     int                  call_arg;
     void                *ptr;
     unsigned int         length;
     int                  ret_val;
     void                *ret_ptr;
     unsigned int         ret_size;
     unsigned int         ret_length;
     int                  processed;
} FusionEventDispatcherCall;

/*
 * from fusion.c
 */
DirectResult _fusion_event_dispatcher_process             ( FusionWorld                      *world,
                                                            const FusionEventDispatcherCall  *call,
                                                            FusionEventDispatcherCall       **ret );

DirectResult _fusion_event_dispatcher_process_reactions   ( FusionWorld                      *world,
                                                            FusionReactor                    *reactor,
                                                            int                               channel,
                                                            void                             *msg_data,
                                                            int                               msg_size );

DirectResult _fusion_event_dispatcher_process_reactor_free( FusionWorld                      *world,
                                                            FusionReactor                    *reactor );

#endif /* FUSION_BUILD_MULTI */

#endif

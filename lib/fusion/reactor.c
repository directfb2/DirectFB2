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

#include <direct/mem.h>
#include <fusion/fusion_internal.h>
#include <fusion/reactor.h>

#if FUSION_BUILD_MULTI

#include <fusion/shmalloc.h>

#if FUSION_BUILD_KERNEL
#include <fusion/conf.h>
#if D_DEBUG_ENABLED
#include <direct/trace.h>
#endif
#else /* FUSION_BUILD_KERNEL */
#include <direct/memcpy.h>
#endif /* FUSION_BUILD_KERNEL */

#endif /* FUSION_BUILD_MULTI */

D_DEBUG_DOMAIN( Fusion_Reactor, "Fusion/Reactor", "Fusion's Reactor" );

/**********************************************************************************************************************/

static void process_globals( FusionReactor *reactor, const void *msg_data, const ReactionFunc *globals );

#if FUSION_BUILD_MULTI

typedef struct {
     DirectLink     link;

     int            magic;

     DirectRWLock   lock;

     int            reactor_id;
     FusionReactor *reactor;

     DirectLink    *links;      /* Reactor listeners attached to node. */
} ReactorNode;

typedef struct {
     DirectLink         link;

     int                magic;

     Reaction          *reaction;
     int                channel;
} NodeLink;

static void
remove_node_link( ReactorNode *node,
                  NodeLink    *link )
{
     D_MAGIC_ASSERT( node, ReactorNode );
     D_MAGIC_ASSERT( link, NodeLink );

     D_ASSUME( link->reaction == NULL );

     direct_list_remove( &node->links, &link->link );

     D_MAGIC_CLEAR( link );

     D_FREE( link );
}

static ReactorNode *lock_node  ( int reactor_id, bool add_it, bool wlock, FusionReactor *reactor, FusionWorld *world );
static void         unlock_node( ReactorNode *node );

#if FUSION_BUILD_KERNEL

FusionReactor *
fusion_reactor_new( int                msg_size,
                    const char        *name,
                    const FusionWorld *world )
{
     FusionEntryInfo  info;
     FusionReactor   *reactor;

     D_ASSERT( name != NULL );
     D_MAGIC_ASSERT( world, FusionWorld );
     D_MAGIC_ASSERT( world->shared, FusionWorldShared );

     D_DEBUG_AT( Fusion_Reactor, "%s( '%s', size %d )\n", __FUNCTION__, name, msg_size );

     /* Allocate shared reactor data. */
     reactor = SHCALLOC( world->shared->main_pool, 1, sizeof(FusionReactor) );
     if (!reactor) {
          D_OOSHM();
          return NULL;
     }

     /* Create a new reactor. */
     while (ioctl( world->fusion_fd, FUSION_REACTOR_NEW, &reactor->id )) {
          if (errno == EINTR)
              continue;

          D_PERROR( "Fusion/Reactor: FUSION_REACTOR_NEW" );
          SHFREE( world->shared->main_pool, reactor );
          return NULL;
     }

     /* Set the static message size. */
     reactor->msg_size = msg_size;

     /* Set default lock for global reactions. */
     reactor->globals_lock = &world->shared->reactor_globals;

     D_DEBUG_AT( Fusion_Reactor, "  -> new reactor %p [%d] with lock %p [%d]\n",
                 reactor, reactor->id, reactor->globals_lock, reactor->globals_lock->multi.id );

     reactor->shared = world->shared;
     reactor->direct = true;

     D_MAGIC_SET( reactor, FusionReactor );

     info.type = FT_REACTOR;
     info.id   = reactor->id;

     direct_snputs( info.name, name, sizeof(info.name) );

     ioctl( world->fusion_fd, FUSION_ENTRY_SET_INFO, &info );

     return reactor;
}

DirectResult
fusion_reactor_destroy( FusionReactor *reactor )
{
     D_MAGIC_ASSERT( reactor, FusionReactor );
     D_MAGIC_ASSERT( reactor->shared, FusionWorldShared );

     D_DEBUG_AT( Fusion_Reactor, "%s( %p [%d] )\n", __FUNCTION__, reactor, reactor->id );

     D_ASSUME( !reactor->destroyed );

     if (reactor->destroyed)
          return DR_DESTROYED;

     while (ioctl( _fusion_fd( reactor->shared ), FUSION_REACTOR_DESTROY, &reactor->id )) {
          switch (errno) {
               case EINTR:
                    continue;

               case EINVAL:
                    D_ERROR( "Fusion/Reactor: Invalid reactor!\n" );
                    return DR_DESTROYED;
          }

          D_PERROR( "Fusion/Reactor: FUSION_REACTOR_DESTROY" );
          return DR_FUSION;
     }

     reactor->destroyed = true;

     return DR_OK;
}

DirectResult
fusion_reactor_free( FusionReactor *reactor )
{
     D_MAGIC_ASSERT( reactor, FusionReactor );
     D_MAGIC_ASSERT( reactor->shared, FusionWorldShared );

     D_DEBUG_AT( Fusion_Reactor, "%s( %p [%d] )\n", __FUNCTION__, reactor, reactor->id );

     D_MAGIC_CLEAR( reactor );

     if (!reactor->destroyed)
          while (ioctl( _fusion_fd( reactor->shared ), FUSION_REACTOR_DESTROY, &reactor->id ) && errno == EINTR);

     /* Free shared reactor data. */
     SHFREE( reactor->shared->main_pool, reactor );

     return DR_OK;
}

DirectResult
fusion_reactor_attach_channel( FusionReactor *reactor,
                               int            channel,
                               ReactionFunc   func,
                               void          *ctx,
                               Reaction      *reaction )
{
     ReactorNode         *node;
     NodeLink            *link;
     FusionReactorAttach  attach;

     D_MAGIC_ASSERT( reactor, FusionReactor );
     D_ASSERT( func != NULL );
     D_ASSERT( reaction != NULL );

     D_DEBUG_AT( Fusion_Reactor, "%s( %p [%d], func %p, ctx %p, reaction %p )\n", __FUNCTION__,
                 reactor, reactor->id, func, ctx, reaction );

     link = D_CALLOC( 1, sizeof(NodeLink) );
     if (!link)
          return D_OOM();

     node = lock_node( reactor->id, true, true, reactor, NULL );
     if (!node) {
          D_FREE( link );
          return DR_FUSION;
     }

     attach.reactor_id = reactor->id;
     attach.channel    = channel;

     while (ioctl( _fusion_fd( reactor->shared ), FUSION_REACTOR_ATTACH, &attach )) {
          switch (errno) {
               case EINTR:
                    continue;

               case EINVAL:
                    D_ERROR( "Fusion/Reactor: Invalid reactor!\n" );
                    unlock_node( node );
                    D_FREE( link );
                    return DR_DESTROYED;
          }

          D_PERROR( "Fusion/Reactor: FUSION_REACTOR_ATTACH" );
          unlock_node( node );
          D_FREE( link );
          return DR_FUSION;
     }

     /* Fill out callback information. */
     reaction->func      = func;
     reaction->ctx       = ctx;
     reaction->node_link = link;

     link->reaction = reaction;
     link->channel  = channel;

     D_MAGIC_SET( link, NodeLink );

     /* Prepend the reaction to the local reaction list. */
     direct_list_prepend( &node->links, &link->link );

     unlock_node( node );

     return DR_OK;
}

DirectResult
fusion_reactor_detach( FusionReactor *reactor,
                       Reaction      *reaction )
{
     ReactorNode *node;
     NodeLink    *link;

     D_MAGIC_ASSERT( reactor, FusionReactor );
     D_ASSERT( reaction != NULL );

     D_DEBUG_AT( Fusion_Reactor, "%s( %p [%d], reaction %p ) <- func %p, ctx %p\n", __FUNCTION__,
                 reactor, reactor->id, reaction, reaction->func, reaction->ctx );

     node = lock_node( reactor->id, false, true, reactor, NULL );
     if (!node) {
          return DR_BUG;
     }

     link = reaction->node_link;

     D_ASSUME( link != NULL );

     if (link) {
          FusionReactorDetach detach;

          D_ASSERT( link->reaction == reaction );

          detach.reactor_id = reactor->id;
          detach.channel    = link->channel;

          reaction->node_link = NULL;

          link->reaction = NULL;

          remove_node_link( node, link );

          while (ioctl( _fusion_fd( reactor->shared ), FUSION_REACTOR_DETACH, &detach )) {
               switch (errno) {
                    case EINTR:
                         continue;

                    case EINVAL:
                         D_ERROR( "Fusion/Reactor: Invalid reactor!\n" );
                         unlock_node( node );
                         return DR_DESTROYED;
               }

               D_PERROR( "Fusion/Reactor: FUSION_REACTOR_DETACH" );
               unlock_node( node );
               return DR_FUSION;
          }
     }

     unlock_node( node );

     return DR_OK;
}

DirectResult
fusion_reactor_dispatch_channel( FusionReactor      *reactor,
                                 int                 channel,
                                 const void         *msg_data,
                                 int                 msg_size,
                                 bool                self,
                                 const ReactionFunc *globals )
{
     FusionWorld           *world;
     FusionReactorDispatch  dispatch;

     D_MAGIC_ASSERT( reactor, FusionReactor );
     D_ASSERT( msg_data != NULL );

     D_DEBUG_AT( Fusion_Reactor, "%s( %p [%d], msg_data %p, self %s, globals %p)\n", __FUNCTION__,
                 reactor, reactor->id, msg_data, self ? "true" : "false", globals );

     world = _fusion_world( reactor->shared );

     fusion_world_flush_calls( world, 1 );

     /* Handle global reactions first. */
     if (channel == 0 && reactor->globals) {
          if (fusion_config->secure_fusion && !fusion_master(world)) {
               D_BUG( "global reactions on channel 0, cannot dispatch from secure slave" );
               return DR_BUG;
          }

          if (globals)
               process_globals( reactor, msg_data, globals );
          else
               D_ERROR( "Fusion/Reactor: There are global reactions but no globals have been passed to dispatch()!\n" );
     }

     /* Handle local reactions. */
     if (self && reactor->direct) {
          _fusion_reactor_process_message( world, reactor->id, channel, msg_data );
          self = false;
     }

     /* Initialize dispatch data. */
     dispatch.reactor_id = reactor->id;
     dispatch.channel    = channel;
     dispatch.self       = self;
     dispatch.msg_size   = msg_size;
     dispatch.msg_data   = msg_data;

     /* Dispatch the message to handle foreign reactions. */
     while (ioctl( _fusion_fd( reactor->shared ), FUSION_REACTOR_DISPATCH, &dispatch )) {
          switch (errno) {
               case EINTR:
                    continue;

               case EINVAL:
                    D_ERROR( "Fusion/Reactor: Invalid reactor!\n" );
                    return DR_DESTROYED;
          }

          D_PERROR( "Fusion/Reactor: FUSION_REACTOR_DISPATCH" );
          return DR_FUSION;
     }

     return DR_OK;
}

DirectResult
fusion_reactor_set_dispatch_callback( FusionReactor *reactor,
                                      FusionCall    *call,
                                      void          *call_ptr )
{
     FusionReactorSetCallback callback;

     D_MAGIC_ASSERT( reactor, FusionReactor );
     D_ASSERT( call != NULL );

     D_DEBUG_AT( Fusion_Reactor, "%s( %p [%d], call %p [%d], ptr %p)\n", __FUNCTION__,
                 reactor, reactor->id, call, call->call_id, call_ptr );

     /* Fill callback info. */
     callback.reactor_id = reactor->id;
     callback.call_id    = call->call_id;
     callback.call_ptr   = call_ptr;

     /* Set the dispatch callback. */
     while (ioctl( _fusion_fd( reactor->shared ), FUSION_REACTOR_SET_DISPATCH_CALLBACK, &callback )) {
          switch (errno) {
               case EINTR:
                    continue;

               case EINVAL:
                    D_ERROR( "Fusion/Reactor: Invalid reactor!\n" );
                    return DR_DESTROYED;
          }

          D_PERROR( "Fusion/Reactor: FUSION_REACTOR_SET_DISPATCH_CALLBACK" );
          return DR_FUSION;
     }

     return DR_OK;
}

DirectResult
fusion_reactor_set_name( FusionReactor *reactor,
                         const char    *name )
{
     FusionEntryInfo info;

     D_MAGIC_ASSERT( reactor, FusionReactor );
     D_ASSERT( name != NULL );

     D_DEBUG_AT( Fusion_Reactor, "%s( %p, '%s' )\n", __FUNCTION__, reactor, name );

     /* Initialize reactor info. */
     info.type = FT_REACTOR;
     info.id   = reactor->id;

     /* Put reactor name into info. */
     direct_snputs( info.name, name, sizeof(info.name) );

     /* Set the reactor info. */
     while (ioctl( _fusion_fd( reactor->shared ), FUSION_ENTRY_SET_INFO, &info )) {
          switch (errno) {
               case EINTR:
                    continue;

               case EINVAL:
                    D_ERROR( "Fusion/Reactor: Invalid reactor!\n" );
                    return DR_IDNOTFOUND;
          }

          D_PERROR( "Fusion/Reactor: FUSION_ENTRY_SET_INFO( reactor %d, '%s' )\n", reactor->id, name );
          return DR_FUSION;
     }

     return DR_OK;
}

DirectResult
fusion_reactor_add_permissions( FusionReactor            *reactor,
                                FusionID                  fusion_id,
                                FusionReactorPermissions  reactor_permissions )
{
     FusionEntryPermissions permissions;

     permissions.type        = FT_REACTOR;
     permissions.id          = reactor->id;
     permissions.fusion_id   = fusion_id;
     permissions.permissions = 0;

     if (reactor_permissions & FUSION_REACTOR_PERMIT_ATTACH_DETACH) {
          FUSION_ENTRY_PERMISSIONS_ADD( permissions.permissions, FUSION_REACTOR_ATTACH );
          FUSION_ENTRY_PERMISSIONS_ADD( permissions.permissions, FUSION_REACTOR_DETACH );
     }

     if (reactor_permissions & FUSION_REACTOR_PERMIT_DISPATCH)
          FUSION_ENTRY_PERMISSIONS_ADD( permissions.permissions, FUSION_REACTOR_DISPATCH );

     while (ioctl( _fusion_fd( reactor->shared ), FUSION_ENTRY_ADD_PERMISSIONS, &permissions ) < 0) {
          if (errno != EINTR) {
               D_PERROR( "Fusion/Reactor: FUSION_ENTRY_ADD_PERMISSIONS( id %d )", reactor->id );
               return DR_FAILURE;
          }
     }

     return DR_OK;
}

void
_fusion_reactor_process_message( FusionWorld *world,
                                 int          reactor_id,
                                 int          channel,
                                 const void  *msg_data )
{
     ReactorNode *node;
     NodeLink    *link;

     D_MAGIC_ASSERT( world, FusionWorld );
     D_ASSERT( msg_data != NULL );

     D_DEBUG_AT( Fusion_Reactor, "  _fusion_reactor_process_message( [%d], msg_data %p )\n", reactor_id, msg_data );

     /* Find the local counter part of the reactor. */
     node = lock_node( reactor_id, false, false, NULL, world );
     if (!node)
          return;

     D_DEBUG_AT( Fusion_Reactor, "    -> node %p, reactor %p\n", node, node->reactor );

     D_ASSUME( node->links != NULL );

     if (!node->links) {
          D_DEBUG_AT( Fusion_Reactor, "    -> no local reactions!\n" );
          unlock_node( node );
          return;
     }

     direct_list_foreach (link, node->links) {
          Reaction *reaction;

          D_MAGIC_ASSERT( link, NodeLink );

          if (link->channel != channel)
               continue;

          reaction = link->reaction;
          if (!reaction)
               continue;

          if (direct_log_domain_check( &Fusion_Reactor ))
               D_DEBUG_AT( Fusion_Reactor, "    -> %s (%p)\n",
                           direct_trace_lookup_symbol_at( reaction->func ), reaction->func );

          if (reaction->func( msg_data, reaction->ctx ) == RS_REMOVE) {
               FusionReactorDetach detach;

               detach.reactor_id = reactor_id;
               detach.channel    = channel;

               D_DEBUG_AT( Fusion_Reactor, "    -> removing %p, func %p, ctx %p\n",
                           reaction, reaction->func, reaction->ctx );

               link->reaction = NULL;

               /* We can't remove the link as we only have read lock, to avoid dead locks. */

               while (ioctl( world->fusion_fd, FUSION_REACTOR_DETACH, &detach )) {
                    switch (errno) {
                         case EINTR:
                              continue;

                         case EINVAL:
                              D_ERROR( "Fusion/Reactor: Invalid reactor!\n" );
                              break;

                         default:
                              D_PERROR( "Fusion/Reactor: FUSION_REACTOR_DETACH" );
                              break;
                    }

                    break;
               }
          }
     }

     unlock_node( node );
}

#else /* FUSION_BUILD_KERNEL */

typedef struct {
     DirectLink   link;

     unsigned int refs;

     FusionID     fusion_id;
     int          channel;
} Listener;

FusionReactor *
fusion_reactor_new( int                msg_size,
                    const char        *name,
                    const FusionWorld *world )
{
     FusionReactor *reactor;

     D_ASSERT( name != NULL );
     D_MAGIC_ASSERT( world, FusionWorld );
     D_MAGIC_ASSERT( world->shared, FusionWorldShared );

     D_DEBUG_AT( Fusion_Reactor, "%s( '%s', size %d )\n", __FUNCTION__, name, msg_size );

     /* Allocate shared reactor data. */
     reactor = SHCALLOC( world->shared->main_pool, 1, sizeof(FusionReactor) );
     if (!reactor) {
          D_OOSHM();
          return NULL;
     }

     /* Generate the reactor id. */
     reactor->id = ++world->shared->reactor_ids;

     /* Set the static message size. */
     reactor->msg_size = msg_size;

     /* Set default lock for global reactions. */
     reactor->globals_lock = &world->shared->reactor_globals;

     fusion_skirmish_init( &reactor->listeners_lock, "Reactor Listeners", world );

     D_DEBUG_AT( Fusion_Reactor, "  -> new reactor %p [%d] with lock %p [%d]\n",
                 reactor, reactor->id, reactor->globals_lock, reactor->globals_lock->multi.id );

     reactor->shared = world->shared;
     reactor->direct = true;

     D_MAGIC_SET( reactor, FusionReactor );

     return reactor;
}

DirectResult
fusion_reactor_destroy( FusionReactor *reactor )
{
     D_MAGIC_ASSERT( reactor, FusionReactor );
     D_MAGIC_ASSERT( reactor->shared, FusionWorldShared );

     D_DEBUG_AT( Fusion_Reactor, "%s( %p [%d] )\n", __FUNCTION__, reactor, reactor->id );

     D_ASSUME( !reactor->destroyed );

     if (reactor->destroyed)
          return DR_DESTROYED;

     fusion_skirmish_destroy( &reactor->listeners_lock );

     reactor->destroyed = true;

     return DR_OK;
}

DirectResult
fusion_reactor_free( FusionReactor *reactor )
{
     Listener *listener, *next;

     D_MAGIC_ASSERT( reactor, FusionReactor );
     D_MAGIC_ASSERT( reactor->shared, FusionWorldShared );

     D_DEBUG_AT( Fusion_Reactor, "%s( %p [%d] )\n", __FUNCTION__, reactor, reactor->id );

     D_MAGIC_CLEAR( reactor );

     direct_list_foreach_safe (listener, next, reactor->listeners) {
          direct_list_remove( &reactor->listeners, &listener->link );
          SHFREE( reactor->shared->main_pool, listener );
     }

     /* Free shared reactor data. */
     SHFREE( reactor->shared->main_pool, reactor );

     return DR_OK;
}

DirectResult
fusion_reactor_attach_channel( FusionReactor *reactor,
                               int            channel,
                               ReactionFunc   func,
                               void          *ctx,
                               Reaction      *reaction )
{
     DirectResult  ret;
     ReactorNode  *node;
     NodeLink     *link;
     FusionID      fusion_id;
     Listener     *listener;

     D_MAGIC_ASSERT( reactor, FusionReactor );
     D_ASSERT( func != NULL );
     D_ASSERT( reaction != NULL );

     D_DEBUG_AT( Fusion_Reactor, "%s( %p [%d], func %p, ctx %p, reaction %p )\n", __FUNCTION__,
                 reactor, reactor->id, func, ctx, reaction );

     if (reactor->destroyed)
          return DR_DESTROYED;

     link = D_CALLOC( 1, sizeof(NodeLink) );
     if (!link)
          return D_OOM();

     node = lock_node( reactor->id, true, true, reactor, NULL );
     if (!node) {
          D_FREE( link );
          return DR_FUSION;
     }

     fusion_id = _fusion_id( reactor->shared );

     fusion_skirmish_prevail( &reactor->listeners_lock );

     direct_list_foreach (listener, reactor->listeners) {
          if (listener->fusion_id == fusion_id && listener->channel == channel) {
               listener->refs++;
               break;
          }
     }

     if (!listener) {
          listener = SHCALLOC( reactor->shared->main_pool, 1, sizeof(Listener) );
          if (!listener) {
               ret = D_OOSHM();
               fusion_skirmish_dismiss( &reactor->listeners_lock );
               unlock_node( node );
               D_FREE( link );
               return ret;
          }

          listener->refs      = 1;
          listener->fusion_id = fusion_id;
          listener->channel   = channel;

          direct_list_append( &reactor->listeners, &listener->link );
     }

     fusion_skirmish_dismiss( &reactor->listeners_lock );

     /* Fill out callback information. */
     reaction->func      = func;
     reaction->ctx       = ctx;
     reaction->node_link = link;

     link->reaction = reaction;
     link->channel  = channel;

     D_MAGIC_SET( link, NodeLink );

     /* Prepend the reaction to the local reaction list. */
     direct_list_prepend( &node->links, &link->link );

     unlock_node( node );

     return DR_OK;
}

DirectResult
fusion_reactor_detach( FusionReactor *reactor,
                       Reaction      *reaction )
{
     ReactorNode *node;
     NodeLink    *link;

     D_MAGIC_ASSERT( reactor, FusionReactor );
     D_ASSERT( reaction != NULL );

     D_DEBUG_AT( Fusion_Reactor, "%s( %p [%d], reaction %p ) <- func %p, ctx %p\n", __FUNCTION__,
                 reactor, reactor->id, reaction, reaction->func, reaction->ctx );

     if (reactor->destroyed)
          return DR_DESTROYED;

     node = lock_node( reactor->id, false, true, reactor, NULL );
     if (!node) {
          return DR_BUG;
     }

     link = reaction->node_link;

     D_ASSUME( link != NULL );

     if (link) {
          Listener *listener;
          FusionID  fusion_id = _fusion_id( reactor->shared );

          D_ASSERT( link->reaction == reaction );

          reaction->node_link = NULL;

          link->reaction = NULL;

          remove_node_link( node, link );

          fusion_skirmish_prevail( &reactor->listeners_lock );

          direct_list_foreach (listener, reactor->listeners) {
               if (listener->fusion_id == fusion_id && listener->channel == link->channel) {
                    if (--listener->refs == 0) {
                         direct_list_remove( &reactor->listeners, &listener->link );
                         SHFREE( reactor->shared->main_pool, listener );
                    }
                    break;
               }
          }

          fusion_skirmish_dismiss( &reactor->listeners_lock );
     }

     unlock_node( node );

     return DR_OK;
}

DirectResult
fusion_reactor_dispatch_channel( FusionReactor      *reactor,
                                 int                 channel,
                                 const void         *msg_data,
                                 int                 msg_size,
                                 bool                self,
                                 const ReactionFunc *globals )
{
     FusionWorld          *world;
     Listener             *listener, *next;
     FusionRef            *ref = NULL;
     FusionReactorMessage *msg;
     struct sockaddr_un    addr;
     int                   len;

     D_MAGIC_ASSERT( reactor, FusionReactor );
     D_ASSERT( msg_data != NULL );

     D_DEBUG_AT( Fusion_Reactor, "%s( %p [%d], msg_data %p, self %s, globals %p)\n", __FUNCTION__,
                 reactor, reactor->id, msg_data, self ? "true" : "false", globals );

     if (reactor->destroyed)
          return DR_DESTROYED;

     if (msg_size > FUSION_MESSAGE_SIZE-sizeof(FusionReactorMessage)) {
          D_ERROR( "Fusion/Reactor: Message too large (%d)!\n", msg_size );
          return DR_UNSUPPORTED;
     }

     world = _fusion_world( reactor->shared );

     if (reactor->call) {
          ref = SHMALLOC( world->shared->main_pool, sizeof(FusionRef) );
          if (!ref)
               return D_OOSHM();

          fusion_ref_init( ref, "Dispatch Ref", world );
          fusion_ref_up( ref, true );
          fusion_ref_watch( ref, reactor->call, 0 );
     }

     /* Handle global reactions first. */
     if (channel == 0 && reactor->globals) {
          if (globals)
               process_globals( reactor, msg_data, globals );
          else
               D_ERROR( "Fusion/Reactor: There are global reactions but no globals have been passed to dispatch()!\n" );
     }

     /* Handle local reactions. */
     if (self && reactor->direct) {
          _fusion_reactor_process_message( _fusion_world(reactor->shared), reactor->id, channel, msg_data );
          self = false;
     }

     msg = alloca( sizeof(FusionReactorMessage) + msg_size );

     msg->type    = FMT_REACTOR;
     msg->id      = reactor->id;
     msg->channel = channel;
     msg->ref     = ref;

     direct_memcpy( (void*) msg + sizeof(FusionReactorMessage), msg_data, msg_size );

     addr.sun_family = AF_UNIX;
     len = snprintf( addr.sun_path, sizeof(addr.sun_path), "/tmp/.fusion-%d/", fusion_world_index( world ) );

     fusion_skirmish_prevail( &reactor->listeners_lock );

     direct_list_foreach_safe (listener, next, reactor->listeners) {
          if (listener->channel == channel) {
               DirectResult ret;

               if (!self && listener->fusion_id == world->fusion_id)
                    continue;

               if (ref)
                    fusion_ref_up( ref, true );

               snprintf( addr.sun_path + len, sizeof(addr.sun_path) - len, "%lx", listener->fusion_id );

               D_DEBUG_AT( Fusion_Reactor, "  -> sending to '%s'\n", addr.sun_path );

               ret = _fusion_send_message( world->fusion_fd, msg, sizeof(FusionReactorMessage) + msg_size, &addr );
               if (ret == DR_FUSION) {
                    D_DEBUG_AT( Fusion_Reactor, "  -> removing dead listener %lu\n", listener->fusion_id );

                    if (ref)
                         fusion_ref_down( ref, true );

                    direct_list_remove( &reactor->listeners, &listener->link );

                    SHFREE( reactor->shared->main_pool, listener );
               }
          }
     }

     fusion_skirmish_dismiss( &reactor->listeners_lock );

     if (ref) {
          fusion_ref_down( ref, true );
          if (fusion_ref_zero_trylock( ref ) == DR_OK) {
               fusion_ref_destroy( ref );
               SHFREE( world->shared->main_pool, ref );
          }
     }

     D_DEBUG_AT( Fusion_Reactor, "%s( %p ) done\n", __FUNCTION__, reactor );

     return DR_OK;
}

DirectResult
fusion_reactor_set_dispatch_callback( FusionReactor *reactor,
                                      FusionCall    *call,
                                      void          *call_ptr )
{
     D_MAGIC_ASSERT( reactor, FusionReactor );
     D_ASSERT( call != NULL );

     D_DEBUG_AT( Fusion_Reactor, "%s( %p [%d], call %p [%d], ptr %p)\n", __FUNCTION__,
                 reactor, reactor->id, call, call->call_id, call_ptr );

     if (reactor->destroyed)
          return DR_DESTROYED;

     if (call_ptr)
          return DR_UNIMPLEMENTED;

     reactor->call = call;

     return DR_OK;
}

DirectResult
fusion_reactor_set_name( FusionReactor *reactor,
                         const char    *name )
{
     return DR_UNIMPLEMENTED;
}

DirectResult
fusion_reactor_add_permissions( FusionReactor            *reactor,
                                FusionID                  fusion_id,
                                FusionReactorPermissions  permissions )
{
     return DR_UNIMPLEMENTED;
}

void
_fusion_reactor_process_message( FusionWorld *world,
                                 int          reactor_id,
                                 int          channel,
                                 const void  *msg_data )
{
     ReactorNode *node;
     NodeLink    *link;

     D_MAGIC_ASSERT( world, FusionWorld );
     D_ASSERT( msg_data != NULL );

     D_DEBUG_AT( Fusion_Reactor, "  _fusion_reactor_process_message( [%d], msg_data %p )\n", reactor_id, msg_data );

     /* Find the local counter part of the reactor. */
     node = lock_node( reactor_id, false, false, NULL, world );
     if (!node)
          return;

     D_DEBUG_AT( Fusion_Reactor, "    -> node %p, reactor %p\n", node, node->reactor );

     D_ASSUME( node->links != NULL );

     if (!node->links) {
          D_DEBUG_AT( Fusion_Reactor, "    -> no local reactions!\n" );
          unlock_node( node );
          return;
     }

     direct_list_foreach (link, node->links) {
          Reaction *reaction;

          D_MAGIC_ASSERT( link, NodeLink );

          if (link->channel != channel)
               continue;

          reaction = link->reaction;
          if (!reaction)
               continue;

          if (reaction->func( msg_data, reaction->ctx ) == RS_REMOVE) {
               FusionReactor *reactor = node->reactor;
               Listener      *listener;

               D_DEBUG_AT( Fusion_Reactor, "    -> removing %p, func %p, ctx %p\n",
                           reaction, reaction->func, reaction->ctx );

               fusion_skirmish_prevail( &reactor->listeners_lock );

               direct_list_foreach (listener, reactor->listeners) {
                    if (listener->fusion_id == world->fusion_id && listener->channel == channel) {
                         if (--listener->refs == 0) {
                              direct_list_remove( &reactor->listeners, &listener->link );
                              SHFREE( world->shared->main_pool, listener );
                         }
                         break;
                    }
               }

               fusion_skirmish_dismiss( &reactor->listeners_lock );
          }
     }

     unlock_node( node );
}

#endif /* FUSION_BUILD_KERNEL */

DirectResult
fusion_reactor_set_lock( FusionReactor  *reactor,
                         FusionSkirmish *lock )
{
     DirectResult    ret;
     FusionSkirmish *old;

     D_MAGIC_ASSERT( reactor, FusionReactor );
     D_ASSERT( lock != NULL );

     old = reactor->globals_lock;

     D_DEBUG_AT( Fusion_Reactor, "%s( %p [%d], lock %p [%d] ) <- old %p [%d]\n", __FUNCTION__,
                 reactor, reactor->id, lock, lock->multi.id, old, old->multi.id );

     /* Acquire the old lock to make sure that changing the lock doesn't result in mismatching lock/unlock pairs. */
     ret = fusion_skirmish_prevail( old );
     if (ret)
          return ret;

     D_ASSUME( reactor->globals_lock != lock );

     /* Set the lock replacement. */
     reactor->globals_lock = lock;

     /* Release the old lock which is obsolete now. */
     fusion_skirmish_dismiss( old );

     return DR_OK;
}

DirectResult
fusion_reactor_set_lock_only( FusionReactor  *reactor,
                              FusionSkirmish *lock )
{
     D_MAGIC_ASSERT( reactor, FusionReactor );
     D_ASSERT( lock != NULL );

     D_DEBUG_AT( Fusion_Reactor, "%s( %p [%d], lock %p [%d] ) <- old %p [%d]\n", __FUNCTION__,
                 reactor, reactor->id, lock, lock->multi.id, reactor->globals_lock, reactor->globals_lock->multi.id );

     D_ASSUME( reactor->globals_lock != lock );

     /* Set the lock replacement. */
     reactor->globals_lock = lock;

     return DR_OK;
}

DirectResult
fusion_reactor_attach( FusionReactor *reactor,
                       ReactionFunc   func,
                       void          *ctx,
                       Reaction      *reaction )
{
     D_MAGIC_ASSERT( reactor, FusionReactor );
     D_ASSERT( func != NULL );
     D_ASSERT( reaction != NULL );

     return fusion_reactor_attach_channel( reactor, 0, func, ctx, reaction );
}

DirectResult
fusion_reactor_attach_global( FusionReactor  *reactor,
                              int             index,
                              void           *ctx,
                              GlobalReaction *reaction )
{
     DirectResult    ret;
     FusionSkirmish *lock;

     D_MAGIC_ASSERT( reactor, FusionReactor );
     D_ASSERT( index >= 0 );
     D_ASSERT( reaction != NULL );

     D_DEBUG_AT( Fusion_Reactor, "%s( %p [%d], index %d, ctx %p, reaction %p )\n", __FUNCTION__,
                 reactor, reactor->id, index, ctx, reaction );

     /* Initialize reaction data. */
     reaction->index    = index;
     reaction->ctx      = ctx;
     reaction->attached = true;

     /* Remember for safety. */
     lock = reactor->globals_lock;

     /* Lock the list of global reactions. */
     ret = fusion_skirmish_prevail( lock );
     if (ret)
          return ret;

     if (lock != reactor->globals_lock)
          D_WARN( "using old lock once more" );

     /* Prepend the reaction to the list. */
     direct_list_prepend( &reactor->globals, &reaction->link );

     /* Unlock the list of global reactions. */
     fusion_skirmish_dismiss( lock );

     return DR_OK;
}

DirectResult
fusion_reactor_detach_global( FusionReactor  *reactor,
                              GlobalReaction *reaction )
{
     DirectResult    ret;
     FusionSkirmish *lock;

     D_MAGIC_ASSERT( reactor, FusionReactor );
     D_ASSERT( reaction != NULL );

     D_DEBUG_AT( Fusion_Reactor, "%s( %p [%d], reaction %p ) <- index %d, ctx %p\n", __FUNCTION__,
                 reactor, reactor->id, reaction, reaction->index, reaction->ctx );

     /* Remember for safety. */
     lock = reactor->globals_lock;

     /* Lock the list of global reactions. */
     ret = fusion_skirmish_prevail( lock );
     if (ret)
          return ret;

     if (lock != reactor->globals_lock)
          D_WARN( "using old lock once more" );

     /* Check against multiple detach. */
     if (reaction->attached) {
          /* Mark as detached. */
          reaction->attached = false;

          /* Remove the reaction from the list. */
          direct_list_remove( &reactor->globals, &reaction->link );
     }

     /* Unlock the list of global reactions. */
     fusion_skirmish_dismiss( lock );

     return DR_OK;
}

DirectResult
fusion_reactor_dispatch( FusionReactor      *reactor,
                         const void         *msg_data,
                         bool                self,
                         const ReactionFunc *globals )
{
     D_MAGIC_ASSERT( reactor, FusionReactor );

     return fusion_reactor_dispatch_channel( reactor, 0, msg_data, reactor->msg_size, self, globals );
}

DirectResult
fusion_reactor_direct( FusionReactor *reactor,
                       bool           direct )
{
     D_MAGIC_ASSERT( reactor, FusionReactor );

     reactor->direct = direct;

     return DR_OK;
}

void
_fusion_reactor_free_all( FusionWorld *world )
{
     ReactorNode *node, *node_next;

     D_MAGIC_ASSERT( world, FusionWorld );

     D_DEBUG_AT( Fusion_Reactor, "%s() <- nodes %p\n", __FUNCTION__, world->reactor_nodes );

     direct_mutex_lock( &world->reactor_nodes_lock );

     direct_list_foreach_safe (node, node_next, world->reactor_nodes) {
          NodeLink *link, *link_next;

          D_MAGIC_ASSERT( node, ReactorNode );

          direct_rwlock_wrlock( &node->lock );

          direct_list_foreach_safe (link, link_next, node->links) {
               D_MAGIC_ASSERT( link, NodeLink );

               D_MAGIC_CLEAR( link );

               D_FREE( link );
          }

          direct_rwlock_unlock( &node->lock );
          direct_rwlock_deinit( &node->lock );

          D_MAGIC_CLEAR( node );

          D_FREE( node );
     }

     world->reactor_nodes = NULL;

     direct_mutex_unlock( &world->reactor_nodes_lock );
}

static void
process_globals( FusionReactor      *reactor,
                 const void         *msg_data,
                 const ReactionFunc *globals )
{
     GlobalReaction *global, *next;
     FusionSkirmish *lock;
     int             max_index = -1;

     D_MAGIC_ASSERT( reactor, FusionReactor );
     D_ASSERT( msg_data != NULL );
     D_ASSERT( globals != NULL );

     D_DEBUG_AT( Fusion_Reactor, "  process_globals( %p [%d], msg_data %p, globals %p )\n",
                 reactor, reactor->id, msg_data, globals );

     /* Find maximum reaction index. */
     while (globals[max_index+1])
          max_index++;

     if (max_index < 0)
          return;

     /* Remember for safety. */
     lock = reactor->globals_lock;

     /* Lock the list of global reactions. */
     if (fusion_skirmish_prevail( lock ))
          return;

     if (lock != reactor->globals_lock)
          D_WARN( "using old lock once more" );

     /* Loop through all global reactions. */
     direct_list_foreach_safe (global, next, reactor->globals) {
          int index = global->index;

          /* Check if the index is valid. */
          if (index < 0 || index > max_index) {
               D_WARN( "global reaction index out of bounds (%d/%d)", global->index, max_index );
               continue;
          }

          /* Remove the reaction if requested. */
          if (globals[global->index]( msg_data, global->ctx ) == RS_REMOVE) {
               D_DEBUG_AT( Fusion_Reactor, "    -> removing %p, index %d, ctx %p\n",
                           global, global->index, global->ctx );

               /* Mark as detached, since the global reaction is being removed from the global reaction list. */
               global->attached = false;

               direct_list_remove( &reactor->globals, &global->link );
          }
     }

     /* Unlock the list of global reactions. */
     fusion_skirmish_dismiss( lock );
}

static ReactorNode *
lock_node( int            reactor_id,
           bool           add_it,
           bool           wlock,
           FusionReactor *reactor,
           FusionWorld   *world )
{
     ReactorNode       *node, *node_next;
     FusionWorldShared *shared;

     D_DEBUG_AT( Fusion_Reactor, "    lock_node( [%d], add %s, reactor %p )\n",
                 reactor_id, add_it ? "true" : "false", reactor );

     D_ASSERT( reactor != NULL || (!add_it && world != NULL) );

     if (reactor) {
          D_MAGIC_ASSERT( reactor, FusionReactor );
          D_MAGIC_ASSERT( reactor->shared, FusionWorldShared );

          shared = reactor->shared;

          world = _fusion_world( shared );
     }
     else {
          D_MAGIC_ASSERT( world, FusionWorld );
          D_MAGIC_ASSERT( world->shared, FusionWorldShared );

          shared = world->shared;
     }

     direct_mutex_lock( &world->reactor_nodes_lock );

     direct_list_foreach_safe (node, node_next, world->reactor_nodes) {
          D_MAGIC_ASSERT( node, ReactorNode );

          if (node->reactor_id == reactor_id) {
               if (wlock) {
                    NodeLink *link, *link_next;

                    direct_rwlock_wrlock( &node->lock );

                    direct_list_foreach_safe (link, link_next, node->links) {
                         D_MAGIC_ASSERT( link, NodeLink );

                         if (!link->reaction) {
                              D_DEBUG_AT( Fusion_Reactor, "    -> cleaning up %p\n", link );

                              remove_node_link( node, link );
                         }
                         else
                              D_ASSERT( link->reaction->node_link == link );
                    }
               }
               else
                    direct_rwlock_rdlock( &node->lock );

               if (!node->links && !add_it) {
                    direct_list_remove( &world->reactor_nodes, &node->link );

                    direct_rwlock_unlock( &node->lock );
                    direct_rwlock_deinit( &node->lock );

                    D_MAGIC_CLEAR( node );

                    D_FREE( node );

                    node = NULL;
               }
               else {
                    D_ASSERT( node->reactor == reactor || reactor == NULL );

                    direct_list_move_to_front( &world->reactor_nodes, &node->link );
               }

               direct_mutex_unlock( &world->reactor_nodes_lock );

               return node;
          }

          if (!direct_rwlock_trywrlock( &node->lock )) {
               if (!node->links) {
                    direct_list_remove( &world->reactor_nodes, &node->link );

                    direct_rwlock_unlock( &node->lock );
                    direct_rwlock_deinit( &node->lock );

                    D_MAGIC_CLEAR( node );

                    D_FREE( node );
               }
               else {
                    direct_rwlock_unlock( &node->lock );
               }
          }
     }

     if (add_it) {
          D_MAGIC_ASSERT( reactor, FusionReactor );

          node = D_CALLOC( 1, sizeof(ReactorNode) );
          if (!node) {
               D_OOM();
               return NULL;
          }

          direct_rwlock_init( &node->lock );

          if (wlock)
               direct_rwlock_wrlock( &node->lock );
          else
               direct_rwlock_rdlock( &node->lock );

          node->reactor_id = reactor_id;
          node->reactor    = reactor;

          D_MAGIC_SET( node, ReactorNode );

          direct_list_prepend( &world->reactor_nodes, &node->link );

          direct_mutex_unlock( &world->reactor_nodes_lock );

          return node;
     }

     direct_mutex_unlock( &world->reactor_nodes_lock );

     return NULL;
}

static void
unlock_node( ReactorNode *node )
{
     D_ASSERT( node != NULL );

     direct_rwlock_unlock( &node->lock );
}

#else /* FUSION_BUILD_MULTI */

FusionReactor *
fusion_reactor_new( int                msg_size,
                    const char        *name,
                    const FusionWorld *world )
{
     FusionReactor *reactor;

     D_ASSERT( name != NULL );
     D_MAGIC_ASSERT( world, FusionWorld );

     D_DEBUG_AT( Fusion_Reactor, "%s( '%s', size %d )\n", __FUNCTION__, name, msg_size );

     reactor = D_CALLOC( 1, sizeof(FusionReactor) );
     if (!reactor)
          return NULL;

     reactor->msg_size = msg_size;
     reactor->world    = (FusionWorld*) world;

     direct_recursive_mutex_init( &reactor->reactions_lock );
     direct_recursive_mutex_init( &reactor->globals_mutex );

     D_MAGIC_SET( reactor, FusionReactor );

     return reactor;
}

DirectResult
fusion_reactor_destroy( FusionReactor *reactor )
{
     D_MAGIC_ASSERT( reactor, FusionReactor );

     D_DEBUG_AT( Fusion_Reactor, "%s( %p )\n", __FUNCTION__, reactor );

     D_ASSUME( !reactor->destroyed );

     reactor->destroyed = true;

     return DR_OK;
}

DirectResult
fusion_reactor_free( FusionReactor *reactor )
{
     D_MAGIC_ASSERT( reactor, FusionReactor );

     D_DEBUG_AT( Fusion_Reactor, "%s( %p )\n", __FUNCTION__, reactor );

     if (_fusion_event_dispatcher_process_reactor_free( reactor->world, reactor ))
          return DR_OK;

     reactor->reactions = NULL;
     direct_mutex_deinit( &reactor->reactions_lock );

     reactor->globals = NULL;
     direct_mutex_deinit( &reactor->globals_mutex );

     D_MAGIC_CLEAR( reactor );

     D_FREE( reactor );

     return DR_OK;
}

DirectResult
fusion_reactor_attach_channel( FusionReactor *reactor,
                               int            channel,
                               ReactionFunc   func,
                               void          *ctx,
                               Reaction      *reaction )
{
     D_MAGIC_ASSERT( reactor, FusionReactor );
     D_ASSERT( func != NULL );
     D_ASSERT( reaction != NULL );

     D_DEBUG_AT( Fusion_Reactor, "%s( %p, func %p, ctx %p, reaction %p )\n", __FUNCTION__,
                 reactor, func, ctx, reaction );

     reaction->func      = func;
     reaction->ctx       = ctx;
     reaction->node_link = (void*)(long) channel;

     direct_mutex_lock( &reactor->reactions_lock );

     direct_list_prepend( &reactor->reactions, &reaction->link );

     direct_mutex_unlock( &reactor->reactions_lock );

     return DR_OK;
}

DirectResult
fusion_reactor_detach( FusionReactor *reactor,
                       Reaction      *reaction )
{
     D_ASSERT( reactor != NULL );
     D_ASSERT( reaction != NULL );

     D_MAGIC_ASSERT( reactor, FusionReactor );

     D_DEBUG_AT( Fusion_Reactor, "%s( %p, reaction %p ) <- func %p, ctx %p\n", __FUNCTION__,
                 reactor, reaction, reaction->func, reaction->ctx );

     direct_mutex_lock( &reactor->reactions_lock );

     direct_list_remove( &reactor->reactions, &reaction->link );

     direct_mutex_unlock( &reactor->reactions_lock );

     return DR_OK;
}

DirectResult
fusion_reactor_dispatch_channel( FusionReactor      *reactor,
                                 int                 channel,
                                 const void         *msg_data,
                                 int                 msg_size,
                                 bool                self,
                                 const ReactionFunc *globals )
{
     D_ASSERT( reactor != NULL );
     D_ASSERT( msg_data != NULL );

     D_MAGIC_ASSERT( reactor, FusionReactor );

     D_DEBUG_AT( Fusion_Reactor, "%s( %p, msg_data %p, self %s, globals %p)\n", __FUNCTION__,
                 reactor, msg_data, self ? "true" : "false", globals );

     if (channel == 0 && reactor->globals) {
          if (globals)
               process_globals( reactor, msg_data, globals );
          else
               D_ERROR( "Fusion/Reactor: There are global reactions but no globals have been passed to dispatch()!\n" );
     }

     if (!self)
          return DR_OK;

     _fusion_event_dispatcher_process_reactions( reactor->world, reactor, channel, (void*) msg_data, msg_size );

     return DR_OK;
}

DirectResult
fusion_reactor_set_dispatch_callback( FusionReactor  *reactor,
                                      FusionCall     *call,
                                      void           *call_ptr )
{
     D_MAGIC_ASSERT( reactor, FusionReactor );

     D_UNIMPLEMENTED();

     return DR_UNIMPLEMENTED;
}

DirectResult
fusion_reactor_set_name( FusionReactor *reactor,
                         const char    *name )
{
     D_MAGIC_ASSERT( reactor, FusionReactor );

     D_UNIMPLEMENTED();

     return DR_UNIMPLEMENTED;
}

DirectResult
fusion_reactor_add_permissions( FusionReactor            *reactor,
                                FusionID                  fusion_id,
                                FusionReactorPermissions  permissions )
{
     D_MAGIC_ASSERT( reactor, FusionReactor );

     return DR_OK;
}

DirectResult
fusion_reactor_set_lock( FusionReactor  *reactor,
                         FusionSkirmish *lock )

{
     D_ASSERT( reactor != NULL );
     D_ASSERT( lock != NULL );

     return DR_UNIMPLEMENTED;
}

DirectResult
fusion_reactor_set_lock_only( FusionReactor  *reactor,
                              FusionSkirmish *lock )
{
     D_ASSERT( reactor != NULL );
     D_ASSERT( lock != NULL );

     return DR_UNIMPLEMENTED;
}

DirectResult
fusion_reactor_attach( FusionReactor *reactor,
                       ReactionFunc   func,
                       void          *ctx,
                       Reaction      *reaction )
{
     D_MAGIC_ASSERT( reactor, FusionReactor );
     D_ASSERT( func != NULL );
     D_ASSERT( reaction != NULL );

     return fusion_reactor_attach_channel( reactor, 0, func, ctx, reaction );
}

DirectResult
fusion_reactor_attach_global( FusionReactor  *reactor,
                              int             index,
                              void           *ctx,
                              GlobalReaction *reaction )
{
     D_MAGIC_ASSERT( reactor, FusionReactor );
     D_ASSERT( index >= 0 );
     D_ASSERT( reaction != NULL );

     D_DEBUG_AT( Fusion_Reactor, "%s( %p, index %d, ctx %p, reaction %p )\n", __FUNCTION__,
                 reactor, index, ctx, reaction );

     reaction->index = index;
     reaction->ctx   = ctx;

     /* Mark the reaction as attached now. */
     reaction->attached = true;

     direct_mutex_lock( &reactor->globals_mutex );

     direct_list_prepend( &reactor->globals, &reaction->link );

     direct_mutex_unlock( &reactor->globals_mutex );

     return DR_OK;
}

DirectResult
fusion_reactor_detach_global( FusionReactor  *reactor,
                              GlobalReaction *reaction )
{
     D_MAGIC_ASSERT( reactor, FusionReactor );
     D_ASSERT( reaction != NULL );

     D_DEBUG_AT( Fusion_Reactor, "%s( %p, reaction %p ) <- index %d, ctx %p\n", __FUNCTION__,
                 reactor, reaction, reaction->index, reaction->ctx );

     direct_mutex_lock( &reactor->globals_mutex );

     /* Check to prevent multiple detaches from being performed. */
     if (reaction->attached) {
          /* Mark as detached, since the global reaction is being removed from the global reaction list. */
          reaction->attached = false;

          /* Remove the reaction from the list. */
          direct_list_remove( &reactor->globals, &reaction->link );
     }

     direct_mutex_unlock( &reactor->globals_mutex );

     return DR_OK;
}

DirectResult
fusion_reactor_dispatch( FusionReactor      *reactor,
                         const void         *msg_data,
                         bool                self,
                         const ReactionFunc *globals )
{
     D_MAGIC_ASSERT( reactor, FusionReactor );

     return fusion_reactor_dispatch_channel( reactor, 0, msg_data, reactor->msg_size, self, globals );
}

DirectResult
fusion_reactor_direct( FusionReactor *reactor,
                       bool           direct )
{
     D_MAGIC_ASSERT( reactor, FusionReactor );

     return DR_OK;
}

static void
process_globals( FusionReactor      *reactor,
                 const void         *msg_data,
                 const ReactionFunc *globals )
{
     GlobalReaction *global, *next;
     int             max_index = -1;

     D_MAGIC_ASSERT( reactor, FusionReactor );
     D_ASSERT( msg_data != NULL );
     D_ASSERT( globals != NULL );

     D_DEBUG_AT( Fusion_Reactor, "  process_globals( %p [%d], msg_data %p, globals %p )\n",
                 reactor, reactor->id, msg_data, globals );

     /* Find maximum reaction index. */
     while (globals[max_index+1])
          max_index++;

     if (max_index < 0)
          return;

     direct_mutex_lock( &reactor->globals_mutex );

     /* Loop through all global reactions. */
     direct_list_foreach_safe (global, next, reactor->globals) {
          if (global->index < 0 || global->index > max_index) {
               D_WARN( "global reaction index out of bounds (%d/%d)", global->index, max_index );
          }
          else {
               /* Remove the reaction if requested. */
               if (globals[global->index]( msg_data, global->ctx ) == RS_REMOVE) {
                    D_DEBUG_AT( Fusion_Reactor, "    -> removing %p, index %d, ctx %p\n",
                                global, global->index, global->ctx );

                    /* Mark as detached, since the global reaction is being removed from the global reaction list. */
                    global->attached = false;

                    direct_list_remove( &reactor->globals, &global->link );
               }
          }
     }

     direct_mutex_unlock( &reactor->globals_mutex );
}

#endif /* FUSION_BUILD_MULTI */

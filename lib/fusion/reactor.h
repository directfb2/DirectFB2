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

#ifndef __FUSION__REACTOR_H__
#define __FUSION__REACTOR_H__

#include <direct/list.h>
#include <fusion/lock.h>

/**********************************************************************************************************************/

struct __Fusion_FusionReactor {
     int                magic;

     int                id;
     int                msg_size;
     bool               direct;
     bool               destroyed;
     bool               free;

     DirectLink        *globals;
     FusionSkirmish    *globals_lock;
     DirectMutex        globals_mutex;

     FusionWorldShared *shared;
     FusionWorld       *world;

     DirectLink        *listeners;
     FusionSkirmish     listeners_lock;
     FusionCall        *call;

     DirectLink        *reactions;
     DirectMutex        reactions_lock;
};

/**********************************************************************************************************************/

typedef enum {
     RS_OK     = 0x00000000,
     RS_REMOVE = 0x00000001,
     RS_DROP   = 0x00000002
} ReactionResult;

typedef enum {
     FUSION_REACTOR_PERMIT_NONE          = 0x00000000,

     FUSION_REACTOR_PERMIT_ATTACH_DETACH = 0x00000001,
     FUSION_REACTOR_PERMIT_DISPATCH      = 0x00000002,

     FUSION_REACTOR_PERMIT_ALL           = 0x00000003
} FusionReactorPermissions;

typedef ReactionResult (*ReactionFunc)( const void *msg_data, void *ctx );

typedef struct {
     DirectLink    link;
     ReactionFunc  func;
     void         *ctx;
     void         *node_link;
} Reaction;

typedef struct {
     DirectLink  link;
     int         index;
     void       *ctx;
     bool        attached;
} GlobalReaction;

/**********************************************************************************************************************/

/*
 * Create a new reactor configured for the specified message data size.
 */
FusionReactor FUSION_API *fusion_reactor_new                  ( int                       msg_size,
                                                                const char               *name,
                                                                const FusionWorld        *world );

/*
 * Destroy the reactor.
 */
DirectResult  FUSION_API  fusion_reactor_destroy              ( FusionReactor            *reactor );

/*
 * Free the reactor.
 */
DirectResult  FUSION_API  fusion_reactor_free                 ( FusionReactor            *reactor );

/*
 * Attach a local reaction to a specific reactor channel (0-1023).
 */
DirectResult  FUSION_API  fusion_reactor_attach_channel       ( FusionReactor            *reactor,
                                                                int                       channel,
                                                                ReactionFunc              func,
                                                                void                     *ctx,
                                                                Reaction                 *reaction );

/*
 * Detach an attached local reaction from the reactor.
 */
DirectResult  FUSION_API  fusion_reactor_detach               ( FusionReactor            *reactor,
                                                                Reaction                 *reaction );

/*
 * Dispatch a message via a specific channel (0-1023).
 * Setting 'self' to false excludes the caller's local reactions.
 */
DirectResult  FUSION_API  fusion_reactor_dispatch_channel     ( FusionReactor            *reactor,
                                                                int                       channel,
                                                                const void               *msg_data,
                                                                int                       msg_size,
                                                                bool                      self,
                                                                const ReactionFunc       *globals );

/*
 * Have the call executed when a dispatched message has been processed by all recipients.
 */
DirectResult  FUSION_API  fusion_reactor_set_dispatch_callback( FusionReactor            *reactor,
                                                                FusionCall               *call,
                                                                void                     *call_ptr );

/*
 * Change the name of the reactor.
 */
DirectResult  FUSION_API  fusion_reactor_set_name             ( FusionReactor            *reactor,
                                                                const char               *name );

/*
 * Give permissions to another fusionee to use the reactor.
 */
DirectResult  FUSION_API  fusion_reactor_add_permissions      ( FusionReactor            *reactor,
                                                                FusionID                  fusion_id,
                                                                FusionReactorPermissions  permissions );

/*
 * Make the reactor use the specified lock for managing global reactions.
 */
DirectResult  FUSION_API  fusion_reactor_set_lock             ( FusionReactor            *reactor,
                                                                FusionSkirmish           *lock );

/*
 * Don't lock, i.e. use this function in fusion_object_set_lock() as this is called during object initialization.
 */
DirectResult  FUSION_API  fusion_reactor_set_lock_only        ( FusionReactor            *reactor,
                                                                FusionSkirmish           *lock );

/*
 * Attach a local reaction to the reactor (channel 0).
 */
DirectResult  FUSION_API  fusion_reactor_attach               ( FusionReactor            *reactor,
                                                                ReactionFunc              func,
                                                                void                     *ctx,
                                                                Reaction                 *reaction );

/*
 * Attach a global reaction to the reactor.
 * It's always called directly, no matter which fusionee calls fusion_reactor_dispatch().
 */
DirectResult  FUSION_API  fusion_reactor_attach_global        ( FusionReactor            *reactor,
                                                                int                       index,
                                                                void                     *ctx,
                                                                GlobalReaction           *reaction );

/*
 * Detach an attached global reaction from the reactor.
 */
DirectResult  FUSION_API  fusion_reactor_detach_global        ( FusionReactor            *reactor,
                                                                GlobalReaction           *reaction );

/*
 * Dispatch a message to any attached reaction (channel 0).
 * Setting 'self' to false excludes the caller's local reactions.
 */
DirectResult  FUSION_API  fusion_reactor_dispatch             ( FusionReactor            *reactor,
                                                                const void               *msg_data,
                                                                bool                      self,
                                                                const ReactionFunc       *globals );

/*
 * Specify whether local message handlers (reactions) should be called directly.
 */
DirectResult  FUSION_API  fusion_reactor_direct               ( FusionReactor            *reactor,
                                                                bool                      direct );

#endif

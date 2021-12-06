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

#ifndef __FUSION__FUSION_H__
#define __FUSION__FUSION_H__

#include <direct/list.h>
#include <fusion/types.h>

/**********************************************************************************************************************/

typedef enum {
     FER_ANY    = 0x00000000,
     FER_MASTER = 0x00000001,
     FER_SLAVE  = 0x00000002
} FusionEnterRole;

typedef enum {
     FFA_CLOSE = 0x00000000,
     FFA_FORK  = 0x00000001
} FusionForkAction;

typedef enum {
     FFS_PREPARE = 0x00000000,
     FFS_PARENT  = 0x00000001,
     FFS_CHILD   = 0x00000002
} FusionForkState;

typedef void (*FusionForkCallback) ( FusionForkAction action, FusionForkState state );

typedef void (*FusionLeaveCallback)( FusionWorld *world, FusionID fusion_id, void *ctx );

typedef void (*FusionDispatchCleanupFunc)( void *ctx );

typedef struct {
     DirectLink                    link;

     FusionDispatchCleanupFunc     func;
     void                         *ctx;
} FusionDispatchCleanup;

/**********************************************************************************************************************/

/*
 * Enters a fusion world by joining or creating it.
 * If 'world_index' is negative, the next free index is used to create a new world.
 * Otherwise the world with the specified index is joined or created.
 */
DirectResult     FUSION_API  fusion_enter                   ( int                         world_index,
                                                              int                         abi_version,
                                                              FusionEnterRole             role,
                                                              FusionWorld               **ret_world );

/*
 * Unblock slaves from entering.
 */
DirectResult     FUSION_API  fusion_world_activate          ( FusionWorld                *world );

/*
 * Stop the dispatcher thread.
 */
DirectResult     FUSION_API  fusion_stop_dispatcher         ( FusionWorld                *world,
                                                              bool                        emergency );

/*
 * Exits the fusion world.
 * If 'emergency' is true the function won't join but kill the dispatcher thread.
 */
DirectResult     FUSION_API  fusion_exit                    ( FusionWorld                *world,
                                                              bool                        emergency );

/*
 * Sends a signal to one or more fusionees and optionally waits for their processes to terminate.
 * A fusion_id of zero means all fusionees.
 * A timeout of zero means infinite waiting while a negative value means no waiting at all.
 */
DirectResult     FUSION_API  fusion_kill                    ( FusionWorld                *world,
                                                              FusionID                    fusion_id,
                                                              int                         signal,
                                                              int                         timeout_ms );

/*
 * Return the location of shared memory file.
 */
const char       FUSION_API *fusion_get_tmpfs               ( FusionWorld                *world );

/*
 * Add dispatch cleanup handler.
 */
DirectResult     FUSION_API  fusion_dispatch_cleanup_add    ( FusionWorld                *world,
                                                              FusionDispatchCleanupFunc   func,
                                                              void                       *ctx,
                                                              FusionDispatchCleanup     **ret_cleanup );

/*
 * Remove dispatch cleanup handler.
 */
DirectResult     FUSION_API  fusion_dispatch_cleanup_remove ( FusionWorld                *world,
                                                              FusionDispatchCleanup      *cleanup );

/*
 * Dispatch.
 */
DirectResult     FUSION_API  fusion_dispatch                ( FusionWorld                *world,
                                                              size_t                      buf_size );

/*
 * Get the executable path of the fusionee.
 */
DirectResult     FUSION_API  fusion_get_fusionee_path       ( const FusionWorld          *world,
                                                              FusionID                    fusion_id,
                                                              char                       *buf,
                                                              size_t                      buf_size,
                                                              size_t                     *ret_size );

/*
 * Get the PID of the fusionee.
 */
DirectResult     FUSION_API  fusion_get_fusionee_pid        ( const FusionWorld          *world,
                                                              FusionID                    fusion_id,
                                                              pid_t                      *ret_pid );

/*
 * Set the world root, i.e. the shared core.
 */
DirectResult     FUSION_API  fusion_world_set_root          ( FusionWorld                *world,
                                                              void                       *root );

/*
 * Get the world root.
 */
void             FUSION_API *fusion_world_get_root          ( FusionWorld                *world );

/*
 * Wait until all pending messages are processed.
 */
DirectResult     FUSION_API  fusion_sync                    ( const FusionWorld          *world );

/*
 * Sets the fork() action of the calling fusionee within the world.
 */
void             FUSION_API  fusion_world_set_fork_action   ( FusionWorld                *world,
                                                              FusionForkAction            action );

/*
 * Gets the current fork() action.
 */
FusionForkAction FUSION_API  fusion_world_get_fork_action   ( FusionWorld                *world );

/*
 * Registers a callback called upon fork().
 */
void             FUSION_API  fusion_world_set_fork_callback ( FusionWorld                *world,
                                                              FusionForkCallback          callback );

/*
 * Registers a callback called when a slave exits.
 */
void             FUSION_API  fusion_world_set_leave_callback( FusionWorld                *world,
                                                              FusionLeaveCallback         callback,
                                                              void                       *ctx );

/*
 * Returns the index of the specified world.
 */
int              FUSION_API  fusion_world_index             ( const FusionWorld          *world );

/*
 * Returns the own Fusion ID within the specified world.
 */
FusionID         FUSION_API  fusion_id                      ( const FusionWorld          *world );

/*
 * Returns if the world is a multi application world.
 */
bool             FUSION_API  fusion_is_multi                ( const FusionWorld          *world );

/*
 * Returns the thread ID of the Fusion Dispatcher within the specified world.
 */
pid_t            FUSION_API  fusion_dispatcher_tid          ( const FusionWorld          *world );

/*
 * Returns true if this process is the master.
 */
bool             FUSION_API  fusion_master                  ( const FusionWorld          *world );

/*
 * Check if a pointer points to the shared memory.
 */
bool             FUSION_API  fusion_is_shared               ( FusionWorld                *world,
                                                              const void                 *ptr );

#endif

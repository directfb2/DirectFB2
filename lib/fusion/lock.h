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

#ifndef __FUSION__LOCK_H__
#define __FUSION__LOCK_H__

#include <direct/debug.h>
#include <direct/waitqueue.h>
#include <fusion/types.h>

/**********************************************************************************************************************/

typedef struct {
     int              magic;
     DirectMutex      lock;
     DirectWaitQueue  cond;
     int              count;
     char            *name;
} FusionSkirmishSingle;

typedef struct {
     /* multi app */
     struct {
          int                      id;
          const FusionWorldShared *shared;
          /* builtin impl */
          struct {
               unsigned int        locked;
               pid_t               owner;
               DirectLink         *waiting;
               bool                requested;
               bool                destroyed;
          } builtin;
     } multi;

     /* single app */
     FusionSkirmishSingle         *single;
} FusionSkirmish;

typedef enum {
     FUSION_SKIRMISH_PERMIT_NONE       = 0x00000000,

     FUSION_SKIRMISH_PERMIT_PREVAIL    = 0x00000001,
     FUSION_SKIRMISH_PERMIT_SWOOP      = 0x00000002,
     FUSION_SKIRMISH_PERMIT_DISMISS    = 0x00000004,
     FUSION_SKIRMISH_PERMIT_LOCK_COUNT = 0x00000008,
     FUSION_SKIRMISH_PERMIT_WAIT       = 0x00000010,
     FUSION_SKIRMISH_PERMIT_NOTIFY     = 0x00000020,
     FUSION_SKIRMISH_PERMIT_DESTROY    = 0x00000040,

     FUSION_SKIRMISH_PERMIT_ALL        = 0x0000007F
} FusionSkirmishPermissions;

#if D_DEBUG_ENABLED
#define FUSION_SKIRMISH_ASSERT(skirmish)                                            \
     do {                                                                           \
          int lock_count;                                                           \
                                                                                    \
          D_ASSERT( skirmish != NULL );                                             \
          D_ASSERT( fusion_skirmish_lock_count( skirmish, &lock_count ) == DR_OK ); \
          D_ASSERT( lock_count > 0 );                                               \
     } while (0)
#else
#define FUSION_SKIRMISH_ASSERT(skirmish)                                            \
     do {                                                                           \
     } while (0)
#endif

/**********************************************************************************************************************/

/*
 * Initialize.
 */
DirectResult FUSION_API fusion_skirmish_init           ( FusionSkirmish             *skirmish,
                                                         const char                 *name,
                                                         const FusionWorld          *world );

/*
 * Initialize with 'local' to create a fake skirmish (simple mutex).
 * This can be used by secure fusion mode when skirmishs are no longer locked by slaves at all.
 */
DirectResult FUSION_API fusion_skirmish_init2          ( FusionSkirmish             *skirmish,
                                                         const char                 *name,
                                                         const FusionWorld          *world,
                                                         bool                        local );

/*
 * Lock.
 */
DirectResult FUSION_API fusion_skirmish_prevail        ( FusionSkirmish             *skirmish );

/*
 * Try lock.
 */
DirectResult FUSION_API fusion_skirmish_swoop          ( FusionSkirmish             *skirmish );

/*
 * Find out how many times current thread has acquired lock.
 */
DirectResult FUSION_API fusion_skirmish_lock_count     ( FusionSkirmish             *skirmish,
                                                         int                        *lock_count );

/*
 * Unlock.
 */
DirectResult FUSION_API fusion_skirmish_dismiss        ( FusionSkirmish             *skirmish );

/*
 * Deinitialize.
 */
DirectResult FUSION_API fusion_skirmish_destroy        ( FusionSkirmish             *skirmish );

/*
 * Wait.
 */
DirectResult FUSION_API fusion_skirmish_wait           ( FusionSkirmish             *skirmish,
                                                         unsigned int                timeout );

/*
 * Notify.
 */
DirectResult FUSION_API fusion_skirmish_notify         ( FusionSkirmish             *skirmish );

/*
 * Give permissions to another fusionee to use the skirmish.
 */
DirectResult FUSION_API fusion_skirmish_add_permissions( FusionSkirmish             *skimrish,
                                                         FusionID                    fusion_id,
                                                         FusionSkirmishPermissions   permissions );

/*
 * Lock by sorting by pointer first, so locks are always taken in same order.
 */
DirectResult FUSION_API fusion_skirmish_prevail_multi  ( FusionSkirmish            **skirmishs,
                                                         unsigned int                num );

/*
 * Unlock by sorting by pointer first.
 */
DirectResult FUSION_API fusion_skirmish_dismiss_multi  ( FusionSkirmish            **skirmishs,
                                                         unsigned int                num );

#endif

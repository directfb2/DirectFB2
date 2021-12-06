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

#ifndef __FUSION__REF_H__
#define __FUSION__REF_H__

#include <fusion/lock.h>

/**********************************************************************************************************************/

struct __Fusion_FusionRef {
     /* multi app */
     struct {
          int                  id;
          FusionWorldShared   *shared;
          FusionID             creator;
          /* builtin impl */
          struct {
               int             local;
               int             global;
               FusionSkirmish  lock;

               FusionCall     *call;
               int             call_arg;
          } builtin;
          bool                 user;
     } multi;

     /* single app */
     struct {
          int                  refs;
          DirectWaitQueue      cond;
          DirectMutex          lock;
          int                  dead;
          bool                 destroyed;
          int                  locked;

          FusionCall          *call;
          int                  call_arg;
     } single;
};

/**********************************************************************************************************************/

typedef enum {
     FUSION_REF_PERMIT_NONE             = 0x00000000,

     FUSION_REF_PERMIT_REF_UNREF_LOCAL  = 0x00000001,
     FUSION_REF_PERMIT_REF_UNREF_GLOBAL = 0x00000002,
     FUSION_REF_PERMIT_ZERO_LOCK_UNLOCK = 0x00000004,
     FUSION_REF_PERMIT_WATCH            = 0x00000008,
     FUSION_REF_PERMIT_INHERIT          = 0x00000010,
     FUSION_REF_PERMIT_DESTROY          = 0x00000020,
     FUSION_REF_PERMIT_CATCH            = 0x00000040,
     FUSION_REF_PERMIT_THROW            = 0x00000080,

     FUSION_REF_PERMIT_ALL              = 0x000000FF
} FusionRefPermissions;

/**********************************************************************************************************************/

/*
 * Initialize.
 */
DirectResult FUSION_API fusion_ref_init           ( FusionRef            *ref,
                                                    const char           *name,
                                                    const FusionWorld    *world );

/*
 * Initialize with 'user' set to true for secure fusion.
 */
DirectResult FUSION_API fusion_ref_init2          ( FusionRef            *ref,
                                                    const char           *name,
                                                    bool                  user,
                                                    const FusionWorld    *world );

/*
 * Change name.
 */
DirectResult FUSION_API fusion_ref_set_name       ( FusionRef            *ref,
                                                    const char           *name );

/*
 * Lock, increase, unlock.
 */
DirectResult FUSION_API fusion_ref_up             ( FusionRef            *ref,
                                                    bool                  global );

/*
 * Lock, decrease, unlock.
 */
DirectResult FUSION_API fusion_ref_down           ( FusionRef            *ref,
                                                    bool                  global );

/*
 * Catch reference
 */
DirectResult FUSION_API fusion_ref_catch          ( FusionRef            *ref );

/*
 * Throw reference
 */
DirectResult FUSION_API fusion_ref_throw          ( FusionRef            *ref,
                                                    FusionID              catcher );

/*
 * Get the current reference count. This value is not reliable, because no locking will be performed.
 */
DirectResult FUSION_API fusion_ref_stat           ( FusionRef            *ref,
                                                    int                  *refs );

/*
 * Check for zero and lock if true.
 */
DirectResult FUSION_API fusion_ref_zero_trylock   ( FusionRef            *ref );

/*
 * Unlock the counter.
 */
DirectResult FUSION_API fusion_ref_unlock         ( FusionRef            *ref );

/*
 * Have the call executed when reference counter reaches zero.
 */
DirectResult FUSION_API fusion_ref_watch          ( FusionRef            *ref,
                                                    FusionCall           *call,
                                                    int                   call_arg );

/*
 * Inherit local reference count from another reference.
 */
DirectResult FUSION_API fusion_ref_inherit        ( FusionRef            *ref,
                                                    FusionRef            *from );

/*
 * Deinitialize.
 */
DirectResult FUSION_API fusion_ref_destroy        ( FusionRef            *ref );

/*
 * Give permissions to another fusionee to use the reference.
 */
DirectResult FUSION_API fusion_ref_add_permissions( FusionRef            *ref,
                                                    FusionID              fusion_id,
                                                    FusionRefPermissions  permissions );

#endif

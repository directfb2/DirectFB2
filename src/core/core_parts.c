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
#include <core/core_parts.h>
#include <fusion/shmalloc.h>

D_DEBUG_DOMAIN( Core_Parts, "Core/Parts", "DirectFB Core Parts" );

/**********************************************************************************************************************/

DFBResult
dfb_core_part_initialize( CoreDFB  *core,
                          CorePart *core_part )
{
     DFBResult            ret;
     void                *local  = NULL;
     void                *shared = NULL;
     FusionSHMPoolShared *pool;

     pool = dfb_core_shmpool( core );

     if (core_part->initialized) {
          D_BUG( "%s already initialized", core_part->name );
          return DFB_BUG;
     }

     D_DEBUG_AT( Core_Parts, "Going to initialize '%s' core...\n", core_part->name );

     if (core_part->size_local)
          local = D_CALLOC( 1, core_part->size_local );

     if (core_part->size_shared)
          shared = SHCALLOC( pool, 1, core_part->size_shared );

     core_part->data_local  = local;
     core_part->data_shared = shared;

     ret = core_part->Initialize( core, local, shared );
     if (ret) {
          D_DERROR( ret, "Core/Parts: Could not initialize '%s' core!\n", core_part->name );

          if (shared)
               SHFREE( pool, shared );

          if (local)
               D_FREE( local );

          core_part->data_local  = NULL;
          core_part->data_shared = NULL;

          return ret;
     }

     if (shared)
          core_arena_add_shared_field( core, core_part->name, shared );

     core_part->initialized = true;

     return DFB_OK;
}

DFBResult
dfb_core_part_join( CoreDFB  *core,
                    CorePart *core_part )
{
     DFBResult  ret;
     void      *local  = NULL;
     void      *shared = NULL;

     if (core_part->initialized) {
          D_BUG( "%s already joined", core_part->name );
          return DFB_BUG;
     }

     D_DEBUG_AT( Core_Parts, "Going to join '%s' core...\n", core_part->name );

     if (core_part->size_shared &&
         core_arena_get_shared_field( core, core_part->name, &shared ))
          return DFB_FUSION;

     if (core_part->size_local)
          local = D_CALLOC( 1, core_part->size_local );

     ret = core_part->Join( core, local, shared );
     if (ret) {
          D_DERROR( ret, "Core/Parts: Could not join '%s' core!\n", core_part->name );
          if (local)
               D_FREE( local );

          return ret;
     }

     core_part->data_local  = local;
     core_part->data_shared = shared;
     core_part->initialized = true;

     return DFB_OK;
}

DFBResult
dfb_core_part_shutdown( CoreDFB  *core,
                        CorePart *core_part,
                        bool      emergency )
{
     DFBResult            ret;
     FusionSHMPoolShared *pool;

     pool = dfb_core_shmpool( core );

     if (!core_part->initialized)
          return DFB_OK;

     D_DEBUG_AT( Core_Parts, "Going to shutdown '%s' core...\n", core_part->name );

     ret = core_part->Shutdown( core_part->data_local, emergency );
     if (ret)
          direct_messages_derror( ret, "Core/Parts: Could not shutdown '%s' core!\n", core_part->name );

     if (core_part->data_shared)
          SHFREE( pool, core_part->data_shared );

     if (core_part->data_local)
          D_FREE( core_part->data_local );

     core_part->data_local  = NULL;
     core_part->data_shared = NULL;
     core_part->initialized = false;

     return DFB_OK;
}

DFBResult
dfb_core_part_leave( CoreDFB  *core,
                     CorePart *core_part,
                     bool      emergency )
{
     DFBResult ret;

     if (!core_part->initialized)
          return DFB_OK;

     D_DEBUG_AT( Core_Parts, "Going to leave '%s' core...\n", core_part->name );

     ret = core_part->Leave( core_part->data_local, emergency );
     if (ret)
          D_DERROR( ret, "Core/Parts: Could not leave '%s' core!\n", core_part->name );

     if (core_part->data_local)
          D_FREE( core_part->data_local );

     core_part->data_local  = NULL;
     core_part->data_shared = NULL;
     core_part->initialized = false;

     return DFB_OK;
}

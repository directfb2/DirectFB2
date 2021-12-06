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

#include <core/CoreGraphicsState.h>
#include <core/core.h>
#include <core/graphics_state.h>

D_DEBUG_DOMAIN( Core_GraphicsState, "Core/GraphicsState", "DirectFB Core Graphics State" );

/**********************************************************************************************************************/

static void
state_destructor( FusionObject *object,
                  bool          zombie,
                  void         *ctx )
{
     CoreGraphicsState *state = (CoreGraphicsState*) object;

     D_MAGIC_ASSERT( state, CoreGraphicsState );

     D_DEBUG_AT( Core_GraphicsState, "Destroying state %p (%p%s)\n", state, &state->state, zombie ? " ZOMBIE" : "" );

     dfb_state_set_destination( &state->state, NULL );
     dfb_state_set_source( &state->state, NULL );
     dfb_state_set_source2( &state->state, NULL );
     dfb_state_set_source_mask( &state->state, NULL );

     dfb_state_destroy( &state->state );

     CoreGraphicsState_Deinit_Dispatch( &state->call );

     D_MAGIC_CLEAR( state );

     /* Destroy the object. */
     fusion_object_destroy( object );
}

FusionObjectPool *
dfb_graphics_state_pool_create( const FusionWorld *world )
{
     FusionObjectPool *pool;

     pool = fusion_object_pool_create( "GraphicsState Pool",
                                       sizeof(CoreGraphicsState), sizeof(CoreGraphicsStateNotification),
                                       state_destructor, NULL, world );

     return pool;
}

/**********************************************************************************************************************/

DFBResult
dfb_graphics_state_create( CoreDFB            *core,
                           CoreGraphicsState **ret_state )
{
     CoreGraphicsState *state;

     D_DEBUG_AT( Core_GraphicsState, "%s()\n", __FUNCTION__ );

     D_ASSERT( ret_state );

     state = dfb_core_create_graphics_state( core );
     if (!state)
          return DFB_FUSION;

     dfb_state_init( &state->state, core );

     CoreGraphicsState_Init_Dispatch( core, state, &state->call );

     if (dfb_config->graphics_state_call_limit)
          fusion_call_set_quota( &state->call, state->object.identity, dfb_config->graphics_state_call_limit );

     D_MAGIC_SET( state, CoreGraphicsState );

     /* Activate object. */
     fusion_object_activate( &state->object );

     /* Return the new state. */
     *ret_state = state;

     D_DEBUG_AT( Core_GraphicsState, "  -> %p\n", state );

     return DFB_OK;
}

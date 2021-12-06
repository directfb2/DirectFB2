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

#ifndef __CORE__COREDFB_INCLUDES_H__
#define __CORE__COREDFB_INCLUDES_H__

#include <core/core.h>
#include <core/graphics_state.h>
#include <core/layer_context.h>
#include <core/palette.h>
#include <core/surface_allocation.h>
#include <core/surface_client.h>
#include <core/windows.h>
#include <core/windowstack.h>

/**********************************************************************************************************************/

static __inline__ DirectResult
CoreDFB_Call( CoreDFB             *core,
              FusionCallExecFlags  flags,
              int                  call_arg,
              void                *ptr,
              unsigned int         length,
              void                *ret_ptr,
              unsigned int         ret_size,
              unsigned int        *ret_length )
{
     return fusion_call_execute3( &core->shared->call, dfb_config->call_nodirect | flags, call_arg, ptr, length,
                                  ret_ptr, ret_size, ret_length );
}

/**********************************************************************************************************************/

static __inline__ DirectResult
CoreGraphicsState_Catch( CoreDFB            *core,
                         void               *object_ptr,
                         CoreGraphicsState **ret_state )
{
     *ret_state = object_ptr;

     return fusion_object_catch( object_ptr );
}

static __inline__ DirectResult
CoreGraphicsState_Throw( CoreGraphicsState *state,
                         FusionID           catcher,
                         u32               *ret_object_id )
{
     *ret_object_id = state->object.id;

     fusion_reactor_add_permissions( state->object.reactor, catcher, FUSION_REACTOR_PERMIT_ATTACH_DETACH );
     fusion_ref_add_permissions( &state->object.ref, catcher,
                                 FUSION_REF_PERMIT_REF_UNREF_LOCAL | FUSION_REF_PERMIT_CATCH );
     fusion_call_add_permissions( &state->call, catcher, FUSION_CALL_PERMIT_EXECUTE );

     if (dfb_config->graphics_state_call_limit)
          fusion_call_set_quota( &state->call, catcher, dfb_config->graphics_state_call_limit );

     fusion_object_add_owner( &state->object, catcher );

     return fusion_ref_throw( &state->object.ref, catcher );
}

/**********************************************************************************************************************/

static __inline__ u32
CoreLayerContext_GetID( const CoreLayerContext *context )
{
     return context->object.id;
}

static __inline__ DirectResult
CoreLayerContext_Lookup( CoreDFB           *core,
                         u32                object_id,
                         FusionID           caller,
                         CoreLayerContext **ret_context )
{
     DFBResult         ret;
     CoreLayerContext *context;

     ret = dfb_core_get_layer_context( core, object_id, &context );
     if (ret)
          return ret;

     if (fusion_object_check_owner( &context->object, caller, true )) {
          dfb_layer_context_unref( context );
          return DR_ACCESSDENIED;
     }

     *ret_context = context;

     return DR_OK;
}

static __inline__ DirectResult
CoreLayerContext_Unref( CoreLayerContext *context )
{
     return dfb_layer_context_unref( context );
}

static __inline__ DirectResult
CoreLayerContext_Catch( CoreDFB           *core,
                        void              *object_ptr,
                        CoreLayerContext **ret_context )
{
     *ret_context = object_ptr;

     return fusion_object_catch( object_ptr );
}

static __inline__ DirectResult
CoreLayerContext_Throw( CoreLayerContext *context,
                        FusionID          catcher,
                        u32              *ret_object_id )
{
     *ret_object_id = context->object.id;

     fusion_reactor_add_permissions( context->object.reactor, catcher, FUSION_REACTOR_PERMIT_ATTACH_DETACH );
     fusion_ref_add_permissions( &context->object.ref, catcher,
                                 FUSION_REF_PERMIT_REF_UNREF_LOCAL | FUSION_REF_PERMIT_CATCH );
     fusion_call_add_permissions( &context->call, catcher, FUSION_CALL_PERMIT_EXECUTE );

     if (context->stack)
          fusion_call_add_permissions( &context->stack->call, catcher, FUSION_CALL_PERMIT_EXECUTE );

     return fusion_ref_throw( &context->object.ref, catcher );
}

/**********************************************************************************************************************/

static __inline__ DirectResult
CoreLayerRegion_Catch( CoreDFB          *core,
                       void             *object_ptr,
                       CoreLayerRegion **ret_region )
{
     *ret_region = object_ptr;

     return fusion_object_catch( object_ptr );
}

static __inline__ DirectResult
CoreLayerRegion_Throw( CoreLayerRegion *region,
                       FusionID         catcher,
                       u32             *ret_object_id )
{
     *ret_object_id = region->object.id;

     fusion_reactor_add_permissions( region->object.reactor, catcher, FUSION_REACTOR_PERMIT_ATTACH_DETACH );
     fusion_ref_add_permissions( &region->object.ref, catcher,
                                 FUSION_REF_PERMIT_REF_UNREF_LOCAL | FUSION_REF_PERMIT_CATCH );
     fusion_call_add_permissions( &region->call, catcher, FUSION_CALL_PERMIT_EXECUTE );

     return fusion_ref_throw( &region->object.ref, catcher );
}

/**********************************************************************************************************************/

static __inline__ u32
CorePalette_GetID( const CorePalette *palette )
{
     return palette->object.id;
}

static __inline__ DirectResult
CorePalette_Lookup( CoreDFB      *core,
                    u32           object_id,
                    FusionID      caller,
                    CorePalette **ret_palette )
{
     DFBResult    ret;
     CorePalette *palette;

     ret = dfb_core_get_palette( core, object_id, &palette );
     if (ret)
          return ret;

     if (fusion_object_check_owner( &palette->object, caller, false )) {
          dfb_palette_unref( palette );
          return DR_ACCESSDENIED;
     }

     *ret_palette = palette;

     return DR_OK;
}

static __inline__ DirectResult
CorePalette_Unref( CorePalette *palette )
{
     return dfb_palette_unref( palette );
}

static __inline__ DirectResult
CorePalette_Catch( CoreDFB      *core,
                   void         *object_ptr,
                   CorePalette **ret_palette )
{
     *ret_palette = object_ptr;

     return fusion_object_catch( object_ptr );
}

static __inline__ DirectResult
CorePalette_Throw( CorePalette *palette,
                   FusionID     catcher,
                   u32         *ret_object_id )
{
     *ret_object_id = palette->object.id;

     fusion_reactor_add_permissions( palette->object.reactor, catcher, FUSION_REACTOR_PERMIT_ATTACH_DETACH );
     fusion_ref_add_permissions( &palette->object.ref, catcher,
                                 FUSION_REF_PERMIT_REF_UNREF_LOCAL | FUSION_REF_PERMIT_CATCH );
     fusion_call_add_permissions( &palette->call, catcher, FUSION_CALL_PERMIT_EXECUTE );

     fusion_object_add_owner( &palette->object, catcher );

     return fusion_ref_throw( &palette->object.ref, catcher );
}

/**********************************************************************************************************************/

static __inline__ u32
CoreSurface_GetID( const CoreSurface *surface )
{
     return surface->object.id;
}

static __inline__ DirectResult
CoreSurface_Lookup( CoreDFB      *core,
                    u32           object_id,
                    FusionID      caller,
                    CoreSurface **ret_surface )
{
     DFBResult    ret;
     CoreSurface *surface;

     ret = fusion_object_lookup( core->shared->surface_pool, object_id, (FusionObject**) &surface );
     if (ret)
          return ret;

     if (caller != FUSION_ID_MASTER && surface->object.identity != caller &&
         fusion_object_check_owner( &surface->object, caller, false )) {
          return DR_ACCESSDENIED;
     }

     *ret_surface = surface;

     return DR_OK;
}

static __inline__ DirectResult
CoreSurface_Unref( CoreSurface *surface )
{
     return DR_OK;
}

static __inline__ DirectResult
CoreSurface_Catch( CoreDFB      *core,
                   void         *object_ptr,
                   CoreSurface **ret_surface )
{
     *ret_surface = object_ptr;

     return fusion_object_catch( object_ptr );
}

static __inline__ DirectResult
CoreSurface_Throw( CoreSurface *surface,
                   FusionID     catcher,
                   u32         *ret_object_id )
{
     *ret_object_id = surface->object.id;

     fusion_reactor_add_permissions( surface->object.reactor, catcher,
                                     FUSION_REACTOR_PERMIT_ATTACH_DETACH | FUSION_REACTOR_PERMIT_DISPATCH );
     fusion_ref_add_permissions( &surface->object.ref, catcher,
                                 FUSION_REF_PERMIT_REF_UNREF_LOCAL | FUSION_REF_PERMIT_CATCH );
     fusion_call_add_permissions( &surface->call, catcher, FUSION_CALL_PERMIT_EXECUTE );

     fusion_object_add_owner( &surface->object, catcher );

     return fusion_ref_throw( &surface->object.ref, catcher );
}

/**********************************************************************************************************************/

static __inline__ DirectResult
CoreSurfaceAllocation_Catch( CoreDFB                *core,
                             void                   *object_ptr,
                             CoreSurfaceAllocation **ret_allocation )
{
     *ret_allocation = object_ptr;

     return fusion_object_catch( object_ptr );
}

static __inline__ DirectResult
CoreSurfaceAllocation_Throw( CoreSurfaceAllocation *allocation,
                             FusionID               catcher,
                             u32                   *ret_object_id )
{
     *ret_object_id = allocation->object.id;

     fusion_reactor_add_permissions( allocation->object.reactor, catcher, FUSION_REACTOR_PERMIT_ATTACH_DETACH );
     fusion_ref_add_permissions( &allocation->object.ref, catcher,
                                 FUSION_REF_PERMIT_REF_UNREF_LOCAL | FUSION_REF_PERMIT_CATCH );
     fusion_call_add_permissions( &allocation->call, catcher, FUSION_CALL_PERMIT_EXECUTE );

     fusion_object_add_owner( &allocation->object, catcher );

     return fusion_ref_throw( &allocation->object.ref, catcher );
}

/**********************************************************************************************************************/

static __inline__ u32
CoreSurfaceBuffer_GetID( const CoreSurfaceBuffer *buffer )
{
     return buffer->object.id;
}

static __inline__ DirectResult
CoreSurfaceBuffer_Lookup( CoreDFB            *core,
                          u32                 object_id,
                          FusionID            caller,
                          CoreSurfaceBuffer **ret_buffer )
{
     DFBResult          ret;
     CoreSurfaceBuffer *buffer;

     ret = dfb_core_get_surface_buffer( core, object_id, &buffer );
     if (ret)
          return ret;

     if (fusion_object_check_owner( &buffer->object, caller, false )) {
          dfb_surface_buffer_unref( buffer );
          return DR_ACCESSDENIED;
     }

     *ret_buffer = buffer;

     return DR_OK;
}

static __inline__ DirectResult
CoreSurfaceBuffer_Unref( CoreSurfaceBuffer *buffer )
{
     return dfb_surface_buffer_unref( buffer );
}

/**********************************************************************************************************************/

static __inline__ DirectResult
CoreSurfaceClient_Catch( CoreDFB            *core,
                         void               *object_ptr,
                         CoreSurfaceClient **ret_client )
{
     *ret_client = object_ptr;

     return fusion_object_catch( object_ptr );
}

static __inline__ DirectResult
CoreSurfaceClient_Throw( CoreSurfaceClient *client,
                         FusionID           catcher,
                         u32               *ret_object_id )
{
     *ret_object_id = client->object.id;

     fusion_reactor_add_permissions( client->object.reactor, catcher, FUSION_REACTOR_PERMIT_ATTACH_DETACH );
     fusion_ref_add_permissions( &client->object.ref, catcher,
                                 FUSION_REF_PERMIT_REF_UNREF_LOCAL | FUSION_REF_PERMIT_CATCH );
     fusion_call_add_permissions( &client->call, catcher, FUSION_CALL_PERMIT_EXECUTE );

     fusion_object_add_owner( &client->object, catcher );

     return fusion_ref_throw( &client->object.ref, catcher );
}

/**********************************************************************************************************************/

static __inline__ u32
CoreWindow_GetID( const CoreWindow *window )
{
     return window->object.id;
}

static __inline__ DirectResult
CoreWindow_Lookup( CoreDFB     *core,
                   u32          object_id,
                   FusionID     caller,
                   CoreWindow **ret_window )
{
     DFBResult   ret;
     CoreWindow *window;

     ret = dfb_core_get_window( core, object_id, &window );
     if (ret)
          return ret;

     if (caller != FUSION_ID_MASTER && window->object.identity != caller &&
         fusion_object_check_owner( &window->object, caller, false )) {
          dfb_window_unref( window );
          return DR_ACCESSDENIED;
     }

     *ret_window = window;

     return DR_OK;
}

static __inline__ DirectResult
CoreWindow_Unref( CoreWindow *window )
{
     return dfb_window_unref( window );
}

static __inline__ DirectResult
CoreWindow_Catch( CoreDFB     *core,
                  void        *object_ptr,
                  CoreWindow **ret_window )
{
     *ret_window = object_ptr;

     return fusion_object_catch( object_ptr );
}

static __inline__ DirectResult
CoreWindow_Throw( CoreWindow *window,
                  FusionID    catcher,
                  u32        *ret_object_id )
{
     *ret_object_id = window->object.id;

     fusion_reactor_add_permissions( window->object.reactor, catcher,
                                     FUSION_REACTOR_PERMIT_ATTACH_DETACH | FUSION_REACTOR_PERMIT_DISPATCH );
     fusion_ref_add_permissions( &window->object.ref, catcher,
                                 FUSION_REF_PERMIT_REF_UNREF_LOCAL | FUSION_REF_PERMIT_CATCH );
     fusion_call_add_permissions( &window->call, catcher, FUSION_CALL_PERMIT_EXECUTE );

     fusion_object_add_owner( &window->object, catcher );

     return fusion_ref_throw( &window->object.ref, catcher );
}

#endif

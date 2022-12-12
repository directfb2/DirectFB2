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

#include <core/CoreLayerRegion.h>
#include <core/CoreSurface.h>
#include <core/CoreSurfaceClient.h>
#include <core/core.h>
#include <core/layers.h>
#include <core/layer_context.h>
#include <core/layer_control.h>
#include <core/layer_region.h>
#include <core/screen.h>
#include <core/surface_allocation.h>
#include <core/surface_client.h>
#include <core/system.h>
#include <fusion/conf.h>
#include <fusion/shmalloc.h>
#include <gfx/util.h>

D_DEBUG_DOMAIN( Core_LayerRegion,       "Core/LayerRegion",        "DirectFB Core Display Layer Region" );
D_DEBUG_DOMAIN( Core_LayerRegionLock,   "Core/LayerRegion/Lock",   "DirectFB Core Display Layer Region Lock" );
D_DEBUG_DOMAIN( Core_LayerRegionUpdate, "Core/LayerRegion/Update", "DirectFB Core Display Layer Region Update" );

/**********************************************************************************************************************/

static DFBResult region_buffer_unlock      ( CoreLayerRegion            *region,
                                             CoreSurfaceBufferLock      *left_buffer_lock,
                                             CoreSurfaceBufferLock      *right_buffer_lock );

static DFBResult region_buffer_lock        ( CoreLayerRegion            *region,
                                             CoreSurface                *surface,
                                             DFBSurfaceBufferRole        role,
                                             CoreSurfaceBufferLock      *left_buffer_lock,
                                             CoreSurfaceBufferLock      *right_buffer_lock );

static DFBResult dfb_layer_region_set      ( CoreLayerRegion            *region,
                                             CoreLayerRegionConfig      *config,
                                             CoreLayerRegionConfigFlags  flags,
                                             CoreSurface                *surface );

static DFBResult dfb_layer_region_realize  ( CoreLayerRegion            *region,
                                             bool                        set );

static DFBResult dfb_layer_region_unrealize( CoreLayerRegion            *region );

/**********************************************************************************************************************/

static void
region_destructor( FusionObject *object,
                   bool          zombie,
                   void         *ctx )
{
     DFBResult         ret;
     CoreLayerRegion  *region = (CoreLayerRegion*) object;
     CoreLayer        *layer;
     CoreLayerShared  *shared;
     CoreLayerContext *context;

     layer  = dfb_layer_at( region->layer_id );

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->shared != NULL );

     shared = layer->shared;

     D_DEBUG_AT( Core_LayerRegion, "Destroying region %p (%s, %dx%d, %s, %s, %s, %s%s)\n",
                 region, shared->description.name, region->config.width, region->config.height,
                 D_FLAGS_IS_SET( region->state, CLRSF_CONFIGURED ) ? "configured" : "unconfigured",
                 D_FLAGS_IS_SET( region->state, CLRSF_ENABLED )    ? "enabled" : "disabled",
                 D_FLAGS_IS_SET( region->state, CLRSF_ACTIVE )     ? "active" : "inactive",
                 D_FLAGS_IS_SET( region->state, CLRSF_REALIZED )   ? "realized" : "not realized",
                 zombie ? " ZOMBIE" : "" );

     /* Hide region etc. */
     if (D_FLAGS_IS_SET( region->state, CLRSF_ENABLED ))
          dfb_layer_region_disable( region );

     /* Remove the region from the context. */
     ret = fusion_object_lookup( core_dfb->shared->layer_context_pool, region->context_id, (FusionObject**) &context );
     if (ret == DFB_OK)
          dfb_layer_context_remove_region( context, region );

     /* Throw away its surface. */
     if (region->surface) {
          /* Detach the global listener. */
          dfb_surface_detach_global( region->surface, &region->surface_reaction );

          /* Detach the surface event listener. */
          dfb_surface_detach( region->surface, &region->surface_event_reaction );

          dfb_surface_client_unref( region->surface_client );

          /* Unlink from structure. */
          dfb_surface_unlink( &region->surface );
     }

     /* Free driver's region data. */
     if (region->region_data)
          SHFREE( shared->shmpool, region->region_data );

     CoreLayerRegion_Deinit_Dispatch( &region->call );

     /* Deinitialize the lock. */
     fusion_skirmish_destroy( &region->lock );

     /* Destroy the object. */
     fusion_object_destroy( object );
}

FusionObjectPool *
dfb_layer_region_pool_create( const FusionWorld *world )
{
     FusionObjectPool *pool;

     pool = fusion_object_pool_create( "Layer Region Pool",
                                       sizeof(CoreLayerRegion), sizeof(CoreLayerRegionNotification),
                                       region_destructor, NULL, world );

     return pool;
}

/**********************************************************************************************************************/

DFBResult
dfb_layer_region_create( CoreLayerContext  *context,
                         CoreLayerRegion  **ret_region )
{
     CoreLayer       *layer;
     CoreLayerShared *shared;
     CoreLayerRegion *region;

     D_ASSERT( context != NULL );
     D_ASSERT( ret_region != NULL );

     D_DEBUG_AT( Core_LayerRegion, "%s()\n", __FUNCTION__ );

     layer = dfb_layer_at( context->layer_id );

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->shared != NULL );

     shared = layer->shared;

     /* Create the region object. */
     region = dfb_core_create_layer_region( layer->core );
     if (!region)
          return DFB_FUSION;

     region->layer_id   = context->layer_id;
     region->context_id = context->object.id;

     /* Initialize the lock. */
     if (fusion_skirmish_init2( &region->lock, "Layer Region", dfb_core_world( layer->core ),
                                fusion_config->secure_fusion )) {
          fusion_object_destroy( &region->object );
          return DFB_FUSION;
     }

     /* Change global reaction lock. */
     fusion_object_set_lock( &region->object, &region->lock );

     region->state = CLRSF_FROZEN;

     if (shared->description.surface_accessor)
          region->surface_accessor = shared->description.surface_accessor;
     else
          region->surface_accessor = CSAID_LAYER0 + region->layer_id;

     CoreLayerRegion_Init_Dispatch( layer->core, region, &region->call );

     /* Activate the object. */
     fusion_object_activate( &region->object );

     /* Add the region to the context. */
     dfb_layer_context_add_region( context, region );

     /* Return the new region. */
     *ret_region = region;

     D_DEBUG_AT( Core_LayerRegion, "  -> %p\n", region );

     return DFB_OK;
}

DirectResult
dfb_layer_region_lock( CoreLayerRegion *region )
{
     D_ASSERT( region != NULL );

     return fusion_skirmish_prevail( &region->lock );
}

DirectResult
dfb_layer_region_unlock( CoreLayerRegion *region )
{
     D_ASSERT( region != NULL );

     return fusion_skirmish_dismiss( &region->lock );
}

DFBResult
dfb_layer_region_activate( CoreLayerRegion *region )
{
     DFBResult ret;

     D_DEBUG_AT( Core_LayerRegion, "%s( %p )\n", __FUNCTION__, region );

     D_ASSERT( region != NULL );

     /* Lock the region. */
     if (dfb_layer_region_lock( region ))
          return DFB_FUSION;

     D_ASSUME( !D_FLAGS_IS_SET( region->state, CLRSF_ACTIVE ) );

     if (D_FLAGS_IS_SET( region->state, CLRSF_ACTIVE )) {
          dfb_layer_region_unlock( region );
          return DFB_OK;
     }

     /* Realize the region if it's enabled. */
     if (D_FLAGS_IS_SET( region->state, CLRSF_ENABLED )) {
          ret = dfb_layer_region_realize( region, true );
          if (ret) {
               dfb_layer_region_unlock( region );
               return ret;
          }
     }

     /* Update the region's state. */
     D_FLAGS_SET( region->state, CLRSF_ACTIVE );

     /* Unlock the region. */
     dfb_layer_region_unlock( region );

     return DFB_OK;
}

DFBResult
dfb_layer_region_deactivate( CoreLayerRegion *region )
{
     DFBResult ret;

     D_DEBUG_AT( Core_LayerRegion, "%s( %p )\n", __FUNCTION__, region );

     D_ASSERT( region != NULL );

     /* Lock the region. */
     if (dfb_layer_region_lock( region ))
          return DFB_FUSION;

     D_ASSUME( D_FLAGS_IS_SET( region->state, CLRSF_ACTIVE ) );

     if (!D_FLAGS_IS_SET( region->state, CLRSF_ACTIVE )) {
          dfb_layer_region_unlock( region );
          return DFB_OK;
     }

     /* Unrealize the region. */
     if (D_FLAGS_IS_SET( region->state, CLRSF_REALIZED )) {
          ret = dfb_layer_region_unrealize( region );
          if (ret) {
               dfb_layer_region_unlock( region );
               return ret;
          }
     }

     /* Update the region's state. */
     D_FLAGS_CLEAR( region->state, CLRSF_ACTIVE );

     /* Unlock the region. */
     dfb_layer_region_unlock( region );

     return DFB_OK;
}

DFBResult
dfb_layer_region_enable( CoreLayerRegion *region )
{
     DFBResult ret;

     D_DEBUG_AT( Core_LayerRegion, "%s( %p )\n", __FUNCTION__, region );

     D_ASSERT( region != NULL );

     /* Lock the region. */
     if (dfb_layer_region_lock( region ))
          return DFB_FUSION;

     D_ASSUME( !D_FLAGS_IS_SET( region->state, CLRSF_ENABLED ) );

     if (D_FLAGS_IS_SET( region->state, CLRSF_ENABLED )) {
          dfb_layer_region_unlock( region );
          return DFB_OK;
     }

     /* Realize the region if it's active. */
     if (D_FLAGS_IS_SET( region->state, CLRSF_ACTIVE )) {
          ret = dfb_layer_region_realize( region, true );
          if (ret) {
               dfb_layer_region_unlock( region );
               return ret;
          }
     }

     /* Update the region's state. */
     D_FLAGS_SET( region->state, CLRSF_ENABLED );

     /* Unlock the region. */
     dfb_layer_region_unlock( region );

     return DFB_OK;
}

DFBResult
dfb_layer_region_disable( CoreLayerRegion *region )
{
     DFBResult ret;

     D_DEBUG_AT( Core_LayerRegion, "%s( %p )\n", __FUNCTION__, region );

     D_ASSERT( region != NULL );

     /* Lock the region. */
     if (dfb_layer_region_lock( region ))
          return DFB_FUSION;

     D_ASSUME( D_FLAGS_IS_SET( region->state, CLRSF_ENABLED ) );

     if (!D_FLAGS_IS_SET( region->state, CLRSF_ENABLED )) {
          dfb_layer_region_unlock( region );
          return DFB_OK;
     }

     /* Unrealize the region. */
     if (D_FLAGS_IS_SET( region->state, CLRSF_REALIZED )) {
          ret = dfb_layer_region_unrealize( region );
          if (ret)
               return ret;
     }

     /* Update the region's state. */
     D_FLAGS_CLEAR( region->state, CLRSF_ENABLED );

     /* Unlock the region. */
     dfb_layer_region_unlock( region );

     return DFB_OK;
}

static ReactionResult
region_surface_react( const void *msg_data,
                      void       *ctx )
{
     const DFBSurfaceEvent *evt  = msg_data;
     CoreLayerRegion       *region = ctx;

     D_DEBUG_AT( Core_LayerRegionUpdate, "%s( %p ) <- type %06x\n", __FUNCTION__, evt, evt->type );
     D_DEBUG_AT( Core_LayerRegionUpdate, "  -> surface id %u\n", evt->surface_id );

     if (evt->type == DSEVT_UPDATE) {
          D_DEBUG_AT( Core_LayerRegionUpdate, "  -> updated %4d,%4d-%4dx%4d (left)\n",
                      DFB_RECTANGLE_VALS_FROM_REGION( &evt->update ) );
          D_DEBUG_AT( Core_LayerRegionUpdate, "  -> updated %4d,%4d-%4dx%4d (right)\n",
                      DFB_RECTANGLE_VALS_FROM_REGION( &evt->update_right ) );
          D_DEBUG_AT( Core_LayerRegionUpdate, "  -> flip count %u\n", evt->flip_count );
          D_DEBUG_AT( Core_LayerRegionUpdate, "  -> time stamp %lld\n", evt->time_stamp );
          D_DEBUG_AT( Core_LayerRegionUpdate, "  -> layer region %p\n", region );

          if (direct_log_domain_check( &Core_LayerRegionUpdate )) {
               dfb_surface_lock( region->surface );
               CoreSurfaceBuffer *buffer;
               D_UNUSED_P( buffer );
               buffer = dfb_surface_get_buffer3( region->surface, DSBR_FRONT, DSSE_LEFT, evt->flip_count );
               D_DEBUG_AT( Core_LayerRegionUpdate, "  -> buffer       %p\n", buffer );
               dfb_surface_unlock( region->surface );
          }

          region->surface_flip_count = evt->flip_count;

          if (!CoreLayerRegion_FlipUpdate2( region, &evt->update, &evt->update_right, DSFLIP_ONSYNC | DSFLIP_UPDATE,
                                            evt->flip_count, evt->time_stamp ))
               CoreSurfaceClient_FrameAck( region->surface_client, evt->flip_count );
     }
     else if (evt->type == DSEVT_DESTROYED)
          return RS_REMOVE;

     return RS_OK;
}

DFBResult
dfb_layer_region_set_surface( CoreLayerRegion *region,
                              CoreSurface     *surface,
                              bool             update )
{
     DFBResult ret;

     D_DEBUG_AT( Core_LayerRegion, "%s( %p, %p, %d )\n", __FUNCTION__, region, surface, update );

     D_ASSERT( region != NULL );
     D_ASSERT( surface != NULL );

     /* Lock the region. */
     if (dfb_layer_region_lock( region ))
          return DFB_FUSION;

     if (region->surface != surface) {
          /* Setup hardware for the new surface if the region is realized. */
          if (D_FLAGS_IS_SET( region->state, CLRSF_REALIZED )) {
               ret = dfb_layer_region_set( region, &region->config, CLRCF_SURFACE | CLRCF_PALETTE, surface );
               if (ret) {
                    dfb_layer_region_unlock( region );
                    return ret;
               }
          }

          /* Throw away the old surface. */
          if (region->surface) {
               /* Detach the global listener. */
               dfb_surface_detach_global( region->surface, &region->surface_reaction );

               /* Detach the surface event listener. */
               dfb_surface_detach( region->surface, &region->surface_event_reaction );

               dfb_surface_client_unref( region->surface_client );

               /* Unlink surface from structure. */
               dfb_surface_unlink( &region->surface );
          }

          /* Take the new surface. */
          if (surface) {
               /* Link surface into structure. */
               if (dfb_surface_link( &region->surface, surface )) {
                    D_WARN( "region has lost its surface" );
                    dfb_layer_region_unlock( region );
                    return DFB_FUSION;
               }

               /* Create the surface client. */
               ret = CoreSurface_CreateClient( region->surface, &region->surface_client );
               if (ret) {
                    D_WARN( "failed to create surface client" );
                    dfb_layer_region_unlock( region );
                    return ret;
               }

               /* Attach the global listener. */
               dfb_surface_attach_global( region->surface, DFB_LAYER_REGION_SURFACE_LISTENER, region,
                                          &region->surface_reaction );

               /* Attach the surface event listener. */
               dfb_surface_attach_channel( region->surface, CSCH_EVENT, region_surface_react, region,
                                           &region->surface_event_reaction );
          }

          if (update && D_FLAGS_ARE_SET( region->state, CLRSF_ENABLED | CLRSF_ACTIVE )) {
               region->surface_flip_count = surface->flips;

               dfb_layer_region_flip_update( region, NULL, DSFLIP_UPDATE );
          }
     }

     /* Unlock the region. */
     dfb_layer_region_unlock( region );

     return DFB_OK;
}

DFBResult
dfb_layer_region_get_surface( CoreLayerRegion  *region,
                              CoreSurface     **ret_surface )
{
     D_DEBUG_AT( Core_LayerRegion, "%s( %p )\n", __FUNCTION__, region );

     D_ASSERT( region != NULL );
     D_ASSERT( ret_surface != NULL );

     /* Lock the region. */
     if (dfb_layer_region_lock( region ))
          return DFB_FUSION;

     D_ASSUME( region->surface != NULL );

     /* Check for NULL surface. */
     if (!region->surface) {
          dfb_layer_region_unlock( region );
          return DFB_UNSUPPORTED;
     }

     /* Increase the surface's reference counter. */
     if (dfb_surface_ref( region->surface )) {
          dfb_layer_region_unlock( region );
          return DFB_FUSION;
     }

     /* Return the surface. */
     *ret_surface = region->surface;

     /* Unlock the region. */
     dfb_layer_region_unlock( region );

     return DFB_OK;
}

DFBResult
dfb_layer_region_flip_update( CoreLayerRegion     *region,
                              const DFBRegion     *update,
                              DFBSurfaceFlipFlags  flags )
{
     DFBResult                ret = DFB_OK;
     DFBRegion                unrotated;
     DFBRegion                rotated;
     CoreLayer               *layer;
     CoreSurface             *surface;
     const DisplayLayerFuncs *funcs;

     if (update)
          D_DEBUG_AT( Core_LayerRegion, "%s( %p, %p, 0x%08x ) <- [%4d,%4d-%4dx%4d]\n", __FUNCTION__,
                      region, update, flags, DFB_RECTANGLE_VALS_FROM_REGION( update ) );
     else
          D_DEBUG_AT( Core_LayerRegion, "%s( %p, %p, 0x%08x )\n", __FUNCTION__, region, update, flags );


     D_ASSERT( region != NULL );

     /* Lock the region. */
     if (dfb_layer_region_lock( region ))
          return DFB_FUSION;

     /* Check for stereo region */
     if (region->config.options & DLOP_STEREO) {
          ret = dfb_layer_region_flip_update_stereo( region, update, update, flags );
          dfb_layer_region_unlock( region );
          return ret;
     }

     D_ASSUME( region->surface != NULL );

     /* Check for NULL surface. */
     if (!region->surface) {
          D_DEBUG_AT( Core_LayerRegion, "  -> no surface => no update!\n" );
          dfb_layer_region_unlock( region );
          return DFB_UNSUPPORTED;
     }

     surface = region->surface;

     layer = dfb_layer_at( region->layer_id );

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->funcs != NULL );

     funcs = layer->funcs;

     /* Unfreeze region. */
     if (D_FLAGS_IS_SET( region->state, CLRSF_FROZEN )) {
          D_FLAGS_CLEAR( region->state, CLRSF_FROZEN );

          if (D_FLAGS_IS_SET( region->state, CLRSF_REALIZED )) {
               ret = dfb_layer_region_set( region, &region->config, CLRCF_ALL, surface );
               if (ret)
                    D_DERROR( ret, "Core/LayerRegion: "
                                   "dfb_layer_region_set() in dfb_layer_region_flip_update() failed!\n" );
          }
          else if (D_FLAGS_ARE_SET( region->state, CLRSF_ENABLED | CLRSF_ACTIVE )) {
               ret = dfb_layer_region_realize( region, true );
               if (ret)
                    D_DERROR( ret, "Core/LayerRegion: "
                                   "dfb_layer_region_realize() in dfb_layer_region_flip_update() failed!\n" );
          }

          if (ret) {
               dfb_layer_region_unlock( region );
               return ret;
          }
     }

     dfb_gfxcard_flush();

     dfb_surface_lock( surface );

     if (!(surface->frametime_config.flags & DFTCF_INTERVAL))
          dfb_screen_get_frame_interval( layer->screen, &surface->frametime_config.interval );

     if (flags & DSFLIP_UPDATE)
          goto update_only;

     /* Depending on the buffer mode. */
     switch (region->config.buffermode) {
          case DLBM_TRIPLE:
          case DLBM_BACKVIDEO:
               /* Check if simply swapping the buffers is possible. */
               if ((flags & DSFLIP_SWAP) ||
                   (!(flags & DSFLIP_BLIT) && !surface->rotation &&
                    (!update || (update->x1 == 0                          &&
                                 update->y1 == 0                          &&
                                 update->x2 == surface->config.size.w - 1 &&
                                 update->y2 == surface->config.size.h - 1)))) {
                    D_DEBUG_AT( Core_LayerRegion, "  -> going to swap buffers...\n" );

                    /* Use the driver's routine if the region is realized. */
                    if (D_FLAGS_IS_SET( region->state, CLRSF_REALIZED )) {
                         CoreSurfaceBufferLock left;

                         D_ASSUME( funcs->FlipRegion != NULL );

                         ret = region_buffer_lock( region, surface, DSBR_BACK, &left, NULL );
                         if (ret)
                              goto out;

                         D_DEBUG_AT( Core_LayerRegion, "  -> flipping region using driver...\n" );

                         if (funcs->FlipRegion)
                              ret = funcs->FlipRegion( layer, layer->driver_data, layer->layer_data,
                                                       region->region_data, surface, flags, update, &left, NULL, NULL );

                         if (!(dfb_system_caps() & CSCAPS_NOTIFY_DISPLAY)) {
                              D_DEBUG_AT( Core_LayerRegion, "  -> system without notify_display support, calling it now\n" );

                              dfb_surface_notify_display2( surface, left.allocation->index );
                         }

                         /* Unlock region buffer since the lock is no longer needed. */
                         region_buffer_unlock(region, &left, NULL);
                    }
                    else {
                         D_DEBUG_AT( Core_LayerRegion, "  -> flipping region not using driver...\n" );

                         /* Just do the hardware independent work. */
                         dfb_surface_flip_buffers( surface, false );
                    }
                    break;
               }
               /* fall through */

          case DLBM_BACKSYSTEM:
               D_DEBUG_AT( Core_LayerRegion, "  -> going to copy portion...\n" );

               if ((flags & DSFLIP_WAITFORSYNC) == DSFLIP_WAITFORSYNC) {
                    D_DEBUG_AT( Core_LayerRegion, "  -> waiting for VSync...\n" );

                    dfb_layer_wait_vsync( layer );
               }

               D_DEBUG_AT( Core_LayerRegion, "  -> copying content from back to front buffer...\n" );

               /* Copy updated contents from back to front buffer. */
               dfb_back_to_front_copy_stereo( surface, DSSE_LEFT, update, NULL, surface->rotation );

               if ((flags & DSFLIP_WAITFORSYNC) == DSFLIP_WAIT) {
                    D_DEBUG_AT( Core_LayerRegion, "  -> waiting for VSync...\n" );

                    dfb_layer_wait_vsync( layer );
               }
               /* fall through */

          case DLBM_FRONTONLY:
update_only:
               /* Tell the driver about the update if the region is realized. */
               if (funcs->UpdateRegion && D_FLAGS_IS_SET( region->state, CLRSF_REALIZED )) {
                    CoreSurfaceBufferLock left;

                    if (surface) {
                         /* Lock region buffer before it is used. */
                         region_buffer_lock( region, surface, DSBR_FRONT, &left, NULL );

                         D_ASSERT( left.allocation != NULL );
                    }

                    D_DEBUG_AT( Core_LayerRegion, "  -> notifying driver about updated content...\n" );

                    if (!update) {
                         unrotated = DFB_REGION_INIT_FROM_RECTANGLE_VALS( 0, 0,
                                                                          region->config.width, region->config.height );
                         update    = &unrotated;
                    }

                    dfb_region_from_rotated( &rotated, update, &surface->config.size, surface->rotation );

                    ret = funcs->UpdateRegion( layer, layer->driver_data, layer->layer_data,
                                               region->region_data, surface, &rotated, &left, NULL, NULL );

                    if (!(dfb_system_caps() & CSCAPS_NOTIFY_DISPLAY)) {
                         D_DEBUG_AT( Core_LayerRegion, "  -> system without notify_display support, calling it now\n" );

                         dfb_surface_notify_display2( surface, left.allocation->index );
                    }

                    /* Unlock region buffer since the lock is no longer needed. */
                    if (surface)
                         region_buffer_unlock(region, &left, NULL);
               }
               break;

          default:
               D_BUG( "unknown buffer mode" );
               ret = DFB_BUG;
     }

     D_DEBUG_AT( Core_LayerRegion, "  -> done\n" );

out:
     dfb_surface_unlock( surface );

     /* Unlock the region. */
     dfb_layer_region_unlock( region );

     return ret;
}

DFBResult
dfb_layer_region_flip_update_stereo( CoreLayerRegion     *region,
                                     const DFBRegion     *left_update,
                                     const DFBRegion     *right_update,
                                     DFBSurfaceFlipFlags  flags )
{
     DFBResult                ret = DFB_OK;
     DFBRegion                unrotated;
     DFBRegion                left_rotated, right_rotated;
     CoreLayer               *layer;
     CoreSurface             *surface;
     const DisplayLayerFuncs *funcs;
     DFBSurfaceStereoEye      eyes = 0;

     D_DEBUG_AT( Core_LayerRegion, "%s( %p, %p, %p, 0x%08x )\n", __FUNCTION__, region, left_update, right_update, flags );
     if (left_update)
          D_DEBUG_AT( Core_LayerRegion, "Left: [%4d,%4d-%4dx%4d]\n", DFB_RECTANGLE_VALS_FROM_REGION( left_update ) );
     if (right_update)
          D_DEBUG_AT( Core_LayerRegion, "Right: [%4d,%4d-%4dx%4d]\n", DFB_RECTANGLE_VALS_FROM_REGION( right_update ) );

     D_ASSERT( region != NULL );

     /* Lock the region. */
     if (dfb_layer_region_lock( region ))
          return DFB_FUSION;

     /* Check for stereo region */
     if (!(region->config.options & DLOP_STEREO)) {
          D_DEBUG_AT( Core_LayerRegion, "  -> not a stereo region!\n" );
          dfb_layer_region_unlock( region );
          return DFB_UNSUPPORTED;
     }

     D_ASSUME( region->surface != NULL );

     /* Check for NULL surface. */
     if (!region->surface) {
          D_DEBUG_AT( Core_LayerRegion, "  -> no surface => no update!\n" );
          dfb_layer_region_unlock( region );
          return DFB_UNSUPPORTED;
     }

     surface = region->surface;
     layer   = dfb_layer_at( region->layer_id );

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->funcs != NULL );

     funcs = layer->funcs;

     /* Unfreeze region. */
     if (D_FLAGS_IS_SET( region->state, CLRSF_FROZEN )) {
          D_FLAGS_CLEAR( region->state, CLRSF_FROZEN );

          if (D_FLAGS_IS_SET( region->state, CLRSF_REALIZED )) {
               ret = dfb_layer_region_set( region, &region->config, CLRCF_ALL, surface );
               if (ret)
                    D_DERROR( ret, "Core/LayerRegion: "
                                   "dfb_layer_region_set() in dfb_layer_region_flip_update() failed!\n" );
          }
          else if (D_FLAGS_ARE_SET( region->state, CLRSF_ENABLED | CLRSF_ACTIVE )) {
               ret = dfb_layer_region_realize( region, true );
               if (ret)
                    D_DERROR( ret, "Core/LayerRegion: "
                                   "dfb_layer_region_realize() in dfb_layer_region_flip_update() failed!\n" );
          }

          if (ret) {
               dfb_layer_region_unlock( region );
               return ret;
          }
     }

     dfb_gfxcard_flush();

     dfb_surface_lock( surface );

     if (!(surface->frametime_config.flags & DFTCF_INTERVAL))
          dfb_screen_get_frame_interval( layer->screen, &surface->frametime_config.interval );

     if (flags & DSFLIP_UPDATE)
          goto update_only;

     /* Depending on the buffer mode. */
     switch (region->config.buffermode) {
          case DLBM_TRIPLE:
          case DLBM_BACKVIDEO:
               /* Check if simply swapping the buffers is possible. */
               if ((flags & DSFLIP_SWAP) ||
                   (!(flags & DSFLIP_BLIT) && !surface->rotation &&
                    ((!left_update && !right_update) ||
                     ((left_update->x1 == 0                          &&
                       left_update->y1 == 0                          &&
                       left_update->x2 == surface->config.size.w - 1 &&
                       left_update->y2 == surface->config.size.h - 1) &&
                      (right_update->x1 == 0                          &&
                       right_update->y1 == 0                          &&
                       right_update->x2 == surface->config.size.w - 1 &&
                       right_update->y2 == surface->config.size.h - 1))))) {
                    D_DEBUG_AT( Core_LayerRegion, "  -> going to swap buffers...\n" );

                    /* Use the driver's routine if the region is realized. */
                    if (D_FLAGS_IS_SET( region->state, CLRSF_REALIZED )) {
                         CoreSurfaceBufferLock left, right;

                         D_ASSUME( funcs->FlipRegion != NULL );

                         ret = region_buffer_lock( region, surface, DSBR_BACK, &left, &right );
                         if (ret)
                              goto out;

                         D_DEBUG_AT( Core_LayerRegion, "  -> flipping region using driver...\n" );

                         if (funcs->FlipRegion)
                              ret = funcs->FlipRegion( layer, layer->driver_data, layer->layer_data,
                                                       region->region_data, surface, flags, left_update, &left,
                                                       right_update, &right );

                         /* Unlock region buffer since the lock is no longer needed. */
                         region_buffer_unlock( region, &left, &right );
                    }
                    else {
                         D_DEBUG_AT( Core_LayerRegion, "  -> flipping region not using driver...\n" );

                         /* Just do the hardware independent work. */
                         dfb_surface_flip_buffers( surface, false );
                    }
                    break;
               }
               /* fall through */

          case DLBM_BACKSYSTEM:
               D_DEBUG_AT( Core_LayerRegion, "  -> going to copy portion...\n" );

               if ((flags & DSFLIP_WAITFORSYNC) == DSFLIP_WAITFORSYNC) {
                    D_DEBUG_AT( Core_LayerRegion, "  -> waiting for VSync...\n" );

                    dfb_layer_wait_vsync( layer );
               }

               D_DEBUG_AT( Core_LayerRegion, "  -> copying content from back to front buffer...\n" );

               if (left_update)
                    eyes |= DSSE_LEFT;

               if (right_update)
                    eyes |= DSSE_RIGHT;

               /* Copy updated contents from back to front buffer. */
               dfb_back_to_front_copy_stereo( surface, eyes, left_update, right_update, surface->rotation );

               if ((flags & DSFLIP_WAITFORSYNC) == DSFLIP_WAIT) {
                    D_DEBUG_AT( Core_LayerRegion, "  -> waiting for VSync...\n" );

                    dfb_layer_wait_vsync( layer );
               }
               /* fall through */

          case DLBM_FRONTONLY:
update_only:
               /* Tell the driver about the update if the region is realized. */
               if (funcs->UpdateRegion && D_FLAGS_IS_SET( region->state, CLRSF_REALIZED )) {
                    CoreSurfaceBufferLock left, right;

                    if (surface) {
                         /* Lock region buffer before it is used. */
                         region_buffer_lock( region, surface, DSBR_FRONT, &left, &right );

                         D_ASSERT( left.allocation != NULL );
                         D_ASSERT( right.allocation != NULL );
                    }

                    D_DEBUG_AT( Core_LayerRegion, "  -> notifying driver about updated content...\n" );

                    if (!left_update && !right_update) {

                         unrotated = DFB_REGION_INIT_FROM_RECTANGLE_VALS( 0, 0,
                                                                          region->config.width, region->config.height );
                         left_update = &unrotated;

                         unrotated = DFB_REGION_INIT_FROM_RECTANGLE_VALS( 0, 0,
                                                                          region->config.width, region->config.height );
                         right_update = &unrotated;
                    }
                    else if (!left_update) {
                         left_update  = right_update;
                    }
                    else if (!right_update) {
                         right_update = left_update;
                    }

                    dfb_region_from_rotated( &left_rotated, left_update, &surface->config.size, surface->rotation );

                    if (left_update != right_update)
                         dfb_region_from_rotated( &right_rotated, right_update, &surface->config.size,
                                                  surface->rotation );
                    else
                         right_rotated = left_rotated;

                    ret = funcs->UpdateRegion( layer, layer->driver_data, layer->layer_data,
                                               region->region_data, surface, &left_rotated, &left,
                                               &right_rotated, &right );

                    /* Unlock region buffer since the lock is no longer needed. */
                    if (surface)
                         region_buffer_unlock( region, &left, &right );
               }
               break;

          default:
               D_BUG( "unknown buffer mode" );
               ret = DFB_BUG;
     }

     D_DEBUG_AT( Core_LayerRegion, "  -> done\n" );

out:
     dfb_surface_unlock( surface );

     /* Unlock the region. */
     dfb_layer_region_unlock( region );

     return ret;
}

DFBResult
dfb_layer_region_flip_update2( CoreLayerRegion     *region,
                               const DFBRegion     *left_update,
                               const DFBRegion     *right_update,
                               DFBSurfaceFlipFlags  flags,
                               unsigned int         flip_count,
                               long long            pts )
{
     if (region->config.options & DLOP_STEREO)
          return dfb_layer_region_flip_update_stereo( region, left_update, right_update, flags );

     return dfb_layer_region_flip_update( region, left_update, flags );
}

DFBResult
dfb_layer_region_set_configuration( CoreLayerRegion             *region,
                                    const CoreLayerRegionConfig *config,
                                    CoreLayerRegionConfigFlags   flags )
{
     DFBResult                   ret;
     CoreLayer                  *layer;
     const DisplayLayerFuncs    *funcs;
     CoreLayerRegionConfig       new_config;
     CoreLayerRegionConfigFlags  failed;

     D_DEBUG_AT( Core_LayerRegion, "%s( %p, %p, 0x%08x )\n", __FUNCTION__, region, config, flags );

     D_ASSERT( region != NULL );
     D_ASSERT( config != NULL );
     D_ASSERT( config->buffermode != DLBM_WINDOWS );
     D_ASSERT( (flags == CLRCF_ALL) || (region->state & CLRSF_CONFIGURED) );

     D_ASSUME( flags != CLRCF_NONE );
     D_ASSUME( !(flags & ~CLRCF_ALL) );

     layer = dfb_layer_at( region->layer_id );

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->funcs != NULL );
     D_ASSERT( layer->funcs->TestRegion != NULL );

     funcs = layer->funcs;

     /* Lock the region. */
     if (dfb_layer_region_lock( region ))
          return DFB_FUSION;

     /* Full configuration supplied. */
     if (flags == CLRCF_ALL) {
          new_config = *config;
     }
     else {
          /* Use the current configuration. */
          new_config = region->config;

          /* Update each modified entry. */
          if (flags & CLRCF_WIDTH)
               new_config.width = config->width;

          if (flags & CLRCF_HEIGHT)
               new_config.height = config->height;

          if (flags & CLRCF_FORMAT)
               new_config.format = config->format;

          if (flags & CLRCF_COLORSPACE)
               new_config.colorspace = config->colorspace;

          if (flags & CLRCF_SURFACE_CAPS)
               new_config.surface_caps = config->surface_caps;

          if (flags & CLRCF_BUFFERMODE)
               new_config.buffermode = config->buffermode;

          if (flags & CLRCF_OPTIONS)
               new_config.options = config->options;

          if (flags & CLRCF_SOURCE_ID)
               new_config.source_id = config->source_id;

          if (flags & CLRCF_SOURCE)
               new_config.source = config->source;

          if (flags & CLRCF_DEST)
               new_config.dest = config->dest;

          if (flags & CLRCF_OPACITY)
               new_config.opacity = config->opacity;

          if (flags & CLRCF_ALPHA_RAMP) {
               new_config.alpha_ramp[0] = config->alpha_ramp[0];
               new_config.alpha_ramp[1] = config->alpha_ramp[1];
               new_config.alpha_ramp[2] = config->alpha_ramp[2];
               new_config.alpha_ramp[3] = config->alpha_ramp[3];
          }

          if (flags & CLRCF_SRCKEY)
               new_config.src_key = config->src_key;

          if (flags & CLRCF_DSTKEY)
               new_config.dst_key = config->dst_key;

          if (flags & CLRCF_PARITY)
               new_config.parity = config->parity;

          if (flags & CLRCF_CLIPS) {
               new_config.clips     = config->clips;
               new_config.num_clips = config->num_clips;
               new_config.positive  = config->positive;
          }
     }

     DFB_CORE_LAYER_REGION_CONFIG_DEBUG_AT( Core_LayerRegion, &new_config );

     /* Check if the new configuration is supported. */
     ret = funcs->TestRegion( layer, layer->driver_data, layer->layer_data, &new_config, &failed );
     if (ret) {
          D_DEBUG_AT( Core_LayerRegion, "  -> FAILED 0x%08x\n", failed );
          dfb_layer_region_unlock( region );
          return ret;
     }

     /* Check if the region should be frozen, thus requiring to apply changes explicitly. */
     if (flags & CLRCF_FREEZE) {
          D_DEBUG_AT( Core_LayerRegion, "  -> FREEZE...\n" );
          region->state |= CLRSF_FROZEN;
     }

     /* Propagate new configuration to the driver if the region is realized. */
     if (D_FLAGS_IS_SET( region->state, CLRSF_REALIZED ) && !D_FLAGS_IS_SET( region->state, CLRSF_FROZEN )) {
          ret = dfb_layer_region_set( region, &new_config, flags, region->surface );
          if (ret) {
               dfb_layer_region_unlock( region );
               return ret;
          }
     }

     /* Update the region's current configuration. */
     region->config = new_config;

     /* Update the region's state. */
     D_FLAGS_SET( region->state, CLRSF_CONFIGURED );

     /* Unlock the region. */
     dfb_layer_region_unlock( region );

     D_DEBUG_AT( Core_LayerRegion, "  -> done\n" );

     return DFB_OK;
}

DFBResult
dfb_layer_region_get_configuration( CoreLayerRegion       *region,
                                    CoreLayerRegionConfig *ret_config )
{
     D_DEBUG_AT( Core_LayerRegion, "%s( %p )\n", __FUNCTION__, region );

     D_ASSERT( region != NULL );
     D_ASSERT( ret_config != NULL );
     D_ASSERT( D_FLAGS_IS_SET( region->state, CLRSF_CONFIGURED ) );

     /* Lock the region. */
     if (dfb_layer_region_lock( region ))
          return DFB_FUSION;

     /* Return the current configuration. */
     *ret_config = region->config;

     /* Unlock the region. */
     dfb_layer_region_unlock( region );

     return DFB_OK;
}

ReactionResult
_dfb_layer_region_surface_listener( const void *msg_data,
                                    void       *ctx )
{
     CoreSurfaceNotificationFlags   flags;
     CoreSurface                   *surface;
     CoreLayer                     *layer;
     CoreLayerShared               *shared;
     const DisplayLayerFuncs       *funcs;
     const CoreSurfaceNotification *notification = msg_data;
     CoreLayerRegion               *region       = ctx;

     D_ASSERT( notification != NULL );
     D_ASSERT( notification->surface != NULL );
     D_ASSERT( region != NULL );

     D_DEBUG_AT( Core_LayerRegion, "%s( %p, %p ) <- 0x%08x\n", __FUNCTION__,
                 notification, region, notification->flags );

     D_ASSUME( notification->surface == region->surface );

     if (notification->surface != region->surface)
          return RS_OK;

     layer = dfb_layer_at( region->layer_id );

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->shared != NULL );
     D_ASSERT( layer->funcs != NULL );
     D_ASSERT( layer->funcs->SetRegion != NULL );

     funcs   = layer->funcs;
     shared  = layer->shared;

     flags   = notification->flags;
     surface = notification->surface;

     if (flags & CSNF_BUFFER_ALLOCATION_DESTROY)
          return RS_OK;

     if (flags & CSNF_DESTROY) {
          D_WARN( "layer region surface destroyed" );
          region->surface = NULL;
          return RS_REMOVE;
     }

     if (flags & CSNF_DISPLAY)
          return RS_OK;

     if (dfb_layer_region_lock( region ))
          return RS_OK;

     if (D_FLAGS_ARE_SET( region->state, CLRSF_REALIZED | CLRSF_CONFIGURED ) &&
         !D_FLAGS_IS_SET( region->state, CLRSF_FROZEN )) {
          if (D_FLAGS_IS_SET( flags, CSNF_PALETTE_CHANGE | CSNF_PALETTE_UPDATE )) {
               if (surface->palette) {
                    CoreSurfaceBufferLock left, right;

                    dfb_surface_lock( surface );

                    /* Lock region buffer before it is used. */
                    region_buffer_lock( region, surface, DSBR_BACK, &left, &right );

                    D_ASSERT( left.buffer != NULL );

                    funcs->SetRegion( layer, layer->driver_data, layer->layer_data,
                                      region->region_data, &region->config, CLRCF_PALETTE, surface, surface->palette,
                                      &left, &right );

                    /* Unlock region buffer since the lock is no longer needed. */
                    region_buffer_unlock( region, &left, &right );

                    dfb_surface_unlock( surface );
               }
          }

          if ((flags & CSNF_FIELD) && funcs->SetInputField)
               funcs->SetInputField( layer, layer->driver_data, layer->layer_data,
                                     region->region_data, surface->field );

          if ((flags & CSNF_ALPHA_RAMP) && (shared->description.caps & DLCAPS_ALPHA_RAMP)) {
               CoreSurfaceBufferLock left, right;

               region->config.alpha_ramp[0] = surface->alpha_ramp[0];
               region->config.alpha_ramp[1] = surface->alpha_ramp[1];
               region->config.alpha_ramp[2] = surface->alpha_ramp[2];
               region->config.alpha_ramp[3] = surface->alpha_ramp[3];

               dfb_surface_lock( surface );

               /* Lock region buffer before it is used. */
               region_buffer_lock( region, surface, DSBR_BACK, &left, &right );

               D_ASSERT( left.buffer != NULL );

               funcs->SetRegion( layer, layer->driver_data, layer->layer_data,
                                 region->region_data, &region->config, CLRCF_ALPHA_RAMP, surface, surface->palette,
                                 &left, &right );

               /* Unlock region buffer since the lock is no longer needed. */
               region_buffer_unlock( region, &left, &right );

               dfb_surface_unlock( surface );
          }
     }

     dfb_layer_region_unlock( region );

     return RS_OK;
}

/**********************************************************************************************************************/

static DFBResult
region_buffer_unlock( CoreLayerRegion       *region,
                      CoreSurfaceBufferLock *left_buffer_lock,
                      CoreSurfaceBufferLock *right_buffer_lock )
{
     DFBResult ret = DFB_OK;

     D_ASSERT( region != NULL );
     D_ASSERT( left_buffer_lock != NULL );

     D_DEBUG_AT( Core_LayerRegionLock, "%s( %p )\n", __FUNCTION__, region );

     if (left_buffer_lock->buffer)
          D_DEBUG_AT( Core_LayerRegionLock, "  -> lock buffer left %p\n", left_buffer_lock->buffer );
     if (right_buffer_lock && right_buffer_lock->buffer)
          D_DEBUG_AT( Core_LayerRegionLock, "  -> lock buffer right %p\n", right_buffer_lock->buffer );

     /* Unlock any previously locked buffer. */
     if (left_buffer_lock->buffer) {
          D_MAGIC_ASSERT( left_buffer_lock->buffer, CoreSurfaceBuffer );

          ret = dfb_surface_unlock_buffer( left_buffer_lock->buffer->surface, left_buffer_lock );
     }

     if (right_buffer_lock && right_buffer_lock->buffer) {
          D_MAGIC_ASSERT( right_buffer_lock->buffer, CoreSurfaceBuffer );

          ret = dfb_surface_unlock_buffer( right_buffer_lock->buffer->surface, right_buffer_lock );
     }

     return ret;
}

static DFBResult
region_buffer_lock( CoreLayerRegion       *region,
                    CoreSurface           *surface,
                    DFBSurfaceBufferRole   role,
                    CoreSurfaceBufferLock *left_buffer_lock,
                    CoreSurfaceBufferLock *right_buffer_lock )
{
     DFBResult          ret;
     CoreSurfaceBuffer *buffer;
     bool               stereo;

     D_ASSERT( region != NULL );
     D_MAGIC_ASSERT( surface, CoreSurface );
     D_ASSERT( left_buffer_lock != NULL );

     FUSION_SKIRMISH_ASSERT( &surface->lock );

     D_DEBUG_AT( Core_LayerRegionLock, "%s( %p, %p, role %u )\n", __FUNCTION__, region, surface, role );

     Core_PushIdentity( FUSION_ID_MASTER );

     /* Save current buffer focus. */
     buffer = dfb_surface_get_buffer3( surface, role, DSSE_LEFT, region->surface_flip_count );

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     D_DEBUG_AT( Core_LayerRegionLock, "  -> buffer  %p\n", buffer );

     /* Lock the surface buffer. */
     ret = dfb_surface_buffer_lock( buffer, region->surface_accessor, CSAF_READ, left_buffer_lock );
     if (ret) {
          D_DERROR( ret, "Core/LayerRegion: Could not lock region surface!\n" );
          Core_PopIdentity();
          return ret;
     }

     D_ASSERT( left_buffer_lock->allocation != NULL );

     stereo = surface->config.caps & DSCAPS_STEREO;

     if (stereo) {
          D_ASSERT( right_buffer_lock != NULL );

          buffer = dfb_surface_get_buffer3( surface, role, DSSE_RIGHT, region->surface_flip_count );

          D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

          D_DEBUG_AT( Core_LayerRegionLock, "  -> buffer  %p\n", buffer );

          /* Lock the surface buffer. */
          ret = dfb_surface_buffer_lock( buffer, region->surface_accessor, CSAF_READ, right_buffer_lock );
          if (ret) {
               D_DERROR( ret, "Core/LayerRegion: Could not lock region surface!\n" );
               Core_PopIdentity();
               return ret;
          }

          D_ASSERT( right_buffer_lock->allocation != NULL );
     }
     else if (right_buffer_lock) {
          /* Clear for region_buffer_unlock(). */
          right_buffer_lock->buffer = NULL;
     }

     Core_PopIdentity();

     return DFB_OK;
}

DFBResult
dfb_layer_region_set( CoreLayerRegion            *region,
                      CoreLayerRegionConfig      *config,
                      CoreLayerRegionConfigFlags  flags,
                      CoreSurface                *surface )
{
     DFBResult                ret;
     CoreLayer               *layer;
     CoreLayerShared         *shared;
     const DisplayLayerFuncs *funcs;
     CoreSurfaceBufferLock    left, right;
     bool                     locked = false;

     D_UNUSED_P( shared );

     D_DEBUG_AT( Core_LayerRegion, "%s( %p, %p, 0x%08x, %p )\n", __FUNCTION__, region, config, flags, surface );

     D_ASSERT( region != NULL );
     D_ASSERT( config != NULL );
     D_ASSERT( config->buffermode != DLBM_WINDOWS );
     D_ASSERT( D_FLAGS_IS_SET( region->state, CLRSF_REALIZED ) );

     DFB_CORE_LAYER_REGION_CONFIG_DEBUG_AT( Core_LayerRegion, config );

     D_DEBUG_AT( Core_LayerRegion, "  -> state 0x%08x\n", region->state );

     layer = dfb_layer_at( region->layer_id );

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->shared != NULL );
     D_ASSERT( layer->funcs != NULL );
     D_ASSERT( layer->funcs->SetRegion != NULL );

     if (region->state & CLRSF_FROZEN) {
          D_DEBUG_AT( Core_LayerRegion, "  -> FROZEN!\n" );
          return DFB_OK;
     }

     shared = layer->shared;
     funcs  = layer->funcs;

     if (surface) {
          if (flags & (CLRCF_SURFACE | CLRCF_WIDTH  | CLRCF_HEIGHT | CLRCF_FORMAT | CLRCF_SRCKEY | CLRCF_DSTKEY |
                       CLRCF_OPACITY | CLRCF_SOURCE | CLRCF_DEST)) {
               dfb_surface_lock( surface );

               ret = region_buffer_lock( region, surface, DSBR_FRONT, &left, &right );

               dfb_surface_unlock( surface );

               if (ret)
                    return ret;

               locked = true;
          }
     }

     D_DEBUG_AT( Core_LayerRegion, "  -> setting region of '%s'\n", shared->description.name );

     /* Setup hardware. */
     ret =  funcs->SetRegion( layer, layer->driver_data, layer->layer_data,
                              region->region_data, config, flags, surface, surface ? surface->palette : NULL,
                              &left, &right );
     if (ret)
          D_DERROR( ret, "Core/LayerRegion: Could not set region!\n" );

     /* Unlock the region buffer since the lock is no longer necessary. */
     if (locked)
          region_buffer_unlock( region, &left, &right );

     return ret;
}

DFBResult
dfb_layer_region_realize( CoreLayerRegion *region,
                          bool             set )
{
     DFBResult                ret;
     CoreLayer               *layer;
     CoreLayerShared         *shared;
     const DisplayLayerFuncs *funcs;

     D_DEBUG_AT( Core_LayerRegion, "%s( %p )\n", __FUNCTION__, region );

     D_ASSERT( region != NULL );

     DFB_CORE_LAYER_REGION_CONFIG_DEBUG_AT( Core_LayerRegion, &region->config );

     D_DEBUG_AT( Core_LayerRegion, "  -> state 0x%08x\n", region->state );

     if (region->state & CLRSF_FROZEN) {
          D_DEBUG_AT( Core_LayerRegion, "  -> FROZEN!\n" );
          return DFB_OK;
     }

     D_ASSERT( D_FLAGS_IS_SET( region->state, CLRSF_CONFIGURED ) );
     D_ASSERT( !D_FLAGS_IS_SET( region->state, CLRSF_REALIZED ) );

     layer = dfb_layer_at( region->layer_id );

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->shared != NULL );
     D_ASSERT( layer->funcs != NULL );

     shared = layer->shared;
     funcs  = layer->funcs;

     D_ASSERT( !fusion_vector_contains( &shared->added_regions, region ) );

     /* Allocate the driver's region data. */
     if (funcs->RegionDataSize) {
          int size = funcs->RegionDataSize();

          if (size > 0) {
               region->region_data = SHCALLOC( shared->shmpool, 1, size );
               if (!region->region_data)
                    return D_OOSHM();
          }
     }

     D_DEBUG_AT( Core_LayerRegion, "  -> adding region to '%s'\n", shared->description.name );

     /* Add the region to the driver. */
     if (funcs->AddRegion) {
          ret = funcs->AddRegion( layer, layer->driver_data, layer->layer_data, region->region_data, &region->config );
          if (ret) {
               D_DERROR( ret, "Core/LayerRegion: Could not add region!\n" );

               if (region->region_data) {
                    SHFREE( shared->shmpool, region->region_data );
                    region->region_data = NULL;
               }

               return ret;
          }
     }

     /* Add the region to the 'added' list. */
     fusion_vector_add( &shared->added_regions, region );

     /* Update the region's state. */
     D_FLAGS_SET( region->state, CLRSF_REALIZED );

     /* Initially setup hardware. */
     if (set) {
          ret = dfb_layer_region_set( region, &region->config, CLRCF_ALL, region->surface );
          if (ret) {
               dfb_layer_region_unrealize( region );
               return ret;
          }
     }

     return DFB_OK;
}

DFBResult
dfb_layer_region_unrealize( CoreLayerRegion *region )
{
     DFBResult                ret;
     int                      index;
     CoreLayer               *layer;
     CoreLayerShared         *shared;
     const DisplayLayerFuncs *funcs;

     D_DEBUG_AT( Core_LayerRegion, "%s( %p )\n", __FUNCTION__, region );

     D_ASSERT( region != NULL );
     D_ASSERT( D_FLAGS_IS_SET( region->state, CLRSF_REALIZED ) );

     DFB_CORE_LAYER_REGION_CONFIG_DEBUG_AT( Core_LayerRegion, &region->config );

     D_DEBUG_AT( Core_LayerRegion, "  -> state 0x%08x\n", region->state );

     layer = dfb_layer_at( region->layer_id );

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->shared != NULL );
     D_ASSERT( layer->funcs != NULL );

     shared = layer->shared;
     funcs  = layer->funcs;

     D_ASSERT( fusion_vector_contains( &shared->added_regions, region ) );

     index = fusion_vector_index_of( &shared->added_regions, region );

     D_DEBUG_AT( Core_LayerRegion, "  -> removing region from '%s'\n", shared->description.name );

     /* Remove the region from hardware and driver. */
     if (funcs->RemoveRegion) {
          ret = funcs->RemoveRegion( layer, layer->driver_data, layer->layer_data, region->region_data );
          if (ret) {
               D_DERROR( ret, "Core/LayerRegion: Could not remove region!\n" );
               return ret;
          }
     }

     /* Remove the region from the 'added' list. */
     fusion_vector_remove( &shared->added_regions, index );

     /* Deallocate the driver's region data. */
     if (region->region_data) {
          SHFREE( shared->shmpool, region->region_data );
          region->region_data = NULL;
     }

     /* Update the region's state. */
     D_FLAGS_CLEAR( region->state, CLRSF_REALIZED );
     D_FLAGS_SET( region->state, CLRSF_FROZEN );

     /* Unlock the region buffer if it is locked. */
     if (region->surface && !region->config.keep_buffers)
          dfb_surface_deallocate_buffers( region->surface );

     return DFB_OK;
}

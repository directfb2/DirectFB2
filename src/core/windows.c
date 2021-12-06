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

#include <core/CoreSurface.h>
#include <core/CoreSurfaceClient.h>
#include <core/CoreWindow.h>
#include <core/core.h>
#include <core/layers.h>
#include <core/layer_context.h>
#include <core/surface_client.h>
#include <core/windows.h>
#include <core/windowstack.h>
#include <core/wm.h>
#include <fusion/shmalloc.h>

D_DEBUG_DOMAIN( Core_Windows,        "Core/Windows",        "DirectFB Core Windows" );
D_DEBUG_DOMAIN( Core_Windows_Events, "Core/Windows/Events", "DirectFB Core Windows Events" );

/**********************************************************************************************************************/

typedef struct {
     DirectLink  link;

     CoreWindow *window;
     int         x;
     int         y;
} BoundWindow;

static const ReactionFunc dfb_window_globals[] = {
     NULL
};

/**********************************************************************************************************************/

static void
window_destructor( FusionObject *object,
                   bool          zombie,
                   void         *ctx )
{
     CoreWindow      *window = (CoreWindow*) object;
     CoreWindowStack *stack  = window->stack;

     D_ASSUME( window->stack != NULL );

     D_DEBUG_AT( Core_Windows, "Destroying window %p (%4d,%4d-%4dx%4d%s)\n",
                 window, DFB_RECTANGLE_VALS( &window->config.bounds ), zombie ? " ZOMBIE" : "" );

     if (!stack) {
          /* Destroy the object. */
          fusion_object_destroy( object );
          return;
     }

     CoreWindow_Deinit_Dispatch( &window->call );

     dfb_windowstack_lock( stack );

     dfb_window_destroy( window );

     if (window->cursor.surface)
          dfb_surface_unlink( &window->cursor.surface );

     if (window->caps & DWCAPS_SUBWINDOW) {
          int         index;
          CoreWindow *toplevel;

          toplevel = window->toplevel;
          D_ASSERT( toplevel != NULL );

          index = fusion_vector_index_of( &toplevel->subwindows, window );
          D_ASSERT( index >= 0 );

          fusion_vector_remove( &toplevel->subwindows, index );

          window->toplevel = NULL;
     }
     else {
          CoreWindow *sub;
          int         i;

          fusion_vector_foreach (sub, i, window->subwindows) {
               sub->toplevel = NULL;
          }

          fusion_vector_destroy( &window->subwindows );
     }

     dfb_windowstack_unlock( stack );

     /* Unlink the primary region of the context. */
     if (window->primary_region)
          dfb_layer_region_unlink( &window->primary_region );

     D_MAGIC_CLEAR( window );

     /* Destroy the object. */
     fusion_object_destroy( object );
}

FusionObjectPool *
dfb_window_pool_create( const FusionWorld *world )
{
     FusionObjectPool *pool;

     pool = fusion_object_pool_create( "Window Pool",
                                       sizeof(CoreWindow), sizeof(DFBWindowEvent),
                                       window_destructor, NULL, world );

     return pool;
}

/**********************************************************************************************************************/

DFBResult
dfb_window_create_region( CoreWindow              *window,
                          CoreLayerContext        *context,
                          CoreSurface             *window_surface,
                          DFBSurfacePixelFormat    format,
                          DFBSurfaceColorSpace     colorspace,
                          DFBSurfaceCapabilities   surface_caps,
                          CoreLayerRegion        **ret_region,
                          CoreSurface            **ret_surface )
{
     DFBResult              ret;
     CoreLayerRegionConfig  config;
     CoreLayerRegion       *region;
     CoreSurface           *surface = window_surface;
     CoreSurfaceConfig      scon;

     D_ASSERT( window != NULL );
     D_ASSERT( context != NULL );
     D_ASSERT( ret_region != NULL );
     D_ASSERT( ret_surface != NULL );

     memset( &config, 0, sizeof(CoreLayerRegionConfig) );

     config.width         = window->config.bounds.w;
     config.height        = window->config.bounds.h;
     config.format        = format;
     config.colorspace    = colorspace;
     config.options       = context->config.options & DLOP_FLICKER_FILTERING;
     config.source        = (DFBRectangle) { 0, 0, config.width, config.height };
     config.dest          = window->config.bounds;
     config.opacity       = 0;
     config.alpha_ramp[0] = 0x00;
     config.alpha_ramp[1] = 0x55;
     config.alpha_ramp[2] = 0xaa;
     config.alpha_ramp[3] = 0xff;

     if (surface_caps & DSCAPS_DOUBLE)
          config.buffermode = DLBM_BACKVIDEO;
     else if (surface_caps & DSCAPS_TRIPLE)
          config.buffermode = DLBM_TRIPLE;
     else
          config.buffermode = DLBM_FRONTONLY;

     if (((context->config.options & DLOP_ALPHACHANNEL) || (window->config.options & DWOP_ALPHACHANNEL)) &&
         DFB_PIXELFORMAT_HAS_ALPHA( format ))
          config.options |= DLOP_ALPHACHANNEL;

     config.options |= DLOP_OPACITY;

     config.surface_caps = surface_caps & (DSCAPS_INTERLACED | DSCAPS_SEPARATED  | DSCAPS_PREMULTIPLIED);

     ret = dfb_layer_region_create( context, &region );
     if (ret)
          return ret;

     region->config.keep_buffers = true;

     do {
          ret = dfb_layer_region_set_configuration( region, &config, CLRCF_ALL );
          if (ret) {
               if (config.options & DLOP_OPACITY)
                    config.options &= ~DLOP_OPACITY;
               else if (config.options & DLOP_ALPHACHANNEL)
                    config.options = (config.options & ~DLOP_ALPHACHANNEL) | DLOP_OPACITY;
               else {
                    D_DERROR( ret, "Core/Windows: Unable to set region configuration!\n" );
                    dfb_layer_region_unref( region );
                    return ret;
               }
          }
     } while (ret);

     if (!surface) {
          scon.flags      = CSCONF_SIZE | CSCONF_FORMAT | CSCONF_COLORSPACE | CSCONF_CAPS;
          scon.size.w     = config.width;
          scon.size.h     = config.height;
          scon.format     = format;
          scon.colorspace = colorspace;
          scon.caps       = surface_caps | DSCAPS_VIDEOONLY;

          ret = dfb_surface_create( core_dfb, &scon, CSTF_SHARED | CSTF_LAYER, context->layer_id, NULL, &surface );
          if (ret) {
               dfb_layer_region_unref( region );
               return ret;
          }
     }

     ret = dfb_layer_region_set_surface( region, surface, false );
     if (ret) {
          dfb_surface_unref( surface );
          dfb_layer_region_unref( region );
          return ret;
     }

     if (!window_surface) {
          ret = dfb_layer_region_enable( region );
          if (ret) {
               dfb_surface_unref( surface );
               dfb_layer_region_unref( region );
               return ret;
          }
     }

     *ret_region  = region;
     *ret_surface = surface;

     return DFB_OK;
}

static DFBResult
init_subwindow( CoreWindow      *window,
                CoreWindowStack *stack,
                DFBWindowID      toplevel_id )
{
     DFBResult   ret;
     CoreWindow *toplevel;

     /* Lookup top level window. */
     ret = dfb_wm_window_lookup( stack, toplevel_id, &toplevel );
     if (ret)
          return ret;

     /* Make sure chosen top level window is not a sub window. */
     if (toplevel->caps & DWCAPS_SUBWINDOW) {
          D_ASSERT( toplevel->toplevel != NULL );
          D_ASSERT( toplevel->toplevel_id != 0 );

          return DFB_INVARG;
     }
     else {
          D_ASSERT( toplevel->toplevel == NULL );
          D_ASSERT( toplevel->toplevel_id == 0 );
     }

     /* Link top level window into sub window structure. */
     window->toplevel = toplevel;
     if (ret)
          return ret;

     /* Add window to sub window list of top level window. */
     ret = fusion_vector_add( &toplevel->subwindows, window );
     if (ret) {
          dfb_window_unlink( &window->toplevel );
          return ret;
     }

     return DFB_OK;
}

static ReactionResult
window_surface_react( const void *msg_data,
                      void       *ctx )
{
     const DFBSurfaceEvent *evt    = msg_data;
     CoreWindow            *window = ctx;

     D_DEBUG_AT( Core_Windows, "%s( %p ) <- type %06x\n", __FUNCTION__, evt, evt->type );
     D_DEBUG_AT( Core_Windows, "  -> surface id %u\n", evt->surface_id );

     if (evt->type == DSEVT_UPDATE) {
          D_DEBUG_AT( Core_Windows, "  -> updated %4d,%4d-%4dx%4d (left)\n",
                      DFB_RECTANGLE_VALS_FROM_REGION( &evt->update ) );
          D_DEBUG_AT( Core_Windows, "  -> updated %4d,%4d-%4dx%4d (right)\n",
                      DFB_RECTANGLE_VALS_FROM_REGION( &evt->update_right ) );
          D_DEBUG_AT( Core_Windows, "  -> flip count %u\n", evt->flip_count );
          D_DEBUG_AT( Core_Windows, "  -> time stamp %lld\n", evt->time_stamp );
          D_DEBUG_AT( Core_Windows, "  -> window %p\n", window );

          if (direct_log_domain_check( &Core_Windows )) {
               dfb_surface_lock( window->surface );
               CoreSurfaceBuffer *buffer;
               D_UNUSED_P( buffer );
               buffer = dfb_surface_get_buffer3( window->surface, DSBR_FRONT, DSSE_LEFT, evt->flip_count );
               D_DEBUG_AT( Core_Windows, "  -> buffer %p\n", buffer );
               dfb_surface_unlock( window->surface );
          }

          /* Lock the window stack. */
          if (dfb_windowstack_lock( window->stack ))
               return RS_OK;

          /* Never call WM after destroying the window. */
          if (DFB_WINDOW_DESTROYED( window )) {
               dfb_windowstack_unlock( window->stack );
               return RS_OK;
          }

          window->surface_flip_count = evt->flip_count;

          if (!dfb_config->single_window || fusion_vector_size( &window->stack->visible_windows ) != 1) {
               D_DEBUG_AT( Core_Windows, "  -> dispatching update to window manager\n" );

               dfb_wm_update_window( window, &evt->update, &evt->update_right, DSFLIP_NONE );
          }

          CoreSurfaceClient_FrameAck( window->surface_client, evt->flip_count );

          /* Unlock the window stack. */
          dfb_windowstack_unlock( window->stack );
     }
     else if (evt->type == DSEVT_DESTROYED)
          return RS_REMOVE;

     return RS_OK;
}

DFBResult
dfb_window_create( CoreWindowStack             *stack,
                   const DFBWindowDescription  *desc,
                   CoreWindow                 **ret_window )
{
     DFBResult               ret;
     CoreSurface            *surface;
     DFBWindowSurfacePolicy  policy = DWSP_SYSTEMONLY;
     CoreLayer              *layer;
     CoreLayerContext       *context;
     CoreWindow             *window;
     CardCapabilities        card_caps;
     CoreWindowConfig        config;
     DFBWindowCapabilities   caps;
     DFBSurfaceCapabilities  surface_caps;
     DFBSurfacePixelFormat   pixelformat;
     DFBSurfaceColorSpace    colorspace;
     DFBWindowID             toplevel_id;

     D_ASSERT( stack != NULL );
     D_ASSERT( stack->context != NULL );
     D_ASSERT( desc != NULL );
     D_ASSERT( desc->width > 0 );
     D_ASSERT( desc->height > 0 );
     D_ASSERT( ret_window != NULL );

     D_DEBUG_AT( Core_Windows, "%s( %p )\n", __FUNCTION__, stack );

     if (desc->width > 4096 || desc->height > 4096) {
          D_DEBUG_AT( Core_Windows, "  -> limit exceeded (%dx%d / 4096x4096)\n", desc->width, desc->height );
          return DFB_LIMITEXCEEDED;
     }

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack )) {
          D_DEBUG_AT( Core_Windows, "  -> window stack lock failed!\n" );
          return DFB_FUSION;
     }

     context = stack->context;
     layer   = dfb_layer_at( context->layer_id );

     D_DEBUG_AT( Core_Windows, "  -> caps 0x%08x\n", desc->caps );

     caps         = desc->caps;
     pixelformat  = desc->pixelformat;
     colorspace   = desc->colorspace;
     surface_caps = desc->surface_caps & (DSCAPS_INTERLACED   | DSCAPS_SEPARATED     | DSCAPS_DOUBLE |
                                          DSCAPS_STATIC_ALLOC | DSCAPS_PREMULTIPLIED | DSCAPS_TRIPLE |
                                          DSCAPS_SYSTEMONLY   | DSCAPS_VIDEOONLY     | DSCAPS_DEPTH  | DSCAPS_GL );
     toplevel_id  = (desc->flags & DWDESC_TOPLEVEL_ID) ? desc->toplevel_id : 0;

     if (toplevel_id != 0)
          caps |= DWCAPS_SUBWINDOW;
     else
          caps &= ~DWCAPS_SUBWINDOW;

     if (caps & DWCAPS_STEREO)
          surface_caps |= DSCAPS_STEREO;

     if (!dfb_config->translucent_windows) {
          caps &= ~DWCAPS_ALPHACHANNEL;
     }

     /* Choose pixel format. */
     if (caps & DWCAPS_ALPHACHANNEL) {
          if (pixelformat == DSPF_UNKNOWN) {
               if (context->config.flags & DLCONF_PIXELFORMAT)
                    pixelformat = context->config.pixelformat;

               if (!DFB_PIXELFORMAT_HAS_ALPHA( pixelformat ))
                    pixelformat = DSPF_ARGB;
          }
          else if (!DFB_PIXELFORMAT_HAS_ALPHA( pixelformat )) {
               D_DEBUG_AT( Core_Windows, "  -> pixelformat '%s' has no alpha!\n", dfb_pixelformat_name( pixelformat ) );
               dfb_windowstack_unlock( stack );
               return DFB_INVARG;
          }
     }
     else if (pixelformat == DSPF_UNKNOWN) {
          if (context->config.flags & DLCONF_PIXELFORMAT)
               pixelformat = context->config.pixelformat;
          else {
               D_WARN( "layer config has no pixel format, using RGB16" );
               pixelformat = DSPF_RGB16;
          }
     }

     /* Set the color space. */
     if (colorspace == DSCS_UNKNOWN) {
          colorspace = DFB_COLORSPACE_DEFAULT( pixelformat );
     }
     else if (!DFB_COLORSPACE_IS_COMPATIBLE( colorspace, pixelformat )) {
          D_DEBUG_AT( Core_Windows, "  -> colorspace '%s' is not compatible with pixelformat '%s'!\n",
                      dfb_colorspace_name(colorspace), dfb_pixelformat_name( pixelformat ) );
          dfb_windowstack_unlock( stack );
          return DFB_INVARG;
     }

     /* Choose window surface policy. */
     if ((surface_caps & DSCAPS_VIDEOONLY) || (context->config.buffermode == DLBM_WINDOWS)) {
          policy = DWSP_VIDEOONLY;
     }
     else if (!(surface_caps & DSCAPS_SYSTEMONLY) && context->config.buffermode != DLBM_BACKSYSTEM) {
          /* Use the explicitly specified policy. */
          if (dfb_config->window_policy != -1) {
               policy = dfb_config->window_policy;
          }
          else {
               /* Examine the hardware capabilities. */
               dfb_gfxcard_get_capabilities( &card_caps );

               if (card_caps.accel & DFXL_BLIT) {
                    if ((card_caps.blitting & DSBLIT_BLEND_ALPHACHANNEL) || !(caps & DWCAPS_ALPHACHANNEL))
                         policy = DWSP_VIDEOHIGH;
               }
          }
     }

     dfb_surface_caps_apply_policy( policy, &surface_caps );

     if (caps & DWCAPS_DOUBLEBUFFER) {
          if (!(surface_caps & DSCAPS_TRIPLE))
               surface_caps |= DSCAPS_DOUBLE;
     }

     memset( &config, 0, sizeof(CoreWindowConfig) );

     config.bounds.x = desc->posx;
     config.bounds.y = desc->posy;
     config.bounds.w = desc->width;
     config.bounds.h = desc->height;
     config.stacking = (desc->flags & DWDESC_STACKING) ? desc->stacking : DWSC_MIDDLE;
     config.events   = DWET_ALL & ~DWET_UPDATE;

     /* Auto enable blending for ARGB only, not indexed. */
     if ((caps & DWCAPS_ALPHACHANNEL) &&
          DFB_PIXELFORMAT_HAS_ALPHA( pixelformat ) &&
         !DFB_PIXELFORMAT_IS_INDEXED( pixelformat ))
          config.options |= DWOP_ALPHACHANNEL;

     /* Override automatic settings. */
     if (desc->flags & DWDESC_OPTIONS)
          config.options = desc->options;

     /* Create the window object. */
     window = dfb_core_create_window( layer->core );
     if (!window) {
          D_DEBUG_AT( Core_Windows, "  -> core window create failed!\n" );
          dfb_windowstack_unlock( stack );
          return DFB_FUSION;
     }

     window->id                  = ++stack->id_pool;
     window->caps                = caps | DWCAPS_NOFOCUS;
     window->requested_caps      = caps;
     window->stack               = stack;
     window->config              = config;
     window->config.association  = (desc->flags & DWDESC_PARENT) ? desc->parent_id : 0;
     window->config.cursor_flags = DWCF_NONE;
     window->toplevel_id         = toplevel_id;
     window->policy              = policy;

     if (desc->flags & DWDESC_RESOURCE_ID)
          window->resource_id = desc->resource_id;

     D_MAGIC_SET( window, CoreWindow );

     ret = dfb_wm_preconfigure_window( stack, window );
     if (ret) {
          D_DEBUG_AT( Core_Windows, "  -> window preconfigure failed!\n" );
          D_MAGIC_CLEAR( window );
          fusion_object_destroy( &window->object );
          dfb_windowstack_unlock( stack );
          return ret;
     }

     /* Window manager may have changed values. */
     config = window->config;
     caps   = window->caps;

     /* Initialize sub window. */
     if (caps & DWCAPS_SUBWINDOW) {
          ret = init_subwindow( window, stack, toplevel_id );
          if (ret) {
               D_DEBUG_AT( Core_Windows, "  -> sub window init failed!\n" );
               D_MAGIC_CLEAR( window );
               fusion_object_destroy( &window->object );
               dfb_windowstack_unlock( stack );
               return ret;
          }
     }
     else {
          /* Initialize top level window. */
          fusion_vector_init( &window->subwindows, 3, stack->shmpool );

          /* In case window manager forbids sub window request, clear the toplevel window ID. */
          window->toplevel_id = 0;
     }

     if (dfb_config->warn.flags & DCWF_CREATE_WINDOW)
          D_WARN( "create-window %4dx%4d %6s, caps 0x%08x, surface-caps 0x%08x, ID %u",
                  window->config.bounds.w, window->config.bounds.h, dfb_pixelformat_name( pixelformat ), window->caps,
                  surface_caps, window->id );

     /* Create the window's surface using the layer's palette if possible. */
     if (!(caps & (DWCAPS_INPUTONLY | DWCAPS_COLOR))) {
          if (context->config.buffermode == DLBM_WINDOWS) {
               CoreLayerRegion *region = NULL;

               /* Create a region for the window. */
               ret = dfb_window_create_region( window, context, NULL, pixelformat, colorspace, surface_caps,
                                               &region, &surface );
               if (ret) {
                    D_DEBUG_AT( Core_Windows, "  -> region create failed!\n" );
                    D_MAGIC_CLEAR( window );
                    fusion_object_destroy( &window->object );
                    dfb_windowstack_unlock( stack );
                    return ret;
               }

               /* Link the region into the window structure. */
               dfb_layer_region_link( &window->region, region );
               dfb_layer_region_unref( region );

               /* Link the surface into the window structure. */
               dfb_surface_link( &window->surface, surface );
               dfb_surface_unref( surface );
          }
          else {
               CoreLayerRegion *region;

               /* Get the primary region of the layer context. */
               ret = dfb_layer_context_get_primary_region( context, true, &region );
               if (ret) {
                    D_DEBUG_AT( Core_Windows, "  -> primary region get failed!\n" );
                    D_MAGIC_CLEAR( window );
                    fusion_object_destroy( &window->object );
                    dfb_windowstack_unlock( stack );
                    return ret;
               }

               /* Link the primary region into the window structure. */
               dfb_layer_region_link( &window->primary_region, region );
               dfb_layer_region_unref( region );

               D_DEBUG_AT( Core_Windows, "  -> %dx%d %s %s\n", window->config.bounds.w, window->config.bounds.h,
                           dfb_pixelformat_name( pixelformat ), (policy == DWSP_VIDEOONLY)  ?  "VIDEOONLY" :
                           (policy == DWSP_SYSTEMONLY) ? "SYSTEMONLY" : "AUTO VIDEO" );

               /* Give the window manager a chance to provide its own surface. */
               if (!window->surface) {
                    /* Create the surface for the window. */
                    ret = dfb_surface_create_simple( layer->core, config.bounds.w, config.bounds.h, pixelformat,
                                                     colorspace, surface_caps, CSTF_SHARED | CSTF_WINDOW,
                                                     desc->flags & DWDESC_RESOURCE_ID ? desc->resource_id : window->id,
                                                     region->surface ? region->surface->palette : NULL, &surface );
                    if (ret) {
                         D_DEBUG_AT( Core_Windows, "  -> surface create failed!\n" );
                         D_DERROR( ret, "Core/Windows: Failed to create window surface!\n" );
                         D_MAGIC_CLEAR( window );
                         dfb_layer_region_unlink( &window->primary_region );
                         fusion_object_destroy( &window->object );
                         dfb_windowstack_unlock( stack );
                         return ret;
                    }

                    /* Link the surface into the window structure. */
                    dfb_surface_link( &window->surface, surface );
                    dfb_surface_unref( surface );
               }

               /* Create the surface client. */
               ret = CoreSurface_CreateClient( window->surface, &window->surface_client );
               if (ret) {
                    D_WARN( "failed to create surface client" );
                    return ret;
               }

               /* Attach the surface event listener. */
               dfb_surface_attach_channel( window->surface, CSCH_EVENT, window_surface_react, window,
                                           &window->surface_event_reaction );
          }
     }
     else
          D_DEBUG_AT( Core_Windows, "  -> %dx%d => INPUTONLY\n", window->config.bounds.w, window->config.bounds.h );

     D_DEBUG_AT( Core_Windows, "  -> %p\n", window );

     /* Pass the new window to the window manager. */
     ret = dfb_wm_add_window( stack, window );
     if (ret) {
          D_DEBUG_AT( Core_Windows, "  -> window add failed!\n" );
          D_DERROR( ret, "Core/Windows: Failed to add window to manager!\n" );

          D_MAGIC_CLEAR( window );

          if (window->surface)
               dfb_surface_unlink( &window->surface );

          if (window->primary_region)
               dfb_layer_region_unlink( &window->primary_region );

          if (window->region)
               dfb_layer_region_unlink( &window->region );

          fusion_object_destroy( &window->object );
          dfb_windowstack_unlock( stack );
          return ret;
     }

     /* Indicate that initialization is complete. */
     D_FLAGS_SET( window->flags, CWF_INITIALIZED );

     CoreWindow_Init_Dispatch( layer->core, window, &window->call );

     /* Activate the object. */
     fusion_object_activate( &window->object );

     fusion_reactor_direct( window->object.reactor, true );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     /* Return the new window. */
     *ret_window = window;

     D_DEBUG_AT( Core_Windows, "  -> %p\n", window );

     return DFB_OK;;
}

void
dfb_window_destroy( CoreWindow *window )
{
     int              i;
     DFBWindowEvent   evt;
     CoreWindowStack *stack;
     BoundWindow     *bound, *next;
     CoreWindow      *subwindow;

     D_MAGIC_ASSERT( window, CoreWindow );
     D_ASSERT( DFB_WINDOW_INITIALIZED( window ) );

     D_ASSUME( window->stack != NULL );

     D_DEBUG_AT( Core_Windows, "%s( %p ) <- %4d,%4d-%4dx%4d\n", __FUNCTION__,
                 window, DFB_RECTANGLE_VALS( &window->config.bounds ) );

     stack = window->stack;
     if (!stack)
          return;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return;

     /* Destroy sub windows first. */
     fusion_vector_foreach_reverse (subwindow, i, window->subwindows) {
          D_ASSERT( subwindow != NULL );
          D_ASSERT( DFB_WINDOW_INITIALIZED( subwindow ) );

          dfb_window_destroy( subwindow );
     }

     /* Avoid multiple destructions. */
     if (DFB_WINDOW_DESTROYED( window )) {
          D_DEBUG_AT( Core_Windows, "  -> %p already destroyed\n", window );
          dfb_windowstack_unlock( stack );
          return;
     }

     /* Unbind bound windows. */
     direct_list_foreach_safe (bound, next, window->bound_windows) {
          direct_list_remove( &window->bound_windows, &bound->link );

          bound->window->boundto = NULL;

          SHFREE( stack->shmpool, bound );
     }

     /* Unbind this window. */
     if (window->boundto)
          dfb_window_unbind( window->boundto, window );

     /* Make sure the window is no longer visible. */
     dfb_window_set_opacity( window, 0 );

     /* Stop managing the window. */
     dfb_wm_remove_window( stack, window );

     /* Indicate destruction. */
     D_FLAGS_SET( window->flags, CWF_DESTROYED );

     /* Hardware allocated. */
     if (window->region) {
          /* Disable region (removing it from hardware). */
          dfb_layer_region_disable( window->region );

          /* Unlink from structure. */
          dfb_layer_region_unlink( &window->region );
     }

     /* Unlink the window's surface. */
     if (window->surface) {
          dfb_surface_client_unref( window->surface_client );

          /* Detach the surface event listener. */
          dfb_surface_detach( window->surface, &window->surface_event_reaction );

          dfb_surface_destroy_buffers( window->surface );
          dfb_surface_unlink( &window->surface );
     }

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     /* Notify listeners. */
     evt.type = DWET_DESTROYED;
     dfb_window_post_event( window, &evt );
}

DFBResult
dfb_window_change_stacking( CoreWindow             *window,
                            DFBWindowStackingClass  stacking )
{
     DFBResult         ret;
     CoreWindowStack  *stack;
     CoreLayerContext *context;
     CoreWindowConfig  config;

     D_MAGIC_ASSERT( window, CoreWindow );
     D_ASSERT( window->stack != NULL );
     D_ASSERT( window->stack->context != NULL );

     stack   = window->stack;
     context = stack->context;

     if (!(dfb_config->layers[context->layer_id].stacking & (1 << stacking))) {
          D_ERROR( "Core/Windows: Cannot change the stacking class!\n" );
          return DFB_INVARG;
     }

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     config.stacking = stacking;

     /* Let the window manager do its work. */
     ret = dfb_wm_set_window_config( window, &config, DWCONF_STACKING );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_set_type_hint( CoreWindow        *window,
                          DFBWindowTypeHint  type_hint )
{
     DFBResult         ret;
     CoreWindowStack  *stack;
     CoreWindowConfig  config;

     D_MAGIC_ASSERT( window, CoreWindow );
     D_ASSERT( window->stack != NULL );

     stack = window->stack;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     config.type_hint = type_hint;

     /* Let the window manager do its work. */
     ret = dfb_wm_set_window_config( window, &config, DWCONF_TYPE_HINT );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_change_hint_flags( CoreWindow         *window,
                              DFBWindowHintFlags  clear,
                              DFBWindowHintFlags  set )
{
     DFBResult         ret;
     CoreWindowStack  *stack;
     CoreWindowConfig  config;

     D_MAGIC_ASSERT( window, CoreWindow );
     D_ASSERT( window->stack != NULL );

     stack = window->stack;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     config.hint_flags = (window->config.hint_flags & ~clear) | set;

     /* Let the window manager do its work. */
     ret = dfb_wm_set_window_config( window, &config, DWCONF_HINT_FLAGS );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_raise( CoreWindow *window )
{
     DFBResult        ret;
     CoreWindowStack *stack;

     D_MAGIC_ASSERT( window, CoreWindow );
     D_ASSERT( window->stack != NULL );

     stack = window->stack;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     /* Let the window manager do its work. */
     ret = dfb_wm_restack_window( window, window, 1 );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_lower( CoreWindow *window )
{
     DFBResult        ret;
     CoreWindowStack *stack;

     D_MAGIC_ASSERT( window, CoreWindow );
     D_ASSERT( window->stack != NULL );

     stack = window->stack;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     /* Let the window manager do its work. */
     ret = dfb_wm_restack_window( window, window, -1 );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_raisetotop( CoreWindow *window )
{
     DFBResult        ret;
     CoreWindowStack *stack;

     D_MAGIC_ASSERT( window, CoreWindow );
     D_ASSERT( window->stack != NULL );

     stack = window->stack;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     /* Let the window manager do its work. */
     ret = dfb_wm_restack_window( window, NULL, 1 );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_lowertobottom( CoreWindow *window )
{
     DFBResult        ret;
     CoreWindowStack *stack;

     D_MAGIC_ASSERT( window, CoreWindow );
     D_ASSERT( window->stack != NULL );

     stack = window->stack;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     /* Let the window manager do its work. */
     ret = dfb_wm_restack_window( window, NULL, 0 );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_putatop( CoreWindow *window,
                    CoreWindow *lower )
{
     DFBResult        ret;
     CoreWindowStack *stack;

     D_MAGIC_ASSERT( window, CoreWindow );
     D_ASSERT( window->stack != NULL );

     stack = window->stack;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     /* Let the window manager do its work. */
     ret = dfb_wm_restack_window( window, lower, 1 );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_putbelow( CoreWindow *window,
                     CoreWindow *upper )
{
     DFBResult        ret;
     CoreWindowStack *stack;

     D_MAGIC_ASSERT( window, CoreWindow );
     D_ASSERT( window->stack != NULL );

     stack = window->stack;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     /* Let the window manager do its work. */
     ret = dfb_wm_restack_window( window, upper, -1 );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_set_config( CoreWindow             *window,
                       const CoreWindowConfig *config,
                       DFBWindowConfigFlags    flags )
{
     DFBResult        ret;
     CoreWindowStack *stack = window->stack;

     D_MAGIC_ASSERT( window, CoreWindow );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     ret = dfb_wm_set_window_config( window, config, flags );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_set_cursor_shape( CoreWindow   *window,
                             CoreSurface  *surface,
                             unsigned int  hot_x,
                             unsigned int  hot_y )
{
     DFBResult        ret   = DFB_OK;
     CoreWindowStack *stack = window->stack;

     D_MAGIC_ASSERT( window, CoreWindow );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     window->cursor.hot_x = hot_x;
     window->cursor.hot_y = hot_y;

     if (window->cursor.surface)
          dfb_surface_unlink( &window->cursor.surface );

     if (surface) {
          ret = dfb_surface_link( &window->cursor.surface, surface );
          if (ret == DFB_OK) {
               if (window->flags & CWF_FOCUSED)
                    dfb_windowstack_cursor_set_shape( stack, surface, hot_x, hot_y );
          }
     }

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

static DFBResult
move_window( CoreWindow *window,
             int         x,
             int         y )
{
     DFBResult         ret;
     CoreWindowConfig  config;
     BoundWindow      *bound;

     D_MAGIC_ASSERT( window, CoreWindow );

     config.bounds.x = x;
     config.bounds.y = y;

     ret = dfb_wm_set_window_config( window, &config, DWCONF_POSITION );
     if (ret)
          return ret;

     direct_list_foreach (bound, window->bound_windows) {
          move_window( bound->window, window->config.bounds.x + bound->x, window->config.bounds.y + bound->y );
     }

     return DFB_OK;
}

DFBResult
dfb_window_move( CoreWindow *window,
                 int         x,
                 int         y,
                 bool        relative )
{
     DFBResult        ret;
     CoreWindowStack *stack = window->stack;

     D_MAGIC_ASSERT( window, CoreWindow );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     if (window->boundto) {
          dfb_windowstack_unlock( stack );
          return DFB_UNSUPPORTED;
     }

     if (relative) {
          x += window->config.bounds.x;
          y += window->config.bounds.y;
     }

     if (x == window->config.bounds.x && y == window->config.bounds.y) {
          dfb_windowstack_unlock( stack );
          return DFB_OK;
     }

     ret = move_window( window, x, y );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_set_bounds( CoreWindow *window,
                       int         x,
                       int         y,
                       int         width,
                       int         height )
{
     DFBResult         ret;
     CoreWindowConfig  config;
     CoreWindowStack  *stack = window->stack;
     int               old_x;
     int               old_y;

     D_MAGIC_ASSERT( window, CoreWindow );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     old_x = window->config.bounds.x;
     old_y = window->config.bounds.y;

     if (window->boundto) {
          if (old_x != x || old_y != y) {
               dfb_windowstack_unlock( stack );
               return DFB_UNSUPPORTED;
          }
     }

     config.bounds.x = x;
     config.bounds.y = y;
     config.bounds.w = width;
     config.bounds.h = height;

     if (window->config.bounds.x == x     && window->config.bounds.y == y &&
         window->config.bounds.w == width && window->config.bounds.h == height) {
          dfb_windowstack_unlock( stack );
          return DFB_OK;
     }

     ret = dfb_wm_set_window_config( window, &config, DWCONF_POSITION | DWCONF_SIZE );
     if (ret) {
          dfb_windowstack_unlock( stack );
          return ret;
     }

     if (old_x != x || old_y != y) {
          BoundWindow *bound;

          direct_list_foreach (bound, window->bound_windows) {
               move_window( bound->window, window->config.bounds.x + bound->x, window->config.bounds.y + bound->y );
          }
     }

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

DFBResult
dfb_window_resize( CoreWindow *window,
                   int         width,
                   int         height )
{
     DFBResult         ret;
     CoreWindowConfig  config;
     CoreWindowStack  *stack = window->stack;

     D_MAGIC_ASSERT( window, CoreWindow );
     D_ASSERT( width > 0 );
     D_ASSERT( height > 0 );

     if (width > 4096 || height > 4096)
          return DFB_LIMITEXCEEDED;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     if (window->config.bounds.w == width && window->config.bounds.h == height) {
          dfb_windowstack_unlock( stack );
          return DFB_OK;
     }

     config.bounds.w = width;
     config.bounds.h = height;

     ret = dfb_wm_set_window_config( window, &config, DWCONF_SIZE );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_bind( CoreWindow *window,
                 CoreWindow *source,
                 int         x,
                 int         y )
{
     DFBResult        ret;
     CoreWindowStack *stack = window->stack;
     BoundWindow     *bound;

     D_MAGIC_ASSERT( window, CoreWindow );

     if (window == source)
          return DFB_UNSUPPORTED;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     if (DFB_WINDOW_DESTROYED( source )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     bound = SHCALLOC( stack->shmpool, 1, sizeof(BoundWindow) );
     if (!bound) {
          dfb_windowstack_unlock( stack );
          return DFB_NOSHAREDMEMORY;
     }

     if (source->boundto)
          dfb_window_unbind( source->boundto, source );

     ret = move_window( source, window->config.bounds.x + x, window->config.bounds.y + y );
     if (ret) {
          SHFREE( stack->shmpool, bound );
          dfb_windowstack_unlock( stack );
          return ret;
     }

     bound->window = source;
     bound->x      = x;
     bound->y      = y;

     direct_list_append( &window->bound_windows, &bound->link );

     source->boundto = window;

     dfb_windowstack_unlock( stack );

     return DFB_OK;
}

DFBResult
dfb_window_unbind( CoreWindow *window,
                   CoreWindow *source )
{
     CoreWindowStack *stack = window->stack;
     BoundWindow     *bound;

     D_MAGIC_ASSERT( window, CoreWindow );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     if (DFB_WINDOW_DESTROYED( source )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     if (source->boundto != window) {
          dfb_windowstack_unlock( stack );
          return DFB_UNSUPPORTED;
     }

     direct_list_foreach (bound, window->bound_windows) {
          if (bound->window == source) {
               direct_list_remove( &window->bound_windows, &bound->link );

               bound->window->boundto = NULL;

               SHFREE( stack->shmpool, bound );

               break;
          }
     }

     if (!bound)
          D_BUG( "window not found" );

     dfb_windowstack_unlock( stack );

     return bound ? DFB_OK : DFB_ITEMNOTFOUND;
}

DFBResult
dfb_window_set_color( CoreWindow *window,
                      DFBColor    color )
{
     DFBResult         ret;
     DFBColor          cc;
     CoreWindowConfig  config;
     CoreWindowStack  *stack = window->stack;

     D_MAGIC_ASSERT( window, CoreWindow );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     cc = window->config.color;
     if ((cc.a == color.a) && (cc.r == color.r) && (cc.g == color.g) && (cc.b == color.b)) {
          dfb_windowstack_unlock( stack );
          return DFB_OK;
     }

     config.color = color;

     ret = dfb_wm_set_window_config( window, &config, DWCONF_COLOR );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_set_colorkey( CoreWindow *window,
                         u32         color_key )
{
     DFBResult         ret;
     CoreWindowConfig  config;
     CoreWindowStack  *stack = window->stack;

     D_MAGIC_ASSERT( window, CoreWindow );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     if (window->config.color_key == color_key) {
          dfb_windowstack_unlock( stack );
          return DFB_OK;
     }

     config.color_key = color_key;

     ret = dfb_wm_set_window_config( window, &config, DWCONF_COLOR_KEY );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_set_opacity( CoreWindow *window,
                        u8          opacity )
{
     DFBResult         ret;
     CoreWindowConfig  config;
     CoreWindowStack  *stack = window->stack;

     D_MAGIC_ASSERT( window, CoreWindow );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     if (window->config.opacity == opacity) {
          dfb_windowstack_unlock( stack );
          return DFB_OK;
     }

     config.opacity = opacity;

     ret = dfb_wm_set_window_config( window, &config, DWCONF_OPACITY );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_change_options( CoreWindow       *window,
                           DFBWindowOptions  disable,
                           DFBWindowOptions  enable )
{
     DFBResult         ret;
     CoreWindowConfig  config;
     CoreWindowStack  *stack = window->stack;

     D_MAGIC_ASSERT( window, CoreWindow );

     D_ASSUME( disable | enable );

     if (!disable && !enable)
          return DFB_OK;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     config.options = (window->config.options & ~disable) | enable;

     ret = dfb_wm_set_window_config( window, &config, DWCONF_OPTIONS );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_set_opaque( CoreWindow      *window,
                       const DFBRegion *region )
{
     DFBResult         ret;
     CoreWindowConfig  config;
     CoreWindowStack  *stack = window->stack;

     D_MAGIC_ASSERT( window, CoreWindow );
     DFB_REGION_ASSERT_IF( region );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     config.opaque.x1 = 0;
     config.opaque.y1 = 0;
     config.opaque.x2 = window->config.bounds.w - 1;
     config.opaque.y2 = window->config.bounds.h - 1;

     if (region && !dfb_region_region_intersect( &config.opaque, region ))
          ret = DFB_INVAREA;
     else
          ret = dfb_wm_set_window_config( window, &config, DWCONF_OPAQUE );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_change_events( CoreWindow         *window,
                          DFBWindowEventType  disable,
                          DFBWindowEventType  enable )
{
     DFBResult         ret;
     CoreWindowConfig  config;
     CoreWindowStack  *stack = window->stack;

     D_MAGIC_ASSERT( window, CoreWindow );

     D_ASSUME( disable | enable );

     if (!disable && !enable)
          return DFB_OK;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     config.events = (window->config.events & ~disable) | enable;

     ret = dfb_wm_set_window_config( window, &config, DWCONF_EVENTS );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_set_key_selection( CoreWindow                    *window,
                              DFBWindowKeySelection          selection,
                              const DFBInputDeviceKeySymbol *keys,
                              unsigned int                   num_keys )
{
     DFBResult         ret;
     CoreWindowConfig  config;
     CoreWindowStack  *stack = window->stack;

     D_MAGIC_ASSERT( window, CoreWindow );
     D_ASSERT( selection == DWKS_ALL || selection == DWKS_NONE || selection == DWKS_LIST );
     D_ASSERT( keys != NULL || selection != DWKS_LIST );
     D_ASSERT( num_keys > 0 || selection != DWKS_LIST );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     config.key_selection = selection;
     config.keys          = (DFBInputDeviceKeySymbol*) keys;
     config.num_keys      = num_keys;

     ret = dfb_wm_set_window_config( window, &config, DWCONF_KEY_SELECTION );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_change_grab( CoreWindow       *window,
                        CoreWMGrabTarget  target,
                        bool              grab )
{
     DFBResult        ret;
     CoreWMGrab       wmgrab;
     CoreWindowStack *stack;

     D_MAGIC_ASSERT( window, CoreWindow );

     stack = window->stack;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     wmgrab.target = target;

     if (grab)
         ret = dfb_wm_grab( window, &wmgrab );
     else
         ret = dfb_wm_ungrab( window, &wmgrab );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_grab_key( CoreWindow                 *window,
                     DFBInputDeviceKeySymbol     symbol,
                     DFBInputDeviceModifierMask  modifiers )
{
     DFBResult        ret;
     CoreWMGrab       grab;
     CoreWindowStack *stack = window->stack;

     D_MAGIC_ASSERT( window, CoreWindow );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     grab.target    = CWMGT_KEY;
     grab.symbol    = symbol;
     grab.modifiers = modifiers;

     ret = dfb_wm_grab( window, &grab );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_ungrab_key( CoreWindow                 *window,
                       DFBInputDeviceKeySymbol     symbol,
                       DFBInputDeviceModifierMask  modifiers )
{
     DFBResult        ret;
     CoreWMGrab       grab;
     CoreWindowStack *stack = window->stack;

     D_MAGIC_ASSERT( window, CoreWindow );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     grab.target    = CWMGT_KEY;
     grab.symbol    = symbol;
     grab.modifiers = modifiers;

     ret = dfb_wm_ungrab( window, &grab );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

static bool
core_window_filter( CoreWindow           *window,
                    const DFBWindowEvent *event )
{
     D_MAGIC_ASSERT( window, CoreWindow );

     switch (event->type) {
          case DWET_GOTFOCUS:
               D_FLAGS_SET( window->flags, CWF_FOCUSED );
               break;

          case DWET_LOSTFOCUS:
               D_FLAGS_CLEAR( window->flags, CWF_FOCUSED );
               break;

          case DWET_ENTER:
               D_FLAGS_SET( window->flags, CWF_ENTERED );
               break;

          case DWET_LEAVE:
               D_FLAGS_CLEAR( window->flags, CWF_ENTERED );
               break;

          default:
               break;
     }

     return false;
}

void
dfb_window_post_event( CoreWindow     *window,
                       DFBWindowEvent *event )
{
     D_MAGIC_ASSERT( window, CoreWindow );
     D_ASSERT( event != NULL );

     D_ASSUME( !DFB_WINDOW_DESTROYED( window ) || event->type == DWET_DESTROYED );

     switch (event->type) {
          case DWET_BUTTONDOWN:
          case DWET_BUTTONUP:
               D_DEBUG_AT( Core_Windows_Events, "%s( %p [%u] )\n", __FUNCTION__, window, window->object.id );
               D_DEBUG_AT( Core_Windows_Events, "  -> TYPE    0x%08x\n", event->type );
               D_DEBUG_AT( Core_Windows_Events, "  -> BUTTON%s\n", event->type == DWET_BUTTONDOWN ? "DOWN" : "UP" );
               D_DEBUG_AT( Core_Windows_Events, "      => button  %u\n", event->button );
               D_DEBUG_AT( Core_Windows_Events, "      => x, y    %d,%d\n", event->x, event->y );
               D_DEBUG_AT( Core_Windows_Events, "      => cx, cy  %d,%d\n", event->cx, event->cy );
               break;

          default:
               break;
     }

     if (!(event->type & window->config.events))
          return;

     long long timestamp_us = direct_clock_get_abs_micros();

     event->clazz             = DFEC_WINDOW;
     event->window_id         = window->id;
     event->timestamp.tv_sec  = timestamp_us / 1000000;
     event->timestamp.tv_usec = timestamp_us % 1000000;

     if (!core_window_filter( window, event ))
          dfb_window_dispatch( window, event, dfb_window_globals );
}

DFBResult
dfb_window_send_configuration( CoreWindow *window )
{
     DFBWindowEvent event;

     D_MAGIC_ASSERT( window, CoreWindow );

     D_ASSUME( !DFB_WINDOW_DESTROYED( window ) );

     event.type = DWET_POSITION_SIZE;
     event.x    = window->config.bounds.x;
     event.y    = window->config.bounds.y;
     event.w    = window->config.bounds.w;
     event.h    = window->config.bounds.h;

     dfb_window_post_event( window, &event );

     return DFB_OK;
}

DFBResult
dfb_window_request_focus( CoreWindow *window )
{
     DFBResult        ret;
     CoreWindowStack *stack = window->stack;

     D_MAGIC_ASSERT( window, CoreWindow );

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     ret = dfb_wm_request_focus( window );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
dfb_window_set_rotation( CoreWindow *window,
                         int         rotation )
{
     DFBResult        ret   = DFB_OK;
     CoreWindowStack *stack = window->stack;

     D_MAGIC_ASSERT( window, CoreWindow );
     D_MAGIC_ASSERT( window->stack, CoreWindowStack );

     stack = window->stack;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( window )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     /* Do nothing if the rotation didn't change. */
     if (window->config.rotation != rotation) {
          CoreWindowConfig config;

          switch (rotation) {
               case 0:
               case 90:
               case 180:
               case 270:
                    config.rotation = rotation;
                    dfb_wm_set_window_config( window, &config, DWCONF_ROTATION );
                    break;

               default:
                    ret = DFB_UNSUPPORTED;
          }
     }

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

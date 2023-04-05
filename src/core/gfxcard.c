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

#include <core/CoreGraphicsStateClient.h>
#include <core/core.h>
#include <core/core_parts.h>
#include <core/fonts.h>
#include <core/gfxcard.h>
#include <core/surface_allocation.h>
#include <core/system.h>
#include <fusion/conf.h>
#include <fusion/shmalloc.h>
#include <core/state.h>
#include <gfx/clip.h>
#include <gfx/generic/generic.h>
#include <gfx/generic/generic_blit.h>
#include <gfx/generic/generic_draw_line.h>
#include <gfx/generic/generic_fill_rectangle.h>
#include <gfx/generic/generic_stretch_blit.h>
#include <gfx/generic/generic_texture_triangles.h>
#include <gfx/util.h>

D_DEBUG_DOMAIN( Core_Graphics,    "Core/Graphics",    "DirectFB Core Graphics" );
D_DEBUG_DOMAIN( Core_GraphicsOps, "Core/GraphicsOps", "DirectFB Core Graphics Operations" );
D_DEBUG_DOMAIN( Core_GfxState,    "Core/GfxState",    "DirectFB Core Gfx State" );

DEFINE_MODULE_DIRECTORY( dfb_graphics_drivers, "gfxdrivers", DFB_GRAPHICS_DRIVER_ABI_VERSION );

/**********************************************************************************************************************/

typedef struct {
     int                      magic;

     unsigned int             videoram_length;    /* Amount of usable memory */

     char                    *module_name;

     GraphicsDriverInfo       driver_info;
     GraphicsDeviceInfo       device_info;
     void                    *device_data;

     FusionSkirmish           lock;
     GraphicsDeviceLockFlags  lock_flags;

     CardState               *state;              /* Current state of the graphics card. */
     FusionID                 holder;             /* Fusion ID of state owner. */

     FusionObjectID           last_allocation_id;
     bool                     pending_ops;

     long long                ts_start;
     long long                ts_busy;
     long long                ts_busy_sum;
} DFBGraphicsCoreShared;

typedef struct {
     int                        magic;

     CoreDFB                   *core;

     DFBGraphicsCoreShared     *shared;

     DirectModuleEntry         *module;
     const GraphicsDriverFuncs *driver_funcs;

     void                      *driver_data;
     void                      *device_data;  /* copy of shared->device_data */

     CardCapabilities           caps;         /* local caps */
     CardLimitations            limits;       /* local limits */

     GraphicsDeviceFuncs        funcs;
} DFBGraphicsCore;

DFB_CORE_PART( graphics_core, GraphicsCore );

/**********************************************************************************************************************/

static DFBGraphicsCore *card = NULL;

/*
 * Load/probe/unload a driver module one at a time until a suitable driver is found and return its symlinked functions.
 */
static void
dfb_gfxcard_find_driver( CoreDFB *core )
{
     DFBGraphicsCoreShared *shared;
     DirectLink            *link;
     FusionSHMPoolShared   *pool = dfb_core_shmpool( core );

     D_DEBUG_AT( Core_Graphics, "%s( %p )\n", __FUNCTION__, core );

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     shared = card->shared;

     link = dfb_graphics_drivers.entries;

     while (direct_list_check_link( link )) {
          DirectModuleEntry         *module = (DirectModuleEntry*) link;
          const GraphicsDriverFuncs *funcs;

          D_DEBUG_AT( Core_Graphics, "  -> module %p\n", module );

          link = link->next;

          funcs = direct_module_ref( module );
          if (!funcs) {
               D_DEBUG_AT( Core_Graphics, "  -> ref failed!\n" );
               continue;
          }

          if (!card->module) {
               D_DEBUG_AT( Core_Graphics, "  -> probing '%s'...\n", module->name );

               if (funcs->Probe()) {
                    D_DEBUG_AT( Core_Graphics, "    => SUCCESS\n" );

                    funcs->GetDriverInfo( &shared->driver_info );

                    card->module       = module;
                    card->driver_funcs = funcs;

                    shared->module_name = SHSTRDUP( pool, module->name );

                    continue;
               }
               else
                    D_DEBUG_AT( Core_Graphics, "    => FAIL\n" );
          }
          else
               D_DEBUG_AT( Core_Graphics, "  -> having driver\n" );

          direct_module_unref( module );
     }
}

static void
InitDevice_Async( void *ctx,
                  void *ctx2 )
{
     DFBResult                  ret;
     DFBGraphicsCore           *data = ctx;
     DFBGraphicsCoreShared     *shared;
     const GraphicsDriverFuncs *funcs;

     D_ASSERT( data != NULL );
     D_ASSERT( data->shared != NULL );
     D_ASSERT( data->driver_funcs != NULL );

     shared = data->shared;
     funcs  = data->driver_funcs;

     ret = funcs->InitDevice( &shared->device_info, data->driver_data, data->device_data );
     if (ret) {
          D_DERROR( ret, "Core/Graphics: Could not init device!\n" );
          return;
     }

     if (data->funcs.EngineReset)
          data->funcs.EngineReset( data->driver_data, data->device_data );

     data->caps   = shared->device_info.caps;
     data->limits = shared->device_info.limits;

     D_INFO( "DirectFB/Graphics: %s %s %d.%d (%s)\n", shared->device_info.vendor, shared->device_info.name,
             shared->driver_info.version.major, shared->driver_info.version.minor, shared->driver_info.vendor );
}

static DFBResult
dfb_graphics_core_initialize( CoreDFB               *core,
                              DFBGraphicsCore       *data,
                              DFBGraphicsCoreShared *shared )
{
     DFBResult            ret;
     int                  videoram_length;
     FusionSHMPoolShared *pool = dfb_core_shmpool( core );

     D_DEBUG_AT( Core_Graphics, "%s( %p, %p, %p )\n", __FUNCTION__, core, data, shared );

     D_ASSERT( data != NULL );
     D_ASSERT( shared != NULL );

     card = data;

     data->core   = core;
     data->shared = shared;

     /* Fill generic driver info. */
     gGetDriverInfo( &shared->driver_info );

     /* Fill generic device info. */
     gGetDeviceInfo( &shared->device_info );

     if (!shared->device_info.limits.dst_max.w)
          shared->device_info.limits.dst_max.w = INT_MAX;

     if (!shared->device_info.limits.dst_max.h)
          shared->device_info.limits.dst_max.h = INT_MAX;

     /* Limit video ram length. */
     videoram_length = dfb_system_videoram_length();
     if (videoram_length) {
          if (dfb_config->videoram_limit > 0 && dfb_config->videoram_limit < videoram_length)
               shared->videoram_length = dfb_config->videoram_limit;
          else
               shared->videoram_length = videoram_length;
     }

     /* Build a list of available drivers. */
     direct_modules_explore_directory( &dfb_graphics_drivers );

     /* Load driver. */
     if (dfb_system_caps() & CSCAPS_ACCELERATION)
          dfb_gfxcard_find_driver( core );

     if (data->driver_funcs) {
          const GraphicsDriverFuncs *funcs = data->driver_funcs;

          data->driver_data = D_CALLOC( 1, shared->driver_info.driver_data_size );

          data->device_data = shared->device_data = SHCALLOC( pool, 1, shared->driver_info.device_data_size );

          ret = funcs->InitDriver( &data->funcs, data->driver_data, data->device_data, core );
          if (ret) {
               SHFREE( pool, shared->device_data );
               SHFREE( pool, shared->module_name );
               D_FREE( data->driver_data );
               card = NULL;
               return ret;
          }

          if (dfb_config->call_nodirect)
               Core_AsyncCall( InitDevice_Async, data, NULL );
          else
               InitDevice_Async( data, NULL );
     }
     else
          D_INFO( "DirectFB/Graphics: %s %s %d.%d (%s)\n", shared->device_info.vendor, shared->device_info.name,
                  shared->driver_info.version.major, shared->driver_info.version.minor, shared->driver_info.vendor );

     if (dfb_config->software_only) {
          if (data->funcs.CheckState) {
               data->funcs.CheckState = NULL;

               D_INFO( "DirectFB/Graphics: Acceleration disabled (by 'no-hardware')\n" );
          }
     }
     else {
          data->caps   = shared->device_info.caps;
          data->limits = shared->device_info.limits;
     }

     fusion_skirmish_init2( &shared->lock, "GfxCard", dfb_core_world( core ), fusion_config->secure_fusion );

     D_MAGIC_SET( data, DFBGraphicsCore );
     D_MAGIC_SET( shared, DFBGraphicsCoreShared );

     return DFB_OK;
}

/*
 * Load the driver module used by the session.
 */
static void
dfb_gfxcard_load_driver( void )
{
     DFBGraphicsCoreShared *shared;
     DirectLink            *link;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     shared = card->shared;

     if (!shared->module_name)
          return;

     link = dfb_graphics_drivers.entries;

     while (direct_list_check_link( link )) {
          DirectModuleEntry         *module = (DirectModuleEntry*) link;
          const GraphicsDriverFuncs *funcs;

          link = link->next;

          funcs = direct_module_ref( module );
          if (!funcs)
               continue;

          if (!card->module && !strcmp( module->name, shared->module_name )) {
               card->module       = module;
               card->driver_funcs = funcs;
          }
          else
               direct_module_unref( module );
     }
}

static DFBResult
dfb_graphics_core_join( CoreDFB               *core,
                        DFBGraphicsCore       *data,
                        DFBGraphicsCoreShared *shared )
{
     DFBResult          ret;
     GraphicsDriverInfo driver_info;

     D_DEBUG_AT( Core_Graphics, "%s( %p, %p, %p )\n", __FUNCTION__, core, data, shared );

     D_ASSERT( data != NULL );
     D_MAGIC_ASSERT( shared, DFBGraphicsCoreShared );

     card = data;

     data->core   = core;
     data->shared = shared;

     /* Initialize software rasterizer. */
     gGetDriverInfo( &driver_info );

     /* Build a list of available drivers. */
     direct_modules_explore_directory( &dfb_graphics_drivers );

     /* Load driver. */
     if (dfb_system_caps() & CSCAPS_ACCELERATION)
          dfb_gfxcard_load_driver();

     if (data->driver_funcs) {
          const GraphicsDriverFuncs *funcs = data->driver_funcs;

          data->driver_data = D_CALLOC( 1, shared->driver_info.driver_data_size );

          data->device_data = shared->device_data;

          ret = funcs->InitDriver( &data->funcs, data->driver_data, data->device_data, core );
          if (ret) {
               D_FREE( data->driver_data );
               return ret;
          }
     }
     else if (shared->module_name) {
          D_ERROR( "Core/Graphics: Could not load driver used by the running session!\n" );
          return DFB_UNSUPPORTED;
     }

     D_INFO( "DirectFB/Graphics: %s %s %d.%d (%s)\n", shared->device_info.vendor, shared->device_info.name,
             shared->driver_info.version.major, shared->driver_info.version.minor, shared->driver_info.vendor );

     if (dfb_config->software_only) {
          if (data->funcs.CheckState) {
               data->funcs.CheckState = NULL;
               D_INFO( "DirectFB/Graphics: Acceleration disabled (by 'no-hardware')\n" );
          }
     }
     else {
          data->caps   = shared->device_info.caps;
          data->limits = shared->device_info.limits;
     }

     D_MAGIC_SET( data, DFBGraphicsCore );

     return DFB_OK;
}

static DFBResult
dfb_graphics_core_shutdown( DFBGraphicsCore *data,
                            bool             emergency )
{
     DFBGraphicsCoreShared *shared;
     FusionSHMPoolShared   *pool;

     D_DEBUG_AT( Core_Graphics, "%s( %p, %semergency )\n", __FUNCTION__, data, emergency ? "" : "no " );

     D_MAGIC_ASSERT( data, DFBGraphicsCore );
     D_MAGIC_ASSERT( data->shared, DFBGraphicsCoreShared );

     shared = data->shared;

     pool = dfb_core_shmpool( data->core );

     dfb_gfxcard_lock( GDLF_SYNC );

     if (data->driver_funcs) {
          const GraphicsDriverFuncs *funcs = data->driver_funcs;

          funcs->CloseDevice( data->driver_data, data->device_data );
          funcs->CloseDriver( data->driver_data );

          direct_module_unref( data->module );

          SHFREE( pool, data->device_data );
          D_FREE( data->driver_data );
     }

     fusion_skirmish_destroy( &shared->lock );

     if (shared->module_name)
          SHFREE( pool, shared->module_name );

     D_MAGIC_CLEAR( data );
     D_MAGIC_CLEAR( shared );

     card = NULL;

     return DFB_OK;
}

static DFBResult
dfb_graphics_core_leave( DFBGraphicsCore *data,
                         bool             emergency )
{
     D_DEBUG_AT( Core_Graphics, "%s( %p, %semergency )\n", __FUNCTION__, data, emergency ? "" : "no " );

     D_MAGIC_ASSERT( data, DFBGraphicsCore );
     D_MAGIC_ASSERT( data->shared, DFBGraphicsCoreShared );

     if (data->driver_funcs) {
          data->driver_funcs->CloseDriver( data->driver_data );

          direct_module_unref( data->module );

          D_FREE( data->driver_data );
     }

     D_MAGIC_CLEAR( data );

     card = NULL;

     return DFB_OK;
}

static DFBResult
dfb_graphics_core_suspend( DFBGraphicsCore *data )
{
     D_DEBUG_AT( Core_Graphics, "%s( %p )\n", __FUNCTION__, data );

     D_MAGIC_ASSERT( data, DFBGraphicsCore );
     D_MAGIC_ASSERT( data->shared, DFBGraphicsCoreShared );

     dfb_gfxcard_lock( GDLF_WAIT | GDLF_SYNC | GDLF_RESET | GDLF_INVALIDATE );

     return DFB_OK;
}

static DFBResult
dfb_graphics_core_resume( DFBGraphicsCore *data )
{
     D_DEBUG_AT( Core_Graphics, "%s( %p )\n", __FUNCTION__, data );

     D_MAGIC_ASSERT( data, DFBGraphicsCore );
     D_MAGIC_ASSERT( data->shared, DFBGraphicsCoreShared );

     dfb_gfxcard_unlock();

     return DFB_OK;
}

/**********************************************************************************************************************/

static void dfb_gfxcard_update_stats( long long now );
static void dfb_gfxcard_switch_busy ( void );
static void dfb_gfxcard_switch_idle ( void );

DFBResult
dfb_gfxcard_lock( GraphicsDeviceLockFlags flags )
{
     DFBResult              ret;
     DFBGraphicsCoreShared *shared;
     GraphicsDeviceFuncs   *funcs;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     shared = card->shared;
     funcs  = &card->funcs;

     D_DEBUG_AT( Core_Graphics, "%s()\n", __FUNCTION__ );

     ret = fusion_skirmish_prevail( &shared->lock );
     if (ret)
          return ret;

     if ((flags & GDLF_SYNC) && funcs->EngineSync) {
          /* Start command processing if not already running. */
          if (!dfb_config->gfx_emit_early && shared->pending_ops && card->funcs.EmitCommands) {
               dfb_gfxcard_switch_busy();

               card->funcs.EmitCommands( card->driver_data, card->device_data );

               shared->pending_ops = false;
          }

          ret = funcs->EngineSync( card->driver_data, card->device_data );
          if (ret) {
               if (funcs->EngineReset)
                    funcs->EngineReset( card->driver_data, card->device_data );

               shared->state = NULL;

               fusion_skirmish_dismiss( &shared->lock );

               return ret;
          }

          dfb_gfxcard_switch_idle();
     }

     if ((shared->lock_flags & GDLF_RESET) && funcs->EngineReset)
          funcs->EngineReset( card->driver_data, card->device_data );

     if (shared->lock_flags & GDLF_INVALIDATE) {
          if (funcs->InvalidateState)
               funcs->InvalidateState( card->driver_data, card->device_data );
          shared->state = NULL;
     }

     shared->lock_flags = flags;

     return DFB_OK;
}

void
dfb_gfxcard_unlock()
{
     DFBGraphicsCoreShared *shared;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     D_DEBUG_AT( Core_Graphics, "%s()\n", __FUNCTION__ );

     shared = card->shared;

     fusion_skirmish_dismiss( &shared->lock );
}

DFBResult
dfb_gfxcard_flush()
{
     DFBResult              ret;
     DFBGraphicsCoreShared *shared;
     GraphicsDeviceFuncs   *funcs;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     D_DEBUG_AT( Core_Graphics, "%s()\n", __FUNCTION__ );

     if (dfb_config->gfx_emit_early) {
          D_DEBUG_AT( Core_Graphics, "  -> gfx-emit-early\n" );

          return DFB_OK;
     }

     shared = card->shared;
     funcs  = &card->funcs;

     ret = fusion_skirmish_prevail( &shared->lock );
     if (ret)
          return ret;

     /* Start command processing if not already running. */
     if (shared->pending_ops && funcs->EmitCommands) {
          D_DEBUG_AT( Core_Graphics, "  -> pending ops, emitting...\n" );

          dfb_gfxcard_switch_busy();

          funcs->EmitCommands( card->driver_data, card->device_data );

          shared->pending_ops = false;
     }
     else
          D_DEBUG_AT( Core_Graphics, "  -> nothing to emit!\n" );

     fusion_skirmish_dismiss( &shared->lock );

     return DFB_OK;
}

void
dfb_gfxcard_start_drawing( CardState *state )
{
     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     D_MAGIC_ASSERT( state, CardState );

     if (card->funcs.StartDrawing)
          card->funcs.StartDrawing( card->driver_data, card->device_data, state );
}

void
dfb_gfxcard_stop_drawing( CardState *state )
{
     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     D_MAGIC_ASSERT( state, CardState );

     if (card->funcs.StopDrawing)
          card->funcs.StopDrawing( card->driver_data, card->device_data, state );
}

bool
dfb_gfxcard_state_check( CardState           *state,
                         DFBAccelerationMask  accel )
{
     DFBResult          ret;
     int                cx2, cy2;
     CoreSurfaceBuffer *dst_buffer;
     CoreSurfaceBuffer *src_buffer;

     D_ASSERT( card != NULL );

     D_MAGIC_ASSERT( state, CardState );
     D_MAGIC_ASSERT_IF( state->destination, CoreSurface );
     D_MAGIC_ASSERT_IF( state->source, CoreSurface );
     D_ASSERT( state->clip.x2 >= state->clip.x1 );
     D_ASSERT( state->clip.y2 >= state->clip.y1 );
     D_ASSERT( state->clip.x1 >= 0 );
     D_ASSERT( state->clip.y1 >= 0 );

     D_DEBUG_AT( Core_GraphicsOps, "%s( %p, 0x%08x, %4d,%4d-%4d,%4d )\n", __FUNCTION__,
                 state, accel, DFB_REGION_VALS( &state->clip ) );

     if (DFB_BLITTING_FUNCTION(accel)) {
          D_DEBUG_AT( Core_GfxState, "%s( %p, 0x%08x ) blitting %p -> %p\n", __FUNCTION__,
                      state, accel, state->source, state->destination );
     }
     else {
          D_DEBUG_AT( Core_GfxState, "%s( %p, 0x%08x ) drawing -> %p\n", __FUNCTION__,
                      state, accel, state->destination );
     }

     if (state->clip.x1 < 0) {
          state->clip.x1   = 0;
          state->modified |= SMF_CLIP;
     }

     if (state->clip.y1 < 0) {
          state->clip.y1   = 0;
          state->modified |= SMF_CLIP;
     }

     D_DEBUG_AT( Core_GfxState, "  <- checked 0x%08x, accel 0x%08x, modified 0x%08x, mod_hw 0x%08x\n",
                 state->checked, state->accel, state->modified, state->mod_hw );

     /* Destination may have been destroyed. */
     if (!state->destination) {
          D_BUG( "no destination" );
          return false;
     }

     /* Destination buffer may have been destroyed (suspended). i.e by a vt-switching. */
     if (state->destination->num_buffers == 0) {
          D_DEBUG_AT( Core_GfxState, "  -> no buffers in destination surface\n" );
          return false;
     }

     if (DFB_BLITTING_FUNCTION( accel )) {
          /* Source may have been destroyed. */
          if (!state->source) {
               D_BUG( "no source" );
               return false;
          }

          /* Mask may have been destroyed. */
          if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR) && !state->source_mask) {
               D_BUG( "no mask" );
               return false;
          }

          /* Source2 may have been destroyed. */
          if (accel == DFXL_BLIT2 && !state->source2) {
               D_BUG( "no source2" );
               return false;
          }
     }

     ret = dfb_surface_lock( state->destination );
     if (ret)
          return false;

     dst_buffer = dfb_surface_get_buffer( state->destination, state->to );

     D_MAGIC_ASSERT( dst_buffer, CoreSurfaceBuffer );

     dfb_surface_unlock( state->destination );

     D_ASSUME( state->clip.x2 < state->destination->config.size.w );
     D_ASSUME( state->clip.y2 < state->destination->config.size.h );

     cx2 = state->destination->config.size.w - 1;
     cy2 = state->destination->config.size.h - 1;

     if (state->clip.x2 > cx2) {
          state->clip.x2 = cx2;

          if (state->clip.x1 > cx2)
               state->clip.x1 = cx2;

          state->modified |= SMF_CLIP;
     }

     if (state->clip.y2 > cy2) {
          state->clip.y2 = cy2;

          if (state->clip.y1 > cy2)
               state->clip.y1 = cy2;

          state->modified |= SMF_CLIP;
     }

     /* If there's no CheckState() function, there's no acceleration at all.  */
     if (!card->funcs.CheckState) {
          D_DEBUG_AT( Core_GfxState, "  -> no acceleration available\n" );
          return false;
     }

     /*
      * Check if this function has been disabled temporarily.
      */
     if (state->disabled & accel) {
          D_DEBUG_AT( Core_GfxState, "  -> acceleration disabled\n" );
          return false;
     }

     /* If destination or blend functions have been changed... */
     if (state->modified & (SMF_DESTINATION | SMF_SRC_BLEND | SMF_DST_BLEND | SMF_RENDER_OPTIONS)) {
          /* ...force rechecking for all functions. */
          state->checked = DFXL_NONE;
     }
     else {
          /* If source/mask or blitting flags have been changed... */
          if (state->modified & (SMF_SOURCE | SMF_BLITTING_FLAGS | SMF_SOURCE_MASK | SMF_SOURCE_MASK_VALS)) {
               /* ...force rechecking for all blitting functions. */
               state->checked &= ~DFXL_ALL_BLIT;
          }
          else if (state->modified & SMF_SOURCE2) {
               /* Otherwise force rechecking for Blit2() function if source2 has been changed. */
               state->checked &= ~DFXL_BLIT2;
          }

          /* If drawing flags have been changed... */
          if (state->modified & SMF_DRAWING_FLAGS) {
               /* ...force rechecking for all drawing functions. */
               state->checked &= ~DFXL_ALL_DRAW;
          }
     }

     D_DEBUG_AT( Core_GfxState, "  -> checked 0x%08x, accel 0x%08x, modified 0x%08x, mod_hw 0x%08x\n",
                 state->checked, state->accel, state->modified, state->mod_hw );

     /* If the function needs to be checked. */
     if (!(state->checked & accel)) {
          /* Unset unchecked functions. */
          state->accel &= state->checked;

          /* Call driver to (re)set the bit if the function is supported. */
          card->funcs.CheckState( card->driver_data, card->device_data, state, accel );

          /* Add the function to 'checked' functions. */
          state->checked |= accel;

          /* Add additional functions the driver might have checked, too. */
          state->checked |= state->accel;
     }

     D_DEBUG_AT( Core_GfxState, "  -> checked 0x%08x, accel 0x%08x, modified 0x%08x, mod_hw 0x%08x\n",
                 state->checked, state->accel, state->modified, state->mod_hw );

     /* Move modification flags to the set for drivers. */
     state->mod_hw   |= state->modified;
     state->modified  = SMF_NONE;

     /* If back_buffer policy is 'system only' and the GPU does not fully support system memory surfaces, there's no
        acceleration available. */
     if ((dst_buffer->policy == CSP_SYSTEMONLY &&
          !(card->caps.flags & CCF_READSYSMEM && card->caps.flags & CCF_WRITESYSMEM)) ||
         (!(card->caps.flags & CCF_RENDEROPTS) && (state->render_options & DSRO_MATRIX))) {
          /* Clear 'accelerated' functions. */
          state->accel   = DFXL_NONE;
          state->checked = DFXL_ALL;
     }
     else if (DFB_BLITTING_FUNCTION( accel )) {
          /* If the front buffer policy of the source is 'system only', no accelerated blitting is available. */
          ret = dfb_surface_lock( state->source );
          if (ret)
               return false;

          src_buffer = dfb_surface_get_buffer( state->source, state->from );

          D_MAGIC_ASSERT( src_buffer, CoreSurfaceBuffer );

          dfb_surface_unlock( state->source );

          if (src_buffer->policy == CSP_SYSTEMONLY && !(card->caps.flags & CCF_READSYSMEM)) {
               /* Clear 'accelerated blitting' functions. */
               state->accel   &= ~DFXL_ALL_BLIT;
               state->checked |=  DFXL_ALL_BLIT;
          }
     }

     D_DEBUG_AT( Core_GfxState, "  -> checked 0x%08x, accel 0x%08x, modified 0x%08x, mod_hw 0x%08x\n",
                 state->checked, state->accel, state->modified, state->mod_hw );

     /* Return whether the function bit is set. */
     return state->accel & accel;
}

/*
 * This function returns non zero after successful locking the surface(s) for access by hardware.
 * Propagate state changes to driver.
 */
static bool
dfb_gfxcard_state_acquire( CardState *state, DFBAccelerationMask accel )
{
     DFBResult               ret;
     DFBGraphicsCoreShared  *shared;
     CoreSurfaceAccessFlags  access = CSAF_WRITE;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     D_MAGIC_ASSERT( state, CardState );
     D_MAGIC_ASSERT_IF( state->destination, CoreSurface );
     D_MAGIC_ASSERT_IF( state->source, CoreSurface );

     D_DEBUG_AT( Core_GraphicsOps, "%s( %p, 0x%08x, %4d,%4d-%4d,%4d )\n", __FUNCTION__,
                 state, accel, DFB_REGION_VALS( &state->clip ) );

     shared = card->shared;

     /* Find locking flags. */
     if (DFB_BLITTING_FUNCTION( accel )) {
          if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA | DSBLIT_DST_COLORKEY))
               access |= CSAF_READ;
     }
     else if (state->drawingflags & (DSDRAW_BLEND | DSDRAW_DST_COLORKEY))
          access |= CSAF_READ;

     if (DFB_BLITTING_FUNCTION(accel)) {
          D_DEBUG_AT( Core_GfxState, "%s( %p, 0x%08x ) blitting %p -> %p\n", __FUNCTION__,
                      state, accel, state->source, state->destination );
     }
     else {
          D_DEBUG_AT( Core_GfxState, "%s( %p, 0x%08x ) drawing -> %p\n", __FUNCTION__,
                      state, accel, state->destination );
     }

     /* Push our own identity for buffer locking calls (locality of accessor). */
     Core_PushIdentity( 0 );

     /* Lock destination. */
     ret = dfb_surface_lock_buffer2( state->destination, state->to, state->destination_flip_count_used ?
                                     state->destination_flip_count : state->destination->flips,
                                     state->to_eye, CSAID_GPU, access, &state->dst );
     if (ret) {
          D_DEBUG_AT( Core_GfxState, "  -> could not lock destination for GPU access!\n" );
          Core_PopIdentity();
          return false;
     }

     /* If blitting... */
     if (DFB_BLITTING_FUNCTION( accel )) {
          /* ...lock source for reading. */
          if (state->source_buffer) {
               dfb_surface_lock( state->source );
               CoreSurfaceBuffer *src_buffer = state->source_buffer;
               ret = dfb_surface_buffer_lock( src_buffer, CSAID_GPU, CSAF_READ, &state->src );
               dfb_surface_unlock( state->source );
          }
          else if (state->source_flip_count_used)
               ret = dfb_surface_lock_buffer2( state->source, state->from, state->source_flip_count,
                                               state->from_eye, CSAID_GPU, CSAF_READ, &state->src );
          else
               ret = dfb_surface_lock_buffer2( state->source, state->from, state->source->flips,
                                               state->from_eye, CSAID_GPU, CSAF_READ, &state->src );
          if (ret) {
               D_DEBUG_AT( Core_GfxState, "  -> could not lock source for GPU access!\n" );
               dfb_surface_unlock_buffer( state->destination, &state->dst );
               Core_PopIdentity();
               return false;
          }

          /* If using a mask... */
          if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) {
               /* ...lock source mask for reading. */
               ret = dfb_surface_lock_buffer2( state->source_mask, state->from, state->source_mask->flips,
                                               state->from_eye, CSAID_GPU, CSAF_READ, &state->src_mask );
               if (ret) {
                    D_DEBUG_AT( Core_GfxState, "  -> could not lock source mask for GPU access!\n" );
                    dfb_surface_unlock_buffer( state->source, &state->src );
                    dfb_surface_unlock_buffer( state->destination, &state->dst );
                    Core_PopIdentity();
                    return false;
               }

               state->flags |= CSF_SOURCE_MASK_LOCKED;
          }

          /* If using source2... */
          if (accel == DFXL_BLIT2) {
               /* ...lock source2 for reading. */
               ret = dfb_surface_lock_buffer2( state->source2, state->from, state->source2->flips,
                                               state->from_eye, CSAID_GPU, CSAF_READ, &state->src2 );
               if (ret) {
                    D_DEBUG_AT( Core_GfxState, "  -> could not lock source2 for GPU access!\n" );

                    if (state->flags & CSF_SOURCE_MASK_LOCKED) {
                         dfb_surface_unlock_buffer( state->source_mask, &state->src_mask );
                         state->flags &= ~CSF_SOURCE_MASK_LOCKED;
                    }

                    dfb_surface_unlock_buffer( state->source, &state->src );
                    dfb_surface_unlock_buffer( state->destination, &state->dst );
                    Core_PopIdentity();
                    return false;
               }

               state->flags |= CSF_SOURCE2_LOCKED;
          }

          state->flags |= CSF_SOURCE_LOCKED;
     }

     /* Make sure that state setting with subsequent command execution isn't done by two processes simultaneously. */
     ret = dfb_gfxcard_lock( GDLF_NONE );
     if (ret) {
          D_DERROR( ret, "Core/GfxState: Could not lock GPU!\n" );

          dfb_surface_unlock_buffer( state->destination, &state->dst );

          if (state->flags & CSF_SOURCE_LOCKED) {
               dfb_surface_unlock_buffer( state->source, &state->src );
               state->flags &= ~CSF_SOURCE_LOCKED;
          }

          if (state->flags & CSF_SOURCE_MASK_LOCKED) {
               dfb_surface_unlock_buffer( state->source_mask, &state->src_mask );
               state->flags &= ~CSF_SOURCE_MASK_LOCKED;
          }

          if (state->flags & CSF_SOURCE2_LOCKED) {
               dfb_surface_unlock_buffer( state->source2, &state->src2 );
               state->flags &= ~CSF_SOURCE2_LOCKED;
          }

          Core_PopIdentity();

          return false;
     }

     /* If we are switching to another state... */
     if (state != shared->state || state->fusion_id != shared->holder) {
          D_DEBUG_AT( Core_GfxState, "  -> switch from %p [%lu] to %p [%lu]\n",
                      shared->state, shared->holder, state, state->fusion_id );

          /* ...set all modification bits and clear 'set' functions. */
          state->mod_hw |= SMF_ALL;
          state->set     = DFXL_NONE;

          shared->state  = state;
          shared->holder = state->fusion_id;
     }

     dfb_state_update( state, state->flags & (CSF_SOURCE_LOCKED | CSF_SOURCE2_LOCKED | CSF_SOURCE_MASK_LOCKED) );

     D_DEBUG_AT( Core_GfxState, "  -> modified 0x%08x, mod_hw 0x%08x\n", state->modified, state->mod_hw );

     /* Move modification flags for drivers. */
     state->mod_hw   |= state->modified;
     state->modified  = SMF_ALL;

     if (shared->last_allocation_id != state->dst.allocation->object.id) {
          shared->last_allocation_id = state->dst.allocation->object.id;

          /* Start command processing if not already running. */
          if (!dfb_config->gfx_emit_early && shared->pending_ops && card->funcs.EmitCommands) {
               dfb_gfxcard_switch_busy();

               card->funcs.EmitCommands( card->driver_data, card->device_data );
               shared->pending_ops = false;
          }
     }

     /* If function hasn't been set or state is modified, call the driver function to propagate the state changes. */
     D_DEBUG_AT( Core_GfxState, "  -> mod_hw 0x%08x, set 0x%08x\n", state->mod_hw, state->set );
     if (state->mod_hw || !(state->set & accel)) {
          card->funcs.SetState( card->driver_data, card->device_data, &card->funcs, state, accel );
          D_DEBUG_AT( Core_GfxState, "  -> mod_hw 0x%08x, set 0x%08x\n", state->mod_hw, state->set );
     }

     state->modified = SMF_NONE;

     return true;
}

/*
 * This function checks if acceleration is available for the specific function using the given state and returns
 * non zero after successful locking the surface(s) for access by hardware. Propagate state changes to driver.
 */
static bool
dfb_gfxcard_state_check_acquire( CardState           *state,
                                 DFBAccelerationMask  accel )
{
     DFBResult               ret;
     int                     cx2, cy2;
     CoreSurfaceBuffer      *dst_buffer;
     CoreSurfaceBuffer      *src_buffer;
     DFBGraphicsCoreShared  *shared;
     FusionSkirmish         *locks[4];
     unsigned int            num_locks = 0;
     CoreSurfaceAccessFlags  access    = CSAF_WRITE;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     D_MAGIC_ASSERT( state, CardState );
     D_MAGIC_ASSERT_IF( state->destination, CoreSurface );
     D_MAGIC_ASSERT_IF( state->source, CoreSurface );
     D_ASSERT( state->clip.x2 >= state->clip.x1 );
     D_ASSERT( state->clip.y2 >= state->clip.y1 );
     D_ASSERT( state->clip.x1 >= 0 );
     D_ASSERT( state->clip.y1 >= 0 );

     D_DEBUG_AT( Core_GraphicsOps, "%s( %p, 0x%08x, %4d,%4d-%4d,%4d )\n", __FUNCTION__,
                 state, accel, DFB_REGION_VALS( &state->clip ) );

     shared = card->shared;

     locks[num_locks++] = &state->destination->lock;

     /* Find locking flags. */
     if (DFB_BLITTING_FUNCTION( accel )) {
          if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA | DSBLIT_DST_COLORKEY))
               access |= CSAF_READ;
     }
     else if (state->drawingflags & (DSDRAW_BLEND | DSDRAW_DST_COLORKEY))
          access |= CSAF_READ;

     if (DFB_BLITTING_FUNCTION(accel)) {
          D_DEBUG_AT( Core_GfxState, "%s( %p, 0x%08x ) blitting %p -> %p\n", __FUNCTION__,
                      state, accel, state->source, state->destination );

          locks[num_locks++] = &state->source->lock;

          /* If using a mask. */
          if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR))
               locks[num_locks++] = &state->source_mask->lock;

          /* If using source2. */
          if (accel == DFXL_BLIT2)
               locks[num_locks++] = &state->source2->lock;
     }
     else {
          D_DEBUG_AT( Core_GfxState, "%s( %p, 0x%08x ) drawing -> %p\n", __FUNCTION__,
                      state, accel, state->destination );
     }

     if (state->clip.x1 < 0) {
          state->clip.x1   = 0;
          state->modified |= SMF_CLIP;
     }

     if (state->clip.y1 < 0) {
          state->clip.y1   = 0;
          state->modified |= SMF_CLIP;
     }

     D_DEBUG_AT( Core_GfxState, "  <- checked 0x%08x, accel 0x%08x, modified 0x%08x, mod_hw 0x%08x\n",
                 state->checked, state->accel, state->modified, state->mod_hw );

     /* Destination may have been destroyed. */
     if (!state->destination) {
          D_BUG( "no destination" );
          return false;
     }

     /* Destination buffer may have been destroyed (suspended). i.e by a vt-switching */
     if (state->destination->num_buffers == 0 )
          return false;

     if (DFB_BLITTING_FUNCTION( accel )) {
          /* Source may have been destroyed. */
          if (!state->source) {
               D_BUG( "no source" );
               return false;
          }

          /* Mask may have been destroyed. */
          if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR) && !state->source_mask) {
               D_BUG( "no mask" );
               return false;
          }

          /* Source2 may have been destroyed. */
          if (accel == DFXL_BLIT2 && !state->source2) {
               D_BUG( "no source2" );
               return false;
          }
     }

     D_ASSUME( state->clip.x2 < state->destination->config.size.w );
     D_ASSUME( state->clip.y2 < state->destination->config.size.h );

     cx2 = state->destination->config.size.w - 1;
     cy2 = state->destination->config.size.h - 1;

     if (state->clip.x2 > cx2) {
          state->clip.x2 = cx2;

          if (state->clip.x1 > cx2)
               state->clip.x1 = cx2;

          state->modified |= SMF_CLIP;
     }

     if (state->clip.y2 > cy2) {
          state->clip.y2 = cy2;

          if (state->clip.y1 > cy2)
               state->clip.y1 = cy2;

          state->modified |= SMF_CLIP;
     }

     /* If there's no CheckState() function, there's no acceleration at all. */
     if (!card->funcs.CheckState)
          return false;

     /* Check if this function has been disabled temporarily. */
     if (state->disabled & accel)
          return false;

     if (core_dfb->shutdown_running)
          return false;

     if (fusion_skirmish_prevail_multi( locks, num_locks ))
          return false;

     dfb_state_update_destination( state );

     /* If destination or blend functions have been changed... */
     if (state->modified & (SMF_DESTINATION | SMF_SRC_BLEND | SMF_DST_BLEND | SMF_RENDER_OPTIONS)) {
          /* ...force rechecking for all functions. */
          state->checked = DFXL_NONE;

          if (state->destination->config.size.w < card->limits.dst_min.w ||
              state->destination->config.size.h < card->limits.dst_min.h ||
              state->destination->config.size.w > card->limits.dst_max.w ||
              state->destination->config.size.h > card->limits.dst_max.h) {
               fusion_skirmish_dismiss_multi( locks, num_locks );
               return false;
          }
     }
     else {
          /* If source/mask or blitting flags have been changed... */
          if (state->modified & (SMF_SOURCE | SMF_BLITTING_FLAGS | SMF_SOURCE_MASK | SMF_SOURCE_MASK_VALS)) {
               /* ...force rechecking for all blitting functions. */
               state->checked &= ~DFXL_ALL_BLIT;
          }
          else if (state->modified & SMF_SOURCE2) {
               /* Otherwise force rechecking for Blit2() function if source2 has been changed. */
               state->checked &= ~DFXL_BLIT2;
          }

          /* If drawing flags have been changed... */
          if (state->modified & SMF_DRAWING_FLAGS) {
               /* ...force rechecking for all drawing functions. */
               state->checked &= ~DFXL_ALL_DRAW;
          }

          if (state->source) {
               if (state->source->config.size.w < card->limits.dst_min.w ||
                   state->source->config.size.h < card->limits.dst_min.h ||
                   state->source->config.size.w > card->limits.dst_max.w ||
                   state->source->config.size.h > card->limits.dst_max.h) {
                    fusion_skirmish_dismiss_multi( locks, num_locks );
                    return false;
               }
          }
     }

     D_DEBUG_AT( Core_GfxState, "  -> checked 0x%08x, accel 0x%08x, modified 0x%08x, mod_hw 0x%08x\n",
                 state->checked, state->accel, state->modified, state->mod_hw );

     if (DFB_BLITTING_FUNCTION(accel)) {
          dfb_state_update_sources( state, CSF_SOURCE );

          /* If using a mask. */
          if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR))
               dfb_state_update_sources( state, CSF_SOURCE_MASK );

          /* If using source2. */
          if (accel == DFXL_BLIT2)
               dfb_state_update_sources( state, CSF_SOURCE2 );
     }

     /* If the function needs to be checked. */
     if (!(state->checked & accel)) {
          /* Unset unchecked functions. */
          state->accel &= state->checked;

          /* Call driver to (re)set the bit if the function is supported. */
          card->funcs.CheckState( card->driver_data, card->device_data, state, accel );

          /* Add the function to 'checked' functions. */
          state->checked |= accel;

          /* Add additional functions the driver might have checked, too. */
          state->checked |= state->accel;
     }

     D_DEBUG_AT( Core_GfxState, "  -> checked 0x%08x, accel 0x%08x, modified 0x%08x, mod_hw 0x%08x\n",
                 state->checked, state->accel, state->modified, state->mod_hw );

     /* Push our own identity for buffer locking calls (locality of accessor) */
     Core_PushIdentity( 0 );

     /* Move modification flags for drivers. */
     state->mod_hw   |= state->modified;
     state->modified  = SMF_NONE;

     if (state->destination_flip_count_used)
          dst_buffer = dfb_surface_get_buffer3( state->destination, state->to, state->to_eye,
                                                state->destination_flip_count );
     else
          dst_buffer = dfb_surface_get_buffer3( state->destination, state->to, state->to_eye,
                                                state->destination->flips );

     D_MAGIC_ASSERT( dst_buffer, CoreSurfaceBuffer );

     /* If back_buffer policy is 'system only' and the GPU does not fully support system memory surfaces, there's no
        acceleration available. */
     if ((dst_buffer->policy == CSP_SYSTEMONLY &&
          !(card->caps.flags & CCF_READSYSMEM && card->caps.flags & CCF_WRITESYSMEM)) ||
         (!(card->caps.flags & CCF_RENDEROPTS) && (state->render_options & DSRO_MATRIX))) {
          /* Clear 'accelerated' functions. */
          state->accel   = DFXL_NONE;
          state->checked = DFXL_ALL;

          Core_PopIdentity();
          fusion_skirmish_dismiss_multi( locks, num_locks );
          return false;
     }

     if (!(state->accel & accel)) {
          D_DEBUG_AT( Core_GfxState, "  -> not accelerated\n" );
          Core_PopIdentity();
          fusion_skirmish_dismiss_multi( locks, num_locks );
          return false;
     }

     ret = dfb_surface_buffer_lock( dst_buffer, CSAID_GPU, access, &state->dst );
     if (ret) {
          D_DEBUG_AT( Core_GfxState, "  -> could not lock destination for GPU access!\n" );
          Core_PopIdentity();
          fusion_skirmish_dismiss_multi( locks, num_locks );
          return false;
     }

     if (DFB_BLITTING_FUNCTION( accel )) {
          /* If the front buffer policy of the source is 'system only', no accelerated blitting is available. */
          if (state->source_buffer)
               src_buffer = state->source_buffer;
          else if (state->source_flip_count_used)
               src_buffer = dfb_surface_get_buffer3( state->source, state->from, state->from_eye,
                                                     state->source_flip_count );
          else
               src_buffer = dfb_surface_get_buffer3( state->source, state->from, state->from_eye,
                                                     state->source->flips );

          D_MAGIC_ASSERT( src_buffer, CoreSurfaceBuffer );

          if (src_buffer->policy == CSP_SYSTEMONLY && !(card->caps.flags & CCF_READSYSMEM)) {
               D_DEBUG_AT( Core_GfxState, "  -> cannot read source sysmem\n" );

               /* Clear 'accelerated blitting' functions. */
               state->accel   &= ~DFXL_ALL_BLIT;
               state->checked |=  DFXL_ALL_BLIT;
          }

          ret = dfb_surface_buffer_lock( src_buffer, CSAID_GPU, CSAF_READ, &state->src );
          if (ret) {
               D_DEBUG_AT( Core_GfxState, "  -> could not lock source for GPU access!\n" );
               dfb_surface_unlock_buffer( state->destination, &state->dst );
               Core_PopIdentity();
               fusion_skirmish_dismiss_multi( locks, num_locks );
               return false;
          }

          /* If using a mask... */
          if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) {
               /* ...lock source mask for reading */
               ret = dfb_surface_lock_buffer2( state->source_mask, state->from, state->source_mask->flips,
                                               state->from_eye, CSAID_GPU, CSAF_READ, &state->src_mask );
               if (ret) {
                    D_DEBUG_AT( Core_GfxState, "  -> could not lock source mask for GPU access!\n" );
                    dfb_surface_unlock_buffer( state->source, &state->src );
                    dfb_surface_unlock_buffer( state->destination, &state->dst );
                    Core_PopIdentity();
                    fusion_skirmish_dismiss_multi( locks, num_locks );
                    return false;
               }

               state->flags |= CSF_SOURCE_MASK_LOCKED;
          }

          /* If using source2... */
          if (accel == DFXL_BLIT2) {
               /* ...lock source2 for reading */
               ret = dfb_surface_lock_buffer2( state->source2, state->from, state->source2->flips,
                                               state->from_eye, CSAID_GPU, CSAF_READ, &state->src2 );
               if (ret) {
                    D_DEBUG_AT( Core_GfxState, "  -> could not lock source2 for GPU access!\n" );
                    if (state->flags & CSF_SOURCE_MASK_LOCKED) {
                         dfb_surface_unlock_buffer( state->source_mask, &state->src_mask );
                         state->flags &= ~CSF_SOURCE_MASK_LOCKED;
                    }
                    dfb_surface_unlock_buffer( state->source, &state->src );
                    dfb_surface_unlock_buffer( state->destination, &state->dst );
                    Core_PopIdentity();
                    fusion_skirmish_dismiss_multi( locks, num_locks );
                    return false;
               }

               state->flags |= CSF_SOURCE2_LOCKED;
          }

          state->flags |= CSF_SOURCE_LOCKED;
     }

     fusion_skirmish_dismiss_multi( locks, num_locks );

     D_DEBUG_AT( Core_GfxState, "  -> checked 0x%08x, accel 0x%08x, modified 0x%08x, mod_hw 0x%08x\n",
                 state->checked, state->accel, state->modified, state->mod_hw );

     /* Make sure that state setting with subsequent command execution isn't done by two processes simultaneously. */
     if (dfb_gfxcard_lock( GDLF_NONE )) {
          D_DERROR( ret, "Core/GfxState: Could not lock GPU!\n" );

          dfb_surface_unlock_buffer( state->destination, &state->dst );

          if (state->flags & CSF_SOURCE_LOCKED) {
               dfb_surface_unlock_buffer( state->source, &state->src );
               state->flags &= ~CSF_SOURCE_LOCKED;
          }

          if (state->flags & CSF_SOURCE_MASK_LOCKED) {
               dfb_surface_unlock_buffer( state->source_mask, &state->src_mask );
               state->flags &= ~CSF_SOURCE_MASK_LOCKED;
          }

          if (state->flags & CSF_SOURCE2_LOCKED) {
               dfb_surface_unlock_buffer( state->source2, &state->src2 );
               state->flags &= ~CSF_SOURCE2_LOCKED;
          }

          Core_PopIdentity();

          return false;
     }

     /* If we are switching to another state... */
     if (state != shared->state || state->fusion_id != shared->holder) {
          D_DEBUG_AT( Core_GfxState, "  -> switch from %p [%lu] to %p [%lu]\n",
                      shared->state, shared->holder, state, state->fusion_id );

          /* ...set all modification bits and clear 'set' functions. */
          state->mod_hw |= SMF_ALL;
          state->set     = DFXL_NONE;

          shared->state  = state;
          shared->holder = state->fusion_id;
     }

     D_DEBUG_AT( Core_GfxState, "  -> modified 0x%08x, mod_hw 0x%08x\n", state->modified, state->mod_hw );

     /* Move modification flags for drivers. */
     state->mod_hw   |= state->modified;
     state->modified  = SMF_ALL;

     if (shared->last_allocation_id != state->dst.allocation->object.id) {
          shared->last_allocation_id = state->dst.allocation->object.id;

          /* Start command processing if not already running. */
          if (!dfb_config->gfx_emit_early && shared->pending_ops && card->funcs.EmitCommands) {
               dfb_gfxcard_switch_busy();

               card->funcs.EmitCommands( card->driver_data, card->device_data );
               shared->pending_ops = false;
          }
     }

     /* If function hasn't been set or state is modified, call the driver function to propagate the state changes. */
     D_DEBUG_AT( Core_GfxState, "  -> mod_hw 0x%08x, set 0x%08x\n", state->mod_hw, state->set );
     if (state->mod_hw || !(state->set & accel)) {
          card->funcs.SetState( card->driver_data, card->device_data, &card->funcs, state, accel );
          D_DEBUG_AT( Core_GfxState, "  -> mod_hw 0x%08x, set 0x%08x\n", state->mod_hw, state->set );
     }

     state->modified = SMF_NONE;

     return true;
}

/*
 * Unlock destination and possibly the source.
 */
static void
dfb_gfxcard_state_release( CardState *state )
{
     DFBGraphicsCoreShared *shared;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( state->destination != NULL );

     shared = card->shared;

     if (!dfb_config->software_only) {
          /* Store the serial of the operation. */
          if (card->funcs.GetSerial)
               card->funcs.GetSerial( card->driver_data, card->device_data, &state->dst.allocation->gfx_serial );

          if (dfb_config->gfx_emit_early && card->funcs.EmitCommands) {
               dfb_gfxcard_switch_busy();

               card->funcs.EmitCommands( card->driver_data, card->device_data );
               shared->pending_ops = false;
          }
          else
               shared->pending_ops = true;
     }

     /* Allow others to use the hardware. */
     dfb_gfxcard_unlock();

     /* Destination always gets locked during acquisition. */
     dfb_surface_unlock_buffer( state->destination, &state->dst );

     if (state->flags & CSF_SOURCE_LOCKED) {
          dfb_surface_unlock_buffer( state->source, &state->src );

          state->flags &= ~CSF_SOURCE_LOCKED;
     }

     if (state->flags & CSF_SOURCE_MASK_LOCKED) {
          dfb_surface_unlock_buffer( state->source_mask, &state->src_mask );

          state->flags &= ~CSF_SOURCE_MASK_LOCKED;
     }

     if (state->flags & CSF_SOURCE2_LOCKED) {
          dfb_surface_unlock_buffer( state->source2, &state->src2 );

          state->flags &= ~CSF_SOURCE2_LOCKED;
     }

     Core_PopIdentity();
}

void
dfb_gfxcard_state_init ( CardState *state )
{
     D_MAGIC_ASSERT( state, CardState );

     if (dfb_config->software_only)
          return;

     if (card) {
          D_ASSERT( card != NULL );
          D_ASSERT( card->shared != NULL );

          if (card->funcs.StateInit)
               card->funcs.StateInit( card->driver_data, card->device_data, state );
     }
}

void
dfb_gfxcard_state_destroy ( CardState *state )
{
     D_MAGIC_ASSERT( state, CardState );

     if (state->gfxs) {
          GenefxState *gfxs = state->gfxs;

          if (gfxs->ABstart)
               D_FREE( gfxs->ABstart );

          D_FREE( gfxs );
     }

     if (dfb_config->software_only)
          return;

     if (card) {
          D_ASSERT( card != NULL );
          D_ASSERT( card->shared != NULL );

          if (card->funcs.StateDestroy)
               card->funcs.StateDestroy( card->driver_data, card->device_data, state );
     }
}

/**********************************************************************************************************************/

#define DFB_TRANSFORM(x,y,m,affine)                                   \
do {                                                                  \
     s32 _x, _y, _w;                                                  \
     if (affine) {                                                    \
          _x = ((x) * (m)[0] + (y) * (m)[1] + (m)[2] + 0x8000) >> 16; \
          _y = ((x) * (m)[3] + (y) * (m)[4] + (m)[5] + 0x8000) >> 16; \
     }                                                                \
     else {                                                           \
          _x = ((x) * (m)[0] + (y) * (m)[1] + (m)[2]);                \
          _y = ((x) * (m)[3] + (y) * (m)[4] + (m)[5]);                \
          _w = ((x) * (m)[6] + (y) * (m)[7] + (m)[8]);                \
          if (!_w) {                                                  \
               _x = (_x < 0) ? -0x7fffffff : 0x7fffffff;              \
               _y = (_y < 0) ? -0x7fffffff : 0x7fffffff;              \
          }                                                           \
          else {                                                      \
               _x /= _w;                                              \
               _y /= _w;                                              \
          }                                                           \
     }                                                                \
     (x) = _x;                                                        \
     (y) = _y;                                                        \
} while (0)

typedef struct {
   int xi;
   int xf;
   int mi;
   int mf;
   int _2dy;
} DDA;

#define SETUP_DDA(xs,ys,xe,ye,dda)           \
     do {                                    \
          int dx = (xe) - (xs);              \
          int dy = (ye) - (ys);              \
          dda.xi = (xs);                     \
          if (dy != 0) {                     \
               dda.mi = dx / dy;             \
               dda.mf = 2 * (dx % dy);       \
               dda.xf = -dy;                 \
               dda._2dy = 2 * dy;            \
               if (dda.mf < 0) {             \
                    dda.mf += 2 * ABS( dy ); \
                    dda.mi--;                \
               }                             \
          }                                  \
          else {                             \
               dda.mi = 0;                   \
               dda.mf = 0;                   \
               dda.xf = 0;                   \
               dda._2dy = 0;                 \
          }                                  \
     } while (0)

#define INC_DDA(dda)                         \
     do {                                    \
          dda.xi += dda.mi;                  \
          dda.xf += dda.mf;                  \
          if (dda.xf > 0) {                  \
               dda.xi++;                     \
               dda.xf -= dda._2dy;           \
          }                                  \
     } while (0)

static void
fill_tri( DFBTriangle *tri,
          CardState   *state,
          bool         accelerated )
{
     int y, yend;
     DDA dda1    = { .xi = 0 };
     DDA dda2    = { .xi = 0 };
     int clip_x1 = state->clip.x1;
     int clip_x2 = state->clip.x2;

     D_MAGIC_ASSERT( state, CardState );

     y = tri->y1;
     yend = tri->y3;

     if (yend > state->clip.y2)
          yend = state->clip.y2;

     SETUP_DDA( tri->x1, tri->y1, tri->x3, tri->y3, dda1 );
     SETUP_DDA( tri->x1, tri->y1, tri->x2, tri->y2, dda2 );

     while (y <= yend) {
          DFBRectangle rect;

          if (y == tri->y2) {
               if (tri->y2 == tri->y3)
                    return;
               SETUP_DDA( tri->x2, tri->y2, tri->x3, tri->y3, dda2 );
          }

          rect.w = ABS( dda1.xi - dda2.xi );
          rect.x = MIN( dda1.xi, dda2.xi );

          if (clip_x2 < rect.x + rect.w)
               rect.w = clip_x2 - rect.x + 1;

          if (rect.w > 0) {
               if (clip_x1 > rect.x) {
                    rect.w -= (clip_x1 - rect.x);
                    rect.x = clip_x1;
               }
               rect.y = y;
               rect.h = 1;

               if (rect.w > 0 && rect.y >= state->clip.y1) {
                    if (accelerated)
                         card->funcs.FillRectangle( card->driver_data, card->device_data, &rect );
                    else
                         gFillRectangle( state, &rect );
               }
          }

          INC_DDA( dda1 );
          INC_DDA( dda2 );

          y++;
     }
}

void
dfb_gfxcard_fillrectangles( DFBRectangle *rects,
                            int           num,
                            CardState    *state )
{
     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( rects != NULL );
     D_ASSERT( num > 0 );

     D_DEBUG_AT( Core_GraphicsOps, "%s( %p [%d], %p )\n", __FUNCTION__, rects, num, state );

     /* The state is locked during graphics operations. */
     dfb_state_lock( state );

     /* Signal beginning of sequence of operations if not already done. */
     dfb_state_start_drawing( state );

     if (!(state->render_options & DSRO_MATRIX)) {
          while (num > 0) {
               if (dfb_rectangle_region_intersects( rects, &state->clip ))
                    break;

               rects++;
               num--;
          }
     }

     if (num > 0) {
          DFBRectangle rect;
          int          i = 0;

          /* Check for acceleration and setup execution. */
          if (dfb_gfxcard_state_check_acquire( state, DFXL_FILLRECTANGLE )) {
               /* Now everything is prepared for execution of the FillRectangle() driver function.  */
               for (; i < num; i++) {
                    if (!(state->render_options & DSRO_MATRIX) &&
                        !dfb_rectangle_region_intersects( &rects[i], &state->clip ))
                         continue;

                    rect = rects[i];

                    if (rect.w > card->limits.dst_max.w || rect.h > card->limits.dst_max.h) {
                         dfb_clip_rectangle( &state->clip, &rect );

                         if (rect.w > card->limits.dst_max.w || rect.h > card->limits.dst_max.h) {
                              D_DEBUG_AT( Core_GraphicsOps, "  -> driver limits prevent hw operation\n" );
                              break;
                         }
                    }
                    else if (!D_FLAGS_IS_SET( card->caps.flags, CCF_CLIPPING ) &&
                             !D_FLAGS_IS_SET( card->caps.clip, DFXL_FILLRECTANGLE ))
                         dfb_clip_rectangle( &state->clip, &rect );

                    if (!card->funcs.FillRectangle( card->driver_data, card->device_data, &rect )) {
                         D_DEBUG_AT( Core_GraphicsOps, "  -> driver returned false\n" );
                         break;
                    }
               }

               /* Release after state acquisition. */
               dfb_gfxcard_state_release( state );
          }

          if (i < num) {
               D_DEBUG_AT( Core_GraphicsOps, "  -> using software fallback\n" );

               /* Use software fallback. */
               if (!(state->render_options & DSRO_MATRIX)) {
                    if (gAcquire( state, DFXL_FILLRECTANGLE )) {
                         for (; i < num; i++) {
                              rect = rects[i];

                              if (dfb_clip_rectangle( &state->clip, &rect ))
                                   gFillRectangle( state, &rect );
                         }

                         gRelease( state );
                    }
               }
               else if (state->matrix[1] == 0 && state->matrix[3] == 0) {
                    /* Scaled/Translated rectangles. */
                    DFBRectangle tr[num];
                    int          n = 0;

                    for (; i < num; i++) {
                         int x1, y1, x2, y2;

                         x1 = rects[i].x;      y1 = rects[i].y;
                         x2 = x1 + rects[i].w; y2 = y1 + rects[i].h;
                         DFB_TRANSFORM( x1, y1, state->matrix, state->affine_matrix );
                         DFB_TRANSFORM( x2, y2, state->matrix, state->affine_matrix );

                         if (x1 < x2) {
                              tr[n].x = x1;
                              tr[n].w = x2 - x1;
                         }
                         else {
                              tr[n].x = x2;
                              tr[n].w = x1 - x2;
                         }
                         if (y1 < y2) {
                              tr[n].y = y1;
                              tr[n].h = y2 - y1;
                         }
                         else {
                              tr[n].y = y2;
                              tr[n].h = y1 - y2;
                         }

                         if (dfb_clip_rectangle( &state->clip, &tr[n] ))
                              n++;
                    }

                    if (n > 0) {
                         state->render_options &= ~DSRO_MATRIX;
                         state->modified       |= SMF_RENDER_OPTIONS;

                         dfb_gfxcard_fillrectangles( tr, n, state );

                         state->render_options |= DSRO_MATRIX;
                         state->modified       |= SMF_RENDER_OPTIONS;
                    }
               }
               else {
                    /* Rotated rectangle: split into triangles. */
                    if (gAcquire( state, DFXL_FILLRECTANGLE )) {
                         for (; i < num; i++) {
                              DFBTriangle tri;

                              tri.x1 = rects[i].x;              tri.y1 = rects[i].y;
                              tri.x2 = rects[i].x + rects[i].w; tri.y2 = rects[i].y;
                              tri.x3 = rects[i].x + rects[i].w; tri.y3 = rects[i].y + rects[i].h;
                              DFB_TRANSFORM( tri.x1, tri.y1, state->matrix, state->affine_matrix );
                              DFB_TRANSFORM( tri.x2, tri.y2, state->matrix, state->affine_matrix );
                              DFB_TRANSFORM( tri.x3, tri.y3, state->matrix, state->affine_matrix );

                              dfb_sort_triangle( &tri );
                              if (tri.y3 - tri.y1 > 0)
                                   fill_tri( &tri, state, false );

                              tri.x1 = rects[i].x;              tri.y1 = rects[i].y;
                              tri.x2 = rects[i].x + rects[i].w; tri.y2 = rects[i].y + rects[i].h;
                              tri.x3 = rects[i].x;              tri.y3 = rects[i].y + rects[i].h;
                              DFB_TRANSFORM( tri.x1, tri.y1, state->matrix, state->affine_matrix );
                              DFB_TRANSFORM( tri.x2, tri.y2, state->matrix, state->affine_matrix );
                              DFB_TRANSFORM( tri.x3, tri.y3, state->matrix, state->affine_matrix );

                              dfb_sort_triangle( &tri );
                              if (tri.y3 - tri.y1 > 0)
                                   fill_tri( &tri, state, false );
                         }

                         gRelease( state );
                    }
               }
          }
     }

     /* Unlock after execution. */
     dfb_state_unlock( state );
}

void
dfb_gfxcard_drawrectangle( DFBRectangle *rect,
                           CardState    *state )
{
     DFBRectangle rects[4];
     bool         hw  = false;
     int          i   = 0;
     int          num = 0;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     D_MAGIC_ASSERT( state, CardState );
     DFB_RECTANGLE_ASSERT( rect );

     D_DEBUG_AT( Core_GraphicsOps, "%s( %4d,%4d-%4dx%4d, %p )\n", __FUNCTION__, DFB_RECTANGLE_VALS( rect ), state );

     /* The state is locked during graphics operations. */
     dfb_state_lock( state );

     /* Signal beginning of sequence of operations if not already done. */
     dfb_state_start_drawing( state );

     if (!(state->render_options & DSRO_MATRIX) &&
         !dfb_rectangle_region_intersects( rect, &state->clip )) {
          dfb_state_unlock( state );
          return;
     }

     if (D_FLAGS_IS_SET( card->caps.flags, CCF_CLIPPING )      ||
         D_FLAGS_IS_SET( card->caps.clip, DFXL_DRAWRECTANGLE ) ||
         !dfb_clip_needed( &state->clip, rect )) {
          if (rect->w <= card->limits.dst_max.w && rect->h <= card->limits.dst_max.h &&
              dfb_gfxcard_state_check_acquire( state, DFXL_DRAWRECTANGLE )) {
               hw = card->funcs.DrawRectangle( card->driver_data, card->device_data, rect );
               dfb_gfxcard_state_release( state );
          }
     }

     if (!hw && !(state->render_options & DSRO_MATRIX)) {
          dfb_build_clipped_rectangle_outlines( rect, &state->clip, rects, &num );

          if (!num) {
               dfb_state_unlock( state );
               return;
          }

          if (dfb_gfxcard_state_check_acquire( state, DFXL_FILLRECTANGLE )) {
               for (; i < num; i++) {
                    hw = rects[i].w <= card->limits.dst_max.w && rects[i].h <= card->limits.dst_max.h &&
                         card->funcs.FillRectangle( card->driver_data, card->device_data, &rects[i] );
                    if (!hw)
                         break;
               }

               dfb_gfxcard_state_release( state );
          }
     }

     if (!hw) {
          if (!(state->render_options & DSRO_MATRIX)) {
               if (gAcquire( state, DFXL_FILLRECTANGLE )) {
                    for (; i < num; i++)
                         gFillRectangle( state, &rects[i] );

                    gRelease( state );
               }
          }
          else {
                if (gAcquire( state, DFXL_DRAWLINE )) {
                    int       x1, x2, x3, x4;
                    int       y1, y2, y3, y4;
                    DFBRegion line;

                    x1 = rect->x;           y1 = rect->y;
                    x2 = rect->x + rect->w; y2 = rect->y;
                    x3 = rect->x + rect->w; y3 = rect->y + rect->h;
                    x4 = rect->x;           y4 = rect->y + rect->h;
                    DFB_TRANSFORM( x1, y1, state->matrix, state->affine_matrix );
                    DFB_TRANSFORM( x2, y2, state->matrix, state->affine_matrix );
                    DFB_TRANSFORM( x3, y3, state->matrix, state->affine_matrix );
                    DFB_TRANSFORM( x4, y4, state->matrix, state->affine_matrix );

                    line = (DFBRegion) { x1, y1, x2, y2 };
                    if (dfb_clip_line( &state->clip, &line ))
                         gDrawLine( state, &line );

                    line = (DFBRegion) { x2, y2, x3, y3 };
                    if (dfb_clip_line( &state->clip, &line ))
                         gDrawLine( state, &line );

                    line = (DFBRegion) { x3, y3, x4, y4 };
                    if (dfb_clip_line( &state->clip, &line ))
                         gDrawLine( state, &line );

                    line = (DFBRegion) { x4, y4, x1, y1 };
                    if (dfb_clip_line( &state->clip, &line ))
                         gDrawLine( state, &line );

                    gRelease( state );
               }
          }
     }

     dfb_state_unlock( state );
}

void
dfb_gfxcard_drawlines( DFBRegion *lines,
                       int        num,
                       CardState *state )
{
     int i = 0;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( lines != NULL );
     D_ASSERT( num > 0 );

     D_DEBUG_AT( Core_GraphicsOps, "%s( %p [%d], %p )\n", __FUNCTION__, lines, num, state );

     /* The state is locked during graphics operations. */
     dfb_state_lock( state );

     /* Signal beginning of sequence of operations if not already done. */
     dfb_state_start_drawing( state );

     if (dfb_gfxcard_state_check_acquire( state, DFXL_DRAWLINE )) {
          for (; i < num; i++) {
               if (!D_FLAGS_IS_SET( card->caps.flags, CCF_CLIPPING ) &&
                   !D_FLAGS_IS_SET( card->caps.clip, DFXL_DRAWLINE ) &&
                   !dfb_clip_line( &state->clip, &lines[i] ))
                    continue;

               if (!card->funcs.DrawLine( card->driver_data, card->device_data, &lines[i] ))
                    break;
          }

          dfb_gfxcard_state_release( state );
     }

     if (i < num) {
          if (gAcquire( state, DFXL_DRAWLINE )) {
               for (; i < num; i++) {
                    if (state->render_options & DSRO_MATRIX) {
                         DFB_TRANSFORM( lines[i].x1, lines[i].y1, state->matrix, state->affine_matrix );
                         DFB_TRANSFORM( lines[i].x2, lines[i].y2, state->matrix, state->affine_matrix );
                    }

                    if (dfb_clip_line( &state->clip, &lines[i] ))
                         gDrawLine( state, &lines[i] );
               }

               gRelease( state );
          }
     }

     dfb_state_unlock( state );
}

void
dfb_gfxcard_filltriangles( DFBTriangle *tris,
                           int          num,
                           CardState   *state )
{
     bool hw = false;
     int  i  = 0;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( tris != NULL );
     D_ASSERT( num > 0 );

     D_DEBUG_AT( Core_GraphicsOps, "%s( %p [%d], %p )\n", __FUNCTION__, tris, num, state );

     /* The state is locked during graphics operations. */
     dfb_state_lock( state );

     /* Signal beginning of sequence of operations if not already done. */
     dfb_state_start_drawing( state );

     if (dfb_gfxcard_state_check_acquire( state, DFXL_FILLTRIANGLE )) {
          if (!D_FLAGS_IS_SET( card->caps.flags, CCF_CLIPPING ) &&
              !D_FLAGS_IS_SET( card->caps.clip, DFXL_FILLTRIANGLE )) {
               DFBPoint p[6];
               int      n;

               for (; i < num; i++) {
                    if (dfb_clip_triangle( &state->clip, &tris[i], p, &n )) {
                         DFBTriangle tri;
                         int         j;

                         tri.x1 = p[0].x; tri.y1 = p[0].y;
                         tri.x2 = p[1].x; tri.y2 = p[1].y;
                         tri.x3 = p[2].x; tri.y3 = p[2].y;
                         hw = card->funcs.FillTriangle( card->driver_data, card->device_data, &tri );
                         if (!hw) {
                              D_DEBUG_AT( Core_GraphicsOps, "  -> driver returned false\n" );
                              break;
                         }

                         for (j = 3; j < n; j++) {
                              tri.x1 = p[0].x;   tri.y1 = p[0].y;
                              tri.x2 = p[j-1].x; tri.y2 = p[j-1].y;
                              tri.x3 = p[j].x;   tri.y3 = p[j].y;
                              hw = card->funcs.FillTriangle( card->driver_data, card->device_data, &tri );
                              if (!hw) {
                                   D_DEBUG_AT( Core_GraphicsOps, "  -> driver returned false\n" );
                                   break;
                              }
                         }
                    }
               }
          }
          else {
               for (; i < num; i++) {
                    DFBTriangle tri = tris[i];

                    hw = card->funcs.FillTriangle( card->driver_data, card->device_data, &tri );
                    if (!hw) {
                         D_DEBUG_AT( Core_GraphicsOps, "  -> driver returned false\n" );
                         break;
                    }
               }

          }

          dfb_gfxcard_state_release( state );
     }

     if (!hw && i < num) {
          D_DEBUG_AT( Core_GraphicsOps, "  -> using software fallback\n" );

          /* Try hardware accelerated rectangle filling. */
          if (!(card->caps.flags & CCF_NOTRIEMU) &&
              dfb_gfxcard_state_check_acquire( state, DFXL_FILLRECTANGLE )) {
               for (; i < num; i++) {
                    DFBTriangle tri = tris[i];

                    dfb_sort_triangle( &tri );

                    if (tri.y3 - tri.y1 > 0)
                         fill_tri( &tri, state, true );
               }

               dfb_gfxcard_state_release( state );
          }
          else if (gAcquire( state, DFXL_FILLRECTANGLE )) {
               for (; i < num; i++) {
                    DFBTriangle tri = tris[i];

                    if (state->render_options & DSRO_MATRIX) {
                         DFB_TRANSFORM( tri.x1, tri.y1, state->matrix, state->affine_matrix );
                         DFB_TRANSFORM( tri.x2, tri.y2, state->matrix, state->affine_matrix );
                         DFB_TRANSFORM( tri.x3, tri.y3, state->matrix, state->affine_matrix );
                    }

                    dfb_sort_triangle( &tri );

                    if (tri.y3 - tri.y1 > 0)
                         fill_tri( &tri, state, false );
               }

               gRelease( state );
          }
     }

     dfb_state_unlock( state );
}

static void
fill_trap( DFBTrapezoid *trap,
           CardState    *state,
           bool          accelerated )
{
     int y, yend;
     DDA dda1    = { .xi = 0 };
     DDA dda2    = { .xi = 0 };
     int clip_x1 = state->clip.x1;
     int clip_x2 = state->clip.x2;

     D_MAGIC_ASSERT( state, CardState );

     y = trap->y1;
     yend = trap->y2;

     if (yend > state->clip.y2)
          yend = state->clip.y2;

     /* Top left to bottom left. */
     SETUP_DDA( trap->x1,                trap->y1, trap->x2,                trap->y2, dda1 );
     /* Top right to bottom right. */
     SETUP_DDA( trap->x1 + trap->w1 - 1, trap->y1, trap->x2 + trap->w2 - 1, trap->y2, dda2 );

     while (y <= yend) {
          DFBRectangle rect;

          rect.w = ABS( dda1.xi - dda2.xi );
          rect.x = MIN( dda1.xi, dda2.xi );

          if (clip_x2 < rect.x + rect.w)
               rect.w = clip_x2 - rect.x + 1;

          if (rect.w > 0) {
               if (clip_x1 > rect.x) {
                    rect.w -= (clip_x1 - rect.x);
                    rect.x = clip_x1;
               }
               rect.y = y;
               rect.h = 1;

               if (rect.w > 0 && rect.y >= state->clip.y1) {
                    if (accelerated)
                         card->funcs.FillRectangle( card->driver_data, card->device_data, &rect );
                    else
                         gFillRectangle( state, &rect );
               }
          }

          INC_DDA( dda1 );
          INC_DDA( dda2 );

          y++;
     }
}

void
dfb_gfxcard_filltrapezoids( DFBTrapezoid *traps,
                            int           num,
                            CardState    *state )
{
     bool hw = false;
     int  i  = 0;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( traps != NULL );
     D_ASSERT( num > 0 );

     D_DEBUG_AT( Core_GraphicsOps, "%s( %p [%d], %p )\n", __FUNCTION__, traps, num, state );

     /* The state is locked during graphics operations. */
     dfb_state_lock( state );

     /* Signal beginning of sequence of operations if not already done. */
     dfb_state_start_drawing( state );

     if (dfb_gfxcard_state_check_acquire( state, DFXL_FILLTRAPEZOID )) {
          if (D_FLAGS_IS_SET( card->caps.flags, CCF_CLIPPING )      ||
              D_FLAGS_IS_SET( card->caps.clip, DFXL_FILLTRAPEZOID ) ||
              (state->render_options & DSRO_MATRIX)) {
               for (; i < num; i++) {
                    DFBTrapezoid trap = traps[i];

                    hw = card->funcs.FillTrapezoid( card->driver_data, card->device_data, &trap );
                    if (!hw)
                         break;
               }

          }

          dfb_gfxcard_state_release( state );
     }

     /* Otherwise use two triangles. */
     if (!hw && i < num) {
          if (dfb_gfxcard_state_check_acquire( state, DFXL_FILLTRIANGLE )) {
               for (; i < num; i++) {
                    bool tri1_failed = true;
                    bool tri2_failed = true;

                    DFBTriangle tri1 = { traps[i].x1,                   traps[i].y1,
                                         traps[i].x1 + traps[i].w1 - 1, traps[i].y1,
                                         traps[i].x2,                   traps[i].y2 };

                    DFBTriangle tri2 = { traps[i].x1 + traps[i].w1 - 1, traps[i].y1,
                                         traps[i].x2,                   traps[i].y2,
                                         traps[i].x2 + traps[i].w2 - 1, traps[i].y2 };

                    if (D_FLAGS_IS_SET( card->caps.flags, CCF_CLIPPING )     ||
                        D_FLAGS_IS_SET( card->caps.clip, DFXL_FILLTRIANGLE ) ||
                        (state->render_options & DSRO_MATRIX)) {
                         tri1_failed = !card->funcs.FillTriangle( card->driver_data, card->device_data, &tri1 );
                         tri2_failed = !card->funcs.FillTriangle( card->driver_data, card->device_data, &tri2 );
                    }

                    if (tri1_failed || tri2_failed) {
                         dfb_gfxcard_state_release( state );

                         if (gAcquire( state, DFXL_FILLTRIANGLE )) {
                              if (state->render_options & DSRO_MATRIX) {
                                   /* Transform first triangle completely. */
                                   if (tri1_failed || tri2_failed) {
                                        DFB_TRANSFORM( tri1.x1, tri1.y1, state->matrix, state->affine_matrix );
                                        DFB_TRANSFORM( tri1.x2, tri1.y2, state->matrix, state->affine_matrix );
                                        DFB_TRANSFORM( tri1.x3, tri1.y3, state->matrix, state->affine_matrix );
                                   }

                                   /* Transform last coordinate of first triangle, and assign first ones from first. */
                                   if (tri2_failed) {
                                        DFB_TRANSFORM( tri2.x3, tri2.y3, state->matrix, state->affine_matrix );
                                        tri2.x1 = tri1.x2;
                                        tri2.y1 = tri1.y2;
                                        tri2.x2 = tri1.x3;
                                        tri2.x2 = tri1.y3;
                                   }

                                   /* Sort triangles (matrix could have rotated them). */
                                   dfb_sort_triangle( &tri1 );
                                   dfb_sort_triangle( &tri2 );
                              }

                              if (tri1_failed && (tri1.y3 - tri1.y1 > 0))
                                   fill_tri( &tri1, state, false );

                              if (tri2_failed && (tri2.y3 - tri2.y1 > 0))
                                   fill_tri( &tri2, state, false );

                              gRelease( state );
                         }

                         dfb_gfxcard_state_acquire( state, DFXL_FILLTRIANGLE );
                    }
               }

               dfb_gfxcard_state_release( state );
          }
          else if (gAcquire( state, DFXL_FILLTRIANGLE )) {
               for (; i < num; i++) {
                    DFBTrapezoid trap = traps[i];
                    dfb_sort_trapezoid( &trap );

                    if (state->render_options & DSRO_MATRIX) {
                         /* Split into triangles, for easier rotation. */
                         DFBTriangle tri1 = { trap.x1,                   traps[i].y1,
                                              trap.x1 + traps[i].w1 - 1, traps[i].y1,
                                              trap.x2,                   traps[i].y2 };

                         DFBTriangle tri2 = { trap.x1 + traps[i].w1 - 1, traps[i].y1,
                                              trap.x2,                   traps[i].y2,
                                              trap.x2 + traps[i].w2 - 1, traps[i].y2 };


                         /* Transform first triangle completely. */
                         DFB_TRANSFORM( tri1.x1, tri1.y1, state->matrix, state->affine_matrix );
                         DFB_TRANSFORM( tri1.x2, tri1.y2, state->matrix, state->affine_matrix );
                         DFB_TRANSFORM( tri1.x3, tri1.y3, state->matrix, state->affine_matrix );

                         /* Transform last coordinate of second triangle, and assign first ones from first. */
                         tri2.x1 = tri1.x2;
                         tri2.y1 = tri1.y2;
                         tri2.x2 = tri1.x3;
                         tri2.y2 = tri1.y3;
                         DFB_TRANSFORM( tri2.x3, tri2.y3, state->matrix, state->affine_matrix );

                         /* Sort triangles (matrix could have rotated them). */
                         dfb_sort_triangle( &tri1 );
                         dfb_sort_triangle( &tri2 );

                         if (tri1.y3 - tri1.y1 > 0)
                              fill_tri( &tri1, state, false );

                         if (tri2.y3 - tri2.y1 > 0)
                              fill_tri( &tri2, state, false );
                    }
                    else
                         fill_trap( &trap, state, false );
               }

               gRelease( state );
          }
     }

     dfb_state_unlock( state );
}

void
dfb_gfxcard_fillquadrangles( DFBPoint  *points,
                             int        num,
                             CardState *state )
{
     bool hw = false;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     D_MAGIC_ASSERT( state, DFBGraphicsState );
     D_ASSERT( points != NULL );

     D_DEBUG_AT( Core_GraphicsOps, "%s( %p [%d], %p )\n", __FUNCTION__, points, num, state );

     /* The state is locked during graphics operations. */
     dfb_state_lock( state );

     /* Signal beginning of sequence of operations if not already done. */
     dfb_state_start_drawing( state );

     if (dfb_gfxcard_state_check_acquire( state, DFXL_FILLQUADRANGLE )) {
          if (!D_FLAGS_IS_SET( card->caps.flags, CCF_CLIPPING ) &&
              !D_FLAGS_IS_SET( card->caps.clip, DFXL_FILLQUADRANGLE ))
               return;

          hw = card->funcs.FillQuadrangles( card->driver_data, card->device_data, points, num );

          dfb_gfxcard_state_release( state );
     }

     if (!hw) {
          if (gAcquire( state, DFXL_FILLTRIANGLE )) {
               int i;

               for (i = 0; i < num * 4; i += 4) {
                    if (state->render_options & DSRO_MATRIX) {
                         DFB_TRANSFORM( points[i+0].x, points[i+0].y, state->matrix, state->affine_matrix );
                         DFB_TRANSFORM( points[i+1].x, points[i+1].y, state->matrix, state->affine_matrix );
                         DFB_TRANSFORM( points[i+2].x, points[i+2].y, state->matrix, state->affine_matrix );
                         DFB_TRANSFORM( points[i+3].x, points[i+3].y, state->matrix, state->affine_matrix );
                    }

                    DFBTriangle tri1 = {
                         points[i+0].x, points[i+0].y,
                         points[i+1].x, points[i+1].y,
                         points[i+2].x, points[i+2].y
                    };

                    DFBTriangle tri2 = {
                         points[i+0].x, points[i+0].y,
                         points[i+2].x, points[i+2].y,
                         points[i+3].x, points[i+3].y
                    };

                    /* Sort triangles (matrix could have rotated them). */
                    dfb_sort_triangle( &tri1 );
                    dfb_sort_triangle( &tri2 );

                    fill_tri( &tri1, state, false );
                    fill_tri( &tri2, state, false );
               }

               gRelease( state );
          }
     }

     dfb_state_unlock( state );
}

void
dfb_gfxcard_fillspans( int        y,
                       DFBSpan   *spans,
                       int        num,
                       CardState *state )
{
     int i = 0;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( spans != NULL );
     D_ASSERT( num > 0 );

     D_DEBUG_AT( Core_GraphicsOps, "%s( %d, %p [%d], %p )\n", __FUNCTION__, y, spans, num, state );

     /* The state is locked during graphics operations. */
     dfb_state_lock( state );

     /* Signal beginning of sequence of operations if not already done. */
     dfb_state_start_drawing( state );

     if (dfb_gfxcard_state_check_acquire( state, DFXL_FILLRECTANGLE )) {
          if (card->funcs.BatchFill) {
               unsigned int done = 0;

               do {
                    int           real_num;
                    DFBRectangle *rects;

                    if (num > 256) {
                         rects = D_MALLOC( sizeof(DFBRectangle) * num );
                         if (!rects) {
                              D_OOM();
                              break;
                         }
                    }
                    else
                         rects = alloca( sizeof(DFBRectangle) * num );

                    for (real_num = 0; i < num; i++) {
                         rects[real_num].x = spans[i].x;
                         rects[real_num].y = y + i;
                         rects[real_num].w = spans[i].w;
                         rects[real_num].h = 1;

                         if (rects[real_num].w > card->limits.dst_max.w ||
                             rects[real_num].h > card->limits.dst_max.h) {
                              if (!dfb_clip_rectangle( &state->clip, &rects[real_num] ))
                                   continue;

                              if (rects[real_num].w > card->limits.dst_max.w ||
                                  rects[real_num].h > card->limits.dst_max.h)
                                   continue;
                         }
                         else if (!D_FLAGS_IS_SET( card->caps.flags, CCF_CLIPPING )      &&
                                  !D_FLAGS_IS_SET( card->caps.clip, DFXL_FILLRECTANGLE ) &&
                                  !dfb_clip_rectangle( &state->clip, &rects[real_num] ))
                              continue;

                         real_num++;
                    }

                    if (card->funcs.BatchFill( card->driver_data, card->device_data, &rects[0], real_num, &done ))
                         i = num;
                    else
                         i = done;

                    if (num > 256)
                         D_FREE( rects );
               } while (0);
          }

          for (; i < num; i++) {
               DFBRectangle rect = { spans[i].x, y + i, spans[i].w, 1 };

               if (rect.w > card->limits.dst_max.w || rect.h > card->limits.dst_max.h) {
                    if (!dfb_clip_rectangle( &state->clip, &rect ))
                         continue;

                    if (rect.w > card->limits.dst_max.w || rect.h > card->limits.dst_max.h)
                         break;
               }
               else if (!D_FLAGS_IS_SET( card->caps.flags, CCF_CLIPPING )      &&
                        !D_FLAGS_IS_SET( card->caps.clip, DFXL_FILLRECTANGLE ) &&
                        !dfb_clip_rectangle( &state->clip, &rect ))
                         continue;

               if (!card->funcs.FillRectangle( card->driver_data, card->device_data, &rect ))
                    break;
          }

          dfb_gfxcard_state_release( state );
     }

     if (i < num) {
          if (gAcquire( state, DFXL_FILLRECTANGLE )) {
               for (; i < num; i++) {
                    DFBRectangle rect = { spans[i].x, y + i, spans[i].w, 1 };

                    if (state->render_options & DSRO_MATRIX) {
                         if (state->matrix[1] == 0 && state->matrix[3] == 0) {
                              int x1, y1, x2, y2;

                              x1 = rect.x;      y1 = rect.y;
                              x2 = x1 + rect.w; y2 = y1 + rect.h;
                              DFB_TRANSFORM( x1, y1, state->matrix, state->affine_matrix );
                              DFB_TRANSFORM( x2, y2, state->matrix, state->affine_matrix );

                              if (x1 < x2) {
                                   rect.x = x1;
                                   rect.w = x2 - x1;
                              }
                              else {
                                   rect.x = x2;
                                   rect.w = x1 - x2;
                              }
                              if (y1 < y2) {
                                   rect.y = y1;
                                   rect.h = y2 - y1;
                              }
                              else {
                                   rect.y = y2;
                                   rect.h = y1 - y2;
                              }

                              if (dfb_clip_rectangle( &state->clip, &rect ))
                                   gFillRectangle( state, &rect );
                         }
                         else {
                              DFBTriangle tri;

                              tri.x1 = rect.x;          tri.y1 = rect.y;
                              tri.x2 = rect.x + rect.w; tri.y2 = rect.y;
                              tri.x3 = rect.x + rect.w; tri.y3 = rect.y + rect.h;
                              DFB_TRANSFORM( tri.x1, tri.y1, state->matrix, state->affine_matrix );
                              DFB_TRANSFORM( tri.x2, tri.y2, state->matrix, state->affine_matrix );
                              DFB_TRANSFORM( tri.x3, tri.y3, state->matrix, state->affine_matrix );

                              dfb_sort_triangle( &tri );
                              if (tri.y3 - tri.y1 > 0)
                                   fill_tri( &tri, state, false );

                              tri.x1 = rect.x;          tri.y1 = rect.y;
                              tri.x2 = rect.x + rect.w; tri.y2 = rect.y + rect.h;
                              tri.x3 = rect.x;          tri.y3 = rect.y + rect.h;
                              DFB_TRANSFORM( tri.x1, tri.y1, state->matrix, state->affine_matrix );
                              DFB_TRANSFORM( tri.x2, tri.y2, state->matrix, state->affine_matrix );
                              DFB_TRANSFORM( tri.x3, tri.y3, state->matrix, state->affine_matrix );

                              dfb_sort_triangle( &tri );
                              if (tri.y3 - tri.y1 > 0)
                                   fill_tri( &tri, state, false );
                         }
                    }
                    else {
                         if (dfb_clip_rectangle( &state->clip, &rect ))
                              gFillRectangle( state, &rect );
                    }
               }

               gRelease( state );
          }
     }

     dfb_state_unlock( state );
}

void
dfb_gfxcard_draw_mono_glyphs( const void                   *glyph[],
                              const DFBMonoGlyphAttributes *attributes,
                              const DFBPoint               *points,
                              unsigned int                  num,
                              CardState                    *state )
{
     int i;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( (glyph != NULL) && (attributes != NULL) && (points != NULL) );

     D_DEBUG_AT( Core_GraphicsOps, "%s( %p, %p, %p, %p )\n", __FUNCTION__, glyph, attributes, points, state );

     /* The state is locked during graphics operations. */
     dfb_state_lock( state );

     /* Signal beginning of sequence of operations if not already done. */
     dfb_state_start_drawing( state );

     if (dfb_gfxcard_state_check_acquire( state, DFXL_DRAWMONOGLYPH )) {
          for (i = 0; i < num; i++) {
               const DFBMonoGlyphAttributes *attri = &attributes[i];

               card->funcs.DrawMonoGlyph( card->driver_data, card->device_data, glyph[i], attri->width, attri->height,
                                          attri->rowbyte, attri->bitoffset, points[i].x, points[i].y,
                                          attri->fgcolor, attri->bgcolor, attri->hzoom, attri->vzoom );
          }

          dfb_gfxcard_state_release( state );
     }

     dfb_state_unlock( state );
}

static void
GenefxVertexAffine_Transform( GenefxVertexAffine *v,
                              unsigned int        num,
                              s32                 matrix[9],
                              bool                affine )
{
     unsigned int i;

     if (affine) {
          for (i = 0; i < num; i++) {
               int _x, _y;

               _x = ((v[i].x) * matrix[0] + (v[i].y) * matrix[1] + matrix[2]) / 0x10000;
               _y = ((v[i].x) * matrix[3] + (v[i].y) * matrix[4] + matrix[5]) / 0x10000;

               v[i].x = _x;
               v[i].y = _y;
          }
     }
     else {
          for (i = 0; i < num; i++) {
               int _x, _y, _w;

               _x = ((v[i].x) * matrix[0] + (v[i].y) * matrix[1] + matrix[2]);
               _y = ((v[i].x) * matrix[3] + (v[i].y) * matrix[4] + matrix[5]);
               _w = ((v[i].x) * matrix[6] + (v[i].y) * matrix[7] + matrix[8]);
               if (!_w) {
                    _x = (_x < 0) ? -0x7fffffff : 0x7fffffff;
                    _y = (_y < 0) ? -0x7fffffff : 0x7fffffff;
               }
               else {
                    _x /= _w;
                    _y /= _w;
               }

               v[i].x = _x;
               v[i].y = _y;
          }
     }
}

static void
dfb_gfxcard_blit_locked( DFBRectangle *rect,
                         int           dx,
                         int           dy,
                         CardState    *state )
{
     bool         hw    = false;
     DFBRectangle drect = { dx, dy, rect->w, rect->h };

     DFBSurfaceBlittingFlags blittingflags;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( state->source != NULL );
     D_ASSERT( rect != NULL );
     D_ASSERT( rect->x >= 0 );
     D_ASSERT( rect->y >= 0 );
     D_ASSERT( rect->x < state->source->config.size.w );
     D_ASSERT( rect->y < state->source->config.size.h );
     D_ASSERT( rect->x + rect->w - 1 < state->source->config.size.w );
     D_ASSERT( rect->y + rect->h - 1 < state->source->config.size.h );

     blittingflags = state->blittingflags;
     dfb_simplify_blittingflags( &blittingflags );

     if (blittingflags & DSBLIT_ROTATE90)
          D_UTIL_SWAP( drect.w, drect.h );

     D_DEBUG_AT( Core_GraphicsOps, "%s( %4d,%4d-%4dx%4d -> %4d,%4d-%4dx%4d, %p )\n", __FUNCTION__,
                 DFB_RECTANGLE_VALS( rect ), DFB_RECTANGLE_VALS( &drect ), state );

     /* Signal beginning of sequence of operations if not already done. */
     dfb_state_start_drawing( state );

     if (!(state->render_options & DSRO_MATRIX) &&
         !dfb_clip_blit_precheck( &state->clip, drect.w, drect.h, drect.x, drect.y ))
          return;

     if (dfb_gfxcard_state_check_acquire( state, DFXL_BLIT )) {
          if (!D_FLAGS_IS_SET( card->caps.flags, CCF_CLIPPING ) &&
              !D_FLAGS_IS_SET( card->caps.clip, DFXL_BLIT ))
               dfb_clip_blit_flipped_rotated( &state->clip, rect, &drect, blittingflags );

          hw = card->funcs.Blit( card->driver_data, card->device_data, rect, drect.x, drect.y );

          dfb_gfxcard_state_release( state );
     }

     if (!hw) {
          /* Use software fallback. */
          if (!(state->render_options & DSRO_MATRIX)) {
               if (gAcquire( state, DFXL_BLIT )) {
                    dfb_clip_blit_flipped_rotated( &state->clip, rect, &drect, blittingflags );

                    gBlit( state, rect, drect.x, drect.y );

                    gRelease( state );
               }
          }
          else if (state->matrix[0] == 0x10000 && state->matrix[1] == 0 &&
                   state->matrix[3] == 0       && state->matrix[4] == 0x10000) {
               state->render_options &= ~DSRO_MATRIX;
               state->modified       |= SMF_RENDER_OPTIONS;

               dfb_gfxcard_blit( rect,
                                 dx + ((state->matrix[2] + 0x8000) >> 16),
                                 dy + ((state->matrix[5] + 0x8000) >> 16), state );

               state->render_options |= DSRO_MATRIX;
               state->modified       |= SMF_RENDER_OPTIONS;
          }
          else {
               if (state->matrix[0] < 0  || state->matrix[1] != 0 ||
                   state->matrix[3] != 0 || state->matrix[4] < 0  ||
                   state->matrix[6] != 0 || state->matrix[7] != 0) {
                    if (gAcquire( state, DFXL_TEXTRIANGLES )) {
                         GenefxVertexAffine v[4];

                         v[0].x = dx;
                         v[0].y = dy;
                         v[0].s = rect->x * 0x10000;
                         v[0].t = rect->y * 0x10000;

                         v[1].x = dx + rect->w - 1;
                         v[1].y = dy;
                         v[1].s = (rect->x + rect->w - 1) * 0x10000;
                         v[1].t = v[0].t;

                         v[2].x = dx + rect->w - 1;
                         v[2].y = dy + rect->h - 1;
                         v[2].s = v[1].s;
                         v[2].t = (rect->y + rect->h - 1) * 0x10000;

                         v[3].x = dx;
                         v[3].y = dy + rect->h - 1;
                         v[3].s = v[0].s;
                         v[3].t = v[2].t;

                         GenefxVertexAffine_Transform( v, 4, state->matrix, state->affine_matrix );

                         Genefx_TextureTrianglesAffine( state, v, 4, DTTF_FAN, &state->clip );

                         gRelease( state );
                    }
               }
               else if (gAcquire( state, DFXL_STRETCHBLIT )) {
                    int x1, y1, x2, y2;

                    x1 = dx;           y1 = dy;
                    x2 = dx + rect->w; y2 = dy + rect->h;
                    DFB_TRANSFORM( x1, y1, state->matrix, state->affine_matrix );
                    DFB_TRANSFORM( x2, y2, state->matrix, state->affine_matrix );

                    drect = (DFBRectangle) { x1, y1, x2 - x1, y2 - y1 };
                    if (dfb_clip_blit_precheck( &state->clip, drect.w, drect.h, drect.x, drect.y ))
                         gStretchBlit( state, rect, &drect );

                    gRelease( state );
               }
          }
     }
}

void
dfb_gfxcard_blit( DFBRectangle *rect,
                  int           dx,
                  int           dy,
                  CardState    *state )
{
     /* The state is locked during graphics operations. */
     dfb_state_lock( state );
     dfb_gfxcard_blit_locked( rect, dx, dy, state );
     dfb_state_unlock( state );
}

static void
clip_blits( const DFBRegion         *clip,
            const DFBRectangle      *rects,
            const DFBPoint          *points,
            unsigned int             num,
            DFBSurfaceBlittingFlags  flags,
            DFBRectangle            *ret_rects,
            DFBPoint                *ret_points,
            unsigned int            *ret_num )
{
     unsigned int i;
     unsigned int clipped_num = 0;

     DFB_REGION_ASSERT( clip );
     D_ASSERT( rects != NULL );
     D_ASSERT( points != NULL );
     D_ASSERT( ret_rects != NULL );
     D_ASSERT( ret_points != NULL );
     D_ASSERT( ret_num != NULL );
     D_ASSERT( !(flags & (DSBLIT_ROTATE270 | DSBLIT_ROTATE180)) );

     for (i = 0; i < num; i++) {
          DFBRectangle drect = { points[i].x, points[i].y, rects[i].w, rects[i].h };

          if (flags & (DSBLIT_ROTATE90))
               D_UTIL_SWAP( drect.w, drect.h );

          if (dfb_clip_blit_precheck( clip, drect.w, drect.h, drect.x, drect.y )) {
               ret_rects[clipped_num] = rects[i];

               dfb_clip_blit_flipped_rotated( clip, &ret_rects[clipped_num], &drect, flags );

               ret_points[clipped_num].x = drect.x;
               ret_points[clipped_num].y = drect.y;

               clipped_num++;
          }
     }

     *ret_num = clipped_num;
}

void
dfb_gfxcard_batchblit( DFBRectangle *rects,
                       DFBPoint     *points,
                       int           num,
                       CardState    *state )
{
     unsigned int i = 0;

     DFBSurfaceBlittingFlags blittingflags;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( rects != NULL );
     D_ASSERT( points != NULL );
     D_ASSERT( num > 0 );

     D_DEBUG_AT( Core_GraphicsOps, "%s( %p, %p [%d], %p )\n", __FUNCTION__, rects, points, num, state );

     blittingflags = state->blittingflags;
     dfb_simplify_blittingflags( &blittingflags );

     /* The state is locked during graphics operations. */
     dfb_state_lock( state );

     /* Signal beginning of sequence of operations if not already done. */
     dfb_state_start_drawing( state );

     if (dfb_gfxcard_state_check_acquire( state, DFXL_BLIT )) {
          if (card->funcs.BatchBlit) {
               unsigned int done = 0;

               if (!D_FLAGS_IS_SET( card->caps.flags, CCF_CLIPPING ) &&
                   !D_FLAGS_IS_SET( card->caps.clip, DFXL_BLIT )) {
                    unsigned int  clipped_num;
                    DFBRectangle *clipped_rects;
                    DFBPoint     *clipped_points;

                    if (num > 256) {
                         clipped_rects = D_MALLOC( sizeof(DFBRectangle) * num );
                         if (!clipped_rects) {
                              D_OOM();
                         }
                         else {
                              clipped_points = D_MALLOC( sizeof(DFBPoint) * num );
                              if (!clipped_points) {
                                   D_OOM();
                                   D_FREE( clipped_rects );
                              }
                              else {
                                   clip_blits( &state->clip, rects, points, num, blittingflags,
                                               clipped_rects, clipped_points, &clipped_num );

                                   /* The driver has to reject all or none. */
                                   if (card->funcs.BatchBlit( card->driver_data, card->device_data,
                                                              clipped_rects, clipped_points, clipped_num, &done ))
                                        i = num;
                                   else
                                        i = done;

                                   D_FREE( clipped_points );
                                   D_FREE( clipped_rects );
                              }
                         }
                    }
                    else {
                         clipped_rects  = alloca( sizeof(DFBRectangle) * num );
                         clipped_points = alloca( sizeof(DFBPoint) * num );

                         clip_blits( &state->clip, rects, points, num, blittingflags,
                                     clipped_rects, clipped_points, &clipped_num );

                         /* The driver has to reject all or none. */
                         if (card->funcs.BatchBlit( card->driver_data, card->device_data,
                                                    clipped_rects, clipped_points, clipped_num, &done ))
                              i = num;
                         else
                              i = done;
                    }
               }
               else {
                    /* The driver has to reject all or none. */
                    if (card->funcs.BatchBlit( card->driver_data, card->device_data, rects, points, num, &done ))
                         i = num;
                    else
                         i = done;
               }
          }
          else {
               for (; i < num; i++) {
                    DFBRectangle drect = { points[i].x, points[i].y, rects[i].w, rects[i].h };

                    if (blittingflags & DSBLIT_ROTATE90)
                         D_UTIL_SWAP( drect.w, drect.h );

                    if ((state->render_options & DSRO_MATRIX) ||
                        dfb_clip_blit_precheck( &state->clip, drect.w, drect.h, drect.x, drect.y )) {
                         DFBRectangle srect = rects[i];

                         if (!D_FLAGS_IS_SET( card->caps.flags, CCF_CLIPPING ) &&
                             !D_FLAGS_IS_SET( card->caps.clip, DFXL_BLIT ))
                              dfb_clip_blit_flipped_rotated( &state->clip, &srect, &drect, blittingflags );

                         if (!card->funcs.Blit( card->driver_data, card->device_data, &srect, drect.x, drect.y ))
                              break;
                    }
               }
          }

          dfb_gfxcard_state_release( state );
     }

     if (i < num) {
          if (state->render_options & DSRO_MATRIX) {
               if (state->matrix[0] < 0  || state->matrix[1] != 0 ||
                   state->matrix[3] != 0 || state->matrix[4] < 0  ||
                   state->matrix[6] != 0 || state->matrix[7] != 0) {
                    if (gAcquire( state, DFXL_TEXTRIANGLES )) {
                         for (; i < num; i++) {
                              GenefxVertexAffine v[4];

                              v[0].x = points[i].x;
                              v[0].y = points[i].y;
                              v[0].s = rects[i].x * 0x10000;
                              v[0].t = rects[i].y * 0x10000;

                              v[1].x = points[i].x + rects[i].w - 1;
                              v[1].y = points[i].y;
                              v[1].s = (rects[i].x + rects[i].w - 1) * 0x10000;
                              v[1].t = v[0].t;

                              v[2].x = points[i].x + rects[i].w - 1;
                              v[2].y = points[i].y + rects[i].h - 1;
                              v[2].s = v[1].s;
                              v[2].t = (rects[i].y + rects[i].h - 1) * 0x10000;

                              v[3].x = points[i].x;
                              v[3].y = points[i].y + rects[i].h - 1;
                              v[3].s = v[0].s;
                              v[3].t = v[2].t;

                              GenefxVertexAffine_Transform( v, 4, state->matrix, state->affine_matrix );

                              Genefx_TextureTrianglesAffine( state, v, 4, DTTF_FAN, &state->clip );
                         }

                         gRelease( state );
                    }
               }
               else if (gAcquire( state, DFXL_STRETCHBLIT )) {
                    for (; i < num; i++) {
                         int          x1, y1, x2, y2;
                         DFBRectangle drect;

                         x1 = points[i].x;     y1 = points[i].y;
                         x2 = x1 + rects[i].w; y2 = y1 + rects[i].h;
                         DFB_TRANSFORM( x1, y1, state->matrix, state->affine_matrix );
                         DFB_TRANSFORM( x2, y2, state->matrix, state->affine_matrix );

                         drect = (DFBRectangle) { x1, y1, x2 - x1, y2 - y1 };
                         if (dfb_clip_blit_precheck( &state->clip, drect.w, drect.h, drect.x, drect.y ))
                              gStretchBlit( state, &rects[i], &drect );
                    }

                    gRelease( state );
               }
          }
          else {
               if (gAcquire( state, DFXL_BLIT )) {
                    for (; i < num; i++) {
                         DFBRectangle drect = { points[i].x, points[i].y, rects[i].w, rects[i].h };

                         if (blittingflags & DSBLIT_ROTATE90)
                              D_UTIL_SWAP( drect.w, drect.h );

                         if (dfb_clip_blit_precheck( &state->clip, drect.w, drect.h, drect.x, drect.y )) {
                              DFBRectangle srect = rects[i];

                              dfb_clip_blit_flipped_rotated( &state->clip, &srect, &drect, blittingflags );
                              gBlit( state, &srect, drect.x, drect.y );
                         }
                    }

                    gRelease( state );
               }
          }
     }

     dfb_state_unlock( state );
}

void
dfb_gfxcard_batchblit2( DFBRectangle *rects,
                        DFBPoint     *points,
                        DFBPoint     *points2,
                        int           num,
                        CardState    *state )
{
     int i = 0;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( rects != NULL );
     D_ASSERT( points != NULL );
     D_ASSERT( points2 != NULL );
     D_ASSERT( num > 0 );

     D_DEBUG_AT( Core_GraphicsOps, "%s( %p, %p, %p [%d], %p )\n", __FUNCTION__, rects, points, points2, num, state );

     /* The state is locked during graphics operations. */
     dfb_state_lock( state );

     /* Signal beginning of sequence of operations if not already done. */
     dfb_state_start_drawing( state );

     if (dfb_gfxcard_state_check_acquire( state, DFXL_BLIT2 )) {
          for (; i < num; i++) {
               if ((state->render_options & DSRO_MATRIX) ||
                   dfb_clip_blit_precheck( &state->clip, rects[i].w, rects[i].h, points[i].x, points[i].y )) {
                    int dx = points[i].x;
                    int dy = points[i].y;

                    if (!D_FLAGS_IS_SET( card->caps.flags, CCF_CLIPPING ) &&
                        !D_FLAGS_IS_SET( card->caps.clip, DFXL_BLIT2 )) {
                         dfb_clip_blit( &state->clip, &rects[i], &dx, &dy );

                         points2[i].x += dx - points[i].x;
                         points2[i].y += dy - points[i].y;
                    }

                    if (!card->funcs.Blit2( card->driver_data, card->device_data, &rects[i], dx, dy,
                                            points2[i].x, points2[i].y ))
                         break;
               }
          }

          dfb_gfxcard_state_release( state );
     }

     if (i < num) {
          D_UNIMPLEMENTED();

          for (; i < num; i++) {
               D_DEBUG_AT( Core_GraphicsOps, "  -> rects[%d]    %4d,%4d-%4dx%4d\n", i,
                           DFB_RECTANGLE_VALS( &rects[i] ) );
               D_DEBUG_AT( Core_GraphicsOps, "  -> points[%d]   %4d,%4d\n", i, points[i].x, points[i].y );
               D_DEBUG_AT( Core_GraphicsOps, "  -> points2[%d]  %4d,%4d\n", i, points2[i].x, points2[i].y );

               if ((state->render_options & DSRO_MATRIX) ||
                   dfb_clip_blit_precheck( &state->clip, rects[i].w, rects[i].h, points[i].x, points[i].y )) {
                    int dx = points[i].x;
                    int dy = points[i].y;

                    dfb_clip_blit( &state->clip, &rects[i], &dx, &dy );

                    points2[i].x += dx - points[i].x;
                    points2[i].y += dy - points[i].y;

                    D_DEBUG_AT( Core_GraphicsOps, "  -> rects[%d]    %4d,%4d-%4dx%4d\n", i,
                                DFB_RECTANGLE_VALS( &rects[i] ) );
                    D_DEBUG_AT( Core_GraphicsOps, "  -> points[%d]   %4d,%4d\n", i, dx, dy );
                    D_DEBUG_AT( Core_GraphicsOps, "  -> points2[%d]  %4d,%4d\n", i, points2[i].x, points2[i].y );
               }
          }
     }

     dfb_state_unlock( state );
}

void
dfb_gfxcard_batchstretchblit( DFBRectangle *srects,
                              DFBRectangle *drects,
                              unsigned int  num,
                              CardState    *state )
{
     int  i;
     bool need_clip;
     bool acquired = false;

     DFBSurfaceBlittingFlags blittingflags;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( srects != NULL );
     D_ASSERT( drects != NULL );
     D_ASSERT( num > 0 );

     D_DEBUG_AT( Core_GraphicsOps, "%s( %p )\n", __FUNCTION__, state );

     for (i = 0; i < num; ++i)
          D_DEBUG_AT( Core_GraphicsOps, "  -> %4d,%4d-%4dx%4d -> %4d,%4d-%4dx%4d\n",
                      DFB_RECTANGLE_VALS( &srects[i] ), DFB_RECTANGLE_VALS( &drects[i] ) );

     blittingflags = state->blittingflags;
     dfb_simplify_blittingflags( &blittingflags );

     /* The state is locked during graphics operations. */
     dfb_state_lock( state );

     /* Signal beginning of sequence of operations if not already done. */
     dfb_state_start_drawing( state );

     need_clip = (!D_FLAGS_IS_SET( card->caps.flags, CCF_CLIPPING ) &&
                  !D_FLAGS_IS_SET( card->caps.clip, DFXL_STRETCHBLIT ));

     for (i = 0; i < num; ++i) {
          DFBRectangle *srect = &srects[i];
          DFBRectangle *drect = &drects[i];

          if (!acquired) {
               if (!dfb_gfxcard_state_check_acquire( state, DFXL_STRETCHBLIT ))
                    break;

               acquired = true;
          }

          if ((srect->w == drect->w && srect->h == drect->h) ||
              ((state->blittingflags & DSBLIT_ROTATE90) && (srect->w == drect->h && srect->h == drect->w))) {
               dfb_gfxcard_state_release( state );
               acquired = false;
               dfb_gfxcard_blit_locked( srect, drect->x, drect->y, state );
               continue;
          }

          if (!(state->render_options & DSRO_MATRIX) &&
              !dfb_clip_blit_precheck( &state->clip, drect->w, drect->h, drect->x, drect->y ))
               continue;

          if (need_clip)
               dfb_clip_stretchblit( &state->clip, srect, drect );

          if (!card->funcs.StretchBlit( card->driver_data, card->device_data, srect, drect ))
               break;
     }

     if (acquired)
          dfb_gfxcard_state_release( state );

     if (i < num) {
          if ((state->render_options & DSRO_MATRIX) &&
              (state->matrix[0] < 0  || state->matrix[1] != 0 ||
               state->matrix[3] != 0 || state->matrix[4] < 0  ||
               state->matrix[6] != 0 || state->matrix[7] != 0)) {
               if (gAcquire( state, DFXL_TEXTRIANGLES )) {
                    for (; i < num; ++i) {
                         DFBRectangle *srect = &srects[i];
                         DFBRectangle *drect = &drects[i];

                         GenefxVertexAffine v[4];

                         v[0].x = drect->x;
                         v[0].y = drect->y;
                         v[0].s = srect->x * 0x10000;
                         v[0].t = srect->y * 0x10000;

                         v[1].x = drect->x + drect->w - 1;
                         v[1].y = drect->y;
                         v[1].s = (srect->x + srect->w - 1) * 0x10000;
                         v[1].t = v[0].t;

                         v[2].x = drect->x + drect->w - 1;
                         v[2].y = drect->y + drect->h - 1;
                         v[2].s = v[1].s;
                         v[2].t = (srect->y + srect->h - 1) * 0x10000;

                         v[3].x = drect->x;
                         v[3].y = drect->y + drect->h - 1;
                         v[3].s = v[0].s;
                         v[3].t = v[2].t;

                         GenefxVertexAffine_Transform( v, 4, state->matrix, state->affine_matrix );

                         Genefx_TextureTrianglesAffine( state, v, 4, DTTF_FAN, &state->clip );
                    }

                    gRelease( state );
               }
          }
          else if (gAcquire( state, DFXL_STRETCHBLIT )) {
               for (; i < num; ++i) {
                    DFBRectangle *srect = &srects[i];
                    DFBRectangle *drect = &drects[i];

                    if (state->render_options & DSRO_MATRIX) {
                         int x1, y1, x2, y2;

                         x1 = drect->x;      y1 = drect->y;
                         x2 = x1 + drect->w; y2 = y1 + drect->h;
                         DFB_TRANSFORM( x1, y1, state->matrix, state->affine_matrix );
                         DFB_TRANSFORM( x2, y2, state->matrix, state->affine_matrix );
                         drect->x = x1;    drect->y = y1;
                         drect->w = x2 - x1; drect->h = y2 - y1;
                    }

                    if (!dfb_clip_blit_precheck( &state->clip, drect->w, drect->h, drect->x, drect->y ))
                         continue;

                    gStretchBlit( state, srect, drect );
               }

               gRelease( state );
          }
     }

     dfb_state_unlock( state );
}

void
dfb_gfxcard_stretchblit( DFBRectangle *srect,
                         DFBRectangle *drect,
                         CardState    *state )
{
     dfb_gfxcard_batchstretchblit( srect, drect, 1, state );
}

void
dfb_gfxcard_tileblit( DFBRectangle *rect,
                      int           dx1,
                      int           dy1,
                      int           dx2,
                      int           dy2,
                      CardState    *state )
{
     int           x, y;
     int           odx;
     DFBRectangle  srect;
     DFBRegion    *clip;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( rect != NULL );
     D_ASSERT( rect->w >= 1 );
     D_ASSERT( rect->h >= 1 );

     D_DEBUG_AT( Core_GraphicsOps, "%s( %4d,%4d-%4d,%4d, %p )\n", __FUNCTION__, dx1, dy1, dx2, dy2, state );

     /* The state is locked during graphics operations. */
     dfb_state_lock( state );

     /* Signal beginning of sequence of operations if not already done. */
     dfb_state_start_drawing( state );

     clip = &state->clip;

     /* Check if anything is drawn at all. */
     if (!(state->render_options & DSRO_MATRIX) &&
         !dfb_clip_blit_precheck( clip, dx2 - dx1 + 1, dy2 - dy1 + 1, dx1, dy1 )) {
          dfb_state_unlock( state );
          return;
     }

     /* Remove clipped tiles. */
     if (dx1 < clip->x1) {
          int outer = clip->x1 - dx1;

          dx1 += outer - (outer % rect->w);
     }

     if (dy1 < clip->y1) {
          int outer = clip->y1 - dy1;

          dy1 += outer - (outer % rect->h);
     }

     if (dx2 > clip->x2) {
          int outer = clip->x2 - dx2;

          dx2 -= outer - (outer % rect->w);
     }

     if (dy2 > clip->y2) {
          int outer = clip->y2 - dy2;

          dy2 -= outer - (outer % rect->h);
     }

     odx = dx1;

     if (dfb_gfxcard_state_check_acquire( state, DFXL_BLIT )) {
          bool hw = true;

          for (; dy1 < dy2; dy1 += rect->h) {
               for (; dx1 < dx2; dx1 += rect->w) {

                    if (!dfb_clip_blit_precheck( clip, rect->w, rect->h, dx1, dy1 ))
                         continue;

                    x = dx1;
                    y = dy1;
                    srect = *rect;

                    if (!D_FLAGS_IS_SET( card->caps.flags, CCF_CLIPPING ) &&
                        !D_FLAGS_IS_SET( card->caps.clip, DFXL_BLIT ))
                         dfb_clip_blit( clip, &srect, &x, &y );

                    hw = card->funcs.Blit( card->driver_data, card->device_data, &srect, x, y );
                    if (!hw)
                         break;
               }

               if (!hw)
                    break;

               dx1 = odx;
          }

          dfb_gfxcard_state_release( state );
     }

     if (dy1 < dy2) {
          if (state->render_options & DSRO_MATRIX) {
               if (state->matrix[0] < 0  || state->matrix[1] != 0 ||
                   state->matrix[3] != 0 || state->matrix[4] < 0  ||
                   state->matrix[6] != 0 || state->matrix[7] != 0) {
                    if (gAcquire( state, DFXL_TEXTRIANGLES )) {
                         /* Build mesh. */
                         for (; dy1 < dy2; dy1 += rect->h) {
                              for (; dx1 < dx2; dx1 += rect->w) {
                                   GenefxVertexAffine v[4];

                                   v[0].x = dx1;
                                   v[0].y = dy1;
                                   v[0].s = rect->x * 0x10000;
                                   v[0].t = rect->y * 0x10000;

                                   v[1].x = dx1 + rect->w - 1;
                                   v[1].y = dy1;
                                   v[1].s = (rect->x + rect->w - 1) * 0x10000;
                                   v[1].t = v[0].t;

                                   v[2].x = dx1 + rect->w - 1;
                                   v[2].y = dy1 + rect->h - 1;
                                   v[2].s = v[1].s;
                                   v[2].t = (rect->y + rect->h - 1) * 0x10000;

                                   v[3].x = dx1;
                                   v[3].y = dy1 + rect->h - 1;
                                   v[3].s = v[0].s;
                                   v[3].t = v[2].t;

                                   GenefxVertexAffine_Transform( v, 4, state->matrix, state->affine_matrix );

                                   Genefx_TextureTrianglesAffine( state, v, 4, DTTF_FAN, &state->clip );
                              }

                              dx1 = odx;
                         }

                         gRelease( state );
                    }
               }
               else if (gAcquire( state, DFXL_STRETCHBLIT )) {
                    for (; dy1 < dy2; dy1 += rect->h) {
                         for (; dx1 < dx2; dx1 += rect->w) {
                              int          x1, y1, x2, y2;
                              DFBRectangle drect;

                              x1 = dx1;           y1 = dy1;
                              x2 = dx1 + rect->w; y2 = dy1 + rect->h;
                              DFB_TRANSFORM( x1, y1, state->matrix, state->affine_matrix );
                              DFB_TRANSFORM( x2, y2, state->matrix, state->affine_matrix );

                              drect = (DFBRectangle) { x1, y1, x2 - x1, y2 - y1 };
                              if (dfb_clip_blit_precheck( &state->clip, drect.w, drect.h, drect.x, drect.y ))
                                   gStretchBlit( state, rect, &drect );
                         }

                         dx1 = odx;
                    }

                    gRelease( state );
               }
          }
          else {
               if (gAcquire( state, DFXL_BLIT )) {
                    for (; dy1 < dy2; dy1 += rect->h) {
                         for (; dx1 < dx2; dx1 += rect->w) {
                              if (!dfb_clip_blit_precheck( clip, rect->w, rect->h, dx1, dy1 ))
                                   continue;

                              x = dx1;
                              y = dy1;
                              srect = *rect;

                              dfb_clip_blit( clip, &srect, &x, &y );

                              gBlit( state, &srect, x, y );
                         }

                         dx1 = odx;
                    }

                    gRelease( state );
               }
          }
     }

     dfb_state_unlock( state );
}

void
dfb_gfxcard_texture_triangles( DFBVertex            *vertices,
                               int                   num,
                               DFBTriangleFormation  formation,
                               CardState            *state )
{
     bool hw = false;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( vertices != NULL );
     D_ASSERT( num >= 3 );

     D_DEBUG_AT( Core_GraphicsOps, "%s( %p [%d], %s, %p )\n", __FUNCTION__, vertices, num,
                 (formation == DTTF_LIST)  ? "LIST"  :
                 (formation == DTTF_STRIP) ? "STRIP" :
                 (formation == DTTF_FAN)   ? "FAN"   : "unknown formation", state );

     /* The state is locked during graphics operations. */
     dfb_state_lock( state );

     /* Signal beginning of sequence of operations if not already done. */
     dfb_state_start_drawing( state );

     if ((D_FLAGS_IS_SET( card->caps.flags, CCF_CLIPPING ) ||
          D_FLAGS_IS_SET( card->caps.clip, DFXL_TEXTRIANGLES )) &&
         dfb_gfxcard_state_check_acquire( state, DFXL_TEXTRIANGLES )) {
          hw = card->funcs.TextureTriangles( card->driver_data, card->device_data, vertices, num, formation );

          dfb_gfxcard_state_release( state );
     }

     if (!hw) {
          if (gAcquire( state, DFXL_TEXTRIANGLES )) {
               int                i;
               GenefxVertexAffine v[num];

               /* Convert vertices. */
               for (i = 0; i < num; i++) {
                    v[i].x = vertices[i].x;
                    v[i].y = vertices[i].y;
                    v[i].s = vertices[i].s * state->source->config.size.w * 0x10000;
                    v[i].t = vertices[i].t * state->source->config.size.h * 0x10000;
               }

               Genefx_TextureTrianglesAffine( state, v, num, formation, &state->clip );

               gRelease( state );
          }
     }

     dfb_state_unlock( state );
}

static void
font_state_prepare( CardState   *state,
                    CardState   *backup,
                    CoreFont    *font,
                    CoreSurface *surface,
                    bool         set_blend )
{
     if (state->blittingflags != DSBLIT_INDEX_TRANSLATION) {
          DFBSurfaceBlittingFlags flags = font->blittingflags;

          backup->blittingflags = state->blittingflags;
          backup->src_blend     = state->src_blend;
          backup->dst_blend     = state->dst_blend;

          /* Additional blending. */
          if ((state->drawingflags & DSDRAW_BLEND) && (state->color.a != 0xff))
               flags |= DSBLIT_BLEND_COLORALPHA;

          if (state->drawingflags & DSDRAW_DST_COLORKEY)
               flags |= DSBLIT_DST_COLORKEY;

          if (state->drawingflags & DSDRAW_XOR)
               flags |= DSBLIT_XOR;

          if (flags & (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA)) {
               /* Porter/Duff SRC_OVER composition. */
               if ((DFB_PIXELFORMAT_HAS_ALPHA( surface->config.format ) &&
                    (surface->config.caps & DSCAPS_PREMULTIPLIED)) ||
                   (font->surface_caps & DSCAPS_PREMULTIPLIED)) {
                    if (font->surface_caps & DSCAPS_PREMULTIPLIED) {
                         if (flags & DSBLIT_BLEND_COLORALPHA)
                              flags |= DSBLIT_SRC_PREMULTCOLOR;
                    }
                    else
                         flags |= DSBLIT_SRC_PREMULTIPLY;

                    if (set_blend)
                         dfb_state_set_src_blend( state, DSBF_ONE );
               }
               else if (set_blend)
                    dfb_state_set_src_blend( state, DSBF_SRCALPHA );

               if (set_blend)
                    dfb_state_set_dst_blend( state, DSBF_INVSRCALPHA );
          }

          dfb_state_set_blitting_flags( state, flags );
     }
     else {
          backup->blittingflags = 0;
          backup->src_blend     = 0;
          backup->dst_blend     = 0;
     }
}

static void
font_state_restore( CardState *state,
                    CardState *backup )
{
     if (state->blittingflags != DSBLIT_INDEX_TRANSLATION) {
          dfb_state_set_blitting_flags( state, backup->blittingflags );
          dfb_state_set_src_blend( state, backup->src_blend );
          dfb_state_set_dst_blend( state, backup->dst_blend );
     }
}

void
dfb_gfxcard_drawstring( const u8                *text,
                        int                      bytes,
                        DFBTextEncodingID        encoding,
                        int                      x,
                        int                      y,
                        CoreFont                *font,
                        unsigned int             layers,
                        CoreGraphicsStateClient *client,
                        DFBSurfaceTextFlags      flags )
{
     DFBResult     ret;
     unsigned int  indices[bytes];
     int           i, l, num;
     int           kern_x;
     int           kern_y;
     DFBRectangle  rects[50];
     DFBPoint      points[50];
     CardState     state_backup;
     CardState    *state;
     CoreSurface  *surface;
     int           num_blits = 0;
     int           ox        = x;
     int           oy        = y;
     unsigned int  prev      = 0;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );

     state = client->state;

     D_MAGIC_ASSERT( state, CardState );
     D_MAGIC_ASSERT( state->destination, CoreSurface );
     D_ASSERT( text != NULL );
     D_ASSERT( bytes > 0 );
     D_ASSERT( font != NULL );

     if (encoding == DTEID_UTF8)
          D_DEBUG_AT( Core_GraphicsOps, "%s( '%s' [%d], %d,%d, %p, %p )\n", __FUNCTION__,
                      text, bytes, x, y, font, client );
     else
          D_DEBUG_AT( Core_GraphicsOps, "%s( %p [%d], %u, %d,%d, %p, %p )\n", __FUNCTION__,
                      text, bytes, encoding, x, y, font, client );

     surface = state->destination;

     /* Simple prechecks. */
     if (!(font->description.flags & DFDESC_ROTATION) || !font->description.rotation) {
          if (!(state->render_options & DSRO_MATRIX) &&
              (x > state->clip.x2 || y > state->clip.y2 ||
               y + font->height <= state->clip.y1)) {
               return;
          }
     }

     /* Decode string to character indices. */
     ret = dfb_font_decode_text( font, encoding, text, bytes, indices, &num );
     if (ret)
          return;

     font_state_prepare( state, &state_backup, font, surface, !(flags & DSTF_BLEND_FUNCS) );

     dfb_font_lock( font );

     for (l = layers - 1; l >= 0; l--) {
          x = ox << 8;
          y = oy << 8;

          if (layers > 1)
               dfb_state_set_color( state, &state->colors[l] );

          /* Blit glyphs. */
          for (i = 0; i < num; i++) {
               CoreGlyphData *glyph;
               unsigned int   current = indices[i];

               ret = dfb_font_get_glyph_data( font, current, l, &glyph );
               if (ret) {
                    D_DEBUG_AT( Core_GraphicsOps, "  -> glyph data loading from font failed!\n" );
                    prev = current;
                    continue;
               }

               if (prev && font->GetKerning && font->GetKerning( font, prev, current, &kern_x, &kern_y) == DFB_OK) {
                    x += kern_x << 8;
                    y += kern_y << 8;
               }

               if (glyph->width) {
                    if (glyph->surface != state->source || num_blits == D_ARRAY_SIZE(rects)) {
                         if (num_blits) {
                              CoreGraphicsStateClient_Blit( client, rects, points, num_blits );
                              num_blits = 0;
                         }

                         if (glyph->surface != state->source)
                              dfb_state_set_source( state, glyph->surface );
                    }

                    points[num_blits] = (DFBPoint) { (x >> 8) + glyph->left, (y >> 8) + glyph->top };
                    rects[num_blits]  = (DFBRectangle) { glyph->start, 0, glyph->width, glyph->height };

                    num_blits++;
               }

               x   += glyph->xadvance;
               y   += glyph->yadvance;
               prev = current;
          }

          if (num_blits) {
               CoreGraphicsStateClient_Blit( client, rects, points, num_blits );
               num_blits = 0;
          }
     }

     dfb_font_unlock( font );

     font_state_restore( state, &state_backup );
}

void
dfb_gfxcard_drawglyph( CoreGlyphData           **glyph,
                       int                       x,
                       int                       y,
                       CoreFont                 *font,
                       unsigned int              layers,
                       CoreGraphicsStateClient  *client,
                       DFBSurfaceTextFlags       flags )
{
     int          l;
     CardState    state_backup;
     CardState   *state;
     CoreSurface *surface;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );

     state = client->state;

     D_MAGIC_ASSERT( state, CardState );
     D_MAGIC_ASSERT( state->destination, CoreSurface );
     D_ASSERT( font != NULL );

     D_DEBUG_AT( Core_GraphicsOps, "%s( %d,%d, %u, %p, %p )\n", __FUNCTION__, x, y, layers, font, client );

     surface = state->destination;

     font_state_prepare( state, &state_backup, font, surface, !(flags & DSTF_BLEND_FUNCS) );

     for (l = layers - 1; l >= 0; l--) {
          if (layers > 1)
               dfb_state_set_color( state, &state->colors[l] );

          /* Blit glyph. */
          if (glyph[l]->width) {
               DFBRectangle rect  = { glyph[l]->start, 0, glyph[l]->width, glyph[l]->height };
               DFBPoint     point = { x + glyph[l]->left, y + glyph[l]->top };

               dfb_state_set_source( state, glyph[l]->surface );

               CoreGraphicsStateClient_Blit( client, &rect, &point, 1 );
          }
     }

     font_state_restore( state, &state_backup );
}

bool
dfb_gfxcard_drawstring_check_state( CoreFont                *font,
                                    CardState               *state,
                                    CoreGraphicsStateClient *client,
                                    DFBSurfaceTextFlags      flags )
{
     int                  i;
     bool                 result = false;
     CoreSurface         *surface;
     CardState            state_backup;
     CoreGlyphData       *data;
     DFBAccelerationMask  mask;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     D_MAGIC_ASSERT( state, CardState );
     D_MAGIC_ASSERT( state->destination, CoreSurface );
     D_ASSERT( font != NULL );
     D_ASSERT( client != NULL );

     D_DEBUG_AT( Core_GfxState, "%s( %p, %p, %p )\n", __FUNCTION__, font, state, client );

     surface = state->destination;

     dfb_font_lock( font );

     for (i = 0; i < 128; i++) {
          if (dfb_font_get_glyph_data( font, i, 0, &data ) == DFB_OK)
               break;
     }

     if (!data) {
          D_DEBUG_AT( Core_GfxState, "  -> no font data!\n" );
          dfb_font_unlock( font );
          return false;
     }

     font_state_prepare( state, &state_backup, font, surface, !(flags & DSTF_BLEND_FUNCS) );

     /* Set the source. */
     dfb_state_set_source( state, data->surface );

     if (CoreGraphicsStateClient_GetAccelerationMask( client, &mask ) == DFB_OK)
          result = mask & DFXL_BLIT;

     dfb_font_unlock( font );

     font_state_restore( state, &state_backup );

     return result;
}

DFBResult
dfb_gfxcard_sync()
{
     DFBResult ret;

     if (!card)
          return DFB_OK;

     ret = dfb_gfxcard_lock( GDLF_SYNC );
     if (ret)
          return ret;

     dfb_gfxcard_unlock();

     return DFB_OK;
}

DFBResult
dfb_gfxcard_wait_serial( const CoreGraphicsSerial *serial )
{
     DFBResult              ret;
     DFBGraphicsCoreShared *shared;

     D_ASSERT( serial != NULL );

     if (!card || dfb_config->software_only)
          return DFB_OK;

     D_ASSERT( card->shared != NULL );

     shared = card->shared;

     ret = dfb_gfxcard_lock( GDLF_NONE );
     if (ret)
          return ret;

     /* Start command processing if not already running. */
     if (shared->pending_ops && card->funcs.EmitCommands) {
          dfb_gfxcard_switch_busy();

          card->funcs.EmitCommands( card->driver_data, card->device_data );

          shared->pending_ops = false;
     }

     if (card->funcs.WaitSerial)
          ret = card->funcs.WaitSerial( card->driver_data, card->device_data, serial );
     else if (card->funcs.EngineSync)
          ret = card->funcs.EngineSync( card->driver_data, card->device_data );

     dfb_gfxcard_switch_idle();

     if (ret) {
          if (card->funcs.EngineReset)
               card->funcs.EngineReset( card->driver_data, card->device_data );

          shared->state = NULL;
     }

     dfb_gfxcard_unlock();

     return ret;
}

void
dfb_gfxcard_flush_texture_cache()
{
     if (dfb_config->software_only)
          return;

     if (card && card->funcs.FlushTextureCache)
          card->funcs.FlushTextureCache( card->driver_data, card->device_data );
}

void
dfb_gfxcard_flush_read_cache()
{
     if (dfb_config->software_only)
          return;

     if (card && card->funcs.FlushReadCache)
          card->funcs.FlushReadCache( card->driver_data, card->device_data );
}

void
dfb_gfxcard_after_set_var()
{
     if (dfb_config->software_only)
          return;

     if (card && card->funcs.AfterSetVar)
          card->funcs.AfterSetVar( card->driver_data, card->device_data );
}

void
dfb_gfxcard_get_capabilities( CardCapabilities *ret_caps )
{
     D_ASSERT( card != NULL );

     D_ASSERT( ret_caps != NULL );

     *ret_caps = card->caps;
}

void
dfb_gfxcard_get_device_info( GraphicsDeviceInfo *ret_device_info )
{
     DFBGraphicsCoreShared *shared;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     D_ASSERT( ret_device_info != NULL );

     shared = card->shared;

     *ret_device_info = shared->device_info;
}

void
dfb_gfxcard_get_driver_info( GraphicsDriverInfo *ret_driver_info )
{
     DFBGraphicsCoreShared *shared;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     D_ASSERT( ret_driver_info != NULL );

     shared = card->shared;

     *ret_driver_info = shared->driver_info;
}

int
dfb_gfxcard_reserve_memory( unsigned int size )
{
     DFBGraphicsCoreShared *shared;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     shared = card->shared;

     if (shared->device_info.limits.surface_byteoffset_alignment) {
          size += shared->device_info.limits.surface_byteoffset_alignment - 1;
          size -= (size % shared->device_info.limits.surface_byteoffset_alignment);
     }
     else
          D_WARN( "no alignment specified yet" );

     if (shared->videoram_length < size) {
          D_WARN( "not enough video memory (%u < %u)", shared->videoram_length, size );
          return -1;
     }

     shared->videoram_length -= size;

     return shared->videoram_length;
}

unsigned int
dfb_gfxcard_memory_length()
{
     DFBGraphicsCoreShared *shared;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     shared = card->shared;

     return shared->videoram_length;
}

volatile void *
dfb_gfxcard_map_mmio( unsigned int offset,
                      int          length )
{
     return dfb_system_map_mmio( offset, length );
}

void
dfb_gfxcard_unmap_mmio( volatile void      *addr,
                        int                 length )
{
     dfb_system_unmap_mmio( addr, length );
}

int
dfb_gfxcard_get_accelerator()
{
     return dfb_system_get_accelerator();
}

void
dfb_gfxcard_calc_buffer_size( CoreSurfaceBuffer *buffer,
                              int               *ret_pitch,
                              int               *ret_length )
{
     DFBResult    ret = DFB_FAILURE;
     int          pitch;
     int          length;
     CoreSurface *surface;

     D_ASSERT( card != NULL );

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_MAGIC_ASSERT( buffer->surface, CoreSurface );

     surface = buffer->surface;

     /* Use the Graphics card's own method to calculate the buffer size. */
     if (card->funcs.CalcBufferSize) {
          ret = card->funcs.CalcBufferSize( card->driver_data, card->device_data, buffer, &pitch, &length );
     }

     if (ret) {
          /* Calculate the required length depending on limitations. */
          pitch = MAX( surface->config.size.w, surface->config.min_size.w );

          if (pitch < card->limits.surface_max_power_of_two_pixelpitch &&
              surface->config.size.h < card->limits.surface_max_power_of_two_height)
               pitch = 1 << direct_log2( pitch );

          if (card->limits.surface_pixelpitch_alignment > 1) {
               pitch += card->limits.surface_pixelpitch_alignment - 1;
               pitch -= pitch % card->limits.surface_pixelpitch_alignment;
          }

          pitch = DFB_BYTES_PER_LINE( surface->config.format, pitch );

          if (pitch < card->limits.surface_max_power_of_two_bytepitch &&
              surface->config.size.h < card->limits.surface_max_power_of_two_height)
               pitch = 1 << direct_log2( pitch );

          if (card->limits.surface_bytepitch_alignment > 1) {
               pitch += card->limits.surface_bytepitch_alignment - 1;
               pitch -= pitch % card->limits.surface_bytepitch_alignment;
          }

          length = DFB_PLANE_MULTIPLY( surface->config.format,
                                       MAX( surface->config.size.h, surface->config.min_size.h ) * pitch );

          if (card->limits.surface_byteoffset_alignment > 1) {
               length += card->limits.surface_byteoffset_alignment - 1;
               length -= length % card->limits.surface_byteoffset_alignment;
          }
     }

     if (ret_pitch)
          *ret_pitch = pitch;

     if (ret_length)
          *ret_length = length;
}

unsigned long
dfb_gfxcard_memory_physical( unsigned int offset )
{
     return dfb_system_video_memory_physical( offset );
}

void *
dfb_gfxcard_memory_virtual( unsigned int offset )
{
     return dfb_system_video_memory_virtual( offset );
}

void *
dfb_gfxcard_get_device_data()
{
     DFBGraphicsCoreShared *shared;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     shared = card->shared;

     return shared->device_data;
}

void *
dfb_gfxcard_get_driver_data()
{
     D_ASSERT( card != NULL );

     return card->driver_data;
}

/**********************************************************************************************************************/

static void
dfb_gfxcard_update_stats( long long now )
{
     if (dfb_config->gfxcard_stats) {
          DFBGraphicsCoreShared *shared;
          long long              total;

          D_ASSERT( card != NULL );
          D_ASSERT( card->shared != NULL );

          shared = card->shared;
          total  = now - shared->ts_start;

          if (total > dfb_config->gfxcard_stats * 1000LL) {
               D_INFO( "DirectFB/Graphics: Stats: busy %lld / %lld -> %3lld.%lld%%\n", shared->ts_busy_sum, total,
                       (1000 * shared->ts_busy_sum / total) / 10LL, (1000 * shared->ts_busy_sum / total) % 10LL );

               shared->ts_start    = now;
               shared->ts_busy_sum = 0;
          }
     }
}

static void
dfb_gfxcard_switch_busy()
{
     if (dfb_config->gfxcard_stats) {
          DFBGraphicsCoreShared *shared;
          long long              now = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );

          D_ASSERT( card != NULL );
          D_ASSERT( card->shared != NULL );

          shared = card->shared;

          if (shared->ts_busy)
               shared->ts_busy_sum += now - shared->ts_busy;

          shared->ts_busy = now;

          if (!shared->ts_start)
               shared->ts_start = shared->ts_busy;

          dfb_gfxcard_update_stats( now );
     }
}

static void
dfb_gfxcard_switch_idle()
{
     if (dfb_config->gfxcard_stats) {
          DFBGraphicsCoreShared *shared;
          long long              now = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );

          D_ASSERT( card != NULL );
          D_ASSERT( card->shared != NULL );

          shared = card->shared;

          if (shared->ts_busy) {
               shared->ts_busy_sum += now - shared->ts_busy;

               shared->ts_busy = 0;
          }

          dfb_gfxcard_update_stats( now );
     }
}

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
#include <core/layer_context.h>
#include <core/surface.h>
#include <core/windows.h>
#include <core/windowstack.h>
#include <core/wm.h>
#include <directfb_util.h>
#include <fusion/shmalloc.h>

D_DEBUG_DOMAIN( Core_WM, "Core/WM", "DirectFB Core WM" );

DEFINE_MODULE_DIRECTORY( dfb_wm_modules, "wm", DFB_CORE_WM_ABI_VERSION );

/**********************************************************************************************************************/

typedef struct {
     int                  magic;

     int                  abi;

     char                *name;
     CoreWMInfo           info;
     void                *data;

     FusionSHMPoolShared *shmpool;

     FusionReactor       *reactor;
} DFBWMCoreShared;

typedef struct {
     int                magic;

     CoreDFB           *core;

     DFBWMCoreShared   *shared;

     DirectModuleEntry *module;
     const CoreWMFuncs *funcs;
     void              *data;
} DFBWMCore;

DFB_CORE_PART( wm_core, WMCore );

/**********************************************************************************************************************/

static DFBWMCore       *wm_local  = NULL;
static DFBWMCoreShared *wm_shared = NULL;

static DFBResult
load_module( const char *name )
{
     DirectModuleEntry *module;

     D_ASSERT( wm_local != NULL );

     direct_modules_explore_directory( &dfb_wm_modules );

     direct_list_foreach (module, dfb_wm_modules.entries) {
          const CoreWMFuncs *funcs;

          funcs = direct_module_ref( module );
          if (!funcs)
               continue;

          if (!name || !strcasecmp( name, module->name )) {
               if (wm_local->module)
                    direct_module_unref( wm_local->module );

               wm_local->module = module;
               wm_local->funcs  = funcs;
          }
          else
               direct_module_unref( module );
     }

     if (!wm_local->module) {
          if (name)
               D_ERROR( "Core/WM: Window manager module '%s' not found!\n", name );
          else
               D_ERROR( "Core/WM: No window manager module found!\n" );

          return DFB_NOIMPL;
     }

     return DFB_OK;
}

static DFBResult
dfb_wm_core_initialize( CoreDFB         *core,
                        DFBWMCore       *data,
                        DFBWMCoreShared *shared )
{
     DFBResult          ret;
     const CoreWMFuncs *funcs;

     D_DEBUG_AT( Core_WM, "%s( %p, %p, %p )\n", __FUNCTION__, core, data, shared );

     D_ASSERT( data != NULL );
     D_ASSERT( shared != NULL );

     data->core   = core;
     data->shared = shared;

     wm_local  = data;
     wm_shared = shared;

     wm_shared->shmpool = dfb_core_shmpool( core );

     /* Set ABI version for the session. */
     wm_shared->abi = DFB_CORE_WM_ABI_VERSION;

     /* Load the module. */
     ret = load_module( dfb_config->wm );
     if (ret)
          goto error;

     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->GetWMInfo != NULL );
     D_ASSERT( wm_local->funcs->Initialize != NULL );

     funcs = wm_local->funcs;

     /* Query module information. */
     funcs->GetWMInfo( &wm_shared->info );

     D_INFO( "DirectFB/Core/WM: %s %d.%d (%s)\n", wm_shared->info.name,
             wm_shared->info.version.major, wm_shared->info.version.minor, wm_shared->info.vendor );

     /* Store module name in shared memory. */
     wm_shared->name = SHSTRDUP( wm_shared->shmpool, wm_local->module->name );
     if (!wm_shared->name) {
          ret = D_OOSHM();
          goto error;
     }

     /* Allocate shared window manager data. */
     if (wm_shared->info.wm_shared_size) {
          wm_shared->data = SHCALLOC( wm_shared->shmpool, 1, wm_shared->info.wm_shared_size );
          if (!wm_shared->data) {
               ret = D_OOSHM();
               goto error;
          }
     }

     /* Allocate local window manager data. */
     if (wm_shared->info.wm_data_size) {
          wm_local->data = D_CALLOC( 1, wm_shared->info.wm_data_size );
          if (!wm_local->data) {
               ret = D_OOM();
               goto error;
          }
     }

     wm_shared->reactor = fusion_reactor_new( 0, "WM", dfb_core_world( core ) );

     fusion_reactor_direct( wm_shared->reactor, false );
     fusion_reactor_add_permissions( wm_shared->reactor, 0, FUSION_REACTOR_PERMIT_ATTACH_DETACH );

     /* Initialize window manager. */
     ret = funcs->Initialize( core, wm_local->data, wm_shared->data );
     if (ret) {
          D_DERROR( ret, "Core/WM: Could not initialize window manager!\n" );
          goto error;
     }

     D_MAGIC_SET( data, DFBWMCore );
     D_MAGIC_SET( wm_shared, DFBWMCoreShared );

     return DFB_OK;

error:
     if (wm_local->data)
          D_FREE( wm_local->data );

     if (wm_shared->data)
          SHFREE( wm_shared->shmpool, wm_shared->data );

     if (wm_shared->name)
          SHFREE( wm_shared->shmpool, wm_shared->name );

     wm_local = NULL;
     wm_shared = NULL;

     return ret;
}

static DFBResult
dfb_wm_core_join( CoreDFB         *core,
                  DFBWMCore       *data,
                  DFBWMCoreShared *shared )
{
     DFBResult          ret;
     CoreWMInfo         info;
     const CoreWMFuncs *funcs;

     D_DEBUG_AT( Core_WM, "%s( %p, %p, %p )\n", __FUNCTION__, core, data, shared );

     D_ASSERT( data != NULL );
     D_MAGIC_ASSERT( shared, DFBWMCoreShared );

     data->core   = core;
     data->shared = shared;

     wm_local  = data;
     wm_shared = shared;

     /* Check binary version numbers. */
     if (wm_shared->abi != DFB_CORE_WM_ABI_VERSION) {
          D_ERROR( "Core/WM: ABI version of running WM module (%d) doesn't match %d!\n",
                   wm_shared->abi, DFB_CORE_WM_ABI_VERSION );
          ret = DFB_VERSIONMISMATCH;
          goto error;
     }

     /* Load the module that is used by the running session. */
     ret = load_module( wm_shared->name );
     if (ret)
          goto error;

     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->GetWMInfo != NULL );
     D_ASSERT( wm_local->funcs->Join != NULL );

     funcs = wm_local->funcs;

     /* Query module information. */
     funcs->GetWMInfo( &info );

     if (wm_shared->info.version.major != info.version.major || wm_shared->info.version.minor != info.version.minor) {
          D_ERROR( "Core/WM: Running WM module version '%d.%d' doesn't match version '%d.%d'!\n",
                    wm_shared->info.version.major, wm_shared->info.version.minor,
                    info.version.major, info.version.minor );
          ret = DFB_VERSIONMISMATCH;
          goto error;
     }

     /* Allocate window manager data. */
     if (wm_shared->info.wm_data_size) {
          wm_local->data = D_CALLOC( 1, wm_shared->info.wm_data_size );
          if (!wm_local->data) {
               ret = D_OOM();
               goto error;
          }
     }

     /* Join window manager. */
     ret = funcs->Join( core, wm_local->data, wm_shared->data );
     if (ret) {
          D_DERROR( ret, "Core/WM: Could not join window manager!\n" );
          goto error;
     }

     D_MAGIC_SET( data, DFBWMCore );

     return DFB_OK;

error:
     if (wm_local->data)
          D_FREE( wm_local->data );

     wm_local = NULL;
     wm_shared = NULL;

     return ret;
}

static DFBResult
dfb_wm_core_shutdown( DFBWMCore *data,
                      bool       emergency )
{
     DFBResult          ret;
     const CoreWMFuncs *funcs;

     D_DEBUG_AT( Core_WM, "%s( %p, %semergency )\n", __FUNCTION__, data, emergency ? "" : "no " );

     D_MAGIC_ASSERT( data, DFBWMCore );
     D_MAGIC_ASSERT( data->shared, DFBWMCoreShared );

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->Shutdown != NULL );
     D_ASSERT( wm_shared == data->shared );

     funcs = wm_local->funcs;

     fusion_reactor_destroy( wm_shared->reactor );

     /* Shutdown window manager. */
     ret = funcs->Shutdown( emergency, wm_local->data, wm_shared->data );

     /* Unload the module. */
     direct_module_unref( wm_local->module );

#if !FUSION_BUILD_MULTI
     fusion_reactor_free( wm_shared->reactor );
#endif /* FUSION_BUILD_MULTI */

     fusion_reactor_free( wm_shared->reactor );

     /* Deallocate local window manager data. */
     if (wm_local->data)
          D_FREE( wm_local->data );

     /* Deallocate shared window manager data. */
     if (wm_shared->data)
          SHFREE( wm_shared->shmpool, wm_shared->data );

     /* Free module name in shared memory. */
     SHFREE( wm_shared->shmpool, wm_shared->name );

     D_MAGIC_CLEAR( data );
     D_MAGIC_CLEAR( wm_shared );

     wm_local = NULL;
     wm_shared = NULL;

     return ret;
}

static DFBResult
dfb_wm_core_leave( DFBWMCore *data,
                    bool      emergency )
{
     DFBResult          ret;
     const CoreWMFuncs *funcs;

     D_DEBUG_AT( Core_WM, "%s( %p, %semergency )\n", __FUNCTION__, data, emergency ? "" : "no " );

     D_MAGIC_ASSERT( data, DFBWMCore );
     D_MAGIC_ASSERT( data->shared, DFBWMCoreShared );

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->Leave != NULL );
     D_ASSERT( wm_shared != NULL );

     funcs = wm_local->funcs;

     /* Leave window manager. */
     ret = funcs->Leave( emergency, wm_local->data, wm_shared->data );

     /* Unload the module. */
     direct_module_unref( wm_local->module );

     /* Deallocate local window manager data. */
     if (wm_local->data)
          D_FREE( wm_local->data );

     wm_local = NULL;
     wm_shared = NULL;

     D_MAGIC_CLEAR( data );

     return ret;
}

static DFBResult
dfb_wm_core_suspend( DFBWMCore *data )
{
     const CoreWMFuncs *funcs;

     D_DEBUG_AT( Core_WM, "%s( %p )\n", __FUNCTION__, data );

     D_MAGIC_ASSERT( data, DFBWMCore );
     D_MAGIC_ASSERT( data->shared, DFBWMCoreShared );

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->Suspend != NULL );
     D_ASSERT( wm_shared != NULL );

     funcs = wm_local->funcs;

     return funcs->Suspend( wm_local->data, wm_shared->data );
}

static DFBResult
dfb_wm_core_resume( DFBWMCore *data )
{
     const CoreWMFuncs *funcs;

     D_DEBUG_AT( Core_WM, "%s( %p )\n", __FUNCTION__, data );

     D_MAGIC_ASSERT( data, DFBWMCore );
     D_MAGIC_ASSERT( data->shared, DFBWMCoreShared );

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->Resume != NULL );
     D_ASSERT( wm_shared != NULL );

     funcs = wm_local->funcs;

     return funcs->Resume( wm_local->data, wm_shared->data );
}

/**********************************************************************************************************************/

static bool
dfb_wm_layer_context_callback( FusionObjectPool *pool,
                               FusionObject     *object,
                               void             *ctx )
{
     CoreLayerContext     *context = (CoreLayerContext*) object;
     CoreWindowStackFlags  flags   = (CoreWindowStackFlags)(long) ctx;

     D_DEBUG_AT( Core_WM, "  -> ref context %p...\n", context );

     dfb_layer_context_ref( context );

     dfb_layer_context_lock( context );

     if (context->stack) {
          if ((context->stack->flags & CWSF_ACTIVATED) & flags)
               dfb_wm_set_active( context->stack, false );

          if ((context->stack->flags & CWSF_INITIALIZED) & flags)
               dfb_wm_close_stack( context->stack );
     }

     dfb_layer_context_unlock( context );

     D_DEBUG_AT( Core_WM, "  -> unref context %p...\n", context );

     dfb_layer_context_unref( context );

     return true;
}

DFBResult
dfb_wm_deactivate_all_stacks( void *data )
{
     DFBWMCore *local = data;

     D_DEBUG_AT( Core_WM, "%s( %p )\n", __FUNCTION__, local );

     D_MAGIC_ASSERT( local, DFBWMCore );
     D_MAGIC_ASSERT( local->shared, DFBWMCoreShared );

     dfb_core_enum_layer_contexts( local->core, dfb_wm_layer_context_callback, (void*)(long) CWSF_ACTIVATED );

     return DFB_OK;
}

DFBResult
dfb_wm_close_all_stacks( void *data )
{
     DFBWMCore *local = data;

     D_DEBUG_AT( Core_WM, "%s( %p )\n", __FUNCTION__, local );

     D_MAGIC_ASSERT( local, DFBWMCore );
     D_MAGIC_ASSERT( local->shared, DFBWMCoreShared );

     dfb_core_enum_layer_contexts( local->core, dfb_wm_layer_context_callback, (void*)(long) CWSF_INITIALIZED );

     return DFB_OK;
}

typedef struct {
     ReactionFunc        func;
     void               *ctx;
     Reaction           *reaction;
} AttachContext;

static void
convert_config( DFBWindowConfig        *config,
                const CoreWindowConfig *from )
{
     config->bounds            = from->bounds;
     config->opacity           = from->opacity;
     config->stacking          = from->stacking;
     config->options           = from->options;
     config->events            = from->events;
     config->association       = from->association;
     config->color_key         = from->color_key;
     config->opaque            = from->opaque;
     config->color             = from->color;
     config->key_selection     = DWKS_ALL;
     config->cursor_flags      = from->cursor_flags;
     config->cursor_resolution = from->cursor_resolution;
     config->src_geometry      = from->src_geometry;
     config->dst_geometry      = from->dst_geometry;
     config->rotation          = from->rotation;
     config->application_id    = from->application_id;
     config->stereo_depth      = from->z;
}

static void
convert_state( DFBWindowState  *state,
               CoreWindowFlags  flags )
{
     state->flags = DWSTATE_NONE;

     if (flags & CWF_INSERTED)
          state->flags |= DWSTATE_INSERTED;

     if (flags & CWF_FOCUSED)
          state->flags |= DWSTATE_FOCUSED;

     if (flags & CWF_ENTERED)
          state->flags |= DWSTATE_ENTERED;
}

static DFBEnumerationResult
wm_window_attach_callback( CoreWindow *window,
                           void       *ctx )
{
     AttachContext *attach_ctx = ctx;

     CoreWM_WindowAdd add;

     memset( &add, 0, sizeof(add ) );

     add.info.window_id   = window->id;
     add.info.caps        = window->caps;
     add.info.resource_id = window->resource_id;

     convert_config( &add.info.config, &window->config );

     convert_state( &add.info.state, window->flags );

     attach_ctx->func( &add, attach_ctx->ctx );

     return DFENUM_OK;
}

static bool
dfb_wm_layer_context_WINDOW_ADD_callback( FusionObjectPool *pool,
                                          FusionObject     *object,
                                          void             *ctx )
{
     DFBResult         ret;
     AttachContext    *attach_ctx = ctx;
     CoreLayerContext *context    = (CoreLayerContext*) object;

     dfb_layer_context_lock( context );

     if (context->stack) {
          ret = dfb_wm_enum_windows( context->stack, wm_window_attach_callback, attach_ctx );
          if (ret)
               D_DERROR( ret, "Core/WM: Could not enumerate windows" );

          ret = fusion_reactor_attach_channel( wm_shared->reactor, CORE_WM_WINDOW_ADD,
                                               attach_ctx->func, attach_ctx->ctx, attach_ctx->reaction );
          if (ret)
               D_DERROR( ret, "Core/WM: Could not attach to reactor" );
     }

     dfb_layer_context_unlock( context );

     return true;
}

DFBResult
dfb_wm_attach( CoreDFB      *core,
               int           channel,
               ReactionFunc  func,
               void         *ctx,
               Reaction     *reaction )
{
     D_ASSERT( wm_shared != NULL );

     if (channel == CORE_WM_WINDOW_ADD) {
          AttachContext attach_ctx = { func, ctx, reaction };

          if (dfb_core_is_master( core ))
               dfb_core_enum_layer_contexts( core, dfb_wm_layer_context_WINDOW_ADD_callback, &attach_ctx );
     }

     return fusion_reactor_attach_channel( wm_shared->reactor, channel, func, ctx, reaction );
}

DFBResult
dfb_wm_detach( CoreDFB  *core,
               Reaction *reaction )
{
     D_ASSERT( wm_shared != NULL );

     return fusion_reactor_detach( wm_shared->reactor, reaction );
}

static DFBResult
dfb_wm_dispatch( CoreDFB    *core,
                 int         channel,
                 const void *data,
                 int         size )
{
     D_ASSERT( wm_shared != NULL );

     return fusion_reactor_dispatch_channel( wm_shared->reactor, channel, data, size, true, NULL );
}

DFBResult
dfb_wm_dispatch_WindowAdd( CoreDFB    *core,
                           CoreWindow *window )
{
     pid_t            pid = 0;
     CoreWM_WindowAdd add;

     fusion_get_fusionee_pid( core->world, window->object.identity, &pid );

     add.info.window_id   = window->id;
     add.info.caps        = window->caps;
     add.info.resource_id = window->resource_id;
     add.info.process_id  = pid;
     add.info.instance_id = window->object.identity;

     convert_config( &add.info.config, &window->config );

     convert_state( &add.info.state, window->flags );

     return dfb_wm_dispatch( core, CORE_WM_WINDOW_ADD, &add, sizeof(add) );
}

DFBResult
dfb_wm_dispatch_WindowRemove( CoreDFB    *core,
                              CoreWindow *window )
{
     CoreWM_WindowRemove remove;

     remove.window_id = window->id;

     return dfb_wm_dispatch( core, CORE_WM_WINDOW_REMOVE, &remove, sizeof(remove) );
}

DFBResult
dfb_wm_dispatch_WindowConfig( CoreDFB              *core,
                              CoreWindow           *window,
                              DFBWindowConfigFlags  flags )
{
     CoreWM_WindowConfig config;

     config.window_id = window->id;
     config.flags     = flags;

     convert_config( &config.config, &window->config );

     return dfb_wm_dispatch( core, CORE_WM_WINDOW_CONFIG, &config, sizeof(config) );
}

DFBResult
dfb_wm_dispatch_WindowState( CoreDFB    *core,
                             CoreWindow *window )
{
     CoreWM_WindowState state;

     state.window_id = window->id;

     convert_state( &state.state, window->flags );

     return dfb_wm_dispatch( core, CORE_WM_WINDOW_STATE, &state, sizeof(state) );
}

DFBResult
dfb_wm_dispatch_WindowRestack( CoreDFB      *core,
                               CoreWindow   *window,
                               unsigned int  index )
{
     CoreWM_WindowRestack restack;

     restack.window_id = window->id;
     restack.index     = index;

     return dfb_wm_dispatch( core, CORE_WM_WINDOW_RESTACK, &restack, sizeof(restack) );
}

DFBResult
dfb_wm_dispatch_WindowFocus( CoreDFB    *core,
                             CoreWindow *window )
{
     CoreWM_WindowFocus focus;

     focus.window_id = window->id;

     return dfb_wm_dispatch( core, CORE_WM_WINDOW_FOCUS, &focus, sizeof(focus) );
}

void *
dfb_wm_get_data()
{
     D_ASSERT( wm_local != NULL );

     return wm_local->data;
}

DFBResult
dfb_wm_post_init( CoreDFB *core )
{
     const CoreWMFuncs *funcs;

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->PostInit != NULL );
     D_ASSERT( wm_shared != NULL );

     funcs = wm_local->funcs;

     return funcs->PostInit( wm_local->data, wm_shared->data );
}

DFBResult
dfb_wm_init_stack( CoreWindowStack *stack )
{
     DFBResult          ret;
     const CoreWMFuncs *funcs;

     D_DEBUG_AT( Core_WM, "%s( %p )\n", __FUNCTION__, stack );

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->InitStack != NULL );
     D_ASSERT( wm_shared != NULL );

     funcs = wm_local->funcs;

     D_MAGIC_ASSERT( stack, CoreWindowStack );
     D_ASSERT( !(stack->flags & CWSF_INITIALIZED) );

     D_MAGIC_ASSERT( stack->context, CoreLayerContext );
     FUSION_SKIRMISH_ASSERT( &stack->context->lock );

     /* Allocate shared stack data. */
     if (wm_shared->info.stack_data_size) {
          if (stack->stack_data)
               SHFREE( stack->shmpool, stack->stack_data );

          stack->stack_data = SHCALLOC( stack->shmpool, 1, wm_shared->info.stack_data_size );
          if (!stack->stack_data) {
               return D_OOSHM();
          }
     }

     /* Window manager specific initialization. */
     ret = funcs->InitStack( stack, wm_local->data, stack->stack_data );
     if (ret) {
          if (stack->stack_data) {
               SHFREE( wm_shared->shmpool, stack->stack_data );
               stack->stack_data = NULL;
          }

          return ret;
     }

     stack->flags |= CWSF_INITIALIZED;

     return DFB_OK;
}

DFBResult
dfb_wm_close_stack( CoreWindowStack *stack )
{
     const CoreWMFuncs *funcs;

     D_DEBUG_AT( Core_WM, "%s( %p )\n", __FUNCTION__, stack );

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->CloseStack != NULL );

     funcs = wm_local->funcs;

     D_MAGIC_ASSERT( stack, CoreWindowStack );

     D_ASSUME( stack->flags & CWSF_INITIALIZED );

     if (!(stack->flags & CWSF_INITIALIZED)) {
          D_ASSUME( !(stack->flags & CWSF_ACTIVATED) );
          return DFB_OK;
     }

     D_MAGIC_ASSERT( stack->context, CoreLayerContext );
     FUSION_SKIRMISH_ASSERT( &stack->context->lock );

     /* Deactivate before deinitialization. */
     if (stack->flags & CWSF_ACTIVATED)
          dfb_wm_set_active( stack, false );

     /* Clear flag and remove stack first, because CloseStack() may cause the stack to be destroyed. */
     stack->flags &= ~CWSF_INITIALIZED;

     /* Window manager specific deinitialization. */
     return funcs->CloseStack( stack, wm_local->data, stack->stack_data );
}

DFBResult
dfb_wm_set_active( CoreWindowStack *stack,
                   bool             active )
{
     DFBResult          ret;
     const CoreWMFuncs *funcs;

     D_DEBUG_AT( Core_WM, "%s( %p, %sactive )\n", __FUNCTION__, stack, active ? "" : "in" );

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->SetActive != NULL );

     funcs = wm_local->funcs;

     D_MAGIC_ASSERT( stack, CoreWindowStack );
     D_ASSERT( stack->flags & CWSF_INITIALIZED );

     D_MAGIC_ASSERT( stack->context, CoreLayerContext );
     FUSION_SKIRMISH_ASSERT( &stack->context->lock );

     if (active) {
          D_ASSUME( !(stack->flags & CWSF_ACTIVATED) );

          if (stack->flags & CWSF_ACTIVATED)
               return DFB_OK;

          ret = funcs->SetActive( stack, wm_local->data, stack->stack_data, true );

          stack->flags |= CWSF_ACTIVATED;
     }
     else {
          D_ASSUME( stack->flags & CWSF_ACTIVATED );

          if (!(stack->flags & CWSF_ACTIVATED))
               return DFB_OK;

          ret = funcs->SetActive( stack, wm_local->data, stack->stack_data, false );

          stack->flags &= ~CWSF_ACTIVATED;
     }

     return ret;
}

DFBResult
dfb_wm_resize_stack( CoreWindowStack *stack,
                     int              width,
                     int              height )
{
     const CoreWMFuncs *funcs;

     D_DEBUG_AT( Core_WM, "%s( %p, %dx%d )\n", __FUNCTION__, stack, width, height );

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->ResizeStack != NULL );

     funcs = wm_local->funcs;

     D_MAGIC_ASSERT( stack, CoreWindowStack );
     D_ASSERT( stack->flags & CWSF_INITIALIZED );

     D_MAGIC_ASSERT( stack->context, CoreLayerContext );
     FUSION_SKIRMISH_ASSERT( &stack->context->lock );

     /* Notify window manager about the new size. */
     return funcs->ResizeStack( stack, wm_local->data, stack->stack_data, width, height );
}

DFBResult
dfb_wm_process_input( CoreWindowStack     *stack,
                      const DFBInputEvent *event )
{
     const CoreWMFuncs *funcs;

     D_DEBUG_AT( Core_WM, "%s( %p, %p )\n", __FUNCTION__, stack, event );

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->ProcessInput != NULL );

     funcs = wm_local->funcs;

     D_MAGIC_ASSERT( stack, CoreWindowStack );
     D_ASSERT( stack->flags & CWSF_INITIALIZED );

     D_MAGIC_ASSERT( stack->context, CoreLayerContext );
     FUSION_SKIRMISH_ASSERT( &stack->context->lock );

     D_ASSERT( event != NULL );

     /* Dispatch input event via window manager. */
     return funcs->ProcessInput( stack, wm_local->data, stack->stack_data, event );
}

DFBResult
dfb_wm_flush_keys( CoreWindowStack *stack )
{
     const CoreWMFuncs *funcs;

     D_DEBUG_AT( Core_WM, "%s( %p )\n", __FUNCTION__, stack );

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->FlushKeys != NULL );

     funcs = wm_local->funcs;

     D_MAGIC_ASSERT( stack, CoreWindowStack );
     D_ASSERT( stack->flags & CWSF_INITIALIZED );

     D_MAGIC_ASSERT( stack->context, CoreLayerContext );
     FUSION_SKIRMISH_ASSERT( &stack->context->lock );

     return funcs->FlushKeys( stack, wm_local->data, stack->stack_data );
}

DFBResult
dfb_wm_window_at( CoreWindowStack  *stack,
                  int               x,
                  int               y,
                  CoreWindow      **ret_window )
{
     const CoreWMFuncs *funcs;

     D_DEBUG_AT( Core_WM, "%s( %p, %d,%d )\n", __FUNCTION__, stack, x, y );

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->WindowAt != NULL );

     funcs = wm_local->funcs;

     D_MAGIC_ASSERT( stack, CoreWindowStack );
     D_ASSERT( stack->flags & CWSF_INITIALIZED );

     D_MAGIC_ASSERT( stack->context, CoreLayerContext );
     FUSION_SKIRMISH_ASSERT( &stack->context->lock );

     D_ASSERT( ret_window != NULL );

     return funcs->WindowAt( stack, wm_local->data, stack->stack_data, x, y, ret_window );
}

DFBResult
dfb_wm_window_lookup( CoreWindowStack  *stack,
                      DFBWindowID       window_id,
                      CoreWindow      **ret_window )
{
     const CoreWMFuncs *funcs;

     D_DEBUG_AT( Core_WM, "%s( %p, %u )\n", __FUNCTION__, stack, window_id );

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->WindowLookup != NULL );

     funcs = wm_local->funcs;

     D_MAGIC_ASSERT( stack, CoreWindowStack );
     D_ASSERT( stack->flags & CWSF_INITIALIZED );

     D_MAGIC_ASSERT( stack->context, CoreLayerContext );
     FUSION_SKIRMISH_ASSERT( &stack->context->lock );

     D_ASSERT( ret_window != NULL );

     return funcs->WindowLookup( stack, wm_local->data, stack->stack_data, window_id, ret_window );
}

DFBResult
dfb_wm_enum_windows( CoreWindowStack      *stack,
                     CoreWMWindowCallback  callback,
                     void                 *callback_ctx )
{
     const CoreWMFuncs *funcs;

     D_DEBUG_AT( Core_WM, "%s( %p, %p, %p )\n", __FUNCTION__, stack, callback, callback_ctx );

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->EnumWindows != NULL );

     funcs = wm_local->funcs;

     D_MAGIC_ASSERT( stack, CoreWindowStack );
     D_ASSERT( stack->flags & CWSF_INITIALIZED );

     D_MAGIC_ASSERT( stack->context, CoreLayerContext );
     FUSION_SKIRMISH_ASSERT( &stack->context->lock );

     D_ASSERT( callback != NULL );

     return funcs->EnumWindows( stack, wm_local->data, stack->stack_data, callback, callback_ctx );
}

DFBResult
dfb_wm_get_insets( CoreWindowStack *stack,
                   CoreWindow      *window,
                   DFBInsets       *ret_insets )
{
     const CoreWMFuncs *funcs;

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->GetInsets != NULL );

     funcs = wm_local->funcs;

     D_MAGIC_ASSERT( stack, CoreWindowStack );
     D_ASSERT( stack->flags & CWSF_INITIALIZED );

     D_MAGIC_ASSERT( stack->context, CoreLayerContext );
     FUSION_SKIRMISH_ASSERT( &stack->context->lock );

     D_ASSERT( window != NULL );
     D_ASSERT( ret_insets != NULL );

     return funcs->GetInsets( stack, window, ret_insets );
}

DFBResult
dfb_wm_preconfigure_window( CoreWindowStack *stack,
                            CoreWindow      *window )
{
     DFBResult          ret;
     void              *window_data = NULL;
     const CoreWMFuncs *funcs;

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->PreConfigureWindow != NULL );
     D_ASSERT( wm_shared != NULL );

     funcs = wm_local->funcs;

     D_MAGIC_ASSERT( stack, CoreWindowStack );
     D_ASSERT( stack->flags & CWSF_INITIALIZED );

     D_MAGIC_ASSERT( stack->context, CoreLayerContext );
     FUSION_SKIRMISH_ASSERT( &stack->context->lock );

     D_ASSERT( window != NULL );

     D_DEBUG_AT( Core_WM, "%s( %p, %p, %4d,%4d-%4dx%4d )\n", __FUNCTION__,
                 stack, window, DFB_RECTANGLE_VALS( &window->config.bounds ) );

     /* Allocate shared window data. */
     if (wm_shared->info.window_data_size) {
          window_data = SHCALLOC( wm_shared->shmpool, 1, wm_shared->info.window_data_size );
          if (!window_data) {
               return D_OOSHM();
          }
     }

     /* Keep shared window data. */
     window->window_data = window_data;

     /* Tell window manager about the new window. */
     ret = funcs->PreConfigureWindow( stack, wm_local->data, stack->stack_data, window, window_data );
     if (ret) {
          if (window_data) {
               SHFREE( wm_shared->shmpool, window_data );
               window->window_data = NULL;
          }

          return ret;
     }

     return DFB_OK;
}

DFBResult
dfb_wm_add_window( CoreWindowStack *stack,
                   CoreWindow      *window )
{
     DFBResult          ret;
     const CoreWMFuncs *funcs;

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->AddWindow != NULL );
     D_ASSERT( wm_shared != NULL );

     funcs = wm_local->funcs;

     D_MAGIC_ASSERT( stack, CoreWindowStack );
     D_ASSERT( stack->flags & CWSF_INITIALIZED );

     D_MAGIC_ASSERT( stack->context, CoreLayerContext );
     FUSION_SKIRMISH_ASSERT( &stack->context->lock );

     D_ASSERT( window != NULL );

     D_DEBUG_AT( Core_WM, "%s( %p, %p, %4d,%4d-%4dx%4d )\n", __FUNCTION__,
                 stack, window, DFB_RECTANGLE_VALS( &window->config.bounds ) );

     /* Tell window manager about the new window. */
     ret = funcs->AddWindow( stack, wm_local->data, stack->stack_data, window, window->window_data );
     if (ret) {
          if (window->window_data)
               SHFREE( wm_shared->shmpool, window->window_data );
          return ret;
     }
     return DFB_OK;
}

DFBResult
dfb_wm_remove_window( CoreWindowStack *stack,
                      CoreWindow      *window )
{
     DFBResult          ret;
     const CoreWMFuncs *funcs;

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->RemoveWindow != NULL );
     D_ASSERT( wm_shared != NULL );

     funcs = wm_local->funcs;

     D_MAGIC_ASSERT( stack, CoreWindowStack );
     D_ASSERT( stack->flags & CWSF_INITIALIZED );

     D_MAGIC_ASSERT( stack->context, CoreLayerContext );
     FUSION_SKIRMISH_ASSERT( &stack->context->lock );

     D_ASSERT( window != NULL );

     D_DEBUG_AT( Core_WM, "%s( %p, %p, %4d,%4d-%4dx%4d )\n", __FUNCTION__,
                 stack, window, DFB_RECTANGLE_VALS( &window->config.bounds ) );

     /* Remove window from window manager. */
     ret = funcs->RemoveWindow( stack, wm_local->data, stack->stack_data, window, window->window_data );

     /* Deallocate shared stack data. */
     if (window->window_data) {
          SHFREE( wm_shared->shmpool, window->window_data );
          window->window_data = NULL;
     }

     return ret;
}

DFBResult
dfb_wm_set_window_property( CoreWindowStack  *stack,
                            CoreWindow       *window,
                            const char       *key,
                            void             *value,
                            void            **ret_old_value )
{
     const CoreWMFuncs *funcs;

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->SetWindowProperty != NULL );

     funcs = wm_local->funcs;

     D_MAGIC_ASSERT( stack, CoreWindowStack );
     D_ASSERT( stack->flags & CWSF_INITIALIZED );

     D_MAGIC_ASSERT( stack->context, CoreLayerContext );
     FUSION_SKIRMISH_ASSERT( &stack->context->lock );

     D_ASSERT( window != NULL );
     D_ASSERT( key != NULL );

     D_DEBUG_AT( Core_WM, "%s( %p, %p, %4d,%4d-%4dx%4d, '%s' = %p )\n", __FUNCTION__,
                 stack, window, DFB_RECTANGLE_VALS( &window->config.bounds ), key, value );

     return funcs->SetWindowProperty( stack, wm_local->data, stack->stack_data, window, window->window_data, key, value,
                                      ret_old_value );
}

DFBResult
dfb_wm_get_window_property( CoreWindowStack  *stack,
                            CoreWindow       *window,
                            const char       *key,
                            void            **ret_value )
{
     const CoreWMFuncs *funcs;

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->GetWindowProperty != NULL );

     funcs = wm_local->funcs;

     D_MAGIC_ASSERT( stack, CoreWindowStack );
     D_ASSERT( stack->flags & CWSF_INITIALIZED );

     D_MAGIC_ASSERT( stack->context, CoreLayerContext );
     FUSION_SKIRMISH_ASSERT( &stack->context->lock );

     D_ASSERT( window != NULL );
     D_ASSERT( key != NULL );

     D_DEBUG_AT( Core_WM, "%s( %p, %p, %4d,%4d-%4dx%4d, '%s' )\n", __FUNCTION__,
                 stack, window, DFB_RECTANGLE_VALS( &window->config.bounds ), key );

     return funcs->GetWindowProperty( stack, wm_local->data, stack->stack_data, window, window->window_data, key,
                                      ret_value );
}

DFBResult
dfb_wm_remove_window_property( CoreWindowStack  *stack,
                               CoreWindow       *window,
                               const char       *key,
                               void            **ret_value )
{
     DFBResult          ret;
     const CoreWMFuncs *funcs;

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->RemoveWindowProperty != NULL );

     funcs = wm_local->funcs;

     D_MAGIC_ASSERT( stack, CoreWindowStack );
     D_ASSERT( stack->flags & CWSF_INITIALIZED );

     D_MAGIC_ASSERT( stack->context, CoreLayerContext );
     FUSION_SKIRMISH_ASSERT( &stack->context->lock );

     D_ASSERT( window != NULL );
     D_ASSERT( key != NULL );

     D_DEBUG_AT( Core_WM, "%s( %p, %p, %4d,%4d-%4dx%4d, '%s' )\n", __FUNCTION__,
                 stack, window, DFB_RECTANGLE_VALS( &window->config.bounds ), key );

     ret = funcs->RemoveWindowProperty( stack, wm_local->data, stack->stack_data, window, window->window_data, key,
                                        ret_value );

     return ret;
}

static void
apply_geometry( const DFBWindowGeometry *geometry,
                const DFBRegion         *clip,
                const DFBWindowGeometry *parent,
                DFBRectangle            *ret_rect )
{
     int width, height;

     D_ASSERT( geometry != NULL );
     DFB_REGION_ASSERT( clip );
     D_ASSERT( ret_rect != NULL );

     if (!geometry)
          return;

     width  = clip->x2 - clip->x1 + 1;
     height = clip->y2 - clip->y1 + 1;

     switch (geometry->mode) {
          case DWGM_DEFAULT:
               D_DEBUG_AT( Core_WM, "  -- DEFAULT\n" );
               *ret_rect = DFB_RECTANGLE_INIT_FROM_REGION( clip );
               D_DEBUG_AT( Core_WM, "    -> [%4d,%4d-%4dx%4d]\n", DFB_RECTANGLE_VALS( ret_rect ) );
               return;

          case DWGM_FOLLOW:
               D_ASSERT( parent != NULL );
               D_DEBUG_AT( Core_WM, "  -- FOLLOW\n" );
               apply_geometry( parent, clip, NULL, ret_rect );
               break;

          case DWGM_RECTANGLE:
               D_DEBUG_AT( Core_WM, "  -- RECTANGLE [%4d,%4d-%4dx%4d]\n", DFB_RECTANGLE_VALS( &geometry->rectangle ) );
               *ret_rect = geometry->rectangle;
               ret_rect->x += clip->x1;
               ret_rect->y += clip->y1;
               break;

          case DWGM_LOCATION:
               D_DEBUG_AT( Core_WM, "  -- LOCATION [%.3f,%.3f-%.3fx%.3f]\n",
                           geometry->location.x, geometry->location.y, geometry->location.w, geometry->location.h );
               ret_rect->x = (int) (geometry->location.x * width  + 0.5f) + clip->x1;
               ret_rect->y = (int) (geometry->location.y * height + 0.5f) + clip->y1;
               ret_rect->w = (int) (geometry->location.w * width  + 0.5f);
               ret_rect->h = (int) (geometry->location.h * height + 0.5f);
               break;

          default:
               D_BUG( "invalid geometry mode %u", geometry->mode );
               return;
     }

     D_DEBUG_AT( Core_WM, "  -> %4d,%4d-%4dx%4d / clip %4d,%4d-%4dx%4d\n",
                 DFB_RECTANGLE_VALS( ret_rect ), DFB_RECTANGLE_VALS_FROM_REGION( clip ) );

     if (!dfb_rectangle_intersect_by_region( ret_rect, clip )) {
          D_WARN( "invalid geometry" );
          dfb_rectangle_from_region( ret_rect, clip );
     }

     D_DEBUG_AT( Core_WM, "  -> %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS( ret_rect ) );
}

DFBResult
dfb_wm_set_window_config( CoreWindow             *window,
                          const CoreWindowConfig *config,
                          DFBWindowConfigFlags    flags )
{
     const CoreWMFuncs *funcs;

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->SetWindowConfig != NULL );

     funcs = wm_local->funcs;

     D_ASSERT( window != NULL );
     D_MAGIC_ASSERT( window->stack, CoreWindowStack );

     D_MAGIC_ASSERT( window->stack->context, CoreLayerContext );
     FUSION_SKIRMISH_ASSERT( &window->stack->context->lock );

     D_ASSERT( config != NULL );

     D_DEBUG_AT( Core_WM, "%s( %p, %4d,%4d-%4dx%4d, %p, 0x%x )\n", __FUNCTION__,
                 window, DFB_RECTANGLE_VALS( &window->config.bounds ), config, flags );

     if (dfb_config->single_window) {
          bool        single_add = false;
          bool        single_remove = false;
          bool        single_update = false;
          CoreWindow *config_window = window;

          if (flags & DWCONF_OPACITY) {
               if (config->opacity != 0) {
                    if (window->config.opacity == 0) {
                         if (fusion_vector_size( &window->stack->visible_windows ) == 0) {
                              single_add = true;
                              single_update = true;
                         }
                         else if (fusion_vector_size( &window->stack->visible_windows ) == 1) {
                              config_window = fusion_vector_at( &window->stack->visible_windows, 0 );
                              single_remove = true;
                         }
                         fusion_vector_add( &window->stack->visible_windows, window );
                    }
               }
               else if (window->config.opacity != 0) {
                    if (fusion_vector_size( &window->stack->visible_windows ) == 2) {
                         single_add = true;
                         single_update = true;
                    }
                    else if (fusion_vector_size( &window->stack->visible_windows ) == 1) {
                         config_window = fusion_vector_at( &window->stack->visible_windows, 0 );
                         single_remove = true;
                    }

                    int idx = fusion_vector_index_of( &window->stack->visible_windows, window );
                    D_ASSERT( idx >= 0 );
                    fusion_vector_remove( &window->stack->visible_windows, idx );
               }
          }

          if (fusion_vector_size( &window->stack->visible_windows ) == 1)
               single_update = true;

          if (single_remove) {
               D_DEBUG_AT( Core_WM, "  -> single window optimisation: removing window %p\n", config_window );

               dfb_layer_region_disable( config_window->region );
               dfb_layer_region_enable( config_window->stack->context->primary.region );
               dfb_windowstack_repaint_all( config_window->stack );
          }
          else {
               if (single_add) {
                    D_DEBUG_AT( Core_WM, "  -> single window optimisation: adding window %p\n", config_window );

                    if (!config_window->region) {
                         DFBResult        ret;
                         CoreLayerRegion *region = NULL;
                         CoreSurface     *surface = config_window->surface;

                         /* Create a region for the window. */
                         ret = dfb_window_create_region( config_window, config_window->stack->context, surface,
                                                         surface->config.format, surface->config.colorspace,
                                                         surface->config.caps & (DSCAPS_INTERLACED | DSCAPS_SEPARATED |
                                                                                 DSCAPS_PREMULTIPLIED | DSCAPS_DEPTH  |
                                                                                 DSCAPS_SYSTEMONLY | DSCAPS_VIDEOONLY |
                                                                                 DSCAPS_STATIC_ALLOC | DSCAPS_TRIPLE  |
                                                                                 DSCAPS_GL),
                                                         &region, &surface );
                         if (ret) {
                              D_DEBUG_AT( Core_WM, "  -> region create failed!\n" );

                              int idx = fusion_vector_index_of( &config_window->stack->visible_windows, config_window );
                              D_ASSERT( idx >= 0 );
                              fusion_vector_remove( &config_window->stack->visible_windows, idx );
                         }
                         else {
                              D_ASSERT( config_window->surface == surface );

                              /* Link the region into the window structure. */
                              dfb_layer_region_link( &config_window->region, region );
                              dfb_layer_region_unref( region );

                              /* Link the surface into the window structure. */
                              dfb_surface_link( &config_window->surface, surface );
                              dfb_surface_unref( surface );
                         }
                    }

                    if (config_window->region) {
                         if (config_window->stack->context->primary.region->state & CLRSF_ENABLED)
                              dfb_layer_region_disable( config_window->stack->context->primary.region );

                         dfb_layer_region_enable( config_window->region );
                    }
               }

               if (single_update) {
                    CoreLayerRegionConfig      region_config = config_window->region->config;
                    CoreLayerRegionConfigFlags region_flags  = CLRCF_NONE;

                    D_DEBUG_AT( Core_WM, "  -> single window optimisation: updating window %p\n", config_window );

                    if (flags & DWCONF_OPACITY) {
                         region_flags |= CLRCF_OPACITY;

                         region_config.opacity = config->opacity;
                    }

                    if (flags & DWCONF_POSITION) {
                         region_flags |= CLRCF_DEST;

                         region_config.dest.x = config->bounds.x;
                         region_config.dest.y = config->bounds.y;
                    }

                    if (flags & DWCONF_SIZE) {
                         region_flags |= (CLRCF_WIDTH | CLRCF_HEIGHT | CLRCF_DEST);

                         region_config.width  = config_window->surface->config.size.w;
                         region_config.height = config_window->surface->config.size.h;

                         region_config.dest.w = config->bounds.w;
                         region_config.dest.h = config->bounds.h;
                    }

                    if (flags & DWCONF_DST_GEOMETRY) {
                         DFBRegion clip = DFB_REGION_INIT_FROM_RECTANGLE( &config->bounds );

                         region_flags |= CLRCF_DEST;
                         apply_geometry( &config->dst_geometry, &clip, NULL, &region_config.dest );
                    }

                    if (flags & DWCONF_SRC_GEOMETRY) {
                         DFBRegion clip = { 0, 0,
                                            config_window->surface->config.size.w - 1,
                                            config_window->surface->config.size.h - 1 };

                         region_flags |= CLRCF_SOURCE;
                         apply_geometry( &config->src_geometry, &clip, NULL, &region_config.source );
                    }

                    if (region_flags != CLRCF_NONE)
                         dfb_layer_region_set_configuration( config_window->region, &region_config, region_flags );
               }
          }
     }

     return funcs->SetWindowConfig( window, wm_local->data, window->window_data, config, flags );
}

DFBResult
dfb_wm_restack_window( CoreWindow *window,
                       CoreWindow *relative,
                       int         relation )
{
     const CoreWMFuncs *funcs;

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->RestackWindow != NULL );

     funcs = wm_local->funcs;

     D_ASSERT( window != NULL );
     D_MAGIC_ASSERT( window->stack, CoreWindowStack );

     D_MAGIC_ASSERT( window->stack->context, CoreLayerContext );
     FUSION_SKIRMISH_ASSERT( &window->stack->context->lock );

     D_ASSERT( relative == NULL || relative == window || relation != 0 );

     D_DEBUG_AT( Core_WM, "%s( %p, %4d,%4d-%4dx%4d, %p, %d )\n", __FUNCTION__,
                 window, DFB_RECTANGLE_VALS( &window->config.bounds ), relative, relation );

     return funcs->RestackWindow( window, wm_local->data, window->window_data, relative,
                                  relative ? relative->window_data : NULL, relation );
}

DFBResult
dfb_wm_grab( CoreWindow *window,
             CoreWMGrab *grab )
{
     const CoreWMFuncs *funcs;

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->Grab != NULL );

     funcs = wm_local->funcs;

     D_ASSERT( window != NULL );
     D_MAGIC_ASSERT( window->stack, CoreWindowStack );

     D_MAGIC_ASSERT( window->stack->context, CoreLayerContext );
     FUSION_SKIRMISH_ASSERT( &window->stack->context->lock );

     D_ASSERT( grab != NULL );

     D_DEBUG_AT( Core_WM, "%s( %p, %4d,%4d-%4dx%4d, %u )\n", __FUNCTION__,
                 window, DFB_RECTANGLE_VALS( &window->config.bounds ), grab->target );

     return funcs->Grab( window, wm_local->data, window->window_data, grab );
}

DFBResult
dfb_wm_ungrab( CoreWindow *window,
               CoreWMGrab *grab )
{
     const CoreWMFuncs *funcs;

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->Ungrab != NULL );

     funcs = wm_local->funcs;

     D_ASSERT( window != NULL );
     D_MAGIC_ASSERT( window->stack, CoreWindowStack );

     D_MAGIC_ASSERT( window->stack->context, CoreLayerContext );
     FUSION_SKIRMISH_ASSERT( &window->stack->context->lock );

     D_ASSERT( grab != NULL );

     D_DEBUG_AT( Core_WM, "%s( %p, %4d,%4d-%4dx%4d, %u )\n", __FUNCTION__,
                 window, DFB_RECTANGLE_VALS( &window->config.bounds ), grab->target );

     return funcs->Ungrab( window, wm_local->data, window->window_data, grab );
}

DFBResult
dfb_wm_request_focus( CoreWindow *window )
{
     const CoreWMFuncs *funcs;

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->RequestFocus != NULL );

     funcs = wm_local->funcs;

     D_ASSERT( window != NULL );
     D_MAGIC_ASSERT( window->stack, CoreWindowStack );

     D_MAGIC_ASSERT( window->stack->context, CoreLayerContext );
     FUSION_SKIRMISH_ASSERT( &window->stack->context->lock );

     D_DEBUG_AT( Core_WM, "%s( %p, %4d,%4d-%4dx%4d )\n", __FUNCTION__,
                 window, DFB_RECTANGLE_VALS( &window->config.bounds ) );

     return funcs->RequestFocus( window, wm_local->data, window->window_data );
}

DFBResult
dfb_wm_begin_updates( CoreWindow      *window,
                      const DFBRegion *update )
{
     const CoreWMFuncs *funcs;

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->BeginUpdates != NULL );

     funcs = wm_local->funcs;

     D_ASSERT( window != NULL );
     D_MAGIC_ASSERT( window->stack, CoreWindowStack );

     D_MAGIC_ASSERT( window->stack->context, CoreLayerContext );
     FUSION_SKIRMISH_ASSERT( &window->stack->context->lock );

     D_DEBUG_AT( Core_WM, "%s( %p, %4d,%4d-%4dx%4d )\n", __FUNCTION__,
                 window, DFB_RECTANGLE_VALS( &window->config.bounds ) );

     return funcs->BeginUpdates( window, wm_local->data, window->window_data, update );
}

DFBResult
dfb_wm_set_cursor_position( CoreWindow *window,
                            int         x,
                            int         y )
{
     const CoreWMFuncs *funcs;

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->SetCursorPosition != NULL );

     funcs = wm_local->funcs;

     D_ASSERT( window != NULL );
     D_MAGIC_ASSERT( window->stack, CoreWindowStack );

     D_MAGIC_ASSERT( window->stack->context, CoreLayerContext );
     FUSION_SKIRMISH_ASSERT( &window->stack->context->lock );

     D_DEBUG_AT( Core_WM, "%s( %p %4d,%4d )\n", __FUNCTION__, window, x, y );

     return funcs->SetCursorPosition( window, wm_local->data, window->window_data, x, y );
}

DFBResult
dfb_wm_update_stack( CoreWindowStack     *stack,
                     const DFBRegion     *region,
                     DFBSurfaceFlipFlags  flags )
{
     const CoreWMFuncs *funcs;

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->UpdateStack != NULL );

     funcs = wm_local->funcs;

     D_MAGIC_ASSERT( stack, CoreWindowStack );
     D_ASSERT( stack->flags & CWSF_INITIALIZED );

     D_MAGIC_ASSERT( stack->context, CoreLayerContext );
     FUSION_SKIRMISH_ASSERT( &stack->context->lock );

     DFB_REGION_ASSERT( region );

     D_DEBUG_AT( Core_WM, "%s( %p, %4d,%4d-%4dx%4d, 0x%x )\n", __FUNCTION__,
                 stack, DFB_RECTANGLE_VALS_FROM_REGION( region ), flags );

     return funcs->UpdateStack( stack, wm_local->data, stack->stack_data, region, flags );
}

DFBResult
dfb_wm_update_window( CoreWindow          *window,
                      const DFBRegion     *left_region,
                      const DFBRegion     *right_region,
                      DFBSurfaceFlipFlags  flags )
{
     bool               stereo;
     const CoreWMFuncs *funcs;

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->UpdateWindow != NULL );

     funcs = wm_local->funcs;

     D_ASSERT( window != NULL );
     D_MAGIC_ASSERT( window->stack, CoreWindowStack );

     D_MAGIC_ASSERT( window->stack->context, CoreLayerContext );
     FUSION_SKIRMISH_ASSERT( &window->stack->context->lock );

     DFB_REGION_ASSERT_IF( left_region );
     DFB_REGION_ASSERT_IF( right_region );

     stereo = window->caps & DWCAPS_STEREO;

     D_DEBUG_AT( Core_WM, "%s( %p, %u, %4d,%4d-%4dx%4d )\n", __FUNCTION__,
                 window, window->object.id, DFB_RECTANGLE_VALS( &window->config.bounds ) );

     if (left_region)
          D_DEBUG_AT( Core_WM, "  -> %s[%4d,%4d-%4dx%4d]\n",
                      stereo ? "Left: " : "", DFB_RECTANGLE_VALS_FROM_REGION( left_region ) );

     if (right_region && stereo)
          D_DEBUG_AT( Core_WM, "  -> Right: [%4d,%4d-%4dx%4d]\n",
                      DFB_RECTANGLE_VALS_FROM_REGION( right_region ) );

     D_DEBUG_AT( Core_WM, "  -> flags: 0x%04x\n", flags );

     return funcs->UpdateWindow( window, wm_local->data, window->window_data, left_region, right_region, flags );
}

DFBResult
dfb_wm_update_cursor( CoreWindowStack       *stack,
                      CoreCursorUpdateFlags  flags )
{
     const CoreWMFuncs *funcs;

     D_ASSERT( wm_local != NULL );
     D_ASSERT( wm_local->funcs != NULL );
     D_ASSERT( wm_local->funcs->UpdateCursor != NULL );

     funcs = wm_local->funcs;

     D_MAGIC_ASSERT( stack, CoreWindowStack );
     D_ASSERT( stack->flags & CWSF_INITIALIZED );

     D_MAGIC_ASSERT( stack->context, CoreLayerContext );
     FUSION_SKIRMISH_ASSERT( &stack->context->lock );

     D_FLAGS_ASSERT( flags, CCUF_ALL );

     if (dfb_config->no_cursor)
          return DFB_OK;

     return funcs->UpdateCursor( stack, wm_local->data, stack->stack_data, flags );
}

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

#include <core/wm.h>

D_DEBUG_DOMAIN( IDirectFBWindows_default, "IDirectFBWindows/Default", "Default Window Stack Implementation" );

static DirectResult Probe    ( void             *ctx );

static DirectResult Construct( IDirectFBWindows *thiz,
                               CoreDFB          *core );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBWindows, default )

/**********************************************************************************************************************/

typedef struct {
     DirectLink         link;

     DFBWindowsWatcher  watcher;
     void              *context;

     Reaction           reactions[_CORE_WM_NUM_CHANNELS];
} RegisteredWatcher;

typedef struct {
     int         ref;      /* reference counter */

     CoreDFB    *core;

     DirectLink *watchers;
} IDirectFBWindows_data;

#define WM_ATTACH( Func, CHANNEL )                                                                                  \
     do {                                                                                                           \
          if (watcher->Func) {                                                                                      \
               ret = dfb_wm_attach( data->core, CORE_WM_##CHANNEL, IDirectFBWindows_WM_Reaction_##Func, registered, \
                                    &registered->reactions[CORE_WM_##CHANNEL] );                                    \
               if (ret) {                                                                                           \
                    D_DERROR( ret, "IDirectFBWindows/Default: Failed to attach CORE_WM_" #CHANNEL "!\n" );          \
                    goto error;                                                                                     \
               }                                                                                                    \
          }                                                                                                         \
     } while (0)

#define WM_DETACH()                                                                                                 \
     do {                                                                                                           \
          int i;                                                                                                    \
          for (i = _CORE_WM_NUM_CHANNELS - 1; i >= 0 ; i--) {                                                       \
               if (registered->reactions[i].func)                                                                   \
                    dfb_wm_detach( data->core, &registered->reactions[i] );                                         \
          }                                                                                                         \
     } while (0)

/**********************************************************************************************************************/

static ReactionResult
IDirectFBWindows_WM_Reaction_WindowAdd( const void *msg_data,
                                        void       *ctx )
{
     const CoreWM_WindowAdd *add        = msg_data;
     RegisteredWatcher      *registered = ctx;

     D_DEBUG_AT( IDirectFBWindows_default, "%s( %p, %p )\n", __FUNCTION__, add, registered );

     D_ASSERT( registered->watcher.WindowAdd != NULL );

     registered->watcher.WindowAdd( registered->context, &add->info );

     return RS_OK;
}

static ReactionResult
IDirectFBWindows_WM_Reaction_WindowRemove( const void *msg_data,
                                           void       *ctx )
{
     const CoreWM_WindowRemove *remove     = msg_data;
     RegisteredWatcher         *registered = ctx;

     D_DEBUG_AT( IDirectFBWindows_default, "%s( %p, %p )\n", __FUNCTION__, remove, registered );

     D_ASSERT( registered->watcher.WindowRemove != NULL );

     registered->watcher.WindowRemove( registered->context, remove->window_id );

     return RS_OK;
}

static ReactionResult
IDirectFBWindows_WM_Reaction_WindowConfig( const void *msg_data,
                                           void       *ctx )
{
     const CoreWM_WindowConfig *config     = msg_data;
     RegisteredWatcher         *registered = ctx;

     D_DEBUG_AT( IDirectFBWindows_default, "%s( %p, %p )\n", __FUNCTION__, config, registered );

     D_ASSERT( registered->watcher.WindowConfig != NULL );

     registered->watcher.WindowConfig( registered->context, config->window_id, &config->config, config->flags );

     return RS_OK;
}

static ReactionResult
IDirectFBWindows_WM_Reaction_WindowState( const void *msg_data,
                                          void       *ctx )
{
     const CoreWM_WindowState *state      = msg_data;
     RegisteredWatcher        *registered = ctx;

     D_DEBUG_AT( IDirectFBWindows_default, "%s( %p, %p )\n", __FUNCTION__, state, registered );

     D_ASSERT( registered->watcher.WindowState != NULL );

     registered->watcher.WindowState( registered->context, state->window_id, &state->state );

     return RS_OK;
}

static ReactionResult
IDirectFBWindows_WM_Reaction_WindowRestack( const void *msg_data,
                                            void       *ctx )
{
     const CoreWM_WindowRestack *restack    = msg_data;
     RegisteredWatcher          *registered = ctx;

     D_DEBUG_AT( IDirectFBWindows_default, "%s( %p, %p )\n", __FUNCTION__, restack, registered );

     D_ASSERT( registered->watcher.WindowRestack != NULL );

     registered->watcher.WindowRestack( registered->context, restack->window_id, restack->index );

     return RS_OK;
}

static ReactionResult
IDirectFBWindows_WM_Reaction_WindowFocus( const void *msg_data,
                                          void       *ctx )
{
     const CoreWM_WindowFocus *focus      = msg_data;
     RegisteredWatcher        *registered = ctx;

     D_DEBUG_AT( IDirectFBWindows_default, "%s( %p, %p )\n", __FUNCTION__, focus, registered );

     D_ASSERT( registered->watcher.WindowFocus != NULL );

     registered->watcher.WindowFocus( registered->context, focus->window_id );

     return RS_OK;
}

static void
IDirectFBWindows_Destruct( IDirectFBWindows *thiz )
{
     IDirectFBWindows_data *data = thiz->priv;
     RegisteredWatcher     *registered, *next;

     D_DEBUG_AT( IDirectFBWindows_default, "%s( %p )\n", __FUNCTION__, thiz );

     direct_list_foreach_safe (registered, next, data->watchers) {
          WM_DETACH();

          D_FREE( registered );
     }

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DirectResult
IDirectFBWindows_AddRef( IDirectFBWindows *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBWindows )

     D_DEBUG_AT( IDirectFBWindows_default, "%s( %p )\n", __FUNCTION__, thiz );

     data->ref++;

     return DFB_OK;
}

static DirectResult
IDirectFBWindows_Release( IDirectFBWindows *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBWindows )

     D_DEBUG_AT( IDirectFBWindows_default, "%s( %p )\n", __FUNCTION__, thiz );

     if (--data->ref == 0)
          IDirectFBWindows_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBWindows_RegisterWatcher( IDirectFBWindows        *thiz,
                                  const DFBWindowsWatcher *watcher,
                                  void                    *context )
{
     DFBResult          ret;
     RegisteredWatcher *registered;

     DIRECT_INTERFACE_GET_DATA( IDirectFBWindows )

     D_DEBUG_AT( IDirectFBWindows_default, "%s( %p )\n", __FUNCTION__, thiz );

     if (!watcher)
          return DFB_INVARG;

     if (!watcher->WindowAdd     &&
         !watcher->WindowRemove  &&
         !watcher->WindowConfig  &&
         !watcher->WindowState   &&
         !watcher->WindowRestack &&
         !watcher->WindowFocus)
          return DFB_INVARG;

     registered = D_CALLOC( 1, sizeof(RegisteredWatcher) );
     if (!registered)
          return D_OOM();

     registered->watcher = *watcher;
     registered->context = context;

     WM_ATTACH( WindowAdd,     WINDOW_ADD );
     WM_ATTACH( WindowRemove,  WINDOW_REMOVE );
     WM_ATTACH( WindowConfig,  WINDOW_CONFIG );
     WM_ATTACH( WindowState,   WINDOW_STATE );
     WM_ATTACH( WindowRestack, WINDOW_RESTACK );
     WM_ATTACH( WindowFocus,   WINDOW_FOCUS );

     direct_list_append( &data->watchers, &registered->link );

     return DFB_OK;


error:
     WM_DETACH();

     D_FREE( registered );

     return ret;
}

static DFBResult
IDirectFBWindows_UnregisterWatcher( IDirectFBWindows *thiz,
                                    void             *context )
{
     RegisteredWatcher *registered;

     DIRECT_INTERFACE_GET_DATA( IDirectFBWindows )

     D_DEBUG_AT( IDirectFBWindows_default, "%s( %p )\n", __FUNCTION__, thiz );

     direct_list_foreach (registered, data->watchers) {
          if (registered->context == context) {
               WM_DETACH();

               direct_list_remove( &data->watchers, &registered->link );

               D_FREE( registered );

               return DFB_OK;
          }
     }

     return DFB_ITEMNOTFOUND;
}

/**********************************************************************************************************************/

static DirectResult
Probe( void *ctx )
{
     return DFB_OK;
}

static DirectResult
Construct( IDirectFBWindows *thiz,
           CoreDFB          *core )
{
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBWindows )

     D_DEBUG_AT( IDirectFBWindows_default, "%s( %p )\n", __FUNCTION__, thiz );

     data->ref  = 1;
     data->core = core;

     thiz->AddRef            = IDirectFBWindows_AddRef;
     thiz->Release           = IDirectFBWindows_Release;
     thiz->RegisterWatcher   = IDirectFBWindows_RegisterWatcher;
     thiz->UnregisterWatcher = IDirectFBWindows_UnregisterWatcher;

     return DFB_OK;
}

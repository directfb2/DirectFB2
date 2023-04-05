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

#include <core/CoreWindow.h>
#include <core/core.h>
#include <core/layer_context.h>
#include <core/surface.h>
#include <core/windowstack.h>
#include <core/windows.h>
#include <core/wm.h>

D_DEBUG_DOMAIN( Core_Window, "DirectFB/CoreWindow", "DirectFB CoreWindow" );

/**********************************************************************************************************************/

DFBResult
IWindow_Real__SetConfig( CoreWindow                    *obj,
                         const CoreWindowConfig        *config,
                         const DFBInputDeviceKeySymbol *keys,
                         u32                            num_keys,
                         DFBWindowConfigFlags           flags )
{
     CoreWindowConfig config_copy;

     D_DEBUG_AT( Core_Window, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreWindow );
     D_ASSERT( config != NULL );

     if ((flags & DWCONF_ASSOCIATION) && config->association) {
          DFBResult ret;
          CoreWindow *parent;

          ret = dfb_core_get_window( core_dfb, config->association, &parent );
          if (ret)
               return ret;

          if (fusion_object_check_owner( &parent->object, Core_GetIdentity(), false )) {
               dfb_window_unref( parent );
               return DFB_ACCESSDENIED;
          }

          dfb_window_unref( parent );
     }

     config_copy = *config;

     config_copy.keys     = (DFBInputDeviceKeySymbol*) keys;
     config_copy.num_keys = num_keys;

     return dfb_window_set_config( obj, &config_copy, flags );
}

DFBResult
IWindow_Real__GetInsets( CoreWindow *obj,
                         DFBInsets  *ret_insets )
{
     DFBResult ret;

     D_DEBUG_AT( Core_Window, "%s( %p )\n", __FUNCTION__, obj );

     ret = dfb_layer_context_lock( obj->stack->context );
     if (ret)
          return ret;

     ret = dfb_wm_get_insets( obj->stack, obj, ret_insets );

     dfb_layer_context_unlock( obj->stack->context );

     return ret;
}

DFBResult
IWindow_Real__Destroy( CoreWindow *obj )
{
     D_DEBUG_AT( Core_Window, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreWindow );

     dfb_window_destroy( obj );

     return DFB_OK;
}

DFBResult
IWindow_Real__ChangeEvents( CoreWindow         *obj,
                            DFBWindowEventType  disable,
                            DFBWindowEventType  enable )
{
     D_DEBUG_AT( Core_Window, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreWindow );

     return dfb_window_change_events( obj, disable, enable );
}

DFBResult
IWindow_Real__ChangeOptions( CoreWindow       *obj,
                             DFBWindowOptions  disable,
                             DFBWindowOptions  enable )
{
     D_DEBUG_AT( Core_Window, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreWindow );

     return dfb_window_change_options( obj, disable, enable );
}

DFBResult
IWindow_Real__SetColor( CoreWindow     *obj,
                        const DFBColor *color )
{
     D_DEBUG_AT( Core_Window, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreWindow );

     return dfb_window_set_color( obj, *color );
}

DFBResult
IWindow_Real__SetColorKey( CoreWindow *obj,
                           u32         key )
{
     D_DEBUG_AT( Core_Window, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreWindow );

     return dfb_window_set_colorkey( obj, key );
}

DFBResult
IWindow_Real__SetOpacity( CoreWindow *obj,
                          u8          opacity )
{
     D_DEBUG_AT( Core_Window, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreWindow );

     return dfb_window_set_opacity( obj, opacity );
}

DFBResult
IWindow_Real__SetOpaque( CoreWindow      *obj,
                         const DFBRegion *opaque )
{
     D_DEBUG_AT( Core_Window, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreWindow );

     return dfb_window_set_opaque( obj, opaque );
}

DFBResult
IWindow_Real__SetCursorShape( CoreWindow     *obj,
                              CoreSurface    *shape,
                              const DFBPoint *hotspot )
{
     D_DEBUG_AT( Core_Window, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreWindow );

     return dfb_window_set_cursor_shape( obj, shape, hotspot->x, hotspot->y );
}

DFBResult
IWindow_Real__Move( CoreWindow *obj,
                    s32         dx,
                    s32         dy )
{
     D_DEBUG_AT( Core_Window, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreWindow );

     return dfb_window_move( obj, dx, dy, true );
}

DFBResult
IWindow_Real__MoveTo( CoreWindow *obj,
                      s32         x,
                      s32         y )
{
     DFBResult ret;
     DFBInsets insets;

     D_DEBUG_AT( Core_Window, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreWindow );

     dfb_windowstack_lock( obj->stack );

     dfb_wm_get_insets( obj->stack, obj, &insets );

     ret = dfb_window_move( obj, x + insets.l, y + insets.t, false );

     dfb_windowstack_unlock( obj->stack );

     return ret;
}

DFBResult
IWindow_Real__Resize( CoreWindow *obj,
                      s32         width,
                      s32         height )
{
     DFBResult ret;
     DFBInsets insets;

     D_DEBUG_AT( Core_Window, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreWindow );

     dfb_windowstack_lock( obj->stack );

     dfb_wm_get_insets( obj->stack, obj, &insets );

     ret = dfb_window_resize( obj, width + insets.l+insets.r, height + insets.t+insets.b );

     dfb_windowstack_unlock( obj->stack );

     return ret;
}

DFBResult
IWindow_Real__SetBounds( CoreWindow         *obj,
                         const DFBRectangle *bounds )
{
     D_DEBUG_AT( Core_Window, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreWindow );

     return dfb_window_set_bounds( obj, bounds->x, bounds->y, bounds->w, bounds->h );
}

DFBResult
IWindow_Real__SetStacking( CoreWindow             *obj,
                           DFBWindowStackingClass  stacking )
{
     D_DEBUG_AT( Core_Window, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreWindow );

     return dfb_window_change_stacking( obj, stacking );
}

DFBResult
IWindow_Real__Restack( CoreWindow *obj,
                       CoreWindow *relative,
                       s32         relation )
{
     DFBResult        ret;
     CoreWindowStack *stack;

     D_DEBUG_AT( Core_Window, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreWindow );
     D_ASSERT( obj->stack != NULL );

     stack = obj->stack;

     /* Lock the window stack. */
     if (dfb_windowstack_lock( stack ))
          return DFB_FUSION;

     /* Never call WM after destroying the window. */
     if (DFB_WINDOW_DESTROYED( obj )) {
          dfb_windowstack_unlock( stack );
          return DFB_DESTROYED;
     }

     /* Let the window manager do its work. */
     ret = dfb_wm_restack_window( obj, relative, relation );

     /* Unlock the window stack. */
     dfb_windowstack_unlock( stack );

     return ret;
}

DFBResult
IWindow_Real__Bind( CoreWindow *obj,
                    CoreWindow *source,
                    s32         x,
                    s32         y )
{
     D_DEBUG_AT( Core_Window, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreWindow );
     D_MAGIC_ASSERT( source, CoreWindow );

     return dfb_window_bind( obj, source, x, y );
}

DFBResult
IWindow_Real__Unbind( CoreWindow *obj,
                      CoreWindow *source )
{
     D_DEBUG_AT( Core_Window, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreWindow );
     D_MAGIC_ASSERT( source, CoreWindow );

     return dfb_window_unbind( obj, source );
}

DFBResult
IWindow_Real__RequestFocus( CoreWindow *obj )
{
     D_DEBUG_AT( Core_Window, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreWindow );

     return dfb_window_request_focus( obj );
}

DFBResult
IWindow_Real__ChangeGrab( CoreWindow       *obj,
                          CoreWMGrabTarget  target,
                          DFBBoolean        grab )
{
     D_DEBUG_AT( Core_Window, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreWindow );

     return dfb_window_change_grab( obj, target, grab );
}

DFBResult
IWindow_Real__GrabKey( CoreWindow                 *obj,
                       DFBInputDeviceKeySymbol     symbol,
                       DFBInputDeviceModifierMask  modifiers )
{
     D_DEBUG_AT( Core_Window, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreWindow );

     return dfb_window_grab_key( obj, symbol, modifiers );
}

DFBResult
IWindow_Real__UngrabKey( CoreWindow                 *obj,
                         DFBInputDeviceKeySymbol     symbol,
                         DFBInputDeviceModifierMask  modifiers )
{
     D_DEBUG_AT( Core_Window, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreWindow );

     return dfb_window_ungrab_key( obj, symbol, modifiers );
}

DFBResult
IWindow_Real__SetKeySelection( CoreWindow                    *obj,
                               DFBWindowKeySelection          selection,
                               const DFBInputDeviceKeySymbol *keys,
                               u32                            num_keys )
{
     D_DEBUG_AT( Core_Window, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreWindow );

     return dfb_window_set_key_selection( obj, selection, keys, num_keys );
}

DFBResult
IWindow_Real__SetRotation( CoreWindow *obj,
                           s32         rotation )
{
     D_DEBUG_AT( Core_Window, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreWindow );

     return dfb_window_set_rotation( obj, rotation );
}

DFBResult
IWindow_Real__BeginUpdates( CoreWindow      *obj,
                            const DFBRegion *update )
{
     DFBResult ret;

     D_DEBUG_AT( Core_Window, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreWindow );

     dfb_windowstack_lock( obj->stack );

     ret = dfb_wm_begin_updates( obj, update );

     dfb_windowstack_unlock( obj->stack );

     return ret;
}

DFBResult
IWindow_Real__PostEvent( CoreWindow           *obj,
                         const DFBWindowEvent *event )
{
     DFBWindowEvent e;

     D_DEBUG_AT( Core_Window, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreWindow );

     e = *event;

     dfb_window_post_event( obj, &e );

     return DFB_OK;
}

DFBResult
IWindow_Real__SetCursorPosition( CoreWindow *obj,
                                 s32         x,
                                 s32         y )
{
     DFBResult ret;

     D_DEBUG_AT( Core_Window, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreWindow );

     dfb_windowstack_lock( obj->stack );

     ret = dfb_wm_set_cursor_position( obj, x, y );

     dfb_windowstack_unlock( obj->stack );

     return ret;
}

DFBResult
IWindow_Real__SetTypeHint( CoreWindow        *obj,
                           DFBWindowTypeHint  type_hint )
{
     D_DEBUG_AT( Core_Window, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreWindow );

     return dfb_window_set_type_hint( obj, type_hint );
}

DFBResult
IWindow_Real__ChangeHintFlags( CoreWindow         *obj,
                               DFBWindowHintFlags  clear,
                               DFBWindowHintFlags  set )
{
     D_DEBUG_AT( Core_Window, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreWindow );

     return dfb_window_change_hint_flags( obj, clear, set );
}

DFBResult
IWindow_Real__AllowFocus( CoreWindow *obj )
{
     D_DEBUG_AT( Core_Window, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreWindow );

     obj->caps = (DFBWindowCapabilities) ((obj->caps & ~DWCAPS_NOFOCUS) | (obj->requested_caps & DWCAPS_NOFOCUS));

     return DFB_OK;
}

DFBResult
IWindow_Real__GetSurface( CoreWindow   *obj,
                          CoreSurface **ret_surface )
{
     DFBResult ret;

     D_DEBUG_AT( Core_Window, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreWindow );
     D_ASSERT( ret_surface != NULL );

     if (!obj->surface)
          return DFB_UNSUPPORTED;

     ret = dfb_surface_ref( obj->surface );
     if (ret)
          return ret;

     *ret_surface = obj->surface;

     return DFB_OK;
}

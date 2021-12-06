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

#ifndef __CORE__WINDOWS_H__
#define __CORE__WINDOWS_H__

#include <core/coretypes.h>
#include <directfb_windows.h>
#include <fusion/object.h>

/**********************************************************************************************************************/

typedef enum {
     CWF_NONE        = 0x00000000,

     CWF_INITIALIZED = 0x00000001,
     CWF_FOCUSED     = 0x00000002,
     CWF_ENTERED     = 0x00000004,
     CWF_DESTROYED   = 0x00000008,
     CWF_INSERTED    = 0x00000010,

     CWF_ALL         = 0x0000001F
} CoreWindowFlags;

#define DFB_WINDOW_INITIALIZED(w) ((w)->flags & CWF_INITIALIZED)
#define DFB_WINDOW_FOCUSED(w)     ((w)->flags & CWF_FOCUSED)
#define DFB_WINDOW_ENTERED(w)     ((w)->flags & CWF_ENTERED)
#define DFB_WINDOW_DESTROYED(w)   ((w)->flags & CWF_DESTROYED)
#define DFB_WINDOW_INSERTED(w)    ((w)->flags & CWF_INSERTED)

struct __DFB_CoreWindowConfig {
     DFBRectangle             bounds;
     int                      opacity;
     DFBWindowStackingClass   stacking;
     DFBWindowOptions         options;
     DFBWindowEventType       events;
     DFBColor                 color;
     u32                      color_key;
     DFBRegion                opaque;
     int                      z;
     DFBWindowKeySelection    key_selection;
     DFBInputDeviceKeySymbol *keys;
     unsigned int             num_keys;
     DFBWindowGeometry        src_geometry;
     DFBWindowGeometry        dst_geometry;
     int                      rotation;
     DFBWindowID              association;
     unsigned long            application_id;
     DFBWindowCursorFlags     cursor_flags;
     DFBDimension             cursor_resolution;
     DFBWindowTypeHint        type_hint;
     DFBWindowHintFlags       hint_flags;
};

struct __DFB_CoreWindow {
     FusionObject            object;

     int                     magic;

     DFBWindowID             id;                     /* window id */

     CoreWindowFlags         flags;                  /* state flags */

     DFBWindowCapabilities   caps;                   /* window capabilities, to enable blending etc. */

     CoreWindowConfig        config;                 /* current window configuration. */

     CoreSurface            *surface;                /* backing store surface */

     CoreWindowStack        *stack;                  /* window stack the window belongs */

     CoreLayerRegion        *primary_region;         /* default region of context */

     CoreLayerRegion        *region;                 /* hardware allocated window */

     void                   *window_data;            /* private data of window manager */

     DirectLink             *bound_windows;          /* list of bound windows */
     CoreWindow             *boundto;                /* window to which this window is bound (window binding) */

     DFBWindowID             toplevel_id;            /* toplevel window id, in case of a sub window */
     CoreWindow             *toplevel;               /* top level window */
     FusionVector            subwindows;             /* list of sub windows (only valid for top level windows) */

     CoreWindow             *subfocus;               /* which of the sub windows has the focus */

     unsigned long           resource_id;            /* resource id */

     FusionCall              call;                   /* dispatch */

     struct {
          int                hot_x;                  /* x position of cursor hot spot */
          int                hot_y;                  /* y position of cursor hot spot */
          CoreSurface       *surface;                /* cursor shape surface */
     } cursor;

     DFBWindowCapabilities   requested_caps;         /* original caps from application upon window creation */

     CoreSurfaceClient      *surface_client;         /* surface client */
     Reaction                surface_event_reaction; /* surface event reaction for CSCH_EVENT */
     u32                     surface_flip_count;     /* surface flip count */

     DFBWindowSurfacePolicy  policy;                 /* window surface swapping policy */
};

/**********************************************************************************************************************/

#define TRANSLUCENT_WINDOW(w) ((w)->config.opacity < 0xff || \
                               (w)->config.options & (DWOP_ALPHACHANNEL | DWOP_COLORKEYING))

#define VISIBLE_WINDOW(w)     (!((w)->caps & DWCAPS_INPUTONLY) && \
                               (w)->config.opacity > 0 && !DFB_WINDOW_DESTROYED((w)))

/**********************************************************************************************************************/

/*
 * Creates a pool of window objects.
 */
FusionObjectPool *dfb_window_pool_create       ( const FusionWorld              *world );

/*
 * Generates dfb_window_ref(), dfb_window_attach() etc.
 */
FUSION_OBJECT_METHODS( CoreWindow, dfb_window )

/**********************************************************************************************************************/

/*
 * Create a region and configure it optionally using the passed window_surface.
 */
DFBResult         dfb_window_create_region     ( CoreWindow                     *window,
                                                 CoreLayerContext               *context,
                                                 CoreSurface                    *window_surface,
                                                 DFBSurfacePixelFormat           format,
                                                 DFBSurfaceColorSpace            colorspace,
                                                 DFBSurfaceCapabilities          surface_caps,
                                                 CoreLayerRegion               **ret_region,
                                                 CoreSurface                   **ret_surface );

/*
 * Create a window on a given stack.
 */
DFBResult         dfb_window_create            ( CoreWindowStack                *stack,
                                                 const DFBWindowDescription     *desc,
                                                 CoreWindow                    **ret_window );

/*
 * Deinitialize a window and remove it from the window stack.
 */
void              dfb_window_destroy           ( CoreWindow                     *window );

/*
 * Change stacking class.
 */
DFBResult         dfb_window_change_stacking   ( CoreWindow                     *window,
                                                 DFBWindowStackingClass          stacking );

/*
 * Set window type hint.
 */
DFBResult         dfb_window_set_type_hint     ( CoreWindow                     *window,
                                                 DFBWindowTypeHint               type_hint );

/*
 * Change window hint flags.
 */
DFBResult         dfb_window_change_hint_flags ( CoreWindow                     *window,
                                                 DFBWindowHintFlags              clear,
                                                 DFBWindowHintFlags              set );

/*
 * Move a window up one step in window stack.
 */
DFBResult         dfb_window_raise             ( CoreWindow                     *window );

/*
 * Move a window down one step in window stack.
 */
DFBResult         dfb_window_lower             ( CoreWindow                     *window );

/*
 * Make a window the first (topmost) window in the window stack.
 */
DFBResult         dfb_window_raisetotop        ( CoreWindow                     *window );

/*
 * Make a window the last (downmost) window in the window stack.
 */
DFBResult         dfb_window_lowertobottom     ( CoreWindow                     *window );

/*
 * Stack a window on top of another one.
 */
DFBResult         dfb_window_putatop           ( CoreWindow                     *window,
                                                 CoreWindow                     *lower );

/*
 * Stack a window below another one.
 */
DFBResult         dfb_window_putbelow          ( CoreWindow                     *window,
                                                 CoreWindow                     *upper );

/*
 * Change window configuration.
 */
DFBResult         dfb_window_set_config        ( CoreWindow                     *window,
                                                 const CoreWindowConfig         *config,
                                                 DFBWindowConfigFlags            flags );

/*
 * Change window cursor.
 */
DFBResult         dfb_window_set_cursor_shape  ( CoreWindow                     *window,
                                                 CoreSurface                    *surface,
                                                 unsigned int                    hot_x,
                                                 unsigned int                    hot_y );

/*
 * Move a window relative to its current position.
 */
DFBResult         dfb_window_move              ( CoreWindow                     *window,
                                                 int                             x,
                                                 int                             y,
                                                 bool                            relative );

/*
 * Set window position and size.
 */
DFBResult         dfb_window_set_bounds        ( CoreWindow                     *window,
                                                 int                             x,
                                                 int                             y,
                                                 int                             width,
                                                 int                             height );

/*
 * Resize a window.
 */
DFBResult         dfb_window_resize            ( CoreWindow                     *window,
                                                 int                             width,
                                                 int                             height );

/*
 * Bind a window to this window.
 */
DFBResult         dfb_window_bind              ( CoreWindow                     *window,
                                                 CoreWindow                     *source,
                                                 int                             x,
                                                 int                             y );

/*
 * Unbind a window from this window.
 */
DFBResult         dfb_window_unbind            ( CoreWindow                     *window,
                                                 CoreWindow                     *source );

/*
 * Set window color.
 */
DFBResult         dfb_window_set_color         ( CoreWindow                     *window,
                                                 DFBColor                        color );

/*
 * Set window color key.
 */
DFBResult         dfb_window_set_colorkey      ( CoreWindow                     *window,
                                                 u32                             color_key );

/*
 * Set window global alpha factor.
 */
DFBResult         dfb_window_set_opacity       ( CoreWindow                     *window,
                                                 u8                              opacity );

/*
 * Set window options.
 */
DFBResult         dfb_window_change_options    ( CoreWindow                     *window,
                                                 DFBWindowOptions                disable,
                                                 DFBWindowOptions                enable );

/*
 * Disable alpha channel blending for one region of a window.
 */
DFBResult         dfb_window_set_opaque        ( CoreWindow                     *window,
                                                 const DFBRegion                *region );

/*
 * Manipulate the window event mask.
 */
DFBResult         dfb_window_change_events     ( CoreWindow                     *window,
                                                 DFBWindowEventType              disable,
                                                 DFBWindowEventType              enable );

/*
 * Select a mode for filtering keys on a focused window.
 */
DFBResult         dfb_window_set_key_selection ( CoreWindow                     *window,
                                                 DFBWindowKeySelection           selection,
                                                 const DFBInputDeviceKeySymbol  *keys,
                                                 unsigned int                    num_keys );

/*
 * Enable/disable a grabbing target of a window.
 */
DFBResult         dfb_window_change_grab       ( CoreWindow                     *window,
                                                 CoreWMGrabTarget                target,
                                                 bool                            grab );

/*
 * Grab a specific key for this window.
 */
DFBResult         dfb_window_grab_key          ( CoreWindow                     *window,
                                                 DFBInputDeviceKeySymbol         symbol,
                                                 DFBInputDeviceModifierMask      modifiers );

/*
 * Ungrab a specific key for this window.
 */
DFBResult         dfb_window_ungrab_key        ( CoreWindow                     *window,
                                                 DFBInputDeviceKeySymbol         symbol,
                                                 DFBInputDeviceModifierMask      modifiers );

/*
 * Post an event.
 */
void              dfb_window_post_event        ( CoreWindow                     *window,
                                                 DFBWindowEvent                 *event );

/*
 * Post a DWET_POSITION_SIZE event to request configuration.
 */
DFBResult         dfb_window_send_configuration( CoreWindow                     *window );

/*
 * Request a window to gain focus.
 */
DFBResult         dfb_window_request_focus     ( CoreWindow                     *window );

/*
 * Set window rotation.
 */
DFBResult         dfb_window_set_rotation      ( CoreWindow                     *window,
                                                 int                             rotation );

/**********************************************************************************************************************/

static __inline__ void
dfb_surface_caps_apply_policy( DFBWindowSurfacePolicy  policy,
                               DFBSurfaceCapabilities *caps )
{
     switch (policy) {
          case DWSP_SYSTEMONLY:
               *caps = (DFBSurfaceCapabilities) ((*caps & ~DSCAPS_VIDEOONLY) | DSCAPS_SYSTEMONLY);
               break;

          case DWSP_VIDEOONLY:
               *caps = (DFBSurfaceCapabilities) ((*caps & ~DSCAPS_SYSTEMONLY) | DSCAPS_VIDEOONLY);
               break;

          default:
               *caps = (DFBSurfaceCapabilities) (*caps & ~(DSCAPS_SYSTEMONLY | DSCAPS_VIDEOONLY));
               break;
     }
}

#endif

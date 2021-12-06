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

#ifndef __CORE__WINDOWSTACK_H__
#define __CORE__WINDOWSTACK_H__

#include <core/coretypes.h>
#include <fusion/call.h>
#include <fusion/fusion.h>
#include <fusion/reactor.h>
#include <fusion/vector.h>

/**********************************************************************************************************************/

typedef enum {
     CWSF_NONE        = 0x00000000,

     CWSF_INITIALIZED = 0x00000001,
     CWSF_ACTIVATED   = 0x00000002,

     CWSF_ALL         = 0x00000003
} CoreWindowStackFlags;

struct __DFB_CoreWindowStack {
     DirectLink                          link;

     int                                 magic;

     CoreLayerContext                   *context;         /* layer context */

     int                                 width;           /* stack width */
     int                                 height;          /* stack height */
     int                                 rotation;        /* stack rotation */
     int                                 rotated_width;   /* stack rotated width */
     int                                 rotated_height;  /* stack rotated height */
     DFBSurfaceBlittingFlags             rotated_blit;    /* stack rotated blitting flag */

     DFBWindowID                         id_pool;         /* window id pool */

     struct {
          int                            enabled;         /* cursor enabled */
          int                            x, y;            /* cursor position */
          DFBDimension                   size;            /* cursor shape size */
          DFBPoint                       hot;             /* cursor hot spot */
          CoreSurface                   *surface;         /* cursor shape */
          u8                             opacity;         /* cursor opacity */
          DFBRegion                      region;          /* cursor is clipped by this region */

          int                            numerator;       /* cursor acceleration factor numerator */
          int                            denominator;     /* cursor acceleration factor denominator */
          int                            threshold;       /* cursor acceleration threshold */

          bool                           set;             /* cursor enable/disable has been called at least one time */

          DFBWindowSurfacePolicy         policy;          /* cursor surface policy */
     } cursor;

     struct {
          DFBDisplayLayerBackgroundMode  mode;            /* background handling mode */

          DFBColor                       color;           /* color for solid background mode */
          int                            color_index;     /* color index for solid background mode */

          CoreSurface                   *image;           /* surface for background image mode */
          GlobalReaction                 image_reaction;  /* global reaction for background image */
     } bg;

     DirectLink                         *devices;         /* input devices attached to the stack */

     bool                                hw_mode;         /* recompositing is done by hardware */

     void                               *stack_data;      /* private data of window manager */

     FusionSHMPoolShared                *shmpool;         /* shared memory pool */

     CoreWindowStackFlags                flags;           /* state flags */

     FusionCall                          call;            /* dispatch */

     FusionDispatchCleanup              *motion_cleanup;  /* motion input dispatch cleanup */
     DFBInputEvent                       motion_x;        /* x motion */
     DFBInputEvent                       motion_y;        /* y motion */
     long long                           motion_ts;       /* micros */

     FusionVector                        visible_windows; /* list of visible windows */
};

/**********************************************************************************************************************/

/*
 * Create a window stack, initialize it and attach input devices for input events.
 */
CoreWindowStack *dfb_windowstack_create                  ( CoreLayerContext              *context );

/*
 * Detach input devices.
 */
void             dfb_windowstack_detach_devices          ( CoreWindowStack               *stack );

/*
 * Destroy a window stack.
 */
void             dfb_windowstack_destroy                 ( CoreWindowStack               *stack );

/*
 * Resize a window stack.
 */
void             dfb_windowstack_resize                  ( CoreWindowStack               *stack,
                                                           int                            width,
                                                           int                            height,
                                                           int                            rotation );

/*
 * Prohibit access to the window stack data (wait until stack is accessible).
 */
DirectResult     dfb_windowstack_lock                    ( CoreWindowStack               *stack );

/*
 * Allow access to the window stack data.
 */
DirectResult     dfb_windowstack_unlock                  ( CoreWindowStack               *stack );

/*
 * Repaints all window on a window stack.
 */
DFBResult dfb_windowstack_repaint_all                    ( CoreWindowStack               *stack );

/*
 * Background handling.
 */

DFBResult dfb_windowstack_set_background_mode            ( CoreWindowStack               *stack,
                                                           DFBDisplayLayerBackgroundMode  mode );

DFBResult dfb_windowstack_set_background_image           ( CoreWindowStack               *stack,
                                                           CoreSurface                   *image );

DFBResult dfb_windowstack_set_background_color           ( CoreWindowStack               *stack,
                                                           const DFBColor                *color );

DFBResult dfb_windowstack_set_background_color_index     ( CoreWindowStack               *stack,
                                                           int                            index );

/*
 * Cursor control.
 */

DFBResult dfb_windowstack_cursor_enable                  ( CoreDFB                       *core,
                                                           CoreWindowStack               *stack,
                                                           bool                           enable );

DFBResult dfb_windowstack_cursor_set_opacity             ( CoreWindowStack               *stack,
                                                           u8                             opacity );

DFBResult dfb_windowstack_cursor_set_shape               ( CoreWindowStack               *stack,
                                                           CoreSurface                   *shape,
                                                           int                            hot_x,
                                                           int                            hot_y );

DFBResult dfb_windowstack_cursor_warp                    ( CoreWindowStack               *stack,
                                                           int                            x,
                                                           int                            y );

DFBResult dfb_windowstack_cursor_set_acceleration        ( CoreWindowStack               *stack,
                                                           int                            numerator,
                                                           int                            denominator,
                                                           int                            threshold );

DFBResult dfb_windowstack_get_cursor_position            ( CoreWindowStack               *stack,
                                                           int                           *ret_x,
                                                           int                           *ret_y );

/*
 * Global reaction, listen to input device events.
 */
ReactionResult _dfb_windowstack_inputdevice_listener     ( const void                    *msg_data,
                                                           void                          *ctx );

/*
 * Global reaction, listen to the background image.
 */
ReactionResult _dfb_windowstack_background_image_listener( const void                    *msg_data,
                                                           void                          *ctx );

#endif

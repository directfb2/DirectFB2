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

#ifndef __DIRECTFB_WINDOWS_H__
#define __DIRECTFB_WINDOWS_H__

#include <directfb.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Window stack interface.
 */
D_DECLARE_INTERFACE( IDirectFBWindows )

/********************
 * IDirectFBWindows *
 ********************/

/*
 * Window configuration flags.
 */
typedef enum {
     DWCONF_NONE                           = 0x00000000,         /* none of these flags */

     DWCONF_POSITION                       = 0x00000001,         /* position */
     DWCONF_SIZE                           = 0x00000002,         /* size */
     DWCONF_OPACITY                        = 0x00000004,         /* opacity */
     DWCONF_STACKING                       = 0x00000008,         /* stacking */
     DWCONF_OPTIONS                        = 0x00000010,         /* options */
     DWCONF_EVENTS                         = 0x00000020,         /* events */
     DWCONF_ASSOCIATION                    = 0x00000040,         /* association */

     DWCONF_COLOR_KEY                      = 0x00000100,         /* color key */
     DWCONF_OPAQUE                         = 0x00000200,         /* opaque */
     DWCONF_COLOR                          = 0x00000400,         /* color */
     DWCONF_STEREO_DEPTH                   = 0x00000800,         /* stereo depth */
     DWCONF_KEY_SELECTION                  = 0x00001000,         /* key selection */
     DWCONF_CURSOR_FLAGS                   = 0x00002000,         /* cursor flags */
     DWCONF_CURSOR_RESOLUTION              = 0x00004000,         /* cursor resolution */

     DWCONF_SRC_GEOMETRY                   = 0x00010000,         /* source geometry */
     DWCONF_DST_GEOMETRY                   = 0x00020000,         /* destination geometry */
     DWCONF_ROTATION                       = 0x00040000,         /* rotation */
     DWCONF_APPLICATION_ID                 = 0x00080000,         /* application id */
     DWCONF_TYPE_HINT                      = 0x00100000,         /* type hint */
     DWCONF_HINT_FLAGS                     = 0x00200000,         /* hint flags */

     DWCONF_ALL                            = 0x003F7F7F          /* all of these */
} DFBWindowConfigFlags;

/*
 * Window configuration.
 */
typedef struct {
     DFBRectangle                            bounds;             /* position and size */
     int                                     opacity;            /* global alpha factor */
     DFBWindowStackingClass                  stacking;           /* level boundaries */
     DFBWindowOptions                        options;            /* flags for appearance/behaviour */
     DFBWindowEventType                      events;             /* mask of enabled events */
     DFBWindowID                             association;        /* ID of window which this is associated to */
     u32                                     color_key;          /* transparent pixel */
     DFBRegion                               opaque;             /* region of the window forced to be opaque */
     DFBColor                                color;              /* constant color (no surface needed) */
     DFBWindowKeySelection                   key_selection;      /* how to filter keys in focus */
     DFBWindowCursorFlags                    cursor_flags;       /* cursor flags */
     DFBDimension                            cursor_resolution;  /* cursor resolution */
     DFBWindowGeometry                       src_geometry;       /* advanced source geometry */
     DFBWindowGeometry                       dst_geometry;       /* advanced destination geometry */
     int                                     rotation;           /* rotation */
     u64                                     application_id;     /* application id */
     int                                     stereo_depth;       /* stereo depth */
     DFBWindowTypeHint                       type_hint;          /* type hint */
     DFBWindowHintFlags                      hint_flags;         /* hint flags */
} DFBWindowConfig;

/*
 * Window state flags.
 */
typedef enum {
     DWSTATE_NONE                          = 0x00000000,         /* None of these. */

     DWSTATE_INSERTED                      = 0x00000001,         /* Window is inserted. */
     DWSTATE_FOCUSED                       = 0x00000002,         /* Window is focused. */
     DWSTATE_ENTERED                       = 0x00000004,         /* Window is entered. */

     DWSTATE_ALL                           = 0x00000007          /* All of these. */
} DFBWindowStateFlags;

/*
 * Window state.
 */
typedef struct {
     DFBWindowStateFlags                     flags;              /* Window state flags. */
} DFBWindowState;

/*
 * Window information.
 */
typedef struct {
     DFBWindowID                             window_id;          /* Window ID */
     DFBWindowCapabilities                   caps;               /* Window capabilities */
     u64                                     resource_id;        /* Resource ID */
     DFBWindowConfig                         config;             /* Window configuration. */
     DFBWindowState                          state;              /* Window state */
     u32                                     process_id;         /* Fusion ID or other element identifying process. */
     u32                                     instance_id;        /* ID of the instance of an application. */
} DFBWindowInfo;

/*
 * Windows watcher callbacks.
 */
typedef struct {
     /*
      * Add window, called for each window existing at watcher
      * registration and each added afterwards.
      */
     void (*WindowAdd) (
          void                              *context,
          const DFBWindowInfo               *info
     );

     /*
      * Remove window, called for each window being removed.
      */
     void (*WindowRemove) (
          void                              *context,
          DFBWindowID                        window_id
     );

     /*
      * Change window configuration, called for each window
      * changing its configuration. The flags specify which
      * of the items have changed actually.
      */
     void (*WindowConfig) (
          void                              *context,
          DFBWindowID                        window_id,
          const DFBWindowConfig             *config,
          DFBWindowConfigFlags               flags
     );

     /*
      * Update window state, called for each window changing its
      * state. In case of insertion of a window, prior to this,
      * the watcher will receive the WindowRestack() call, which
      * contains the z-position at which the window has been
      * inserted.
      */
     void (*WindowState) (
          void                              *context,
          DFBWindowID                        window_id,
          const DFBWindowState              *state
     );

     /*
      * Update window z-position, called for each window
      * changing its z-position. In case of insertion of a
      * window, after this call, the watcher will receive the
      * WindowState() call, which indicates insertion of the
      * window. Upon reinsertion, only this call will be
      * received.
      */
     void (*WindowRestack) (
          void                              *context,
          DFBWindowID                        window_id,
          unsigned int                       index
     );

     /*
      * Switch window focus, called for each window getting the
      * focus.
      */
     void (*WindowFocus) (
          void                              *context,
          DFBWindowID                        window_id
     );
} DFBWindowsWatcher;

/*
 * IDirectFBWindows is the window stack interface.
 */
D_DEFINE_INTERFACE( IDirectFBWindows,

   /** Watching **/

     /*
      * Register a new windows watcher.
      */
     DFBResult (*RegisterWatcher) (
          IDirectFBWindows                  *thiz,
          const DFBWindowsWatcher           *watcher,
          void                              *context
     );

     /*
      * Unregister a windows watcher.
      */
     DFBResult (*UnregisterWatcher) (
          IDirectFBWindows                  *thiz,
          void                              *context
     );
)

#ifdef __cplusplus
}
#endif

#endif

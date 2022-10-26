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

#ifndef __CORE__WM_H__
#define __CORE__WM_H__

#include <core/coretypes.h>
#include <direct/modules.h>
#include <directfb_windows.h>
#include <fusion/reactor.h>

DECLARE_MODULE_DIRECTORY( dfb_wm_modules );

/**********************************************************************************************************************/

#define DFB_CORE_WM_ABI_VERSION          10

#define DFB_CORE_WM_INFO_NAME_LENGTH     60
#define DFB_CORE_WM_INFO_VENDOR_LENGTH   80
#define DFB_CORE_WM_INFO_URL_LENGTH     120
#define DFB_CORE_WM_INFO_LICENSE_LENGTH  40

typedef struct {
     int major; /* major version */
     int minor; /* minor version */
} CoreWMVersion;

typedef struct {
     CoreWMVersion version;

     char          name[DFB_CORE_WM_INFO_NAME_LENGTH];       /* Name of WM module */
     char          vendor[DFB_CORE_WM_INFO_VENDOR_LENGTH];   /* Vendor (or author) of the module */
     char          url[DFB_CORE_WM_INFO_URL_LENGTH];         /* URL for module updates */
     char          license[DFB_CORE_WM_INFO_LICENSE_LENGTH]; /* License, e.g. 'LGPL' or 'proprietary' */

     unsigned int  wm_data_size;                             /* WM local data size to allocate */
     unsigned int  wm_shared_size;                           /* WM shared data size to allocate */
     unsigned int  stack_data_size;                          /* WM shared stack data size to allocate */
     unsigned int  window_data_size;                         /* WM shared window data size to allocate */
} CoreWMInfo;

typedef struct {
     CoreWMGrabTarget           target;

     DFBInputDeviceKeySymbol    symbol;
     DFBInputDeviceModifierMask modifiers;
} CoreWMGrab;

typedef enum {
     CCUF_NONE     = 0x00000000,

     CCUF_ENABLE   = 0x00000001,
     CCUF_DISABLE  = 0x00000002,

     CCUF_POSITION = 0x00000010,
     CCUF_SIZE     = 0x00000020,
     CCUF_SHAPE    = 0x00000040,
     CCUF_OPACITY  = 0x00000080,

     CCUF_ALL      = 0x000000F3
} CoreCursorUpdateFlags;

typedef DFBEnumerationResult (*CoreWMWindowCallback)( CoreWindow *window, void *ctx );

typedef struct {
     void      (*GetWMInfo)          ( CoreWMInfo              *info );

     DFBResult (*Initialize)         ( CoreDFB                 *core,
                                       void                    *wm_data,
                                       void                    *shared_data );

     DFBResult (*Join)               ( CoreDFB                 *core,
                                       void                    *wm_data,
                                       void                    *shared_data );

     DFBResult (*Shutdown)           ( bool                     emergency,
                                       void                    *wm_data,
                                       void                    *shared_data );

     DFBResult (*Leave)              ( bool                     emergency,
                                       void                    *wm_data,
                                       void                    *shared_data );

     DFBResult (*Suspend)            ( void                    *wm_data,
                                       void                    *shared_data );

     DFBResult (*Resume)             ( void                    *wm_data,
                                       void                    *shared_data );

     DFBResult (*PostInit)           ( void                    *wm_data,
                                       void                    *shared_data );

     DFBResult (*InitStack)          ( CoreWindowStack         *stack,
                                       void                    *wm_data,
                                       void                    *stack_data );

     DFBResult (*CloseStack)         ( CoreWindowStack         *stack,
                                       void                    *wm_data,
                                       void                    *stack_data );

     DFBResult (*SetActive)          ( CoreWindowStack         *stack,
                                       void                    *wm_data,
                                       void                    *stack_data,
                                       bool                     active );

     DFBResult (*ResizeStack)        ( CoreWindowStack         *stack,
                                       void                    *wm_data,
                                       void                    *stack_data,
                                       int                      width,
                                       int                      height );

     DFBResult (*ProcessInput)       ( CoreWindowStack         *stack,
                                       void                    *wm_data,
                                       void                    *stack_data,
                                       const DFBInputEvent     *event );

     DFBResult (*FlushKeys)          ( CoreWindowStack         *stack,
                                       void                    *wm_data,
                                       void                    *stack_data );

     DFBResult (*WindowAt)           ( CoreWindowStack         *stack,
                                       void                    *wm_data,
                                       void                    *stack_data,
                                       int                      x,
                                       int                      y,
                                       CoreWindow             **ret_window );

     DFBResult (*WindowLookup)       ( CoreWindowStack         *stack,
                                       void                    *wm_data,
                                       void                    *stack_data,
                                       DFBWindowID              window_id,
                                       CoreWindow             **ret_window );

     DFBResult (*EnumWindows)        ( CoreWindowStack         *stack,
                                       void                    *wm_data,
                                       void                    *stack_data,
                                       CoreWMWindowCallback     callback,
                                       void                    *callback_ctx );

    DFBResult (*SetWindowProperty)   ( CoreWindowStack         *stack,
                                       void                    *wm_data,
                                       void                    *stack_data,
                                       CoreWindow              *window,
                                       void                    *window_data,
                                       const char              *key,
                                       void                    *value,
                                       void                   **old_value );

    DFBResult (*GetWindowProperty)   ( CoreWindowStack         *stack,
                                       void                    *wm_data,
                                       void                    *stack_data,
                                       CoreWindow              *window,
                                       void                    *window_data,
                                       const char              *key,
                                       void                   **value );

    DFBResult (*RemoveWindowProperty)( CoreWindowStack         *stack,
                                       void                    *wm_data,
                                       void                    *stack_data,
                                       CoreWindow              *window,
                                       void                    *window_data,
                                       const char              *key,
                                       void                   **value );

     DFBResult (*GetInsets)          ( CoreWindowStack         *stack,
                                       CoreWindow              *window,
                                       DFBInsets               *insets );

     DFBResult (*PreConfigureWindow) ( CoreWindowStack         *stack,
                                       void                    *wm_data,
                                       void                    *stack_data,
                                       CoreWindow              *window,
                                       void                    *window_data );

     DFBResult (*AddWindow)          ( CoreWindowStack         *stack,
                                       void                    *wm_data,
                                       void                    *stack_data,
                                       CoreWindow              *window,
                                       void                    *window_data );

     DFBResult (*RemoveWindow)       ( CoreWindowStack         *stack,
                                       void                    *wm_data,
                                       void                    *stack_data,
                                       CoreWindow              *window,
                                       void                    *window_data );

     DFBResult (*SetWindowConfig)    ( CoreWindow              *window,
                                       void                    *wm_data,
                                       void                    *window_data,
                                       const CoreWindowConfig  *config,
                                       DFBWindowConfigFlags     flags );

     DFBResult (*RestackWindow)      ( CoreWindow              *window,
                                       void                    *wm_data,
                                       void                    *window_data,
                                       CoreWindow              *relative,
                                       void                    *relative_data,
                                       int                      relation );

     DFBResult (*Grab)               ( CoreWindow              *window,
                                       void                    *wm_data,
                                       void                    *window_data,
                                       CoreWMGrab              *grab );

     DFBResult (*Ungrab)             ( CoreWindow              *window,
                                       void                    *wm_data,
                                       void                    *window_data,
                                       CoreWMGrab              *grab );

     DFBResult (*RequestFocus)       ( CoreWindow              *window,
                                       void                    *wm_data,
                                       void                    *window_data );

     DFBResult (*BeginUpdates)       ( CoreWindow              *window,
                                       void                    *wm_data,
                                       void                    *window_data,
                                       const DFBRegion         *update );

     DFBResult (*SetCursorPosition)  ( CoreWindow              *window,
                                       void                    *wm_data,
                                       void                    *window_data,
                                       int                      x,
                                       int                      y );

     DFBResult (*UpdateStack)        ( CoreWindowStack         *stack,
                                       void                    *wm_data,
                                       void                    *stack_data,
                                       const DFBRegion         *region,
                                       DFBSurfaceFlipFlags      flags );

     DFBResult (*UpdateWindow)       ( CoreWindow              *window,
                                       void                    *wm_data,
                                       void                    *window_data,
                                       const DFBRegion         *left_region,
                                       const DFBRegion         *right_region,
                                       DFBSurfaceFlipFlags      flags );

     DFBResult (*UpdateCursor)       ( CoreWindowStack         *stack,
                                       void                    *wm_data,
                                       void                    *stack_data,
                                       CoreCursorUpdateFlags    flags );
} CoreWMFuncs;

typedef enum {
     CORE_WM_WINDOW_ADD     = 0x00000001,
     CORE_WM_WINDOW_REMOVE  = 0x00000002,
     CORE_WM_WINDOW_CONFIG  = 0x00000003,
     CORE_WM_WINDOW_STATE   = 0x00000004,
     CORE_WM_WINDOW_RESTACK = 0x00000005,
     CORE_WM_WINDOW_FOCUS   = 0x00000006,

     _CORE_WM_NUM_CHANNELS  = 0x00000007
} CoreWMChannels;

typedef struct {
     DFBWindowInfo        info;
} CoreWM_WindowAdd;

typedef struct {
     DFBWindowID          window_id;
} CoreWM_WindowRemove;

typedef struct {
     DFBWindowID          window_id;
     DFBWindowConfig      config;
     DFBWindowConfigFlags flags;
} CoreWM_WindowConfig;

typedef struct {
     DFBWindowID          window_id;
     DFBWindowState       state;
} CoreWM_WindowState;

typedef struct {
     DFBWindowID          window_id;
     unsigned int         index;
} CoreWM_WindowRestack;

typedef struct {
     DFBWindowID          window_id;
} CoreWM_WindowFocus;

/**********************************************************************************************************************/

DFBResult dfb_wm_deactivate_all_stacks ( void                    *data );

DFBResult dfb_wm_close_all_stacks      ( void                    *data );

DFBResult dfb_wm_attach                ( CoreDFB                 *core,
                                         int                      channel,
                                         ReactionFunc             func,
                                         void                    *ctx,
                                         Reaction                *reaction );

DFBResult dfb_wm_detach                ( CoreDFB                 *core,
                                         Reaction                *reaction );

DFBResult dfb_wm_dispatch_WindowAdd    ( CoreDFB                 *core,
                                         CoreWindow              *window );

DFBResult dfb_wm_dispatch_WindowRemove ( CoreDFB                 *core,
                                         CoreWindow              *window );

DFBResult dfb_wm_dispatch_WindowConfig ( CoreDFB                 *core,
                                         CoreWindow              *window,
                                         DFBWindowConfigFlags     flags );

DFBResult dfb_wm_dispatch_WindowState  ( CoreDFB                 *core,
                                         CoreWindow              *window );

DFBResult dfb_wm_dispatch_WindowRestack( CoreDFB                 *core,
                                         CoreWindow              *window,
                                         unsigned int             index );

DFBResult dfb_wm_dispatch_WindowFocus  ( CoreDFB                 *core,
                                         CoreWindow              *window );

void *dfb_wm_get_data                  ( void );

DFBResult dfb_wm_post_init             ( CoreDFB                 *core );

DFBResult dfb_wm_init_stack            ( CoreWindowStack         *stack );

DFBResult dfb_wm_close_stack           ( CoreWindowStack         *stack );

DFBResult dfb_wm_set_active            ( CoreWindowStack         *stack,
                                         bool                     active );

DFBResult dfb_wm_resize_stack          ( CoreWindowStack         *stack,
                                         int                      width,
                                         int                      height );

DFBResult dfb_wm_process_input         ( CoreWindowStack         *stack,
                                         const DFBInputEvent     *event );

DFBResult dfb_wm_flush_keys            ( CoreWindowStack         *stack );

DFBResult dfb_wm_window_at             ( CoreWindowStack         *stack,
                                         int                      x,
                                         int                      y,
                                         CoreWindow             **ret_window );

DFBResult dfb_wm_window_lookup         ( CoreWindowStack         *stack,
                                         DFBWindowID              window_id,
                                         CoreWindow             **ret_window );

DFBResult dfb_wm_enum_windows          ( CoreWindowStack         *stack,
                                         CoreWMWindowCallback     callback,
                                         void                    *callback_ctx );

DFBResult dfb_wm_get_insets            ( CoreWindowStack         *stack,
                                         CoreWindow              *window,
                                         DFBInsets               *ret_insets );

DFBResult dfb_wm_preconfigure_window   ( CoreWindowStack         *stack,
                                         CoreWindow              *window );

DFBResult dfb_wm_add_window            ( CoreWindowStack         *stack,
                                         CoreWindow              *window );

DFBResult dfb_wm_remove_window         ( CoreWindowStack         *stack,
                                         CoreWindow              *window );

DFBResult dfb_wm_set_window_property   ( CoreWindowStack         *stack,
                                         CoreWindow              *window,
                                         const char              *key,
                                         void                    *value,
                                         void                   **ret_old_value );

DFBResult dfb_wm_get_window_property   ( CoreWindowStack         *stack,
                                         CoreWindow              *window,
                                         const char              *key,
                                         void                   **ret_value );

DFBResult dfb_wm_remove_window_property( CoreWindowStack         *stack,
                                         CoreWindow              *window,
                                         const char              *key,
                                         void                   **ret_value );

DFBResult dfb_wm_set_window_config     ( CoreWindow              *window,
                                         const CoreWindowConfig  *config,
                                         DFBWindowConfigFlags     flags );

DFBResult dfb_wm_restack_window        ( CoreWindow              *window,
                                         CoreWindow              *relative,
                                         int                      relation );

DFBResult dfb_wm_grab                  ( CoreWindow              *window,
                                         CoreWMGrab              *grab );

DFBResult dfb_wm_ungrab                ( CoreWindow              *window,
                                         CoreWMGrab              *grab );

DFBResult dfb_wm_request_focus         ( CoreWindow              *window );

DFBResult dfb_wm_begin_updates         ( CoreWindow              *window,
                                         const DFBRegion         *update );

DFBResult dfb_wm_set_cursor_position   ( CoreWindow              *window,
                                         int                      x,
                                         int                      y );

DFBResult dfb_wm_update_stack          ( CoreWindowStack         *stack,
                                         const DFBRegion         *region,
                                         DFBSurfaceFlipFlags      flags );

DFBResult dfb_wm_update_window         ( CoreWindow              *window,
                                         const DFBRegion         *left_region,
                                         const DFBRegion         *right_region,
                                         DFBSurfaceFlipFlags      flags );

DFBResult dfb_wm_update_cursor         ( CoreWindowStack         *stack,
                                         CoreCursorUpdateFlags    flags );

#endif

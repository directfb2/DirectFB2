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

#ifndef __MISC__CONF_H__
#define __MISC__CONF_H__

#include <core/coredefs.h>
#include <core/coretypes.h>
#include <fusion/types.h>

/**********************************************************************************************************************/

typedef struct {
     bool                                init;

     DFBDisplayLayerConfig               config;

     DFBColor                            src_key;
     int                                 src_key_index;

     struct {
          DFBDisplayLayerBackgroundMode  mode;
          DFBColor                       color;
          int                            color_index;
          char                          *filename;
     } background;

     DFBWindowStackingClass              stacking;

     DFBColor                           *palette;
     bool                                palette_set;

     int                                 rotate;
     bool                                rotate_set;
} DFBConfigLayer;

typedef enum {
     DCWF_NONE            = 0x00000000,

     DCWF_CREATE_SURFACE  = 0x00000001,
     DCWF_CREATE_WINDOW   = 0x00000002,

     DCWF_ALLOCATE_BUFFER = 0x00000010,

     DCWF_ALL             = 0x00000013
} DFBConfigWarnFlags;

typedef struct
{
     char                       *system;
     char                       *wm;
     bool                        banner;
     bool                        sync;
     FusionCallExecFlags         call_nodirect;
     bool                        block_all_signals;
     bool                        core_sighandler;
     bool                        ownership_check;
     bool                        deinit_check;
     char                       *resource_manager;
     int                         session;
     long long                   screen_frame_interval;
     DFBSurfaceID                primary_id;
     int                         primary_layer;
     bool                        primary_only;
     struct {
          int                    width;
          int                    height;
          int                    depth;
          DFBSurfacePixelFormat  format;
     } mode;
     DFBConfigLayer              layers[MAX_LAYERS];
     DFBConfigLayer             *config_layer;
     unsigned int                graphics_state_call_limit;
     bool                        software_only;
     bool                        hardware_only;
     bool                        software_warn;
     bool                        software_trace;
     unsigned int                gfxcard_stats;
     unsigned int                videoram_limit;
     bool                        gfx_emit_early;
     bool                        startstop;
     DFBSurfaceRenderOptions     render_options;
     int                         keep_accumulators;
     bool                        mmx;
     bool                        neon;
     struct {
          DFBConfigWarnFlags     flags;
          struct {
               DFBDimension      min_size;
          } create_surface;
          struct {
               DFBDimension      min_size;
          } allocate_buffer;
     } warn;
     bool                        surface_clear;
     bool                        thrifty_surface_buffers;
     int                         surface_shmpool_size;
     unsigned int                system_surface_align_base;
     unsigned int                system_surface_align_pitch;
     long long                   max_frame_advance;
     bool                        force_frametime;
     bool                        subsurface_caching;
     int                         window_policy;
     bool                        single_window;
     bool                        translucent_windows;
     bool                        force_windowed;
     struct {
          int                    width;
          int                    height;
     } scaled;
     bool                        autoflip_window;
     bool                        no_cursor;
     bool                        cursor_videoonly;
     unsigned long               cursor_resource_id;
     bool                        cursor_automation;
     bool                        discard_repeat_events;
     bool                        lefty;
     bool                        capslock_meta;
     char                       *screenshot_dir;
     DFBSurfacePixelFormat       font_format;
     bool                        font_premult;
     unsigned long               font_resource_id;
     int                         max_font_rows;
     int                         max_font_row_width;
} DFBConfig;

/**********************************************************************************************************************/

extern DFBConfig  *dfb_config;

extern const char *dfb_config_usage;

/*
 * Set indiviual option.
 */
DFBResult dfb_config_set   ( const char  *name,
                             const char  *value );

/*
 * Allocate config struct, fill with defaults and parse command line options for overrides.
 */
DFBResult dfb_config_init  ( int         *argc,
                             char       **argv[] );

/*
 * Free config struct.
 */
void      dfb_config_deinit( void );

#endif

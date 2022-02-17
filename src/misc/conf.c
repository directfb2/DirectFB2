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

#include <direct/filesystem.h>
#include <direct/memcpy.h>
#include <direct/system.h>
#include <direct/util.h>
#include <directfb_util.h>
#include <directfb_version.h>
#include <fusion/conf.h>
#include <misc/conf.h>

D_DEBUG_DOMAIN( DirectFB_Config, "DirectFB/Config", "DirectFB Runtime Configuration options" );

/**********************************************************************************************************************/

DFBConfig         *dfb_config = NULL;
static const char *dfb_config_usage =
     "\n"
     " --dfb-help                      Output DirectFB usage information and exit\n"
     " --dfb:<option>[,<option>...]    Pass options to DirectFB (see below)\n"
     "\n"
     "DirectFB options:\n"
     "\n"
     "  help                           Output DirectFB usage information and exit\n"
     "  include=<config file>          Include the specified file, relative to the current file\n"
     "  system=<system>                Specify the system ('drmkms', 'fbdev', etc.)\n"
     "  wm=<wm>                        Specify the window manager module ('default', etc.)\n"
     "  [no-]banner                    Show DirectFB banner at startup (default enabled)\n"
     "  [no-]sync                      Flush all disk buffers before initializing DirectFB\n"
     "  [no-]always-indirect           Use purely indirect Flux calls (for secure master)\n"
     "  [no-]block-all-signals         Block all signals\n"
     "  [no-]core-sighandler           Enable core signal handler, for emergency shutdowns (default enabled)\n"
     "  [no-]ownership-check           Check privileges when calling GetSurface() or GetWindow() (default enabled)\n"
     "  [no-]deinit-check              Check if all allocated resources have been released on exit (default enabled)\n"
     "  [no-]shutdown-info             Dump objects from all pools if some objects remain alive\n"
     "  resource-manager=<impl>        Specify a resource manager implementation\n"
     "  session=<num>                  Select the multi app world which is joined (starting with 0) or created (-1)\n"
     "  screen-frame-interval=<us>     Screen refresh interval used if not defined by the encoder (default = 16666)\n"
     "  [no-]primary-only              Tell application only about the primary layer\n"
     "  primary-id=<surface-id>        Set ID of primary surface to use\n"
     "  primary-layer=<layer-id>       Select the primary layer (default is the first)\n"
     "  mode=<width>x<height>          Set the default screen resolution\n"
     "  depth=<pixeldepth>             Set the default pixel depth in bits per pixel\n"
     "  pixelformat=<pixelformat>      Set the default pixel format\n"
     "  [no-]init-layer=<id>           Initialize layer with ID\n"
     "  [layer-]size=<width>x<height>  Set the pixel resolution\n"
     "  [layer-]depth=<pixeldepth>     Set the pixel depth\n"
     "  [layer-]format=<pixelformat>   Set the pixel format\n"
     "  [layer-]buffer-mode=<mode>     Specify the buffer mode\n"
     "                                 [ auto | triple | backvideo | backsystem | frontonly | windows ]\n"
     "                                 auto:       DirectFB decides depending on hardware capabilities\n"
     "                                 triple:     Triple buffering (allocations in video memory only)\n"
     "                                 backvideo:  Front and back buffer are allocated in video memory\n"
     "                                 backsystem: The back buffer is allocated in system memory\n"
     "                                 frontonly:  There is no back buffer\n"
     "                                 windows:    Special mode with window buffers directly displayed\n"
     "  [layer-]src-key=<AARRGGBB>     Enable color keying (hexadecimal)\n"
     "  [layer-]src-key-index=<index>  Enable color keying index (decimal)\n"
     "  [layer-]bg-none                Disable background handling\n"
     "  [layer-]bg-image=<filename>    Use background image\n"
     "  [layer-]bg-tile=<filename>     Use tiled background image\n"
     "  [layer-]bg-color=<AARRGGBB>    Use background color (hexadecimal)\n"
     "  [layer-]bg-color-index=<index> Use background color index (decimal)\n"
     "  [layer-]stacking=<classes>     Set stacking classes\n"
     "  [layer-]palette-<i>=<AARRGGBB> Set palette entry at decimal index 'i' (hexadecimal)\n"
     "  [layer-]rotate=<degree>        Set the layer rotation for double buffer mode (0,90,180,270)\n"
     "  graphics-state-call-limit=<n>  Set FusionCall quota for graphics state object (default = 5000)\n"
     "  [no-]hardware                  Turn hardware acceleration on (default enabled)\n"
     "  [no-]software                  Enable software fallbacks (default enabled)\n"
     "  [no-]software-warn             Show warnings when doing/dropping software operations\n"
     "  [no-]software-trace            Show every stage of the software rendering pipeline\n"
     "  [no-]gfxcard-stats=[<ms>]      Print GPU usage statistics periodically (1000 ms if no period is specified)\n"
     "  videoram-limit=<amount>        Limit the amount of Video RAM used (kilobytes)\n"
     "  [no-]gfx-emit-early            Early emit GFX commands to prevent being IDLE\n"
     "  [no-]startstop                 Issue StartDrawing/StopDrawing to driver\n"
     "  [no-]smooth-upscale            Enable smooth upscaling\n"
     "  [no-]smooth-downscale          Enable smooth downscaling\n"
     "  keep-accumulators=<limit>      Free accumulators above the limit (default = 1024)\n"
     "                                 Setting -1 never frees accumulators until the state is destroyed\n"
     "  [no-]mmx                       Enable MMX assembly support (enabled by default if available)\n"
     "  [no-]neon                      Enable NEON assembly support (enabled by default if available)\n"
     "  warn=<type[:<width>x<height>]> Print warnings on surface/window creations or surface buffer allocations\n"
     "                                 [ create-surface | create-window | allocate-buffer ]\n"
     "  [no-]surface-clear             Clear all surface buffers after creation\n"
     "  [no-]thrifty-surface-buffers   Release system instance while video instance is alive\n"
     "  surface-shmpool-size=<kb>      Set the size of the shared memory pool used for shared system memory surfaces\n"
     "  system-surface-base-alignment=<byte alignment>\n"
     "                                 If GPU supports system memory, set the byte alignment for system memory based\n"
     "                                 surface's base address (value must be a positive power of two that is four or\n"
     "                                 greater), or zero for no alignment: aligning the base address (along with the\n"
     "                                 pitch) allows the data to travel more efficiently through the CPU and memory\n"
     "                                 bus to increase performance, and meet GPU requirements\n"
     "  system-surface-pitch-alignment=<byte alignment>\n"
     "                                 If GPU supports system memory, set the pitch alignment for system memory based\n"
     "                                 system memory based surface's pitch (value must be a positive power of two),\n"
     "                                 or zero for no alignment\n"
     "  max-frame-advance=<us>         Set the maximum time ahead for rendering frames (default 100000)\n"
     "  [no-]force-frametime           Call GetFrameTime() before each Flip() automatically\n"
     "  [no-]subsurface-caching        Optimize the recreation of sub-surfaces\n"
     "  window-surface-policy=<policy> Specify the swapping policy for window surfaces (default = auto)\n"
     "                                 [ auto | videohigh | videolow | systemonly | videoonly ]\n"
     "                                 auto:       DirectFB decides depending on hardware capabilities\n"
     "                                 videohigh:  Swapping system/video with high priority\n"
     "                                 videolow:   Swapping system/video with low priority\n"
     "                                 systemonly: Window surfaces are stored in system memory\n"
     "                                 videoonly:  Window surfaces are stored in video memory\n"
     "  [no-]single-window             Set configuration on region when window changes its attributes\n"
     "  [no-]translucent-windows       Allow translucent windows (default enabled)\n"
     "  [no-]force-windowed            Force the primary surface to be a window\n"
     "  scaled=<width>x<height>        Scale the window to this size for 'force-windowed' apps\n"
     "  [no-]autoflip-window           Auto flip non-flipping windowed primary surfaces (default enabled)\n"
     "  [no-]cursor                    Create a cursor when an application makes use of windows (default enabled)\n"
     "  [no-]cursor-videoonly          Make the cursor a video only surface\n"
     "  cursor-resource-id=<id>        Specify a resource id for the cursor surface\n"
     "  [no-]cursor-automation         Automated cursor show/hide for windowed primary surfaces\n"
     "  [no-]discard-repeat-events     Discard repeat events\n"
     "  [no-]capslock-meta             Map the CapsLock key to Meta\n"
     "  [no-]lefty                     Swap left and right mouse buttons\n"
     "  screenshot-dir=<directory>     Dump screen content on <Print> key presses\n"
     "  font-format=<pixelformat>      Set the preferred font format (default is 8 bit alpha)\n"
     "  [no-]font-premult              Enable premultiplied glyph images in ARGB format (default enabled)\n"
     "  font-resource-id=<id>          Resource ID to use for font cache row surfaces\n"
     "  max-font-rows=<number>         Maximum number of glyph cache rows (default = 99)\n"
     "  max-font-row-width=<pixels>    Maximum width of glyph cache row surface (default = 2048)\n"
     "\n";

/**********************************************************************************************************************/

static void
print_config_usage()
{
    fprintf( stderr, "DirectFB version %d.%d.%d\n",
             DIRECTFB_MAJOR_VERSION, DIRECTFB_MINOR_VERSION, DIRECTFB_MICRO_VERSION );

    fprintf( stderr, "%s", dfb_config_usage );

    fprintf( stderr, "%s%s", fusion_config_usage, direct_config_usage );
}

static DFBResult
parse_args( const char *args )
{
     int   len = strlen( args );
     char *name, *buf;

     buf = D_MALLOC( len + 1 );
     if (!buf)
          return D_OOM();

     name = buf;

     direct_memcpy( name, args, len + 1 );

     while (name && name[0]) {
          DFBResult  ret;
          char      *value;
          char      *next;

          if ((next = strchr( name, ',' )) != NULL)
               *next++ = '\0';

          if (strcmp( name, "help" ) == 0) {
               print_config_usage();
               exit( 1 );
          }

          if (strcmp( name, "memcpy=help" ) == 0) {
               direct_print_memcpy_routines();
               exit( 1 );
          }

          if ((value = strchr( name, '=' )) != NULL)
               *value++ = '\0';

          ret = dfb_config_set( name, value );
          switch (ret) {
               case DFB_OK:
                    break;
               default:
                    D_ERROR( "DirectFB/Config: Invalid option '%s' in args!\n", name );
                    D_FREE( buf );
                    return ret;
          }

          name = next;
     }

     D_FREE( buf );

     return DFB_OK;
}

static int
config_read_cmdline( char       *cmdbuf,
                     int         size,
                     DirectFile *f )
{
     size_t bytes;
     int    len = 0;

     direct_file_read( f, cmdbuf, 1, &bytes );

     if (bytes == 1 && *cmdbuf == 0) {
          direct_file_read( f, cmdbuf, 1, &bytes );
     }

     while (bytes == 1 && len < (size - 1)) {
          len++;
          direct_file_read( f, ++cmdbuf, 1, &bytes );
          if (*cmdbuf == 0)
               break;
     }

     if (len) {
          cmdbuf[len]=0;
     }

     return len != 0;
}

static void
config_allocate()
{
     int i;

     if (dfb_config)
          return;

     dfb_config = D_CALLOC( 1, sizeof(DFBConfig) );

     if (direct_access( "/dev/dri/card0", O_RDWR ) == DR_OK)
          dfb_config->system                           = D_STRDUP( "drmkms" );
     else if (direct_access( "/dev/fb0", O_RDWR ) == DR_OK)
          dfb_config->system                           = D_STRDUP( "fbdev" );

     dfb_config->banner                                = true;

     dfb_config->core_sighandler                       = true;

     dfb_config->ownership_check                       = true;
     dfb_config->deinit_check                          = true;

     dfb_config->screen_frame_interval                 = 16666;

     dfb_config->layers[0].init                        = true;
     dfb_config->layers[0].stacking                    = (1 << DWSC_UPPER) | (1 << DWSC_MIDDLE) | (1 << DWSC_LOWER);
     for (i = 0; i < D_ARRAY_SIZE(dfb_config->layers); i++) {
          dfb_config->layers[i].src_key_index          = -1;
          dfb_config->layers[i].background.mode        = DLBM_COLOR;
          dfb_config->layers[i].background.color_index = -1;
     }

     dfb_config->graphics_state_call_limit             = 5000;

     dfb_config->keep_accumulators                     = 1024;

     dfb_config->mmx                                   = true;
     dfb_config->neon                                  = true;

     dfb_config->surface_shmpool_size                  = 64 * 1024 * 1024;

     dfb_config->max_frame_advance                     = 100000;

     dfb_config->window_policy                         = -1;
     dfb_config->translucent_windows                   = true;
     dfb_config->autoflip_window                       = true;

     dfb_config->font_format                           = DSPF_A8;
     dfb_config->font_premult                          = true;
     dfb_config->max_font_rows                         = 99;
     dfb_config->max_font_row_width                    = 2048;
}

static DFBResult
config_read( const char *filename )
{
     DFBResult   ret = DFB_OK;
     DirectFile  f;
     char        line[400];
     char       *slash = 0;
     char       *cwd   = 0;

     config_allocate();

     dfb_config->config_layer = &dfb_config->layers[0];

     ret = direct_file_open( &f, filename, O_RDONLY, 0 );
     if (ret) {
          D_DEBUG_AT( DirectFB_Config, "Unable to open config file '%s'!\n", filename );
          return DFB_IO;
     } else {
          D_DEBUG_AT( DirectFB_Config, "Parsing config file '%s'\n", filename );
     }

     /* Store the current working directory for the 'include' command. */
     slash = strrchr( filename, '/' );
     if (slash) {
          char nwd[strlen( filename )];

          cwd = D_MALLOC( PATH_MAX );
          if (!cwd) {
               direct_file_close( &f );
               return D_OOM();
          }

          ret = direct_dir_get_current( cwd, PATH_MAX );
          if (ret) {
               D_FREE( cwd );
               direct_file_close( &f );
               return ret;
          }

          strcpy( nwd, filename );
          nwd[slash-filename] = 0;
          ret = direct_dir_change( nwd );
          if (ret)
               D_WARN( "failed to change directory to %s", nwd );
          else
               D_DEBUG_AT( DirectFB_Config, "Changing configuration lookup directory to '%s'\n", nwd );
     }

     while (!direct_file_get_string( &f, line, 400 )) {
          char *name    = line;
          char *comment = strchr( line, '#' );
          char *value;

          if (comment) {
               *comment = 0;
          }

          value = strchr( line, '=' );

          if (value) {
               *value++ = 0;
               direct_trim( &value );
          }

          direct_trim( &name );

          if (!*name || *name == '#')
               continue;

          ret = dfb_config_set( name, value );
          if (ret) {
               D_ERROR( "DirectFB/Config: Invalid option '%s' in config file '%s'!\n", name, filename );
               break;
          }
     }

     direct_file_close( &f );

     /* Restore the original current working directory. */
     if (cwd) {
          if (direct_dir_change( cwd ))
               D_WARN( "failed to change directory to %s", cwd );
          else
               D_DEBUG_AT( DirectFB_Config, "Back to directory '%s'\n", cwd );

          D_FREE( cwd );
     }

     return ret;
}

/**********************************************************************************************************************/

DFBResult
dfb_config_set( const char *name,
                const char *value )
{
     bool dfboption = true;

     if (strcmp( name, "include" ) == 0) {
          if (value) {
               DFBResult ret;

               ret = config_read( value );
               if (ret)
                    return ret;
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No include file name specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp( name, "system" ) == 0) {
          if (value) {
               if (dfb_config->system)
                    D_FREE( dfb_config->system );

               dfb_config->system = D_STRDUP( value );
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No system specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp( name, "wm" ) == 0) {
          if (value) {
               if (dfb_config->wm)
                    D_FREE( dfb_config->wm );

               dfb_config->wm = D_STRDUP( value );
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No window manager module specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp( name, "banner" ) == 0) {
          dfb_config->banner = true;
     } else
     if (strcmp( name, "no-banner" ) == 0) {
          dfb_config->banner = false;
     } else
     if (strcmp( name, "sync" ) == 0) {
          dfb_config->sync = true;
     } else
     if (strcmp( name, "no-sync" ) == 0) {
          dfb_config->sync = false;
     } else
     if (strcmp( name, "always-indirect" ) == 0) {
          dfb_config->call_nodirect = FCEF_NODIRECT;
     } else
     if (strcmp( name, "no-always-indirect" ) == 0) {
          dfb_config->call_nodirect = FCEF_NONE;
     } else
     if (strcmp( name, "block-all-signals" ) == 0) {
          dfb_config->block_all_signals = true;
     } else
     if (strcmp( name, "no-block-all-signals" ) == 0) {
          dfb_config->block_all_signals = false;
     } else
     if (strcmp( name, "core-sighandler" ) == 0) {
          dfb_config->core_sighandler = true;
     } else
     if (strcmp( name, "no-core-sighandler" ) == 0) {
          dfb_config->core_sighandler = false;
     } else
     if (strcmp( name, "ownership-check" ) == 0) {
          dfb_config->ownership_check = true;
     } else
     if (strcmp( name, "no-ownership-check" ) == 0) {
          dfb_config->ownership_check = false;
     } else
     if (strcmp( name, "deinit-check" ) == 0) {
          dfb_config->deinit_check = true;
     } else
     if (strcmp( name, "no-deinit-check" ) == 0) {
          dfb_config->deinit_check = false;
     } else
     if (strcmp( name, "shutdown-info" ) == 0) {
          dfb_config->shutdown_info = true;
     } else
     if (strcmp( name, "no-shutdown-info" ) == 0) {
          dfb_config->shutdown_info = false;
     } else
     if (strcmp( name, "resource-manager" ) == 0) {
          if (value) {
               if (dfb_config->resource_manager)
                    D_FREE( dfb_config->resource_manager );

               dfb_config->resource_manager = D_STRDUP( value );
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No implementation specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp( name, "session" ) == 0) {
          if (value) {
               int session;

               if (sscanf( value, "%d", &session ) < 1) {
                    D_ERROR( "DirectFB/Config: '%s': Could not parse value!\n", name );
                    return DFB_INVARG;
               }

               dfb_config->session = session;
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp( name, "screen-frame-interval" ) == 0) {
          if (value) {
               long long interval;

               if (sscanf( value, "%lld", &interval ) < 1) {
                    D_ERROR("DirectFB/Config '%s': Could not parse value!\n", name);
                    return DFB_INVARG;
               }

               dfb_config->screen_frame_interval = interval;
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp( name, "primary-id" ) == 0) {
          if (value) {
               unsigned int id;

               if (sscanf( value, "%u", &id ) < 1) {
                    D_ERROR( "DirectFB/Config: '%s': Could not parse id!\n", name );
                    return DFB_INVARG;
               }

               dfb_config->primary_id = id;
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No id specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp( name, "primary-layer" ) == 0) {
          if (value) {
               int id;

               if (sscanf( value, "%d", &id ) < 1) {
                    D_ERROR( "DirectFB/Config: '%s': Could not parse id!\n", name );
                    return DFB_INVARG;
               }

               dfb_config->primary_layer = id;
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No id specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp( name, "primary-only" ) == 0) {
          dfb_config->primary_only = true;
     } else
     if (strcmp( name, "no-primary-only" ) == 0) {
          dfb_config->primary_only = false;
     } else
     if (strcmp( name, "init-layer" ) == 0) {
          if (value) {
               int id;

               if (sscanf( value, "%d", &id ) < 1) {
                    D_ERROR( "DirectFB/Config: '%s': Could not parse id!\n", name );
                    return DFB_INVARG;
               }

               if (id < 0 || id > D_ARRAY_SIZE(dfb_config->layers)) {
                    D_ERROR( "DirectFB/Config: '%s': id %d out of bounds!\n", name, id );
                    return DFB_INVARG;
               }

               dfb_config->layers[id].init = true;

               dfb_config->config_layer = &dfb_config->layers[id];
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No id specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp( name, "no-init-layer" ) == 0) {
          if (value) {
               int id;

               if (sscanf( value, "%d", &id ) < 1) {
                    D_ERROR( "DirectFB/Config: '%s': Could not parse id!\n", name );
                    return DFB_INVARG;
               }

               if (id < 0 || id > D_ARRAY_SIZE(dfb_config->layers)) {
                    D_ERROR( "DirectFB/Config: '%s': id %d out of bounds!\n", name, id );
                    return DFB_INVARG;
               }

               dfb_config->layers[id].init = false;

               dfb_config->config_layer = &dfb_config->layers[id];
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No id specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp( name, "mode" ) == 0 || strcmp( name, "layer-size" ) == 0) {
          DFBConfigLayer *conf = dfb_config->config_layer;

          if (value) {
               int width, height;

               if (sscanf( value, "%dx%d", &width, &height ) < 2) {
                    D_ERROR( "DirectFB/Config: '%s': Could not parse size!\n", name );
                    return DFB_INVARG;
               }

               if (conf == &dfb_config->layers[0]) {
                    dfb_config->mode.width  = width;
                    dfb_config->mode.height = height;
               }

               conf->config.width   = width;
               conf->config.height  = height;
               conf->config.flags  |= DLCONF_WIDTH | DLCONF_HEIGHT;
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No size specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp( name, "depth" ) == 0 || strcmp( name, "layer-depth" ) == 0) {
          DFBConfigLayer *conf = dfb_config->config_layer;

          if (value) {
               int depth;

               if (sscanf( value, "%d", &depth ) < 1) {
                    D_ERROR( "DirectFB/Config: '%s': Could not parse depth!\n", name );
                    return DFB_INVARG;
               }

               if (conf == &dfb_config->layers[0]) {
                    dfb_config->mode.depth = depth;
               }

               conf->config.pixelformat  = dfb_pixelformat_for_depth( depth );
               conf->config.flags       |= DLCONF_PIXELFORMAT;
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No depth specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp( name, "pixelformat" ) == 0 || strcmp( name, "layer-format" ) == 0) {
          DFBConfigLayer *conf = dfb_config->config_layer;

          if (value) {
               DFBSurfacePixelFormat format;

               format = dfb_pixelformat_parse( value );
               if (format == DSPF_UNKNOWN) {
                    D_ERROR( "DirectFB/Config: '%s': Could not parse format!\n", name );
                    return DFB_INVARG;
               }

               if (conf == &dfb_config->layers[0])
                    dfb_config->mode.format = format;

               conf->config.pixelformat  = format;
               conf->config.flags       |= DLCONF_PIXELFORMAT;
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No format specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "buffer-mode" ) == 0 || strcmp( name, "layer-buffer-mode" ) == 0) {
          DFBConfigLayer *conf = dfb_config->config_layer;

          if (value) {
               if (strcmp( value, "auto" ) == 0) {
                    conf->config.flags      &= ~DLCONF_BUFFERMODE;
               }
               else if (strcmp( value, "triple" ) == 0) {
                    conf->config.buffermode  = DLBM_TRIPLE;
                    conf->config.flags      |= DLCONF_BUFFERMODE;
               }
               else if (strcmp( value, "backvideo" ) == 0) {
                    conf->config.buffermode  = DLBM_BACKVIDEO;
                    conf->config.flags      |= DLCONF_BUFFERMODE;
               }
               else if (strcmp( value, "backsystem" ) == 0) {
                    conf->config.buffermode  = DLBM_BACKSYSTEM;
                    conf->config.flags      |= DLCONF_BUFFERMODE;
               }
               else if (strcmp( value, "frontonly" ) == 0) {
                    conf->config.buffermode  = DLBM_FRONTONLY;
                    conf->config.flags      |= DLCONF_BUFFERMODE;
               }
               else if (strcmp( value, "windows" ) == 0) {
                    conf->config.buffermode  = DLBM_WINDOWS;
                    conf->config.flags      |= DLCONF_BUFFERMODE;
               }
               else {
                    D_ERROR( "DirectFB/Config: '%s': Unknown buffer mode '%s'!\n", name, value );
                    return DFB_INVARG;
               }
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No buffer mode specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "src-key" ) == 0 || strcmp( name, "layer-src-key" ) == 0) {
          DFBConfigLayer *conf = dfb_config->config_layer;

          if (value) {
               char *error;
               u32   argb;

               argb = strtoul( value, &error, 16 );

               if (*error) {
                    D_ERROR( "DirectFB/Config: '%s': Error in color '%s'!\n", name, error );
                    return DFB_INVARG;
               }

               conf->src_key.b = argb & 0xff;
               argb >>= 8;
               conf->src_key.g = argb & 0xff;
               argb >>= 8;
               conf->src_key.r = argb & 0xff;
               argb >>= 8;
               conf->src_key.a = argb & 0xff;

               conf->config.options |= DLOP_SRC_COLORKEY;
               conf->config.flags   |= DLCONF_OPTIONS;
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No color specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "src-key-index" ) == 0 || strcmp( name, "layer-src-key-index" ) == 0) {
          DFBConfigLayer *conf = dfb_config->config_layer;

          if (value) {
               char *error;
               u32   index;

               index = strtoul( value, &error, 10 );

               if (*error) {
                    D_ERROR( "DirectFB/Config: '%s': Error in index '%s'!\n", name, error );
                    return DFB_INVARG;
               }

               conf->src_key_index = index;

               conf->config.options |= DLOP_SRC_COLORKEY;
               conf->config.flags   |= DLCONF_OPTIONS;
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No index specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp( name, "bg-none" ) == 0 || strcmp( name, "layer-bg-none" ) == 0) {
          DFBConfigLayer *conf = dfb_config->config_layer;

          conf->background.mode = DLBM_DONTCARE;
     } else
     if (strcmp( name, "bg-image" ) == 0 || strcmp( name, "bg-tile" ) == 0 ||
         strcmp( name, "layer-bg-image" ) == 0 || strcmp( name, "layer-bg-tile" ) == 0) {
          DFBConfigLayer *conf = dfb_config->config_layer;

          if (value) {
               if (conf->background.filename)
                    D_FREE( conf->background.filename );

               conf->background.filename = D_STRDUP( value );
               conf->background.mode     = strstr( name, "bg-image" ) ? DLBM_IMAGE : DLBM_TILE;
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No file name specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp( name, "bg-color" ) == 0 || strcmp( name, "layer-bg-color" ) == 0) {
          DFBConfigLayer *conf = dfb_config->config_layer;

          if (value) {
               char *error;
               u32   argb;

               argb = strtoul( value, &error, 16 );

               if (*error) {
                    D_ERROR( "DirectFB/Config: '%s': Error in color '%s'!\n", name, error );
                    return DFB_INVARG;
               }

               conf->background.color.b     = argb & 0xff;
               argb >>= 8;
               conf->background.color.g     = argb & 0xff;
               argb >>= 8;
               conf->background.color.r     = argb & 0xff;
               argb >>= 8;
               conf->background.color.a     = argb & 0xff;
               conf->background.color_index = -1;
               conf->background.mode        = DLBM_COLOR;
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No color specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp( name, "bg-color-index" ) == 0 || strcmp( name, "layer-bg-color-index" ) == 0) {
          DFBConfigLayer *conf = dfb_config->config_layer;

          if (value) {
               char *error;
               u32   index;

               index = strtoul( value, &error, 10 );

               if (*error) {
                    D_ERROR( "DirectFB/Config: '%s': Error in index '%s'!\n", name, error );
                    return DFB_INVARG;
               }

               conf->background.color_index = index;
               conf->background.mode        = DLBM_COLOR;
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No index specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp( name, "stacking" ) == 0 || strcmp( name, "layer-stacking" ) == 0) {
          DFBConfigLayer *conf = dfb_config->config_layer;

          if (value) {
               char *stackings = D_STRDUP( value );
               char *p = NULL, *r, *s = stackings;

               conf->stacking = 0;

               while ((r = direct_strtok_r( s, ",", &p ))) {
                    direct_trim( &r );

                    if (!strcmp( r, "lower" ))
                         conf->stacking |= (1 << DWSC_LOWER);
                    else if (!strcmp( r, "middle" ))
                         conf->stacking |= (1 << DWSC_MIDDLE);
                    else if (!strcmp( r, "upper" ))
                         conf->stacking |= (1 << DWSC_UPPER);
                    else {
                         D_ERROR( "DirectFB/Config: '%s': Unknown stacking class '%s'!\n", name, r );
                         D_FREE( stackings );
                         return DFB_INVARG;
                    }

                    s = NULL;
               }

               D_FREE( stackings );
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No stacking classes specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strncmp( name, "palette-", 8 ) == 0 || strncmp (name, "layer-palette-", 14 ) == 0) {
          char           *error;
          int             index;
          DFBConfigLayer *conf = dfb_config->config_layer;

          index = (name[0] == 'p') ? strtoul( name + 8, &error, 10 ) : strtoul( name + 14, &error, 10 );

          if (*error) {
               D_ERROR( "DirectFB/Config: '%s': Error in index '%s'!\n", name, error );
               return DFB_INVARG;
          }

          if (index < 0 || index > 255) {
               D_ERROR( "DirectFB/Config: '%s': Index %d out of bounds!\n", name, index );
               return DFB_INVARG;
          }

          if (value) {
               char *error;
               u32   argb;

               argb = strtoul( value, &error, 16 );

               if (*error) {
                    D_ERROR( "DirectFB/Config: '%s': Error in color '%s'!\n", name, error );
                    return DFB_INVARG;
               }

               if (!conf->palette) {
                    conf->palette = D_CALLOC( 256, sizeof(DFBColor) );
                    if (!conf->palette)
                         return D_OOM();
               }

               conf->palette[index].a = (argb & 0xff000000) >> 24;
               conf->palette[index].r = (argb & 0xff0000) >> 16;
               conf->palette[index].g = (argb & 0xff00) >> 8;
               conf->palette[index].b = (argb & 0xff);
               conf->palette_set      = true;
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No color specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp (name, "rotate" ) == 0 || strcmp( name, "layer-rotate" ) == 0) {
          DFBConfigLayer *conf = dfb_config->config_layer;

          if (value) {
               int rotate;

               if (sscanf( value, "%d", &rotate ) < 1) {
                    D_ERROR( "DirectFB/Config: '%s': Could not parse value!\n", name );
                    return DFB_INVARG;
               }

               if (rotate != 0 && rotate != 90 && rotate != 180 && rotate != 270) {
                    D_ERROR( "DirectFB/Config: '%s': Only 0, 90, 180 or 270 supported!\n", name );
                    return DFB_INVARG;
               }

               conf->rotate = rotate;
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp( name, "graphics-state-call-limit" ) == 0) {
          if (value) {
               unsigned int limit;

               if (sscanf( value, "%u", &limit ) < 1) {
                    D_ERROR( "DirectFB/Config: '%s': Could not parse value!\n", name );
                    return DFB_INVARG;
               }

               dfb_config->graphics_state_call_limit = limit;
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp( name, "hardware" ) == 0) {
          dfb_config->software_only = false;
     } else
     if (strcmp( name, "no-hardware" ) == 0) {
          dfb_config->software_only = true;
     } else
     if (strcmp( name, "software" ) == 0) {
          dfb_config->hardware_only = false;
     } else
     if (strcmp( name, "no-software" ) == 0) {
          dfb_config->hardware_only = true;
     } else
     if (strcmp( name, "software-warn" ) == 0) {
          dfb_config->software_warn = true;
     } else
     if (strcmp( name, "no-software-warn" ) == 0) {
          dfb_config->software_warn = false;
     } else
     if (strcmp( name, "software-trace" ) == 0) {
          dfb_config->software_trace = true;
     } else
     if (strcmp( name, "no-software-trace" ) == 0) {
          dfb_config->software_trace = false;
     } else
     if (strcmp( name, "gfxcard-stats" ) == 0) {
          if (value) {
               unsigned int interval;

               if (sscanf( value, "%u", &interval ) < 1) {
                    D_ERROR( "DirectFB/Config: '%s': Could not parse value!\n", name );
                    return DFB_INVARG;
               }

               dfb_config->gfxcard_stats = interval;
          }
          else
               dfb_config->gfxcard_stats = 1000;
     } else
     if (strcmp( name, "no-gfxcard-stats" ) == 0) {
          dfb_config->gfxcard_stats = 0;
     } else
     if (strcmp( name, "videoram-limit" ) == 0) {
          if (value) {
               unsigned int limit;

               if (sscanf( value, "%u", &limit ) < 1) {
                    D_ERROR( "DirectFB/Config: '%s': Could not parse value!\n", name );
                    return DFB_INVARG;
               }

               dfb_config->videoram_limit = limit << 10;
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp( name, "gfx-emit-early" ) == 0) {
          dfb_config->gfx_emit_early = true;
     } else
     if (strcmp( name, "no-gfx-emit-early" ) == 0) {
          dfb_config->gfx_emit_early = false;
     } else
     if (strcmp( name, "startstop" ) == 0) {
          dfb_config->startstop = true;
     } else
     if (strcmp( name, "no-startstop" ) == 0) {
          dfb_config->startstop = false;
     } else
     if (strcmp( name, "smooth-upscale" ) == 0) {
          dfb_config->render_options |= DSRO_SMOOTH_UPSCALE;
     } else
     if (strcmp( name, "no-smooth-upscale" ) == 0) {
          dfb_config->render_options &= ~DSRO_SMOOTH_UPSCALE;
     } else
     if (strcmp( name, "smooth-downscale" ) == 0) {
          dfb_config->render_options |= DSRO_SMOOTH_DOWNSCALE;
     } else
     if (strcmp( name, "no-smooth-downscale" ) == 0) {
          dfb_config->render_options &= ~DSRO_SMOOTH_DOWNSCALE;
     } else
     if (strcmp( name, "keep-accumulators" ) == 0) {
          if (value) {
               int limit;

               if (sscanf( value, "%d", &limit ) < 1) {
                    D_ERROR( "DirectFB/Config: '%s': Could not parse value!\n", name );
                    return DFB_INVARG;
               }

               dfb_config->keep_accumulators = limit;
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp( name, "mmx" ) == 0) {
          dfb_config->mmx = true;
     } else
     if (strcmp( name, "no-mmx" ) == 0) {
          dfb_config->mmx = false;
     } else
     if (strcmp( name, "neon" ) == 0) {
          dfb_config->neon = true;
     } else
     if (strcmp( name, "no-neon" ) == 0) {
          dfb_config->neon = false;
     } else
     if (strcmp( name, "warn" ) == 0 || strcmp( name, "no-warn" ) == 0) {
          DFBConfigWarnFlags flags = DCWF_ALL;

          if (value) {
               char *opt = strchr( value, ':' );

               if (opt)
                    opt++;

               if (!strncmp( value, "create-surface", 14 )) {
                    flags = DCWF_CREATE_SURFACE;

                    if (opt)
                         sscanf( opt, "%dx%d",
                                 &dfb_config->warn.create_surface.min_size.w,
                                 &dfb_config->warn.create_surface.min_size.h );
               }
               else if (!strncmp( value, "create-window", 13 )) {
                    flags = DCWF_CREATE_WINDOW;
               }
               else if (!strncmp( value, "allocate-buffer", 15 )) {
                    flags = DCWF_ALLOCATE_BUFFER;

                    if (opt)
                         sscanf( opt, "%dx%d",
                                 &dfb_config->warn.allocate_buffer.min_size.w,
                                 &dfb_config->warn.allocate_buffer.min_size.h );
               }
               else {
                    D_ERROR( "DirectFB/Config: '%s': Unknown warning type '%s'!\n", name, value );
                    return DFB_INVARG;
               }
          }

          if (name[0] == 'w')
               dfb_config->warn.flags |= flags;
          else
               dfb_config->warn.flags &= ~flags;
     } else
     if (strcmp( name, "surface-clear" ) == 0) {
          dfb_config->surface_clear = true;
     } else
     if (strcmp( name, "no-surface-clear" ) == 0) {
          dfb_config->surface_clear = false;
     } else
     if (strcmp( name, "thrifty-surface-buffers" ) == 0) {
          dfb_config->thrifty_surface_buffers = true;
     } else
     if (strcmp( name, "no-thrifty-surface-buffers" ) == 0) {
          dfb_config->thrifty_surface_buffers = false;
     } else
     if (!strcmp( name, "surface-shmpool-size" )) {
          if (value) {
               int size_kb;

               if (sscanf( value, "%d", &size_kb ) < 1) {
                    D_ERROR( "DirectFB/Config: '%s': Could not parse value!\n", name );
                    return DFB_INVARG;
               }

               dfb_config->surface_shmpool_size = size_kb * 1024;
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp( name, "system-surface-base-alignment" ) == 0) {
          if (value) {
               unsigned int base_align;

               if (sscanf( value, "%u", &base_align ) < 1) {
                    D_ERROR( "DirectFB/Config: '%s': Could not parse value!\n", name );
                    return DFB_INVARG;
               }

               if (base_align && (base_align < 4 || (base_align & (base_align - 1)) != 0)) {
                   D_ERROR( "DirectFB/Config: '%s': Value must be a positive power of two that is four or greater!\n",
                            name );
                   return DFB_INVARG;
               }

               dfb_config->system_surface_align_base = base_align;
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp( name, "system-surface-pitch-alignment" ) == 0) {
          if (value) {
               unsigned int pitch_align;

               if (sscanf( value, "%u", &pitch_align ) < 1) {
                    D_ERROR( "DirectFB/Config: '%s': Could not parse value!\n", name );
                    return DFB_INVARG;
               }

               if (pitch_align && (pitch_align == 1 || (pitch_align & (pitch_align - 1)) != 0)) {
                   D_ERROR( "DirectFB/Config: '%s': Value must be a positive power of two!\n", name );
                   return DFB_INVARG;
               }

               dfb_config->system_surface_align_pitch = pitch_align;
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp( name, "max-frame-advance" ) == 0) {
          if (value) {
               long long advance;

               if (sscanf( value, "%lld", &advance ) < 1) {
                    D_ERROR("DirectFB/Config '%s': Could not parse value!\n", name);
                    return DFB_INVARG;
               }

               dfb_config->max_frame_advance = advance;
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp( name, "force-frametime" ) == 0) {
          dfb_config->force_frametime = true;
     } else
     if (strcmp( name, "no-force-frametime" ) == 0) {
          dfb_config->force_frametime = false;
     } else
     if (strcmp( name, "subsurface-caching" ) == 0) {
          dfb_config->subsurface_caching = true;
     } else
     if (strcmp( name, "no-subsurface-caching" ) == 0) {
          dfb_config->subsurface_caching = false;
     } else
     if (strcmp( name, "window-surface-policy" ) == 0) {
          if (value) {
               if (strcmp( value, "auto" ) == 0) {
                    dfb_config->window_policy = -1;
               }
               else if (strcmp( value, "videohigh" ) == 0) {
                    dfb_config->window_policy = DWSP_VIDEOHIGH;
               }
               else if (strcmp( value, "videolow" ) == 0) {
                    dfb_config->window_policy = DWSP_VIDEOLOW;
               }
               else if (strcmp( value, "systemonly" ) == 0) {
                    dfb_config->window_policy = DWSP_SYSTEMONLY;
               }
               else if (strcmp( value, "videoonly" ) == 0) {
                    dfb_config->window_policy = DWSP_VIDEOONLY;
               }
               else {
                    D_ERROR( "DirectFB/Config: '%s': Unknown window surface policy '%s'!\n", name, value );
                    return DFB_INVARG;
               }
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No window surface policy specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp( name, "single-window" ) == 0) {
          dfb_config->single_window = true;
     } else
     if (strcmp( name, "no-single-window" ) == 0) {
          dfb_config->single_window = false;
     } else
     if (strcmp( name, "translucent-windows" ) == 0) {
          dfb_config->translucent_windows = true;
     } else
     if (strcmp( name, "no-translucent-windows" ) == 0) {
          dfb_config->translucent_windows = false;
     } else
     if (strcmp( name, "force-windowed" ) == 0) {
          dfb_config->force_windowed = true;
     } else
     if (strcmp( name, "no-force-windowed" ) == 0) {
          dfb_config->force_windowed = false;
     } else
     if (strcmp( name, "scaled" ) == 0) {
          if (value) {
               int width, height;

               if (sscanf( value, "%dx%d", &width, &height ) < 2) {
                    D_ERROR( "DirectFB/Config: '%s': Could not parse size!\n", name );
                    return DFB_INVARG;
               }

               dfb_config->scaled.width  = width;
               dfb_config->scaled.height = height;
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No size specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp( name, "autoflip-window" ) == 0) {
          dfb_config->autoflip_window = true;
     } else
     if (strcmp( name, "no-autoflip-window" ) == 0) {
          dfb_config->autoflip_window = false;
     } else
     if (strcmp( name, "cursor" ) == 0) {
          dfb_config->no_cursor = false;
     } else
     if (strcmp( name, "no-cursor" ) == 0) {
          dfb_config->no_cursor = true;
     } else
     if (strcmp( name, "cursor-videoonly" ) == 0) {
          dfb_config->cursor_videoonly = true;
     } else
     if (strcmp( name, "no-cursor-videoonly" ) == 0) {
          dfb_config->cursor_videoonly = false;
     } else
     if (strcmp( name, "cursor-resource-id" ) == 0) {
          if (value) {
               unsigned long resource_id;

               if (sscanf( value, "%lu", &resource_id ) < 1) {
                    D_ERROR("DirectFB/Config '%s': Could not parse id!\n", name);
                    return DFB_INVARG;
               }

               dfb_config->cursor_resource_id = resource_id;
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No id specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp( name, "cursor-automation" ) == 0) {
          dfb_config->cursor_automation = true;
     } else
     if (strcmp( name, "no-cursor-automation" ) == 0) {
          dfb_config->cursor_automation = false;
     } else
     if (strcmp( name, "discard-repeat-events" ) == 0) {
          dfb_config->discard_repeat_events = true;
     } else
     if (strcmp( name, "no-discard-repeat-events" ) == 0) {
          dfb_config->discard_repeat_events = false;
     } else
     if (strcmp( name, "lefty" ) == 0) {
          dfb_config->lefty = true;
     } else
     if (strcmp( name, "no-lefty" ) == 0) {
          dfb_config->lefty = false;
     } else
     if (strcmp( name, "capslock-meta" ) == 0) {
          dfb_config->capslock_meta = true;
     } else
     if (strcmp( name, "no-capslock-meta" ) == 0) {
          dfb_config->capslock_meta = false;
     } else
     if (strcmp( name, "screenshot-dir" ) == 0) {
          if (value) {
               if (dfb_config->screenshot_dir)
                    D_FREE( dfb_config->screenshot_dir );

               dfb_config->screenshot_dir = D_STRDUP( value );
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No directory name specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp( name, "font-format" ) == 0) {
          if (value) {
               DFBSurfacePixelFormat format;

               format = dfb_pixelformat_parse( value );
               if (format == DSPF_UNKNOWN) {
                    D_ERROR( "DirectFB/Config: '%s': Could not parse format!\n", name );
                    return DFB_INVARG;
               }

               dfb_config->font_format = format;
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No format specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp( name, "font-premult" ) == 0) {
          dfb_config->font_premult = true;
     } else
     if (strcmp( name, "no-font-premult" ) == 0) {
          dfb_config->font_premult = false;
     } else
     if (strcmp( name, "font-resource-id" ) == 0) {
          if (value) {
               unsigned long resource_id;

               if (sscanf( value, "%lu", &resource_id ) < 1) {
                    D_ERROR("DirectFB/Config '%s': Could not parse id!\n", name);
                    return DFB_INVARG;
               }

               dfb_config->font_resource_id = resource_id;
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No id specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp( name, "max-font-rows" ) == 0) {
          if (value) {
               int rows;

               if (sscanf( value, "%d", &rows ) < 1) {
                    D_ERROR( "DirectFB/Config: '%s': Could not parse value!\n", name );
                    return DFB_INVARG;
               }

               dfb_config->max_font_rows = rows;
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     } else
     if (strcmp( name, "max-font-row-width" ) == 0) {
          if (value) {
               int row_width;

               if (sscanf( value, "%d", &row_width ) < 1) {
                    D_ERROR( "DirectFB/Config: '%s': Could not parse value!\n", name );
                    return DFB_INVARG;
               }


               dfb_config->max_font_row_width = row_width;
          }
          else {
               D_ERROR( "DirectFB/Config: '%s': No value specified!\n", name );
               return DFB_INVARG;
          }
     }
     else {
          dfboption = false;
          if (fusion_config_set( name, value ) && direct_config_set( name, value ))
               return DFB_INVARG;
     }

     if (dfboption)
          D_DEBUG_AT( DirectFB_Config, "Set %s '%s'\n", name, value ?: "" );

     return DFB_OK;
}

DFBResult
dfb_config_init( int    *argc,
                 char *(*argv[]) )
{
     DFBResult  ret;
     int        i;
     char      *home = direct_getenv( "HOME" );
     char      *prog = NULL;
     char      *session;
     char      *dfbargs;
     char       cmdbuf[1024];

     if (dfb_config) {
          /* Check again for session environment setting. */
          session = direct_getenv( "DIRECTFB_SESSION" );
          if (session)
               dfb_config_set( "session", session );

          return DFB_OK;
     }

     config_allocate();

     /* Read system settings. */
     ret = config_read( SYSCONFDIR"/directfbrc" );
     if (ret && ret != DFB_IO)
          return ret;

     /* Read user settings. */
     if (home) {
          int  len = strlen( home ) + strlen( "/.directfbrc" ) + 1;
          char buf[len];

          snprintf( buf, len, "%s/.directfbrc", home );

          ret = config_read( buf );
          if (ret && ret != DFB_IO)
               return ret;
     }

     /* Get application name. */
     if (argc && *argc && argv && *argv) {
          prog = strrchr( (*argv)[0], '/' );

          if (prog)
               prog++;
          else
               prog = (*argv)[0];
     }
     else {
          /* If we didn't receive argc/argv, we try the procfs interface. */
          DirectFile f;
          size_t     len;

          ret = direct_file_open( &f, "/proc/self/cmdline", O_RDONLY, 0 );
          if (!ret) {
               ret = direct_file_read( &f, cmdbuf, sizeof(cmdbuf) - 1, &len );
               if (!ret && len) {
                    cmdbuf[len] = 0;
                    prog = strrchr( cmdbuf, '/' );
                    if (prog)
                         prog++;
                    else
                         prog = cmdbuf;
               }

               direct_file_close( &f );
          }
     }

     /* Read global application settings. */
     if (prog && prog[0]) {
          int  len = strlen( SYSCONFDIR"/directfbrc." ) + strlen( prog ) + 1;
          char buf[len];

          snprintf( buf, len, SYSCONFDIR"/directfbrc.%s", prog );

          ret = config_read( buf );
          if (ret && ret != DFB_IO)
               return ret;
     }

     /* Read user application settings. */
     if (home && prog && prog[0]) {
          int  len = strlen( home ) + strlen( "/.directfbrc." ) + strlen( prog ) + 1;
          char buf[len];

          snprintf( buf, len, "%s/.directfbrc.%s", home, prog );

          ret = config_read( buf );
          if (ret && ret != DFB_IO)
               return ret;
     }

     /* Read settings from environment variable. */
     dfbargs = direct_getenv( "DFBARGS" );
     if (dfbargs) {
          ret = parse_args( dfbargs );
          if (ret)
               return ret;
     }

     /* Active session is used if present, only command line can override. */
     session = direct_getenv( "DIRECTFB_SESSION" );
     if (session)
          dfb_config_set( "session", session );

     /* Read settings from command line. */
     if (argc && argv) {
          for (i = 1; i < *argc; i++) {
               if (strcmp( (*argv)[i], "--dfb-help" ) == 0) {
                    print_config_usage();
                    exit( 1 );
               }

               if (strncmp( (*argv)[i], "--dfb:", 6 ) == 0) {
                    ret = parse_args( (*argv)[i] + 6 );
                    if (ret)
                         return ret;

                    (*argv)[i] = NULL;
               }
          }

          for (i = 1; i < *argc; i++) {
               int k;

               for (k = i; k < *argc; k++)
                    if ((*argv)[k] != NULL)
                         break;

               if (k > i) {
                    int j;

                    k -= i;

                    for (j = i + k; j < *argc; j++)
                         (*argv)[j-k] = (*argv)[j];

                    *argc -= k;
               }
          }
     }
     else if (prog) {
          /* We have prog, so we try again the proc filesystem. */
          DirectFile f;
          size_t     len = strlen( cmdbuf );

          ret = direct_file_open( &f, "/proc/self/cmdline", O_RDONLY, 0 );
          if (!ret) {
               direct_file_read( &f, cmdbuf, len, &len );

               while (config_read_cmdline( cmdbuf, 1024, &f )) {
                    if (strcmp( cmdbuf, "--dfb-help" ) == 0) {
                         print_config_usage();
                         exit( 1 );
                    }

                    if (strncmp( cmdbuf, "--dfb:", 6 ) == 0) {
                         ret = parse_args( cmdbuf + 6 );
                         if (ret) {
                              direct_file_close( &f );
                              return ret;
                         }
                    }
               }

               direct_file_close( &f );
          }
     }

     return DFB_OK;
}

void
dfb_config_deinit()
{
     if (!dfb_config) {
          return;
     }

     if (dfb_config->screenshot_dir)
          D_FREE( dfb_config->screenshot_dir );

     DFBConfigLayer *conf = dfb_config->config_layer;
     if (conf->palette)
          D_FREE( conf->palette );

     if (dfb_config->wm)
          D_FREE( dfb_config->wm );

     if (dfb_config->system)
          D_FREE( dfb_config->system );

     D_FREE( dfb_config );
     dfb_config = NULL;
}

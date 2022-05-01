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

#include <core/gfxcard.h>
#include <core/surface_buffer.h>
#include <direct/memcpy.h>
#include <directfb_util.h>
#include <fusion/shmalloc.h>

#include "fbdev_mode.h"

D_DEBUG_DOMAIN( FBDev_Mode, "FBDev/Mode", "FBDev Mode" );

/**********************************************************************************************************************/

static DFBResult
mode_to_var( const VideoMode           *mode,
             DFBSurfacePixelFormat      pixelformat,
             unsigned int               vxres,
             unsigned int               vyres,
             unsigned short             xpanstep,
             unsigned short             ypanstep,
             unsigned short             ywrapstep,
             unsigned int               xoffset,
             unsigned int               yoffset,
             DFBDisplayLayerBufferMode  buffermode,
             struct fb_var_screeninfo  *ret_var )
{
     struct fb_var_screeninfo var;

     D_DEBUG_AT( FBDev_Mode, "%s( %p )\n", __FUNCTION__, mode );

     D_ASSERT( mode != NULL );
     D_ASSERT( ret_var != NULL );

     D_DEBUG_AT( FBDev_Mode, "  -> resolution   %dx%d\n", mode->xres, mode->yres );
     D_DEBUG_AT( FBDev_Mode, "  -> virtual      %ux%u\n", vxres, vyres );
     D_DEBUG_AT( FBDev_Mode, "  -> pixelformat  %s\n", dfb_pixelformat_name( pixelformat ) );
     D_DEBUG_AT( FBDev_Mode, "  -> buffermode   %s\n",
                 buffermode == DLBM_FRONTONLY  ? "FRONTONLY"  :
                 buffermode == DLBM_BACKVIDEO  ? "BACKVIDEO"  :
                 buffermode == DLBM_BACKSYSTEM ? "BACKSYSTEM" :
                 buffermode == DLBM_TRIPLE     ? "TRIPLE"     : "invalid!" );

     /* Start from current information. */
     var = *ret_var;

     /* Set values now. */
     var.activate = FB_ACTIVATE_NOW;

     /* Set timings. */
     var.pixclock     = mode->pixclock;
     var.left_margin  = mode->left_margin;
     var.right_margin = mode->right_margin;
     var.upper_margin = mode->upper_margin;
     var.lower_margin = mode->lower_margin;
     var.hsync_len    = mode->hsync_len;
     var.vsync_len    = mode->vsync_len;

     /* Set resolution. */
     var.xres         = mode->xres;
     var.yres         = mode->yres;
     var.xres_virtual = vxres;
     var.yres_virtual = vyres;

     if (xpanstep)
          var.xoffset = xoffset - (xoffset % xpanstep);
     else
          var.xoffset = 0;

     if (ywrapstep)
          var.yoffset = yoffset - (yoffset % ywrapstep);
     else if (ypanstep)
          var.yoffset = yoffset - (yoffset % ypanstep);
     else
          var.yoffset = 0;

     /* Set buffer mode. */
     switch (buffermode) {
          case DLBM_TRIPLE:
               if (ypanstep == 0 && ywrapstep == 0)
                    return DFB_UNSUPPORTED;

               var.yres_virtual *= 3;
               break;

          case DLBM_BACKVIDEO:
               if (ypanstep == 0 && ywrapstep == 0)
                    return DFB_UNSUPPORTED;

               var.yres_virtual *= 2;
               break;

          case DLBM_BACKSYSTEM:
          case DLBM_FRONTONLY:
               break;

          default:
               return DFB_UNSUPPORTED;
     }

     /* Set pixel format. */
     var.bits_per_pixel = DFB_BITS_PER_PIXEL( pixelformat );
     var.transp.length  = var.transp.offset = 0;

     switch (pixelformat) {
          case DSPF_ARGB1555:
               var.transp.length = 1;
               var.red.length    = 5;
               var.green.length  = 5;
               var.blue.length   = 5;
               var.transp.offset = 15;
               var.red.offset    = 10;
               var.green.offset  = 5;
               var.blue.offset   = 0;
               break;

          case DSPF_RGBA5551:
               var.transp.length = 1;
               var.red.length    = 5;
               var.green.length  = 5;
               var.blue.length   = 5;
               var.red.offset    = 11;
               var.green.offset  = 6;
               var.blue.offset   = 1;
               var.transp.offset = 0;
               break;

          case DSPF_RGB555:
               var.red.length    = 5;
               var.green.length  = 5;
               var.blue.length   = 5;
               var.red.offset    = 10;
               var.green.offset  = 5;
               var.blue.offset   = 0;
               break;

          case DSPF_BGR555:
               var.red.length    = 5;
               var.green.length  = 5;
               var.blue.length   = 5;
               var.red.offset    = 0;
               var.green.offset  = 5;
               var.blue.offset   = 10;
               break;

          case DSPF_ARGB4444:
               var.transp.length = 4;
               var.red.length    = 4;
               var.green.length  = 4;
               var.blue.length   = 4;
               var.transp.offset = 12;
               var.red.offset    = 8;
               var.green.offset  = 4;
               var.blue.offset   = 0;
               break;

          case DSPF_RGBA4444:
               var.transp.length = 4;
               var.red.length    = 4;
               var.green.length  = 4;
               var.blue.length   = 4;
               var.transp.offset = 0;
               var.red.offset    = 12;
               var.green.offset  = 8;
               var.blue.offset   = 4;
               break;

         case DSPF_RGB444:
               var.red.length    = 4;
               var.green.length  = 4;
               var.blue.length   = 4;
               var.red.offset    = 8;
               var.green.offset  = 4;
               var.blue.offset   = 0;
               break;

         case DSPF_RGB32:
               var.red.length    = 8;
               var.green.length  = 8;
               var.blue.length   = 8;
               var.red.offset    = 16;
               var.green.offset  = 8;
               var.blue.offset   = 0;
               break;

          case DSPF_ARGB8565:
               var.transp.length = 8;
               var.transp.offset = 16;
               /* fall through */

          case DSPF_RGB16:
               var.red.length    = 5;
               var.green.length  = 6;
               var.blue.length   = 5;
               var.red.offset    = 11;
               var.green.offset  = 5;
               var.blue.offset   = 0;
               break;

          case DSPF_ARGB:
          case DSPF_AiRGB:
               var.transp.length = 8;
               var.red.length    = 8;
               var.green.length  = 8;
               var.blue.length   = 8;
               var.transp.offset = 24;
               var.red.offset    = 16;
               var.green.offset  = 8;
               var.blue.offset   = 0;
               break;

          case DSPF_ABGR:
               var.transp.length = 8;
               var.red.length    = 8;
               var.green.length  = 8;
               var.blue.length   = 8;
               var.transp.offset = 24;
               var.red.offset    = 0;
               var.green.offset  = 8;
               var.blue.offset   = 16;
               break;

          case DSPF_LUT8:
          case DSPF_RGB24:
          case DSPF_RGB332:
               break;

          case DSPF_ARGB1666:
               var.transp.length = 1;
               var.red.length    = 6;
               var.green.length  = 6;
               var.blue.length   = 6;
               var.transp.offset = 18;
               var.red.offset    = 12;
               var.green.offset  = 6;
               var.blue.offset   = 0;
               break;

          case DSPF_ARGB6666:
               var.transp.length = 6;
               var.red.length    = 6;
               var.green.length  = 6;
               var.blue.length   = 6;
               var.transp.offset = 18;
               var.red.offset    = 12;
               var.green.offset  = 6;
               var.blue.offset   = 0;
               break;

          case DSPF_RGB18:
               var.red.length    = 6;
               var.green.length  = 6;
               var.blue.length   = 6;
               var.red.offset    = 12;
               var.green.offset  = 6;
               var.blue.offset   = 0;
               break;

          case DSPF_RGBAF88871:
               var.transp.length = 7;
               var.red.length    = 8;
               var.green.length  = 8;
               var.blue.length   = 8;
               var.transp.offset = 1;
               var.red.offset    = 24;
               var.green.offset  = 16;
               var.blue.offset   = 8;
               break;

          default:
               return DFB_UNSUPPORTED;
     }

     /* Set sync options. */
     var.sync = 0;
     if (mode->hsync_high)
          var.sync |= FB_SYNC_HOR_HIGH_ACT;
     if (mode->vsync_high)
          var.sync |= FB_SYNC_VERT_HIGH_ACT;
     if (mode->csync_high)
          var.sync |= FB_SYNC_COMP_HIGH_ACT;
     if (mode->sync_on_green)
          var.sync |= FB_SYNC_ON_GREEN;
     if (mode->external_sync)
          var.sync |= FB_SYNC_EXT;
     if (mode->broadcast)
          var.sync |= FB_SYNC_BROADCAST;

     /* Set interlace/linedouble. */
     var.vmode = 0;
     if (mode->laced)
          var.vmode |= FB_VMODE_INTERLACED;
     if (mode->doubled)
          var.vmode |= FB_VMODE_DOUBLE;

     *ret_var = var;

     return DFB_OK;
}

static DFBResult
test_mode_simple( FBDevData       *fbdev,
                  const VideoMode *mode )
{
     DFBResult                 ret;
     FBDevDataShared          *shared;
     struct fb_var_screeninfo  var;

     D_DEBUG_AT( FBDev_Mode, "%s( %p )\n", __FUNCTION__, mode );

     D_ASSERT( fbdev != NULL );
     D_ASSERT( fbdev->shared != NULL );
     D_ASSERT( mode != NULL );

     shared = fbdev->shared;

     var = shared->current_var;
     ret = mode_to_var( mode, dfb_pixelformat_for_depth(mode->bpp), mode->xres, mode->yres,
                        fbdev->fix->xpanstep, fbdev->fix->ypanstep, fbdev->fix->ywrapstep, 0, 0, DLBM_FRONTONLY,
                        &var );
     if (ret)
          return ret;

     /* Enable test mode. */
     var.activate = FB_ACTIVATE_TEST;

     if (fbdev_ioctl( fbdev, FBIOPUT_VSCREENINFO, &var, sizeof(var) ) < 0) {
          ret = errno2result( errno );
          D_DEBUG_AT( FBDev_Mode, "  -> FBIOPUT_VSCREENINFO failed!\n" );
          return ret;
     }

     return DFB_OK;
}

static void
read_modes( FBDevData *fbdev )
{
     FBDevDataShared *shared;
     FILE            *fp;
     char             line[80], label[32], value[16];
     int              geometry, timings, dummy;
     VideoMode        temp_mode;
     VideoMode       *prev;

     D_DEBUG_AT( FBDev_Mode, "%s()\n", __FUNCTION__ );

     D_ASSERT( fbdev != NULL );
     D_ASSERT( fbdev->shared != NULL );

     shared = fbdev->shared;

     prev = shared->modes;

     if (!(fp = fopen( shared->modes_file, "r" )))
          return;

     while (fgets( line, 79, fp )) {
          if (sscanf( line, "mode \"%31[^\"]\"", label ) == 1) {
               memset( &temp_mode, 0, sizeof(VideoMode) );

               geometry = 0;
               timings = 0;

               while (fgets( line, 79, fp ) && !(strstr( line, "endmode" ))) {
                    if (5 == sscanf( line, " geometry %d %d %d %d %d",
                                     &temp_mode.xres, &temp_mode.yres, &dummy, &dummy, &temp_mode.bpp )) {
                         geometry = 1;
                    }
                    else if (7 == sscanf( line, " timings %d %d %d %d %d %d %d",
                                          &temp_mode.pixclock,
                                          &temp_mode.left_margin, &temp_mode.right_margin,
                                          &temp_mode.upper_margin, &temp_mode.lower_margin,
                                          &temp_mode.hsync_len, &temp_mode.vsync_len )) {
                         timings = 1;
                    }
                    else if (1 == sscanf( line, " hsync %15s", value ) && 0 == strcasecmp( value, "high" )) {
                         temp_mode.hsync_high = 1;
                    }
                    else if (1 == sscanf( line, " vsync %15s", value ) && 0 == strcasecmp( value, "high" )) {
                         temp_mode.vsync_high = 1;
                    }
                    else if (1 == sscanf( line, " csync %15s", value ) && 0 == strcasecmp( value, "high" )) {
                         temp_mode.csync_high = 1;
                    }
                    else if (1 == sscanf( line, " laced %15s", value ) && 0 == strcasecmp( value, "true" )) {
                         temp_mode.laced = 1;
                    }
                    else if (1 == sscanf( line, " double %15s", value) && 0 == strcasecmp( value, "true" )) {
                         temp_mode.doubled = 1;
                    }
                    else if (1 == sscanf( line, " gsync %15s", value ) && 0 == strcasecmp( value, "true" )) {
                         temp_mode.sync_on_green = 1;
                    }
                    else if (1 == sscanf( line, " extsync %15s", value ) && 0 == strcasecmp( value, "true" )) {
                         temp_mode.external_sync = 1;
                    }
                    else if (1 == sscanf( line, " bcast %15s", value ) && 0 == strcasecmp( value, "true" )) {
                         temp_mode.broadcast = 1;
                    }
               }

               if (geometry && timings && !test_mode_simple( fbdev, &temp_mode )) {
                    VideoMode *mode = SHCALLOC( shared->shmpool, 1, sizeof(VideoMode) );
                    if (!mode) {
                         D_OOSHM();
                         continue;
                    }

                    if (!prev)
                         shared->modes = mode;
                    else
                         prev->next = mode;

                    direct_memcpy( mode, &temp_mode, sizeof(VideoMode) );

                    prev = mode;

                    D_DEBUG_AT( FBDev_Mode, "  -> %16s %4dx%4d  %s%s\n", label, temp_mode.xres, temp_mode.yres,
                                temp_mode.laced ? "interlaced " : "", temp_mode.doubled ? "doublescan" : "" );
               }
          }
     }

     fclose( fp );
}

static unsigned short
calc_gamma( int n,
            int max )
{
     int res = 65535 * n / max;

     return CLAMP( res, 0, 65535 );
}

static void
set_rgb332_gamma_ramp( FBDevData *fbdev )
{
     FBDevDataShared *shared;
     struct fb_cmap  *cmap;
     int              i;
     int              red_val;
     int              green_val;
     int              blue_val;

     D_DEBUG_AT( FBDev_Mode, "%s()\n", __FUNCTION__ );

     D_ASSERT( fbdev != NULL );
     D_ASSERT( fbdev->shared != NULL );

     shared = fbdev->shared;

     cmap = &shared->current_cmap;

     i = 0;
     cmap->len = 256;

     for (red_val = 0; red_val < 8 ; red_val++) {
          for (green_val = 0; green_val < 8 ; green_val++) {
               for (blue_val = 0; blue_val < 4 ; blue_val++) {
                    cmap->red[i]    = calc_gamma( red_val, 7 );
                    cmap->green[i]  = calc_gamma( green_val, 7 );
                    cmap->blue[i]   = calc_gamma( blue_val, 3 );
                    cmap->transp[i] = (i ? 0x2000 : 0xffff);
                    i++;
               }
          }
     }

     fbdev_ioctl( fbdev, FBIOPUTCMAP, cmap, sizeof(*cmap) );
}

static void
set_gamma_ramp( FBDevData             *fbdev,
                DFBSurfacePixelFormat  format )
{
     FBDevDataShared *shared;
     struct fb_cmap  *cmap;
     int              i;
     int              red_size   = 0;
     int              green_size = 0;
     int              blue_size  = 0;
     int              red_max    = 0;
     int              green_max  = 0;
     int              blue_max   = 0;

     D_DEBUG_AT( FBDev_Mode, "%s()\n", __FUNCTION__ );

     D_ASSERT( fbdev != NULL );
     D_ASSERT( fbdev->shared != NULL );

     shared = fbdev->shared;

     cmap = &shared->current_cmap;

     switch (format) {
          case DSPF_ARGB1555:
          case DSPF_RGBA5551:
          case DSPF_RGB555:
          case DSPF_BGR555:
               red_size   = 32;
               green_size = 32;
               blue_size  = 32;
               break;
          case DSPF_ARGB4444:
          case DSPF_RGBA4444:
          case DSPF_RGB444:
               red_size   = 16;
               green_size = 16;
               blue_size  = 16;
               break;
          case DSPF_RGB16:
          case DSPF_ARGB8565:
               red_size   = 32;
               green_size = 64;
               blue_size  = 32;
               break;
          case DSPF_RGB24:
          case DSPF_RGB32:
          case DSPF_ARGB:
          case DSPF_ABGR:
          case DSPF_RGBAF88871:
               red_size   = 256;
               green_size = 256;
               blue_size  = 256;
               break;
          default:
               return;
     }

     /* The gamma ramp must be set differently if in DirectColor. */
     if (fbdev->fix->visual == FB_VISUAL_DIRECTCOLOR) {
          red_max   = 65536 / (256 / red_size);
          green_max = 65536 / (256 / green_size);
          blue_max  = 65536 / (256 / blue_size);
     }
     else {
          red_max   = red_size;
          green_max = green_size;
          blue_max  = blue_size;
     }

     /* Assume green to have most weight. */
     cmap->len = green_size;

     for (i = 0; i < red_size; i++)
          cmap->red[i] = calc_gamma( i, red_max );

     for (i = 0; i < green_size; i++)
          cmap->green[i] = calc_gamma( i, green_max );

     for (i = 0; i < blue_size; i++)
          cmap->blue[i] = calc_gamma( i, blue_max );

     /* Some drivers use the upper byte, some use the lower. */
     if (fbdev->fix->visual == FB_VISUAL_DIRECTCOLOR) {
          for (i = 0; i < red_size; i++)
               cmap->red[i] |= cmap->red[i] << 8;

          for (i = 0; i < green_size; i++)
               cmap->green[i] |= cmap->green[i] << 8;

          for (i = 0; i < blue_size; i++)
               cmap->blue[i] |= cmap->blue[i] << 8;
     }

     fbdev_ioctl( fbdev, FBIOPUTCMAP, cmap, sizeof(*cmap) );
}

/**********************************************************************************************************************/

DFBResult
fbdev_init_modes( FBDevData *fbdev )
{
     FBDevDataShared *shared;

     D_DEBUG_AT( FBDev_Mode, "%s()\n", __FUNCTION__ );

     D_ASSERT( fbdev != NULL );
     D_ASSERT( fbdev->shared != NULL );

     shared = fbdev->shared;

     read_modes( fbdev );

     if (!shared->modes) {
          /* Try to use current mode. */
          shared->modes = SHCALLOC( shared->shmpool, 1, sizeof(VideoMode) );
          if (!shared->modes)
               return D_OOSHM();

          *shared->modes = shared->mode;

          if (test_mode_simple( fbdev, shared->modes )) {
               D_ERROR( "FBDev/Layer: No supported modes found and current mode not supported!\n"
                        "  -> RGBA %u/%u, %u/%u, %u/%u, %u/%u (%u bits)\n",
                        shared->orig_var.red.length,    shared->orig_var.red.offset,
                        shared->orig_var.green.length,  shared->orig_var.green.offset,
                        shared->orig_var.blue.length,   shared->orig_var.blue.offset,
                        shared->orig_var.transp.length, shared->orig_var.transp.offset,
                        shared->orig_var.bits_per_pixel );

               return DFB_INIT;
          }
     }

     return DFB_OK;
}

VideoMode *
fbdev_find_mode( FBDevData *fbdev,
                 int        width,
                 int        height )
{
     FBDevDataShared *shared;
     VideoMode       *mode;

     D_DEBUG_AT( FBDev_Mode, "%s()\n", __FUNCTION__ );

     D_ASSERT( fbdev != NULL );
     D_ASSERT( fbdev->shared != NULL );

     shared = fbdev->shared;

     mode = shared->modes;

     while (mode) {
          if (mode->xres == width && mode->yres == height)
               break;

          mode = mode->next;
     }

     if (!mode)
          D_ONCE( "no mode found for %dx%d", width, height );

     return mode;
}

void
fbdev_var_to_mode( const struct fb_var_screeninfo *var,
                   VideoMode                      *mode )
{
     D_DEBUG_AT( FBDev_Mode, "%s()\n", __FUNCTION__ );

     mode->xres          = var->xres;
     mode->yres          = var->yres;
     mode->bpp           = var->bits_per_pixel;
     mode->pixclock      = var->pixclock;
     mode->left_margin   = var->left_margin;
     mode->right_margin  = var->right_margin;
     mode->upper_margin  = var->upper_margin;
     mode->lower_margin  = var->lower_margin;
     mode->hsync_len     = var->hsync_len;
     mode->vsync_len     = var->vsync_len;
     mode->hsync_high    = (var->sync  & FB_SYNC_HOR_HIGH_ACT)  ? 1 : 0;
     mode->vsync_high    = (var->sync  & FB_SYNC_VERT_HIGH_ACT) ? 1 : 0;
     mode->csync_high    = (var->sync  & FB_SYNC_COMP_HIGH_ACT) ? 1 : 0;
     mode->laced         = (var->vmode & FB_VMODE_INTERLACED)   ? 1 : 0;
     mode->doubled       = (var->vmode & FB_VMODE_DOUBLE)       ? 1 : 0;
     mode->sync_on_green = (var->sync  & FB_SYNC_ON_GREEN)      ? 1 : 0;
     mode->external_sync = (var->sync  & FB_SYNC_EXT)           ? 1 : 0;
     mode->broadcast     = (var->sync  & FB_SYNC_BROADCAST)     ? 1 : 0;
}

DFBResult
fbdev_test_mode( FBDevData                   *fbdev,
                 const VideoMode             *mode,
                 const CoreLayerRegionConfig *config )
{
     DFBResult                 ret;
     FBDevDataShared          *shared;
     const DFBRectangle       *source;
     struct fb_var_screeninfo  var;
     unsigned int              need_mem;

     D_DEBUG_AT( FBDev_Mode, "%s( %p, %p )\n", __FUNCTION__, mode, config );

     D_ASSERT( fbdev != NULL );
     D_ASSERT( fbdev->shared != NULL );

     shared = fbdev->shared;

     source = &config->source;

     /* Panning support. */
     if (source->w != mode->xres && fbdev->fix->xpanstep == 0)
          return DFB_UNSUPPORTED;
     if (source->h != mode->yres && fbdev->fix->ypanstep == 0 && fbdev->fix->ywrapstep == 0)
          return DFB_UNSUPPORTED;

     var = shared->current_var;
     ret = mode_to_var( mode, config->format, config->width, config->height,
                        fbdev->fix->xpanstep, fbdev->fix->ypanstep, fbdev->fix->ywrapstep, 0, 0, config->buffermode,
                        &var );
     if (ret)
          return ret;

     need_mem = DFB_BYTES_PER_LINE( config->format, var.xres_virtual ) *
                DFB_PLANE_MULTIPLY( config->format, var.yres_virtual );
     if (fbdev->fix->smem_len < need_mem) {
          D_DEBUG_AT( FBDev_Mode, "  -> not enough framebuffer memory (%u < %u)\n", fbdev->fix->smem_len, need_mem );

          return DFB_LIMITEXCEEDED;
     }

     /* Enable test mode. */
     var.activate = FB_ACTIVATE_TEST;

     dfb_gfxcard_lock( GDLF_WAIT | GDLF_SYNC | GDLF_RESET | GDLF_INVALIDATE );

     if (fbdev_ioctl( fbdev, FBIOPUT_VSCREENINFO, &var, sizeof(var) ) < 0) {
          ret = errno2result( errno );
          dfb_gfxcard_unlock();
          D_DEBUG_AT( FBDev_Mode, "  -> FBIOPUT_VSCREENINFO failed!\n" );
          return ret;
     }

     dfb_gfxcard_unlock();

     return DFB_OK;
}

DFBResult
fbdev_set_mode( FBDevData         *fbdev,
                const VideoMode   *mode,
                const CoreSurface *surface,
                unsigned int       xoffset,
                unsigned int       yoffset )
{
     DFBResult                  ret;
     FBDevDataShared           *shared;
     const CoreSurfaceConfig   *config;
     int                        num;
     struct fb_var_screeninfo   var;
     struct fb_var_screeninfo   var2;
     DFBDisplayLayerBufferMode  buffermode;

     D_DEBUG_AT( FBDev_Mode, "%s( %p, %p )\n", __FUNCTION__, mode, surface );

     D_ASSERT( fbdev != NULL );
     D_ASSERT( fbdev->shared != NULL );
     D_ASSERT( mode != NULL );
     D_ASSERT( surface != NULL );

     shared = fbdev->shared;

     config = &surface->config;

     for (num = 0; num < surface->num_buffers; num++) {
          if (surface->buffers[num]->policy == CSP_SYSTEMONLY)
               break;
     }

     switch (num) {
          case 3:
               buffermode = DLBM_TRIPLE;
               break;
          case 2:
               buffermode = DLBM_BACKVIDEO;
               break;
          case 1:
               buffermode = DLBM_FRONTONLY;
               break;
          default:
               D_BUG( "unexpected video buffers number %d", num );
               return DFB_BUG;
     }

     var = shared->current_var;
     ret = mode_to_var( mode, config->format, config->size.w, config->size.h,
                        fbdev->fix->xpanstep, fbdev->fix->ypanstep, fbdev->fix->ywrapstep, xoffset, yoffset, buffermode,
                        &var );
     if (ret) {
          D_ERROR( "FBDev/Mode: Failed to switch to %dx%d (%s) with buffermode %u!\n",
                   config->size.w, config->size.h, dfb_pixelformat_name( config->format ), buffermode );
          return ret;
     }

     dfb_gfxcard_lock( GDLF_WAIT | GDLF_SYNC | GDLF_RESET | GDLF_INVALIDATE );

     if (fbdev_ioctl( fbdev, FBIOPUT_VSCREENINFO, &var, sizeof(var) )) {
          ret = errno2result( errno );
          D_DEBUG_AT( FBDev_Mode, "  -> FBIOPUT_VSCREENINFO failed!\n" );
          goto error;
     }

     if (fbdev_ioctl( fbdev, FBIOGET_VSCREENINFO, &var2, sizeof(var2) )) {
          ret = errno2result( errno );
          D_DEBUG_AT( FBDev_Mode, "  -> FBIOGET_VSCREENINFO failed!\n" );
          goto error;
     }

     if (var.xres != var2.xres || var.xres_virtual != var2.xres_virtual ||
         var.yres != var2.yres || var.yres_virtual != var2.yres_virtual) {
          D_DEBUG_AT( FBDev_Mode, "  -> Variable screen information mismatch (%ux%u [%ux%u] should be %ux%u [%ux%u])\n",
                      var2.xres, var2.yres, var2.xres_virtual, var2.yres_virtual,
                      var.xres, var.yres, var.xres_virtual, var.yres_virtual );
          ret = DFB_IO;
          goto error;
     }

     shared->current_var = var;
     fbdev_var_to_mode( &var, &shared->mode );
     fbdev_ioctl( fbdev, FBIOGET_FSCREENINFO, fbdev->fix, sizeof(*fbdev->fix) );

     D_INFO( "FBDev/Mode: Switched to %ux%u (virtual %ux%u) at %u bits (%s), pitch %u\n",
             var.xres, var.yres, var.xres_virtual, var.yres_virtual, var.bits_per_pixel,
             dfb_pixelformat_name( config->format ), fbdev->fix->line_length );

     /* Some drivers use the palette as gamma ramp, so the gamme ramp has to be initialized to have correct colors. */
     if (config->format == DSPF_RGB332)
          set_rgb332_gamma_ramp( fbdev );
     else
          set_gamma_ramp( fbdev, config->format );

     /* Invalidate original pan offset. */
     shared->orig_var.xoffset = 0;
     shared->orig_var.yoffset = 0;

     surfacemanager_adjust_heap_offset( shared->manager, var.yres_virtual * fbdev->fix->line_length );

     dfb_gfxcard_after_set_var();

     dfb_gfxcard_unlock();

     return DFB_OK;

error:
     dfb_gfxcard_unlock();

     D_ERROR( "FBDev/Mode: Failed to switch to %ux%u (virtual %ux%u) at %u bits (%s)!\n",
              var.xres, var.yres, var.xres_virtual, var.yres_virtual, var.bits_per_pixel,
              dfb_pixelformat_name( config->format ) );

     return ret;
}

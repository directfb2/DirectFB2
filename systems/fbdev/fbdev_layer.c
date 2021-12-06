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

#include <core/layers.h>
#include <core/palette.h>
#include <core/screen.h>
#include <core/screens.h>

#include "fbdev_mode.h"

D_DEBUG_DOMAIN( FBDev_Layer, "FBDev/Layer", "FBDev Layer" );

/**********************************************************************************************************************/

static DFBResult
pan_display( FBDevData *fbdev,
             int        xoffset,
             int        yoffset,
             bool       onsync )
{
     DFBResult                 ret;
     FBDevDataShared          *shared;
     struct fb_var_screeninfo *var;

     D_ASSERT( fbdev != NULL );
     D_ASSERT( fbdev->shared != NULL );

     shared = fbdev->shared;

     var = &shared->current_var;

     if (!fbdev->fix->xpanstep && !fbdev->fix->ypanstep && !fbdev->fix->ywrapstep)
          return DFB_OK;

     if (var->xres_virtual < xoffset + var->xres) {
          D_ERROR( "FBDev/Layer: Panning buffer out of range (xres %u, virtual xres %u, xoffset %d)!\n",
                   var->xres, var->xres_virtual, xoffset );
          return DFB_BUG;
     }

     if (var->yres_virtual < yoffset + var->yres) {
          D_ERROR( "FBDev/Layer: Panning buffer out of range (yres %u, virtual yres %u, yoffset %d)!\n",
                    var->yres, var->yres_virtual, yoffset );
          return DFB_BUG;
     }

     if (fbdev->fix->xpanstep)
          var->xoffset = xoffset - (xoffset % fbdev->fix->xpanstep);
     else
          var->xoffset = 0;

     if (fbdev->fix->ywrapstep) {
          var->yoffset  = yoffset - (yoffset % fbdev->fix->ywrapstep);
          var->vmode   |= FB_VMODE_YWRAP;
     }
     else if (fbdev->fix->ypanstep) {
          var->yoffset  = yoffset - (yoffset % fbdev->fix->ypanstep);
          var->vmode   &= ~FB_VMODE_YWRAP;
     }
     else
          var->yoffset = 0;

     var->activate = onsync ? FB_ACTIVATE_VBL : FB_ACTIVATE_NOW;

     if (fbdev_ioctl( fbdev, FBIOPAN_DISPLAY, var, sizeof(*var) ) < 0) {
          ret = errno2result( errno );
          D_PERROR( "FBDev/Layer: Panning display failed (xoffset = %u, yoffset = %u, ywrap = %d, vbl = %d)!\n",
                    var->xoffset, var->yoffset,
                    (var->vmode & FB_VMODE_YWRAP) ? 1 : 0, (var->activate & FB_ACTIVATE_VBL) ? 1 : 0);

          return ret;
     }

     return DFB_OK;
}

static DFBResult
set_palette( FBDevData   *fbdev,
             CorePalette *palette )
{
     DFBResult        ret;
     int              i;
     FBDevDataShared *shared;
     struct fb_cmap  *cmap;

     D_ASSERT( fbdev != NULL );
     D_ASSERT( fbdev->shared != NULL );
     D_ASSERT( palette != NULL );

     shared = fbdev->shared;

     cmap = &shared->current_cmap;

     cmap->len = palette->num_entries <= 256 ? palette->num_entries : 256;

     for (i = 0; i < cmap->len; i++) {
          cmap->red[i]     = palette->entries[i].r;
          cmap->green[i]   = palette->entries[i].g;
          cmap->blue[i]    = palette->entries[i].b;
          cmap->transp[i]  = 0xff - palette->entries[i].a;

          cmap->red[i]    |= cmap->red[i] << 8;
          cmap->green[i]  |= cmap->green[i] << 8;
          cmap->blue[i]   |= cmap->blue[i] << 8;
          cmap->transp[i] |= cmap->transp[i] << 8;
     }

     if (fbdev_ioctl( fbdev, FBIOPUTCMAP, cmap, sizeof(*cmap) ) < 0) {
          ret = errno2result( errno );
          D_PERROR( "FBDev/Layer: Could not set the palette!\n" );

          return ret;
     }

     return DFB_OK;
}

/**********************************************************************************************************************/

static DFBResult
fbdevPrimaryInitLayer( CoreLayer                  *layer,
                       void                       *driver_data,
                       void                       *layer_data,
                       DFBDisplayLayerDescription *description,
                       DFBDisplayLayerConfig      *config,
                       DFBColorAdjustment         *adjustment )
{
     FBDevData       *fbdev = driver_data;
     FBDevDataShared *shared;

     D_DEBUG_AT( FBDev_Layer, "%s()\n", __FUNCTION__ );

     D_ASSERT( fbdev != NULL );
     D_ASSERT( fbdev->shared != NULL );

     shared = fbdev->shared;

     /* Set type and capabilities. */
     description->type = DLTF_GRAPHICS;
     description->caps = DLCAPS_SURFACE | DLCAPS_BRIGHTNESS | DLCAPS_CONTRAST | DLCAPS_SATURATION;

     /* Set name. */
     snprintf( description->name, DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "FBDev Primary Layer" );

     /* Fill out default color adjustment. */
     adjustment->flags      = DCAF_BRIGHTNESS | DCAF_CONTRAST | DCAF_SATURATION;
     adjustment->brightness = 0x8000;
     adjustment->contrast   = 0x8000;
     adjustment->saturation = 0x8000;

     /* Fill out the default configuration. */
     config->flags       = DLCONF_WIDTH | DLCONF_HEIGHT | DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE;
     config->width       = shared->mode.xres;
     config->height      = shared->mode.yres;
     config->pixelformat = dfb_config->mode.format ?: dfb_pixelformat_for_depth( shared->modes->bpp );
     config->buffermode  = DLBM_FRONTONLY;

     return DFB_OK;
}

static DFBResult
fbdevPrimarySetColorAdjustment( CoreLayer          *layer,
                                void               *driver_data,
                                void               *layer_data,
                                DFBColorAdjustment *adjustment )
{
     DFBResult        ret;
     FBDevData       *fbdev      = driver_data;
     FBDevDataShared *shared;
     struct fb_cmap  *cmap;
     struct fb_cmap  *temp;
     int              brightness = (adjustment->brightness >> 8) - 128;
     int              contrast   =  adjustment->contrast   >> 8;
     int              saturation =  adjustment->saturation >> 8;
     int              r, g, b, i;

     D_DEBUG_AT( FBDev_Layer, "%s()\n", __FUNCTION__ );

     D_ASSERT( fbdev != NULL );
     D_ASSERT( fbdev->shared != NULL );

     shared = fbdev->shared;

     cmap = &shared->current_cmap;
     temp = &shared->temp_cmap;

     if (fbdev->fix->visual != FB_VISUAL_DIRECTCOLOR)
          return DFB_UNIMPLEMENTED;

     /* Use gamma ramp to set color attributes. */
     for (i = 0; i < cmap->len; i++) {
          r = cmap->red[i];
          g = cmap->green[i];
          b = cmap->blue[i];
          r >>= 8;
          g >>= 8;
          b >>= 8;

          /* Brightness adjustment: increase/decrease each color channel by a constant amount as specified by the
             brightness value. */
          if (adjustment->flags & DCAF_BRIGHTNESS) {
               r += brightness;
               g += brightness;
               b += brightness;

               r = CLAMP( r, 0, 255 );
               g = CLAMP( g, 0, 255 );
               b = CLAMP( b, 0, 255 );
          }

          /* Contrast adjustment: increase/decrease the "separation" between colors in proportion to the value specified
             by the contrast control. */
          if (adjustment->flags & DCAF_CONTRAST) {
               /* Increase contrast */
               if (contrast > 128) {
                    int c = contrast - 128;

                    r = ((r + c / 2) / c) * c;
                    g = ((g + c / 2) / c) * c;
                    b = ((b + c / 2) / c) * c;
               }
               /* Decrease contrast */
               else if (contrast < 127) {
                    r = (r * contrast) >> 7;
                    g = (g * contrast) >> 7;
                    b = (b * contrast) >> 7;
               }

               r = CLAMP( r, 0, 255 );
               g = CLAMP( g, 0, 255 );
               b = CLAMP( b, 0, 255 );
          }

          /* Saturation adjustment: mix a proportion of medium gray to the color value. */
          if (adjustment->flags & DCAF_SATURATION) {
               if (saturation > 128) {
                    int gray = saturation - 128;
                    int color = 128 - gray;

                    r = ((r - gray) << 7) / color;
                    g = ((g - gray) << 7) / color;
                    b = ((b - gray) << 7) / color;
               }
               else if (saturation < 128) {
                    int color = saturation;
                    int gray = 128 - color;

                    r = ((r * color) >> 7) + gray;
                    g = ((g * color) >> 7) + gray;
                    b = ((b * color) >> 7) + gray;
               }

               r = CLAMP( r, 0, 255 );
               g = CLAMP( g, 0, 255 );
               b = CLAMP( b, 0, 255 );
          }

          r |= r << 8;
          g |= g << 8;
          b |= b << 8;

          temp->red[i]   = r;
          temp->green[i] = g;
          temp->blue[i]  = b;
     }

     temp->start = cmap->start;
     temp->len   = cmap->len;

     if (fbdev_ioctl( fbdev, FBIOPUTCMAP, temp, sizeof(*temp) ) < 0) {
          ret = errno2result( errno );
          D_PERROR( "FBDev/Layer: Could not set the palette!\n" );
          return ret;
     }

     return DFB_OK;
}

static DFBResult
fbdevPrimaryTestRegion( CoreLayer                  *layer,
                        void                       *driver_data,
                        void                       *layer_data,
                        CoreLayerRegionConfig      *config,
                        CoreLayerRegionConfigFlags *ret_failed )
{
     CoreLayerRegionConfigFlags  failed = CLRCF_NONE;
     FBDevData                  *fbdev  = driver_data;
     FBDevDataShared            *shared;
     VideoMode                  *mode, dummy;

     D_DEBUG_AT( FBDev_Layer, "%s( %dx%d, %s )\n", __FUNCTION__,
                 config->source.w, config->source.h, dfb_pixelformat_name( config->format ) );

     D_ASSERT( fbdev != NULL );
     D_ASSERT( fbdev->shared != NULL );

     shared = fbdev->shared;

     mode = fbdev_find_mode( fbdev, config->source.w, config->source.h );
     if (!mode) {
          dummy = shared->mode;

          dummy.xres = config->source.w;
          dummy.yres = config->source.h;
          dummy.bpp  = DFB_BITS_PER_PIXEL( config->format );

          mode = &dummy;
     }

     if (fbdev_test_mode( fbdev, mode, config ))
          failed |= CLRCF_WIDTH | CLRCF_HEIGHT | CLRCF_FORMAT | CLRCF_BUFFERMODE;

     if (config->options)
          failed |= CLRCF_OPTIONS;

     if ((config->source.x && !fbdev->fix->xpanstep) ||
         (config->source.y && !fbdev->fix->ypanstep && !fbdev->fix->ywrapstep))
          failed |= CLRCF_SOURCE;

     if (ret_failed)
          *ret_failed = failed;

     if (failed)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult
fbdevPrimarySetRegion( CoreLayer                  *layer,
                       void                       *driver_data,
                       void                       *layer_data,
                       void                       *region_data,
                       CoreLayerRegionConfig      *config,
                       CoreLayerRegionConfigFlags  updated,
                       CoreSurface                *surface,
                       CorePalette                *palette,
                       CoreSurfaceBufferLock      *left_lock,
                       CoreSurfaceBufferLock      *right_lock )
{
     DFBResult        ret;
     FBDevData       *fbdev = driver_data;
     FBDevDataShared *shared;

     D_DEBUG_AT( FBDev_Layer, "%s()\n", __FUNCTION__ );

     D_ASSERT( fbdev != NULL );
     D_ASSERT( fbdev->shared != NULL );

     shared = fbdev->shared;

     if (updated & (CLRCF_SOURCE | CLRCF_WIDTH | CLRCF_HEIGHT | CLRCF_FORMAT | CLRCF_BUFFERMODE)) {
          if (updated & (CLRCF_WIDTH | CLRCF_HEIGHT | CLRCF_FORMAT | CLRCF_BUFFERMODE) ||
              config->source.w != shared->current_var.xres || config->source.h != shared->current_var.yres) {
               VideoMode *mode, dummy;

               D_INFO( "FBDev/Mode: Setting %dx%d %s\n", config->source.w, config->source.h,
                       dfb_pixelformat_name( surface->config.format ) );

               mode = fbdev_find_mode( fbdev, config->source.w, config->source.h );
               if (!mode) {
                    dummy = shared->mode;

                    dummy.xres = config->source.w;
                    dummy.yres = config->source.h;
                    dummy.bpp  = DFB_BITS_PER_PIXEL( config->format );

                    mode = &dummy;
               }

               ret = fbdev_set_mode( fbdev, mode, surface,
                                     config->source.x, left_lock->offset / left_lock->pitch + config->source.y );
               if (ret)
                    return ret;
          }
          else {
               ret = pan_display( fbdev, config->source.x, left_lock->offset / left_lock->pitch + config->source.y,
                                  true );
               if (ret)
                    return ret;
          }
     }

     if ((updated & CLRCF_PALETTE) && palette)
          set_palette( fbdev, palette );

     shared->config = *config;

     return DFB_OK;
}

static DFBResult
fbdevPrimaryFlipRegion( CoreLayer             *layer,
                        void                  *driver_data,
                        void                  *layer_data,
                        void                  *region_data,
                        CoreSurface           *surface,
                        DFBSurfaceFlipFlags    flags,
                        const DFBRegion       *left_update,
                        CoreSurfaceBufferLock *left_lock,
                        const DFBRegion       *right_update,
                        CoreSurfaceBufferLock *right_lock )
{
     DFBResult              ret;
     FBDevData             *fbdev = driver_data;
     FBDevDataShared       *shared;
     CoreLayerRegionConfig *config;

     D_DEBUG_AT( FBDev_Layer, "%s()\n", __FUNCTION__ );

     D_ASSERT( fbdev != NULL );
     D_ASSERT( fbdev->shared != NULL );

     shared = fbdev->shared;

     config = &shared->config;

     if (((flags & DSFLIP_WAITFORSYNC) == DSFLIP_WAITFORSYNC) && !shared->pollvsync_after)
          dfb_screen_wait_vsync( dfb_screen_at( DSCID_PRIMARY ) );

     ret = pan_display( fbdev, config->source.x, left_lock->offset / left_lock->pitch + config->source.y,
                        (flags & DSFLIP_WAITFORSYNC) == DSFLIP_ONSYNC );
     if (ret)
          return ret;

     if ((flags & DSFLIP_WAIT) && (shared->pollvsync_after || !(flags & DSFLIP_ONSYNC)))
          dfb_screen_wait_vsync( dfb_screen_at( DSCID_PRIMARY ) );

     dfb_surface_flip( surface, false );

     return DFB_OK;
}

const DisplayLayerFuncs fbdevPrimaryLayerFuncs = {
     .InitLayer          = fbdevPrimaryInitLayer,
     .SetColorAdjustment = fbdevPrimarySetColorAdjustment,
     .TestRegion         = fbdevPrimaryTestRegion,
     .SetRegion          = fbdevPrimarySetRegion,
     .FlipRegion         = fbdevPrimaryFlipRegion,
};

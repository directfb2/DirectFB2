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

#include "nuttxfb_system.h"

D_DEBUG_DOMAIN( NuttXFB_Layer, "NuttXFB/Layer", "NuttXFB Layer" );

/**********************************************************************************************************************/

static DFBResult
nuttxfbPrimaryInitLayer( CoreLayer                  *layer,
                         void                       *driver_data,
                         void                       *layer_data,
                         DFBDisplayLayerDescription *description,
                         DFBDisplayLayerConfig      *config,
                         DFBColorAdjustment         *adjustment )
{
     NuttXFBData       *nuttxfb = driver_data;
     NuttXFBDataShared *shared;

     D_DEBUG_AT( NuttXFB_Layer, "%s()\n", __FUNCTION__ );

     D_ASSERT( nuttxfb != NULL );
     D_ASSERT( nuttxfb->shared != NULL );

     shared = nuttxfb->shared;

     /* Set type and capabilities. */
     description->type = DLTF_GRAPHICS;
     description->caps = DLCAPS_SURFACE;

     /* Set name. */
     snprintf( description->name, DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "NuttXFB Primary Layer" );

     /* Fill out the default configuration. */
     config->flags       = DLCONF_WIDTH | DLCONF_HEIGHT | DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE;
     config->width       = shared->mode.xres;
     config->height      = shared->mode.yres;
     config->pixelformat = dfb_pixelformat_for_depth( shared->mode.bpp );
     config->buffermode  = DLBM_FRONTONLY;

     return DFB_OK;
}

static DFBResult
nuttxfbPrimaryTestRegion( CoreLayer                  *layer,
                          void                       *driver_data,
                          void                       *layer_data,
                          CoreLayerRegionConfig      *config,
                          CoreLayerRegionConfigFlags *ret_failed )
{
     CoreLayerRegionConfigFlags failed = CLRCF_NONE;

     D_DEBUG_AT( NuttXFB_Layer, "%s( %dx%d, %s )\n", __FUNCTION__,
                 config->source.w, config->source.h, dfb_pixelformat_name( config->format ) );

     switch (config->buffermode) {
          case DLBM_FRONTONLY:
          case DLBM_BACKSYSTEM:
               break;

          default:
               failed |= CLRCF_BUFFERMODE;
               break;
     }

     if (config->options)
          failed |= CLRCF_OPTIONS;

     if (ret_failed)
          *ret_failed = failed;

     if (failed)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult
nuttxfbPrimarySetRegion( CoreLayer                  *layer,
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
     D_DEBUG_AT( NuttXFB_Layer, "%s()\n", __FUNCTION__ );

     return DFB_OK;
}

static DFBResult
nuttxfbPrimaryUpdateRegion( CoreLayer             *layer,
                            void                  *driver_data,
                            void                  *layer_data,
                            void                  *region_data,
                            CoreSurface           *surface,
                            const DFBRegion       *left_update,
                            CoreSurfaceBufferLock *left_lock,
                            const DFBRegion       *right_update,
                            CoreSurfaceBufferLock *right_lock )
{
     D_DEBUG_AT( NuttXFB_Layer, "%s()\n", __FUNCTION__ );

     usleep( 1 );

     return DFB_OK;
}

const DisplayLayerFuncs nuttxfbPrimaryLayerFuncs = {
     .InitLayer    = nuttxfbPrimaryInitLayer,
     .TestRegion   = nuttxfbPrimaryTestRegion,
     .SetRegion    = nuttxfbPrimarySetRegion,
     .UpdateRegion = nuttxfbPrimaryUpdateRegion
};

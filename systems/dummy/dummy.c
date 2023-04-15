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
#include <core/screens.h>
#include <core/core_system.h>

D_DEBUG_DOMAIN( Dummy_System, "Dummy/System", "Dummy System Module" );

DFB_CORE_SYSTEM( dummy )

/**********************************************************************************************************************/

#define DUMMY_WIDTH  8
#define DUMMY_HEIGHT 8
#define DUMMY_FORMAT DSPF_ARGB

static DFBResult
dummyInitScreen( CoreScreen           *screen,
                 void                 *driver_data,
                 void                 *screen_data,
                 DFBScreenDescription *description )
{
     /* Set name. */
     snprintf( description->name, DFB_SCREEN_DESC_NAME_LENGTH, "Dummy Screen" );

     return DFB_OK;
}

static DFBResult
dummyGetScreenSize( CoreScreen *screen,
                    void       *driver_data,
                    void       *screen_data,
                    int        *ret_width,
                    int        *ret_height )
{
     *ret_width  = dfb_config->mode.width  ?: DUMMY_WIDTH;
     *ret_height = dfb_config->mode.height ?: DUMMY_HEIGHT;

     return DFB_OK;
}

static ScreenFuncs dummyScreenFuncs = {
     .InitScreen    = dummyInitScreen,
     .GetScreenSize = dummyGetScreenSize
};

/**********************************************************************************************************************/

static DFBResult
dummyPrimaryInitLayer( CoreLayer                  *layer,
                       void                       *driver_data,
                       void                       *layer_data,
                       DFBDisplayLayerDescription *description,
                       DFBDisplayLayerConfig      *config,
                       DFBColorAdjustment         *adjustment )
{
     /* Set type and capabilities. */
     description->type             = DLTF_GRAPHICS;
     description->caps             = DLCAPS_SURFACE;
     description->surface_caps     = DSCAPS_SYSTEMONLY;
     description->surface_accessor = CSAID_CPU;

     /* Set name. */
     snprintf( description->name, DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "Dummy Primary Layer" );

     /* Fill out the default configuration. */
     config->flags       = DLCONF_WIDTH | DLCONF_HEIGHT | DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE;
     config->width       = dfb_config->mode.width  ?: DUMMY_WIDTH;
     config->height      = dfb_config->mode.height ?: DUMMY_HEIGHT;
     config->pixelformat = dfb_config->mode.format ?: DUMMY_FORMAT;
     config->buffermode  = DLBM_FRONTONLY;

     return DFB_OK;
}

static DFBResult
dummyPrimaryTestRegion( CoreLayer                  *layer,
                        void                       *driver_data,
                        void                       *layer_data,
                        CoreLayerRegionConfig      *config,
                        CoreLayerRegionConfigFlags *ret_failed )
{
     if (ret_failed)
          *ret_failed = DLCONF_NONE;

     return DFB_OK;
}

static DFBResult
dummyPrimarySetRegion( CoreLayer                  *layer,
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
     return DFB_OK;
}

static DFBResult
dummyPrimaryFlipRegion( CoreLayer             *layer,
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
     dfb_surface_notify_display( surface, left_lock->buffer );

     return DFB_OK;
}

static DFBResult
dummyPrimaryUpdateRegion( CoreLayer             *layer,
                          void                  *driver_data,
                          void                  *layer_data,
                          void                  *region_data,
                          CoreSurface           *surface,
                          const DFBRegion       *left_update,
                          CoreSurfaceBufferLock *left_lock,
                          const DFBRegion       *right_update,
                          CoreSurfaceBufferLock *right_lock )
{
     dfb_surface_notify_display( surface, left_lock->buffer );

     return DFB_OK;
}

static DisplayLayerFuncs dummyPrimaryLayerFuncs = {
     .InitLayer    = dummyPrimaryInitLayer,
     .TestRegion   = dummyPrimaryTestRegion,
     .SetRegion    = dummyPrimarySetRegion,
     .FlipRegion   = dummyPrimaryFlipRegion,
     .UpdateRegion = dummyPrimaryUpdateRegion
};

/**********************************************************************************************************************/

static void
system_get_info( CoreSystemInfo *info )
{
     info->version.major = 0;
     info->version.minor = 1;

     info->caps = CSCAPS_ACCELERATION | CSCAPS_NOTIFY_DISPLAY | CSCAPS_SYSMEM_EXTERNAL;

     snprintf( info->name,   DFB_CORE_SYSTEM_INFO_NAME_LENGTH,   "Dummy" );
     snprintf( info->vendor, DFB_CORE_SYSTEM_INFO_VENDOR_LENGTH, "DirectFB" );
}

static DFBResult
system_initialize( CoreDFB  *core,
                   void    **ret_data )
{
     CoreScreen *screen;

     D_DEBUG_AT( Dummy_System, "%s()\n", __FUNCTION__ );

     D_INFO( "Dummy/System: Using offscreen\n" );

     screen = dfb_screens_register( NULL, &dummyScreenFuncs );

     dfb_layers_register( screen, NULL, &dummyPrimaryLayerFuncs );

     return DFB_OK;
}

static DFBResult
system_join( CoreDFB  *core,
             void    **ret_data )
{
     CoreScreen *screen;

     D_DEBUG_AT( Dummy_System, "%s()\n", __FUNCTION__ );

     D_INFO( "Dummy/System: Using offscreen\n" );

     screen = dfb_screens_register( NULL, &dummyScreenFuncs );

     dfb_layers_register( screen, NULL, &dummyPrimaryLayerFuncs );

     return DFB_OK;
}

static DFBResult
system_shutdown( bool emergency )
{
     D_DEBUG_AT( Dummy_System, "%s()\n", __FUNCTION__ );

     return DFB_OK;
}

static DFBResult
system_leave( bool emergency )
{
     D_DEBUG_AT( Dummy_System, "%s()\n", __FUNCTION__ );

     return DFB_OK;
}

static DFBResult
system_suspend()
{
     return DFB_OK;
}

static DFBResult
system_resume()
{
     return DFB_OK;
}

static VideoMode *
system_get_modes()
{
     return NULL;
}

static VideoMode *
system_get_current_mode()
{
     return NULL;
}

static DFBResult
system_thread_init()
{
     return DFB_OK;
}

static bool
system_input_filter( CoreInputDevice *device,
                     DFBInputEvent   *event )
{
     return false;
}

static volatile void *
system_map_mmio( unsigned int    offset,
                 int             length )
{
     return NULL;
}

static void
system_unmap_mmio( volatile void  *addr,
                   int             length )
{
}

static int
system_get_accelerator()
{
     return direct_config_get_int_value( "accelerator" );
}

static unsigned long
system_video_memory_physical( unsigned int offset )
{
     return 0;
}

static void *
system_video_memory_virtual( unsigned int offset )
{
     return NULL;
}

static unsigned int
system_videoram_length()
{
     return 0;
}

static void
system_get_busid( int *ret_bus,
                  int *ret_dev,
                  int *ret_func )
{
}

static void
system_get_deviceid( unsigned int *ret_vendor_id,
                     unsigned int *ret_device_id )
{
}

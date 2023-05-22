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

#include <core/core.h>
#include <core/core_system.h>
#include <core/layers.h>
#include <core/screens.h>
#include <core/surface_pool.h>
#include <direct/system.h>
#include <fusion/shmalloc.h>

#include "nuttxfb_system.h"

D_DEBUG_DOMAIN( NuttXFB_System, "NuttXFB/System", "NuttXFB System Module" );

DFB_CORE_SYSTEM( nuttxfb )

/**********************************************************************************************************************/

extern const ScreenFuncs       nuttxfbScreenFuncs;
extern const DisplayLayerFuncs nuttxfbPrimaryLayerFuncs;
extern const SurfacePoolFuncs  nuttxfbSurfacePoolFuncs;

static DFBResult
local_init( const char  *device_name,
            NuttXFBData *nuttxfb )
{
     CoreScreen *screen;

     /* Open framebuffer device. */
     nuttxfb->fd = open( device_name, O_RDWR );
     if (nuttxfb->fd < 0) {
          D_PERROR( "NuttXFB/System: Failed to open '%s'!\n", device_name );
          return DFB_INIT;
     }

     /* Retrieve plane information. */
     nuttxfb->planeinfo = D_CALLOC( 1, sizeof(struct fb_planeinfo_s) );
     if (!nuttxfb->planeinfo)
          return D_OOM();

     if (ioctl( nuttxfb->fd, FBIOGET_PLANEINFO, nuttxfb->planeinfo ) < 0) {
          D_PERROR( "NuttXFB/System: Could not retrieve plane information!\n" );
          return DFB_INIT;
     }

     D_INFO( "NuttXFB/System: Found display with framebuffer at %p, "_ZU"k\n",
             nuttxfb->planeinfo->fbmem, nuttxfb->planeinfo->fblen >> 10 );

     /* Map the framebuffer. */
     nuttxfb->addr = mmap( NULL, nuttxfb->planeinfo->fblen, PROT_READ | PROT_WRITE, MAP_SHARED, nuttxfb->fd, 0 );
     if (nuttxfb->addr == MAP_FAILED) {
          D_PERROR( "NuttXFB/System: Could not mmap the framebuffer!\n" );
          return DFB_INIT;
     }

     screen = dfb_screens_register( nuttxfb, &nuttxfbScreenFuncs );

     dfb_layers_register( screen, nuttxfb, &nuttxfbPrimaryLayerFuncs );

     return DFB_OK;
}

static DFBResult
local_deinit( NuttXFBData *nuttxfb )
{
     if (nuttxfb->addr)
          munmap( nuttxfb->addr, nuttxfb->planeinfo->fblen );

     if (nuttxfb->planeinfo)
          D_FREE( nuttxfb->planeinfo );

     if (nuttxfb->fd != -1)
          close( nuttxfb->fd );

     return DFB_OK;
}

/**********************************************************************************************************************/

static void
system_get_info( CoreSystemInfo *info )
{
     info->version.major = 0;
     info->version.minor = 1;

     info->caps = CSCAPS_ACCELERATION;

     snprintf( info->name,   DFB_CORE_SYSTEM_INFO_NAME_LENGTH,   "NuttXFB" );
     snprintf( info->vendor, DFB_CORE_SYSTEM_INFO_VENDOR_LENGTH, "DirectFB" );
}

static DFBResult
system_initialize( CoreDFB  *core,
                   void    **ret_data )
{
     DFBResult              ret;
     struct fb_videoinfo_s  videoinfo;
     NuttXFBData           *nuttxfb;
     NuttXFBDataShared     *shared;
     FusionSHMPoolShared   *pool;
     const char            *value;

     D_DEBUG_AT( NuttXFB_System, "%s()\n", __FUNCTION__ );

     nuttxfb = D_CALLOC( 1, sizeof(NuttXFBData) );
     if (!nuttxfb)
          return D_OOM();

     nuttxfb->core = core;

     pool = dfb_core_shmpool( core );

     shared = SHCALLOC( pool, 1, sizeof(NuttXFBDataShared) );
     if (!shared) {
          D_FREE( nuttxfb );
          return D_OOSHM();
     }

     shared->shmpool = pool;

     nuttxfb->shared = shared;

     if ((value = direct_config_get_value( "nuttxfb" ))) {
          direct_snputs( shared->device_name, value, 255 );
          D_INFO( "NuttXFB/System: Using device %s as specified in DirectFB configuration\n", shared->device_name );
     }
     else if (getenv( "FRAMEBUFFER" ) && *direct_getenv( "FRAMEBUFFER" ) != '\0') {
          direct_snputs( shared->device_name, direct_getenv( "FRAMEBUFFER" ), 255 );
          D_INFO( "NuttXFB/System: Using device %s as set in FRAMEBUFFER environment variable\n", shared->device_name );
     }
     else {
          snprintf( shared->device_name, 255, "/dev/fb0" );
          D_INFO( "NuttXFB/System: Using device %s (default)\n", shared->device_name );
     }

     ret = local_init( shared->device_name, nuttxfb );
     if (ret)
          goto error;

     if (ioctl( nuttxfb->fd, FBIOGET_VIDEOINFO, &videoinfo ) < 0) {
          D_PERROR( "NuttXFB/System: Could not get video information!\n" );
          ret = DFB_INIT;
          goto error;
     }

     shared->mode.xres = videoinfo.xres;
     shared->mode.yres = videoinfo.yres;
     shared->mode.bpp  = nuttxfb->planeinfo->bpp;

     *ret_data = nuttxfb;

     ret = dfb_surface_pool_initialize( core, &nuttxfbSurfacePoolFuncs, &shared->pool );
     if (ret)
          goto error;

     ret = core_arena_add_shared_field( core, "nuttxfb", shared );
     if (ret)
          goto error;

     return DFB_OK;

error:
     local_deinit( nuttxfb );

     SHFREE( pool, shared );

     D_FREE( nuttxfb );

     return ret;
}

static DFBResult
system_join( CoreDFB  *core,
             void    **ret_data )
{
     DFBResult          ret;
     NuttXFBData       *nuttxfb;
     NuttXFBDataShared *shared;

     D_DEBUG_AT( NuttXFB_System, "%s()\n", __FUNCTION__ );

     nuttxfb = D_CALLOC( 1, sizeof(NuttXFBData) );
     if (!nuttxfb)
          return D_OOM();

     nuttxfb->core = core;

     ret = core_arena_get_shared_field( core, "nuttxfb", (void**) &shared );
     if (ret) {
          D_FREE( nuttxfb );
          return ret;
     }

     nuttxfb->shared = shared;

     ret = local_init( shared->device_name, nuttxfb );
     if (ret)
          goto error;

     *ret_data = nuttxfb;

     ret = dfb_surface_pool_join( core, shared->pool, &nuttxfbSurfacePoolFuncs );
     if (ret)
          goto error;

     return DFB_OK;

error:
     local_deinit( nuttxfb );

     D_FREE( nuttxfb );

     return ret;
}

static DFBResult
system_shutdown( bool emergency )
{
     NuttXFBData       *nuttxfb = dfb_system_data();
     NuttXFBDataShared *shared;

     D_DEBUG_AT( NuttXFB_System, "%s()\n", __FUNCTION__ );

     D_ASSERT( nuttxfb != NULL );
     D_ASSERT( nuttxfb->shared != NULL );

     shared = nuttxfb->shared;

     dfb_surface_pool_destroy( shared->pool );

     local_deinit( nuttxfb );

     SHFREE( shared->shmpool, shared );

     D_FREE( nuttxfb );

     return DFB_OK;
}

static DFBResult
system_leave( bool emergency )
{
     NuttXFBData       *nuttxfb = dfb_system_data();
     NuttXFBDataShared *shared;

     D_DEBUG_AT( NuttXFB_System, "%s()\n", __FUNCTION__ );

     D_ASSERT( nuttxfb != NULL );
     D_ASSERT( nuttxfb->shared != NULL );

     shared = nuttxfb->shared;

     dfb_surface_pool_leave( shared->pool );

     local_deinit( nuttxfb );

     D_FREE( nuttxfb );

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
     NuttXFBData       *nuttx = dfb_system_data();
     NuttXFBDataShared *shared;

     D_ASSERT( nuttx != NULL );
     D_ASSERT( nuttx->shared != NULL );

     shared = nuttx->shared;

     return &shared->mode;
}

static VideoMode *
system_get_current_mode()
{
     NuttXFBData       *nuttx = dfb_system_data();
     NuttXFBDataShared *shared;

     D_ASSERT( nuttx != NULL );
     D_ASSERT( nuttx->shared != NULL );

     shared = nuttx->shared;

     return &shared->mode;
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
system_map_mmio( unsigned int offset,
                 int          length )
{
     return NULL;
}

static void
system_unmap_mmio( volatile void *addr,
                   int            length )
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
     NuttXFBData *nuttxfb = dfb_system_data();

     D_ASSERT( nuttxfb != NULL );

     return (unsigned long) nuttxfb->planeinfo->fbmem;
}

static void *
system_video_memory_virtual( unsigned int offset )
{
     NuttXFBData *nuttxfb = dfb_system_data();

     D_ASSERT( nuttxfb != NULL );

     return nuttxfb->addr + offset;
}

static unsigned int
system_videoram_length()
{
     NuttXFBData *nuttxfb = dfb_system_data();

     D_ASSERT( nuttxfb != NULL );

     return nuttxfb->planeinfo->fblen;
}

static void
system_get_busid( int *ret_bus,
                  int *ret_dev,
                  int *ret_func )
{
     return;
}

static void
system_get_deviceid( unsigned int *ret_vendor_id,
                     unsigned int *ret_device_id )
{
     return;
}

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

#include <core/screens.h>
#include <misc/conf.h>

#include "fbdev_mode.h"

D_DEBUG_DOMAIN( FBDev_Screen, "FBDev/Screen", "FBDev Screen" );

/**********************************************************************************************************************/

static DFBResult
fbdevInitScreen( CoreScreen           *screen,
                 void                 *driver_data,
                 void                 *screen_data,
                 DFBScreenDescription *description )
{
     FBDevData       *fbdev = driver_data;
     FBDevDataShared *shared;
     VideoMode       *mode;
     int              count = 0;

     D_DEBUG_AT( FBDev_Screen, "%s()\n", __FUNCTION__ );

     D_ASSERT( fbdev != NULL );
     D_ASSERT( fbdev->shared != NULL );

     shared = fbdev->shared;

     mode = shared->modes;

     /* Set capabilities. */
     description->caps = DSCCAPS_VSYNC | DSCCAPS_POWER_MANAGEMENT;

     /* Set name. */
     snprintf( description->name, DFB_SCREEN_DESC_NAME_LENGTH, "FBDev Screen" );

     while (mode) {
          count++;
          mode = mode->next;
     }

     if (dfb_config->mode.width && dfb_config->mode.height) {
          mode = fbdev_find_mode( fbdev, dfb_config->mode.width, dfb_config->mode.height );
          if (mode)
               shared->mode = *mode;
     }

     D_INFO( "FBDev/Screen: Default mode is %dx%d (%d modes in total)\n",
             shared->mode.xres, shared->mode.yres, count );

     return DFB_OK;
}

static DFBResult
fbdevSetPowerMode( CoreScreen         *screen,
                   void               *driver_data,
                   void               *screen_data,
                   DFBScreenPowerMode  mode )
{
     FBDevData *fbdev = driver_data;
     int        level;

     D_DEBUG_AT( FBDev_Screen, "%s()\n", __FUNCTION__ );

     switch (mode) {
          case DSPM_OFF:
               level = 4;
               break;
          case DSPM_SUSPEND:
               level = 3;
               break;
          case DSPM_STANDBY:
               level = 2;
               break;
          case DSPM_ON:
               level = 0;
               break;
          default:
               return DFB_INVARG;
     }

     return fbdev_ioctl( fbdev, FBIOBLANK, (void*)(long) level, sizeof(void*) );
}

static inline void
waitretrace( void )
{
#if defined(__i386__) || defined(__x86_64__)
     if (iopl( 3 ))
          return;

     if (!(inb( 0x3cc ) & 1)) {
          while (  inb( 0x3ba ) & 0x8);
          while (!(inb( 0x3ba ) & 0x8)) ;
     }
     else {
          while (  inb( 0x3da ) & 0x8);
          while (!(inb( 0x3da ) & 0x8));
     }
#endif
}

static DFBResult
fbdevWaitVSync( CoreScreen *screen,
                void       *driver_data,
                void       *screen_data )
{
     static unsigned int  zero  = 0;
     FBDevData           *fbdev = driver_data;
     FBDevDataShared     *shared;

     D_DEBUG_AT( FBDev_Screen, "%s()\n", __FUNCTION__ );

     D_ASSERT( fbdev != NULL );
     D_ASSERT( fbdev->shared != NULL );

     shared = fbdev->shared;

     if (shared->pollvsync_none)
          return DFB_OK;

     if (fbdev_ioctl( fbdev, FBIO_WAITFORVSYNC, &zero, sizeof(zero) ))
          waitretrace();

     return DFB_OK;
}

static DFBResult
fbdevGetScreenSize( CoreScreen *screen,
                    void       *driver_data,
                    void       *screen_data,
                    int        *ret_width,
                    int        *ret_height )
{
     FBDevData       *fbdev = driver_data;
     FBDevDataShared *shared;

     D_DEBUG_AT( FBDev_Screen, "%s()\n", __FUNCTION__ );

     D_ASSERT( fbdev != NULL );
     D_ASSERT( fbdev->shared != NULL );

     shared = fbdev->shared;

     *ret_width  = shared->mode.xres;
     *ret_height = shared->mode.yres;

     return DFB_OK;
}

static DFBResult
fbdevGetVSyncCount( CoreScreen    *screen,
                    void          *driver_data,
                    void          *screen_data,
                    unsigned long *ret_count )
{
     FBDevData        *fbdev = driver_data;
     struct fb_vblank  vblank;

     D_DEBUG_AT( FBDev_Screen, "%s()\n", __FUNCTION__ );

     if (!ret_count)
          return DFB_INVARG;

     if (fbdev_ioctl( fbdev, FBIOGET_VBLANK, &vblank, sizeof(vblank) ))
          return errno2result( errno );

     if (!D_FLAGS_IS_SET( vblank.flags, FB_VBLANK_HAVE_COUNT ))
          return DFB_UNSUPPORTED;

     *ret_count = vblank.count;

     return DFB_OK;
}

const ScreenFuncs fbdevScreenFuncs = {
     .InitScreen    = fbdevInitScreen,
     .SetPowerMode  = fbdevSetPowerMode,
     .WaitVSync     = fbdevWaitVSync,
     .GetScreenSize = fbdevGetScreenSize,
     .GetVSyncCount = fbdevGetVSyncCount
};

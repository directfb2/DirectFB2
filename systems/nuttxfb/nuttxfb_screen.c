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

#include "nuttxfb_system.h"

D_DEBUG_DOMAIN( NuttXFB_Screen, "NuttXFB/Screen", "NuttXFB Screen" );

/**********************************************************************************************************************/

static DFBResult
nuttxfbInitScreen( CoreScreen           *screen,
                   void                 *driver_data,
                   void                 *screen_data,
                   DFBScreenDescription *description )
{
     NuttXFBData       *nuttxfb = driver_data;
     NuttXFBDataShared *shared;

     D_DEBUG_AT( NuttXFB_Screen, "%s()\n", __FUNCTION__ );

     D_ASSERT( nuttxfb != NULL );
     D_ASSERT( nuttxfb->shared != NULL );

     shared = nuttxfb->shared;

     /* Set name. */
     snprintf( description->name, DFB_SCREEN_DESC_NAME_LENGTH, "NuttX Screen" );

     D_INFO( "NuttXFB/Screen: Default mode is %dx%d\n", shared->mode.xres, shared->mode.yres );

     return DFB_OK;
}

static DFBResult
nuttxfbGetScreenSize( CoreScreen *screen,
                      void       *driver_data,
                      void       *screen_data,
                      int        *ret_width,
                      int        *ret_height )
{
     NuttXFBData       *nuttxfb = driver_data;
     NuttXFBDataShared *shared;

     D_DEBUG_AT( NuttXFB_Screen, "%s()\n", __FUNCTION__ );

     D_ASSERT( nuttxfb != NULL );
     D_ASSERT( nuttxfb->shared != NULL );

     shared = nuttxfb->shared;

     *ret_width  = shared->mode.xres;
     *ret_height = shared->mode.yres;

     return DFB_OK;
}

const ScreenFuncs nuttxfbScreenFuncs = {
     .InitScreen    = nuttxfbInitScreen,
     .GetScreenSize = nuttxfbGetScreenSize,
};

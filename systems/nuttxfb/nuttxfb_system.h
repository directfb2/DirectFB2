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

#ifndef __NUTTXFB_SYSTEM_H__
#define __NUTTXFB_SYSTEM_H__

#include <core/coretypes.h>
#include <core/video_mode.h>
#include <fusion/types.h>
#include <nuttx/video/fb.h>

/**********************************************************************************************************************/

typedef struct {
     FusionSHMPoolShared *shmpool;

     CoreSurfacePool     *pool;

     char                 device_name[256]; /* NuttXFB device name, e.g. /dev/fb0 */

     VideoMode            mode;             /* current video mode */
} NuttXFBDataShared;

typedef struct {
     NuttXFBDataShared     *shared;

     CoreDFB               *core;

     int                    fd;        /* framebuffer file descriptor */

     struct fb_planeinfo_s *planeinfo; /* plane information */

     void                  *addr;      /* framebuffer memory address */
} NuttXFBData;

#endif

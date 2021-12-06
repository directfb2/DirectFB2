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

#ifndef __FBDEV_SYSTEM_H__
#define __FBDEV_SYSTEM_H__

#include <core/layer_region.h>
#include <core/system.h>
#include <linux/fb.h>

#include "fbdev_surfacemanager.h"

/**********************************************************************************************************************/

typedef struct {
     FusionSHMPoolShared      *shmpool;

     CoreSurfacePool          *pool;

     char                      device_name[256];    /* FBDev device name, e.g. /dev/fb0 */
     char                      modes_file[256];     /* FBDev modes file, e.g. /etc/fb.modes */

     bool                      vt;                  /* use VT handling */

     bool                      pollvsync_after;     /* wait for the vertical retrace after flipping */
     bool                      pollvsync_none;      /* disable polling for vertical retrace */

     VideoMode                *modes;               /* linked list of valid video modes */
     VideoMode                 mode;                /* current video mode */

     struct fb_var_screeninfo  orig_var;            /* variable screen information before DirectFB was started */
     struct fb_var_screeninfo  current_var;         /* variable screen information set by DirectFB */

     unsigned long             page_mask;           /* PAGE_SIZE - 1 */

     FusionSHMPoolShared      *shmpool_data;        /* shared memory pool for palette values defined in a colormap */
     void                     *orig_cmap_memory;    /* memory to store the values of the original palette */
     void                     *current_cmap_memory; /* memory to store the values of the current palette */
     void                     *temp_cmap_memory;    /* memory to store the values of the adjusted palette */
     struct fb_cmap            orig_cmap;           /* original colormap */
     struct fb_cmap            current_cmap;        /* current colormap */
     struct fb_cmap            temp_cmap;           /* adjusted colormap */

     struct {
          int                  bus;                 /* PCI Bus */
          int                  dev;                 /* PCI Device */
          int                  func;                /* PCI Function */
     } pci;

     struct {
          unsigned short       vendor;               /* graphics device vendor id */
          unsigned short       model;                /* graphics device model id */
     } device;

     FusionCall                call;                 /* ioctl rpc */

     CoreLayerRegionConfig     config;               /* configuration of the layer region */

     SurfaceManager           *manager;              /* surface manager */
} FBDevDataShared;

typedef struct {
     FBDevDataShared          *shared;

     CoreDFB                  *core;

     int                       fd;     /* framebuffer file descriptor */

     struct fb_fix_screeninfo *fix;    /* fixed screen information (infos about memory and type of card) */

     void                     *addr;   /* framebuffer memory address */
} FBDevData;

/**********************************************************************************************************************/

int fbdev_ioctl( FBDevData *fbdev,
                 int        request,
                 void      *arg,
                 int        arg_size );

#endif

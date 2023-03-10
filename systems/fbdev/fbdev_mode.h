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

#ifndef __FBDEV_MODE_H__
#define __FBDEV_MODE_H__

#include <core/layer_region.h>

#include "fbdev_system.h"

/**********************************************************************************************************************/

DFBResult        fbdev_init_modes ( FBDevData                      *fbdev );

VideoMode       *fbdev_find_mode  ( FBDevData                      *fbdev,
                                    int                             width,
                                    int                             height );

void             fbdev_var_to_mode( const struct fb_var_screeninfo *var,
                                    VideoMode                      *mode );

DFBResult        fbdev_test_mode  ( FBDevData                      *fbdev,
                                    const VideoMode                *mode,
                                    const CoreLayerRegionConfig    *config );

DFBResult        fbdev_set_mode   ( FBDevData                      *fbdev,
                                    const VideoMode                *mode,
                                    const CoreSurface              *surface,
                                    unsigned int                    xoffset,
                                    unsigned int                    yoffset );

#endif

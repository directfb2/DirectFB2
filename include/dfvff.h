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

#ifndef __DFVFF_H__
#define __DFVFF_H__

#include <directfb.h>

typedef struct {
     u8                    magic[5];      /* "DFVFF" magic */

     u8                    major;         /* Major version number */
     u8                    minor;         /* Minor version number */

     u8                    flags;

     u32                   width;
     u32                   height;
     DFBSurfacePixelFormat format;
     DFBSurfaceColorSpace  colorspace;
     u32                   framerate_num;
     u32                   framerate_den;
} DFVFFHeader;

#endif

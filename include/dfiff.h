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

#ifndef __DFIFF_H__
#define __DFIFF_H__

#include <directfb.h>

typedef struct {
     unsigned char         magic[5]; /* "DFIFF" magic */

     unsigned char         major;    /* Major version number */
     unsigned char         minor;    /* Minor version number */

     unsigned char         flags;

     uint32_t              width;
     uint32_t              height;
     DFBSurfacePixelFormat format;
     uint32_t              pitch;
} DFIFFHeader;

#endif

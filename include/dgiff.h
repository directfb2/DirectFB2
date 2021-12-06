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

#ifndef __DGIFF_H__
#define __DGIFF_H__

#include <directfb.h>

typedef struct {
     unsigned char magic[5];  /* "DGIFF" magic */

     unsigned char major;     /* Major version number */
     unsigned char minor;     /* Minor version number */

     unsigned char flags;

     uint32_t      num_faces;

     uint32_t      __pad;
} DGIFFHeader;

typedef struct {
     int32_t                 next_face;     /* byte offset from this to next face */

     int32_t                 size;

     int32_t                 ascender;
     int32_t                 descender;
     int32_t                 height;

     int32_t                 max_advance;

     uint32_t                pixelformat;

     uint32_t                num_glyphs;
     uint32_t                num_rows;

     DFBSurfaceBlittingFlags blittingflags;
} DGIFFFaceHeader;

typedef struct {
     uint32_t unicode;

     uint32_t row;

     int32_t  offset;
     int32_t  width;
     int32_t  height;

     int32_t  left;
     int32_t  top;
     int32_t  advance;
} DGIFFGlyphInfo;

typedef struct {
     int32_t  width;
     int32_t  height;
     int32_t  pitch;

     uint32_t __pad;

     /* Raw pixel data follows, 'height * pitch' bytes. */
} DGIFFGlyphRow;

#endif

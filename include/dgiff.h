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
     u8  magic[5];  /* "DGIFF" magic */

     u8  major;     /* Major version number */
     u8  minor;     /* Minor version number */

     u8  flags;

     u32 num_faces;

     u32 __pad;
} DGIFFHeader;

typedef struct {
     s32                     next_face;     /* byte offset from this to next face */

     s32                     size;

     s32                     ascender;
     s32                     descender;
     s32                     height;

     s32                     max_advance;

     u32                     pixelformat;

     u32                     num_glyphs;
     u32                     num_rows;

     DFBSurfaceBlittingFlags blittingflags;
} DGIFFFaceHeader;

typedef struct {
     u32 unicode;

     u32 row;

     s32 offset;
     s32 width;
     s32 height;

     s32 left;
     s32 top;
     s32 advance;
} DGIFFGlyphInfo;

typedef struct {
     s32 width;
     s32 height;
     s32 pitch;

     u32 __pad;

     /* Raw pixel data follows, 'height * pitch' bytes. */
} DGIFFGlyphRow;

#endif

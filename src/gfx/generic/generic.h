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

#ifndef __GENERIC_H__
#define __GENERIC_H__

#include <core/coretypes.h>

/**********************************************************************************************************************/

typedef void (*GenefxFunc)( GenefxState *gfxs );

typedef union {
     struct {
          u16 b;
          u16 g;
          u16 r;
          u16 a;
     } RGB;
     struct {
          u16 u;
          u16 v;
          u16 y;
          u16 a;
     } YUV;
} GenefxAccumulator;

struct __DFB_GenefxState {
     GenefxFunc               funcs[32];

     int                      length;            /* span length */
     int                      Slen;              /* span length (source) */
     int                      Dlen;              /* span length (destination) */

     void                    *dst_org[3];
     void                    *src_org[3];
     void                    *mask_org[3];

     int                      dst_pitch;
     int                      src_pitch;
     int                      mask_pitch;

     int                      dst_bpp;
     int                      src_bpp;
     int                      mask_bpp;

     DFBSurfaceCapabilities   dst_caps;
     DFBSurfaceCapabilities   src_caps;
     DFBSurfaceCapabilities   mask_caps;

     DFBSurfacePixelFormat    dst_format;
     DFBSurfacePixelFormat    src_format;
     DFBSurfacePixelFormat    mask_format;

     int                      dst_height;
     int                      src_height;
     int                      mask_height;

     int                      dst_field_offset;
     int                      src_field_offset;
     int                      mask_field_offset;

     DFBColor                 color;

     /*
      * operands
      */
     void                    *Aop[3];
     void                    *Bop[3];
     void                    *Mop[3];
     u32                      Cop;

     int                      Astep;
     int                      Bstep;

     u8                       YCop;
     u8                       CbCop;
     u8                       CrCop;

     int                      Aop_field;
     int                      Bop_field;
     int                      Mop_field;

     int                      AopY;
     int                      BopY;
     int                      MopY;

     int                      s;
     int                      t;

     /*
      * color keys
      */
     u32                      Dkey;
     u32                      Skey;

     /*
      * color lookup tables
      */
     CorePalette             *Alut;
     CorePalette             *Blut;

     /*
      * accumulators
      */
     void                    *ABstart;
     int                      ABsize;
     GenefxAccumulator       *Aacc;
     GenefxAccumulator       *Bacc;
     GenefxAccumulator       *Tacc;              /* for simultaneous S+D blending */
     GenefxAccumulator        Cacc;
     GenefxAccumulator        SCacc;

     /*
      * dataflow control
      */
     GenefxAccumulator       *Xacc;              /* writing pointer for blending */
     GenefxAccumulator       *Yacc;              /* input pointer for blending */
     GenefxAccumulator       *Dacc;
     GenefxAccumulator       *Sacc;

     void                   **Sop;
     CorePalette             *Slut;

     int                      Ostep;             /* for horizontal blitting direction */

     int                      SperD;             /* for scaled/texture routines only */
     int                      TperD;             /* for texture routines only */
     int                      Xphase;            /* initial value for fractional steps (zero if not clipped) */

     bool                     need_accumulator;

     int                     *trans;
     int                      num_trans;
};

/**********************************************************************************************************************/

void gGetDriverInfo( GraphicsDriverInfo  *driver_info );

void gGetDeviceInfo( GraphicsDeviceInfo  *device_info );

bool gAcquire      ( CardState           *state,
                     DFBAccelerationMask  accel );

void gRelease      ( CardState           *state );

#endif

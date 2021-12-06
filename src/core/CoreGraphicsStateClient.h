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

#ifndef __CORE__CORE_GRAPHICS_STATE_CLIENT_H__
#define __CORE__CORE_GRAPHICS_STATE_CLIENT_H__

#include <core/coretypes.h>

/**********************************************************************************************************************/

struct __DFB_CoreGraphicsStateClient {
     int                magic;

     void              *priv;

     CoreDFB           *core;
     CardState         *state;     /* Local state structure. */

     CoreGraphicsState *gfx_state; /* Remote object for rendering, syncing values from local state as needed. */
};

/**********************************************************************************************************************/

DFBResult CoreGraphicsStateClient_Init               ( CoreGraphicsStateClient *client,
                                                       CardState               *state );

void      CoreGraphicsStateClient_Deinit             ( CoreGraphicsStateClient *client );

void      CoreGraphicsStateClient_Flush              ( CoreGraphicsStateClient *client );

DFBResult CoreGraphicsStateClient_ReleaseSource      ( CoreGraphicsStateClient *client );

DFBResult CoreGraphicsStateClient_SetColorAndIndex   ( CoreGraphicsStateClient *client,
                                                       const DFBColor          *color,
                                                       u32                      index );

DFBResult CoreGraphicsStateClient_Update             ( CoreGraphicsStateClient *client,
                                                       DFBAccelerationMask      accel,
                                                       CardState               *state );

DFBResult CoreGraphicsStateClient_GetAccelerationMask( CoreGraphicsStateClient *client,
                                                       DFBAccelerationMask     *ret_accel );

DFBResult CoreGraphicsStateClient_FillRectangles     ( CoreGraphicsStateClient *client,
                                                       const DFBRectangle      *rects,
                                                       unsigned int             num );

DFBResult CoreGraphicsStateClient_DrawRectangles     ( CoreGraphicsStateClient *client,
                                                       const DFBRectangle      *rects,
                                                       unsigned int             num );

DFBResult CoreGraphicsStateClient_DrawLines          ( CoreGraphicsStateClient *client,
                                                       const DFBRegion         *lines,
                                                       unsigned int             num );

DFBResult CoreGraphicsStateClient_FillTriangles      ( CoreGraphicsStateClient *client,
                                                       const DFBTriangle       *triangles,
                                                       unsigned int             num );

DFBResult CoreGraphicsStateClient_FillTrapezoids     ( CoreGraphicsStateClient *client,
                                                       const DFBTrapezoid      *trapezoids,
                                                       unsigned int             num );

DFBResult CoreGraphicsStateClient_FillQuadrangles    ( CoreGraphicsStateClient *client,
                                                       const DFBPoint          *points,
                                                       unsigned int             num );

DFBResult CoreGraphicsStateClient_FillSpans          ( CoreGraphicsStateClient *client,
                                                       int                      y,
                                                       const DFBSpan           *spans,
                                                       unsigned int             num );

DFBResult CoreGraphicsStateClient_Blit               ( CoreGraphicsStateClient *client,
                                                       const DFBRectangle      *rects,
                                                       const DFBPoint          *points,
                                                       unsigned int             num );

DFBResult CoreGraphicsStateClient_Blit2              ( CoreGraphicsStateClient *client,
                                                       const DFBRectangle      *rects,
                                                       const DFBPoint          *points1,
                                                       const DFBPoint          *points2,
                                                       unsigned int             num );

DFBResult CoreGraphicsStateClient_StretchBlit        ( CoreGraphicsStateClient *client,
                                                       const DFBRectangle      *srects,
                                                       const DFBRectangle      *drects,
                                                       unsigned int             num );

DFBResult CoreGraphicsStateClient_TileBlit           ( CoreGraphicsStateClient *client,
                                                       const DFBRectangle      *rects,
                                                       const DFBPoint          *points1,
                                                       const DFBPoint          *points2,
                                                       unsigned int             num );

DFBResult CoreGraphicsStateClient_TextureTriangles   ( CoreGraphicsStateClient *client,
                                                       const DFBVertex         *vertices,
                                                       int                      num,
                                                       DFBTriangleFormation     formation );

#endif

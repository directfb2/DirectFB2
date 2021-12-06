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

#ifndef __GFX__CLIP_H__
#define __GFX__CLIP_H__

#include <core/coretypes.h>

/**********************************************************************************************************************/

/*
 * Clip the line to the clipping region.
 * Return true if at least one pixel of the line resides in the region.
 */
DFBBoolean   dfb_clip_line                       ( const DFBRegion         *clip,
                                                   DFBRegion               *line );

/*
 * Clip the rectangle to the clipping region.
 * Return true if there was an intersection with the clipping region.
 */
DFBBoolean   dfb_clip_rectangle                  ( const DFBRegion         *clip,
                                                   DFBRectangle            *rect );

/*
 * Clip the triangle to the clipping region.
 * Return true if the triangle if visible within the region.
 * The vertices of the polygon resulting from intersection are returned in 'buf'. The number of vertices is at least 3.
 */
DFBBoolean   dfb_clip_triangle                   ( const DFBRegion         *clip,
                                                   const DFBTriangle       *tri,
                                                   DFBPoint                 buf[6],
                                                   int                     *num );
/*
 * Get the outlines of a clipped rectangle.
 */
void         dfb_build_clipped_rectangle_outlines( DFBRectangle            *rect,
                                                   const DFBRegion         *clip,
                                                   DFBRectangle            *ret_outlines,
                                                   int                     *ret_num );

/*
 * Clip the blitting request to the clipping region. This includes adjustment of source and destination coordinates.
 */
void         dfb_clip_blit                       ( const DFBRegion         *clip,
                                                   DFBRectangle            *srect,
                                                   int                     *dx,
                                                   int                     *dy );

/*
 * Clip the stretch blit request to the clipping region.
 * This includes adjustment of source and destination coordinates based on the scaling factor.
 */
void         dfb_clip_stretchblit                ( const DFBRegion         *clip,
                                                   DFBRectangle            *srect,
                                                   DFBRectangle            *drect );

/*
 * Clip the blitting request to the clipping region. This includes adjustment of source and destination coordinates.
 * In contrast to dfb_clip_blit() this also honors DSBLIT_ROTATE_ and DSBLIT_FLIP_ blitting flags.
 */
void         dfb_clip_blit_flipped_rotated       ( const DFBRegion         *clip,
                                                   DFBRectangle            *srect,
                                                   DFBRectangle            *drect,
                                                   DFBSurfaceBlittingFlags  flags );

/**********************************************************************************************************************/

/*
 * Check if a clip ot the rectangle is needed.
 */
static __inline__ DFBBoolean
dfb_clip_needed( const DFBRegion *clip,
                 DFBRectangle    *rect )
{
     return ((clip->x1 > rect->x)               || (clip->y1 > rect->y) ||
             (clip->x2 < rect->x + rect->w - 1) || (clip->y2 < rect->y + rect->h - 1)) ? DFB_TRUE : DFB_FALSE;
}

/*
 * Check if requested blitting lies outside of the clipping region.
 * Return true if blitting may need to be performed.
 */
static __inline__ DFBBoolean
dfb_clip_blit_precheck( const DFBRegion *clip,
                        int              w,
                        int              h,
                        int              dx,
                        int              dy )
{
     if (w < 1 || h < 1 || (clip->x1 >= dx + w) || (clip->x2 < dx) || (clip->y1 >= dy + h) || (clip->y2 < dy))
          return DFB_FALSE;

     return DFB_TRUE;
}

#endif

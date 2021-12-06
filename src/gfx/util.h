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

#ifndef __GFX__UTIL_H__
#define __GFX__UTIL_H__

#include <core/coretypes.h>

/**********************************************************************************************************************/

void dfb_gfx_copy_stereo          ( CoreSurface             *source,
                                    DFBSurfaceStereoEye      source_eye,
                                    CoreSurface             *destination,
                                    DFBSurfaceStereoEye      destination_eye,
                                    const DFBRectangle      *rect,
                                    int                      x,
                                    int                      y,
                                    bool                     from_back );

void dfb_gfx_clear                ( CoreSurface             *surface,
                                    DFBSurfaceBufferRole     role );

void dfb_gfx_stretch_stereo       ( CoreSurface             *source,
                                    DFBSurfaceStereoEye      source_eye,
                                    CoreSurface             *destination,
                                    DFBSurfaceStereoEye      destination_eye,
                                    const DFBRectangle      *srect,
                                    const DFBRectangle      *drect,
                                    bool                     from_back );

void dfb_gfx_copy_regions_client  ( CoreSurface             *source,
                                    DFBSurfaceBufferRole     from,
                                    DFBSurfaceStereoEye      source_eye,
                                    CoreSurface             *destination,
                                    DFBSurfaceBufferRole     to,
                                    DFBSurfaceStereoEye      destination_eye,
                                    const DFBRegion         *regions,
                                    unsigned int             num,
                                    int                      x,
                                    int                      y,
                                    CoreGraphicsStateClient *client );

void dfb_back_to_front_copy_stereo( CoreSurface             *surface,
                                    DFBSurfaceStereoEye      eyes,
                                    const DFBRegion         *left_region,
                                    const DFBRegion         *right_region,
                                    int                      rotation );

void dfb_sort_triangle            ( DFBTriangle             *tri );

void dfb_sort_trapezoid           ( DFBTrapezoid            *trap );

/**********************************************************************************************************************/

/*
 * Simplify blitting flags.
 *
 * Allow any combination of DSBLIT_ROTATE_ and DSBLIT_FLIP_ flags to be reduced to a combination of DSBLIT_ROTATE_90,
 * DSBLIT_FLIP_HORIZONTAL and DSBLIT_FLIP_VERTICAL
 */
static __inline__ void
dfb_simplify_blittingflags( DFBSurfaceBlittingFlags *flags )
{
     if (*flags & DSBLIT_ROTATE180)
          *flags = *flags ^ (DSBLIT_ROTATE180 | DSBLIT_FLIP_HORIZONTAL | DSBLIT_FLIP_VERTICAL);

     if (*flags & DSBLIT_ROTATE270) {
          if (*flags & DSBLIT_ROTATE90)
               *flags = *flags ^ (DSBLIT_ROTATE90 | DSBLIT_ROTATE270);
          else
               *flags = *flags ^ (DSBLIT_ROTATE90 | DSBLIT_ROTATE270 | DSBLIT_FLIP_HORIZONTAL | DSBLIT_FLIP_VERTICAL);
     }
}

#endif

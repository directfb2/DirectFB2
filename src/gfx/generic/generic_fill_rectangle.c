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

#include <core/state.h>
#include <gfx/generic/generic.h>
#include <gfx/generic/generic_fill_rectangle.h>
#include <gfx/generic/generic_util.h>

/**********************************************************************************************************************/

void
gFillRectangle( CardState    *state,
                DFBRectangle *rect )
{
     GenefxState *gfxs;
     int          h;

     D_ASSERT( state != NULL );
     D_ASSERT( state->gfxs != NULL );
     D_ASSERT( state->clip.x1 <= rect->x );
     D_ASSERT( state->clip.y1 <= rect->y );
     D_ASSERT( state->clip.x2 >= (rect->x + rect->w - 1) );
     D_ASSERT( state->clip.y2 >= (rect->y + rect->h - 1) );

     gfxs = state->gfxs;

     if (dfb_config->software_warn) {
          D_WARN( "FillRectangle (%4d,%4d-%4dx%4d) %6s, flags 0x%08x, color 0x%02x%02x%02x%02x",
                  DFB_RECTANGLE_VALS( rect ), dfb_pixelformat_name( gfxs->dst_format ), state->drawingflags,
                  state->color.a, state->color.r, state->color.g, state->color.b );
     }

     CHECK_PIPELINE();

     if (!Genefx_ABacc_prepare( gfxs, rect->w ))
          return;

     gfxs->length = rect->w;

     Genefx_Aop_xy( gfxs, rect->x, rect->y );

     h = rect->h;
     while (h--) {
          RUN_PIPELINE();

          Genefx_Aop_next( gfxs );
     }

     Genefx_ABacc_flush( gfxs );
}

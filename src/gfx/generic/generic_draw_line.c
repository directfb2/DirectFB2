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
#include <gfx/generic/generic_draw_line.h>
#include <gfx/generic/generic_fill_rectangle.h>
#include <gfx/generic/generic_util.h>

/**********************************************************************************************************************/

void
gDrawLine( CardState *state,
           DFBRegion *line )
{
     GenefxState *gfxs;
     int          i, dx, dy, sdy, dxabs, dyabs, x, y, px, py;

     D_ASSERT( state != NULL );
     D_ASSERT( state->gfxs != NULL );

     gfxs = state->gfxs;

     CHECK_PIPELINE();

     /* The horizontal distance of the line. */
     dx    = line->x2 - line->x1;
     dxabs = ABS( dx );

     if (!Genefx_ABacc_prepare( gfxs, dxabs ))
          return;

     /* The vertical distance of the line. */
     dy    = line->y2 - line->y1;
     dyabs = ABS( dy );

     /* Draw horizontal/vertical line. */
     if (!dx || !dy) {
          DFBRectangle rect = { MIN( line->x1, line->x2 ), MIN( line->y1, line->y2 ), dxabs + 1, dyabs + 1 };
          gFillRectangle( state, &rect );
          return;
     }

     if (dfb_config->software_warn) {
          D_WARN( "DrawLine (%4d,%4d-%4d,%4d) %6s, flags 0x%08x, color 0x%02x%02x%02x%02x",
                  DFB_RECTANGLE_VALS_FROM_REGION( line ), dfb_pixelformat_name( gfxs->dst_format ), state->drawingflags,
                  state->color.a, state->color.r, state->color.g, state->color.b );
     }

     sdy = SIGN( dy ) * SIGN( dx );
     x   = dyabs >> 1;
     y   = dxabs >> 1;

     if (dx > 0) {
          px = line->x1;
          py = line->y1;
     }
     else {
          px = line->x2;
          py = line->y2;
     }

     /* The line is more horizontal than vertical. */
     if (dxabs >= dyabs) {
          for (i = 0, gfxs->length = 1; i < dxabs; i++, gfxs->length++) {
               y += dyabs;

               if (y >= dxabs) {
                    Genefx_Aop_xy( gfxs, px, py );

                    RUN_PIPELINE();

                    px += gfxs->length;

                    gfxs->length = 0;

                    y -= dxabs;

                    py += sdy;
               }
          }

          Genefx_Aop_xy( gfxs, px, py );

          RUN_PIPELINE();
     }
     /* The line is more vertical than horizontal. */
     else {
          gfxs->length = 1;

          Genefx_Aop_xy( gfxs, px, py );

          RUN_PIPELINE();

          for (i = 0; i < dyabs; i++) {
               x += dxabs;

               if (x >= dyabs) {
                    x -= dyabs;
                    px++;
               }

               py += sdy;

               Genefx_Aop_xy( gfxs, px, py );

               RUN_PIPELINE();
          }
     }

     Genefx_ABacc_flush( gfxs );
}

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
#include <gfx/generic/generic_blit.h>
#include <gfx/generic/generic_util.h>
#include <gfx/util.h>

/**********************************************************************************************************************/

typedef void (*XopAdvanceFunc)( GenefxState *gfxs );

void
gBlit( CardState    *state,
       DFBRectangle *rect,
       int           dx,
       int           dy )
{
     GenefxState             *gfxs;
     XopAdvanceFunc           Aop_advance;
     XopAdvanceFunc           Bop_advance;
     int                      Aop_X;
     int                      Aop_Y;
     int                      Bop_X;
     int                      Bop_Y;
     XopAdvanceFunc           Mop_advance = NULL;
     int                      Mop_X       = 0;
     int                      Mop_Y       = 0;
     int                      h;
     int                      mask_h;
     int                      mask_x;
     int                      mask_y;
     DFBSurfaceBlittingFlags  rotflip_blittingflags;

     D_ASSERT( state != NULL );
     D_ASSERT( state->gfxs != NULL );

     gfxs = state->gfxs;

     rotflip_blittingflags = state->blittingflags;

     dfb_simplify_blittingflags( &rotflip_blittingflags );
     rotflip_blittingflags &= (DSBLIT_FLIP_HORIZONTAL | DSBLIT_FLIP_VERTICAL | DSBLIT_ROTATE90 );

     if (dfb_config->software_warn) {
          D_WARN( "Blit (%4d,%4d-%4dx%4d) %6s, flags 0x%08x, funcs %u/%u, color 0x%02x%02x%02x%02x <- (%4d,%4d) %6s",
                  dx, dy, rect->w, rect->h, dfb_pixelformat_name( gfxs->dst_format ), state->blittingflags,
                  state->src_blend, state->dst_blend, state->color.a, state->color.r, state->color.g, state->color.b,
                  rect->x, rect->y, dfb_pixelformat_name( gfxs->src_format ) );
     }

     D_ASSERT( state->clip.x1 <= dx );
     D_ASSERT( state->clip.y1 <= dy );
     D_ASSERT(  (rotflip_blittingflags & DSBLIT_ROTATE90) || state->clip.x2 >= (dx + rect->w - 1) );
     D_ASSERT(  (rotflip_blittingflags & DSBLIT_ROTATE90) || state->clip.y2 >= (dy + rect->h - 1) );
     D_ASSERT( !(rotflip_blittingflags & DSBLIT_ROTATE90) || state->clip.x2 >= (dx + rect->h - 1) );
     D_ASSERT( !(rotflip_blittingflags & DSBLIT_ROTATE90) || state->clip.y2 >= (dy + rect->w - 1) );

     CHECK_PIPELINE();

     if (!Genefx_ABacc_prepare( gfxs, rect->w ))
          return;

     switch (gfxs->src_format) {
          case DSPF_A4:
          case DSPF_YUY2:
          case DSPF_UYVY:
               rect->x &= ~1;
               break;
          default:
               break;
     }

     switch (gfxs->dst_format) {
          case DSPF_A4:
          case DSPF_YUY2:
          case DSPF_UYVY:
               dx &= ~1;
               break;
          default:
               break;
     }

     gfxs->length = rect->w;

     if (gfxs->src_org[0] == gfxs->dst_org[0] && dy == rect->y && dx > rect->x)
          /* We must blit from right to left. */
          gfxs->Astep = gfxs->Bstep = -1;
     else
          /* We must blit from left to right. */
          gfxs->Astep = gfxs->Bstep = 1;

     mask_x = mask_y = 0;

     if ((state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) &&
         (state->src_mask_flags & DSMF_STENCIL)) {
          mask_x = state->src_mask_offset.x;
          mask_y = state->src_mask_offset.y;
     }

     mask_h = gfxs->mask_height;

     if (rotflip_blittingflags == (DSBLIT_FLIP_HORIZONTAL | DSBLIT_FLIP_VERTICAL)) {
          gfxs->Astep *= -1;

          Aop_X = dx + rect->w - 1;
          Aop_Y = dy;

          Bop_X = rect->x;
          Bop_Y = rect->y + rect->h - 1;

          Aop_advance = Genefx_Aop_next;
          Bop_advance = Genefx_Bop_prev;

          if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) {
               Mop_X = mask_x;
               Mop_Y = mask_y + mask_h - 1;

               Mop_advance = Genefx_Mop_prev;
          }
     }
     else if (rotflip_blittingflags == DSBLIT_FLIP_HORIZONTAL) {
          gfxs->Astep *= -1;

          Aop_X = dx + rect->w - 1;
          Aop_Y = dy;

          Bop_X = rect->x;
          Bop_Y = rect->y;

          Aop_advance = Genefx_Aop_next;
          Bop_advance = Genefx_Bop_next;

          if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) {
               Mop_X = mask_x;
               Mop_Y = mask_y;

               Mop_advance = Genefx_Mop_next;
          }
     }
     else if (rotflip_blittingflags == DSBLIT_FLIP_VERTICAL) {
          Aop_X = dx;
          Aop_Y = dy + rect->h - 1;

          Bop_X = rect->x;
          Bop_Y = rect->y;

          Aop_advance = Genefx_Aop_prev;
          Bop_advance = Genefx_Bop_next;

          if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) {
               Mop_X = mask_x;
               Mop_Y = mask_y;

               Mop_advance = Genefx_Mop_next;
          }
     }
     else if (rotflip_blittingflags == (DSBLIT_ROTATE90 | DSBLIT_FLIP_HORIZONTAL | DSBLIT_FLIP_VERTICAL)) {
          if (gfxs->dst_bpp == 0) {
               D_UNIMPLEMENTED();
               return;
          }

          gfxs->Astep *= gfxs->dst_pitch / gfxs->dst_bpp;

          Aop_X = dx;
          Aop_Y = dy;

          Bop_X = rect->x;
          Bop_Y = rect->y + rect->h - 1;

          Aop_advance = Genefx_Aop_crab;
          Bop_advance = Genefx_Bop_prev;

          if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) {
               Mop_X = mask_x;
               Mop_Y = mask_y + mask_h - 1;

               Mop_advance = Genefx_Mop_prev;
          }
     }
     else if (rotflip_blittingflags == DSBLIT_ROTATE90) {
          if (gfxs->dst_bpp == 0) {
               D_UNIMPLEMENTED();
               return;
          }

          gfxs->Astep *= -gfxs->dst_pitch / gfxs->dst_bpp;

          Aop_X = dx;
          Aop_Y = dy + rect->w - 1;

          Bop_X = rect->x;
          Bop_Y = rect->y;

          Aop_advance = Genefx_Aop_crab;
          Bop_advance = Genefx_Bop_next;

          if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) {
               Mop_X = mask_x;
               Mop_Y = mask_y;

               Mop_advance = Genefx_Mop_next;
          }
     }
     else if (rotflip_blittingflags == (DSBLIT_ROTATE90 | DSBLIT_FLIP_VERTICAL)) {
          if (gfxs->dst_bpp == 0) {
               D_UNIMPLEMENTED();
               return;
          }

          gfxs->Astep *= -gfxs->dst_pitch / gfxs->dst_bpp;

          Aop_X = dx + rect->h - 1;
          Aop_Y = dy + rect->w - 1;

          Bop_X = rect->x;
          Bop_Y = rect->y;

          Aop_advance = Genefx_Aop_prev_crab;
          Bop_advance = Genefx_Bop_next;

          if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) {
               Mop_X = mask_x;
               Mop_Y = mask_y;

               Mop_advance = Genefx_Mop_next;
          }
     }
     else if (rotflip_blittingflags == (DSBLIT_ROTATE90 | DSBLIT_FLIP_HORIZONTAL)) {
          if (gfxs->dst_bpp == 0) {
               D_UNIMPLEMENTED();
               return;
          }

          gfxs->Astep *= gfxs->dst_pitch / gfxs->dst_bpp;

          Aop_X = dx;
          Aop_Y = dy;

          Bop_X = rect->x;
          Bop_Y = rect->y;

          Aop_advance = Genefx_Aop_crab;
          Bop_advance = Genefx_Bop_next;

          if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) {
               Mop_X = mask_x;
               Mop_Y = mask_y;

               Mop_advance = Genefx_Mop_next;
          }
     }
     else if (gfxs->src_org[0] == gfxs->dst_org[0] && dy > rect->y && !(state->blittingflags & DSBLIT_DEINTERLACE)) {
          /* We must blit from bottom to top. */
          Aop_X = dx;
          Aop_Y = dy + rect->h - 1;

          Bop_X = rect->x;
          Bop_Y = rect->y + rect->h - 1;

          Aop_advance = Genefx_Aop_prev;
          Bop_advance = Genefx_Bop_prev;

          if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) {
               Mop_X = mask_x;
               Mop_Y = mask_y + mask_h - 1;

               Mop_advance = Genefx_Mop_prev;
          }
     }
     else {
          /* We must blit from top to bottom. */
          Aop_X = dx;
          Aop_Y = dy;

          Bop_X = rect->x;
          Bop_Y = rect->y;

          Aop_advance = Genefx_Aop_next;
          Bop_advance = Genefx_Bop_next;

          if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) {
               Mop_X = mask_x;
               Mop_Y = mask_y;

               Mop_advance = Genefx_Mop_next;
          }
     }

     Genefx_Aop_xy( gfxs, Aop_X, Aop_Y );
     Genefx_Bop_xy( gfxs, Bop_X, Bop_Y );

     if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR))
          Genefx_Mop_xy( gfxs, Mop_X, Mop_Y );

     if (state->blittingflags & DSBLIT_DEINTERLACE) {
          if (state->source->field) {
               Aop_advance( gfxs );
               Bop_advance( gfxs );

               if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR))
                    Mop_advance( gfxs );

               rect->h--;
          }

          for (h = rect->h / 2; h; h--) {
               RUN_PIPELINE();

               Aop_advance( gfxs );

               RUN_PIPELINE();

               Aop_advance( gfxs );

               Bop_advance( gfxs );
               Bop_advance( gfxs );

               if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) {
                    Mop_advance( gfxs );
                    Mop_advance( gfxs );
               }
          }
     }
     else {
          for (h = rect->h; h; h--) {
               RUN_PIPELINE();

               Aop_advance( gfxs );
               Bop_advance( gfxs );

               if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR))
                    Mop_advance( gfxs );
          }
     }

     Genefx_ABacc_flush( gfxs );
}

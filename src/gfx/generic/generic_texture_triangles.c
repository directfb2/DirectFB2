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
#include <gfx/generic/generic_texture_triangles.h>
#include <gfx/generic/generic_util.h>

/**********************************************************************************************************************/

typedef struct {
   int xi;
   int xf;
   int mi;
   int mf;
   int _2dy;
} DDA;

#define SETUP_DDA(xs,ys,xe,ye,dda)           \
     do {                                    \
          int dx = (xe) - (xs);              \
          int dy = (ye) - (ys);              \
          dda.xi = (xs);                     \
          if (dy != 0) {                     \
               dda.mi = dx / dy;             \
               dda.mf = 2 * (dx % dy);       \
               dda.xf = -dy;                 \
               dda._2dy = 2 * dy;            \
               if (dda.mf < 0) {             \
                    dda.mf += 2 * ABS( dy ); \
                    dda.mi--;                \
               }                             \
          }                                  \
          else {                             \
               dda.mi = 0;                   \
               dda.mf = 0;                   \
               dda.xf = 0;                   \
               dda._2dy = 0;                 \
          }                                  \
     } while (0)

#define INC_DDA(dda)                         \
     do {                                    \
          dda.xi += dda.mi;                  \
          dda.xf += dda.mf;                  \
          if (dda.xf > 0) {                  \
               dda.xi++;                     \
               dda.xf -= dda._2dy;           \
          }                                  \
     } while (0)

static void
Genefx_TextureTriangleAffine( GenefxState        *gfxs,
                              GenefxVertexAffine *v0,
                              GenefxVertexAffine *v1,
                              GenefxVertexAffine *v2,
                              const DFBRegion    *clip )
{
     /* All points on one horizontal line. */
     if (v0->y == v1->y && v1->y == v2->y)
          return;

     GenefxVertexAffine *v_tmp;

     /* Triangle sorting (vertical). */
     if (v1->y < v0->y) {
          v_tmp = v0;
          v0    = v1;
          v1    = v_tmp;
     }
     if (v2->y < v0->y) {
          v_tmp = v2;
          v2 = v1;
          v1 = v0;
          v0 = v_tmp;
     }
     else if (v2->y < v1->y) {
          v_tmp = v1;
          v1    = v2;
          v2    = v_tmp;
     }

     int y_top    = v0->y;
     int y_bottom = v2->y;

     /* Totally clipped (vertical). */
     if (y_top > clip->y2 || y_bottom < clip->y1)
          return;

     /* Totally clipped right. */
     if (v0->x > clip->x2 && v1->x > clip->x2 && v2->x > clip->x2)
          return;

     /* Totally clipped left. */
     if (v0->x < clip->x1 && v1->x < clip->x1 && v2->x < clip->x1)
          return;

     int y_update = -1;

     /* Triangle Sorting (horizontal). */
     if (v0->y == v1->y) {
          if (v0->x > v1->x) {
               v_tmp = v0;
               v0    = v1;
               v1    = v_tmp;
          }
     }
     else if (v1->y == v2->y) {
          if (v1->x > v2->x) {
               v_tmp = v1;
               v1    = v2;
               v2    = v_tmp;
          }
     }
     else
          y_update = v1->y;

     /* Triangle Setup. */

     int height      = v2->y - v0->y;
     int half_top    = v1->y - v0->y;
     int half_bottom = v2->y - v1->y;
     int y;
     int sl, sr;
     int tl, tr;
     int dsl, dsr;
     int dtl, dtr;
     int dsl2, dsr2;
     int dtl2, dtr2;

     /* Flat top. */
     if (v0->y == v1->y) {
          /* Top points equal. */
          if (v0->x == v1->x)
               return;

          sl = v0->s;
          sr = v1->s;

          dsl = dsl2 = (v2->s - sl) / height;
          dsr = dsr2 = (v2->s - sr) / height;

          tl = v0->t;
          tr = v1->t;

          dtl = dtl2 = (v2->t - tl) / height;
          dtr = dtr2 = (v2->t - tr) / height;
     }
     /* Flat bottom. */
     else if (v1->y == v2->y) {
          /* Bottom points equal. */
          if (v1->x == v2->x)
               return;

          sl = sr = v0->s;

          dsl = dsl2 = (v1->s - sl) / height;
          dsr = dsr2 = (v2->s - sr) / height;

          tl = tr = v0->t;

          dtl = dtl2 = (v1->t - tl) / height;
          dtr = dtr2 = (v2->t - tr) / height;
     }
     /* Two parts. */
     else {
          sl = sr = v0->s;
          tl = tr = v0->t;

          int x_v1 = v0->x + (v2->x - v0->x) * (v1->y - v0->y) / height;

          /* Update left. */
          if (x_v1 > v1->x) {
               dsl = (v1->s - sl) / half_top;
               dsr = (v2->s - sr) / height;

               dsl2 = (v2->s - v1->s) / half_bottom;
               dsr2 = dsr;

               dtl = (v1->t - tl) / half_top;
               dtr = (v2->t - tr) / height;

               dtl2 = (v2->t - v1->t) / half_bottom;
               dtr2 = dtr;
          }
          /* Update right. */
          else if (x_v1 < v1->x) {
               dsl = (v2->s - sl) / height;
               dsr = (v1->s - sr) / half_top;

               dsl2 = dsl;
               dsr2 = (v2->s - v1->s) / half_bottom;

               dtl = (v2->t - tl) / height;
               dtr = (v1->t - tr) / half_top;

               dtl2 = dtl;
               dtr2 = (v2->t - v1->t) / half_bottom;
          }
          /* All points on one line. */
          else
               return;
     }

     DDA dda1 = { .xi = 0 }, dda2 = { .xi = 0 };

     SETUP_DDA( v0->x, v0->y, v2->x, v2->y, dda1 );
     SETUP_DDA( v0->x, v0->y, v1->x, v1->y, dda2 );

     /* Vertical clipping. */

     if (y_top < clip->y1) {
          y_top = clip->y1;
     }

     if (y_bottom > clip->y2) {
          y_bottom = clip->y2;
     }

     if (y_top > v0->y) {
          for (y = v0->y; y < y_top; y++) {
               if (y == v1->y)
                    SETUP_DDA( v1->x, v1->y, v2->x, v2->y, dda2 );

               INC_DDA( dda1 );
               INC_DDA( dda2 );
          }

          /* Two parts. */
          if (y_update != -1 && y_top > y_update) {
               sl += dsl * (y_update - v0->y) + dsl2 * (y_top - v1->y);
               sr += dsr * (y_update - v0->y) + dsr2 * (y_top - v1->y);

               tl += dtl * (y_update - v0->y) + dtl2 * (y_top - v1->y);
               tr += dtr * (y_update - v0->y) + dtr2 * (y_top - v1->y);

               dsl = dsl2;
               dsr = dsr2;

               dtl = dtl2;
               dtr = dtr2;
          }
          /* One part or only top clipped. */
          else {
               sl += dsl * (y_top - v0->y);
               sr += dsr * (y_top - v0->y);

               tl += dtl * (y_top - v0->y);
               tr += dtr * (y_top - v0->y);
          }
     }

     /* Loop over clipped lines. */
     for (y = y_top; y <= y_bottom; y++) {
          /* Slope update (for bottom half). */
          if (y == y_update) {
               dsl = dsl2;
               dtl = dtl2;

               dsr = dsr2;
               dtr = dtr2;
          }

          if (y == v1->y)
               SETUP_DDA( v1->x, v1->y, v2->x, v2->y, dda2 );

          int len = ABS( dda1.xi - dda2.xi );
          int x1  = MIN( dda1.xi, dda2.xi );
          int x2  = x1 + len - 1;

          if (len > 0 && x1 <= clip->x2 && x2 >= clip->x1) {
               int csl   = sl;
               int ctl   = tl;
               int SperD = (sr - sl) / len;
               int TperD = (tr - tl) / len;

               /* Horizontal clipping. */

               if (x1 < clip->x1) {
                    csl += SperD * (clip->x1 - x1);
                    ctl += TperD * (clip->x1 - x1);

                    x1 = clip->x1;
               }

               if (x2 > clip->x2) {
                    x2 = clip->x2;
               }

               gfxs->Dlen   = x2 - x1 + 1;
               gfxs->length = gfxs->Dlen;
               gfxs->SperD  = SperD;
               gfxs->TperD  = TperD;
               gfxs->s      = csl;
               gfxs->t      = ctl;

               Genefx_Aop_xy( gfxs, x1, y );

               RUN_PIPELINE();
          }

          sl += dsl;
          sr += dsr;

          tl += dtl;
          tr += dtr;

          INC_DDA( dda1 );
          INC_DDA( dda2 );
     }
}

void
Genefx_TextureTrianglesAffine( CardState            *state,
                               GenefxVertexAffine   *vertices,
                               int                   num,
                               DFBTriangleFormation  formation,
                               const DFBRegion      *clip )
{
     GenefxState *gfxs;
     int          index = 0;

     D_ASSERT( state != NULL );
     D_ASSERT( state->gfxs != NULL );

     gfxs = state->gfxs;

     CHECK_PIPELINE();

     if (!Genefx_ABacc_prepare( gfxs, state->destination->config.size.w ))
          return;

     /* Reset Bop to 0,0 as texture lookup accesses the whole buffer arbitrarily. */
     Genefx_Bop_xy( gfxs, 0, 0 );

     /* Render triangles. */
     for (index = 0; index < num;) {
          GenefxVertexAffine *v[3];

          if (index == 0) {
               v[0] = &vertices[index+0];
               v[1] = &vertices[index+1];
               v[2] = &vertices[index+2];

               index += 3;
          }
          else {
               switch (formation) {
                    case DTTF_LIST:
                         v[0] = &vertices[index+0];
                         v[1] = &vertices[index+1];
                         v[2] = &vertices[index+2];

                         index += 3;
                         break;

                    case DTTF_STRIP:
                         v[0] = &vertices[index-2];
                         v[1] = &vertices[index-1];
                         v[2] = &vertices[index+0];

                         index += 1;
                         break;

                    case DTTF_FAN:
                         v[0] = &vertices[0];
                         v[1] = &vertices[index-1];
                         v[2] = &vertices[index+0];

                         index += 1;
                         break;

                    default:
                         D_BUG( "unknown formation %u", formation );
                         Genefx_ABacc_flush( gfxs );
                         return;
               }
          }

          if (dfb_config->software_warn) {
               D_WARN( "TexTriangles (%d,%d-%d,%d-%d,%d) %6s, flags 0x%08x, color 0x%02x%02x%02x%02x <- (%4d,%4d) %6s",
                       v[0]->x, v[0]->y, v[1]->x, v[1]->y, v[2]->x, v[2]->y, dfb_pixelformat_name( gfxs->dst_format ),
                       state->blittingflags, state->color.a, state->color.r, state->color.g, state->color.b,
                       state->source->config.size.w, state->source->config.size.h,
                       dfb_pixelformat_name( gfxs->src_format ) );
          }

          Genefx_TextureTriangleAffine( gfxs, v[0], v[1], v[2], clip );
     }

     Genefx_ABacc_flush( gfxs );
}

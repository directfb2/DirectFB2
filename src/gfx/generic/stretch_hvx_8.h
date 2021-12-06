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

#define SOURCE_LOOKUP(x) (x)
#define SOURCE_TYPE u8

#if D_DEBUG_ENABLED
#define HVX_DEBUG(x...) direct_log_printf( NULL, x )
#else
#define HVX_DEBUG(x...) do {} while (0)
#endif

/* static void
   stretch_hvx_8_up/down */ ( void       *dst,
                              int         dpitch,
                              const void *src,
                              int         spitch,
                              int         width,
                              int         height,
                              int         dst_width,
                              int         dst_height,
                              DFBRegion  *clip )
{
     long  x, y;
     long  cw     = clip->x2 - clip->x1 + 1;
     long  ch     = clip->y2 - clip->y1 + 1;
     long  hfraq  = ((width  - MINUS_1) << 18) / dst_width;
     long  vfraq  = ((height - MINUS_1) << 18) / dst_height;
     long  point0 = POINT_0 + clip->x1 * hfraq;
     long  point  = point0;
     long  line   = LINE_0 + clip->y1 * vfraq;
     long  ratios[cw];
     u8   *dst8;
     u8    _lbT[cw+32];
     u8    _lbB[cw+32];
     u8   *lbX;
     u8   *lbT    = (u8*) ((((unsigned long) (&_lbT[0])) + 31) & ~31);
     u8   *lbB    = (u8*) ((((unsigned long) (&_lbB[0])) + 31) & ~31);
     long  lineT  = -2000;

     for (x = 0; x < cw; x++) {
          ratios[x] = POINT_TO_RATIO( point, hfraq );

          point += hfraq;
     }

     HVX_DEBUG( "%dx%d -> %dx%d  (0x%lx, 0x%lx)\n", width, height, dst_width, dst_height,
                (unsigned long) hfraq, (unsigned long) vfraq );

     dst += clip->x1 + clip->y1 * dpitch;

     dst8 = dst;

     /* Scale line by line.  */
     for (y = 0; y < ch; y++) {
          long X;
          long nlT = LINE_T( line, vfraq );

          D_ASSERT( nlT >= 0 );
          D_ASSERT( nlT < height - 1 );

          /* Fill line buffer(s). */
          if (nlT != lineT) {
               u8 L, R;
               const SOURCE_TYPE *srcT = src + spitch * nlT;
               const SOURCE_TYPE *srcB = src + spitch * (nlT + 1);
               long               diff = nlT - lineT;

               if (diff > 1) {
                    for (x = 0, point = point0; x < cw; x++, point += hfraq) {
                         /* Horizontal interpolation. */
                         long pl = POINT_L( point, hfraq );

                         HVX_DEBUG( "%ld,%ld %ld  (%ld/%ld)  0x%lx  0x%lx\n",
                                    x, y, pl, POINT_L( point, hfraq ), POINT_R( point, hfraq ),
                                    (unsigned long) point, (unsigned long) ratios[x] );

                         D_ASSERT( pl >= 0 );
                         D_ASSERT( pl < width - 1 );

                         L = SOURCE_LOOKUP( srcT[pl] );
                         R = SOURCE_LOOKUP( srcT[pl+1] );

                         lbT[x] = (((R - L) * ratios[x]) >> 8) + L;

                         L = SOURCE_LOOKUP( srcB[pl] );
                         R = SOURCE_LOOKUP( srcB[pl+1] );

                         lbB[x] = (((R - L) * ratios[x]) >> 8) + L;
                    }
               }
               else {
                    /* Swap. */
                    lbX = lbT;
                    lbT = lbB;
                    lbB = lbX;

                    for (x = 0, point = point0; x < cw; x++, point += hfraq) {
                         /* Horizontal interpolation. */
                         long pl = POINT_L( point, hfraq );

                         HVX_DEBUG( "%ld,%ld %ld  (%ld/%ld)  0x%lx  0x%lx\n",
                                    x, y, pl, POINT_L( point, hfraq ), POINT_R( point, hfraq ),
                                    (unsigned long) point, (unsigned long) ratios[x] );

                         D_ASSERT( pl >= 0 );
                         D_ASSERT( pl < width - 1 );

                         L = SOURCE_LOOKUP( srcB[pl] );
                         R = SOURCE_LOOKUP( srcB[pl+1] );

                         lbB[x] = (((R - L) * ratios[x]) >> 8) + L;
                    }
               }

               lineT = nlT;
          }

          /* Vertical interpolation. */
          X = LINE_TO_RATIO( line, vfraq );

          for (x = 0; x < cw; x++)
               dst8[x] = (((lbB[x] - lbT[x]) * X) >> 8) + lbT[x];

          dst8 += dpitch;
          line += vfraq;
     }
}

#undef SOURCE_LOOKUP
#undef SOURCE_TYPE

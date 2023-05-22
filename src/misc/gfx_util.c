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

#include <config.h>
#include <core/palette.h>
#include <core/surface.h>
#include <direct/memcpy.h>
#include <gfx/clip.h>
#include <gfx/convert.h>
#include <misc/gfx_util.h>

/**********************************************************************************************************************/

#define SUBSAMPLE_BITS 4
#define SUBSAMPLE       (1 << SUBSAMPLE_BITS)
#define SUBSAMPLE_MASK ((1 << SUBSAMPLE_BITS) - 1)

#define SCALE_SHIFT 16

#define LINE_PTR(dst,caps,y,h,pitch)                                                                      \
     ((caps & DSCAPS_SEPARATED) ? ((u8*) (dst) + (y) / 2 * (pitch) + ((y) % 2 ? (h) / 2 * (pitch) : 0)) : \
                                  ((u8*) (dst) + (y) * (pitch)))

typedef struct {
     int   *weights;
     int    n_x;
     int    n_y;
     float  x_offset;
     float  y_offset;
} PixopsFilter;

/**********************************************************************************************************************/

static void
write_argb_span( u32         *src,
                 u8          *dst[],
                 int          len,
                 int          x,
                 int          y,
                 CoreSurface *dst_surface )
{
     int          i, j;
     u32          a, y0, u0, v0, y1, u1, v1;
     u8          *d       = dst[0];
     u8          *d1      = dst[1];
     u8          *d2      = dst[2];
     CorePalette *palette = dst_surface->palette;

     if (dst_surface->config.caps & DSCAPS_PREMULTIPLIED) {
          for (i = 0; i < len; i++) {
               u32 s = src[i];

               a = (s >> 24) + 1;

               src[i] = ((((s & 0x00ff00ff) * a) >> 8) & 0x00ff00ff) |
                        ((((s & 0x0000ff00) * a) >> 8) & 0x0000ff00) |
                        (   s & 0xff000000                         );
          }
     }

#define RGB_TO_YCBCR(r,g,b,y,cb,cr)                          \
     if (dst_surface->config.colorspace == DSCS_BT601)       \
          RGB_TO_YCBCR_BT601(r,g,b,y,cb,cr);                 \
     else if (dst_surface->config.colorspace == DSCS_BT709)  \
          RGB_TO_YCBCR_BT709(r,g,b,y,cb,cr);                 \
     else if (dst_surface->config.colorspace == DSCS_BT2020) \
          RGB_TO_YCBCR_BT2020(r,g,b,y,cb,cr);                \
     else {                                                  \
          y = 16;                                            \
          cb = cr = 128;                                     \
     }

     switch (dst_surface->config.format) {
          case DSPF_A1:
               for (i = 0; i < len; i++) {
                    if (i & 7)
                         d[i>>3] |= (src[i] >> 31) << (7 - (i & 7));
                    else
                         d[i>>3]  = (src[i] >> 24) & 0x80;
               }
               break;

          case DSPF_A1_LSB:
               for (i = 0; i < len; i++) {
                    if (i & 7)
                         d[i>>3] |= (src[i] >> 31) << (i & 7);
                    else
                         d[i>>3]  = (src[i] >> 31);
               }
               break;

          case DSPF_A4:
               for (i = 0, j = 0; i < len; i += 2, j++)
                    d[j] = ((src[i] >> 24) & 0xf0) | (src[i+1] >> 28);
               break;

          case DSPF_A8:
               for (i = 0; i < len; i++)
                    d[i] = src[i] >> 24;
               break;

          case DSPF_RGB332:
               for (i = 0; i < len; i++)
                    d[i] = ARGB_TO_RGB332( src[i] );
               break;

          case DSPF_ARGB1555:
               for (i = 0; i < len; i++)
                    ((u16*) d)[i] = ARGB_TO_ARGB1555( src[i] );
               break;

          case DSPF_ARGB2554:
               for (i = 0; i < len; i++)
                    ((u16*) d)[i] = ARGB_TO_ARGB2554( src[i] );
               break;

          case DSPF_ARGB4444:
               for (i = 0; i < len; i++)
                    ((u16*) d)[i] = ARGB_TO_ARGB4444( src[i] );
               break;

          case DSPF_RGBA4444:
               for (i = 0; i < len; i++)
                    ((u16*) d)[i] = ARGB_TO_RGBA4444( src[i] );
               break;

          case DSPF_ARGB8565:
               for (i = 0, j = -1; i < len; i++) {
                    u32 pixel = ARGB_TO_ARGB8565 ( src[i] );
#ifdef WORDS_BIGENDIAN
                    d[++j] = (pixel >> 16) & 0xff;
                    d[++j] = (pixel >>  8) & 0xff;
                    d[++j] =  pixel        & 0xff;
#else
                    d[++j] =  pixel        & 0xff;
                    d[++j] = (pixel >>  8) & 0xff;
                    d[++j] = (pixel >> 16) & 0xff;
#endif
               }
               break;

          case DSPF_RGB16:
               for (i = 0; i < len; i++)
                    ((u16*) d)[i] = ARGB_TO_RGB16( src[i] );
               break;

          case DSPF_ARGB1666:
               for (i = 0; i < len; i++) {
                    u32 pixel = PIXEL_ARGB1666( src[i] >> 24, src[i] >> 16, src[i] >> 8, src[i] );
                    *d++ = pixel;
                    *d++ = pixel >>  8;
                    *d++ = pixel >> 16;
               }
               break;

          case DSPF_ARGB6666:
               for (i = 0; i < len; i++) {
                    u32 pixel = PIXEL_ARGB6666( src[i] >> 24, src[i] >> 16, src[i] >> 8, src[i] );
                    *d++ = pixel;
                    *d++ = pixel >>  8;
                    *d++ = pixel >> 16;
               }
               break;

          case DSPF_RGB18:
               for (i = 0; i < len; i++) {
                    u32 pixel = PIXEL_RGB18( src[i] >> 16, src[i] >> 8, src[i] );
                    *d++ = pixel;
                    *d++ = pixel >>  8;
                    *d++ = pixel >> 16;
               }
               break;

          case DSPF_RGB24:
               for (i = 0; i < len; i++) {
#ifdef WORDS_BIGENDIAN
                    *d++ = src[i] >> 16;
                    *d++ = src[i] >>  8;
                    *d++ = src[i];
#else
                    *d++ = src[i];
                    *d++ = src[i] >>  8;
                    *d++ = src[i] >> 16;
#endif
               }
               break;

          case DSPF_BGR24:
               for (i = 0; i < len; i++) {
#ifdef WORDS_BIGENDIAN
                    *d++ = src[i];
                    *d++ = src[i] >>  8;
                    *d++ = src[i] >> 16;
#else
                    *d++ = src[i] >> 16;
                    *d++ = src[i] >>  8;
                    *d++ = src[i];
#endif
               }
               break;

          case DSPF_RGB32:
          case DSPF_ARGB:
               direct_memcpy( d, src, len * 4 );
               break;

          case DSPF_ABGR:
               for (i = 0; i < len; i++)
                    ((u32*) d)[i] = ARGB_TO_ABGR( src[i] );
               break;

          case DSPF_AiRGB:
               for (i = 0; i < len; i++)
                    ((u32*) d)[i] = src[i] ^ 0xff000000;
               break;

          case DSPF_LUT8:
               if (palette) {
                    for (i = 0; i < len; i++) {
                         d[i] = dfb_palette_search( palette, (src[i] >> 16) & 0xff,
                                                             (src[i] >>  8) & 0xff,
                                                              src[i]        & 0xff,
                                                             (src[i] >> 24) & 0xff );
                    }
               }
               break;

          case DSPF_ALUT44:
               if (palette) {
                    for (i = 0; i < len; i++) {
                         d[i] = ((src[i] >> 24) & 0xf0) + dfb_palette_search( palette, (src[i] >> 16) & 0xff,
                                                                                       (src[i] >>  8) & 0xff,
                                                                                        src[i]        & 0xff,
                                                                                       0x80 );
                    }
               }
               break;

          case DSPF_YUY2:
               if (x & 1) {
                    RGB_TO_YCBCR( (src[0] >> 16) & 0xff, (src[0] >> 8) & 0xff, src[0] & 0xff, y0, u0, v0 );
                    *((u16*) d) = y0 | (v0 << 8);
                    d += 2;
                    src++;
                    len--;
               }
               for (i = 0; i < (len - 1); i += 2) {
                    RGB_TO_YCBCR( (src[i]   >> 16) & 0xff, (src[i]   >> 8) & 0xff, src[i]   & 0xff, y0, u0, v0 );
                    RGB_TO_YCBCR( (src[i+1] >> 16) & 0xff, (src[i+1] >> 8) & 0xff, src[i+1] & 0xff, y1, u1, v1 );
                    u0 = (u0 + u1) >> 1;
                    v1 = (v0 + v1) >> 1;
                    ((u16*) d)[i]   = y0 | (u0 << 8);
                    ((u16*) d)[i+1] = y1 | (v1 << 8);
               }
               if (len & 1) {
                    src += len - 1;
                    d   += (len - 1) * 2;
                    RGB_TO_YCBCR( (*src >> 16) & 0xff, (*src >> 8) & 0xff, *src & 0xff, y0, u0, v0 );
                    *((u16*) d) = y0 | (u0 << 8);
               }
               break;

          case DSPF_UYVY:
               if (x & 1) {
                    RGB_TO_YCBCR( (src[0] >> 16) & 0xff, (src[0] >> 8) & 0xff, src[0] & 0xff, y0, u0, v0 );
                    *((u16*) d) = v0 | (y0 << 8);
                    d += 2;
                    src++;
                    len--;
               }
               for (i = 0; i < (len - 1); i += 2) {
                    RGB_TO_YCBCR( (src[i]   >> 16) & 0xff, (src[i]   >> 8) & 0xff, src[i]   & 0xff, y0, u0, v0 );
                    RGB_TO_YCBCR( (src[i+1] >> 16) & 0xff, (src[i+1] >> 8) & 0xff, src[i+1] & 0xff, y1, u1, v1 );
                    u0 = (u0 + u1) >> 1;
                    v1 = (v0 + v1) >> 1;
                    ((u16*) d)[i]   = u0 | (y0 << 8);
                    ((u16*) d)[i+1] = v1 | (y1 << 8);
               }
               if (len & 1) {
                    src += len - 1;
                    d   += (len - 1) * 2;
                    RGB_TO_YCBCR( (*src >> 16) & 0xff, (*src >> 8) & 0xff, *src & 0xff, y0, u0, v0 );
                    *((u16*) d) = u0 | (y0 << 8);
               }
               break;

          case DSPF_AYUV:
               for (i = 0; i < len; i++) {
                    RGB_TO_YCBCR( (src[i] >> 16) & 0xff, (src[i] >> 8) & 0xff, src[i] & 0xff, y0, u0, v0 );
                    a = (src[i] >> 24) & 0xff;
                    ((u32*) d)[i] = PIXEL_AYUV( a, y0, u0, v0 );
               }
               break;

          case DSPF_AVYU:
               for (i = 0; i < len; i++) {
                    RGB_TO_YCBCR( (src[i] >> 16) & 0xff, (src[i] >> 8) & 0xff, src[i] & 0xff, y0, u0, v0 );
                    a = (src[i] >> 24) & 0xff;
                    ((u32*) d)[i] = PIXEL_AVYU( a, y0, u0, v0 );
               }
               break;

          case DSPF_VYU:
               for (i = 0, j = -1; i < len; i++) {
                    RGB_TO_YCBCR( (src[i] >> 16) & 0xff, (src[i] >> 8) & 0xff, src[i] & 0xff, y0, u0, v0 );
#ifdef WORDS_BIGENDIAN
                    d[++j] = v0;
                    d[++j] = y0;
                    d[++j] = u0;
#else
                    d[++j] = u0;
                    d[++j] = y0;
                    d[++j] = v0;
#endif
               }
               break;

          case DSPF_I420:
          case DSPF_YV12:
          case DSPF_Y42B:
          case DSPF_YV16:
               for (i = 0; i < (len - 1); i += 2) {
                    RGB_TO_YCBCR( (src[i]   >> 16) & 0xff, (src[i]   >> 8) & 0xff, src[i]   & 0xff, y0, u0, v0 );
                    RGB_TO_YCBCR( (src[i+1] >> 16) & 0xff, (src[i+1] >> 8) & 0xff, src[i+1] & 0xff, y1, u1, v1 );
                    d[i]   = y0;
                    d[i+1] = y1;
                    if (y & 1) {
                         d1[i>>1] = (u0 + u1) >> 1;
                         d2[i>>1] = (v0 + v1) >> 1;
                    }
               }
               if (len & 1) {
                    i = len - 1;
                    RGB_TO_YCBCR( (src[i] >> 16) & 0xff, (src[i] >> 8) & 0xff, src[i] & 0xff, y0, u0, v0 );
                    d[i] = y0;
                    if (y & 1) {
                         d1[i>>1] = u0;
                         d2[i>>1] = v0;
                    }
               }
               break;

          case DSPF_Y444:
          case DSPF_YV24: {
               for (i = 0; i < len; i++)
                    RGB_TO_YCBCR( (src[i] >> 16) & 0xff, (src[i] >> 8) & 0xff, src[i] & 0xff, d[i], d1[i], d2[i] );
               break;
          }

          case DSPF_NV12:
          case DSPF_NV16:
               for (i = 0; i < (len - 1); i += 2) {
                    RGB_TO_YCBCR( (src[i]   >> 16) & 0xff, (src[i]   >> 8) & 0xff, src[i]   & 0xff, y0, u0, v0 );
                    RGB_TO_YCBCR( (src[i+1] >> 16) & 0xff, (src[i+1] >> 8) & 0xff, src[i+1] & 0xff, y1, u1, v1 );
                    d[i]   = y0;
                    d[i+1] = y1;
                    if (dst_surface->config.format == DSPF_NV16 || y & 1) {
#ifdef WORDS_BIGENDIAN
                         ((u16*) d1)[i>>1] = ((v0 + v1) >> 1) | (((u0 + u1) >> 1) << 8);
#else
                         ((u16*) d1)[i>>1] = ((u0 + u1) >> 1) | (((v0 + v1) >> 1) << 8);
#endif
                    }
               }
               if (len & 1) {
                    i = len - 1;
                    RGB_TO_YCBCR( (src[i] >> 16) & 0xff, (src[i] >> 8) & 0xff, src[i] & 0xff, y0, u0, v0 );
                    d[i] = y0;
                    if (dst_surface->config.format == DSPF_NV16 || y & 1)
#ifdef WORDS_BIGENDIAN
                         ((u16*) d1)[i>>1] = v0 | (u0 << 8);
#else
                         ((u16*) d1)[i>>1] = u0 | (v0 << 8);
#endif
               }
               break;

          case DSPF_NV21:
          case DSPF_NV61:
               for (i = 0; i < (len - 1); i += 2) {
                    RGB_TO_YCBCR( (src[i]   >> 16) & 0xff, (src[i]   >> 8) & 0xff, src[i]   & 0xff, y0, u0, v0 );
                    RGB_TO_YCBCR( (src[i+1] >> 16) & 0xff, (src[i+1] >> 8) & 0xff, src[i+1] & 0xff, y1, u1, v1 );
                    d[i]   = y0;
                    d[i+1] = y1;
                    if (dst_surface->config.format == DSPF_NV61 || y & 1) {
#ifdef WORDS_BIGENDIAN
                         ((u16*) d1)[i>>1] = ((u0 + u1) >> 1) | (((v0 + v1) >> 1) << 8);
#else
                         ((u16*) d1)[i>>1] = ((v0 + v1) >> 1) | (((u0 + u1) >> 1) << 8);
#endif
                    }
               }
               if (len & 1) {
                    i = len - 1;
                    RGB_TO_YCBCR( (src[i] >> 16) & 0xff, (src[i] >> 8) & 0xff, src[i] & 0xff, y0, u0, v0 );
                    d[i] = y0;
                    if (dst_surface->config.format == DSPF_NV61 || y & 1)
#ifdef WORDS_BIGENDIAN
                         ((u16*) d1)[i>>1] = u0 | (v0 << 8);
#else
                         ((u16*) d1)[i>>1] = v0 | (u0 << 8);
#endif
               }
               break;

          case DSPF_NV24:
               for (i = 0; i < len; i++) {
                    RGB_TO_YCBCR( (src[i] >> 16) & 0xff, (src[i] >> 8) & 0xff, src[i] & 0xff, y0, u0, v0 );
                    d[i]      = y0;
                    d1[2*i]   = u0;
                    d1[2*i+1] = v0;
               }
               break;

          case DSPF_NV42:
               for (i = 0; i < len; i++) {
                    RGB_TO_YCBCR( (src[i] >> 16) & 0xff, (src[i] >> 8) & 0xff, src[i] & 0xff, y0, u0, v0 );
                    d[i]      = y0;
                    d1[2*i]   = v0;
                    d1[2*i+1] = u0;
               }
               break;

          case DSPF_RGB555:
               for (i = 0; i < len; i++)
                    ((u16*) d)[i] = ARGB_TO_RGB555( src[i] );
               break;

          case DSPF_BGR555:
               for (i = 0; i < len; i++)
                    ((u16*) d)[i] = ARGB_TO_BGR555( src[i] );
               break;

          case DSPF_RGB444:
               for (i = 0; i < len; i++)
                    ((u16*) d)[i] = ARGB_TO_RGB444( src[i] );
               break;

          case DSPF_RGBA5551:
               for (i = 0; i < len; i++)
                    ((u16*) d)[i] = ARGB_TO_RGBA5551( src[i] );
               break;

          case DSPF_RGBAF88871:
               for (i = 0; i < len; i++)
                    ((u32*) d)[i] = ARGB_TO_RGBAF88871( src[i] );
               break;

          default:
               D_ONCE( "unimplemented destination format (0x%08x)", (unsigned int) dst_surface->config.format );
               break;
     }

#undef RGB_TO_YCBCR
}

void
dfb_copy_buffer_32( u32             *src,
                    void            *dst,
                    int              dpitch,
                    DFBRectangle    *drect,
                    CoreSurface     *dst_surface,
                    const DFBRegion *dst_clip )
{
     int  x, y;
     u8  *dst1 = NULL;
     u8  *dst2 = NULL;

     if (dst_clip) {
          int sx = 0, sy = 0;

          if (drect->x < dst_clip->x1) {
               sx = dst_clip->x1 - drect->x;
               drect->w -= sx;
               drect->x += sx;
          }

          if (drect->y < dst_clip->y1) {
               sy = dst_clip->y1 - drect->y;
               drect->h -= sy;
               drect->y += sy;
          }

          if ((drect->x + drect->w - 1) > dst_clip->x2) {
               drect->w -= drect->x + drect->w - 1 - dst_clip->x2;
          }

          if ((drect->y + drect->h - 1) > dst_clip->y2) {
               drect->h -= drect->y + drect-> h - 1 - dst_clip->y2;
          }

          src += sy * drect->w + sx;
     }

     if (drect->w < 1 || drect->h < 1)
          return;

     switch (dst_surface->config.format) {
          case DSPF_I420:
               dst1 = (u8*) dst + dpitch * dst_surface->config.size.h;
               dst2 = dst1 + dpitch / 2 * dst_surface->config.size.h / 2;
               break;
          case DSPF_YV12:
               dst2 = (u8*) dst + dpitch * dst_surface->config.size.h;
               dst1 = dst2 + dpitch / 2 * dst_surface->config.size.h / 2;
               break;
          case DSPF_Y42B:
               dst1 = (u8*) dst + dpitch * dst_surface->config.size.h;
               dst2 = dst1 + dpitch / 2 * dst_surface->config.size.h;
               break;
          case DSPF_YV16:
               dst2 = (u8*) dst + dpitch * dst_surface->config.size.h;
               dst1 = dst2 + dpitch / 2 * dst_surface->config.size.h;
               break;
          case DSPF_Y444:
               dst1 = (u8*) dst + dpitch * dst_surface->config.size.h;
               dst2 = dst1 + dpitch * dst_surface->config.size.h;
               break;
          case DSPF_YV24:
               dst2 = (u8*) dst + dpitch * dst_surface->config.size.h;
               dst1 = dst2 + dpitch * dst_surface->config.size.h;
               break;
          case DSPF_NV12:
          case DSPF_NV21:
          case DSPF_NV16:
          case DSPF_NV61:
          case DSPF_NV24:
          case DSPF_NV42:
               dst1 = (u8*) dst + dpitch * dst_surface->config.size.h;
               break;
          default:
               break;
     }

     x = drect->x;

     for (y = drect->y; y < drect->y + drect->h; y++) {
          u8 *d[3];

          d[0] = LINE_PTR( dst, dst_surface->config.caps, y, dst_surface->config.size.h, dpitch ) +
                 DFB_BYTES_PER_LINE( dst_surface->config.format, x );

          switch (dst_surface->config.format) {
               case DSPF_I420:
               case DSPF_YV12:
                    d[1] = LINE_PTR( dst1, dst_surface->config.caps, y / 2, dst_surface->config.size.h / 2,
                                     dpitch / 2 ) + x / 2;
                    d[2] = LINE_PTR( dst2, dst_surface->config.caps, y / 2, dst_surface->config.size.h / 2,
                                     dpitch / 2 ) + x / 2;
                    break;
               case DSPF_Y42B:
               case DSPF_YV16:
                    d[1] = LINE_PTR( dst1, dst_surface->config.caps, y, dst_surface->config.size.h,
                                     dpitch / 2 ) + x / 2;
                    d[2] = LINE_PTR( dst2, dst_surface->config.caps, y, dst_surface->config.size.h,
                                     dpitch / 2 ) + x / 2;
                    break;
               case DSPF_Y444:
               case DSPF_YV24:
                    d[1] = LINE_PTR( dst1, dst_surface->config.caps, y, dst_surface->config.size.h,
                                     dpitch ) + x;
                    d[2] = LINE_PTR( dst2, dst_surface->config.caps, y, dst_surface->config.size.h,
                                     dpitch ) + x;
                 break;
               case DSPF_NV12:
               case DSPF_NV21:
                    d[1] = LINE_PTR( dst1, dst_surface->config.caps, y / 2, dst_surface->config.size.h / 2,
                                     dpitch ) + (x & ~1);
                    break;
               case DSPF_NV16:
               case DSPF_NV61:
                    d[1] = LINE_PTR( dst1, dst_surface->config.caps, y, dst_surface->config.size.h,
                                     dpitch ) + (x & ~1);
                    break;
               case DSPF_NV24:
               case DSPF_NV42:
                    d[1] = LINE_PTR( dst1, dst_surface->config.caps, y, dst_surface->config.size.h,
                                     dpitch * 2 ) + x * 2;
                    break;
               default:
                    break;
          }

          write_argb_span( src, d, drect->w, x, y, dst_surface );

          src += drect->w;
     }
}

static int
bilinear_make_fast_weights( PixopsFilter *filter,
                            float         x_scale,
                            float         y_scale )
{
     int    i_offset, j_offset;
     float *x_weights, *y_weights;
     int    n_x, n_y;

     if (x_scale > 1.0) { /* Bilinear */
          n_x = 2;
          filter->x_offset = 0.5 * (1.0 / x_scale - 1);
     }
     else {               /* Tile */
          n_x = D_ICEIL ( 1.0 + 1.0 / x_scale );
          filter->x_offset = 0.0;
     }

     if (y_scale > 1.0) { /* Bilinear */
          n_y = 2;
          filter->y_offset = 0.5 * (1.0 / y_scale - 1);
     }
     else {               /* Tile */
          n_y = D_ICEIL ( 1.0 + 1.0 / y_scale );
          filter->y_offset = 0.0;
     }

     if (n_x > 64)
          n_x = 64;

     if (n_y > 64)
          n_y = 64;

     filter->n_y = n_y;
     filter->n_x = n_x;
     filter->weights = D_MALLOC( SUBSAMPLE * SUBSAMPLE * n_x * n_y * sizeof(int) );
     if (!filter->weights) {
          D_WARN( "couldn't allocate memory for scaling" );
          return 0;
     }

     x_weights = alloca( n_x * sizeof(float) );
     y_weights = alloca( n_y * sizeof(float) );
     if (!x_weights || !y_weights) {
          D_FREE( filter->weights );
          D_WARN( "couldn't allocate memory for scaling" );
          return 0;
     }

     for (i_offset = 0; i_offset < SUBSAMPLE; i_offset++)
          for (j_offset = 0; j_offset < SUBSAMPLE; j_offset++) {
               int    i, j;
               float  x             = j_offset / 16.0;
               float  y             = i_offset / 16.0;
               int   *pixel_weights = filter->weights + ((i_offset * SUBSAMPLE) + j_offset) * n_x * n_y;

               if (x_scale > 1.0) { /* Bilinear */
                    for (j = 0; j < n_x; j++) {
                         x_weights[j] = ((j == 0) ? (1 - x) : x) / x_scale;
                    }
               }
               else {               /* Tile */
                    for (j = 0; j < n_x; j++) {
                         if (j < x) {
                              if (j + 1 > x)
                                   x_weights[j] = MIN( j + 1, x + 1.0 / x_scale ) - x;
                              else
                                   x_weights[j] = 0;
                         }
                         else {
                              if (x + 1 / x_scale > j)
                                   x_weights[j] = MIN( j + 1, x + 1.0 / x_scale ) - j;
                              else
                                   x_weights[j] = 0;
                         }
                    }
               }

               if (y_scale > 1.0) { /* Bilinear */
                    for (i = 0; i < n_y; i++) {
                         y_weights[i] = ((i == 0) ? (1 - y) : y) / y_scale;
                    }
               }
               else {               /* Tile */
                    for (i = 0; i < n_y; i++) {
                         if (i < y) {
                              if (i + 1 > y)
                                   y_weights[i] = MIN( i + 1, y + 1.0 / y_scale ) - y;
                              else
                                   y_weights[i] = 0;
                         }
                         else {
                              if (y + 1 / y_scale > i)
                                   y_weights[i] = MIN( i + 1, y + 1.0 / y_scale ) - i;
                              else
                                   y_weights[i] = 0;
                         }
                    }
               }

               for (i = 0; i < n_y; i++) {
                    for (j = 0; j < n_x; j++) {
                         pixel_weights[n_x * i + j] = 65536 * x_weights[j] * x_scale * y_weights[i] * y_scale;
                    }
               }
          }

     return 1;
}

static void
scale_pixel( const int  *weights,
             int         n_x,
             int         n_y,
             u32        *dst,
             const u32 **src,
             int         x,
             int         sw )
{
     int i, j;
     u32 r = 0, g = 0, b = 0, a = 0;

     for (i = 0; i < n_y; i++) {
          const int *pixel_weights = weights + n_x * i;

          for (j = 0; j < n_x; j++) {
               const u32 *q;

               if (x + j < 0)
                    q = src[i];
               else if (x + j < sw)
                    q = src[i] + x + j;
               else
                    q = src[i] + sw - 1;

               b += pixel_weights[j] * ( *q & 0xff             );
               g += pixel_weights[j] * ((*q & 0xff00)     >>  8);
               r += pixel_weights[j] * ((*q & 0xff0000)   >> 16);
               a += pixel_weights[j] * ((*q & 0xff000000) >> 24);
          }
     }

     r = (r >> 16) == 0xff ? 0xff : (r + 0x8000) >> 16;
     g = (g >> 16) == 0xff ? 0xff : (g + 0x8000) >> 16;
     b = (b >> 16) == 0xff ? 0xff : (b + 0x8000) >> 16;
     a = (a >> 16) == 0xff ? 0xff : (a + 0x8000) >> 16;

     *dst = (a << 24) | (r << 16) | (g << 8) | b;
}

static u32 *
scale_line( const int  *weights,
            int         n_x,
            int         n_y,
            u32        *dst,
            u32        *dst_end,
            const u32 **src,
            int         x,
            int         x_step,
            int         sw )
{
     while (dst < dst_end) {
          int        i, j;
          u32        r, g, b, a;
          int        x_scaled      = x >> SCALE_SHIFT;
          const int *pixel_weights = weights + ((x >> (SCALE_SHIFT - SUBSAMPLE_BITS)) & SUBSAMPLE_MASK) * n_x * n_y;

          r = g = b = a = 0;

          for (i = 0; i < n_y; i++) {
               const int *line_weights = pixel_weights + n_x * i;
               const u32 *q            = src[i] + x_scaled;

               for (j = 0; j < n_x; j++) {
                    b += line_weights[j] * ( *q & 0xff             );
                    g += line_weights[j] * ((*q & 0xff00)     >>  8);
                    r += line_weights[j] * ((*q & 0xff0000)   >> 16);
                    a += line_weights[j] * ((*q & 0xff000000) >> 24);

                    if (x_scaled + j < sw - 1)
                         q++;
               }
          }

          r = (r >> 16) == 0xff ? 0xff : (r + 0x8000) >> 16;
          g = (g >> 16) == 0xff ? 0xff : (g + 0x8000) >> 16;
          b = (b >> 16) == 0xff ? 0xff : (b + 0x8000) >> 16;
          a = (a >> 16) == 0xff ? 0xff : (a + 0x8000) >> 16;

          *dst++ = (a << 24) | (r << 16) | (g << 8) | b;

          x += x_step;
     }

     return dst;
}

void
dfb_scale_linear_32( u32             *src,
                     int              sw,
                     int              sh,
                     void            *dst,
                     int              dpitch,
                     DFBRectangle    *drect,
                     CoreSurface     *dst_surface,
                     const DFBRegion *dst_clip )
{
     float         scale_x, scale_y;
     int           x, y;
     int           sx, sy;
     int           x_step, y_step;
     int           scaled_x_offset;
     PixopsFilter  filter;
     u32          *scaled_src;
     u8           *dst1  = NULL;
     u8           *dst2  = NULL;
     DFBRectangle  srect = { 0, 0, sw, sh };

     if (drect->w == sw && drect->h == sh) {
          dfb_copy_buffer_32( src, dst, dpitch, drect, dst_surface, dst_clip );
          return;
     }

     if (dst_clip)
          dfb_clip_stretchblit( dst_clip, &srect, drect );

     if (srect.w < 1 || srect.h < 1 || drect->w < 1 || drect->h < 1)
          return;

     src += srect.y * sw + srect.x;

     scale_x = (float) drect->w / srect.w;
     scale_y = (float) drect->h / srect.h;

     x_step = (1 << SCALE_SHIFT) / scale_x;
     y_step = (1 << SCALE_SHIFT) / scale_y;

     if (!bilinear_make_fast_weights( &filter, scale_x, scale_y ))
          return;

     scaled_x_offset = D_IFLOOR( filter.x_offset * (1 << SCALE_SHIFT) );
     sy              = D_IFLOOR( filter.y_offset * (1 << SCALE_SHIFT) );

     scaled_src = alloca( drect->w * 4 );

     switch (dst_surface->config.format) {
          case DSPF_I420:
               dst1 = (u8*) dst + dpitch * dst_surface->config.size.h;
               dst2 = dst1 + dpitch / 2 * dst_surface->config.size.h / 2;
               break;
          case DSPF_YV12:
               dst2 = (u8*) dst + dpitch * dst_surface->config.size.h;
               dst1 = dst2 + dpitch / 2 * dst_surface->config.size.h / 2;
               break;
          case DSPF_Y42B:
               dst1 = (u8*) dst + dpitch * dst_surface->config.size.h;
               dst2 = dst1 + dpitch / 2 * dst_surface->config.size.h;
               break;
          case DSPF_YV16:
               dst2 = (u8*) dst + dpitch * dst_surface->config.size.h;
               dst1 = dst2 + dpitch / 2 * dst_surface->config.size.h;
               break;
          case DSPF_Y444:
               dst1 = (u8*) dst + dpitch * dst_surface->config.size.h;
               dst2 = dst1 + dpitch * dst_surface->config.size.h;
               break;
          case DSPF_YV24:
               dst2 = (u8*) dst + dpitch * dst_surface->config.size.h;
               dst1 = dst2 + dpitch * dst_surface->config.size.h;
               break;
          case DSPF_NV12:
          case DSPF_NV21:
          case DSPF_NV16:
          case DSPF_NV61:
          case DSPF_NV24:
          case DSPF_NV42:
               dst1 = (u8*) dst + dpitch * dst_surface->config.size.h;
               break;
          default:
               break;
     }

     x = drect->x;

     for (y = drect->y; y < drect->y + drect->h; y++) {
          int         i;
          int         x_start, y_start;
          const int  *run_weights;
          u8         *d[3];
          const u32 **bufs;
          u32        *buf     = scaled_src;
          u32        *buf_end = scaled_src + drect->w;
          u32        *new_buf;

          y_start = sy >> SCALE_SHIFT;

          run_weights = filter.weights +
                        ((sy >> (SCALE_SHIFT - SUBSAMPLE_BITS)) & SUBSAMPLE_MASK) * filter.n_x * filter.n_y * SUBSAMPLE;

          bufs = alloca( filter.n_y * sizeof(void*) );

          for (i = 0; i < filter.n_y; i++) {
               if (y_start <  0)
                    bufs[i] = src;
               else if (y_start < sh)
                    bufs[i] = src + sw * y_start;
               else
                    bufs[i] = src + sw * (sh - 1);

               y_start++;
          }

          sx = scaled_x_offset;
          x_start = sx >> SCALE_SHIFT;

          while (x_start < 0 && buf < buf_end) {
               scale_pixel( run_weights +
                            ((sx >> (SCALE_SHIFT - SUBSAMPLE_BITS)) & SUBSAMPLE_MASK) * filter.n_x * filter.n_y,
                            filter.n_x, filter.n_y, buf, bufs, sx >> SCALE_SHIFT, sw );
               sx += x_step;
               x_start = sx >> SCALE_SHIFT;
               buf++;
          }

          new_buf = scale_line( run_weights,
                                filter.n_x, filter.n_y, buf, buf_end, bufs, sx >> SCALE_SHIFT, x_step, sw );

          sx = ((buf_end - buf) >> 2) * x_step + scaled_x_offset;
          buf = new_buf;

          while (buf < buf_end) {
               scale_pixel( run_weights +
                            ((sx >> (SCALE_SHIFT - SUBSAMPLE_BITS)) & SUBSAMPLE_MASK) * filter.n_x * filter.n_y,
                            filter.n_x, filter.n_y, buf, bufs, sx >> SCALE_SHIFT, sw );
               sx += x_step;
               buf++;
          }

          sy += y_step;

          d[0] = LINE_PTR( dst, dst_surface->config.caps, y, dst_surface->config.size.h, dpitch ) +
                 DFB_BYTES_PER_LINE( dst_surface->config.format, x );

          switch (dst_surface->config.format) {
               case DSPF_I420:
               case DSPF_YV12:
                    d[1] = LINE_PTR( dst1, dst_surface->config.caps, y / 2, dst_surface->config.size.h / 2,
                                     dpitch / 2 ) + x / 2;
                    d[2] = LINE_PTR( dst2, dst_surface->config.caps, y / 2, dst_surface->config.size.h / 2,
                                     dpitch / 2 ) + x / 2;
                    break;
               case DSPF_Y42B:
               case DSPF_YV16:
                    d[1] = LINE_PTR( dst1, dst_surface->config.caps, y, dst_surface->config.size.h,
                                     dpitch / 2 ) + x / 2;
                    d[2] = LINE_PTR( dst2, dst_surface->config.caps, y, dst_surface->config.size.h,
                                     dpitch / 2 ) + x / 2;
                    break;
               case DSPF_Y444:
               case DSPF_YV24:
                    d[1] = LINE_PTR( dst1, dst_surface->config.caps, y, dst_surface->config.size.h,
                                     dpitch ) + x;
                    d[2] = LINE_PTR( dst2, dst_surface->config.caps, y, dst_surface->config.size.h,
                                     dpitch ) + x;
                    break;
               case DSPF_NV12:
               case DSPF_NV21:
                    d[1] = LINE_PTR( dst1, dst_surface->config.caps, y / 2, dst_surface->config.size.h / 2,
                                     dpitch ) + (x & ~1);
                    break;
               case DSPF_NV16:
               case DSPF_NV61:
                    d[1] = LINE_PTR( dst1, dst_surface->config.caps, y, dst_surface->config.size.h,
                                     dpitch ) + (x & ~1);
                    break;
               case DSPF_NV24:
               case DSPF_NV42:
                    d[1] = LINE_PTR( dst1, dst_surface->config.caps, y, dst_surface->config.size.h,
                                     dpitch * 2 ) + (x * 2);
                    break;
               default:
                    break;
          }

          write_argb_span( scaled_src, d, drect->w, x, y, dst_surface );
     }

     D_FREE( filter.weights );
}

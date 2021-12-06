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

#if UPDOWN == 1
#define FUNC_NAME_(K,P,F) FUNC_NAME(up,K,P,F)
#else
#define FUNC_NAME_(K,P,F) FUNC_NAME(down,K,P,F)
#endif

/**********************************************************************************************************************/
/* DST_FORMAT */

/* DST_FORMAT NONE */
static void
FUNC_NAME_(_,_,DST_FORMAT)
#include STRETCH_HVX_N_H

/* DST_FORMAT SRCKEY */
#define COLOR_KEY ctx->key
static void
FUNC_NAME_(K,_,DST_FORMAT)
#include STRETCH_HVX_N_H
#undef COLOR_KEY

/* DST_FORMAT PROTECT */
#define KEY_PROTECT ctx->protect
static void
FUNC_NAME_(_,P,DST_FORMAT)
#include STRETCH_HVX_N_H

/* DST_FORMAT SRCKEY PROTECT */
#define COLOR_KEY ctx->key
static void
FUNC_NAME_(K,P,DST_FORMAT)
#include STRETCH_HVX_N_H
#undef COLOR_KEY
#undef KEY_PROTECT

/**********************************************************************************************************************/
/* DSPF_LUT8 */
#define SOURCE_LOOKUP(x) ((const uN*) ctx->colors)[x]
#define SOURCE_TYPE      u8

/* DSPF_LUT8 NONE */
static void
FUNC_NAME_(_,_,DSPF_LUT8)
#include STRETCH_HVX_N_H

/* DSPF_LUT8 SRCKEY */
#define COLOR_KEY ctx->key
static void
FUNC_NAME_(K,_,DSPF_LUT8)
#include STRETCH_HVX_N_H
#undef COLOR_KEY

/* DSPF_LUT8 PROTECT */
#define KEY_PROTECT ctx->protect
static void
FUNC_NAME_(_,P,DSPF_LUT8)
#include STRETCH_HVX_N_H

/* DSPF_LUT8 SRCKEY PROTECT */
#define COLOR_KEY ctx->key
static void
FUNC_NAME_(K,P,DSPF_LUT8)
#include STRETCH_HVX_N_H
#undef COLOR_KEY
#undef KEY_PROTECT

#undef SOURCE_LOOKUP
#undef SOURCE_TYPE

#ifdef FORMAT_RGB16

/**********************************************************************************************************************/
/* DSPF_ARGB4444 */
#define SOURCE_LOOKUP(x) PIXEL_RGB16( (((x) & 0x0f00) >> 4) | (((x) & 0x0f00) >> 8), \
                                      ( (x) & 0x00f0      ) | (((x) & 0x00f0) >> 4), \
                                      (((x) & 0x000f) << 4) | ( (x) & 0x000f      ) )

/* DSPF_ARGB4444 NONE */
static void
FUNC_NAME_(_,_,DSPF_ARGB4444)
#include STRETCH_HVX_N_H

/* DSPF_ARGB4444 SRCKEY */
#define COLOR_KEY ctx->key
static void
FUNC_NAME_(K,_,DSPF_ARGB4444)
#include STRETCH_HVX_N_H
#undef COLOR_KEY

/* DSPF_ARGB4444 PROTECT */
#define KEY_PROTECT ctx->protect
static void
FUNC_NAME_(_,P,DSPF_ARGB4444)
#include STRETCH_HVX_N_H

/* DSPF_ARGB4444 PROTECT SRCKEY */
#define COLOR_KEY ctx->key
static void
FUNC_NAME_(K,P,DSPF_ARGB4444)
#include STRETCH_HVX_N_H
#undef COLOR_KEY
#undef KEY_PROTECT

#undef SOURCE_LOOKUP

#endif /* FORMAT_RGB16 */

#ifdef FORMAT_ARGB4444

/**********************************************************************************************************************/
/* DSPF_RGB16 */
#define SOURCE_LOOKUP(x) PIXEL_ARGB4444( 0xff,                  \
                                         (((x) & 0xf800) >> 8), \
                                         (((x) & 0x07e0) >> 3), \
                                         (((x) & 0x001f) << 3) )

/* DSPF_RGB16 NONE */
static void
FUNC_NAME_(_,_,DSPF_RGB16)
#include STRETCH_HVX_N_H

/* DSPF_RGB16 SRCKEY */
#define COLOR_KEY ctx->key
static void
FUNC_NAME_(K,_,DSPF_RGB16)
#include STRETCH_HVX_N_H
#undef COLOR_KEY

/* DSPF_RGB16 PROTECT */
#define KEY_PROTECT ctx->protect
static void
FUNC_NAME_(_,P,DSPF_RGB16)
#include STRETCH_HVX_N_H

/* DSPF_RGB16 PROTECT SRCKEY */
#define COLOR_KEY ctx->key
static void
FUNC_NAME_(K,P,DSPF_RGB16)
#include STRETCH_HVX_N_H
#undef COLOR_KEY
#undef KEY_PROTECT

#undef SOURCE_LOOKUP

#endif /* FORMAT_ARGB4444 */

/**********************************************************************************************************************/

#undef FUNC_NAME_

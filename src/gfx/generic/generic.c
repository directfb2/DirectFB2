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
#include <core/core.h>
#include <core/state.h>
#include <core/palette.h>
#include <direct/memcpy.h>
#include <gfx/convert.h>
#include <gfx/generic/duffs_device.h>
#include <gfx/generic/generic.h>
#include <gfx/util.h>

/**********************************************************************************************************************/

/* lookup tables for 2/3bit to 8bit color conversion */
static const u8 lookup3to8[] = { 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff };
static const u8 lookup2to8[] = { 0x00, 0x55, 0xaa, 0xff };

#define EXPAND_1to8(v) ((v) ? 0xff : 0x00)
#define EXPAND_2to8(v) lookup2to8[v]
#define EXPAND_3to8(v) lookup3to8[v]
#define EXPAND_4to8(v) (((v) << 4) |  (v)      )
#define EXPAND_5to8(v) (((v) << 3) | ((v) >> 2))
#define EXPAND_6to8(v) (((v) << 2) | ((v) >> 4))
#define EXPAND_7to8(v) (((v) << 1) | ((v) >> 6))

/**********************************************************************************************************************/

/* ARGB1555 / RGB555 / BGR555 / RGBA5551 */
#define RGB_MASK 0x7fff
#define Cop_OP_Aop_PFI(op) Cop_##op##_Aop_15
#define Bop_PFI_OP_Aop_PFI(op) Bop_15_##op##_Aop
#include "template_colorkey_16.h"

/* RGB16 */
#define RGB_MASK 0xffff
#define Cop_OP_Aop_PFI(op) Cop_##op##_Aop_16
#define Bop_PFI_OP_Aop_PFI(op) Bop_16_##op##_Aop
#include "template_colorkey_16.h"

/* RGB24 / BGR24 / VYU */
#define RGB_MASK 0xffffff
#define Cop_OP_Aop_PFI(op) Cop_##op##_Aop_24_24
#define Bop_PFI_OP_Aop_PFI(op) Bop_24_24_##op##_Aop
#include "template_colorkey_24.h"

/* RGB32 / ARGB / ABGR / AiRGB / AYUV / AVYU */
#define RGB_MASK 0x00ffffff
#define Cop_OP_Aop_PFI(op) Cop_##op##_Aop_32
#define Bop_PFI_OP_Aop_PFI(op) Bop_32_##op##_Aop
#include "template_colorkey_32.h"

/* ARGB2554 */
#define RGB_MASK 0x3fff
#define Cop_OP_Aop_PFI(op) Cop_##op##_Aop_14
#define Bop_PFI_OP_Aop_PFI(op) Bop_14_##op##_Aop
#include "template_colorkey_16.h"

/* ARGB4444 / RGB444 */
#define RGB_MASK 0x0fff
#define Cop_OP_Aop_PFI(op) Cop_##op##_Aop_12
#define Bop_PFI_OP_Aop_PFI(op) Bop_12_##op##_Aop
#include "template_colorkey_16.h"

/* RGBA4444 */
#define RGB_MASK 0xfff0
#define Cop_OP_Aop_PFI(op) Cop_##op##_Aop_12vv
#define Bop_PFI_OP_Aop_PFI(op) Bop_12vv_##op##_Aop
#include "template_colorkey_16.h"

/* ARGB1666 / ARGB6666 / RGB18 */
#define RGB_MASK 0x03ffff
#define Cop_OP_Aop_PFI(op) Cop_##op##_Aop_24_18
#define Bop_PFI_OP_Aop_PFI(op) Bop_24_18_##op##_Aop
#include "template_colorkey_24.h"

/* ARGB8565 */
#define RGB_MASK 0x00ffff
#define Cop_OP_Aop_PFI(op) Cop_##op##_Aop_24_16
#define Bop_PFI_OP_Aop_PFI(op) Bop_24_16_##op##_Aop
#include "template_colorkey_24.h"

/* RGBAF88871 */
#define RGB_MASK 0xffffff00
#define Cop_OP_Aop_PFI(op) Cop_##op##_Aop_32_24
#define Bop_PFI_OP_Aop_PFI(op) Bop_32_24_##op##_Aop
#include "template_colorkey_32.h"

/**********************************************************************************************************************/

/* ARGB1555 */
#define EXPAND_Ato8(a) EXPAND_1to8( a )
#define EXPAND_Rto8(r) EXPAND_5to8( r )
#define EXPAND_Gto8(g) EXPAND_5to8( g )
#define EXPAND_Bto8(b) EXPAND_5to8( b )
#define PIXEL_OUT(a,r,g,b) PIXEL_ARGB1555( a, r, g, b )
#define Sop_PFI_OP_Dacc(op) Sop_argb1555_##op##_Dacc
#define Sacc_OP_Aop_PFI(op) Sacc_##op##_Aop_argb1555
#define A_SHIFT 15
#define R_SHIFT 10
#define G_SHIFT 5
#define B_SHIFT 0
#define A_MASK 0x8000
#define R_MASK 0x7c00
#define G_MASK 0x03e0
#define B_MASK 0x001f
#include "template_acc_16.h"

/* RGB16 */
#define EXPAND_Ato8(a) 0xff
#define EXPAND_Rto8(r) EXPAND_5to8( r )
#define EXPAND_Gto8(g) EXPAND_6to8( g )
#define EXPAND_Bto8(b) EXPAND_5to8( b )
#define PIXEL_OUT(a,r,g,b) PIXEL_RGB16( r, g, b )
#define Sop_PFI_OP_Dacc(op) Sop_rgb16_##op##_Dacc
#define Sacc_OP_Aop_PFI(op) Sacc_##op##_Aop_rgb16
#define A_SHIFT 0
#define R_SHIFT 11
#define G_SHIFT 5
#define B_SHIFT 0
#define A_MASK 0
#define R_MASK 0xf800
#define G_MASK 0x07e0
#define B_MASK 0x001f
#include "template_acc_16.h"

/* RGB24 */
#define EXPAND_Ato8(a) 0xff
#define EXPAND_Rto8(r) (r)
#define EXPAND_Gto8(g) (g)
#define EXPAND_Bto8(b) (b)
#define PIXEL_OUT(a,r,g,b) PIXEL_RGB32( r, g, b )
#define Sop_PFI_OP_Dacc(op) Sop_rgb24_##op##_Dacc
#define Sacc_OP_Aop_PFI(op) Sacc_##op##_Aop_rgb24
#define A_SHIFT 0
#define R_SHIFT 16
#define G_SHIFT 8
#define B_SHIFT 0
#define A_MASK 0
#define R_MASK 0xff0000
#define G_MASK 0x00ff00
#define B_MASK 0x0000ff
#include "template_acc_24.h"

/* BGR24 */
#define EXPAND_Ato8(a) 0xff
#define EXPAND_Rto8(r) (r)
#define EXPAND_Gto8(g) (g)
#define EXPAND_Bto8(b) (b)
#define PIXEL_OUT(a,r,g,b) PIXEL_RGB32( b, g, r )
#define Sop_PFI_OP_Dacc(op) Sop_bgr24_##op##_Dacc
#define Sacc_OP_Aop_PFI(op) Sacc_##op##_Aop_bgr24
#define A_SHIFT 0
#define B_SHIFT 16
#define G_SHIFT 8
#define R_SHIFT 0
#define A_MASK 0
#define B_MASK 0xff0000
#define G_MASK 0x00ff00
#define R_MASK 0x0000ff
#include "template_acc_24.h"

/* RGB32 */
#define EXPAND_Ato8(a) 0xff
#define EXPAND_Rto8(r) (r)
#define EXPAND_Gto8(g) (g)
#define EXPAND_Bto8(b) (b)
#define PIXEL_OUT(a,r,g,b) PIXEL_RGB32( r, g, b )
#define Sop_PFI_OP_Dacc(op) Sop_rgb32_##op##_Dacc
#define Sacc_OP_Aop_PFI(op) Sacc_##op##_Aop_rgb32
#define A_SHIFT 0
#define R_SHIFT 16
#define G_SHIFT 8
#define B_SHIFT 0
#define A_MASK 0
#define R_MASK 0x00ff0000
#define G_MASK 0x0000ff00
#define B_MASK 0x000000ff
#include "template_acc_32.h"

/* ARGB */
#define EXPAND_Ato8(a) (a)
#define EXPAND_Rto8(r) (r)
#define EXPAND_Gto8(g) (g)
#define EXPAND_Bto8(b) (b)
#define PIXEL_OUT(a,r,g,b) PIXEL_ARGB( a, r, g, b )
#define Sop_PFI_OP_Dacc(op) Sop_argb_##op##_Dacc
#define Sacc_OP_Aop_PFI(op) Sacc_##op##_Aop_argb
#define A_SHIFT 24
#define R_SHIFT 16
#define G_SHIFT 8
#define B_SHIFT 0
#define A_MASK 0xff000000
#define R_MASK 0x00ff0000
#define G_MASK 0x0000ff00
#define B_MASK 0x000000ff
#include "template_acc_32.h"

/* ABGR */
#define EXPAND_Ato8(a) (a)
#define EXPAND_Rto8(r) (r)
#define EXPAND_Gto8(g) (g)
#define EXPAND_Bto8(b) (b)
#define PIXEL_OUT(a,r,g,b) PIXEL_ABGR( a, r, g, b )
#define Sop_PFI_OP_Dacc(op) Sop_abgr_##op##_Dacc
#define Sacc_OP_Aop_PFI(op) Sacc_##op##_Aop_abgr
#define A_SHIFT 24
#define B_SHIFT 16
#define G_SHIFT 8
#define R_SHIFT 0
#define A_MASK 0xff000000
#define B_MASK 0x00ff0000
#define G_MASK 0x0000ff00
#define R_MASK 0x000000ff
#include "template_acc_32.h"

/* AiRGB */
#define EXPAND_Ato8(a) ((a) ^ 0xff)
#define EXPAND_Rto8(r) (r)
#define EXPAND_Gto8(g) (g)
#define EXPAND_Bto8(b) (b)
#define PIXEL_OUT(a,r,g,b) PIXEL_AiRGB( a, r, g, b )
#define Sop_PFI_OP_Dacc(op) Sop_airgb_##op##_Dacc
#define Sacc_OP_Aop_PFI(op) Sacc_##op##_Aop_airgb
#define A_SHIFT 24
#define R_SHIFT 16
#define G_SHIFT 8
#define B_SHIFT 0
#define A_MASK 0xff000000
#define R_MASK 0x00ff0000
#define G_MASK 0x0000ff00
#define B_MASK 0x000000ff
#include "template_acc_32.h"

/* ARGB2554 */
#define EXPAND_Ato8(a) EXPAND_2to8( a )
#define EXPAND_Rto8(r) EXPAND_5to8( r )
#define EXPAND_Gto8(g) EXPAND_5to8( g )
#define EXPAND_Bto8(b) EXPAND_4to8( b )
#define PIXEL_OUT(a,r,g,b) PIXEL_ARGB2554( a, r, g, b )
#define Sop_PFI_OP_Dacc(op) Sop_argb2554_##op##_Dacc
#define Sacc_OP_Aop_PFI(op) Sacc_##op##_Aop_argb2554
#define A_SHIFT 14
#define R_SHIFT 9
#define G_SHIFT 4
#define B_SHIFT 0
#define A_MASK 0xc000
#define R_MASK 0x3e00
#define G_MASK 0x01f0
#define B_MASK 0x000f
#include "template_acc_16.h"

/* ARGB4444 */
#define EXPAND_Ato8(a) EXPAND_4to8( a )
#define EXPAND_Rto8(r) EXPAND_4to8( r )
#define EXPAND_Gto8(g) EXPAND_4to8( g )
#define EXPAND_Bto8(b) EXPAND_4to8( b )
#define PIXEL_OUT(a,r,g,b) PIXEL_ARGB4444( a, r, g, b )
#define Sop_PFI_OP_Dacc(op) Sop_argb4444_##op##_Dacc
#define Sacc_OP_Aop_PFI(op) Sacc_##op##_Aop_argb4444
#define A_SHIFT 12
#define R_SHIFT 8
#define G_SHIFT 4
#define B_SHIFT 0
#define A_MASK 0xf000
#define R_MASK 0x0f00
#define G_MASK 0x00f0
#define B_MASK 0x000f
#include "template_acc_16.h"

/* RGBA4444 */
#define EXPAND_Ato8(a) EXPAND_4to8( a )
#define EXPAND_Rto8(r) EXPAND_4to8( r )
#define EXPAND_Gto8(g) EXPAND_4to8( g )
#define EXPAND_Bto8(b) EXPAND_4to8( b )
#define PIXEL_OUT(a,r,g,b) PIXEL_RGBA4444( a, r, g, b )
#define Sop_PFI_OP_Dacc(op) Sop_rgba4444_##op##_Dacc
#define Sacc_OP_Aop_PFI(op) Sacc_##op##_Aop_rgba4444
#define A_SHIFT 0
#define R_SHIFT 12
#define G_SHIFT 8
#define B_SHIFT 4
#define A_MASK 0x000f
#define R_MASK 0xf000
#define G_MASK 0x0f00
#define B_MASK 0x00f0
#include "template_acc_16.h"

/* ARGB1666 */
#define EXPAND_Ato8(a) EXPAND_1to8( a )
#define EXPAND_Rto8(r) EXPAND_6to8( r )
#define EXPAND_Gto8(g) EXPAND_6to8( g )
#define EXPAND_Bto8(b) EXPAND_6to8( b )
#define PIXEL_OUT(a,r,g,b) PIXEL_ARGB1666( a, r, g, b )
#define Sop_PFI_OP_Dacc(op) Sop_argb1666_##op##_Dacc
#define Sacc_OP_Aop_PFI(op) Sacc_##op##_Aop_argb1666
#define A_SHIFT 18
#define R_SHIFT 12
#define G_SHIFT 6
#define B_SHIFT 0
#define A_MASK 0x040000
#define R_MASK 0x03f000
#define G_MASK 0x000fc0
#define B_MASK 0x00003f
#include "template_acc_24.h"

/* ARGB6666 */
#define EXPAND_Ato8(a) EXPAND_6to8( a )
#define EXPAND_Rto8(r) EXPAND_6to8( r )
#define EXPAND_Gto8(g) EXPAND_6to8( g )
#define EXPAND_Bto8(b) EXPAND_6to8( b )
#define PIXEL_OUT(a,r,g,b) PIXEL_ARGB6666( a, r, g, b )
#define Sop_PFI_OP_Dacc(op) Sop_argb6666_##op##_Dacc
#define Sacc_OP_Aop_PFI(op) Sacc_##op##_Aop_argb6666
#define A_SHIFT 18
#define R_SHIFT 12
#define G_SHIFT 6
#define B_SHIFT 0
#define A_MASK 0xfc0000
#define R_MASK 0x03f000
#define G_MASK 0x000fc0
#define B_MASK 0x00003f
#include "template_acc_24.h"

/* RGB18 */
#define EXPAND_Ato8(a) 0xff
#define EXPAND_Rto8(r) EXPAND_6to8( r )
#define EXPAND_Gto8(g) EXPAND_6to8( g )
#define EXPAND_Bto8(b) EXPAND_6to8( b )
#define PIXEL_OUT(a,r,g,b) PIXEL_RGB18( r, g, b )
#define Sop_PFI_OP_Dacc(op) Sop_rgb18_##op##_Dacc
#define Sacc_OP_Aop_PFI(op) Sacc_##op##_Aop_rgb18
#define A_SHIFT 0
#define R_SHIFT 12
#define G_SHIFT 6
#define B_SHIFT 0
#define A_MASK 0
#define R_MASK 0x03f000
#define G_MASK 0x000fc0
#define B_MASK 0x00003f
#include "template_acc_24.h"

/* RGB444 */
#define EXPAND_Ato8(a) 0xff
#define EXPAND_Rto8(r) EXPAND_4to8( r )
#define EXPAND_Gto8(g) EXPAND_4to8( g )
#define EXPAND_Bto8(b) EXPAND_4to8( b )
#define PIXEL_OUT(a,r,g,b) PIXEL_RGB444( r, g, b )
#define Sop_PFI_OP_Dacc(op) Sop_xrgb4444_##op##_Dacc
#define Sacc_OP_Aop_PFI(op) Sacc_##op##_Aop_xrgb4444
#define A_SHIFT 0
#define R_SHIFT 8
#define G_SHIFT 4
#define B_SHIFT 0
#define A_MASK 0
#define R_MASK 0x0f00
#define G_MASK 0x00f0
#define B_MASK 0x000f
#include "template_acc_16.h"

/* RGB555 */
#define EXPAND_Ato8(a) 0xff
#define EXPAND_Rto8(r) EXPAND_5to8( r )
#define EXPAND_Gto8(g) EXPAND_5to8( g )
#define EXPAND_Bto8(b) EXPAND_5to8( b )
#define PIXEL_OUT(a,r,g,b) PIXEL_RGB555( r, g, b )
#define Sop_PFI_OP_Dacc(op) Sop_xrgb1555_##op##_Dacc
#define Sacc_OP_Aop_PFI(op) Sacc_##op##_Aop_xrgb1555
#define A_SHIFT 0
#define R_SHIFT 10
#define G_SHIFT 5
#define B_SHIFT 0
#define A_MASK 0
#define R_MASK 0x7c00
#define G_MASK 0x03e0
#define B_MASK 0x001f
#include "template_acc_16.h"

/* BGR555 */
#define EXPAND_Ato8(a) 0xff
#define EXPAND_Rto8(r) EXPAND_5to8( r )
#define EXPAND_Gto8(g) EXPAND_5to8( g )
#define EXPAND_Bto8(b) EXPAND_5to8( b )
#define PIXEL_OUT(a,r,g,b) PIXEL_BGR555( r, g, b )
#define Sop_PFI_OP_Dacc(op) Sop_xbgr1555_##op##_Dacc
#define Sacc_OP_Aop_PFI(op) Sacc_##op##_Aop_xbgr1555
#define A_SHIFT 0
#define B_SHIFT 10
#define G_SHIFT 5
#define R_SHIFT 0
#define A_MASK 0
#define B_MASK 0x7c00
#define G_MASK 0x03e0
#define R_MASK 0x001f
#include "template_acc_16.h"

/* RGBA5551 */
#define EXPAND_Ato8(a) EXPAND_1to8( a )
#define EXPAND_Rto8(r) EXPAND_5to8( r )
#define EXPAND_Gto8(g) EXPAND_5to8( g )
#define EXPAND_Bto8(b) EXPAND_5to8( b )
#define PIXEL_OUT(a,r,g,b) PIXEL_RGBA5551( a, r, g, b )
#define Sop_PFI_OP_Dacc(op) Sop_rgba5551_##op##_Dacc
#define Sacc_OP_Aop_PFI(op) Sacc_##op##_Aop_rgba5551
#define A_SHIFT 0
#define R_SHIFT 11
#define G_SHIFT 6
#define B_SHIFT 1
#define A_MASK 0x0001
#define R_MASK 0xf800
#define G_MASK 0x07c0
#define B_MASK 0x003e
#include "template_acc_16.h"

/* ARGB8565 */
#define EXPAND_Ato8(a) (a)
#define EXPAND_Rto8(r) EXPAND_5to8( r )
#define EXPAND_Gto8(g) EXPAND_6to8( g )
#define EXPAND_Bto8(b) EXPAND_5to8( b )
#define PIXEL_OUT(a,r,g,b) PIXEL_ARGB8565( a, r, g, b )
#define Sop_PFI_OP_Dacc(op) Sop_argb8565_##op##_Dacc
#define Sacc_OP_Aop_PFI(op) Sacc_##op##_Aop_argb8565
#define A_SHIFT 16
#define R_SHIFT 11
#define G_SHIFT 5
#define B_SHIFT 0
#define A_MASK 0xff0000
#define R_MASK 0x00f800
#define G_MASK 0x0007e0
#define B_MASK 0x00001f
#include "template_acc_24.h"

/* RGBAF88871 */
#define EXPAND_Ato8(a) EXPAND_7to8(a)
#define EXPAND_Rto8(r) (r)
#define EXPAND_Gto8(g) (g)
#define EXPAND_Bto8(b) (b)
#define PIXEL_OUT(a,r,g,b) PIXEL_RGBAF88871( a, r, g, b )
#define Sop_PFI_OP_Dacc(op) Sop_rgbaf88871_##op##_Dacc
#define Sacc_OP_Aop_PFI(op) Sacc_##op##_Aop_rgbaf88871
#define A_SHIFT 1
#define R_SHIFT 24
#define G_SHIFT 16
#define B_SHIFT 8
#define A_MASK 0x000000fe
#define R_MASK 0xff000000
#define G_MASK 0x00ff0000
#define B_MASK 0x0000ff00
#include "template_acc_32.h"

/**********************************************************************************************************************/

/* Whether pixelformat is YUV or not. */
static const bool is_ycbcr[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1555)]   = false,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB16)]      = false,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB24)]      = false,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB32)]      = false,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]       = false,
     [DFB_PIXELFORMAT_INDEX(DSPF_A8)]         = false,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUY2)]       = true,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB332)]     = false,
     [DFB_PIXELFORMAT_INDEX(DSPF_UYVY)]       = true,
     [DFB_PIXELFORMAT_INDEX(DSPF_I420)]       = true,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV12)]       = true,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT8)]       = false,
     [DFB_PIXELFORMAT_INDEX(DSPF_ALUT44)]     = false,
     [DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)]      = false,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1)]         = false,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV12)]       = true,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV16)]       = true,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB2554)]   = false,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)]   = false,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA4444)]   = false,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV21)]       = true,
     [DFB_PIXELFORMAT_INDEX(DSPF_AYUV)]       = true,
     [DFB_PIXELFORMAT_INDEX(DSPF_A4)]         = false,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1666)]   = false,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB6666)]   = false,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB18)]      = false,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT2)]       = false,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB444)]     = false,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB555)]     = false,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR555)]     = false,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA5551)]   = false,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y444)]       = true,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB8565)]   = false,
     [DFB_PIXELFORMAT_INDEX(DSPF_AVYU)]       = true,
     [DFB_PIXELFORMAT_INDEX(DSPF_VYU)]        = true,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1_LSB)]     = false,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV16)]       = true,
     [DFB_PIXELFORMAT_INDEX(DSPF_ABGR)]       = false,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBAF88871)] = false,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT1)]       = false,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV61)]       = true,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y42B)]       = true,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV24)]       = true,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV24)]       = true,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV42)]       = true,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR24)]      = false,
};

/**********************************************************************************************************************
 ********************************* Cop_to_Aop_PFI *********************************************************************
 **********************************************************************************************************************/

static void
Cop_to_Aop_16( GenefxState *gfxs )
{
     int  l;
     int  w   = gfxs->length;
     u32 *D   = gfxs->Aop[0];
     u32  Cop = gfxs->Cop;

     u32 DCop = ((Cop << 16) | Cop);

     if (((long) D) & 2) {
          u16* tmp = (u16*) D;
          --w;
          *tmp = Cop;
          D = (u32*) (tmp + 1);
     }

     l = w >> 1;
     while (l) {
          *D = DCop;

          ++D;
          --l;
     }

     if (w & 1)
          *((u16*) D) = (u16) Cop;
}

static void
Cop_to_Aop_24( GenefxState *gfxs )
{
     int  w   = gfxs->length + 1;
     u8  *D   = gfxs->Aop[0];
     u32  Cop = gfxs->Cop;

     while (--w) {
#ifdef WORDS_BIGENDIAN
          D[0] = (Cop >> 16) & 0xff;
          D[1] = (Cop >>  8) & 0xff;
          D[2] =  Cop        & 0xff;
#else
          D[0] =  Cop        & 0xff;
          D[1] = (Cop >>  8) & 0xff;
          D[2] = (Cop >> 16) & 0xff;
#endif

          D += 3;
     }
}

static void
Cop_to_Aop_32( GenefxState *gfxs )
{
     int  w   = gfxs->length;
     u32 *D   = gfxs->Aop[0];
     u32  Cop = gfxs->Cop;

     while (w) {
          if (!(w % 8)) {
               *D++ = Cop;
               *D++ = Cop;
               *D++ = Cop;
               *D++ = Cop;
               *D++ = Cop;
               *D++ = Cop;
               *D++ = Cop;
               *D++ = Cop;
               w -= 8;
          }
          else if (!(w % 4)) {
               *D++ = Cop;
               *D++ = Cop;
               *D++ = Cop;
               *D++ = Cop;
               w -= 4;
          }
          else if (!(w % 2)) {
               *D++ = Cop;
               *D++ = Cop;
               w -= 2;
          }
          else {
               *D++ = Cop;
               --w;
          }
     }
}

static void
Cop_to_Aop_8( GenefxState *gfxs )
{
     memset( gfxs->Aop[0], gfxs->Cop, gfxs->length );
}

static void
Cop_to_Aop_yuv422( GenefxState *gfxs )
{
     int  l;
     int  w   = gfxs->length;
     u16 *D   = gfxs->Aop[0];
     u32  Cop = gfxs->Cop;

     if ((long) D & 2) {
#ifdef WORDS_BIGENDIAN
          *D++ = Cop & 0xffff;
#else
          *D++ = Cop >> 16;
#endif
          --w;
     }

     for (l = w >> 1; l--;) {
          *((u32*) D) = Cop;
          D += 2;
     }

     if (w & 1) {
#ifdef WORDS_BIGENDIAN
          *D = Cop >> 16;
#else
          *D = Cop & 0xffff;
#endif
     }
}

static void
Cop_to_Aop_i420( GenefxState *gfxs )
{
     memset( gfxs->Aop[0], gfxs->YCop, gfxs->length );

     if (gfxs->AopY & 1) {
          memset( gfxs->Aop[1], gfxs->CbCop, gfxs->length >> 1 );
          memset( gfxs->Aop[2], gfxs->CrCop, gfxs->length >> 1 );
     }
}

static void
Cop_to_Aop_nv12( GenefxState *gfxs )
{
     memset( gfxs->Aop[0], gfxs->YCop, gfxs->length );

     if (gfxs->AopY & 1) {
          int  w   = (gfxs->length >> 1) + 1;
          u16 *D   =  gfxs->Aop[1];
          u16  Cop =  gfxs->CbCop | (gfxs->CrCop << 8);

          while (--w)
               *D++ = Cop;
     }
}

static void
Cop_to_Aop_nv16( GenefxState *gfxs )
{
     int  w   = (gfxs->length >> 1) + 1;
     u16 *D   =  gfxs->Aop[1];
     u16  Cop =  gfxs->CbCop | (gfxs->CrCop << 8);

     memset( gfxs->Aop[0], gfxs->YCop, gfxs->length );

     while (--w)
          *D++ = Cop;
}

static void
Cop_to_Aop_nv21( GenefxState *gfxs )
{
     memset( gfxs->Aop[0], gfxs->YCop, gfxs->length );

     if (gfxs->AopY & 1) {
          int  w   = (gfxs->length >> 1) + 1;
          u16 *D   =  gfxs->Aop[1];
          u16  Cop =  gfxs->CrCop | (gfxs->CbCop << 8);

          while (--w)
               *D++ = Cop;
     }
}

static void
Cop_to_Aop_18( GenefxState *gfxs )
{
     int  w   = gfxs->length + 1;
     u8  *D   = gfxs->Aop[0];
     u32  Cop = gfxs->Cop;

     while (--w) {
          D[0] = Cop;
          D[1] = Cop >> 8;
          D[2] = Cop >> 16;

          D += 3;
     }
}

static void
Cop_to_Aop_y444( GenefxState *gfxs )
{
     memset( gfxs->Aop[0], gfxs->YCop,  gfxs->length );
     memset( gfxs->Aop[1], gfxs->CbCop, gfxs->length );
     memset( gfxs->Aop[2], gfxs->CrCop, gfxs->length );
}

static void
Cop_to_Aop_argb8565( GenefxState *gfxs )
{
     int  w   = gfxs->length + 1;
     u8  *D   = gfxs->Aop[0];
     u32  Cop = gfxs->Cop;

     while (--w) {
#ifdef WORDS_BIGENDIAN
          D[0] = (Cop >> 16) & 0xff;
          D[1] = (Cop >>  8) & 0xff;
          D[2] =  Cop        & 0xff;
#else
          D[0] =  Cop        & 0xff;
          D[1] = (Cop >>  8) & 0xff;
          D[2] = (Cop >> 16) & 0xff;
#endif

          D += 3;
     }
}

static void
Cop_to_Aop_vyu( GenefxState *gfxs )
{
     int  w = gfxs->length + 1;
     u8  *D = gfxs->Aop[0];

     while (--w) {
#ifdef WORDS_BIGENDIAN
          D[0] = gfxs->CrCop;
          D[1] = gfxs->YCop;
          D[2] = gfxs->CbCop;
#else
          D[0] = gfxs->CbCop;
          D[1] = gfxs->YCop;
          D[2] = gfxs->CrCop;
#endif

          D += 3;
     }
}

static void
Cop_to_Aop_y42b( GenefxState *gfxs )
{
     memset( gfxs->Aop[0], gfxs->YCop,  gfxs->length );
     memset( gfxs->Aop[1], gfxs->CbCop, gfxs->length / 2 );
     memset( gfxs->Aop[2], gfxs->CrCop, gfxs->length / 2 );
}

static void
Cop_to_Aop_nv61( GenefxState *gfxs )
{
     int  w   = (gfxs->length >> 1) + 1;
     u16 *D   =  gfxs->Aop[1];
     u16  Cop =  gfxs->CrCop | (gfxs->CbCop << 8);

     memset( gfxs->Aop[0], gfxs->YCop, gfxs->length );

     while (--w)
          *D++ = Cop;
}

static void
Cop_to_Aop_nv24( GenefxState *gfxs )
{
     int  w   = gfxs->length + 1;
     u16 *D   = gfxs->Aop[1];
     u16  Cop = gfxs->CbCop | (gfxs->CrCop << 8);

     memset( gfxs->Aop[0], gfxs->YCop, gfxs->length );

     while (--w)
          *D++ = Cop;
}

static void
Cop_to_Aop_nv42( GenefxState *gfxs )
{
     int  w   = gfxs->length + 1;
     u16 *D   = gfxs->Aop[1];
     u16  Cop = gfxs->CrCop | (gfxs->CbCop << 8);

     memset( gfxs->Aop[0], gfxs->YCop, gfxs->length );

     while (--w)
          *D++ = Cop;
}

static GenefxFunc Cop_to_Aop_PFI[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1555)]   = Cop_to_Aop_16,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB16)]      = Cop_to_Aop_16,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB24)]      = Cop_to_Aop_24,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB32)]      = Cop_to_Aop_32,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]       = Cop_to_Aop_32,
     [DFB_PIXELFORMAT_INDEX(DSPF_A8)]         = Cop_to_Aop_8,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUY2)]       = Cop_to_Aop_yuv422,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB332)]     = Cop_to_Aop_8,
     [DFB_PIXELFORMAT_INDEX(DSPF_UYVY)]       = Cop_to_Aop_yuv422,
     [DFB_PIXELFORMAT_INDEX(DSPF_I420)]       = Cop_to_Aop_i420,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV12)]       = Cop_to_Aop_i420,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT8)]       = Cop_to_Aop_8,
     [DFB_PIXELFORMAT_INDEX(DSPF_ALUT44)]     = Cop_to_Aop_8,
     [DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)]      = Cop_to_Aop_32,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV12)]       = Cop_to_Aop_nv12,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV16)]       = Cop_to_Aop_nv16,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB2554)]   = Cop_to_Aop_16,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)]   = Cop_to_Aop_16,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA4444)]   = Cop_to_Aop_16,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV21)]       = Cop_to_Aop_nv21,
     [DFB_PIXELFORMAT_INDEX(DSPF_AYUV)]       = Cop_to_Aop_32,
     [DFB_PIXELFORMAT_INDEX(DSPF_A4)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1666)]   = Cop_to_Aop_18,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB6666)]   = Cop_to_Aop_18,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB18)]      = Cop_to_Aop_18,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB444)]     = Cop_to_Aop_16,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB555)]     = Cop_to_Aop_16,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR555)]     = Cop_to_Aop_16,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA5551)]   = Cop_to_Aop_16,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y444)]       = Cop_to_Aop_y444,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB8565)]   = Cop_to_Aop_argb8565,
     [DFB_PIXELFORMAT_INDEX(DSPF_AVYU)]       = Cop_to_Aop_32,
     [DFB_PIXELFORMAT_INDEX(DSPF_VYU)]        = Cop_to_Aop_vyu,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1_LSB)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV16)]       = Cop_to_Aop_y42b,
     [DFB_PIXELFORMAT_INDEX(DSPF_ABGR)]       = Cop_to_Aop_32,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBAF88871)] = Cop_to_Aop_32,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT1)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV61)]       = Cop_to_Aop_nv61,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y42B)]       = Cop_to_Aop_y42b,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV24)]       = Cop_to_Aop_y444,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV24)]       = Cop_to_Aop_nv24,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV42)]       = Cop_to_Aop_nv42,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR24)]      = Cop_to_Aop_24,
};

/**********************************************************************************************************************
 ********************************* Cop_toK_Aop_PFI ********************************************************************
 **********************************************************************************************************************/

static void
Cop_toK_Aop_8( GenefxState *gfxs )
{
     int  w    = gfxs->length + 1;
     u8  *D    = gfxs->Aop[0];
     u32  Cop  = gfxs->Cop;
     u32  Dkey = gfxs->Dkey;

     while (--w) {
          if (Dkey == *D)
               *D = Cop;

          ++D;
     }
}

static void
Cop_toK_Aop_yuv422( GenefxState *gfxs )
{
     int  l;
     int  w    = gfxs->length;
     u16 *D    = gfxs->Aop[0];
     u32  Cop  = gfxs->Cop;
     u32  Dkey = gfxs->Dkey;

     if ((long) D & 2) {
#ifdef WORDS_BIGENDIAN
          if (*D == (Dkey & 0xffff))
               *D = Cop & 0xffff;
#else
          if (*D == (Dkey >> 16))
               *D = Cop >> 16;
#endif

          ++D;
          --w;
     }

     for (l = w >> 1; l--;) {
          if (*((u32*) D) == Dkey)
               *((u32*) D) = Cop;

          D += 2;
     }

     if (w & 1) {
#ifdef WORDS_BIGENDIAN
          if (*D == (Dkey >> 16))
               *D = Cop >> 16;
#else
          if (*D == (Dkey & 0xffff))
               *D = Cop & 0xffff;
#endif
     }
}

static void
Cop_toK_Aop_alut44( GenefxState *gfxs )
{
     int  w    = gfxs->length + 1;
     u8  *D    = gfxs->Aop[0];
     u32  Cop  = gfxs->Cop;
     u32  Dkey = gfxs->Dkey;

     while (--w) {
          if (Dkey == (*D & 0x0f))
               *D = Cop;

          ++D;
     }
}

static void
Cop_toK_Aop_y444( GenefxState *gfxs )
{
     int  w    = gfxs->length + 1;
     u8  *Dy   = gfxs->Aop[0];
     u8  *Du   = gfxs->Aop[1];
     u8  *Dv   = gfxs->Aop[2];
     u32  Cop  = gfxs->Cop;
     u32  Dkey = gfxs->Dkey;

     while (--w) {
          u8 dy = *Dy;
          u8 du = *Du;
          u8 dv = *Dv;

          if (Dkey == (u32) ((dy << 16) | (du << 8) | dv)) {
               *Dy = (Cop >> 16) & 0xff;
               *Du = (Cop >>  8) & 0xff;
               *Dv =  Cop        & 0xff;
          }

          ++Dy; ++Du; ++Dv;
     }
}

static void
Cop_toK_Aop_avyu( GenefxState *gfxs )
{
     int  w    = gfxs->length + 1;
     u32 *D    = gfxs->Aop[0];
     u32  Cop  = gfxs->Cop;
     u32  Dkey = gfxs->Dkey;

     while (--w) {
          if ((*D & 0x00ffffff) == Dkey)
               *D = Cop;

          ++D;
     }
}

static GenefxFunc Cop_toK_Aop_PFI[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1555)]   = Cop_toK_Aop_15,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB16)]      = Cop_toK_Aop_16,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB24)]      = Cop_toK_Aop_24_24,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB32)]      = Cop_toK_Aop_32,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]       = Cop_toK_Aop_32,
     [DFB_PIXELFORMAT_INDEX(DSPF_A8)]         = Cop_toK_Aop_8,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUY2)]       = Cop_toK_Aop_yuv422,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB332)]     = Cop_toK_Aop_8,
     [DFB_PIXELFORMAT_INDEX(DSPF_UYVY)]       = Cop_toK_Aop_yuv422,
     [DFB_PIXELFORMAT_INDEX(DSPF_I420)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT8)]       = Cop_toK_Aop_8,
     [DFB_PIXELFORMAT_INDEX(DSPF_ALUT44)]     = Cop_toK_Aop_alut44,
     [DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)]      = Cop_toK_Aop_32,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB2554)]   = Cop_toK_Aop_14,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)]   = Cop_toK_Aop_12,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA4444)]   = Cop_toK_Aop_12vv,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV21)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AYUV)]       = Cop_toK_Aop_32,
     [DFB_PIXELFORMAT_INDEX(DSPF_A4)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1666)]   = Cop_toK_Aop_24_18,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB6666)]   = Cop_toK_Aop_24_18,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB18)]      = Cop_toK_Aop_24_18,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB444)]     = Cop_toK_Aop_12,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB555)]     = Cop_toK_Aop_15,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR555)]     = Cop_toK_Aop_15,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA5551)]   = Cop_toK_Aop_15,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y444)]       = Cop_toK_Aop_y444,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB8565)]   = Cop_toK_Aop_24_16,
     [DFB_PIXELFORMAT_INDEX(DSPF_AVYU)]       = Cop_toK_Aop_avyu,
     [DFB_PIXELFORMAT_INDEX(DSPF_VYU)]        = Cop_toK_Aop_24_24,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1_LSB)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ABGR)]       = Cop_toK_Aop_32,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBAF88871)] = Cop_toK_Aop_32_24,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT1)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV61)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y42B)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV24)]       = Cop_toK_Aop_y444,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV24)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV42)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR24)]      = Cop_toK_Aop_24_24,
};

/**********************************************************************************************************************
 ********************************* Sop_PFI_to_Dacc ********************************************************************
 **********************************************************************************************************************/

static void
Sop_a8_to_Dacc( GenefxState *gfxs )
{
     int                w = gfxs->length + 1;
     u8                *S = gfxs->Sop[0];
     GenefxAccumulator *D = gfxs->Dacc;

     while (--w) {
          D->RGB.a = *S++;
          D->RGB.r = 0xff;
          D->RGB.g = 0xff;
          D->RGB.b = 0xff;

          ++D;
     }
}

static void
Sop_yuy2_to_Dacc( GenefxState *gfxs )
{
     int                w = (gfxs->length >> 1) + 1;
     u32               *S =  gfxs->Sop[0];
     GenefxAccumulator *D =  gfxs->Dacc;

     while (--w) {
          u32 s = *S++;

          D[0].YUV.a = D[1].YUV.a = 0xff;
#ifdef WORDS_BIGENDIAN
          D[0].YUV.y =              (s & 0x00ff0000) >> 16;
          D[1].YUV.y =               s & 0x000000ff;
          D[0].YUV.u = D[1].YUV.u = (s & 0xff000000) >> 24;
          D[0].YUV.v = D[1].YUV.v = (s & 0x0000ff00) >>  8;
#else
          D[0].YUV.y =               s & 0x000000ff;
          D[1].YUV.y =              (s & 0x00ff0000) >> 16;
          D[0].YUV.u = D[1].YUV.u = (s & 0x0000ff00) >>  8;
          D[0].YUV.v = D[1].YUV.v = (s & 0xff000000) >> 24;
#endif

          D += 2;
     }

     if (gfxs->length & 1) {
          u16 s = *((u16*) S);

          D->YUV.a = 0xff;
          D->YUV.y = s & 0xff;
          D->YUV.u = s >> 8;
          D->YUV.v = 0x00;
     }
}

static void
Sop_rgb332_to_Dacc( GenefxState *gfxs )
{
     int                w = gfxs->length + 1;
     u8                *S = gfxs->Sop[0];
     GenefxAccumulator *D = gfxs->Dacc;

     while (--w) {
          u8 s = *S++;

          D->RGB.a = 0xff;
          D->RGB.r = EXPAND_3to8(  s >> 5         );
          D->RGB.g = EXPAND_3to8( (s & 0x1c) >> 2 );
          D->RGB.b = EXPAND_2to8(  s & 0x03       );

          ++D;
     }
}

static void
Sop_uyvy_to_Dacc( GenefxState *gfxs )
{
     int                w = (gfxs->length >> 1) + 1;
     u32               *S =  gfxs->Sop[0];
     GenefxAccumulator *D =  gfxs->Dacc;

     while (--w) {
          u32 s = *S++;

          D[0].YUV.a = D[1].YUV.a = 0xff;
#ifdef WORDS_BIGENDIAN
          D[0].YUV.y =              (s & 0xff000000) >> 24;
          D[1].YUV.y =              (s & 0x0000ff00) >> 8;
          D[0].YUV.u = D[1].YUV.u = (s & 0x00ff0000) >> 16;
          D[0].YUV.v = D[1].YUV.v =  s & 0x000000ff;
#else
          D[0].YUV.y =              (s & 0x0000ff00) >> 8;
          D[1].YUV.y =              (s & 0xff000000) >> 24;
          D[0].YUV.u = D[1].YUV.u =  s & 0x000000ff;
          D[0].YUV.v = D[1].YUV.v = (s & 0x00ff0000) >> 16;
#endif

          D += 2;
     }

     if (gfxs->length & 1) {
          u16 s = *((u16*) S);

          D->YUV.a = 0xff;
          D->YUV.y = s >> 8;
          D->YUV.u = s & 0xff;
          D->YUV.v = 0x00;
     }
}

static void
Sop_i420_to_Dacc( GenefxState *gfxs )
{
    int                 w  = (gfxs->length >> 1) + 1;
     u8                *Sy =  gfxs->Sop[0];
     u8                *Su =  gfxs->Sop[1];
     u8                *Sv =  gfxs->Sop[2];
     GenefxAccumulator *D  =  gfxs->Dacc;

     while (--w) {
          D[1].YUV.a = D[0].YUV.a = 0xff;
          D[0].YUV.y = Sy[0];
          D[1].YUV.y = Sy[1];
          D[1].YUV.u = D[0].YUV.u = Su[0];
          D[1].YUV.v = D[0].YUV.v = Sv[0];

          Sy += 2; ++Su; ++Sv;
          D  += 2;
     }
}

static void
Sop_lut8_to_Dacc( GenefxState *gfxs )
{
     int                w       = gfxs->length + 1;
     u8                *S       = gfxs->Sop[0];
     GenefxAccumulator *D       = gfxs->Dacc;
     DFBColor          *entries = gfxs->Slut->entries;

     while (--w) {
          u8 s = *S++;

          D->RGB.a = entries[s].a;
          D->RGB.r = entries[s].r;
          D->RGB.g = entries[s].g;
          D->RGB.b = entries[s].b;

          ++D;
     }
}

static void
Sop_alut44_to_Dacc( GenefxState *gfxs )
{
     int                w       = gfxs->length + 1;
     u8                *S       = gfxs->Sop[0];
     GenefxAccumulator *D       = gfxs->Dacc;
     DFBColor          *entries = gfxs->Slut->entries;

     while (--w) {
          u8 s = *S++;

          D->RGB.a = s & 0xf0;
          s &= 0x0f;
          D->RGB.r = entries[s].r;
          D->RGB.g = entries[s].g;
          D->RGB.b = entries[s].b;

          ++D;
     }
}

static void
Sop_nv12_to_Dacc( GenefxState *gfxs )
{
     int                w   = (gfxs->length >> 1) + 1;
     u8                *Sy  =  gfxs->Sop[0];
     u16               *Suv =  gfxs->Sop[1];
     GenefxAccumulator *D   =  gfxs->Dacc;

     while (--w) {
          D[1].YUV.a = D[0].YUV.a = 0xff;
          D[0].YUV.y = Sy[0];
          D[1].YUV.y = Sy[1];
          D[1].YUV.u = D[0].YUV.u = Suv[0] & 0xff;
          D[1].YUV.v = D[0].YUV.v = Suv[0] >> 8;

          Sy += 2; ++Suv;
          D  += 2;
     }
}

static void
Sop_nv21_to_Dacc( GenefxState *gfxs )
{
     int                w   = (gfxs->length >> 1) + 1;
     u8                *Sy  =  gfxs->Sop[0];
     u16               *Svu =  gfxs->Sop[1];
     GenefxAccumulator *D   =  gfxs->Dacc;

     while (--w) {
          D[1].YUV.a = D[0].YUV.a = 0xff;
          D[0].YUV.y = Sy[0];
          D[1].YUV.y = Sy[1];
          D[1].YUV.u = D[0].YUV.u = Svu[0] >> 8;
          D[1].YUV.v = D[0].YUV.v = Svu[0] & 0xff;

          Sy += 2; ++Svu;
          D  += 2;
     }
}

static void
Sop_ayuv_to_Dacc( GenefxState *gfxs )
{
     int                w = gfxs->length + 1;
     u32               *S = gfxs->Sop[0];
     GenefxAccumulator *D = gfxs->Dacc;

     while (--w) {
          u32 s = *S++;

          D->YUV.a =  s >> 24;
          D->YUV.y = (s >> 16) & 0xff;
          D->YUV.u = (s >>  8) & 0xff;
          D->YUV.v =  s        & 0xff;

          ++D;
     }
}

static void
Sop_a4_to_Dacc( GenefxState *gfxs )
{
     int                i, n;
     u8                *S = gfxs->Sop[0];
     GenefxAccumulator *D = gfxs->Dacc;

     for (i = 0, n = 0; i < gfxs->length; i += 2, n++) {
          int left  = S[n] & 0xf0;
          int right = S[n] & 0x0f;

          D[i].RGB.a = left | (left >> 4);
          D[i].RGB.r = 0xff;
          D[i].RGB.g = 0xff;
          D[i].RGB.b = 0xff;

          D[i+1].RGB.a = right | (right << 4);
          D[i+1].RGB.r = 0xff;
          D[i+1].RGB.g = 0xff;
          D[i+1].RGB.b = 0xff;
     }
}

static void
Sop_y444_to_Dacc( GenefxState *gfxs )
{
     int                w  = gfxs->length + 1;
     u8                *Sy = gfxs->Sop[0];
     u8                *Su = gfxs->Sop[1];
     u8                *Sv = gfxs->Sop[2];
     GenefxAccumulator *D  = gfxs->Dacc;

     while (--w) {
          D->YUV.a = 0xff;
          D->YUV.y = *Sy++;
          D->YUV.u = *Su++;
          D->YUV.v = *Sv++;

          ++D;
     }
}

static void
Sop_avyu_to_Dacc( GenefxState *gfxs )
{
     int                w = gfxs->length + 1;
     u32               *S = gfxs->Sop[0];
     GenefxAccumulator *D = gfxs->Dacc;

     while (--w) {
          u32 s = *S++;

          D->YUV.a =  s >> 24;
          D->YUV.v = (s >> 16) & 0xff;
          D->YUV.y = (s >>  8) & 0xff;
          D->YUV.u =  s        & 0xff;

          ++D;
     }
}

static void
Sop_vyu_to_Dacc( GenefxState *gfxs )
{
     int                w = gfxs->length + 1;
     u8                *S = gfxs->Sop[0];
     GenefxAccumulator *D = gfxs->Dacc;

     while (--w) {
#ifdef WORDS_BIGENDIAN
          D->YUV.a = 0xff;
          D->YUV.v = S[0];
          D->YUV.y = S[1];
          D->YUV.u = S[2];
#else
          D->YUV.a = 0xff;
          D->YUV.v = S[2];
          D->YUV.y = S[1];
          D->YUV.u = S[0];
#endif

          S += 3;
          ++D;
     }
}

static void
Sop_nv24_to_Dacc( GenefxState *gfxs )
{
     int                w   = gfxs->length + 1;
     u8                *Sy  = gfxs->Sop[0];
     u16               *Suv = gfxs->Sop[1];
     GenefxAccumulator *D   = gfxs->Dacc;

     while (--w) {
          D[0].YUV.a = 0xff;
          D[0].YUV.y = *Sy++;
          D[0].YUV.u = Suv[0] & 0xff;
          D[0].YUV.v = Suv[0] >> 8;

          ++Suv;
          D++;
     }
}

static void
Sop_nv42_to_Dacc( GenefxState *gfxs )
{
     int                w   = gfxs->length + 1;
     u8                *Sy  = gfxs->Sop[0];
     u16               *Svu = gfxs->Sop[1];
     GenefxAccumulator *D   = gfxs->Dacc;

     while (--w) {
          D[0].YUV.a = 0xff;
          D[0].YUV.y = *Sy++;
          D[0].YUV.u = Svu[0] >> 8;
          D[0].YUV.v = Svu[0] & 0xff;

          ++Svu;
          D++;
     }
}

static GenefxFunc Sop_PFI_to_Dacc[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1555)]   = Sop_argb1555_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB16)]      = Sop_rgb16_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB24)]      = Sop_rgb24_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB32)]      = Sop_rgb32_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]       = Sop_argb_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_A8)]         = Sop_a8_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUY2)]       = Sop_yuy2_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB332)]     = Sop_rgb332_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_UYVY)]       = Sop_uyvy_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_I420)]       = Sop_i420_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV12)]       = Sop_i420_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT8)]       = Sop_lut8_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_ALUT44)]     = Sop_alut44_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)]      = Sop_airgb_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV12)]       = Sop_nv12_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV16)]       = Sop_nv12_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB2554)]   = Sop_argb2554_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)]   = Sop_argb4444_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA4444)]   = Sop_rgba4444_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV21)]       = Sop_nv21_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_AYUV)]       = Sop_ayuv_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_A4)]         = Sop_a4_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1666)]   = Sop_argb1666_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB6666)]   = Sop_argb6666_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB18)]      = Sop_rgb18_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB444)]     = Sop_xrgb4444_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB555)]     = Sop_xrgb1555_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR555)]     = Sop_xbgr1555_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA5551)]   = Sop_rgba5551_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y444)]       = Sop_y444_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB8565)]   = Sop_argb8565_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_AVYU)]       = Sop_avyu_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_VYU)]        = Sop_vyu_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1_LSB)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV16)]       = Sop_i420_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_ABGR)]       = Sop_abgr_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBAF88871)] = Sop_rgbaf88871_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT1)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV61)]       = Sop_nv21_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y42B)]       = Sop_i420_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV24)]       = Sop_y444_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV24)]       = Sop_nv24_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV42)]       = Sop_nv42_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR24)]      = Sop_bgr24_to_Dacc,
};

/**********************************************************************************************************************
 ********************************* Sop_PFI_Kto_Dacc *******************************************************************
 **********************************************************************************************************************/

static void
Sop_a8_Kto_Dacc( GenefxState *gfxs )
{
     int                w = gfxs->length + 1;
     u8                *S = gfxs->Sop[0];
     GenefxAccumulator *D = gfxs->Dacc;

     /* No color to key. */
     while (--w) {
          D->RGB.a = *S++;
          D->RGB.r = 0xff;
          D->RGB.g = 0xff;
          D->RGB.b = 0xff;

          ++D;
     }
}

static void
Sop_yuy2_Kto_Dacc( GenefxState *gfxs )
{
     int                w     = (gfxs->length >> 1) + 1;
     u32               *S     =  gfxs->Sop[0];
     GenefxAccumulator *D     =  gfxs->Dacc;
     u32                Skey  =  gfxs->Skey;
     u32                Skey0 =  gfxs->Skey & 0xff00ffff;
     u32                Skey1 =  gfxs->Skey & 0xffffff00;

#ifdef WORDS_BIGENDIAN
#define S0_MASK  0xffffff00
#define S1_MASK  0xff00ffff
#else
#define S0_MASK  0xff00ffff
#define S1_MASK  0xffffff00
#endif

     while (--w) {
          u32 s = *S++;

          if (s != Skey) {
               u32 cb, cr;

#ifdef WORDS_BIGENDIAN
               cb = (s & 0xff000000) >> 24;
               cr = (s & 0x0000ff00) >>  8;
#else
               cb = (s & 0x0000ff00) >>  8;
               cr = (s & 0xff000000) >> 24;
#endif

               if ((s & S0_MASK) != Skey0) {
                    D[0].YUV.a = 0xff;
#ifdef WORDS_BIGENDIAN
                    D[0].YUV.y = (s & 0x00ff0000) >> 16;
#else
                    D[0].YUV.y =  s & 0x000000ff;
#endif
                    D[0].YUV.u = cb;
                    D[0].YUV.v = cr;
               }
               else
                    D[0].YUV.a = 0xf000;

               if ((s & S1_MASK) != Skey1) {
                    D[1].YUV.a = 0xff;
#ifdef WORDS_BIGENDIAN
                    D[1].YUV.y =  s & 0x000000ff;
#else
                    D[1].YUV.y = (s & 0x00ff0000) >> 16;
#endif
                    D[1].YUV.u = cb;
                    D[1].YUV.v = cr;
               }
               else
                    D[1].YUV.a = 0xf000;
          }

          D += 2;
     }

     if (gfxs->length & 1) {
          u16 s = *((u16*) S);

          if (s != Skey0) {
               D->YUV.a = 0xff;
               D->YUV.y = s & 0xff;
               D->YUV.u = s >> 8;
               D->YUV.v = 0x00;
          }
          else
               D->YUV.a = 0xf000;
     }

#undef S0_MASK
#undef S1_MASK
}

static void
Sop_rgb332_Kto_Dacc( GenefxState *gfxs )
{
     int                w    = gfxs->length + 1;
     u8                *S    = gfxs->Sop[0];
     GenefxAccumulator *D    = gfxs->Dacc;
     u32                Skey = gfxs->Skey;

     while (--w) {
          u8 s = *S++;

          if (s != Skey) {
               D->RGB.a = 0xff;
               D->RGB.r = EXPAND_3to8(  s >> 5         );
               D->RGB.g = EXPAND_3to8( (s & 0x1c) >> 2 );
               D->RGB.b = EXPAND_2to8(  s & 0x03       );
          }
          else
               D->RGB.a = 0xf000;

          ++D;
     }
}

static void
Sop_uyvy_Kto_Dacc( GenefxState *gfxs )
{
     int                w     = (gfxs->length >> 1) + 1;
     u32               *S     =  gfxs->Sop[0];
     GenefxAccumulator *D     =  gfxs->Dacc;
     u32                Skey  =  gfxs->Skey;
     u32                Skey0 =  gfxs->Skey & 0x00ffffff;
     u32                Skey1 =  gfxs->Skey & 0xffff00ff;

#ifdef WORDS_BIGENDIAN
#define S0_MASK 0xffff00ff
#define S1_MASK 0x00ffffff
#else
#define S0_MASK 0x00ffffff
#define S1_MASK 0xffff00ff
#endif

     while (--w) {
          u32 s = *S++;

          if (s != Skey) {
               u32 cb, cr;

#ifdef WORDS_BIGENDIAN
               cb = (s & 0x00ff0000) >> 16;
               cr =  s & 0x000000ff;
#else
               cb =  s & 0x000000ff;
               cr = (s & 0x00ff0000) >> 16;
#endif

               if ((s & S0_MASK) != Skey0) {
                    D[0].YUV.a = 0xff;
#ifdef WORDS_BIGENDIAN
                    D[0].YUV.y = (s & 0xff000000) >> 24;
#else
                    D[0].YUV.y = (s & 0x0000ff00) >>  8;
#endif
                    D[0].YUV.u = cb;
                    D[0].YUV.v = cr;
               }
               else
                    D[0].YUV.a = 0xf000;

               if ((s & S1_MASK) != Skey1) {
                    D[1].YUV.a = 0xff;
#ifdef WORDS_BIGENDIAN
                    D[1].YUV.y = (s & 0x0000ff00) >> 8;
#else
                    D[1].YUV.y = (s & 0xff000000) >>24;
#endif
                    D[1].YUV.u = cb;
                    D[1].YUV.v = cr;
               }
               else
                    D[1].YUV.a = 0xf000;
          }

          D += 2;
     }

     if (gfxs->length & 1) {
          u16 s = *((u16*) S);

          if (s != Skey0) {
               D->YUV.a = 0xff;
               D->YUV.y = s >> 8;
               D->YUV.u = s & 0xff;
               D->YUV.v = 0x00;
          }
          else
               D->YUV.a = 0xf000;
     }

#undef S0_MASK
#undef S1_MASK
}

static void
Sop_lut8_Kto_Dacc( GenefxState *gfxs )
{
     int                w       = gfxs->length + 1;
     u8                *S       = gfxs->Sop[0];
     GenefxAccumulator *D       = gfxs->Dacc;
     u32                Skey    = gfxs->Skey;
     DFBColor          *entries = gfxs->Slut->entries;

     while (--w) {
          u8 s = *S++;

          if (s != Skey) {
               D->RGB.a = entries[s].a;
               D->RGB.r = entries[s].r;
               D->RGB.g = entries[s].g;
               D->RGB.b = entries[s].b;
          }
          else
               D->RGB.a = 0xf000;

          ++D;
     }
}

static void
Sop_alut44_Kto_Dacc( GenefxState *gfxs )
{
     int                w       = gfxs->length + 1;
     u8                *S       = gfxs->Sop[0];
     GenefxAccumulator *D       = gfxs->Dacc;
     u32                Skey    = gfxs->Skey;
     DFBColor          *entries = gfxs->Slut->entries;

     while (--w) {
          u8 s = *S++;

          if ((s & 0x0f) != Skey) {
               D->RGB.a = ((s & 0xf0) >> 4) | (s & 0xf0);
               s &= 0x0f;
               D->RGB.r = entries[s].r;
               D->RGB.g = entries[s].g;
               D->RGB.b = entries[s].b;
          }
          else
               D->RGB.a = 0xf000;

          ++D;
     }
}

static void
Sop_y444_Kto_Dacc( GenefxState *gfxs )
{
     int                w    = gfxs->length + 1;
     u8                *Sy   = gfxs->Sop[0];
     u8                *Su   = gfxs->Sop[1];
     u8                *Sv   = gfxs->Sop[2];
     GenefxAccumulator *D    = gfxs->Dacc;
     u32                Skey = gfxs->Skey;

     while (--w) {
          u8 sy = *Sy++;
          u8 su = *Su++;
          u8 sv = *Sv++;

          if (Skey != (u32) ((sy << 16) | (su << 8) | sv)) {
               D->YUV.a = 0xff;
               D->YUV.y = sy;
               D->YUV.u = su;
               D->YUV.v = sv;
          }
          else
               D->YUV.a = 0xf000;

          ++D;
     }
}

static void
Sop_avyu_Kto_Dacc( GenefxState *gfxs )
{
     int                w    = gfxs->length + 1;
     u32               *S    = gfxs->Sop[0];
     GenefxAccumulator *D    = gfxs->Dacc;
     u32                Skey = gfxs->Skey;

     while (--w) {
          u32 s = *S++;

          if ((s & 0x00ffffff) != Skey) {
               D->YUV.a = (s & 0xff000000) >> 24;
               D->YUV.v = (s & 0x00ff0000) >> 16;
               D->YUV.y = (s & 0x0000ff00) >>  8;
               D->YUV.u =  s & 0x000000ff;
          }
          else
               D->YUV.a = 0xf000;

          ++D;
     }
}

static void
Sop_vyu_Kto_Dacc( GenefxState *gfxs )
{
     int                w    = gfxs->length + 1;
     u8                *S    = gfxs->Sop[0];
     GenefxAccumulator *D    = gfxs->Dacc;
     u32                Skey = gfxs->Skey;

     while (--w) {
#ifdef WORDS_BIGENDIAN
          u32 s = S[0] << 16 | S[1] << 8 | S[2];
#else
          u32 s = S[2] << 16 | S[1] << 8 | S[0];
#endif

          if (Skey != s) {
#ifdef WORDS_BIGENDIAN
               D->YUV.a = 0xff;
               D->YUV.v = S[0];
               D->YUV.y = S[1];
               D->YUV.u = S[2];
#else
               D->YUV.a = 0xff;
               D->YUV.v = S[2];
               D->YUV.y = S[1];
               D->YUV.u = S[0];
#endif
          }
          else
               D->YUV.a = 0xf000;

          S += 3;
          ++D;
     }
}

static GenefxFunc Sop_PFI_Kto_Dacc[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1555)]   = Sop_argb1555_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB16)]      = Sop_rgb16_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB24)]      = Sop_rgb24_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB32)]      = Sop_rgb32_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]       = Sop_argb_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_A8)]         = Sop_a8_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUY2)]       = Sop_yuy2_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB332)]     = Sop_rgb332_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_UYVY)]       = Sop_uyvy_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_I420)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT8)]       = Sop_lut8_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_ALUT44)]     = Sop_alut44_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)]      = Sop_airgb_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB2554)]   = Sop_argb2554_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)]   = Sop_argb4444_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA4444)]   = Sop_rgba4444_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV21)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AYUV)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A4)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1666)]   = Sop_argb6666_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB6666)]   = Sop_argb1666_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB18)]      = Sop_rgb18_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB444)]     = Sop_xrgb4444_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB555)]     = Sop_xrgb1555_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR555)]     = Sop_xbgr1555_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA5551)]   = Sop_rgba5551_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y444)]       = Sop_y444_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB8565)]   = Sop_argb8565_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_AVYU)]       = Sop_avyu_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_VYU)]        = Sop_vyu_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1_LSB)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ABGR)]       = Sop_abgr_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBAF88871)] = Sop_rgbaf88871_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT1)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV61)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y42B)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV24)]       = Sop_y444_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV24)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV42)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR24)]      = Sop_bgr24_Kto_Dacc,
};

/**********************************************************************************************************************
 ********************************* Sop_PFI_Sto_Dacc *******************************************************************
 **********************************************************************************************************************/

static void
Sop_a8_Sto_Dacc( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     u8                *S     = gfxs->Sop[0];
     GenefxAccumulator *D     = gfxs->Dacc;
     int                SperD = gfxs->SperD;

     while (--w) {
          u8 s = S[i>>16];

          D->RGB.a = s;
          D->RGB.r = 0xff;
          D->RGB.g = 0xff;
          D->RGB.b = 0xff;

          ++D;
          i += SperD;
     }
}

static void
Sop_yuy2_Sto_Dacc( GenefxState *gfxs )
{
     int                i     =  gfxs->Xphase;
     int                w     = (gfxs->length >> 1) + 1;
     u32               *S     =  gfxs->Sop[0];
     GenefxAccumulator *D     =  gfxs->Dacc;
     int                SperD =  gfxs->SperD;

     while (--w) {
          u32 s = S[i>>17];

          D[0].YUV.a = D[1].YUV.a = 0xff;
#ifdef WORDS_BIGENDIAN
          D[0].YUV.u = D[1].YUV.u = (s & 0xff000000) >> 24;
          D[0].YUV.v = D[1].YUV.v = (s & 0x0000ff00) >>  8;
#else
          D[0].YUV.u = D[1].YUV.u = (s & 0x0000ff00) >>  8;
          D[0].YUV.v = D[1].YUV.v = (s & 0xff000000) >> 24;
#endif
          D[0].YUV.y = ((u16*) S)[i>>16]         & 0x00ff;
          D[1].YUV.y = ((u16*) S)[(i+SperD)>>16] & 0x00ff;

          D += 2;
          i += SperD << 1;
     }

     if (gfxs->length & 1) {
          u16 s = ((u16*) S)[i>>17];

          D->YUV.a = 0xff;
          D->YUV.y = s & 0xff;
          D->YUV.u = s >> 8;
          D->YUV.v = 0x00;
     }
}

static void
Sop_rgb332_Sto_Dacc( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     u8                *S     = gfxs->Sop[0];
     GenefxAccumulator *D     = gfxs->Dacc;
     int                SperD = gfxs->SperD;

     while (--w) {
          u8 s = S[i>>16];

          D->RGB.a = 0xff;
          D->RGB.r = EXPAND_3to8(  s >> 5         );
          D->RGB.g = EXPAND_3to8( (s & 0x1c) >> 2 );
          D->RGB.b = EXPAND_2to8(  s & 0x03       );

          ++D;
          i += SperD;
     }
}

static void
Sop_uyvy_Sto_Dacc( GenefxState *gfxs )
{
     int                i     =  gfxs->Xphase;
     int                w     = (gfxs->length >> 1) + 1;
     u32               *S     =  gfxs->Sop[0];
     GenefxAccumulator *D     =  gfxs->Dacc;
     int                SperD =  gfxs->SperD;

     while (--w) {
          u32 s = S[i>>17];

          D[0].YUV.a = D[1].YUV.a = 0xff;
#ifdef WORDS_BIGENDIAN
          D[0].YUV.u = D[1].YUV.u = (s & 0x00ff0000) >> 16;
          D[0].YUV.v = D[1].YUV.v =  s & 0x000000ff;
#else
          D[0].YUV.u = D[1].YUV.u =  s & 0x000000ff;
          D[0].YUV.v = D[1].YUV.v = (s & 0x00ff0000) >> 16;
#endif
          D[0].YUV.y = (((u16*) S)[i>>16]         & 0xff00) >> 8;
          D[1].YUV.y = (((u16*) S)[(i+SperD)>>16] & 0xff00) >> 8;

          D += 2;
          i += SperD << 1;
     }

     if (gfxs->length & 1) {
          u16 s = ((u16*) S)[i>>16];

          D->YUV.a = 0xff;
          D->YUV.y = s >> 8;
          D->YUV.u = s & 0xff;
          D->YUV.v = 0x00;
     }
}

static void
Sop_i420_Sto_Dacc( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     u8                *Sy    = gfxs->Sop[0];
     u8                *Su    = gfxs->Sop[1];
     u8                *Sv    = gfxs->Sop[2];
     GenefxAccumulator *D     = gfxs->Dacc;
     int                SperD = gfxs->SperD;

     while (--w) {
          D->YUV.a = 0xff;
          D->YUV.y = Sy[i>>16];
          D->YUV.u = Su[i>>17];
          D->YUV.v = Sv[i>>17];

          ++D;
          i += SperD;
     }
}

static void
Sop_lut8_Sto_Dacc( GenefxState *gfxs )
{
     int                i       = gfxs->Xphase;
     int                w       = gfxs->length + 1;
     u8                *S       = gfxs->Sop[0];
     GenefxAccumulator *D       = gfxs->Dacc;
     int                SperD   = gfxs->SperD;
     DFBColor          *entries = gfxs->Slut->entries;

     while (--w) {
          u8 s = S[i>>16];

          D->RGB.a = entries[s].a;
          D->RGB.r = entries[s].r;
          D->RGB.g = entries[s].g;
          D->RGB.b = entries[s].b;

          ++D;
          i += SperD;
     }
}

static void
Sop_alut44_Sto_Dacc( GenefxState *gfxs )
{
     int                i       = gfxs->Xphase;
     int                w       = gfxs->length + 1;
     u8                *S       = gfxs->Sop[0];
     GenefxAccumulator *D       = gfxs->Dacc;
     int                SperD   = gfxs->SperD;
     DFBColor          *entries = gfxs->Slut->entries;

     while (--w) {
          u8 s = S[i>>16];

          D->RGB.a = s & 0xf0;
          s &= 0x0f;
          D->RGB.r = entries[s].r;
          D->RGB.g = entries[s].g;
          D->RGB.b = entries[s].b;

          ++D;
          i += SperD;
     }
}

static void
Sop_nv12_Sto_Dacc( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     u8                *Sy    = gfxs->Sop[0];
     u16               *Suv   = gfxs->Sop[1];
     GenefxAccumulator *D     = gfxs->Dacc;
     int                SperD = gfxs->SperD;

     while (--w) {
          D->YUV.a = 0xff;
          D->YUV.y = Sy[i>>16];
          D->YUV.u = Suv[i>>17] & 0xff;
          D->YUV.v = Suv[i>>17] >> 8;

          ++D;
          i += SperD;
     }
}

static void
Sop_nv21_Sto_Dacc( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     u8                *Sy    = gfxs->Sop[0];
     u16               *Svu   = gfxs->Sop[1];
     GenefxAccumulator *D     = gfxs->Dacc;
     int                SperD = gfxs->SperD;

     while (--w) {
          D->YUV.a = 0xff;
          D->YUV.y = Sy[i>>16];
          D->YUV.u = Svu[i>>17] >> 8;
          D->YUV.v = Svu[i>>17] & 0xff;

          ++D;
          i += SperD;
     }
}

static void
Sop_ayuv_Sto_Dacc( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     u32               *S     = gfxs->Sop[0];
     GenefxAccumulator *D     = gfxs->Dacc;
     int                SperD = gfxs->SperD;

     while (--w) {
          u32 s = S[i>>16];

          D->YUV.a = (s >> 24);
          D->YUV.y = (s >> 16) & 0xff;
          D->YUV.u = (s >>  8) & 0xff;
          D->YUV.v =  s        & 0xff;

          ++D;
          i += SperD;
     }
}

static void
Sop_a4_Sto_Dacc( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     u8                *S     = gfxs->Sop[0];
     GenefxAccumulator *D     = gfxs->Dacc;
     int                SperD = gfxs->SperD;

     while (--w) {
          int j = i >> 16;
          u8  s = S[j>>1];

          if (j & 1)
               D->RGB.a = (s & 0x0f) | ((s << 4) & 0xf0);
          else
               D->RGB.a = (s & 0xf0) | (s >> 4);

          D->RGB.r = 0xff;
          D->RGB.g = 0xff;
          D->RGB.b = 0xff;

          ++D;
          i += SperD;
     }
}

static void
Sop_y444_Sto_Dacc( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     u8                *Sy    = gfxs->Sop[0];
     u8                *Su    = gfxs->Sop[1];
     u8                *Sv    = gfxs->Sop[2];
     GenefxAccumulator *D     = gfxs->Dacc;
     int                SperD = gfxs->SperD;

     while (--w) {
          D->YUV.a = 0xff;
          D->YUV.y = Sy[i>>16];
          D->YUV.u = Su[i>>16];
          D->YUV.v = Sv[i>>16];

          ++D;
          i += SperD;
     }
}

static void
Sop_avyu_Sto_Dacc( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     u32               *S     = gfxs->Sop[0];
     GenefxAccumulator *D     = gfxs->Dacc;
     int                SperD = gfxs->SperD;

     while (--w) {
          u32 s = S[i>>16];

          D->YUV.a = (s >> 24);
          D->YUV.v = (s >> 16) & 0xff;
          D->YUV.y = (s >>  8) & 0xff;
          D->YUV.u =  s        & 0xff;

          ++D;
          i += SperD;
     }
}

static void
Sop_vyu_Sto_Dacc( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     u8                *S     = gfxs->Sop[0];
     GenefxAccumulator *D     = gfxs->Dacc;
     int                SperD = gfxs->SperD;

     while (--w) {
          int pixelstart = (i >> 16) * 3;

#ifdef WORDS_BIGENDIAN
          D->YUV.a = 0xff;
          D->YUV.v = S[pixelstart+0];
          D->YUV.y = S[pixelstart+1];
          D->YUV.u = S[pixelstart+2];
#else
          D->YUV.a = 0xff;
          D->YUV.v = S[pixelstart+2];
          D->YUV.y = S[pixelstart+1];
          D->YUV.u = S[pixelstart+0];
#endif

          ++D;
          i += SperD;
     }
}

static void
Sop_nv24_Sto_Dacc( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     u8                *Sy    = gfxs->Sop[0];
     u16               *Suv   = gfxs->Sop[1];
     GenefxAccumulator *D     = gfxs->Dacc;
     int                SperD = gfxs->SperD;

     while (--w) {
          D->YUV.a = 0xff;
          D->YUV.y = Sy[i>>16];
          D->YUV.u = Suv[i>>16] & 0xff;
          D->YUV.v = Suv[i>>16] >> 8;

          ++D;
          i += SperD;
     }
}

static void
Sop_nv42_Sto_Dacc( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     u8                *Sy    = gfxs->Sop[0];
     u16               *Svu   = gfxs->Sop[1];
     GenefxAccumulator *D     = gfxs->Dacc;
     int                SperD = gfxs->SperD;

     while (--w) {
          D->YUV.a = 0xff;
          D->YUV.y = Sy[i>>16];
          D->YUV.u = Svu[i>>16] >> 8;
          D->YUV.v = Svu[i>>16] & 0xff;

          ++D;
          i += SperD;
     }
}

static GenefxFunc Sop_PFI_Sto_Dacc[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1555)]   = Sop_argb1555_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB16)]      = Sop_rgb16_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB24)]      = Sop_rgb24_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB32)]      = Sop_rgb32_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]       = Sop_argb_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_A8)]         = Sop_a8_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUY2)]       = Sop_yuy2_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB332)]     = Sop_rgb332_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_UYVY)]       = Sop_uyvy_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_I420)]       = Sop_i420_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV12)]       = Sop_i420_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT8)]       = Sop_lut8_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_ALUT44)]     = Sop_alut44_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)]      = Sop_airgb_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV12)]       = Sop_nv12_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV16)]       = Sop_nv12_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB2554)]   = Sop_argb2554_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)]   = Sop_argb4444_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA4444)]   = Sop_rgba4444_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV21)]       = Sop_nv21_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_AYUV)]       = Sop_ayuv_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_A4)]         = Sop_a4_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1666)]   = Sop_argb1666_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB6666)]   = Sop_argb6666_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB18)]      = Sop_rgb18_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB444)]     = Sop_xrgb4444_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB555)]     = Sop_xrgb1555_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR555)]     = Sop_xbgr1555_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA5551)]   = Sop_rgba5551_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y444)]       = Sop_y444_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB8565)]   = Sop_argb8565_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_AVYU)]       = Sop_avyu_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_VYU)]        = Sop_vyu_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1_LSB)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV16)]       = Sop_i420_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_ABGR)]       = Sop_abgr_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBAF88871)] = Sop_rgbaf88871_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT1)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV61)]       = Sop_nv21_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y42B)]       = Sop_i420_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV24)]       = Sop_y444_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV24)]       = Sop_nv24_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV42)]       = Sop_nv42_Sto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR24)]      = Sop_bgr24_Sto_Dacc,
};

/**********************************************************************************************************************
 ********************************* Sop_PFI_SKto_Dacc ******************************************************************
 **********************************************************************************************************************/

static void
Sop_a8_SKto_Dacc( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     u8                *S     = gfxs->Sop[0];
     GenefxAccumulator *D     = gfxs->Dacc;
     int                SperD = gfxs->SperD;

     /* No color to key. */
     while (--w) {
          u8 s = S[i>>16];

          D->RGB.a = s;
          D->RGB.r = 0xff;
          D->RGB.g = 0xff;
          D->RGB.b = 0xff;

          ++D;
          i += SperD;
     }
}

static void
Sop_yuy2_SKto_Dacc( GenefxState *gfxs )
{
     int                i     =  gfxs->Xphase;
     int                w     = (gfxs->length >> 1) + 1;
     u32               *S     =  gfxs->Sop[0];
     GenefxAccumulator *D     =  gfxs->Dacc;
     u32                Ky    =  gfxs->Skey & 0x000000ff;
#ifdef WORDS_BIGENDIAN
     u32                Kcb   = (gfxs->Skey & 0xff000000) >> 24;
     u32                Kcr   = (gfxs->Skey & 0x0000ff00) >>  8;
#else
     u32                Kcb   = (gfxs->Skey & 0x0000ff00) >>  8;
     u32                Kcr   = (gfxs->Skey & 0xff000000) >> 24;
#endif
     int                SperD =  gfxs->SperD;

     while (--w) {
          u32 s = S[i>>17];
          u32 y0, cb, y1, cr;

#ifdef WORDS_BIGENDIAN
          cb = (s & 0xff000000) >> 24;
          cr = (s & 0x0000ff00) >>  8;
#else
          cb = (s & 0x0000ff00) >>  8;
          cr = (s & 0xff000000) >> 24;
#endif
          y0 = ((u16*) S)[i>>16]         & 0x00ff;
          y1 = ((u16*) S)[(i+SperD)>>16] & 0x00ff;

          if (y0 != Ky || cb != Kcb || cr != Kcr) {
               D[0].YUV.a = 0xff;
               D[0].YUV.y = y0;
               D[0].YUV.u = cb;
               D[0].YUV.v = cr;
          }
          else
               D[0].YUV.a = 0xf000;

          if (y0 != Ky || cb != Kcb || cr != Kcr) {
               D[1].YUV.a = 0xff;
               D[1].YUV.y = y1;
               D[1].YUV.u = cb;
               D[1].YUV.v = cr;
          }
          else
               D[1].YUV.a = 0xf000;

          D += 2;
          i += SperD << 1;
     }

     if (gfxs->length & 1) {
          u16 s = ((u16*) S)[i>>16];

          if (s != (Ky | (Kcb << 8))) {
               D->YUV.a = 0xff;
               D->YUV.y = s & 0xff;
               D->YUV.u = s >> 8;
               D->YUV.v = 0x00;
          }
          else
               D->YUV.a = 0xf000;
     }
}

static void
Sop_rgb332_SKto_Dacc( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     u8                *S     = gfxs->Sop[0];
     GenefxAccumulator *D     = gfxs->Dacc;
     u8                 Skey  = gfxs->Skey;
     int                SperD = gfxs->SperD;

     while (--w) {
          u8 s = S[i>>16];

          if (s != Skey) {
               D->RGB.a = 0xff;
               D->RGB.r = EXPAND_3to8(  s >> 5         );
               D->RGB.g = EXPAND_3to8( (s & 0x1c) >> 2 );
               D->RGB.b = EXPAND_2to8(  s & 0x03       );
          }
          else
               D->RGB.a = 0xf000;

          ++D;
          i += SperD;
     }
}

static void
Sop_uyvy_SKto_Dacc( GenefxState *gfxs )
{
     int                i     =  gfxs->Xphase;
     int                w     = (gfxs->length >> 1) + 1;
     u32               *S     =  gfxs->Sop[0];
     GenefxAccumulator *D     =  gfxs->Dacc;
     u32                Ky    = (gfxs->Skey & 0x0000ff00) >>  8;
#ifdef WORDS_BIGENDIAN
     u32                Kcb   = (gfxs->Skey & 0x00ff0000) >> 16;
     u32                Kcr   =  gfxs->Skey & 0x000000ff;
#else
     u32                Kcb   =  gfxs->Skey & 0x000000ff;
     u32                Kcr   = (gfxs->Skey & 0x00ff0000) >> 16;
#endif
     int                SperD =  gfxs->SperD;

     while (--w) {
          u32 s = S[i>>17];
          u32 cb, y0, cr, y1;

#ifdef WORDS_BIGENDIAN
          cb = (s & 0x00ff0000) >> 16;
          cr =  s & 0x000000ff;
#else
          cb =  s & 0x000000ff;
          cr = (s & 0x00ff0000) >> 16;
#endif
          y0 = (((u16*) S)[i>>16]         & 0xff00) >> 8;
          y1 = (((u16*) S)[(i+SperD)>>16] & 0xff00) >> 8;

          if (y0 != Ky || cb != Kcb || cr != Kcr) {
               D[0].YUV.a = 0xff;
               D[0].YUV.y = y0;
               D[0].YUV.u = cb;
               D[0].YUV.v = cr;
          }
          else
              D[0].YUV.a = 0xf000;

          if (y0 != Ky || cb != Kcb || cr != Kcr) {
               D[1].YUV.a = 0xff;
               D[1].YUV.y = y1;
               D[1].YUV.u = cb;
               D[1].YUV.v = cr;
          }
          else
               D[1].YUV.a = 0xf000;

          D += 2;
          i += SperD << 1;
     }

     if (gfxs->length & 1) {
          u16 s = ((u16*) S)[i>>16];

          if (s != (Kcb | (Ky << 8))) {
               D->YUV.a = 0xff;
               D->YUV.y = s >> 8;
               D->YUV.u = s & 0xff;
               D->YUV.v = 0x00;
          }
          else
               D->YUV.a = 0xf000;
     }
}

static void
Sop_lut8_SKto_Dacc( GenefxState *gfxs )
{
     int                i       = gfxs->Xphase;
     int                w       = gfxs->length + 1;
     u8                *S       = gfxs->Sop[0];
     GenefxAccumulator *D       = gfxs->Dacc;
     u32                Skey    = gfxs->Skey;
     int                SperD   = gfxs->SperD;
     DFBColor          *entries = gfxs->Slut->entries;

     while (--w) {
          u8 s = S[i>>16];

          if (s != Skey) {
               D->RGB.a = entries[s].a;
               D->RGB.r = entries[s].r;
               D->RGB.g = entries[s].g;
               D->RGB.b = entries[s].b;
          }
          else
               D->RGB.a = 0xf000;

          ++D;
          i += SperD;
     }
}

static void
Sop_alut44_SKto_Dacc( GenefxState *gfxs )
{
     int                i       = gfxs->Xphase;
     int                w       = gfxs->length + 1;
     u8                *S       = gfxs->Sop[0];
     GenefxAccumulator *D       = gfxs->Dacc;
     u32                Skey    = gfxs->Skey;
     int                SperD   = gfxs->SperD;
     DFBColor          *entries = gfxs->Slut->entries;

     while (--w) {
          u8 s = S[i>>16];

          if ((s & 0x0f) != Skey) {
               D->RGB.a = ((s & 0xf0) >> 4) | (s & 0xf0);
               s &= 0x0f;
               D->RGB.r = entries[s].r;
               D->RGB.g = entries[s].g;
               D->RGB.b = entries[s].b;
          }
          else
               D->RGB.a = 0xf000;

          ++D;
          i += SperD;
     }
}

static void
Sop_y444_SKto_Dacc( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     u8                *Sy    = gfxs->Sop[0];
     u8                *Su    = gfxs->Sop[1];
     u8                *Sv    = gfxs->Sop[2];
     GenefxAccumulator *D     = gfxs->Dacc;
     u32                Skey  = gfxs->Skey;
     int                SperD = gfxs->SperD;

     while (--w) {
          u8 sy = Sy[i>>16];
          u8 su = Su[i>>16];
          u8 sv = Sv[i>>16];

          if (Skey != (u32) (sy << 16 | su << 8 | sv)) {
               D->YUV.a = 0xff;
               D->YUV.y = sy;
               D->YUV.u = su;
               D->YUV.v = sv;
          }
          else
               D->YUV.a = 0xff00;

          ++D;
          i += SperD;
     }
}

static void
Sop_avyu_SKto_Dacc( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     u32               *S     = gfxs->Sop[0];
     GenefxAccumulator *D     = gfxs->Dacc;
     u32                Skey  = gfxs->Skey;
     int                SperD = gfxs->SperD;

     while (--w) {
          u32 s = S[i>>16];

          if ((s & 0x00ffffff) != Skey) {
               D->YUV.a = (s & 0xff000000) >> 24;
               D->YUV.v = (s & 0x00ff0000) >> 16;
               D->YUV.y = (s & 0x0000ff00) >>  8;
               D->YUV.u =  s & 0x000000ff;
          }
          else
               D->YUV.a = 0xf000;

          ++D;
          i += SperD;
     }
}

static void
Sop_vyu_SKto_Dacc( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     u8                *S     = gfxs->Sop[0];
     GenefxAccumulator *D     = gfxs->Dacc;
     u32                Skey  = gfxs->Skey;
     int                SperD = gfxs->SperD;

     while (--w) {
          int pixelstart = (i >> 16) * 3;

#ifdef WORDS_BIGENDIAN
          u32 s = S[pixelstart+0] << 16 | S[pixelstart+1] << 8 | S[pixelstart+2];
#else
          u32 s = S[pixelstart+2] << 16 | S[pixelstart+1] << 8 | S[pixelstart+0];
#endif

          if (Skey != s) {
#ifdef WORDS_BIGENDIAN
               D->YUV.a = 0xff;
               D->YUV.v = S[pixelstart+0];
               D->YUV.y = S[pixelstart+1];
               D->YUV.u = S[pixelstart+2];
#else
               D->YUV.a = 0xff;
               D->YUV.v = S[pixelstart+2];
               D->YUV.y = S[pixelstart+1];
               D->YUV.u = S[pixelstart+0];
#endif
          }
          else
               D->YUV.a = 0xf000;

          ++D;
          i += SperD;
     }
}

static GenefxFunc Sop_PFI_SKto_Dacc[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1555)]   = Sop_argb1555_SKto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB16)]      = Sop_rgb16_SKto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB24)]      = Sop_rgb24_SKto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB32)]      = Sop_rgb32_SKto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]       = Sop_argb_SKto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_A8)]         = Sop_a8_SKto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUY2)]       = Sop_yuy2_SKto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB332)]     = Sop_rgb332_SKto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_UYVY)]       = Sop_uyvy_SKto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_I420)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT8)]       = Sop_lut8_SKto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_ALUT44)]     = Sop_alut44_SKto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)]      = Sop_airgb_SKto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB2554)]   = Sop_argb2554_SKto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)]   = Sop_argb4444_SKto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA4444)]   = Sop_rgba4444_SKto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV21)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AYUV)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A4)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1666)]   = Sop_argb1666_SKto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB6666)]   = Sop_argb6666_SKto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB18)]      = Sop_rgb18_SKto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB444)]     = Sop_xrgb4444_SKto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB555)]     = Sop_xrgb1555_SKto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR555)]     = Sop_xbgr1555_SKto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA5551)]   = Sop_rgba5551_SKto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y444)]       = Sop_y444_SKto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB8565)]   = Sop_argb8565_SKto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_AVYU)]       = Sop_avyu_SKto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_VYU)]        = Sop_vyu_SKto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1_LSB)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ABGR)]       = Sop_abgr_SKto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBAF88871)] = Sop_rgbaf88871_SKto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT1)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV61)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y42B)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV24)]       = Sop_y444_SKto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV24)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV42)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR24)]      = Sop_bgr24_SKto_Dacc,
};

/**********************************************************************************************************************
 ********************************* Sop_PFI_TEX_to_Dacc ****************************************************************
 **********************************************************************************************************************/

static void
Sop_a8_TEX_to_Dacc( GenefxState *gfxs )
{
     int                s     = gfxs->s;
     int                t     = gfxs->t;
     int                w     = gfxs->length + 1;
     u8                *S     = gfxs->Sop[0];
     GenefxAccumulator *D     = gfxs->Dacc;
     int                SperD = gfxs->SperD;
     int                TperD = gfxs->TperD;

     while (--w) {
          D->RGB.a = S[(s>>16)+(t>>16)*gfxs->src_pitch];
          D->RGB.r = 0xff;
          D->RGB.g = 0xff;
          D->RGB.b = 0xff;

          ++D;
          s += SperD;
          t += TperD;
     }
}

static GenefxFunc Sop_PFI_TEX_to_Dacc[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1555)]   = Sop_argb1555_TEX_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB16)]      = Sop_rgb16_TEX_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB24)]      = Sop_rgb24_TEX_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB32)]      = Sop_rgb32_TEX_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]       = Sop_argb_TEX_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_A8)]         = Sop_a8_TEX_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUY2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB332)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_UYVY)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_I420)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT8)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ALUT44)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)]      = Sop_airgb_TEX_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB2554)]   = Sop_argb2554_TEX_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)]   = Sop_argb4444_TEX_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA4444)]   = Sop_rgba4444_TEX_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV21)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AYUV)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A4)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1666)]   = Sop_argb1666_TEX_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB6666)]   = Sop_argb6666_TEX_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB18)]      = Sop_rgb18_TEX_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB444)]     = Sop_xrgb4444_TEX_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB555)]     = Sop_xrgb1555_TEX_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR555)]     = Sop_xbgr1555_TEX_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA5551)]   = Sop_rgba5551_TEX_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y444)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB8565)]   = Sop_argb8565_TEX_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_AVYU)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_VYU)]        = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1_LSB)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ABGR)]       = Sop_abgr_TEX_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBAF88871)] = Sop_rgbaf88871_TEX_to_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT1)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV61)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y42B)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV24)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV24)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV42)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR24)]      = Sop_bgr24_TEX_to_Dacc,
};

/**********************************************************************************************************************
 ********************************* Sop_PFI_TEX_Kto_Dacc ***************************************************************
 **********************************************************************************************************************/

static GenefxFunc Sop_PFI_TEX_Kto_Dacc[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1555)]   = Sop_argb1555_TEX_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB16)]      = Sop_rgb16_TEX_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB24)]      = Sop_rgb24_TEX_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB32)]      = Sop_rgb32_TEX_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]       = Sop_argb_TEX_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_A8)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUY2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB332)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_UYVY)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_I420)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT8)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ALUT44)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)]      = Sop_airgb_TEX_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB2554)]   = Sop_argb2554_TEX_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)]   = Sop_argb4444_TEX_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA4444)]   = Sop_rgba4444_TEX_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV21)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AYUV)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A4)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1666)]   = Sop_argb1666_TEX_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB6666)]   = Sop_argb6666_TEX_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB18)]      = Sop_rgb18_TEX_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB444)]     = Sop_xrgb4444_TEX_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB555)]     = Sop_xrgb1555_TEX_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR555)]     = Sop_xbgr1555_TEX_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA5551)]   = Sop_rgba5551_TEX_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y444)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB8565)]   = Sop_argb8565_TEX_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_AVYU)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_VYU)]        = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1_LSB)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ABGR)]       = Sop_abgr_TEX_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBAF88871)] = Sop_rgbaf88871_TEX_Kto_Dacc,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT1)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV61)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y42B)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV24)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV24)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV42)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR24)]      = Sop_bgr24_TEX_Kto_Dacc,
};

/**********************************************************************************************************************
 ********************************* Sacc_to_Aop_PFI ********************************************************************
 **********************************************************************************************************************/

static void
Sacc_to_Aop_a8( GenefxState *gfxs )
{
     int                w = gfxs->length + 1;
     GenefxAccumulator *S = gfxs->Sacc;
     u8                *D = gfxs->Aop[0];

     while (--w) {
          if (!(S->RGB.a & 0xf000)) {
               *D = (S->RGB.a & 0xff00) ? 0xff : S->RGB.a;
          }

          ++S;
          ++D;
     }
}

static void
Sacc_to_Aop_yuy2( GenefxState *gfxs )
{
     int                l;
     int                w = gfxs->length;
     GenefxAccumulator *S = gfxs->Sacc;
     u16               *D = gfxs->Aop[0];

     if ((long) D & 2) {
          if (!(S->YUV.a & 0xf00)) {
               *D = ((S->YUV.y & 0xff00) ? 0x00ff : S->YUV.y) | ((S->YUV.v & 0xff00) ? 0xff00 : (S->YUV.v << 8));
          }

          ++S;
          ++D;
          --w;
     }

     for (l = w >> 1; l--;) {
          if (!(S[0].YUV.a & 0xf000) && !(S[1].YUV.a & 0xf000)) {
               u32 y0, cb, y1, cr;

               y0 = (S[0].YUV.y & 0xff00) ? 0xff : S[0].YUV.y;
               y1 = (S[1].YUV.y & 0xff00) ? 0xff : S[1].YUV.y;

               cb = (S[0].YUV.u + S[1].YUV.u) >> 1;
               if (cb & 0xff00)
                    cb = 0xff;

               cr = (S[0].YUV.v + S[1].YUV.v) >> 1;
               if (cr & 0xff00)
                    cr = 0xff;

#ifdef WORDS_BIGENDIAN
               *((u32*) D) = y1 | (cr << 8) | (y0 << 16) | (cb << 24);
#else
               *((u32*) D) = y0 | (cb << 8) | (y1 << 16) | (cr << 24);
#endif
          }
          else if (!(S[0].YUV.a & 0xf000)) {
               D[0] = ((S[0].YUV.y & 0xff00) ? 0x00ff :  S[0].YUV.y) |
                      ((S[0].YUV.u & 0xff00) ? 0xff00 : (S[0].YUV.u << 8));
          }
          else if (!(S[1].YUV.a & 0xf000)) {
               D[1] = ((S[1].YUV.y & 0xff00) ? 0x00ff :  S[1].YUV.y) |
                      ((S[1].YUV.v & 0xff00) ? 0xff00 : (S[1].YUV.v << 8));
          }

          S += 2;
          D += 2;
     }

     if (w & 1) {
          if (!(S->YUV.a & 0xf00)) {
               *D = ((S->YUV.y & 0xff00) ? 0x00ff : S->YUV.y) | ((S->YUV.u & 0xff00) ? 0xff00 : (S->YUV.u << 8));
          }
     }
}

static void
Sacc_to_Aop_rgb332( GenefxState *gfxs )
{
     int                w = gfxs->length + 1;
     GenefxAccumulator *S = gfxs->Sacc;
     u8                *D = gfxs->Aop[0];

     while (--w) {
          if (!(S->RGB.a & 0xf000)) {
               *D = PIXEL_RGB332( (S->RGB.r & 0xff00) ? 0xff : S->RGB.r,
                                  (S->RGB.g & 0xff00) ? 0xff : S->RGB.g,
                                  (S->RGB.b & 0xff00) ? 0xff : S->RGB.b );
          }

          ++S;
          ++D;
     }
}

static void
Sacc_to_Aop_uyvy( GenefxState *gfxs )
{
     int                l;
     int                w = gfxs->length;
     GenefxAccumulator *S = gfxs->Sacc;
     u16               *D = gfxs->Aop[0];

     if ((long) D & 2) {
          if (!(S->YUV.a & 0xf00)) {
               *D = ((S->YUV.v & 0xff00) ? 0x00ff : S->YUV.v) | ((S->YUV.y & 0xff00) ? 0xff00 : (S->YUV.y << 8));
          }

          ++S;
          ++D;
          --w;
     }

     for (l = w >> 1; l--;) {
          if (!(S[0].YUV.a & 0xf000) && !(S[1].YUV.a & 0xf000)) {
               u32 cb, y0, cr, y1;

               y0 = (S[0].YUV.y & 0xff00) ? 0xff : S[0].YUV.y;
               y1 = (S[1].YUV.y & 0xff00) ? 0xff : S[1].YUV.y;

               cb = (S[0].YUV.u + S[1].YUV.u) >> 1;
               if (cb & 0xff00)
                    cb = 0xff;

               cr = (S[0].YUV.v + S[1].YUV.v) >> 1;
               if (cr & 0xff00)
                    cr = 0xff;

#ifdef WORDS_BIGENDIAN
               *((u32*) D) = cr | (y1 << 8) | (cb << 16) | (y0 << 24);
#else
               *((u32*) D) = cb | (y0 << 8) | (cr << 16) | (y1 << 24);
#endif
          }
          else if (!(S[0].YUV.a & 0xf000)) {
               D[0] = ((S[0].YUV.u & 0xff00) ? 0x00ff :  S[0].YUV.u) |
                      ((S[0].YUV.y & 0xff00) ? 0xff00 : (S[0].YUV.y << 8));
          }
          else if (!(S[1].YUV.a & 0xf000)) {
               D[1] = ((S[1].YUV.v & 0xff00) ? 0x00ff :  S[1].YUV.v) |
                      ((S[1].YUV.y & 0xff00) ? 0xff00 : (S[1].YUV.y << 8));
          }

          S += 2;
          D += 2;
     }

     if (w & 1) {
          if (!(S->YUV.a & 0xf00)) {
               *D = ((S->YUV.u & 0xff00) ? 0x00ff : S->YUV.u) | ((S->YUV.y & 0xff00) ? 0xff00 : (S->YUV.y << 8));
          }
     }
}

static void
Sacc_to_Aop_i420( GenefxState *gfxs )
{
     int                w  = gfxs->length + 1;
     GenefxAccumulator *S  = gfxs->Sacc;
     u8                *Dy = gfxs->Aop[0];

     while (--w) {
          if (!(S->YUV.a & 0xf000)) {
               *Dy = (S->YUV.y & 0xff00) ? 0xff : S->YUV.y;
          }

          ++S;
          ++Dy;
     }

     if (gfxs->AopY & 1) {
          u8 *Du = gfxs->Aop[1];
          u8 *Dv = gfxs->Aop[2];

          w = (gfxs->length >> 1) + 1;
          S =  gfxs->Sacc;

          while (--w) {
               if (!(S[0].YUV.a & 0xf000) && !(S[1].YUV.a & 0xf000)) {
                    u32 tmp;

                    tmp = (S[0].YUV.u + S[1].YUV.u) >> 1;
                    if (tmp & 0xff00)
                         tmp = 0xff;
                    *Du = tmp;

                    tmp = (S[0].YUV.v + S[1].YUV.v) >> 1;
                    if (tmp & 0xff00)
                         tmp = 0xff;
                    *Dv = tmp;

               }
               else if (!(S[0].YUV.a & 0xf000)) {
                    *Du = (*Du + ((S[0].YUV.u & 0xff00) ? 0xff : S[0].YUV.u)) >> 1;
                    *Dv = (*Dv + ((S[0].YUV.v & 0xff00) ? 0xff : S[0].YUV.v)) >> 1;
               }
               else if (!(S[1].YUV.a & 0xf000)) {
                    *Du = (*Du + ((S[1].YUV.u & 0xff00) ? 0xff : S[1].YUV.u)) >> 1;
                    *Dv = (*Dv + ((S[1].YUV.v & 0xff00) ? 0xff : S[1].YUV.v)) >> 1;
               }

               S += 2;
               ++Du; ++Dv;
          }
     }
}

static void
Sacc_to_Aop_lut8( GenefxState *gfxs )
{
     int                w = gfxs->length + 1;
     GenefxAccumulator *S = gfxs->Sacc;
     u8                *D = gfxs->Aop[0];

     while (--w) {
          if (!(S->RGB.a & 0xf000)) {
               *D = dfb_palette_search( gfxs->Alut,
                                        (S->RGB.r & 0xff00) ? 0xff : S->RGB.r,
                                        (S->RGB.g & 0xff00) ? 0xff : S->RGB.g,
                                        (S->RGB.b & 0xff00) ? 0xff : S->RGB.b,
                                        (S->RGB.a & 0xff00) ? 0xff : S->RGB.a );
          }

          ++S;
          ++D;
     }
}

static void
Sacc_to_Aop_alut44( GenefxState *gfxs )
{
     int                w = gfxs->length + 1;
     GenefxAccumulator *S = gfxs->Sacc;
     u8                *D = gfxs->Aop[0];

     while (--w) {
          if (!(S->RGB.a & 0xf000)) {
               *D = (S->RGB.a & 0xff00) ?
                    0xf0 : (S->RGB.a & 0xf0) + dfb_palette_search( gfxs->Alut,
                                                                   (S->RGB.r & 0xff00) ? 0xff : S->RGB.r,
                                                                   (S->RGB.g & 0xff00) ? 0xff : S->RGB.g,
                                                                   (S->RGB.b & 0xff00) ? 0xff : S->RGB.b,
                                                                   0x80 );
          }

          ++S;
          ++D;
     }
}

static void
Sacc_to_Aop_nv12( GenefxState *gfxs )
{
     int                w  = gfxs->length + 1;
     GenefxAccumulator *S  = gfxs->Sacc;
     u8                *Dy = gfxs->Aop[0];

     while (--w) {
          if (!(S->YUV.a & 0xf000)) {
               *Dy = (S->YUV.y & 0xff00) ? 0xff : S->YUV.y;
          }

          ++S;
          ++Dy;
     }

     if (gfxs->AopY & 1) {
          u16 *Duv = gfxs->Aop[1];

          w = (gfxs->length >> 1) + 1;
          S =  gfxs->Sacc;

          while (--w) {
               u32 cb, cr;

               if (!(S[0].YUV.a & 0xf000) && !(S[1].YUV.a & 0xf000)) {
                    cb = (S[0].YUV.u + S[1].YUV.u) >> 1;
                    if (cb & 0xff00)
                         cb = 0xff;

                    cr = (S[0].YUV.v + S[1].YUV.v) >> 1;
                    if (cr & 0xff00)
                         cr = 0xff;

                    *Duv = cb | (cr << 8);
               }
               else if (!(S[0].YUV.a & 0xf000)) {
                    cb   = ((*Duv & 0xff) + ((S[0].YUV.u & 0xff00) ? 0xff : S[0].YUV.u)) >> 1;
                    cr   = ((*Duv >> 8)   + ((S[0].YUV.v & 0xff00) ? 0xff : S[0].YUV.v)) >> 1;
                    *Duv = cb | (cr << 8);
               }
               else if (!(S[1].YUV.a & 0xf000)) {
                    cb   = ((*Duv & 0xff) + ((S[1].YUV.u & 0xff00) ? 0xff : S[1].YUV.u)) >> 1;
                    cr   = ((*Duv >> 8)   + ((S[1].YUV.v & 0xff00) ? 0xff : S[1].YUV.v)) >> 1;
                    *Duv = cb | (cr << 8);
               }

               S += 2;
               ++Duv;
          }
     }
}

static void
Sacc_to_Aop_nv16( GenefxState *gfxs )
{
     int                w   = gfxs->length + 1;
     GenefxAccumulator *S   = gfxs->Sacc;
     u8                *Dy  = gfxs->Aop[0];
     u16               *Duv = gfxs->Aop[1];

     while (--w) {
          if (!(S->YUV.a & 0xf000)) {
               *Dy = (S->YUV.y & 0xff00) ? 0xff : S->YUV.y;
          }

          ++S;
          ++Dy;
     }

     w = (gfxs->length >> 1) + 1;
     S =  gfxs->Sacc;

     while (--w) {
          u32 cb, cr;

          if (!(S[0].YUV.a & 0xf000) && !(S[1].YUV.a & 0xf000)) {
               cb = (S[0].YUV.u + S[1].YUV.u) >> 1;
               if (cb & 0xff00)
                    cb = 0xff;

               cr = (S[0].YUV.v + S[1].YUV.v) >> 1;
               if (cr & 0xff00)
                    cr = 0xff;

               *Duv = cb | (cr << 8);
          }
          else if (!(S[0].YUV.a & 0xf000)) {
               cb   = ((*Duv & 0xff) + ((S[0].YUV.u & 0xff00) ? 0xff : S[0].YUV.u)) >> 1;
               cr   = ((*Duv >> 8)   + ((S[0].YUV.v & 0xff00) ? 0xff : S[0].YUV.v)) >> 1;
               *Duv = cb | (cr << 8);
          }
          else if (!(S[1].YUV.a & 0xf000)) {
               cb   = ((*Duv & 0xff) + ((S[1].YUV.u & 0xff00) ? 0xff : S[1].YUV.u)) >> 1;
               cr   = ((*Duv >> 8)   + ((S[1].YUV.v & 0xff00) ? 0xff : S[1].YUV.v)) >> 1;
               *Duv = cb | (cr << 8);
          }

          S += 2;
          ++Duv;
     }
}

static void
Sacc_to_Aop_nv21( GenefxState *gfxs )
{
     int                w  = gfxs->length + 1;
     GenefxAccumulator *S  = gfxs->Sacc;
     u8                *Dy = gfxs->Aop[0];

     while (--w) {
          if (!(S->YUV.a & 0xf000)) {
               *Dy = (S->YUV.y & 0xff00) ? 0xff : S->YUV.y;
          }

          ++S;
          ++Dy;
     }

     if (gfxs->AopY & 1) {
          u16 *Dvu = gfxs->Aop[1];

          w = (gfxs->length >> 1) + 1;
          S =  gfxs->Sacc;

          while (--w) {
               u32 cb, cr;

               if (!(S[0].YUV.a & 0xf000) && !(S[1].YUV.a & 0xf000)) {
                    cb = (S[0].YUV.u + S[1].YUV.u) >> 1;
                    if (cb & 0xff00)
                         cb = 0xff;

                    cr = (S[0].YUV.v + S[1].YUV.v) >> 1;
                    if (cr & 0xff00)
                         cr = 0xff;

                    *Dvu = cr | (cb << 8);
               }
               else if (!(S[0].YUV.a & 0xf000)) {
                    cb   = ((*Dvu >> 8)   + ((S[0].YUV.u & 0xff00) ? 0xff : S[0].YUV.u)) >> 1;
                    cr   = ((*Dvu & 0xff) + ((S[0].YUV.v & 0xff00) ? 0xff : S[0].YUV.v)) >> 1;
                    *Dvu = cr | (cb << 8);
               }
               else if (!(S[1].YUV.a & 0xf000)) {
                    cb   = ((*Dvu >> 8)   + ((S[1].YUV.u & 0xff00) ? 0xff : S[1].YUV.u)) >> 1;
                    cr   = ((*Dvu & 0xff) + ((S[1].YUV.v & 0xff00) ? 0xff : S[1].YUV.v)) >> 1;
                    *Dvu = cr | (cb << 8);
               }

               S += 2;
               ++Dvu;
          }
     }
}

static void
Sacc_to_Aop_ayuv( GenefxState *gfxs )
{
     int                w = gfxs->length + 1;
     GenefxAccumulator *S = gfxs->Sacc;
     u32               *D = gfxs->Aop[0];

     while (--w) {
          if (!(S->YUV.a & 0xf000)) {
               *D = PIXEL_AYUV( (S->YUV.a & 0xff00) ? 0xff : S->YUV.a,
                                (S->YUV.y & 0xff00) ? 0xff : S->YUV.y,
                                (S->YUV.u & 0xff00) ? 0xff : S->YUV.u,
                                (S->YUV.v & 0xff00) ? 0xff : S->YUV.v );
          }

          ++S;
          ++D;
     }
}

static void
Sacc_to_Aop_a4( GenefxState *gfxs )
{
     int                w = (gfxs->length >> 1) + 1;
     GenefxAccumulator *S =  gfxs->Sacc;
     u8                *D =  gfxs->Aop[0];

     while (--w) {
          if (!(S[0].RGB.a & 0xf000) && !(S[1].RGB.a & 0xf000)) {
               *D = ((S[0].RGB.a & 0xff00) ? 0xf0 : (S[0].RGB.a & 0xf0)) |
                    ((S[1].RGB.a & 0xff00) ? 0x0f : (S[1].RGB.a >> 4));
          }
          else if (!(S[0].RGB.a & 0xf000)) {
               *D = (*D & 0x0f) | ((S[0].RGB.a & 0xff00) ? 0xf0 : (S[0].RGB.a & 0xf0));
          }
          else if (!(S[1].RGB.a & 0xf000)) {
               *D = (*D & 0xf0) | ((S[1].RGB.a & 0xff00) ? 0x0f : (S[1].RGB.a >> 4));
          }

          S += 2;
          ++D;
     }

     if (gfxs->length & 1) {
          if (!(S->RGB.a & 0xf000))
               *D = (*D & 0x0f) | ((S->RGB.a & 0xff00) ? 0xf0 : (S->RGB.a & 0xf0));
     }
}

static void
Sacc_to_Aop_y444( GenefxState *gfxs )
{
     int                w  = gfxs->length + 1;
     GenefxAccumulator *S  = gfxs->Sacc;
     u8                *Dy = gfxs->Aop[0];
     u8                *Du = gfxs->Aop[1];
     u8                *Dv = gfxs->Aop[2];

     while (--w) {
          if (!(S->YUV.a & 0xf000)) {
               *Dy = (S->YUV.y & 0xff00) ? 0xff : S->YUV.y;
               *Du = (S->YUV.u & 0xff00) ? 0xff : S->YUV.u;
               *Dv = (S->YUV.v & 0xff00) ? 0xff : S->YUV.v;
          }

          ++S;
          ++Dy; ++Du; ++Dv;
     }
}

static void
Sacc_to_Aop_avyu( GenefxState *gfxs )
{
     int                w = gfxs->length + 1;
     GenefxAccumulator *S = gfxs->Sacc;
     u32               *D = gfxs->Aop[0];

     while (--w) {
          if (!(S->YUV.a & 0xf000)) {
               *D = PIXEL_AVYU( (S->YUV.a & 0xff00) ? 0xff : S->YUV.a,
                                (S->YUV.y & 0xff00) ? 0xff : S->YUV.y,
                                (S->YUV.u & 0xff00) ? 0xff : S->YUV.u,
                                (S->YUV.v & 0xff00) ? 0xff : S->YUV.v );
          }

          ++S;
          ++D;
     }
}

static void
Sacc_to_Aop_vyu( GenefxState *gfxs )
{
     int                w = gfxs->length + 1;
     GenefxAccumulator *S = gfxs->Sacc;
     u8                *D = gfxs->Aop[0];

     while (--w) {
          if (!(S->YUV.a & 0xf000)) {
               u8 y = (S->YUV.y & 0xff00) ? 0xff : S->YUV.y;
               u8 u = (S->YUV.u & 0xff00) ? 0xff : S->YUV.u;
               u8 v = (S->YUV.v & 0xff00) ? 0xff : S->YUV.v;

#ifdef WORDS_BIGENDIAN
               D[0] = v;
               D[1] = y;
               D[2] = u;
#else
               D[0] = u;
               D[1] = y;
               D[2] = v;
#endif
          }

          ++S;
          D += 3;
     }
}

static void
Sacc_to_Aop_y42b( GenefxState *gfxs )
{
     int                w  = gfxs->length + 1;
     GenefxAccumulator *S  = gfxs->Sacc;
     u8                *Dy = gfxs->Aop[0];
     u8                *Du = gfxs->Aop[1];
     u8                *Dv = gfxs->Aop[2];

     while (--w) {
          if (!(S->YUV.a & 0xf000)) {
               *Dy = (S->YUV.y & 0xff00) ? 0xff : S->YUV.y;
          }

          ++S;
          ++Dy;
     }

     w = (gfxs->length / 2) + 1;
     S =  gfxs->Sacc;

     while (--w) {
          if (!(S[0].YUV.a & 0xf000) && !(S[1].YUV.a & 0xf000)) {
               u32 tmp;

               tmp = (S[0].YUV.u + S[1].YUV.u) / 2;
               if (tmp & 0xff00)
                    tmp = 0xff;
               *Du = tmp;

               tmp = (S[0].YUV.v + S[1].YUV.v) / 2;
               if (tmp & 0xff00)
                    tmp = 0xff;
               *Dv = tmp;
          }
          else if (!(S[0].YUV.a & 0xf000)) {
               *Du = (*Du + ((S[0].YUV.u & 0xff00) ? 0xff : S[0].YUV.u)) / 2;
               *Dv = (*Dv + ((S[0].YUV.v & 0xff00) ? 0xff : S[0].YUV.v)) / 2;
          }
          else if (!(S[1].YUV.a & 0xf000)) {
               *Du = (*Du + ((S[1].YUV.u & 0xff00) ? 0xff : S[1].YUV.u)) / 2;
               *Dv = (*Dv + ((S[1].YUV.v & 0xff00) ? 0xff : S[1].YUV.v)) / 2;
          }

          S += 2;
          ++Du; ++Dv;
     }
}

static void
Sacc_to_Aop_nv61( GenefxState *gfxs )
{
     int                w   = gfxs->length + 1;
     GenefxAccumulator *S   = gfxs->Sacc;
     u8                *Dy  = gfxs->Aop[0];
     u16               *Dvu = gfxs->Aop[1];

     while (--w) {
          if (!(S->YUV.a & 0xf000)) {
               *Dy = (S->YUV.y & 0xff00) ? 0xff : S->YUV.y;
          }

          ++S;
          ++Dy;
     }

     w = (gfxs->length >> 1) + 1;
     S =  gfxs->Sacc;

     while (--w) {
          u32 cb, cr;

          if (!(S[0].YUV.a & 0xf000) && !(S[1].YUV.a & 0xf000)) {
               cb = (S[0].YUV.u + S[1].YUV.u) >> 1;
               if (cb & 0xff00)
                    cb = 0xff;

               cr = (S[0].YUV.v + S[1].YUV.v) >> 1;
               if (cr & 0xff00)
                    cr = 0xff;

               *Dvu = cr | (cb << 8);
          }
          else if (!(S[0].YUV.a & 0xf000)) {
               cb   = ((*Dvu >> 8)   + ((S[0].YUV.u & 0xff00) ? 0xff : S[0].YUV.u)) >> 1;
               cr   = ((*Dvu & 0xff) + ((S[0].YUV.v & 0xff00) ? 0xff : S[0].YUV.v)) >> 1;
               *Dvu = cr | (cb << 8);
          }
          else if (!(S[1].YUV.a & 0xf000)) {
               cb   = ((*Dvu >> 8)   + ((S[1].YUV.u & 0xff00) ? 0xff : S[1].YUV.u)) >> 1;
               cr   = ((*Dvu & 0xff) + ((S[1].YUV.v & 0xff00) ? 0xff : S[1].YUV.v)) >> 1;
               *Dvu = cr | (cb << 8);
          }

          S += 2;
          ++Dvu;
     }
}

static void
Sacc_to_Aop_nv24( GenefxState *gfxs )
{
     int                w   = gfxs->length+1;
     GenefxAccumulator *S   = gfxs->Sacc;
     u8                *Dy  = gfxs->Aop[0];
     u16               *Duv = gfxs->Aop[1];

     while (--w) {
          if (!(S->YUV.a & 0xf000)) {
               *Dy = (S->YUV.y & 0xff00) ? 0xff : S->YUV.y;
          }

          ++S;
          ++Dy;
     }

     w = gfxs->length + 1;
     S = gfxs->Sacc;

     while (--w) {
          u32 cb, cr;

          if (!(S[0].YUV.a & 0xf000)) {
               cb   = ((*Duv & 0xff) + ((S[0].YUV.u & 0xff00) ? 0xff : S[0].YUV.u)) >> 1;
               cr   = ((*Duv >> 8)   + ((S[0].YUV.v & 0xff00) ? 0xff : S[0].YUV.v)) >> 1;
               *Duv = cb | (cr << 8);
          }

          S++;
          ++Duv;
     }
}

static void
Sacc_to_Aop_nv42( GenefxState *gfxs )
{
     int                w   = gfxs->length+1;
     GenefxAccumulator *S   = gfxs->Sacc;
     u8                *Dy  = gfxs->Aop[0];
     u16               *Dvu = gfxs->Aop[1];

     while (--w) {
          if (!(S->YUV.a & 0xf000)) {
               *Dy = (S->YUV.y & 0xff00) ? 0xff : S->YUV.y;
          }

          ++S;
          ++Dy;
     }

     w = gfxs->length + 1;
     S = gfxs->Sacc;

     while (--w) {
          u32 cb, cr;

          if (!(S[0].YUV.a & 0xf000)) {
               cb   = ((*Dvu >> 8)   + ((S[0].YUV.u & 0xff00) ? 0xff : S[0].YUV.u)) >> 1;
               cr   = ((*Dvu & 0xff) + ((S[0].YUV.v & 0xff00) ? 0xff : S[0].YUV.v)) >> 1;
               *Dvu = cr | (cb << 8);
          }

          S++;
          ++Dvu;
     }
}

static GenefxFunc Sacc_to_Aop_PFI[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1555)]   = Sacc_to_Aop_argb1555,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB16)]      = Sacc_to_Aop_rgb16,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB24)]      = Sacc_to_Aop_rgb24,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB32)]      = Sacc_to_Aop_rgb32,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]       = Sacc_to_Aop_argb,
     [DFB_PIXELFORMAT_INDEX(DSPF_A8)]         = Sacc_to_Aop_a8,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUY2)]       = Sacc_to_Aop_yuy2,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB332)]     = Sacc_to_Aop_rgb332,
     [DFB_PIXELFORMAT_INDEX(DSPF_UYVY)]       = Sacc_to_Aop_uyvy,
     [DFB_PIXELFORMAT_INDEX(DSPF_I420)]       = Sacc_to_Aop_i420,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV12)]       = Sacc_to_Aop_i420,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT8)]       = Sacc_to_Aop_lut8,
     [DFB_PIXELFORMAT_INDEX(DSPF_ALUT44)]     = Sacc_to_Aop_alut44,
     [DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)]      = Sacc_to_Aop_airgb,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV12)]       = Sacc_to_Aop_nv12,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV16)]       = Sacc_to_Aop_nv16,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB2554)]   = Sacc_to_Aop_argb2554,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)]   = Sacc_to_Aop_argb4444,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA4444)]   = Sacc_to_Aop_rgba4444,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV21)]       = Sacc_to_Aop_nv21,
     [DFB_PIXELFORMAT_INDEX(DSPF_AYUV)]       = Sacc_to_Aop_ayuv,
     [DFB_PIXELFORMAT_INDEX(DSPF_A4)]         = Sacc_to_Aop_a4,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1666)]   = Sacc_to_Aop_argb1666,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB6666)]   = Sacc_to_Aop_argb6666,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB18)]      = Sacc_to_Aop_rgb18,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB444)]     = Sacc_to_Aop_xrgb4444,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB555)]     = Sacc_to_Aop_xrgb1555,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR555)]     = Sacc_to_Aop_xbgr1555,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA5551)]   = Sacc_to_Aop_rgba5551,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y444)]       = Sacc_to_Aop_y444,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB8565)]   = Sacc_to_Aop_argb8565,
     [DFB_PIXELFORMAT_INDEX(DSPF_AVYU)]       = Sacc_to_Aop_avyu,
     [DFB_PIXELFORMAT_INDEX(DSPF_VYU)]        = Sacc_to_Aop_vyu,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1_LSB)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV16)]       = Sacc_to_Aop_y42b,
     [DFB_PIXELFORMAT_INDEX(DSPF_ABGR)]       = Sacc_to_Aop_abgr,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBAF88871)] = Sacc_to_Aop_rgbaf88871,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT1)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV61)]       = Sacc_to_Aop_nv61,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y42B)]       = Sacc_to_Aop_y42b,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV24)]       = Sacc_to_Aop_y444,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV24)]       = Sacc_to_Aop_nv24,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV42)]       = Sacc_to_Aop_nv42,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR24)]      = Sacc_to_Aop_bgr24,
};

/**********************************************************************************************************************
 ********************************* Sacc_toK_Aop_PFI *******************************************************************
 **********************************************************************************************************************/

static void
Sacc_toK_Aop_a8( GenefxState *gfxs )
{
     int                w = gfxs->length + 1;
     GenefxAccumulator *S = gfxs->Sacc;
     u8                *D = gfxs->Aop[0];

     while (--w) {
          if (!(S->RGB.a & 0xf000)) {
               *D = (S->RGB.a & 0xff00) ? 0xff : S->RGB.a;
          }

          ++S;
          ++D;
     }
}

static void
Sacc_toK_Aop_yuy2( GenefxState *gfxs )
{
     int                l;
     int                w     = gfxs->length;
     GenefxAccumulator *S     = gfxs->Sacc;
     u16               *D     = gfxs->Aop[0];
     u32                Dkey  = gfxs->Dkey;
#ifdef WORDS_BIGENDIAN
     u16                Dkey0 = gfxs->Dkey >> 16;
     u16                Dkey1 = gfxs->Dkey & 0xffff;
#else
     u16                Dkey0 = gfxs->Dkey & 0xffff;
     u16                Dkey1 = gfxs->Dkey >> 16;
#endif

     if ((long) D & 2) {
          if (!(S->YUV.a & 0xf000) && (*D == Dkey1)) {
               *D = ((S->YUV.y & 0xff00) ? 0x00ff : S->YUV.y) | ((S->YUV.v & 0xff00) ? 0xff00 : (S->YUV.v << 8));
          }

          ++S;
          ++D;
          --w;
     }

     for (l = w >> 1; l--;) {
          if (*D == Dkey) {
               if (!(S[0].YUV.a & 0xf000) && !(S[1].YUV.a & 0xf000)) {
                    u32 y0, cb, y1, cr;

                    y0 = (S[0].YUV.y & 0xff00) ? 0xff : S[0].YUV.y;
                    y1 = (S[1].YUV.y & 0xff00) ? 0xff : S[1].YUV.y;

                    cb = (S[0].YUV.u + S[1].YUV.u) >> 1;
                    if (cb & 0xff00)
                         cb = 0xff;

                    cr = (S[0].YUV.v + S[1].YUV.v) >> 1;
                    if (cr & 0xff00)
                         cr = 0xff;

#ifdef WORDS_BIGENDIAN
                    *((u32*) D) = y1 | (cr << 8) | (y0 << 16) | (cb << 24);
#else
                    *((u32*) D) = y0 | (cb << 8) | (y1 << 16) | (cr << 24);
#endif
               }
               else if (!(S[0].YUV.a & 0xf000)) {
                    D[0] = ((S[0].YUV.y & 0xff00) ? 0x00ff :  S[0].YUV.y) |
                           ((S[0].YUV.u & 0xff00) ? 0xff00 : (S[0].YUV.u << 8));
               }
               else if (!(S[1].YUV.a & 0xf000)) {
                    D[1] = ((S[1].YUV.y & 0xff00) ? 0x00ff :  S[1].YUV.y) |
                           ((S[1].YUV.v & 0xff00) ? 0xff00 : (S[1].YUV.v << 8));
               }
          }

          S += 2;
          D += 2;
     }

     if (w & 1) {
          if (!(S->YUV.a & 0xf000) && (*D == Dkey0)) {
               *D = ((S->YUV.y & 0xff00) ? 0x00ff : S->YUV.y) | ((S->YUV.u & 0xff00) ? 0xff00 : (S->YUV.u << 8));
          }
     }
}

static void
Sacc_toK_Aop_rgb332( GenefxState *gfxs )
{
     int                w    = gfxs->length + 1;
     GenefxAccumulator *S    = gfxs->Sacc;
     u8                *D    = gfxs->Aop[0];
     u32                Dkey = gfxs->Dkey;

     while (--w) {
          if (!(S->RGB.a & 0xf000) && (*D == Dkey)) {
               *D = PIXEL_RGB332( (S->RGB.r & 0xff00) ? 0xff : S->RGB.r,
                                  (S->RGB.g & 0xff00) ? 0xff : S->RGB.g,
                                  (S->RGB.b & 0xff00) ? 0xff : S->RGB.b );
          }

          ++S;
          ++D;
     }
}

static void
Sacc_toK_Aop_uyvy( GenefxState *gfxs )
{
     int                l;
     int                w     = gfxs->length;
     GenefxAccumulator *S     = gfxs->Sacc;
     u16               *D     = gfxs->Aop[0];
     u32                Dkey  = gfxs->Dkey;
#ifdef WORDS_BIGENDIAN
     u16                Dkey0 = gfxs->Dkey >> 16;
     u16                Dkey1 = gfxs->Dkey & 0xffff;
#else
     u16                Dkey0 = gfxs->Dkey & 0xffff;
     u16                Dkey1 = gfxs->Dkey >> 16;
#endif

     if ((long) D & 2) {
          if (!(S->YUV.a & 0xf000) && (*D == Dkey1)) {
               *D = ((S->YUV.v & 0xff00) ? 0x00ff : S->YUV.v) | ((S->YUV.y & 0xff00) ? 0xff00 : (S->YUV.y << 8));
          }

          ++S;
          ++D;
          --w;
     }

     for (l = w >> 1; l--;) {
          if (*D == Dkey) {
               if (!(S[0].YUV.a & 0xf000) && !(S[1].YUV.a & 0xf000)) {
                    u32 cb, y0, cr, y1;

                    y0 = (S[0].YUV.y & 0xff00) ? 0xff : S[0].YUV.y;
                    y1 = (S[1].YUV.y & 0xff00) ? 0xff : S[1].YUV.y;

                    cb = (S[0].YUV.u + S[1].YUV.u) >> 1;
                    if (cb & 0xff00)
                         cb = 0xff;

                    cr = (S[0].YUV.v + S[1].YUV.v) >> 1;
                    if (cr & 0xff00)
                         cr = 0xff;

#ifdef WORDS_BIGENDIAN
                    *((u32*) D) = cr | (y1 << 8) | (cb << 16) | (y0 << 24);
#else
                    *((u32*) D) = cb | (y0 << 8) | (cr << 16) | (y1 << 24);
#endif
               }
               else if (!(S[0].YUV.a & 0xf000)) {
                    D[0] = ((S[0].YUV.u & 0xff00) ? 0x00ff :  S[0].YUV.u) |
                           ((S[0].YUV.y & 0xff00) ? 0xff00 : (S[0].YUV.y << 8));
               }
               else if (!(S[1].YUV.a & 0xf000)) {
                    D[1] = ((S[1].YUV.v & 0xff00) ? 0x00ff :  S[1].YUV.v) |
                           ((S[1].YUV.y & 0xff00) ? 0xff00 : (S[1].YUV.y << 8));
               }
          }

          S += 2;
          D += 2;
     }

     if (w & 1) {
          if (!(S->YUV.a & 0xf000) && (*D == Dkey0)) {
               *D = ((S->YUV.u & 0xff00) ? 0x00ff : S->YUV.u) | ((S->YUV.y & 0xff00) ? 0xff00 : (S->YUV.y << 8));
          }
     }
}

static void
Sacc_toK_Aop_lut8( GenefxState *gfxs )
{
     int                w    = gfxs->length + 1;
     GenefxAccumulator *S    = gfxs->Sacc;
     u8                *D    = gfxs->Aop[0];
     u32                Dkey = gfxs->Dkey;

     while (--w) {
          if (!(S->RGB.a & 0xf000) && (*D == Dkey)) {
               *D = dfb_palette_search( gfxs->Alut,
                                        (S->RGB.r & 0xff00) ? 0xff : S->RGB.r,
                                        (S->RGB.g & 0xff00) ? 0xff : S->RGB.g,
                                        (S->RGB.b & 0xff00) ? 0xff : S->RGB.b,
                                        (S->RGB.a & 0xff00) ? 0xff : S->RGB.a );
          }

          ++S;
          ++D;
     }
}

static void
Sacc_toK_Aop_alut44( GenefxState *gfxs )
{
     int                w    = gfxs->length + 1;
     GenefxAccumulator *S    = gfxs->Sacc;
     u8                *D    = gfxs->Aop[0];
     u32                Dkey = gfxs->Dkey;

     while (--w) {
          if (!(S->RGB.a & 0xf000) && ((*D & 0x0f) == Dkey)) {
               *D = (S->RGB.a & 0xff00) ? 0xf0 :
                    (S->RGB.a & 0xf0) + dfb_palette_search( gfxs->Alut,
                                                            (S->RGB.r & 0xff00) ? 0xff : S->RGB.r,
                                                            (S->RGB.g & 0xff00) ? 0xff : S->RGB.g,
                                                            (S->RGB.b & 0xff00) ? 0xff : S->RGB.b,
                                                            0x80 );
          }

          ++S;
          ++D;
     }
}

static void
Sacc_toK_Aop_y444( GenefxState *gfxs )
{
     int                w    = gfxs->length + 1;
     GenefxAccumulator *S    = gfxs->Sacc;
     u8                *Dy   = gfxs->Aop[0];
     u8                *Du   = gfxs->Aop[1];
     u8                *Dv   = gfxs->Aop[2];
     u32                Dkey = gfxs->Dkey;

     while (--w) {
          u8 dy = *Dy;
          u8 du = *Du;
          u8 dv = *Dv;

          if (!(S->YUV.a & 0xf000) && Dkey == (u32) (dy << 16 | du << 8 | dv)) {
               *Dy = (S->YUV.y & 0xff00) ? 0xff : S->YUV.y;
               *Du = (S->YUV.u & 0xff00) ? 0xff : S->YUV.u;
               *Dv = (S->YUV.v & 0xff00) ? 0xff : S->YUV.v;
          }

          ++S;
          ++Dy; ++Du; ++Dv;
     }
}

static void
Sacc_toK_Aop_avyu( GenefxState *gfxs )
{
     int                w    = gfxs->length + 1;
     GenefxAccumulator *S    = gfxs->Sacc;
     u32               *D    = gfxs->Aop[0];
     u32                Dkey = gfxs->Dkey;

     while (--w) {
          if (!(S->YUV.a & 0xf000) && (*D & 0x00ffffff) == Dkey) {
               *D = PIXEL_AVYU( (S->YUV.a & 0xff00) ? 0xff : S->YUV.a,
                                (S->YUV.y & 0xff00) ? 0xff : S->YUV.y,
                                (S->YUV.u & 0xff00) ? 0xff : S->YUV.u,
                                (S->YUV.v & 0xff00) ? 0xff : S->YUV.v );
          }

          ++S;
          ++D;
     }
}

static void
Sacc_toK_Aop_vyu( GenefxState *gfxs )
{
     int                w    = gfxs->length + 1;
     GenefxAccumulator *S    = gfxs->Sacc;
     u8                *D    = gfxs->Aop[0];
     u32                Dkey = gfxs->Dkey;

     while (--w) {
#ifdef WORDS_BIGENDIAN
          u32 d = D[0] << 16 | D[1] << 8 | D[2];
#else
          u32 d = D[2] << 16 | D[1] << 8 | D[0];
#endif

          if (!(S->YUV.a & 0xf000) && Dkey == d) {
               u8 y = (S->YUV.y & 0xff00) ? 0xff : S->YUV.y;
               u8 u = (S->YUV.u & 0xff00) ? 0xff : S->YUV.u;
               u8 v = (S->YUV.v & 0xff00) ? 0xff : S->YUV.v;

#ifdef WORDS_BIGENDIAN
               D[0] = v;
               D[1] = y;
               D[2] = u;
#else
               D[0] = u;
               D[1] = y;
               D[2] = v;
#endif
          }

          ++S;
          D += 3;
     }
}

static GenefxFunc Sacc_toK_Aop_PFI[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1555)]   = Sacc_toK_Aop_argb1555,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB16)]      = Sacc_toK_Aop_rgb16,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB24)]      = Sacc_toK_Aop_rgb24,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB32)]      = Sacc_toK_Aop_rgb32,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]       = Sacc_toK_Aop_argb,
     [DFB_PIXELFORMAT_INDEX(DSPF_A8)]         = Sacc_toK_Aop_a8,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUY2)]       = Sacc_toK_Aop_yuy2,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB332)]     = Sacc_toK_Aop_rgb332,
     [DFB_PIXELFORMAT_INDEX(DSPF_UYVY)]       = Sacc_toK_Aop_uyvy,
     [DFB_PIXELFORMAT_INDEX(DSPF_I420)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT8)]       = Sacc_toK_Aop_lut8,
     [DFB_PIXELFORMAT_INDEX(DSPF_ALUT44)]     = Sacc_toK_Aop_alut44,
     [DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)]      = Sacc_toK_Aop_airgb,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB2554)]   = Sacc_toK_Aop_argb2554,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)]   = Sacc_toK_Aop_argb4444,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA4444)]   = Sacc_toK_Aop_rgba4444,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV21)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AYUV)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A4)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1666)]   = Sacc_toK_Aop_argb1666,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB6666)]   = Sacc_toK_Aop_argb6666,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB18)]      = Sacc_toK_Aop_rgb18,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB444)]     = Sacc_toK_Aop_xrgb4444,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB555)]     = Sacc_toK_Aop_xrgb1555,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR555)]     = Sacc_toK_Aop_xbgr1555,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA5551)]   = Sacc_toK_Aop_rgba5551,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y444)]       = Sacc_toK_Aop_y444,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB8565)]   = Sacc_toK_Aop_argb8565,
     [DFB_PIXELFORMAT_INDEX(DSPF_AVYU)]       = Sacc_toK_Aop_avyu,
     [DFB_PIXELFORMAT_INDEX(DSPF_VYU)]        = Sacc_toK_Aop_vyu,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1_LSB)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ABGR)]       = Sacc_toK_Aop_abgr,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBAF88871)] = Sacc_toK_Aop_rgbaf88871,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT1)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV61)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y42B)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV24)]       = Sacc_toK_Aop_y444,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV24)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV42)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR24)]      = Sacc_toK_Aop_bgr24,
};

/**********************************************************************************************************************
 ********************************* Sacc_Sto_Aop_PFI *******************************************************************
 **********************************************************************************************************************/

static void
Sacc_Sto_Aop_a8( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     GenefxAccumulator *S     = gfxs->Sacc;
     u8                *D     = gfxs->Aop[0];
     int                SperD = gfxs->SperD;

     while (--w) {
          GenefxAccumulator *S0 = &S[i>>16];

          if (!(S0->RGB.a & 0xf000))
               *D = (S0->RGB.a & 0xff00) ? 0xff : S0->RGB.a;

          ++D;
          i += SperD;
     }
}

static void
Sacc_Sto_Aop_yuy2( GenefxState *gfxs )
{
     int                l;
     int                i      = gfxs->Xphase;
     int                w      = gfxs->length;
     GenefxAccumulator *S      = gfxs->Sacc;
     u16               *D      = gfxs->Aop[0];
     int                SperD  = gfxs->SperD;
     int                SperD2 = gfxs->SperD << 1;

     if ((long) D & 2) {
          if (!(S->YUV.a & 0xf00)) {
               *D = ((S->YUV.y & 0xff00) ? 0x00ff : S->YUV.y) | ((S->YUV.v & 0xff00) ? 0xff00 : (S->YUV.v << 8));
          }

          ++D;
          i = SperD;
          --w;
     }

     for (l = w >> 1; l--;) {
          GenefxAccumulator *S0 = &S[i>>16];
          GenefxAccumulator *S1 = &S[(i+SperD)>>16];

          if (!(S0->YUV.a & 0xf000) && !(S1->YUV.a & 0xf000)) {
               u32 y0, cb, y1, cr;

               y0 = (S0->YUV.y & 0xff00) ? 0xff : S0->YUV.y;
               y1 = (S1->YUV.y & 0xff00) ? 0xff : S1->YUV.y;

               cb = (S0->YUV.u + S1->YUV.u) >> 1;
               if (cb & 0xff00)
                    cb = 0xff;

               cr = (S0->YUV.v + S1->YUV.v) >> 1;
               if (cr & 0xff00)
                    cr = 0xff;

#ifdef WORDS_BIGENDIAN
               *((u32*) D) = y1 | (cr << 8) | (y0 << 16) | (cb << 24);
#else
               *((u32*) D) = y0 | (cb << 8) | (y1 << 16) | (cr << 24);
#endif
          }
          else if (!(S0->YUV.a & 0xf000)) {
               D[0] = ((S0->YUV.y & 0xff00) ? 0x00ff : S0->YUV.y) | ((S0->YUV.u & 0xff00) ? 0xff00 : (S0->YUV.u << 8));
          }
          else if (!(S1->YUV.a & 0xf000)) {
               D[1] = ((S1->YUV.y & 0xff00) ? 0x00ff : S1->YUV.y) | ((S1->YUV.v & 0xff00) ? 0xff00 : (S1->YUV.v << 8));
          }

          D += 2;
          i += SperD2;
     }

     if (w & 1) {
          GenefxAccumulator *S0 = &S[i>>16];
          if (!(S0->YUV.a & 0xf00)) {
               *D = ((S0->YUV.y & 0xff00) ? 0x00ff : S0->YUV.y) | ((S0->YUV.u & 0xff00) ? 0xff00 : (S0->YUV.u << 8));
          }
     }
}

static void
Sacc_Sto_Aop_rgb332( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     GenefxAccumulator *S     = gfxs->Sacc;
     u8                *D     = gfxs->Aop[0];
     int                SperD = gfxs->SperD;

     while (--w) {
          GenefxAccumulator *S0 = &S[i>>16];

          if (!(S0->RGB.a & 0xf000)) {
               *D = PIXEL_RGB332( (S0->RGB.r & 0xff00) ? 0xff : S0->RGB.r,
                                  (S0->RGB.g & 0xff00) ? 0xff : S0->RGB.g,
                                  (S0->RGB.b & 0xff00) ? 0xff : S0->RGB.b );
          }

          ++D;
          i += SperD;
     }
}

static void
Sacc_Sto_Aop_uyvy( GenefxState *gfxs )
{
     int                l;
     int                i      = gfxs->Xphase;
     int                w      = gfxs->length;
     GenefxAccumulator *S      = gfxs->Sacc;
     u16               *D      = gfxs->Aop[0];
     int                SperD  = gfxs->SperD;
     int                SperD2 = gfxs->SperD << 1;

     if ((long) D & 2) {
          if (!(S->YUV.a & 0xf00)) {
               *D = ((S->YUV.v & 0xff00) ? 0x00ff : S->YUV.v) | ((S->YUV.y & 0xff00) ? 0xff00 : (S->YUV.y << 8));
          }

          ++D;
          i = SperD;
          --w;
     }

     for (l = w >> 1; l--;) {
          GenefxAccumulator *S0 = &S[i>>16];
          GenefxAccumulator *S1 = &S[(i+SperD)>>16];

          if (!(S0->YUV.a & 0xf000) && !(S1->YUV.a & 0xf000)) {
               u32 cb, y0, cr, y1;

               y0 = (S0->YUV.y & 0xff00) ? 0xff : S0->YUV.y;
               y1 = (S1->YUV.y & 0xff00) ? 0xff : S1->YUV.y;

               cb = (S0->YUV.u + S1->YUV.u) >> 1;
               if (cb & 0xff00)
                    cb = 0xff;

               cr = (S0->YUV.v + S1->YUV.v) >> 1;
               if (cr & 0xff00)
                    cr = 0xff;

#ifdef WORDS_BIGENDIAN
               *((u32*) D) = cr | (y1 << 8) | (cb << 16) | (y0 << 24);
#else
               *((u32*) D) = cb | (y0 << 8) | (cr << 16) | (y1 << 24);
#endif
          }
          else if (!(S0->YUV.a & 0xf000)) {
               D[0] = ((S0->YUV.u & 0xff00) ? 0x00ff : S0->YUV.u) | ((S0->YUV.y & 0xff00) ? 0xff00 : (S0->YUV.y << 8));
          }
          else if (!(S1->YUV.a & 0xf000)) {
               D[1] = ((S1->YUV.v & 0xff00) ? 0x00ff : S1->YUV.v) | ((S1->YUV.y & 0xff00) ? 0xff00 : (S1->YUV.y << 8));
          }

          D += 2;
          i += SperD2;
     }

     if (w & 1) {
          GenefxAccumulator *S0 = &S[i>>16];
          if (!(S0->YUV.a & 0xf00)) {
               *D = ((S0->YUV.u & 0xff00) ? 0x00ff : S0->YUV.u) | ((S0->YUV.y & 0xff00) ? 0xff00 : (S0->YUV.y << 8));
          }
     }
}

static void
Sacc_Sto_Aop_i420( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     GenefxAccumulator *S     = gfxs->Sacc;
     u8                *Dy    = gfxs->Aop[0];
     int                SperD = gfxs->SperD;

     while (--w) {
          GenefxAccumulator *S0 = &S[i>>16];

          if (!(S0->YUV.a & 0xf000)) {
               *Dy = (S0->YUV.y & 0xff00) ? 0xff : S0->YUV.y;
          }

          ++Dy;
          i += SperD;
     }

     if (gfxs->AopY & 1) {
          u8 *Du = gfxs->Aop[1];
          u8 *Dv = gfxs->Aop[2];

          i =  gfxs->Xphase >> 1;
          w = (gfxs->length >> 1) + 1;

          while (--w) {
               GenefxAccumulator *S0 = &S[i>>16];
               GenefxAccumulator *S1 = &S[(i+SperD)>>16];

               if (!(S0->YUV.a & 0xf000) && !(S1->YUV.a & 0xf000)) {
                    u32 tmp;

                    tmp = (S0->YUV.u + S1->YUV.u) >> 1;
                    if (tmp & 0xff00)
                         tmp = 0xff;
                    *Du = tmp;

                    tmp = (S0->YUV.v + S1->YUV.v) >> 1;
                    if (tmp & 0xff00)
                         tmp = 0xff;
                    *Dv = tmp;

               }
               else if (!(S0->YUV.a & 0xf000)) {
                    *Du = (*Du + ((S0->YUV.u & 0xff00) ? 0xff : S0->YUV.u)) >> 1;
                    *Dv = (*Dv + ((S0->YUV.v & 0xff00) ? 0xff : S0->YUV.v)) >> 1;
               }
               else if (!(S1->YUV.a & 0xf000)) {
                    *Du = (*Du + ((S1->YUV.u & 0xff00) ? 0xff : S1->YUV.u)) >> 1;
                    *Dv = (*Dv + ((S1->YUV.v & 0xff00) ? 0xff : S1->YUV.v)) >> 1;
               }

               ++Du; ++Dv;
               i += SperD << 1;
          }
     }
}

static void
Sacc_Sto_Aop_lut8( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     GenefxAccumulator *S     = gfxs->Sacc;
     u8                *D     = gfxs->Aop[0];
     int                SperD = gfxs->SperD;

     while (--w) {
          GenefxAccumulator *S0 = &S[i>>16];

          if (!(S0->RGB.a & 0xf000)) {
               *D = dfb_palette_search( gfxs->Alut,
                                        (S0->RGB.r & 0xff00) ? 0xff : S0->RGB.r,
                                        (S0->RGB.g & 0xff00) ? 0xff : S0->RGB.g,
                                        (S0->RGB.b & 0xff00) ? 0xff : S0->RGB.b,
                                        (S0->RGB.a & 0xff00) ? 0xff : S0->RGB.a );
          }

          ++D;
          i += SperD;
     }
}

static void
Sacc_Sto_Aop_alut44( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     GenefxAccumulator *S     = gfxs->Sacc;
     u8                *D     = gfxs->Aop[0];
     int                SperD = gfxs->SperD;

     while (--w) {
          GenefxAccumulator *S0 = &S[i>>16];

          if (!(S0->RGB.a & 0xf000)) {
               *D = (S0->RGB.a & 0xff00) ?
                    0xf0 : (S0->RGB.a & 0xf0) + dfb_palette_search( gfxs->Alut,
                                                                   (S0->RGB.r & 0xff00) ? 0xff : S0->RGB.r,
                                                                   (S0->RGB.g & 0xff00) ? 0xff : S0->RGB.g,
                                                                   (S0->RGB.b & 0xff00) ? 0xff : S0->RGB.b,
                                                                   0x80 );
          }

          ++D;
          i += SperD;
     }
}

static void
Sacc_Sto_Aop_nv12( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     GenefxAccumulator *S     = gfxs->Sacc;
     u8                *Dy    = gfxs->Aop[0];
     int                SperD = gfxs->SperD;

     while (--w) {
          GenefxAccumulator *S0 = &S[i>>16];

          if (!(S0->YUV.a & 0xf000)) {
               *Dy = (S0->YUV.y & 0xff00) ? 0xff : S0->YUV.y;
          }

          ++Dy;
          i += SperD;
     }

     if (gfxs->AopY & 1) {
          u16 *Duv = gfxs->Aop[1];

          i =  gfxs->Xphase >> 1;
          w = (gfxs->length >> 1) + 1;

          while (--w) {
               GenefxAccumulator *S0 = &S[i>>16];
               GenefxAccumulator *S1 = &S[(i+SperD)>>16];
               u32                cb, cr;

               if (!(S0->YUV.a & 0xf000) && !(S1->YUV.a & 0xf000)) {
                    cb = (S0->YUV.u + S1->YUV.u) >> 1;
                    if (cb & 0xff00)
                         cb = 0xff;

                    cr = (S0->YUV.v + S1->YUV.v) >> 1;
                    if (cr & 0xff00)
                         cr = 0xff;

                    *Duv = cb | (cr << 8);
               }
               else if (!(S0->YUV.a & 0xf000)) {
                    cb   = ((*Duv & 0xff) + ((S0->YUV.u & 0xff00) ? 0xff : S0->YUV.u)) >> 1;
                    cr   = ((*Duv >> 8)   + ((S0->YUV.v & 0xff00) ? 0xff : S0->YUV.v)) >> 1;
                    *Duv = cb | (cr << 8);
               }
               else if (!(S1->YUV.a & 0xf000)) {
                    cb   = ((*Duv & 0xff) + ((S1->YUV.u & 0xff00) ? 0xff : S1->YUV.u)) >> 1;
                    cr   = ((*Duv >> 8)   + ((S1->YUV.v & 0xff00) ? 0xff : S1->YUV.v)) >> 1;
                    *Duv = cb | (cr << 8);
               }

               ++Duv;
               i += SperD << 1;
          }
     }
}

static void
Sacc_Sto_Aop_nv16( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     GenefxAccumulator *S     = gfxs->Sacc;
     u8                *Dy    = gfxs->Aop[0];
     u16               *Duv   = gfxs->Aop[1];
     int                SperD = gfxs->SperD;

     while (--w) {
          GenefxAccumulator *S0 = &S[i>>16];

          if (!(S0->YUV.a & 0xf000)) {
               *Dy = (S0->YUV.y & 0xff00) ? 0xff : S0->YUV.y;
          }

          ++Dy;
          i += SperD;
     }

     i =  gfxs->Xphase >> 1;
     w = (gfxs->length >> 1) + 1;

     while (--w) {
          GenefxAccumulator *S0 = &S[i>>16];
          GenefxAccumulator *S1 = &S[(i+SperD)>>16];
          u32                cb, cr;

          if (!(S0->YUV.a & 0xf000) && !(S1->YUV.a & 0xf000)) {
               cb = (S0->YUV.u + S1->YUV.u) >> 1;
               if (cb & 0xff00)
                    cb = 0xff;

               cr = (S0->YUV.v + S1->YUV.v) >> 1;
               if (cr & 0xff00)
                    cr = 0xff;

               *Duv = cb | (cr << 8);
          }
          else if (!(S0->YUV.a & 0xf000)) {
               cb   = ((*Duv & 0xff) + ((S0->YUV.u & 0xff00) ? 0xff : S0->YUV.u)) >> 1;
               cr   = ((*Duv >> 8)   + ((S0->YUV.v & 0xff00) ? 0xff : S0->YUV.v)) >> 1;
               *Duv = cb | (cr << 8);
          }
          else if (!(S1->YUV.a & 0xf000)) {
               cb   = ((*Duv & 0xff) + ((S1->YUV.u & 0xff00) ? 0xff : S1->YUV.u)) >> 1;
               cr   = ((*Duv >> 8)   + ((S1->YUV.v & 0xff00) ? 0xff : S1->YUV.v)) >> 1;
               *Duv = cb | (cr << 8);
          }

          ++Duv;
          i += SperD << 1;
     }
}

static void
Sacc_Sto_Aop_nv21( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     GenefxAccumulator *S     = gfxs->Sacc;
     u8                *Dy    = gfxs->Aop[0];
     int                SperD = gfxs->SperD;

     while (--w) {
          GenefxAccumulator *S0 = &S[i>>16];

          if (!(S0->YUV.a & 0xf000)) {
               *Dy = (S0->YUV.y & 0xff00) ? 0xff : S0->YUV.y;
          }

          ++Dy;
          i += SperD;
     }

     if (gfxs->AopY & 1) {
          u16 *Dvu = gfxs->Aop[1];

          i =  gfxs->Xphase >> 1;
          w = (gfxs->length >> 1) + 1;

          while (--w) {
               GenefxAccumulator *S0 = &S[i>>16];
               GenefxAccumulator *S1 = &S[(i+SperD)>>16];
               u32                cb, cr;

               if (!(S0->YUV.a & 0xf000) && !(S1->YUV.a & 0xf000)) {
                    cb = (S0->YUV.u + S1->YUV.u) >> 1;
                    if (cb & 0xff00)
                         cb = 0xff;

                    cr = (S0->YUV.v + S1->YUV.v) >> 1;
                    if (cr & 0xff00)
                         cr = 0xff;

                    *Dvu = cr | (cb << 8);
               }
               else if (!(S0->YUV.a & 0xf000)) {
                    cb   = ((*Dvu >> 8)   + ((S0->YUV.u & 0xff00) ? 0xff : S0->YUV.u)) >> 1;
                    cr   = ((*Dvu & 0xff) + ((S0->YUV.v & 0xff00) ? 0xff : S0->YUV.v)) >> 1;
                    *Dvu = cr | (cb << 8);
               }
               else if (!(S1->YUV.a & 0xf000)) {
                    cb   = ((*Dvu >> 8)   + ((S1->YUV.u & 0xff00) ? 0xff : S1->YUV.u)) >> 1;
                    cr   = ((*Dvu & 0xff) + ((S1->YUV.v & 0xff00) ? 0xff : S1->YUV.v)) >> 1;
                    *Dvu = cr | (cb << 8);
               }

               ++Dvu;
               i += SperD << 1;
          }
     }
}

static void
Sacc_Sto_Aop_ayuv( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     GenefxAccumulator *S     = gfxs->Sacc;
     u32               *D     = gfxs->Aop[0];
     int                SperD = gfxs->SperD;

     while (--w) {
          GenefxAccumulator *S0 = &S[i>>16];

          if (!(S0->YUV.a & 0xf000)) {
               *D = PIXEL_AYUV( (S0->YUV.a & 0xff00) ? 0xff : S0->YUV.a,
                                (S0->YUV.y & 0xff00) ? 0xff : S0->YUV.y,
                                (S0->YUV.u & 0xff00) ? 0xff : S0->YUV.u,
                                (S0->YUV.v & 0xff00) ? 0xff : S0->YUV.v );
          }

          ++D;
          i += SperD;
     }
}

static void
Sacc_Sto_Aop_y444( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     GenefxAccumulator *S     = gfxs->Sacc;
     u8                *Dy    = gfxs->Aop[0];
     u8                *Du    = gfxs->Aop[1];
     u8                *Dv    = gfxs->Aop[2];
     int                SperD = gfxs->SperD;

     while (--w) {
          GenefxAccumulator *S0 = &S[i>>16];

          if (!(S0->YUV.a & 0xf000)) {
               *Dy = (S0->YUV.y & 0xff00) ? 0xff : S0->YUV.y;
               *Du = (S0->YUV.u & 0xff00) ? 0xff : S0->YUV.u;
               *Dv = (S0->YUV.v & 0xff00) ? 0xff : S0->YUV.v;
          }

          ++Dy; ++Du; ++Dv;
          i += SperD;
     }
}

static void
Sacc_Sto_Aop_avyu( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     GenefxAccumulator *S     = gfxs->Sacc;
     u32               *D     = gfxs->Aop[0];
     int                SperD = gfxs->SperD;

     while (--w) {
          GenefxAccumulator *S0 = &S[i>>16];

          if (!(S0->YUV.a & 0xf000)) {
               *D = PIXEL_AVYU( (S0->YUV.a & 0xff00) ? 0xff : S0->YUV.a,
                                (S0->YUV.y & 0xff00) ? 0xff : S0->YUV.y,
                                (S0->YUV.u & 0xff00) ? 0xff : S0->YUV.u,
                                (S0->YUV.v & 0xff00) ? 0xff : S0->YUV.v );
          }

          ++D;
          i += SperD;
     }
}

static void
Sacc_Sto_Aop_vyu( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     GenefxAccumulator *S     = gfxs->Sacc;
     u8                *D     = gfxs->Aop[0];
     int                SperD = gfxs->SperD;

     while (--w) {
          GenefxAccumulator *S0 = &S[i>>16];

          if (!(S0->YUV.a & 0xf000)) {
               u8 y = (S0->YUV.y & 0xff00) ? 0xff : S0->YUV.y;
               u8 u = (S0->YUV.u & 0xff00) ? 0xff : S0->YUV.u;
               u8 v = (S0->YUV.v & 0xff00) ? 0xff : S0->YUV.v;

#ifdef WORDS_BIGENDIAN
               D[0] = v;
               D[1] = y;
               D[2] = u;
#else
               D[0] = u;
               D[1] = y;
               D[2] = v;
#endif
          }

          D += 3;
          i += SperD;
     }
}

static void
Sacc_Sto_Aop_y42b( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     GenefxAccumulator *S     = gfxs->Sacc;
     u8                *Dy    = gfxs->Aop[0];
     u8                *Du    = gfxs->Aop[1];
     u8                *Dv    = gfxs->Aop[2];
     int                SperD = gfxs->SperD;

     while (--w) {
          GenefxAccumulator *S0 = &S[i>>16];

          if (!(S0->YUV.a & 0xf000)) {
               *Dy = (S0->YUV.y & 0xff00) ? 0xff : S0->YUV.y;
          }

          Dy++;
          i += SperD;
     }

     i =  gfxs->Xphase / 2;
     w = (gfxs->length / 2) + 1;

     while (--w) {
          GenefxAccumulator *S0 = &S[i>>16];
          GenefxAccumulator *S1 = &S[(i+SperD)>>16];

          if (!(S0->YUV.a & 0xf000) && !(S1->YUV.a & 0xf000)) {
               u32 tmp;

               tmp = (S0->YUV.u + S1->YUV.u) / 2;
               if (tmp & 0xff00)
                    tmp = 0xff;
               *Du = tmp;

               tmp = (S0->YUV.v + S1->YUV.v) / 2;
               if (tmp & 0xff00)
                    tmp = 0xff;
               *Dv = tmp;
          }
          else if (!(S0->YUV.a & 0xf000)) {
               *Du = (*Du + ((S0->YUV.u & 0xff00) ? 0xff : S0->YUV.u)) / 2;
               *Dv = (*Dv + ((S0->YUV.v & 0xff00) ? 0xff : S0->YUV.v)) / 2;
          }
          else if (!(S1->YUV.a & 0xf000)) {
               *Du = (*Du + ((S1->YUV.u & 0xff00) ? 0xff : S1->YUV.u)) / 2;
               *Dv = (*Dv + ((S1->YUV.v & 0xff00) ? 0xff : S1->YUV.v)) / 2;
          }

          Du++; Dv++;
          i += SperD << 1;
     }
}

static void
Sacc_Sto_Aop_nv61( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     GenefxAccumulator *S     = gfxs->Sacc;
     u8                *Dy    = gfxs->Aop[0];
     u16               *Dvu   = gfxs->Aop[1];
     int                SperD = gfxs->SperD;

     while (--w) {
          GenefxAccumulator *S0 = &S[i>>16];

          if (!(S0->YUV.a & 0xf000)) {
               *Dy = (S0->YUV.y & 0xff00) ? 0xff : S0->YUV.y;
          }

          ++Dy;
          i += SperD;
     }

     i =  gfxs->Xphase >> 1;
     w = (gfxs->length >> 1) + 1;

     while (--w) {
          GenefxAccumulator *S0 = &S[i>>16];
          GenefxAccumulator *S1 = &S[(i+SperD)>>16];
          u32                cb, cr;

          if (!(S0->YUV.a & 0xf000) && !(S1->YUV.a & 0xf000)) {
               cb = (S0->YUV.u + S1->YUV.u) >> 1;
               if (cb & 0xff00)
                    cb = 0xff;

               cr = (S0->YUV.v + S1->YUV.v) >> 1;
               if (cr & 0xff00)
                    cr = 0xff;

               *Dvu = cr | (cb << 8);
          }
          else if (!(S0->YUV.a & 0xf000)) {
               cb   = ((*Dvu >> 8)   + ((S0->YUV.u & 0xff00) ? 0xff : S0->YUV.u)) >> 1;
               cr   = ((*Dvu & 0xff) + ((S0->YUV.v & 0xff00) ? 0xff : S0->YUV.v)) >> 1;
               *Dvu = cr | (cb << 8);
          }
          else if (!(S1->YUV.a & 0xf000)) {
               cb   = ((*Dvu >> 8)   + ((S1->YUV.u & 0xff00) ? 0xff : S1->YUV.u)) >> 1;
               cr   = ((*Dvu & 0xff) + ((S1->YUV.v & 0xff00) ? 0xff : S1->YUV.v)) >> 1;
               *Dvu = cr | (cb << 8);
          }

          ++Dvu;
          i += SperD << 1;
     }
}

static void
Sacc_Sto_Aop_nv24( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     GenefxAccumulator *S     = gfxs->Sacc;
     u8                *Dy    = gfxs->Aop[0];
     u16               *Duv   = gfxs->Aop[1];
     int                SperD = gfxs->SperD;

     while (--w) {
          GenefxAccumulator *S0 = &S[i>>16];

          if (!(S0->YUV.a & 0xf000)) {
               *Dy = (S0->YUV.y & 0xff00) ? 0xff : S0->YUV.y;
          }

          ++Dy;
          i += SperD;
     }

     i = gfxs->Xphase;
     w = gfxs->length + 1;

     while (--w) {
          GenefxAccumulator *S0 = &S[i>>16];
          u32                cb, cr;

          if (!(S0->YUV.a & 0xf000)) {
               cb   = ((*Duv & 0xff) + ((S0->YUV.u & 0xff00) ? 0xff : S0->YUV.u)) >> 1;
               cr   = ((*Duv >> 8)   + ((S0->YUV.v & 0xff00) ? 0xff : S0->YUV.v)) >> 1;
               *Duv = cb | (cr << 8);
          }

          ++Duv;
          i += SperD;
     }
}

static void
Sacc_Sto_Aop_nv42( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     GenefxAccumulator *S     = gfxs->Sacc;
     u8                *Dy    = gfxs->Aop[0];
     u16               *Dvu   = gfxs->Aop[1];
     int                SperD = gfxs->SperD;

     while (--w) {
          GenefxAccumulator *S0 = &S[i>>16];

          if (!(S0->YUV.a & 0xf000)) {
               *Dy = (S0->YUV.y & 0xff00) ? 0xff : S0->YUV.y;
          }

          ++Dy;
          i += SperD;
     }

     i = gfxs->Xphase;
     w = gfxs->length + 1;

     while (--w) {
          GenefxAccumulator *S0 = &S[i>>16];
          u32                cb, cr;

          if (!(S0->YUV.a & 0xf000)) {
               cb   = ((*Dvu >> 8)   + ((S0->YUV.u & 0xff00) ? 0xff : S0->YUV.u)) >> 1;
               cr   = ((*Dvu & 0xff) + ((S0->YUV.v & 0xff00) ? 0xff : S0->YUV.v)) >> 1;
               *Dvu = cr | (cb << 8);
          }

          ++Dvu;
          i += SperD;
     }
}

static GenefxFunc Sacc_Sto_Aop_PFI[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1555)]   = Sacc_Sto_Aop_argb1555,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB16)]      = Sacc_Sto_Aop_rgb16,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB24)]      = Sacc_Sto_Aop_rgb24,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB32)]      = Sacc_Sto_Aop_rgb32,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]       = Sacc_Sto_Aop_argb,
     [DFB_PIXELFORMAT_INDEX(DSPF_A8)]         = Sacc_Sto_Aop_a8,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUY2)]       = Sacc_Sto_Aop_yuy2,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB332)]     = Sacc_Sto_Aop_rgb332,
     [DFB_PIXELFORMAT_INDEX(DSPF_UYVY)]       = Sacc_Sto_Aop_uyvy,
     [DFB_PIXELFORMAT_INDEX(DSPF_I420)]       = Sacc_Sto_Aop_i420,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV12)]       = Sacc_Sto_Aop_i420,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT8)]       = Sacc_Sto_Aop_lut8,
     [DFB_PIXELFORMAT_INDEX(DSPF_ALUT44)]     = Sacc_Sto_Aop_alut44,
     [DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)]      = Sacc_Sto_Aop_airgb,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV12)]       = Sacc_Sto_Aop_nv12,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV16)]       = Sacc_Sto_Aop_nv16,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB2554)]   = Sacc_Sto_Aop_argb2554,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)]   = Sacc_Sto_Aop_argb4444,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA4444)]   = Sacc_Sto_Aop_rgba4444,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV21)]       = Sacc_Sto_Aop_nv21,
     [DFB_PIXELFORMAT_INDEX(DSPF_AYUV)]       = Sacc_Sto_Aop_ayuv,
     [DFB_PIXELFORMAT_INDEX(DSPF_A4)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1666)]   = Sacc_Sto_Aop_argb1666,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB6666)]   = Sacc_Sto_Aop_argb6666,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB18)]      = Sacc_Sto_Aop_rgb18,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB444)]     = Sacc_Sto_Aop_xrgb4444,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB555)]     = Sacc_Sto_Aop_xrgb1555,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR555)]     = Sacc_Sto_Aop_xbgr1555,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA5551)]   = Sacc_Sto_Aop_rgba5551,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y444)]       = Sacc_Sto_Aop_y444,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB8565)]   = Sacc_Sto_Aop_argb8565,
     [DFB_PIXELFORMAT_INDEX(DSPF_AVYU)]       = Sacc_Sto_Aop_avyu,
     [DFB_PIXELFORMAT_INDEX(DSPF_VYU)]        = Sacc_Sto_Aop_vyu,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1_LSB)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV16)]       = Sacc_Sto_Aop_y42b,
     [DFB_PIXELFORMAT_INDEX(DSPF_ABGR)]       = Sacc_Sto_Aop_abgr,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBAF88871)] = Sacc_Sto_Aop_rgbaf88871,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT1)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV61)]       = Sacc_Sto_Aop_nv61,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y42B)]       = Sacc_Sto_Aop_y42b,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV24)]       = Sacc_Sto_Aop_y444,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV24)]       = Sacc_Sto_Aop_nv24,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV42)]       = Sacc_Sto_Aop_nv42,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR24)]      = Sacc_Sto_Aop_bgr24,
};

/**********************************************************************************************************************
 ********************************* Sacc_StoK_Aop_PFI ******************************************************************
 **********************************************************************************************************************/

static void
Sacc_StoK_Aop_y444( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     GenefxAccumulator *S     = gfxs->Sacc;
     u8                *Dy    = gfxs->Aop[0];
     u8                *Du    = gfxs->Aop[1];
     u8                *Dv    = gfxs->Aop[2];
     u32                Dkey  = gfxs->Dkey;
     int                SperD = gfxs->Xphase;

     while (--w) {
          GenefxAccumulator *S0 = &S[i>>16];
          u8                 dy = *Dy;
          u8                 du = *Du;
          u8                 dv = *Dv;

          if (!(S0->YUV.a & 0xf000) && Dkey == (u32) (dy << 16 | du << 8 | dv)) {
               *Dy = (S0->YUV.y & 0xff00) ? 0xff : S0->YUV.y;
               *Du = (S0->YUV.u & 0xff00) ? 0xff : S0->YUV.u;
               *Dv = (S0->YUV.v & 0xff00) ? 0xff : S0->YUV.v;
          }

          ++Dy; ++Du; ++Dv;
          i += SperD;
     }
}

static void
Sacc_StoK_Aop_avyu( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     GenefxAccumulator *S     = gfxs->Sacc;
     u32               *D     = gfxs->Aop[0];
     u32                Dkey  = gfxs->Dkey;
     int                SperD = gfxs->SperD;

     while (--w) {
          GenefxAccumulator *S0 = &S[i>>16];

          if (!(S0->YUV.a & 0xf000) && (*D & 0x00ffffff) == Dkey) {
               *D = PIXEL_AYUV( (S0->YUV.a & 0xff00) ? 0xff : S0->YUV.a,
                                (S0->YUV.y & 0xff00) ? 0xff : S0->YUV.y,
                                (S0->YUV.u & 0xff00) ? 0xff : S0->YUV.u,
                                (S0->YUV.v & 0xff00) ? 0xff : S0->YUV.v );
          }

          ++D;
          i += SperD;
     }
}

static void
Sacc_StoK_Aop_vyu( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     GenefxAccumulator *S     = gfxs->Sacc;
     u8                *D     = gfxs->Aop[0];
     u32                Dkey  = gfxs->Dkey;
     int                SperD = gfxs->SperD;

     while (--w) {
          GenefxAccumulator *S0 = &S[i>>16];

#ifdef WORDS_BIGENDIAN
          u32 d = D[0] << 16 | D[1] << 8 | D[2];
#else
          u32 d = D[2] << 16 | D[1] << 8 | D[0];
#endif

          if (!(S0->YUV.a & 0xf000) && Dkey == d) {
               u8 y = (S0->YUV.y & 0xff00) ? 0xff : S0->YUV.y;
               u8 u = (S0->YUV.u & 0xff00) ? 0xff : S0->YUV.u;
               u8 v = (S0->YUV.v & 0xff00) ? 0xff : S0->YUV.v;

#ifdef WORDS_BIGENDIAN
               D[0] = v;
               D[1] = y;
               D[2] = u;
#else
               D[0] = u;
               D[1] = y;
               D[2] = v;
#endif
          }

          D += 3;
          i += SperD;
     }
}

static GenefxFunc Sacc_StoK_Aop_PFI[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1555)]   = Sacc_StoK_Aop_argb1555,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB16)]      = Sacc_StoK_Aop_rgb16,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB24)]      = Sacc_StoK_Aop_rgb24,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB32)]      = Sacc_StoK_Aop_rgb32,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]       = Sacc_StoK_Aop_argb,
     [DFB_PIXELFORMAT_INDEX(DSPF_A8)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUY2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB332)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_UYVY)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_I420)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT8)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ALUT44)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)]      = Sacc_StoK_Aop_airgb,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB2554)]   = Sacc_StoK_Aop_argb2554,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)]   = Sacc_StoK_Aop_argb4444,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA4444)]   = Sacc_StoK_Aop_rgba4444,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV21)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AYUV)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A4)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1666)]   = Sacc_StoK_Aop_argb1666,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB6666)]   = Sacc_StoK_Aop_argb6666,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB18)]      = Sacc_StoK_Aop_rgb18,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB444)]     = Sacc_StoK_Aop_xrgb4444,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB555)]     = Sacc_StoK_Aop_xrgb1555,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR555)]     = Sacc_StoK_Aop_xbgr1555,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA5551)]   = Sacc_StoK_Aop_rgba5551,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y444)]       = Sacc_StoK_Aop_y444,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB8565)]   = Sacc_StoK_Aop_argb8565,
     [DFB_PIXELFORMAT_INDEX(DSPF_AVYU)]       = Sacc_StoK_Aop_avyu,
     [DFB_PIXELFORMAT_INDEX(DSPF_VYU)]        = Sacc_StoK_Aop_vyu,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1_LSB)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ABGR)]       = Sacc_StoK_Aop_abgr,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBAF88871)] = Sacc_StoK_Aop_rgbaf88871,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT1)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV61)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y42B)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV24)]       = Sacc_StoK_Aop_y444,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV24)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV42)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR24)]      = Sacc_StoK_Aop_bgr24,
};

/**********************************************************************************************************************
 ********************************* Bop_PFI_to_Aop_PFI *****************************************************************
 **********************************************************************************************************************/

static void
Bop_16_to_Aop( GenefxState *gfxs )
{
     direct_memmove( gfxs->Aop[0], gfxs->Bop[0], gfxs->length * 2 );
}

static void
Bop_24_to_Aop( GenefxState *gfxs )
{
     direct_memmove( gfxs->Aop[0], gfxs->Bop[0], gfxs->length * 3 );
}

static void
Bop_32_to_Aop( GenefxState *gfxs )
{
     direct_memmove( gfxs->Aop[0], gfxs->Bop[0], gfxs->length * 4 );
}

static void
Bop_8_to_Aop( GenefxState *gfxs )
{
     direct_memmove( gfxs->Aop[0], gfxs->Bop[0], gfxs->length );
}

static void
Bop_i420_to_Aop( GenefxState *gfxs )
{
     direct_memmove( gfxs->Aop[0], gfxs->Bop[0], gfxs->length );
     if (gfxs->AopY & 1) {
          direct_memmove( gfxs->Aop[1], gfxs->Bop[1], gfxs->length >> 1 );
          direct_memmove( gfxs->Aop[2], gfxs->Bop[2], gfxs->length >> 1 );
     }
}

static void
Bop_nv12_to_Aop( GenefxState *gfxs )
{
     direct_memmove( gfxs->Aop[0], gfxs->Bop[0], gfxs->length );
     if (gfxs->AopY & 1)
          direct_memmove( gfxs->Aop[1], gfxs->Bop[1], gfxs->length & ~1 );
}

static void
Bop_nv16_to_Aop( GenefxState *gfxs )
{
     direct_memmove( gfxs->Aop[0], gfxs->Bop[0], gfxs->length );
     direct_memmove( gfxs->Aop[1], gfxs->Bop[1], gfxs->length & ~1 );
}

static void
Bop_4_to_Aop( GenefxState *gfxs )
{
     direct_memmove( gfxs->Aop[0], gfxs->Bop[0], gfxs->length >> 1 );
}

static void
Bop_y444_to_Aop( GenefxState *gfxs )
{
     direct_memmove( gfxs->Aop[0], gfxs->Bop[0], gfxs->length );
     direct_memmove( gfxs->Aop[1], gfxs->Bop[1], gfxs->length );
     direct_memmove( gfxs->Aop[2], gfxs->Bop[2], gfxs->length );
}

static void
Bop_y42b_to_Aop( GenefxState *gfxs )
{
     direct_memmove( gfxs->Aop[0], gfxs->Bop[0], gfxs->length );
     direct_memmove( gfxs->Aop[1], gfxs->Bop[1], gfxs->length / 2 );
     direct_memmove( gfxs->Aop[2], gfxs->Bop[2], gfxs->length / 2 );
}

static void Bop_nv24_to_Aop( GenefxState *gfxs )
{
     direct_memmove( gfxs->Aop[0], gfxs->Bop[0], gfxs->length );
     direct_memmove( gfxs->Aop[1], gfxs->Bop[1], gfxs->length * 2 );
}

static GenefxFunc Bop_PFI_to_Aop_PFI[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1555)]   = Bop_16_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB16)]      = Bop_16_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB24)]      = Bop_24_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB32)]      = Bop_32_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]       = Bop_32_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A8)]         = Bop_8_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUY2)]       = Bop_16_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB332)]     = Bop_8_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_UYVY)]       = Bop_16_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_I420)]       = Bop_i420_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV12)]       = Bop_i420_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT8)]       = Bop_8_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ALUT44)]     = Bop_8_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)]      = Bop_32_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV12)]       = Bop_nv12_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV16)]       = Bop_nv16_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB2554)]   = Bop_16_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)]   = Bop_16_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA4444)]   = Bop_16_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV21)]       = Bop_nv12_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_AYUV)]       = Bop_32_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A4)]         = Bop_4_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1666)]   = Bop_24_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB6666)]   = Bop_24_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB18)]      = Bop_24_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB444)]     = Bop_16_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB555)]     = Bop_16_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR555)]     = Bop_16_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA5551)]   = Bop_16_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y444)]       = Bop_y444_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB8565)]   = Bop_24_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_AVYU)]       = Bop_32_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_VYU)]        = Bop_24_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1_LSB)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV16)]       = Bop_y42b_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ABGR)]       = Bop_32_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBAF88871)] = Bop_32_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT1)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV61)]       = Bop_nv16_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y42B)]       = Bop_y42b_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV24)]       = Bop_y444_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV24)]       = Bop_nv24_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV42)]       = Bop_nv24_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR24)]      = Bop_24_to_Aop,
};

/**********************************************************************************************************************
 ********************************* Bop_PFI_toR_Aop_PFI ****************************************************************
 **********************************************************************************************************************/

static void
Bop_16_toR_Aop( GenefxState *gfxs )
{
     int  w     = gfxs->length + 1;
     u16 *S     = gfxs->Bop[0];
     u16 *D     = gfxs->Aop[0];
     int  Dstep = gfxs->Astep;

     while (--w) {
          *D = *S;

          ++S;
          D += Dstep;
     }
}

static void
Bop_24_toR_Aop( GenefxState *gfxs )
{
     int  w     = gfxs->length + 1;
     u8  *S     = gfxs->Bop[0];
     u8  *D     = gfxs->Aop[0];
     int  Dstep = gfxs->Astep;

     while (--w) {
          D[0] = S[0];
          D[1] = S[1];
          D[2] = S[2];

          S += 3;
          D += Dstep * 3;
     }
}

static void
Bop_32_toR_Aop( GenefxState *gfxs )
{
     int  w     = gfxs->length + 1;
     u32 *S     = gfxs->Bop[0];
     u32 *D     = gfxs->Aop[0];
     int  Dstep = gfxs->Astep;

     while (--w) {
          *D = *S;

          ++S;
          D += Dstep;
     }
}

static void
Bop_8_toR_Aop( GenefxState *gfxs )
{
     int  w     = gfxs->length + 1;
     u8  *S     = gfxs->Bop[0];
     u8  *D     = gfxs->Aop[0];
     int  Dstep = gfxs->Astep;

     while (--w) {
          *D = *S;

          ++S;
          D += Dstep;
     }
}

static void
Bop_i420_toR_Aop( GenefxState *gfxs )
{
     Bop_8_toR_Aop( gfxs );

     if (gfxs->AopY & 1) {
          int  w     = (gfxs->length >> 1) + 1;
          u8  *S1    =  gfxs->Bop[1];
          u8  *S2    =  gfxs->Bop[2];
          u8  *D1    =  gfxs->Aop[1];
          u8  *D2    =  gfxs->Aop[2];
          int  Dstep =  gfxs->Astep >> 1;

          while (--w) {
               *D1 = *S1++;
               *D2 = *S2++;
               D1 += Dstep;
               D2 += Dstep;
          }
     }
}

static void
Bop_nv12_toR_Aop( GenefxState *gfxs )
{
     Bop_8_toR_Aop( gfxs );

     if (gfxs->AopY & 1) {
          int  w     = (gfxs->length & ~1) + 1;
          u8  *S     =  gfxs->Bop[1];
          u8  *D     =  gfxs->Aop[1];
          int  Dstep =  gfxs->Astep;

          while (--w) {
               *D = *S++;
               D += Dstep;
          }
     }
}

static void
Bop_nv16_toR_Aop( GenefxState *gfxs )
{
     int  w     = (gfxs->length & ~1) + 1;
     u8  *S     =  gfxs->Bop[1];
     u8  *D     =  gfxs->Aop[1];
     int  Dstep =  gfxs->Astep;

     Bop_8_toR_Aop( gfxs );

     while (--w) {
          *D = *S++;
          D += Dstep;
     }
}

static void
Bop_4_toR_Aop( GenefxState *gfxs )
{
     int  w     = (gfxs->length >> 1) + 1;
     u8  *S     =  gfxs->Bop[0];
     u8  *D     =  gfxs->Aop[0];
     int  Dstep =  gfxs->Astep;

     while (--w) {
          *D = *S;

          ++S;
          D += Dstep;
     }
}

static void
Bop_y444_toR_Aop( GenefxState *gfxs )
{
     int  w     = gfxs->length + 1;
     u8  *Sy    = gfxs->Bop[0];
     u8  *Su    = gfxs->Bop[1];
     u8  *Sv    = gfxs->Bop[2];
     u8  *Dy    = gfxs->Aop[0];
     u8  *Du    = gfxs->Aop[1];
     u8  *Dv    = gfxs->Aop[2];
     int  Dstep = gfxs->Astep;

     while (--w) {
          u8 sy = *Sy++;
          u8 su = *Su++;
          u8 sv = *Sv++;

          *Dy = sy;
          *Du = su;
          *Dv = sv;

          Dy += Dstep;
          Du += Dstep;
          Dv += Dstep;
     }
}

static void
Bop_y42b_toR_Aop( GenefxState *gfxs )
{
     Bop_8_toR_Aop( gfxs );

     int  w     = (gfxs->length / 2) + 1;
     u8  *S1    =  gfxs->Bop[1];
     u8  *S2    =  gfxs->Bop[2];
     u8  *D1    =  gfxs->Aop[1];
     u8  *D2    =  gfxs->Aop[2];
     int  Dstep =  gfxs->Astep / 2;

     while (--w) {
          *D1 = *S1++;
          *D2 = *S2++;
          D1 += Dstep;
          D2 += Dstep;
     }
}

static void
Bop_nv24_toR_Aop( GenefxState *gfxs )
{
     int  w     = gfxs->length + 1;
     u16 *S     = gfxs->Bop[1];
     u16 *D     = gfxs->Aop[1];
     int  Dstep = gfxs->Astep;

     Bop_8_toR_Aop( gfxs );

     while (--w) {
          *D = *S++;
          D += Dstep;
     }
}

static GenefxFunc Bop_PFI_toR_Aop_PFI[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1555)]   = Bop_16_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB16)]      = Bop_16_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB24)]      = Bop_24_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB32)]      = Bop_32_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]       = Bop_32_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A8)]         = Bop_8_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUY2)]       = Bop_16_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB332)]     = Bop_8_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_UYVY)]       = Bop_16_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_I420)]       = Bop_i420_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV12)]       = Bop_i420_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT8)]       = Bop_8_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ALUT44)]     = Bop_8_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)]      = Bop_32_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV12)]       = Bop_nv12_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV16)]       = Bop_nv16_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB2554)]   = Bop_16_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)]   = Bop_16_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA4444)]   = Bop_16_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV21)]       = Bop_nv12_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_AYUV)]       = Bop_32_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A4)]         = Bop_4_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1666)]   = Bop_24_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB6666)]   = Bop_24_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB18)]      = Bop_24_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB444)]     = Bop_16_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB555)]     = Bop_16_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR555)]     = Bop_16_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA5551)]   = Bop_16_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y444)]       = Bop_y444_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB8565)]   = Bop_24_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_AVYU)]       = Bop_32_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_VYU)]        = Bop_24_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1_LSB)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV16)]       = Bop_y42b_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ABGR)]       = Bop_32_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBAF88871)] = Bop_32_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT1)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV61)]       = Bop_nv16_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y42B)]       = Bop_y42b_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV24)]       = Bop_y444_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV24)]       = Bop_nv24_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV42)]       = Bop_nv24_toR_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR24)]      = Bop_24_toR_Aop,
};

/**********************************************************************************************************************
 ********************************* Bop_PFI_toK_Aop_PFI ****************************************************************
 **********************************************************************************************************************/

static void
Bop_yuv422_toK_Aop( GenefxState *gfxs )
{
     int  l;
     int  w     = gfxs->length;
     u16 *S     = gfxs->Bop[0];
     u16 *D     = gfxs->Aop[0];
     u32  Dkey  = gfxs->Dkey;
     int  Ostep = gfxs->Ostep;

     if (Ostep < 0) {
          S += gfxs->length - 1;
          D += gfxs->length - 1;
     }

     if ((long) D & 2) {
#ifdef WORDS_BIGENDIAN
          if (*D == (Dkey & 0xffff))
               *D = *S;
#else
          if (*D == (Dkey >> 16))
               *D = *S;
#endif

          S += Ostep;
          D += Ostep;
          --w;
     }

     if (Ostep < 0) {
          S--;
          D--;
     }

     for (l = w >> 1; l--;) {
          if (*D == Dkey)
               *D = *S;

          S += Ostep << 1;
          D += Ostep << 1;
     }

     if (w & 1) {
#ifdef WORDS_BIGENDIAN
          if (*D == (Dkey >> 16))
               *D = *S;
#else
          if (*D == (Dkey & 0xffff))
               *D = *S;
#endif
     }
}

static void
Bop_rgb332_toK_Aop( GenefxState *gfxs )
{
      int  w    = gfxs->length + 1;
      u8  *S    = gfxs->Bop[0];
      u8  *D    = gfxs->Aop[0];
      u8   Dkey = gfxs->Dkey;

      while (--w) {
           if (*D == Dkey) {
                *D = *S;
           }

           ++S;
           ++D;
      }
}

static void
Bop_y444_toK_Aop( GenefxState *gfxs )
{
     int  w    = gfxs->length + 1;
     u8  *Sy   = gfxs->Bop[0];
     u8  *Su   = gfxs->Bop[1];
     u8  *Sv   = gfxs->Bop[2];
     u8  *Dy   = gfxs->Aop[0];
     u8  *Du   = gfxs->Aop[1];
     u8  *Dv   = gfxs->Aop[2];
     u32  Dkey = gfxs->Dkey;

     while (--w) {
          u8 dy = *Dy;
          u8 du = *Du;
          u8 dv = *Dv;

          if (Dkey == (u32) (dy << 16 | du << 8 | dv)) {
               u8 sy = *Sy;
               u8 su = *Su;
               u8 sv = *Sv;

               *Dy = sy;
               *Du = su;
               *Dv = sv;
          }

          ++Sy; ++Su; ++Sv;
          ++Dy; ++Du; ++Dv;
     }
}

static void
Bop_8_toK_Aop( GenefxState *gfxs )
{
      int  w    = gfxs->length + 1;
      u8  *S    = gfxs->Bop[0];
      u8  *D    = gfxs->Aop[0];
      u8   Dkey = gfxs->Dkey;

      while (--w) {
           if (*D == Dkey) {
                *D = *S;
           }

           ++S;
           ++D;
      }
}

static GenefxFunc Bop_PFI_toK_Aop_PFI[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1555)]   = Bop_15_toK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB16)]      = Bop_16_toK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB24)]      = Bop_24_24_toK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB32)]      = Bop_32_toK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]       = Bop_32_toK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A8)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUY2)]       = Bop_yuv422_toK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB332)]     = Bop_rgb332_toK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_UYVY)]       = Bop_yuv422_toK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_I420)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT8)]       = Bop_8_toK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ALUT44)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)]      = Bop_32_toK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB2554)]   = Bop_14_toK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)]   = Bop_12_toK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA4444)]   = Bop_12vv_toK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV21)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AYUV)]       = Bop_32_toK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A4)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1666)]   = Bop_24_18_toK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB6666)]   = Bop_24_18_toK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB18)]      = Bop_24_18_toK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB444)]     = Bop_12_toK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB555)]     = Bop_15_toK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR555)]     = Bop_15_toK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA5551)]   = Bop_15_toK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y444)]       = Bop_y444_toK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB8565)]   = Bop_24_16_toK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_AVYU)]       = Bop_32_toK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_VYU)]        = Bop_24_24_toK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1_LSB)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ABGR)]       = Bop_32_toK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBAF88871)] = Bop_32_24_toK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT1)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV61)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y42B)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV24)]       = Bop_y444_toK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV24)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV42)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR24)]      = Bop_24_24_toK_Aop,
};

/**********************************************************************************************************************
 ********************************* Bop_PFI_Kto_Aop_PFI ****************************************************************
 **********************************************************************************************************************/

static void
Bop_a8_Kto_Aop( GenefxState *gfxs )
{
     /* No color to key. */
     direct_memmove( gfxs->Aop[0], gfxs->Bop[0], gfxs->length );
}

static void
Bop_yuv422_Kto_Aop( GenefxState *gfxs )
{
     int  l;
     int  w     = gfxs->length;
     u16 *S     = gfxs->Bop[0];
     u16 *D     = gfxs->Aop[0];
     u32  Skey  = gfxs->Skey;
     int  Ostep = gfxs->Ostep;

     if (Ostep < 0) {
          S += gfxs->length - 1;
          D += gfxs->length - 1;
     }

     if ((long) D & 2) {
          u16 s = *S;

#ifdef WORDS_BIGENDIAN
          if (s != (Skey >> 16))
               *D = s;
#else
          if (s != (Skey & 0xffff))
               *D = s;
#endif

          S += Ostep;
          D += Ostep;
          --w;
     }

     if (Ostep < 0) {
         S--;
         D--;
     }

     for (l = w >> 1; l--;) {
          u32 s = *((u32*) S);

          if (s != Skey)
               *((u32*) D) = s;

          S += Ostep << 1;
          D += Ostep << 1;
     }

     if (w & 1) {
          u16 s = *S;

#ifdef WORDS_BIGENDIAN
          if (s != (Skey & 0xffff))
               *D = s;
#else
          if (s != (Skey >> 16))
               *D = s;
#endif
     }
}

/* change the last value to adjust the size of the device (1-4) */
#define SET_PIXEL_DUFFS_DEVICE(D,S,w) \
     SET_PIXEL_DUFFS_DEVICE_N( D, S, w, 4 )

static void
Bop_8_Kto_Aop( GenefxState *gfxs )
{
     int  i;
     int  w    = gfxs->length;
     u8  *S    = gfxs->Bop[0];
     u8  *D    = gfxs->Aop[0];
     u32  Skey = gfxs->Skey;

#define SET_PIXEL(d,s)   \
     do {                \
          if (s != Skey) \
               d = s;    \
     } while (0)

     if (gfxs->Ostep > 0) {
          SET_PIXEL_DUFFS_DEVICE( D, S, w );
     }
     else {
          for (i = w - 1; i >= 0; i--)
               if (S[i] != Skey)
                    D[i] = S[i];
     }

#undef SET_PIXEL
}

#undef SET_PIXEL_DUFFS_DEVICE

static void
Bop_alut44_Kto_Aop( GenefxState *gfxs )
{
     int  w     = gfxs->length +1;
     u8  *S     = gfxs->Bop[0];
     u8  *D     = gfxs->Aop[0];
     u32  Skey  = gfxs->Skey;
     int  Ostep = gfxs->Ostep;

     if (Ostep < 0) {
          S += gfxs->length - 1;
          D += gfxs->length - 1;
     }

     while (--w) {
          u8 s = *S;

          if ((s & 0x0f) != Skey)
               *D = s;

          S += Ostep;
          D += Ostep;
     }
}

static void
Bop_y444_Kto_Aop( GenefxState *gfxs )
{
     int  w     = gfxs->length + 1;
     u8  *Sy    = gfxs->Bop[0];
     u8  *Su    = gfxs->Bop[1];
     u8  *Sv    = gfxs->Bop[2];
     u8  *Dy    = gfxs->Aop[0];
     u8  *Du    = gfxs->Aop[1];
     u8  *Dv    = gfxs->Aop[2];
     u32  Skey  = gfxs->Skey;
     int  Ostep = gfxs->Ostep;

     if (Ostep < 0) {
          int offset = gfxs->length - 1;

          Sy += offset; Su += offset; Sv += offset;
          Dy += offset; Du += offset; Dv += offset;
     }

     while (--w) {
          u8 sy = *Sy;
          u8 su = *Su;
          u8 sv = *Sv;

          if (Skey != (u32) (sy << 16 | su << 8 | sv)) {
               *Dy = sy;
               *Du = su;
               *Dv = sv;
          }

          Sy += Ostep; Su += Ostep; Sv += Ostep;
          Dy += Ostep; Du += Ostep; Dv += Ostep;
     }
}

static GenefxFunc Bop_PFI_Kto_Aop_PFI[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1555)]   = Bop_15_Kto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB16)]      = Bop_16_Kto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB24)]      = Bop_24_24_Kto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB32)]      = Bop_32_Kto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]       = Bop_32_Kto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A8)]         = Bop_a8_Kto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUY2)]       = Bop_yuv422_Kto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB332)]     = Bop_8_Kto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_UYVY)]       = Bop_yuv422_Kto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_I420)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT8)]       = Bop_8_Kto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ALUT44)]     = Bop_alut44_Kto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)]      = Bop_32_Kto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB2554)]   = Bop_14_Kto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)]   = Bop_12_Kto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA4444)]   = Bop_12vv_Kto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV21)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AYUV)]       = Bop_32_Kto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A4)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1666)]   = Bop_24_18_Kto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB6666)]   = Bop_24_18_Kto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB18)]      = Bop_24_18_Kto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB444)]     = Bop_12_Kto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB555)]     = Bop_15_Kto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR555)]     = Bop_15_Kto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA5551)]   = Bop_15_Kto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y444)]       = Bop_y444_Kto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB8565)]   = Bop_24_16_Kto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_AVYU)]       = Bop_32_Kto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_VYU)]        = Bop_24_24_Kto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1_LSB)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ABGR)]       = Bop_32_Kto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBAF88871)] = Bop_32_24_Kto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT1)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV61)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y42B)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV24)]       = Bop_y444_Kto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV24)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV42)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR24)]      = Bop_24_24_Kto_Aop,
};

/**********************************************************************************************************************
 ********************************* Bop_PFI_KtoK_Aop_PFI ***************************************************************
 **********************************************************************************************************************/

static void
Bop_y444_KtoK_Aop( GenefxState *gfxs )
{
     int  w     = gfxs->length + 1;
     u8  *Sy    = gfxs->Bop[0];
     u8  *Su    = gfxs->Bop[1];
     u8  *Sv    = gfxs->Bop[2];
     u8  *Dy    = gfxs->Aop[0];
     u8  *Du    = gfxs->Aop[1];
     u8  *Dv    = gfxs->Aop[2];
     u32  Skey  = gfxs->Skey;
     u32  Dkey  = gfxs->Dkey;
     int  Ostep = gfxs->Ostep;

     if (Ostep < 0) {
          int offset = gfxs->length - 1;

          Sy += offset; Su += offset; Sv += offset;
          Dy += offset; Du += offset; Dv += offset;
     }

     while (--w) {
          u8 sy = *Sy;
          u8 su = *Su;
          u8 sv = *Sv;
          u8 dy = *Dy;
          u8 du = *Du;
          u8 dv = *Dv;

          if (Skey != (u32) (sy << 16 | su << 8 | sv) && Dkey == (u32) (dy << 16 | du << 8 | dv)) {
               *Dy = sy;
               *Du = su;
               *Dv = sv;
          }

          ++Sy; ++Su; ++Sv;
          ++Dy; ++Du; ++Dv;
     }
}

static GenefxFunc Bop_PFI_KtoK_Aop_PFI[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1555)]   = Bop_15_KtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB16)]      = Bop_16_KtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB24)]      = Bop_24_24_KtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB32)]      = Bop_32_KtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]       = Bop_32_KtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A8)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUY2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB332)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_UYVY)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_I420)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT8)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ALUT44)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)]      = Bop_32_KtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB2554)]   = Bop_14_KtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)]   = Bop_12_KtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA4444)]   = Bop_12vv_KtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV21)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AYUV)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A4)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1666)]   = Bop_24_18_KtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB6666)]   = Bop_24_18_KtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB18)]      = Bop_24_18_KtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB444)]     = Bop_12_KtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB555)]     = Bop_15_KtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR555)]     = Bop_15_KtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA5551)]   = Bop_15_KtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y444)]       = Bop_y444_KtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB8565)]   = Bop_24_16_KtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_AVYU)]       = Bop_32_KtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_VYU)]        = Bop_24_24_KtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1_LSB)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ABGR)]       = Bop_32_KtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBAF88871)] = Bop_32_24_KtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT1)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV61)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y42B)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV24)]       = Bop_y444_KtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV24)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV42)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR24)]      = Bop_24_24_KtoK_Aop,
};

/**********************************************************************************************************************
 ********************************* Bop_PFI_Sto_Aop_PFI ****************************************************************
 **********************************************************************************************************************/

static void
Bop_16_Sto_Aop( GenefxState *gfxs )
{
     int  l;
     int  i      = gfxs->Xphase;
     int  w      = gfxs->length;
     u16 *S      = gfxs->Bop[0];
     u32 *D      = gfxs->Aop[0];
     int  SperD  = gfxs->SperD;
     int  SperD2 = SperD << 1;

     if (((long) D) & 2) {
          *(u16*) D = *S;

          D = gfxs->Aop[0] + 2;
          i += SperD;
          --w;
     }

     l = w >> 1;
     while (l--) {
#ifdef WORDS_BIGENDIAN
          *D++ =  S[i>>16] << 16 | S[(i+SperD)>>16];
#else
          *D++ = (S[(i+SperD)>>16] << 16) | S[i>>16];
#endif
          i += SperD2;
     }

     if (w & 1) {
          *(u16*) D = S[i>>16];
     }
}

static void
Bop_24_Sto_Aop( GenefxState *gfxs )
{
     int  i     = gfxs->Xphase;
     int  w     = gfxs->length + 1;
     u8  *S     = gfxs->Bop[0];
     u8  *D     = gfxs->Aop[0];
     int  SperD = gfxs->SperD;

     while (--w) {
          int pixelstart = (i >> 16) * 3;

          *D++ = S[pixelstart+0];
          *D++ = S[pixelstart+1];
          *D++ = S[pixelstart+2];
          i += SperD;
     }
}

static void
Bop_32_Sto_Aop( GenefxState *gfxs )
{
     int  i     = gfxs->Xphase;
     int  w     = gfxs->length + 1;
     u32 *S     = gfxs->Bop[0];
     u32 *D     = gfxs->Aop[0];
     int  SperD = gfxs->SperD;

     while (--w) {
          *D++ = S[i>>16];
          i += SperD;
     }
}

static void
Bop_8_Sto_Aop( GenefxState *gfxs )
{
     int  i     = gfxs->Xphase;
     int  w     = gfxs->length + 1;
     u8  *S     = gfxs->Bop[0];
     u8  *D     = gfxs->Aop[0];
     int  SperD = gfxs->SperD;

     while (--w) {
          *D++ = S[i>>16];
          i += SperD;
     }
}

static void
Bop_yuy2_Sto_Aop( GenefxState *gfxs )
{
     int  l;
     int  i     = gfxs->Xphase;
     int  w     = gfxs->length;
     u16 *S     = gfxs->Bop[0];
     u16 *D     = gfxs->Aop[0];
     int  SperD = gfxs->SperD;

     if ((long) D & 2) {
          *D++ = *S;
          i = SperD;
          --w;
     }

     for (l = w >> 1; l--;) {
          u32 d;

          d  = ((u32*) S)[i>>17] & 0xff00ff00;
#ifdef WORDS_BIGENDIAN
          d |= (S[i>>16]          & 0x00ff) << 16;
          d |=  S[(i+SperD)>>16]  & 0x00ff;
#else
          d |=  S[i>>16]          & 0x00ff;
          d |= (S[(i+SperD)>>16]  & 0x00ff) << 16;
#endif
          *((u32*) D) = d;

          D += 2;
          i += SperD << 1;
     }

     if (w & 1)
          *D = S[i>>16];
}

static void
Bop_uyvy_Sto_Aop( GenefxState *gfxs )
{
     int  l;
     int  i     = gfxs->Xphase;
     int  w     = gfxs->length;
     u16 *S     = gfxs->Bop[0];
     u16 *D     = gfxs->Aop[0];
     int  SperD = gfxs->SperD;

     if ((long) D & 2) {
          *D++ = *S;
          i = SperD;
          --w;
     }

     for (l = w >> 1; l--;) {
          u32 d;

          d  = ((u32*) S)[i>>17] & 0x00ff00ff;
#ifdef WORDS_BIGENDIAN
          d |= (S[i>>16]          & 0xff00) << 16;
          d |= (S[(i+SperD)>>16]  & 0xff00);
#else
          d |= (S[i>>16]          & 0xff00);
          d |= (S[(i+SperD)>>16]  & 0xff00) << 16;
#endif
          *((u32*) D) = d;

          D += 2;
          i += SperD << 1;
     }

     if (w & 1)
          *D = S[i>>16];
}

static void
Bop_i420_Sto_Aop( GenefxState *gfxs )
{
     int  i     = gfxs->Xphase;
     int  w     = gfxs->length + 1;
     u8  *Sy    = gfxs->Bop[0];
     u8  *Dy    = gfxs->Aop[0];
     int  SperD = gfxs->SperD;

     while (--w) {
          *Dy++ = Sy[i>>16];
          i += SperD;
     }

     if (gfxs->AopY & 1) {
          u8 *Su = gfxs->Bop[1];
          u8 *Sv = gfxs->Bop[2];
          u8 *Du = gfxs->Aop[1];
          u8 *Dv = gfxs->Aop[2];

          for (w = gfxs->length >> 1, i = 0; w--;) {
               *Du++ = Su[i>>16];
               i += SperD;
          }

          for (w = gfxs->length >> 1, i = 0; w--;) {
               *Dv++ = Sv[i>>16];
               i += SperD;
          }
     }
}

static void
Bop_nv12_Sto_Aop( GenefxState *gfxs )
{
     int  i     = gfxs->Xphase;
     int  w     = gfxs->length +1;
     u8  *Sy    = gfxs->Bop[0];
     u8  *Dy    = gfxs->Aop[0];
     int  SperD = gfxs->SperD;

     while (--w) {
          *Dy++ = Sy[i>>16];
          i += SperD;
     }

     if (gfxs->AopY & 1) {
          u16 *Suv = gfxs->Bop[1];
          u16 *Duv = gfxs->Aop[1];

          for (w = gfxs->length >> 1, i = 0; w--;) {
               *Duv++ = Suv[i>>16];
               i += SperD;
          }
     }
}

static void
Bop_nv16_Sto_Aop( GenefxState *gfxs )
{
     int  i     = gfxs->Xphase;
     int  w     = gfxs->length +1;
     u8  *Sy    = gfxs->Bop[0];
     u16 *Suv   = gfxs->Bop[1];
     u8  *Dy    = gfxs->Aop[0];
     u16 *Duv   = gfxs->Aop[1];
     int  SperD = gfxs->SperD;

     while (--w) {
          *Dy++ = Sy[i>>16];
          i += SperD;
     }

     for (w = gfxs->length >> 1, i = 0; w--;) {
          *Duv++ = Suv[i>>16];
          i += SperD;
     }
}

static void
Bop_y444_Sto_Aop( GenefxState *gfxs )
{
     int  i     = gfxs->Xphase;
     int  w     = gfxs->length + 1;
     u8  *Sy    = gfxs->Bop[0];
     u8  *Su    = gfxs->Bop[1];
     u8  *Sv    = gfxs->Bop[2];
     u8  *Dy    = gfxs->Aop[0];
     u8  *Du    = gfxs->Aop[1];
     u8  *Dv    = gfxs->Aop[2];
     int  SperD = gfxs->SperD;

     while (--w) {
          u8 sy = Sy[i>>16];
          u8 su = Su[i>>16];
          u8 sv = Sv[i>>16];

          *Dy++ = sy;
          *Du++ = su;
          *Dv++ = sv;
          i += SperD;
     }
}

static void
Bop_y42b_Sto_Aop( GenefxState *gfxs )
{
     int  i     = gfxs->Xphase;
     int  w     = gfxs->length + 1;
     u8  *Sy    = gfxs->Bop[0];
     u8  *Su    = gfxs->Bop[1];
     u8  *Sv    = gfxs->Bop[2];
     u8  *Dy    = gfxs->Aop[0];
     u8  *Du    = gfxs->Aop[1];
     u8  *Dv    = gfxs->Aop[2];
     int  SperD = gfxs->SperD;

     while (--w) {
          *Dy++ = Sy[i>>16];
          i += SperD;
     }

     i = 0;
     w = (gfxs->length / 2) + 1;
     while (--w) {
          *Du++ = Su[i>>16];
          *Dv++ = Sv[i>>16];
          i += SperD;
     }
}

static void
Bop_nv24_Sto_Aop( GenefxState *gfxs )
{
     int  i     = gfxs->Xphase;
     int  w     = gfxs->length + 1;
     u8  *Sy    = gfxs->Bop[0];
     u16 *Suv   = gfxs->Bop[1];
     u8  *Dy    = gfxs->Aop[0];
     u16 *Duv   = gfxs->Aop[1];
     int  SperD = gfxs->SperD;

     while (--w) {
          *Dy++ = Sy[i>>16];
          *Duv++ = Suv[i>>16];

          i += SperD;
     }
}

static GenefxFunc Bop_PFI_Sto_Aop_PFI[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1555)]   = Bop_16_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB16)]      = Bop_16_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB24)]      = Bop_24_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB32)]      = Bop_32_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]       = Bop_32_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A8)]         = Bop_8_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUY2)]       = Bop_yuy2_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB332)]     = Bop_8_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_UYVY)]       = Bop_uyvy_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_I420)]       = Bop_i420_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV12)]       = Bop_i420_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT8)]       = Bop_8_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ALUT44)]     = Bop_8_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)]      = Bop_32_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV12)]       = Bop_nv12_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV16)]       = Bop_nv16_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB2554)]   = Bop_16_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)]   = Bop_16_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA4444)]   = Bop_16_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV21)]       = Bop_nv12_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_AYUV)]       = Bop_32_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A4)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1666)]   = Bop_24_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB6666)]   = Bop_24_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB18)]      = Bop_24_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB444)]     = Bop_16_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB555)]     = Bop_16_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR555)]     = Bop_16_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA5551)]   = Bop_16_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y444)]       = Bop_y444_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB8565)]   = Bop_24_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_AVYU)]       = Bop_32_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_VYU)]        = Bop_24_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1_LSB)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV16)]       = Bop_y42b_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ABGR)]       = Bop_32_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBAF88871)] = Bop_32_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT1)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV61)]       = Bop_nv16_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y42B)]       = Bop_y42b_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV24)]       = Bop_y444_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV24)]       = Bop_nv24_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV42)]       = Bop_nv24_Sto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR24)]      = Bop_24_Sto_Aop,
};

/**********************************************************************************************************************
 ********************************* Bop_PFI_SKto_Aop_PFI ***************************************************************
 **********************************************************************************************************************/

static void
Bop_a8_SKto_Aop( GenefxState *gfxs )
{
     int  i     = gfxs->Xphase;
     int  w     = gfxs->length + 1;
     u8  *S     = gfxs->Bop[0];
     u8  *D     = gfxs->Aop[0];
     int  SperD = gfxs->SperD;

     /* No color to key. */
     while (--w) {
          *D++ = S[i>>16];
          i += SperD;
     }
}

static void
Bop_yuy2_SKto_Aop( GenefxState *gfxs )
{
     int  l;
     int  i     = gfxs->Xphase;
     int  w     = gfxs->length;
     u16 *S     = gfxs->Bop[0];
     u16 *D     = gfxs->Aop[0];
     u32  Skey  = gfxs->Skey;
#ifdef WORDS_BIGENDIAN
     u16  Skey0 = gfxs->Skey >> 16;
     u16  Skey1 = gfxs->Skey & 0xffff;
#else
     u16  Skey0 = gfxs->Skey & 0xffff;
     u16  Skey1 = gfxs->Skey >> 16;
#endif
     int  SperD = gfxs->SperD;

     if ((long) D & 2) {
          u16 s = *S;

          if (s != Skey0)
               *D = s;

          ++D;
          i = SperD;
          --w;
     }

     for (l = w >> 1; l--;) {
          u32 s;

          s  = ((u32*) S)[i>>17] & 0xff00ff00;
#ifdef WORDS_BIGENDIAN
          s |= (S[i>>16]          & 0x00ff) << 16;
          s |=  S[(i+SperD)>>16]  & 0x00ff;
#else
          s |=  S[i>>16]          & 0x00ff;
          s |= (S[(i+SperD)>>16]  & 0x00ff) << 16;
#endif
          if (s != Skey)
               *((u32*) D) = s;

          D += 2;
          i += SperD << 1;
     }

     if (w & 1) {
          u16 s = S[i>>16];
          if (i & 0x20000) {
               if (s != Skey1)
                    *D = s;
          }
          else {
               if (s != Skey0)
                    *D = s;
          }
     }
}

static void
Bop_8_SKto_Aop( GenefxState *gfxs )
{
     int  i     = gfxs->Xphase;
     int  w     = gfxs->length + 1;
     u8  *S     = gfxs->Bop[0];
     u8  *D     = gfxs->Aop[0];
     u32  Skey  = gfxs->Skey;
     int  SperD = gfxs->SperD;

     while (--w) {
          u8 s = S[i>>16];

          if (s != Skey)
               *D = s;

          ++D;
          i += SperD;
     }
}

static void
Bop_uyvy_SKto_Aop( GenefxState *gfxs )
{
     int  l;
     int  i     = gfxs->Xphase;
     int  w     = gfxs->length;
     u16 *S     = gfxs->Bop[0];
     u16 *D     = gfxs->Aop[0];
     u32  Skey  = gfxs->Skey;
#ifdef WORDS_BIGENDIAN
     u16  Skey0 = gfxs->Skey >> 16;
     u16  Skey1 = gfxs->Skey & 0xffff;
#else
     u16  Skey0 = gfxs->Skey & 0xffff;
     u16  Skey1 = gfxs->Skey >> 16;
#endif
     int  SperD = gfxs->SperD;

     if ((long) D & 2) {
          u16 s = *S;
          if (s != Skey0)
               *D = s;
          ++D;
          i = SperD;
          --w;
     }

     for (l = w >> 1; l--;) {
          u32 s;

          s  = ((u32*) S)[i>>17] & 0x00ff00ff;
#ifdef WORDS_BIGENDIAN
          s |= (S[i>>16]          & 0xff00) << 16;
          s |= (S[(i+SperD)>>16]  & 0xff00);
#else
          s |= (S[i>>16]          & 0xff00);
          s |= (S[(i+SperD)>>16]  & 0xff00) << 16;
#endif
          if (s != Skey)
               *((u32*) D) = s;

          D += 2;
          i += SperD << 1;
     }

     if (w & 1) {
          u16 s = S[i>>16];
          if (i & 0x20000) {
               if (s != Skey1)
                    *D = s;
          }
          else {
               if (s != Skey0)
                    *D = s;
          }
     }
}

static void
Bop_alut44_SKto_Aop( GenefxState *gfxs )
{
     int  i     = gfxs->Xphase;
     int  w     = gfxs->length + 1;
     u8  *S     = gfxs->Bop[0];
     u8  *D     = gfxs->Aop[0];
     u32  Skey  = gfxs->Skey;
     int  SperD = gfxs->SperD;

     while (--w) {
          u8 s = S[i>>16];

          if ((s & 0x0f) != Skey)
               *D = s;

          ++D;
          i += SperD;
     }
}

static void
Bop_y444_SKto_Aop( GenefxState *gfxs )
{
     int  i     = gfxs->Xphase;
     int  w     = gfxs->length + 1;
     u8  *Sy    = gfxs->Bop[0];
     u8  *Su    = gfxs->Bop[1];
     u8  *Sv    = gfxs->Bop[2];
     u8  *Dy    = gfxs->Aop[0];
     u8  *Du    = gfxs->Aop[1];
     u8  *Dv    = gfxs->Aop[2];
     u32  Skey  = gfxs->Skey;
     int  SperD = gfxs->SperD;

     while (--w) {
          u8 sy = Sy[i>>16];
          u8 su = Su[i>>16];
          u8 sv = Sv[i>>16];

          if (Skey != (u32) (sy << 16 | su << 8 | sv)) {
               *Dy = sy;
               *Du = su;
               *Dv = sv;
          }

          ++Dy; ++Du; ++Dv;
          i += SperD;
     }
}

static GenefxFunc Bop_PFI_SKto_Aop_PFI[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1555)]   = Bop_15_SKto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB16)]      = Bop_16_SKto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB24)]      = Bop_24_24_SKto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB32)]      = Bop_32_SKto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]       = Bop_32_SKto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A8)]         = Bop_a8_SKto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUY2)]       = Bop_yuy2_SKto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB332)]     = Bop_8_SKto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_UYVY)]       = Bop_uyvy_SKto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_I420)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT8)]       = Bop_8_SKto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ALUT44)]     = Bop_alut44_SKto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)]      = Bop_32_SKto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB2554)]   = Bop_14_SKto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)]   = Bop_12_SKto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA4444)]   = Bop_12vv_SKto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV21)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AYUV)]       = Bop_32_SKto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A4)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1666)]   = Bop_24_18_SKto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB6666)]   = Bop_24_18_SKto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB18)]      = Bop_24_18_SKto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB444)]     = Bop_12_SKto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB555)]     = Bop_15_SKto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR555)]     = Bop_15_SKto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA5551)]   = Bop_15_SKto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y444)]       = Bop_y444_SKto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB8565)]   = Bop_24_16_SKto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_AVYU)]       = Bop_32_SKto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_VYU)]        = Bop_24_24_SKto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1_LSB)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ABGR)]       = Bop_32_SKto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBAF88871)] = Bop_32_24_SKto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT1)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV61)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y42B)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV24)]       = Bop_y444_SKto_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV24)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV42)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR24)]      = Bop_24_24_SKto_Aop,
};

/**********************************************************************************************************************
 ********************************* Bop_PFI_StoK_Aop_PFI ***************************************************************
 **********************************************************************************************************************/

static void
Bop_y444_StoK_Aop( GenefxState *gfxs )
{
     int  i     = gfxs->Xphase;
     int  w     = gfxs->length + 1;
     u8  *Sy    = gfxs->Bop[0];
     u8  *Su    = gfxs->Bop[1];
     u8  *Sv    = gfxs->Bop[2];
     u8  *Dy    = gfxs->Aop[0];
     u8  *Du    = gfxs->Aop[1];
     u8  *Dv    = gfxs->Aop[2];
     u32  Dkey  = gfxs->Dkey;
     int  SperD = gfxs->SperD;

     while (--w) {
          u8 dy = *Dy;
          u8 du = *Du;
          u8 dv = *Dv;

          if (Dkey == (u32) (dy << 16 | du << 8 | dv)) {
               u8 sy = Sy[i>>16];
               u8 su = Su[i>>16];
               u8 sv = Sv[i>>16];

               *Dy = sy;
               *Du = su;
               *Dv = sv;
          }

          ++Dy; ++Du; ++Dv;
          i += SperD;
     }
}

static GenefxFunc Bop_PFI_StoK_Aop_PFI[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1555)]   = Bop_15_StoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB16)]      = Bop_16_StoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB24)]      = Bop_24_24_StoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB32)]      = Bop_32_StoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]       = Bop_32_StoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A8)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUY2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB332)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_UYVY)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_I420)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT8)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ALUT44)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)]      = Bop_32_StoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB2554)]   = Bop_14_StoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)]   = Bop_12_StoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA4444)]   = Bop_12vv_StoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV21)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AYUV)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A4)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1666)]   = Bop_24_18_StoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB6666)]   = Bop_24_18_StoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB18)]      = Bop_24_18_StoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB444)]     = Bop_12_StoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB555)]     = Bop_15_StoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR555)]     = Bop_15_StoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA5551)]   = Bop_15_StoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y444)]       = Bop_y444_StoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB8565)]   = Bop_24_16_StoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_AVYU)]       = Bop_32_StoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_VYU)]        = Bop_24_24_StoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1_LSB)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ABGR)]       = Bop_32_StoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBAF88871)] = Bop_32_24_StoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT1)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV61)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y42B)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV24)]       = Bop_y444_StoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV24)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV42)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR24)]      = Bop_24_24_StoK_Aop,
};

/**********************************************************************************************************************
 ********************************* Bop_PFI_SKtoK_Aop_PFI **************************************************************
 **********************************************************************************************************************/

static void
Bop_y444_SKtoK_Aop( GenefxState *gfxs )
{
     int  i     = gfxs->Xphase;
     int  w     = gfxs->length + 1;
     u8  *Sy    = gfxs->Bop[0];
     u8  *Su    = gfxs->Bop[1];
     u8  *Sv    = gfxs->Bop[2];
     u8  *Dy    = gfxs->Aop[0];
     u8  *Du    = gfxs->Aop[1];
     u8  *Dv    = gfxs->Aop[2];
     u32  Skey  = gfxs->Skey;
     u32  Dkey  = gfxs->Dkey;
     int  SperD = gfxs->SperD;

     while (--w) {
          u8 sy = Sy[i>>16];
          u8 su = Su[i>>16];
          u8 sv = Sv[i>>16];
          u8 dy = *Dy;
          u8 du = *Du;
          u8 dv = *Dv;


          if (Skey != (u32) (sy << 16 | su << 8 | sv) && Dkey == (u32) (dy << 16 | du << 8 | dv)) {
               *Dy = sy;
               *Du = su;
               *Dv = sv;
          }

          ++Dy; ++Du; ++Dv;
          i += SperD;
     }
}

static GenefxFunc Bop_PFI_SKtoK_Aop_PFI[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1555)]   = Bop_15_SKtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB16)]      = Bop_16_SKtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB24)]      = Bop_24_24_SKtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB32)]      = Bop_32_SKtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]       = Bop_32_SKtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A8)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUY2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB332)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_UYVY)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_I420)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT8)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ALUT44)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)]      = Bop_32_SKtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB2554)]   = Bop_14_SKtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)]   = Bop_12_SKtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA4444)]   = Bop_12vv_SKtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV21)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AYUV)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A4)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1666)]   = Bop_24_18_SKtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB6666)]   = Bop_24_18_SKtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB18)]      = Bop_24_18_SKtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB444)]     = Bop_12_SKtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB555)]     = Bop_15_SKtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR555)]     = Bop_15_SKtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA5551)]   = Bop_15_SKtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y444)]       = Bop_y444_SKtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB8565)]   = Bop_24_16_SKtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_AVYU)]       = Bop_32_SKtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_VYU)]        = Bop_24_24_SKtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1_LSB)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ABGR)]       = Bop_32_SKtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBAF88871)] = Bop_32_24_SKtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT1)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV61)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y42B)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV24)]       = Bop_y444_SKtoK_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV24)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV42)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR24)]      = Bop_24_24_SKtoK_Aop,
};

/**********************************************************************************************************************
 ********************************* Bop_PFI_TEX_to_Aop_PFI *************************************************************
 **********************************************************************************************************************/

static void
Bop_24_TEX_to_Aop( GenefxState *gfxs )
{
     int  s     = gfxs->s;
     int  t     = gfxs->t;
     int  w     = gfxs->length + 1;
     u8  *S     = gfxs->Bop[0];
     u8  *D     = gfxs->Aop[0];
     int  sp3   = gfxs->src_pitch / 3;
     int  SperD = gfxs->SperD;
     int  TperD = gfxs->TperD;

     while (--w) {
          int pixelstart = ((s >> 16) + (t >> 16) * sp3) * 3;
          *D++ = S[pixelstart++];
          *D++ = S[pixelstart++];
          *D++ = S[pixelstart];

          s += SperD;
          t += TperD;
     }
}

static void
Bop_32_TEX_to_Aop( GenefxState *gfxs )
{
     int  s     = gfxs->s;
     int  t     = gfxs->t;
     int  w     = gfxs->length + 1;
     u32 *S     = gfxs->Bop[0];
     u32 *D     = gfxs->Aop[0];
     int  sp4   = gfxs->src_pitch / 4;
     int  SperD = gfxs->SperD;
     int  TperD = gfxs->TperD;

     while (--w) {
          *D++ = S[(s>>16)+(t>>16)*sp4];

          s += SperD;
          t += TperD;
     }
}

static GenefxFunc Bop_PFI_TEX_to_Aop_PFI[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1555)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB16)]      = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB24)]      = Bop_24_TEX_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB32)]      = Bop_32_TEX_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]       = Bop_32_TEX_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A8)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUY2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB332)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_UYVY)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_I420)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT8)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ALUT44)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)]      = Bop_32_TEX_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB2554)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA4444)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV21)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AYUV)]       = Bop_32_TEX_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A4)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1666)]   = Bop_24_TEX_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB6666)]   = Bop_24_TEX_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB18)]      = Bop_24_TEX_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB444)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB555)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR555)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA5551)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y444)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB8565)]   = Bop_24_TEX_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_AVYU)]       = Bop_32_TEX_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_VYU)]        = Bop_24_TEX_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1_LSB)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ABGR)]       = Bop_32_TEX_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBAF88871)] = Bop_32_TEX_to_Aop,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT1)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV61)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y42B)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV24)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV24)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV42)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR24)]      = Bop_24_TEX_to_Aop,
};

/**********************************************************************************************************************
 ********************************* Bop_argb_blend_alphachannel_src_invsrc_Aop_PFI *************************************
 **********************************************************************************************************************/

/* change the last value to adjust the size of the device (1-4) */
#define SET_PIXEL_DUFFS_DEVICE(D,S,w) \
     SET_PIXEL_DUFFS_DEVICE_N( D, S, w, 3 )

static void
Bop_argb_blend_alphachannel_src_invsrc_Aop_rgb16( GenefxState *gfxs )
{
     int  w = gfxs->length;
     u32 *S = gfxs->Bop[0];
     u16 *D = gfxs->Aop[0];

#define SET_PIXEL(d,s)                                                                                                \
     switch (s >> 26) {                                                                                               \
          case 0:                                                                                                     \
               break;                                                                                                 \
          case 0x3f:                                                                                                  \
               d = ARGB_TO_RGB16( s );                                                                               \
               break;                                                                                                 \
          default:                                                                                                    \
               d = (((((((s >> 8) & 0xf800) | ((s >> 3) & 0x001f)) - (d & 0xf81f)) * ((s >> 26) + 1) +                \
                      ((d & 0xf81f) << 6)                                                           ) & 0x003e07c0) + \
                    (((( (s >> 5) & 0x07e0)                        - (d & 0x07e0)) * ((s >> 26) + 1) +                \
                      ((d & 0x07e0) << 6)                                                           ) & 0x0001f800)   \
                   ) >> 6;                                                                                            \
     }

     SET_PIXEL_DUFFS_DEVICE( D, S, w );

#undef SET_PIXEL
}

#undef SET_PIXEL_DUFFS_DEVICE

static void
Bop_argb_blend_alphachannel_src_invsrc_Aop_rgb32( GenefxState *gfxs )
{
     int  w     = gfxs->length + 1;
     u32 *S     = gfxs->Bop[0];
     u32 *D     = gfxs->Aop[0];
     int  Dstep = gfxs->Astep;

     while (--w) {
          u32 dp32   = *D;
          u32 sp32   = *S++;
          int salpha = (sp32 >> 25) + 1;

#define rb (sp32 & 0xff00ff)
#define g  (sp32 & 0x00ff00)

          *D = ((((rb - (dp32 & 0xff00ff)) * salpha + ((dp32 & 0xff00ff) << 7)) & 0x7f807f80) +
                ((( g - (dp32 & 0x00ff00)) * salpha + ((dp32 & 0x00ff00) << 7)) & 0x007f8000)) >> 7;
          D += Dstep;

#undef rb
#undef g
     }
}

static void
Bop_argb_blend_alphachannel_src_invsrc_Aop_argb8565( GenefxState *gfxs )
{
     int  w = gfxs->length + 1;
     u32 *S = gfxs->Bop[0];
     u8  *D = gfxs->Aop[0];

     while (--w) {
#ifdef WORDS_BIGENDIAN
          u16 dp16 = D[1] << 8 | D[2];
#else
          u16 dp16 = D[1] << 8 | D[0];
#endif
          u32 sp32   = *S++;
          int salpha = (sp32 >> 26) + 1;

#define srb (((sp32 >> 8) & 0xf800) | ((sp32 >> 3) & 0x001f))
#define sg   ((sp32 >> 5) & 0x07e0)
#define drb (dp16 & 0xf81f)
#define dg  (dp16 & 0x07e0)

          dp16 = ((((srb - drb) * salpha + (drb << 6)) & 0x003e07c0) +
                  ((( sg -  dg) * salpha + ( dg << 6)) & 0x0001f800)) >> 6;

#ifdef WORDS_BIGENDIAN
          D[0] = 0;
          D[1] = (dp16 >> 8) & 0xff;
          D[2] =  dp16       & 0xff;
#else
          D[0] =  dp16       & 0xff;
          D[1] = (dp16 >> 8) & 0xff;
          D[2] = 0;
#endif

          D += 3;
#undef dg
#undef drb
#undef sg
#undef srb
     }
}

static GenefxFunc Bop_argb_blend_alphachannel_src_invsrc_Aop_PFI[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1555)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB16)]      = Bop_argb_blend_alphachannel_src_invsrc_Aop_rgb16,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB24)]      = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB32)]      = Bop_argb_blend_alphachannel_src_invsrc_Aop_rgb32,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A8)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUY2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB332)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_UYVY)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_I420)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT8)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ALUT44)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)]      = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB2554)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA4444)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV21)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AYUV)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A4)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1666)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB6666)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB18)]      = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB444)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB555)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR555)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA5551)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y444)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB8565)]   = Bop_argb_blend_alphachannel_src_invsrc_Aop_argb8565,
     [DFB_PIXELFORMAT_INDEX(DSPF_AVYU)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_VYU)]        = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1_LSB)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ABGR)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBAF88871)] = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT1)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV61)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y42B)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV24)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV24)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV42)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR24)]      = NULL,
};

/**********************************************************************************************************************
 ********************************* Bop_argb_blend_alphachannel_one_invsrc_Aop_argb ************************************
 **********************************************************************************************************************/

/* change the last value to adjust the size of the device (1-4) */
#define SET_PIXEL_DUFFS_DEVICE(D,S,w) \
     SET_PIXEL_DUFFS_DEVICE_N( D, S, w, 3 )

static void
Bop_argb_blend_alphachannel_one_invsrc_Aop_argb( GenefxState *gfxs )
{
     int  w = gfxs->length;
     u32 *S = gfxs->Bop[0];
     u32 *D = gfxs->Aop[0];

#define SET_PIXEL(d,s)                                          \
     switch (s >> 24) {                                         \
          case 0:                                               \
               d += s;                                          \
               break;                                           \
          case 0xff:                                            \
               d = s;                                           \
               break;                                           \
          default: {                                            \
               int invsrc = 256 - (s >> 24);                    \
               u32 Drb = ((d & 0x00ff00ff) * invsrc) >> 8;      \
               u32 Dag = ((d & 0xff00ff00) >> 8) * invsrc;      \
               d = s + (Drb & 0x00ff00ff) + (Dag & 0xff00ff00); \
          }                                                     \
     }

     SET_PIXEL_DUFFS_DEVICE( D, S, w );

#undef SET_PIXEL
}

#undef SET_PIXEL_DUFFS_DEVICE

static GenefxFunc Bop_argb_blend_alphachannel_one_invsrc_Aop_PFI[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1555)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB16)]      = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB24)]      = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB32)]      = Bop_argb_blend_alphachannel_one_invsrc_Aop_argb,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]       = Bop_argb_blend_alphachannel_one_invsrc_Aop_argb,
     [DFB_PIXELFORMAT_INDEX(DSPF_A8)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUY2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB332)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_UYVY)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_I420)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT8)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ALUT44)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)]      = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB2554)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA4444)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV21)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AYUV)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A4)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1666)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB6666)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB18)]      = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB444)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB555)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR555)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA5551)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y444)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB8565)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AVYU)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_VYU)]        = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1_LSB)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ABGR)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBAF88871)] = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT1)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV61)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y42B)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV24)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV24)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV42)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR24)]      = NULL,
};

/**********************************************************************************************************************
 ********************************* Bop_argb_blend_alphachannel_one_invsrc_premultiply_Aop_PFI *************************
 **********************************************************************************************************************/

/* change the last value to adjust the size of the device (1-4) */
#define SET_PIXEL_DUFFS_DEVICE(D,S,w) \
     SET_PIXEL_DUFFS_DEVICE_N( D, S, w, 3 )

static void
Bop_argb_blend_alphachannel_one_invsrc_premultiply_Aop_argb( GenefxState *gfxs )
{
     int  w = gfxs->length;
     u32 *S = gfxs->Bop[0];
     u32 *D = gfxs->Aop[0];

#define SET_PIXEL(d,s)                                       \
     switch (s >> 24) {                                      \
          case 0:                                            \
               break;                                        \
          case 0xff:                                         \
               d = s;                                        \
               break;                                        \
          default: {                                         \
               int src    = (s >> 24) + 1;                   \
               int invsrc = 256 - (s >> 24);                 \
               u32 Drb = ((d & 0x00ff00ff) * invsrc) >> 8;   \
               u32 Dag = ((d & 0xff00ff00) >> 8) * invsrc;   \
               u32 Srb = ((s & 0x00ff00ff) * src) >> 8;      \
               u32 Sxg = ((s & 0xff00ff00) >> 8) * src;      \
               d = (Srb & 0x00ff00ff) + (Sxg & 0x0000ff00) + \
                   (Drb & 0x00ff00ff) + (Dag & 0xff00ff00) + \
                   (  s & 0xff000000);                       \
          }                                                  \
     }

     SET_PIXEL_DUFFS_DEVICE( D, S, w );

#undef SET_PIXEL
}

#undef SET_PIXEL_DUFFS_DEVICE

static GenefxFunc Bop_argb_blend_alphachannel_one_invsrc_premultiply_Aop_PFI[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1555)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB16)]      = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB24)]      = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB32)]      = Bop_argb_blend_alphachannel_one_invsrc_premultiply_Aop_argb,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]       = Bop_argb_blend_alphachannel_one_invsrc_premultiply_Aop_argb,
     [DFB_PIXELFORMAT_INDEX(DSPF_A8)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUY2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB332)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_UYVY)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_I420)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT8)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ALUT44)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)]      = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB2554)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA4444)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV21)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AYUV)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A4)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1666)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB6666)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB18)]      = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB444)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB555)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR555)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA5551)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y444)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB8565)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AVYU)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_VYU)]        = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1_LSB)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ABGR)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBAF88871)] = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT1)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV61)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y42B)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV24)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV24)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV42)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR24)]      = NULL,
};

/**********************************************************************************************************************
 ********************************* Bop_a8_set_alphapixel_Aop_PFI ******************************************************
 **********************************************************************************************************************/

/* change the last value to adjust the size of the device (1-4) */
#define SET_PIXEL_DUFFS_DEVICE(D,S,w) \
     SET_PIXEL_DUFFS_DEVICE_N( D, S, w, 3 )

static void
Bop_a8_set_alphapixel_Aop_argb1555( GenefxState *gfxs )
{
     int  w   = gfxs->length;
     u8  *S   = gfxs->Bop[0];
     u16 *D   = gfxs->Aop[0];
     u32  Cop = gfxs->Cop;
     u32  rb  = Cop & 0x7c1f;
     u32  g   = Cop & 0x03e0;

#define SET_PIXEL(d,s)                                                                                            \
     switch (s) {                                                                                                 \
          case 0xff: d = Cop;                                                                                     \
          case 0: break;                                                                                          \
          default: {                                                                                              \
               u32 a  = (s >> 3) + 1;                                                                             \
               u32 t1 = d & 0x7c1f;                                                                               \
               u32 t2 = d & 0x03e0;                                                                               \
               d = (d & 0x8000)      |                                                                            \
                   ((s & 0x80) << 8) |                                                                            \
                   ((((rb - t1) * a + (t1 << 5)) & 0x000f83e0) + (((g - t2) * a + (t2 << 5)) & 0x00007c00)) >> 5; \
          }                                                                                                       \
     }

     SET_PIXEL_DUFFS_DEVICE( D, S, w );

#undef SET_PIXEL
}

static void
Bop_a8_set_alphapixel_Aop_rgb16( GenefxState *gfxs )
{
     int  w   = gfxs->length;
     u8  *S   = gfxs->Bop[0];
     u16 *D   = gfxs->Aop[0];
     u32  Cop = gfxs->Cop;
     u32  rb  = Cop & 0xf81f;
     u32  g   = Cop & 0x07e0;

#define SET_PIXEL(d,s)                                                                                            \
     switch (s) {                                                                                                 \
          case 0xff: d = Cop;                                                                                     \
          case 0: break;                                                                                          \
          default: {                                                                                              \
               u32 a  = (s >> 2) + 1;                                                                             \
               u32 t1 = d & 0xf81f;                                                                               \
               u32 t2 = d & 0x07e0;                                                                               \
               d = ((((rb - t1) * a + (t1 << 6)) & 0x003e07c0) + (((g - t2) * a + (t2 << 6)) & 0x0001f800)) >> 6; \
          }                                                                                                       \
     }

     SET_PIXEL_DUFFS_DEVICE( D, S, w );

#undef SET_PIXEL
}

static void
Bop_a8_set_alphapixel_Aop_rgb24( GenefxState *gfxs )
{
     int       w     = gfxs->length;
     u8       *S     = gfxs->Bop[0];
     u8       *D     = gfxs->Aop[0];
     DFBColor  color = gfxs->color;

#define SET_PIXEL(D,r,g,b,s)                               \
     switch (s) {                                          \
         case 0xff:                                        \
               D[0] = b;                                   \
               D[1] = g;                                   \
               D[2] = r;                                   \
          case 0: break;                                   \
          default: {                                       \
               u16 a = s + 1;                              \
               D[0] = ((b - D[0]) * a + (D[0] << 8)) >> 8; \
               D[1] = ((g - D[1]) * a + (D[1] << 8)) >> 8; \
               D[2] = ((r - D[2]) * a + (D[2] << 8)) >> 8; \
          }                                                \
     }

     while (w > 4) {
          SET_PIXEL( D, color.r, color.g, color.b, *S ); D += 3; ++S;
          SET_PIXEL( D, color.r, color.g, color.b, *S ); D += 3; ++S;
          SET_PIXEL( D, color.r, color.g, color.b, *S ); D += 3; ++S;
          SET_PIXEL( D, color.r, color.g, color.b, *S ); D += 3; ++S;
          w -= 4;
     }

     while (w--) {
          SET_PIXEL( D, color.r, color.g, color.b, *S ); D += 3; ++S;
     }

#undef SET_PIXEL
}

static void
Bop_a8_set_alphapixel_Aop_rgb32( GenefxState *gfxs )
{
     int  w   = gfxs->length;
     u8  *S   = gfxs->Bop[0];
     u32 *D   = gfxs->Aop[0];
     u32  Cop = gfxs->Cop;
     u32  rb  = Cop & 0xff00ff;
     u32  g   = Cop & 0x00ff00;

#define SET_PIXEL(d,s)                                                                                            \
     switch (s) {                                                                                                 \
          case 0xff: d = Cop;                                                                                     \
          case 0: break;                                                                                          \
          default: {                                                                                              \
               u32 a  = s + 1;                                                                                    \
               u32 t1 = d & 0x00ff00ff;                                                                           \
               u32 t2 = d & 0x0000ff00;                                                                           \
               d = ((((rb - t1) * a + (t1 << 8)) & 0xff00ff00) + (((g - t2) * a + (t2 << 8)) & 0x00ff0000)) >> 8; \
          }                                                                                                       \
     }

     SET_PIXEL_DUFFS_DEVICE( D, S, w );

#undef SET_PIXEL
}

static void
Bop_a8_set_alphapixel_Aop_argb( GenefxState *gfxs )
{
     int  w   = gfxs->length;
     u8  *S   = gfxs->Bop[0];
     u32 *D   = gfxs->Aop[0];
     u32  Cop = gfxs->Cop | 0xff000000;
     u32  rb  = Cop & 0x00ff00ff;
     u32  g   = gfxs->color.g;

#define SET_PIXEL(d,s)                                                                 \
     switch (s) {                                                                      \
          case 0xff: d = Cop;                                                          \
          case 0: break;                                                               \
          default: {                                                                   \
               u32 a  = s + 1;                                                         \
               u32 a1 = 256 - s;                                                       \
               u32 sa = (((d >> 24) * a1) >> 8) + s;                                   \
               d = (sa << 24)                                                        + \
                    (((((d & 0x00ff00ff)       * a1) + (rb * a)) >> 8) & 0x00ff00ff) + \
                    (((((d & 0x0000ff00) >> 8) * a1) + ( g * a))       & 0x0000ff00);  \
          }                                                                            \
     }

     SET_PIXEL_DUFFS_DEVICE( D, S, w );

#undef SET_PIXEL
}

static void
Bop_a8_set_alphapixel_Aop_a8( GenefxState *gfxs )
{
     int  w = gfxs->length;
     u8  *S = gfxs->Bop[0];
     u8  *D = gfxs->Aop[0];

#define SET_PIXEL(d,s)                  \
     switch (s) {                       \
          case 0xff: d = 0xff;          \
          case 0: break;                \
          default: {                    \
               u16 a1 = 255 - s;        \
               d = ((d * a1) >> 8) + s; \
          }                             \
     }

     SET_PIXEL_DUFFS_DEVICE( D, S, w );

#undef SET_PIXEL
}

static void
Bop_a8_set_alphapixel_Aop_yuy2( GenefxState *gfxs )
{
     int  w = gfxs->length;
     u8  *S = gfxs->Bop[0];
     u16 *D = gfxs->Aop[0];
     u32  y = gfxs->YCop;
     u32  u = gfxs->CbCop;
     u32  v = gfxs->CrCop;

#ifdef WORDS_BIGENDIAN
     u16 Cop0 = u | (y << 8);
     u16 Cop1 = v | (y << 8);
#define SET_PIXEL(d,s)                                                                             \
     switch (s) {                                                                                  \
          case 0xff:                                                                               \
               d = ((long) &(d) & 2) ? Cop1 : Cop0;                                                \
          case 0x00: break;                                                                        \
          default: {                                                                               \
               u32 a  = s + 1;                                                                     \
               u32 t1 = d & 0xff;                                                                  \
               u32 t2 = d >> 8;                                                                    \
               if ((long) &(d) & 2)                                                                \
                    d = (((v - t1) * a + (t1 << 8)) >> 8) | (((y - t2) * a + (t2 << 8)) & 0xff00); \
               else                                                                                \
                    d = (((u - t1) * a + (t1 << 8)) >> 8) | (((y - t2) * a + (t2 << 8)) & 0xff00); \
          }                                                                                        \
     }
#else
     u16 Cop0 = y | (u << 8);
     u16 Cop1 = y | (v << 8);
#define SET_PIXEL(d,s)                                                                             \
     switch (s) {                                                                                  \
          case 0xff:                                                                               \
               d = ((long) &(d) & 2) ? Cop1 : Cop0;                                                \
          case 0x00: break;                                                                        \
          default: {                                                                               \
               u32 a  = s + 1;                                                                     \
               u32 t1 = d & 0xff;                                                                  \
               u32 t2 = d >> 8;                                                                    \
               if ((long) &(d) & 2)                                                                \
                    d = (((y - t1) * a + (t1 << 8)) >> 8) | (((v - t2) * a + (t2 << 8)) & 0xff00); \
               else                                                                                \
                    d = (((y - t1) * a + (t1 << 8)) >> 8) | (((u - t2) * a + (t2 << 8)) & 0xff00); \
          }                                                                                        \
     }
#endif

     SET_PIXEL_DUFFS_DEVICE( D, S, w );

#undef SET_PIXEL
}

static void
Bop_a8_set_alphapixel_Aop_rgb332( GenefxState *gfxs )
{
     int  w   = gfxs->length;
     u8  *S   = gfxs->Bop[0];
     u8  *D   = gfxs->Aop[0];
     u32  Cop = gfxs->Cop;
     u32  rgb = ((Cop & 0xe0) << 16) | ((Cop & 0x1c) << 8) | (Cop & 0x03);

#define SET_PIXEL(d,s)                                                      \
     switch (s) {                                                           \
          case 0xff: d = Cop;                                               \
          case 0: break;                                                    \
          default: {                                                        \
               u32 a = s + 1;                                               \
               u32 t = ((d & 0xe0) << 16) | ((d & 0x1c) << 8) | (d & 0x03); \
               u32 c = ((rgb - t) * a + (t << 8)) & 0xe01c0300;             \
               d = (c >> 24) | ((c >> 16) & 0xff) | ((c >> 8) & 0xff);      \
          }                                                                 \
     }

     SET_PIXEL_DUFFS_DEVICE( D, S, w );

#undef SET_PIXEL
}

static void
Bop_a8_set_alphapixel_Aop_uyvy( GenefxState *gfxs )
{
     int  w    = gfxs->length;
     u8  *S    = gfxs->Bop[0];
     u16 *D    = gfxs->Aop[0];
     u32  y    = gfxs->YCop;
     u32  u    = gfxs->CbCop;
     u32  v    = gfxs->CrCop;
     u16  Cop0 = u | (y << 8);
     u16  Cop1 = v | (y << 8);

#define SET_PIXEL(d,s)                                                                             \
     switch (s) {                                                                                  \
          case 0xff: d = ((long) &(d) & 2) ? Cop1 : Cop0;                                          \
          case 0x00: break;                                                                        \
          default: {                                                                               \
               u32 a  = s + 1;                                                                     \
               u32 t1 = d & 0xff;                                                                  \
               u32 t2 = d >> 8;                                                                    \
               if ((long) &(d) & 2)                                                                \
                    d = (((v - t1) * a + (t1 << 8)) >> 8) | (((y - t2) * a + (t2 << 8)) & 0xff00); \
               else                                                                                \
                    d = (((u - t1) * a + (t1 << 8)) >> 8) | (((y - t2) * a + (t2 << 8)) & 0xff00); \
          }                                                                                        \
     }

     SET_PIXEL_DUFFS_DEVICE( D, S, w );

#undef SET_PIXEL
}

static void
Bop_a8_set_alphapixel_Aop_lut8( GenefxState *gfxs )
{
     int  w   = gfxs->length;
     u8  *S   = gfxs->Bop[0];
     u8  *D   = gfxs->Aop[0];
     u32  Cop = gfxs->Cop;

#define SET_PIXEL(d,s) \
     if (s & 0x80)     \
          d = Cop;

     SET_PIXEL_DUFFS_DEVICE( D, S, w );

#undef SET_PIXEL
}

static void
Bop_a8_set_alphapixel_Aop_alut44( GenefxState *gfxs )
{
     int       w       = gfxs->length + 1;
     u8       *S       = gfxs->Bop[0];
     u8       *D       = gfxs->Aop[0];
     u32       Cop     = gfxs->Cop;
     DFBColor  color   = gfxs->color;
     DFBColor *entries = gfxs->Alut->entries;

     while (--w) {
          u16 s = *S;
          switch (s) {
               case 0xff: *D = Cop;
               case 0: break;
               default: {
                    u16      a  = s + 1;
                    u8       d  = *D;
                    DFBColor dc = entries[d&0x0f];
                    u16      sa = (d & 0xf0) + s;
                    dc.r = ((color.r - dc.r) * a + (dc.r << 8)) >> 8;
                    dc.g = ((color.g - dc.g) * a + (dc.g << 8)) >> 8;
                    dc.b = ((color.b - dc.b) * a + (dc.b << 8)) >> 8;
                    if (sa & 0xff00) sa = 0xf0;
                    *D = (sa & 0xf0) + dfb_palette_search( gfxs->Alut, dc.r, dc.g, dc.b, 0x80 );
               }
          }

          ++S;
          ++D;
     }
}

static void
Bop_a8_set_alphapixel_Aop_airgb( GenefxState *gfxs )
{
     int  w   = gfxs->length;
     u8  *S   = gfxs->Bop[0];
     u32 *D   = gfxs->Aop[0];
     u32  Cop = gfxs->Cop;
     u32  rb  = Cop & 0x00ff00ff;
     u32  g   = gfxs->color.g;

#define SET_PIXEL(d,s)                                                                 \
     switch (s) {                                                                      \
          case 0xff: d = Cop;                                                          \
          case 0: break;                                                               \
          default: {                                                                   \
               u32 a  = s + 1;                                                         \
               u32 a1 = 256 - a;                                                       \
               s32 sa = (d >> 24) - s;                                                 \
               if (sa < 0) sa = 0;                                                     \
               d = (sa << 24) +                                                        \
                    (((((d & 0x00ff00ff)       * a1) + (rb * a)) >> 8) & 0x00ff00ff) + \
                    (((((d & 0x0000ff00) >> 8) * a1) + ( g * a))       & 0x0000ff00);  \
          }                                                                            \
     }

     SET_PIXEL_DUFFS_DEVICE( D, S, w );

#undef SET_PIXEL
}

static void
Bop_a8_set_alphapixel_Aop_argb1666( GenefxState *gfxs )
{
     int  w   = gfxs->length;
     u8  *S   = gfxs->Bop[0];
     u8  *D   = gfxs->Aop[0];
     u32  Cop = gfxs->Cop;
     u32  rb  = Cop & 0x3f03f;
     u32  g   = Cop & 0xfc0;

#define SET_PIXEL(d,s)                                                                                            \
     switch (s) {                                                                                                 \
          case 0xff: d = Cop;                                                                                     \
          case 0: break;                                                                                          \
          default: {                                                                                              \
               u32 a  = (s >> 2) + 1;                                                                             \
               u32 t1 = d & 0x3f03f;                                                                              \
               u32 t2 = d & 0x00fc0;                                                                              \
               d = (d & 0x40000)      |                                                                           \
                   ((s & 0x80) << 11) |                                                                           \
                   ((((rb - t1) * a + (t1 << 6)) & 0x00fc0fc0) + (((g - t2) * a + (t2 << 6)) & 0x0003f000)) >> 6; \
          }                                                                                                       \
     }

     SET_PIXEL_DUFFS_DEVICE( D, S, w );

#undef SET_PIXEL
}

static void
Bop_a8_set_alphapixel_Aop_argb6666( GenefxState *gfxs )
{

     int  w   = gfxs->length;
     u8  *S   = gfxs->Bop[0];
     u8  *D   = gfxs->Aop[0];
     u32  Cop = gfxs->Cop;
     u32  rb  = Cop & 0x3f03f;
     u32  g   = Cop & 0xfc0;

#define SET_PIXEL(d,s)                                                                                            \
     switch (s) {                                                                                                 \
          case 0xff: d = Cop;                                                                                     \
          case 0: break;                                                                                          \
          default: {                                                                                              \
               u32 a  = (s >> 2) + 1;                                                                             \
               u32 t1 = d & 0x3f03f;                                                                              \
               u32 t2 = d & 0x00fc0;                                                                              \
               d = (d & 0xfc0000)     |                                                                           \
                   ((s & 0xfc) << 16) |                                                                           \
                   ((((rb - t1) * a + (t1 << 6)) & 0x00fc0fc0) + (((g - t2) * a + (t2 << 6)) & 0x0003f000)) >> 6; \
          }                                                                                                       \
     }

     SET_PIXEL_DUFFS_DEVICE( D, S, w );

#undef SET_PIXEL
}

static void
Bop_a8_set_alphapixel_Aop_rgb18( GenefxState *gfxs )
{
     int  w   = gfxs->length;
     u8  *S   = gfxs->Bop[0];
     u8  *D   = gfxs->Aop[0];
     u32  Cop = gfxs->Cop;
     u32  rb  = Cop & 0x3f03f;
     u32  g   = Cop & 0xfc0;

#define SET_PIXEL(d,s)                                                                                            \
     switch (s) {                                                                                                 \
          case 0xff: d = Cop;                                                                                     \
          case 0: break;                                                                                          \
          default: {                                                                                              \
               u32 a  = (s >> 2) + 1;                                                                             \
               u32 t1 = d & 0x3f03f;                                                                              \
               u32 t2 = d & 0x00fc0;                                                                              \
               d = ((((rb - t1) * a + (t1 << 6)) & 0x00fc0fc0) + (((g - t2) * a + (t2 << 6)) & 0x000fc000)) >> 6; \
          }                                                                                                       \
     }

     SET_PIXEL_DUFFS_DEVICE( D, S, w );

#undef SET_PIXEL
}

static void
Bop_a8_set_alphapixel_Aop_rgba5551( GenefxState *gfxs )
{
     int  w   = gfxs->length;
     u8  *S   = gfxs->Bop[0];
     u16 *D   = gfxs->Aop[0];
     u32  Cop = gfxs->Cop;
     u32  rb  = Cop & 0xf83e;
     u32  g   = Cop & 0x07c0;

#define SET_PIXEL(d,s)                                                                                            \
     switch (s) {                                                                                                 \
          case 0xff: d = Cop;                                                                                     \
          case 0: break;                                                                                          \
          default: {                                                                                              \
               u32 a  = (s >> 3) + 1;                                                                             \
               u32 t1 = d & 0xf83e;                                                                               \
               u32 t2 = d & 0x07c0;                                                                               \
               d = (d & 0x0001)      |                                                                            \
                   ((s & 0x80) >> 7) |                                                                            \
                   ((((rb - t1) * a + (t1 << 4)) & 0x000f83e0) + (((g - t2) * a + (t2 << 4)) & 0x00007c00)) >> 4; \
          }                                                                                                       \
     }

     SET_PIXEL_DUFFS_DEVICE( D, S, w );

#undef SET_PIXEL
}

static void
Bop_a8_set_alphapixel_Aop_y444( GenefxState *gfxs )
{
     int  w     = gfxs->length + 1;
     u8  *S     = gfxs->Bop[0];
     u8  *Dy    = gfxs->Aop[0];
     u8  *Du    = gfxs->Aop[1];
     u8  *Dv    = gfxs->Aop[2];
     u8   YCop  = gfxs->YCop;
     u8   CbCop = gfxs->CbCop;
     u8   CrCop = gfxs->CrCop;

     while (--w) {
          u16 s = *S;
          switch (s) {
               case 0xff: *Dy = YCop; *Du = CbCop; *Dv = CrCop;
               case 0: break;
               default: {
                    u16 a  = s + 1;
                    u8  dy = *Dy;
                    u8  du = *Du;
                    u8  dv = *Dv;
                    *Dy = ((YCop  - dy) * a + (dy << 8)) >> 8;
                    *Du = ((CbCop - du) * a + (du << 8)) >> 8;
                    *Dv = ((CrCop - dv) * a + (dv << 8)) >> 8;
               }
          }

          ++S;
          ++Dy; ++Du; ++Dv;
     }
}

static void
Bop_a8_set_alphapixel_Aop_argb8565( GenefxState *gfxs )
{
     int  w   = gfxs->length;
     u8  *S   = gfxs->Bop[0];
     u8  *D   = gfxs->Aop[0];
     u32  Cop = gfxs->Cop | 0xff0000;
     u32  srb = Cop & 0xf81f;
     u32  sg  = Cop & 0x07e0;

#ifdef WORDS_BIGENDIAN
#define SET_PIXEL(D,s)                                                                                                 \
     switch (s) {                                                                                                      \
          case 0xff:                                                                                                   \
            D[0] = (Cop >> 16) & 0xff;                                                                                 \
            D[1] = (Cop >>  8) & 0xff;                                                                                 \
            D[2] =  Cop        & 0xff;                                                                                 \
          case 0: break;                                                                                               \
          default: {                                                                                                   \
               u32 a   = s + 1;                                                                                        \
               u32 a1  = 256 - s;                                                                                      \
               u16 d16 = D[1] << 8 | D[2];                                                                             \
               u32 t1  = (d16 & 0xf81f);                                                                               \
               u32 t2  = (d16 & 0x07e0);                                                                               \
               D[0] = ((D[0] * a1) >> 8) + s;                                                                          \
               d16  = ((((srb - t1) * a + (t1 << 8)) & 0x00f81f00) + (((sg - t2) * a + (t2 << 8)) & 0x0007e000)) >> 8; \
               D[1] = (d16 >> 8) & 0xff;                                                                               \
               D[2] =  d16       & 0xff;                                                                               \
          }                                                                                                            \
     }
#else
#define SET_PIXEL(D,s)                                                                                                 \
     switch (s) {                                                                                                      \
          case 0xff:                                                                                                   \
            D[0] =  Cop        & 0xff;                                                                                 \
            D[1] = (Cop >>  8) & 0xff;                                                                                 \
            D[2] = (Cop >> 16) & 0xff;                                                                                 \
          case 0: break;                                                                                               \
          default: {                                                                                                   \
               u32 a   = s + 1;                                                                                        \
               u32 a1  = 256 - s;                                                                                      \
               u16 d16 = D[1] << 8 | D[0];                                                                             \
               u32 t1  = (d16 & 0xf81f);                                                                               \
               u32 t2  = (d16 & 0x07e0);                                                                               \
               D[2] = ((D[2] * a1) >> 8) + s;                                                                          \
               d16  = ((((srb - t1) * a + (t1 << 8)) & 0x00f81f00) + (((sg - t2) * a + (t2 << 8)) & 0x0007e000)) >> 8; \
               D[1] = (d16 >> 8) & 0xff;                                                                               \
               D[0] =  d16       & 0xff;                                                                               \
          }                                                                                                            \
     }
#endif

     while (w > 4) {
          SET_PIXEL( D, *S ); D += 3; ++S;
          SET_PIXEL( D, *S ); D += 3; ++S;
          SET_PIXEL( D, *S ); D += 3; ++S;
          SET_PIXEL( D, *S ); D += 3; ++S;
          w -= 4;
     }

     while (w--) {
          SET_PIXEL( D, *S ); D += 3; ++S;
     }

#undef SET_PIXEL
}

static void
Bop_a8_set_alphapixel_Aop_vyu( GenefxState *gfxs )
{
     int  w     = gfxs->length;
     u8  *S     = gfxs->Bop[0];
     u8  *D     = gfxs->Aop[0];
     u8   YCop  = gfxs->YCop;
     u8   CbCop = gfxs->CbCop;
     u8   CrCop = gfxs->CrCop;

#ifdef WORDS_BIGENDIAN
#define SET_PIXEL(D,y,cb,cr,s)                              \
     switch (s) {                                           \
         case 0xff:                                         \
               D[0] = cr;                                   \
               D[1] = y;                                    \
               D[2] = cb;                                   \
          case 0: break;                                    \
          default: {                                        \
               u16 a = s + 1;                               \
               D[0] = ((cr - D[0]) * a + (D[0] << 8)) >> 8; \
               D[1] = ((y  - D[1]) * a + (D[1] << 8)) >> 8; \
               D[2] = ((cb - D[2]) * a + (D[2] << 8)) >> 8; \
          }                                                 \
     }
#else
#define SET_PIXEL(D,y,cb,cr,s)                              \
     switch (s) {                                           \
         case 0xff:                                         \
               D[0] = cb;                                   \
               D[1] = y;                                    \
               D[2] = cr;                                   \
          case 0: break;                                    \
          default: {                                        \
               u16 a = s + 1;                               \
               D[0] = ((cb - D[0]) * a + (D[0] << 8)) >> 8; \
               D[1] = ((y  - D[1]) * a + (D[1] << 8)) >> 8; \
               D[2] = ((cr - D[2]) * a + (D[2] << 8)) >> 8; \
          }                                                 \
     }
#endif

     while (w > 4) {
          SET_PIXEL( D, YCop, CbCop, CrCop, *S ); D += 3; ++S;
          SET_PIXEL( D, YCop, CbCop, CrCop, *S ); D += 3; ++S;
          SET_PIXEL( D, YCop, CbCop, CrCop, *S ); D += 3; ++S;
          SET_PIXEL( D, YCop, CbCop, CrCop, *S ); D += 3; ++S;
          w -= 4;
     }

     while (w--) {
          SET_PIXEL( D, YCop, CbCop, CrCop, *S ); D += 3; ++S;
     }

#undef SET_PIXEL
}

static void
Bop_a8_set_alphapixel_Aop_bgr24( GenefxState *gfxs )
{
     int       w     = gfxs->length;
     u8       *S     = gfxs->Bop[0];
     u8       *D     = gfxs->Aop[0];
     DFBColor  color = gfxs->color;

#define SET_PIXEL(D,r,g,b,s)                               \
     switch (s) {                                          \
         case 0xff:                                        \
               D[0] = r;                                   \
               D[1] = g;                                   \
               D[2] = b;                                   \
          case 0: break;                                   \
          default: {                                       \
               u16 a = s + 1;                              \
               D[0] = ((r - D[0]) * a + (D[0] << 8)) >> 8; \
               D[1] = ((g - D[1]) * a + (D[1] << 8)) >> 8; \
               D[2] = ((b - D[2]) * a + (D[2] << 8)) >> 8; \
          }                                                \
     }

     while (w > 4) {
          SET_PIXEL( D, color.r, color.g, color.b, *S ); D += 3; ++S;
          SET_PIXEL( D, color.r, color.g, color.b, *S ); D += 3; ++S;
          SET_PIXEL( D, color.r, color.g, color.b, *S ); D += 3; ++S;
          SET_PIXEL( D, color.r, color.g, color.b, *S ); D += 3; ++S;
          w -= 4;
     }

     while (w--) {
          SET_PIXEL( D, color.r, color.g, color.b, *S ); D += 3, ++S;
     }

#undef SET_PIXEL
}

#undef SET_PIXEL_DUFFS_DEVICE

static GenefxFunc Bop_a8_set_alphapixel_Aop_PFI[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1555)]   = Bop_a8_set_alphapixel_Aop_argb1555,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB16)]      = Bop_a8_set_alphapixel_Aop_rgb16,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB24)]      = Bop_a8_set_alphapixel_Aop_rgb24,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB32)]      = Bop_a8_set_alphapixel_Aop_rgb32,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]       = Bop_a8_set_alphapixel_Aop_argb,
     [DFB_PIXELFORMAT_INDEX(DSPF_A8)]         = Bop_a8_set_alphapixel_Aop_a8,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUY2)]       = Bop_a8_set_alphapixel_Aop_yuy2,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB332)]     = Bop_a8_set_alphapixel_Aop_rgb332,
     [DFB_PIXELFORMAT_INDEX(DSPF_UYVY)]       = Bop_a8_set_alphapixel_Aop_uyvy,
     [DFB_PIXELFORMAT_INDEX(DSPF_I420)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT8)]       = Bop_a8_set_alphapixel_Aop_lut8,
     [DFB_PIXELFORMAT_INDEX(DSPF_ALUT44)]     = Bop_a8_set_alphapixel_Aop_alut44,
     [DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)]      = Bop_a8_set_alphapixel_Aop_airgb,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB2554)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA4444)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV21)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AYUV)]       = Bop_a8_set_alphapixel_Aop_argb,
     [DFB_PIXELFORMAT_INDEX(DSPF_A4)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1666)]   = Bop_a8_set_alphapixel_Aop_argb1666,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB6666)]   = Bop_a8_set_alphapixel_Aop_argb6666,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB18)]      = Bop_a8_set_alphapixel_Aop_rgb18,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB444)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB555)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR555)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA5551)]   = Bop_a8_set_alphapixel_Aop_rgba5551,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y444)]       = Bop_a8_set_alphapixel_Aop_y444,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB8565)]   = Bop_a8_set_alphapixel_Aop_argb8565,
     [DFB_PIXELFORMAT_INDEX(DSPF_AVYU)]       = Bop_a8_set_alphapixel_Aop_argb,
     [DFB_PIXELFORMAT_INDEX(DSPF_VYU)]        = Bop_a8_set_alphapixel_Aop_vyu,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1_LSB)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ABGR)]       = Bop_a8_set_alphapixel_Aop_argb,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBAF88871)] = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT1)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV61)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y42B)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV24)]       = Bop_a8_set_alphapixel_Aop_y444,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV24)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV42)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR24)]      = Bop_a8_set_alphapixel_Aop_bgr24,
};

/**********************************************************************************************************************
 ********************************* Bop_a1_set_alphapixel_Aop_PFI ******************************************************
 **********************************************************************************************************************/

static void
Bop_a1_set_alphapixel_Aop_argb1555( GenefxState *gfxs )
{
     int  i;
     int  w   = gfxs->length;
     u8  *S   = gfxs->Bop[0];
     u16 *D   = gfxs->Aop[0];
     u16  Cop = gfxs->Cop | 0x8000;

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (0x80 >> (i & 7))) {
               D[i] = Cop;
          }
     }
}

static void
Bop_a1_set_alphapixel_Aop_rgb16( GenefxState *gfxs )
{
     int  i;
     int  w   = gfxs->length;
     u8  *S   = gfxs->Bop[0];
     u16 *D   = gfxs->Aop[0];
     u16  Cop = gfxs->Cop;

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (0x80 >> (i & 7))) {
               D[i] = Cop;
          }
     }
}

static void
Bop_a1_set_alphapixel_Aop_rgb24( GenefxState *gfxs )
{
     int       i;
     int       w     = gfxs->length;
     u8       *S     = gfxs->Bop[0];
     u8       *D     = gfxs->Aop[0];
     DFBColor  color = gfxs->color;

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (0x80 >> (i & 7))) {
               D[0] = color.b;
               D[1] = color.g;
               D[2] = color.r;
          }

          D += 3;
     }
}

static void
Bop_a1_set_alphapixel_Aop_rgb32( GenefxState *gfxs )
{
     int  i;
     int  w   = gfxs->length;
     u8  *S   = gfxs->Bop[0];
     u32 *D   = gfxs->Aop[0];
     u32  Cop = gfxs->Cop;

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (0x80 >> (i & 7))) {
               D[i] = Cop;
          }
     }
}

static void
Bop_a1_set_alphapixel_Aop_argb( GenefxState *gfxs )
{
     int  i;
     int  w   = gfxs->length;
     u8  *S   = gfxs->Bop[0];
     u32 *D   = gfxs->Aop[0];
     u32  Cop = gfxs->Cop | 0xff000000;

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (0x80 >> (i & 7))) {
               D[i] = Cop;
          }
     }
}

static void
Bop_a1_set_alphapixel_Aop_a8( GenefxState *gfxs )
{
     int  i;
     int  w = gfxs->length;
     u8  *S = gfxs->Bop[0];
     u8  *D = gfxs->Aop[0];

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (0x80 >> (i & 7))) {
               D[i] = 0xff;
          }
     }
}

static void
Bop_a1_set_alphapixel_Aop_yuy2( GenefxState *gfxs )
{
     int  i;
     int  w    = gfxs->length;
     u8  *S    = gfxs->Bop[0];
     u16 *D    = gfxs->Aop[0];
     u16  Cop0 = gfxs->YCop | (gfxs->CbCop << 8);
     u16  Cop1 = gfxs->YCop | (gfxs->CrCop << 8);

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (0x80 >> (i & 7))) {
               D[i] = ((long) &D[i] & 2) ? Cop1 : Cop0;
          }
     }
}

static void
Bop_a1_set_alphapixel_Aop_rgb332( GenefxState *gfxs )
{
     int  i;
     int  w   = gfxs->length;
     u8  *S   = gfxs->Bop[0];
     u8  *D   = gfxs->Aop[0];
     u8   Cop = gfxs->Cop;

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (0x80 >> (i & 7))) {
               D[i] = Cop;
          }
     }
}

static void
Bop_a1_set_alphapixel_Aop_uyvy( GenefxState *gfxs )
{
     int  i;
     int  w    = gfxs->length;
     u8  *S    = gfxs->Bop[0];
     u16 *D    = gfxs->Aop[0];
     u16  Cop0 = gfxs->CbCop | (gfxs->YCop << 8);
     u16  Cop1 = gfxs->CrCop | (gfxs->YCop << 8);

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (0x80 >> (i & 7))) {
               D[i] = ((long) &D[i] & 2) ? Cop1 : Cop0;
          }
     }
}

static void
Bop_a1_set_alphapixel_Aop_lut8( GenefxState *gfxs )
{
     int  i;
     int  w   = gfxs->length;
     u8  *S   = gfxs->Bop[0];
     u8  *D   = gfxs->Aop[0];
     u8   Cop = gfxs->Cop;

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (0x80 >> (i & 7))) {
               D[i] = Cop;
          }
     }
}

static void
Bop_a1_set_alphapixel_Aop_alut44( GenefxState *gfxs )
{
     int  i;
     int  w   = gfxs->length;
     u8  *S   = gfxs->Bop[0];
     u8  *D   = gfxs->Aop[0];
     u8   Cop = gfxs->Cop;

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (0x80 >> (i & 7))) {
               D[i] = Cop;
          }
     }
}

static void
Bop_a1_set_alphapixel_Aop_airgb( GenefxState *gfxs )
{
     int  i;
     int  w   = gfxs->length;
     u8  *S   = gfxs->Bop[0];
     u32 *D   = gfxs->Aop[0];
     u32  Cop = gfxs->Cop & 0x00ffffff;

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (0x80 >> (i & 7))) {
               D[i] = Cop;
          }
     }
}

static void
Bop_a1_set_alphapixel_Aop_argb2554( GenefxState *gfxs )
{
     int  i;
     int  w   = gfxs->length;
     u8  *S   = gfxs->Bop[0];
     u16 *D   = gfxs->Aop[0];
     u16  Cop = gfxs->Cop | 0xc000;

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (0x80 >> (i & 7))) {
               D[i] = Cop;
          }
     }
}

static void
Bop_a1_set_alphapixel_Aop_argb4444( GenefxState *gfxs )
{
     int  i;
     int  w   = gfxs->length;
     u8  *S   = gfxs->Bop[0];
     u16 *D   = gfxs->Aop[0];
     u16  Cop = gfxs->Cop | 0xf000;

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (0x80 >> (i & 7))) {
               D[i] = Cop;
          }
     }
}

static void
Bop_a1_set_alphapixel_Aop_rgba4444( GenefxState *gfxs )
{
     int  i;
     int  w   = gfxs->length;
     u8  *S   = gfxs->Bop[0];
     u16 *D   = gfxs->Aop[0];
     u16  Cop = gfxs->Cop | 0x000f;

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (0x80 >> (i & 7))) {
               D[i] = Cop;
          }
     }
}

static void
Bop_a1_set_alphapixel_Aop_argb1666( GenefxState *gfxs )
{
     int       i;
     int       w     = gfxs->length;
     u8       *S     = gfxs->Bop[0];
     u8       *D     = gfxs->Aop[0];
     DFBColor  color = gfxs->color;

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (0x80 >> (i & 7))) {
               u32 pixel = PIXEL_ARGB1666( color.a, color.r, color.g, color.b );

               D[0] = pixel;
               D[1] = pixel >> 8;
               D[2] = pixel >> 16;
          }

          D += 3;
     }
}

static void
Bop_a1_set_alphapixel_Aop_argb6666( GenefxState *gfxs )
{
     int       i;
     int       w     = gfxs->length;
     u8       *S     = gfxs->Bop[0];
     u8       *D     = gfxs->Aop[0];
     DFBColor  color = gfxs->color;

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (0x80 >> (i & 7))) {
               u32 pixel = PIXEL_ARGB6666( color.a, color.r, color.g, color.b );

               D[0] = pixel;
               D[1] = pixel >> 8;
               D[2] = pixel >> 16;
          }

          D += 3;
     }
}

static void
Bop_a1_set_alphapixel_Aop_rgb18( GenefxState *gfxs )
{
     int       i;
     int       w     = gfxs->length;
     u8       *S     = gfxs->Bop[0];
     u8       *D     = gfxs->Aop[0];
     DFBColor  color = gfxs->color;

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (0x80 >> (i & 7))) {
               u32 pixel = PIXEL_RGB18( color.r, color.g, color.b );

               D[0] = pixel;
               D[1] = pixel >> 8;
               D[2] = pixel >> 16;
          }

          D += 3;
     }
}

static void
Bop_a1_set_alphapixel_Aop_rgba5551( GenefxState *gfxs )
{
     int  i;
     int  w   = gfxs->length;
     u8  *S   = gfxs->Bop[0];
     u16 *D   = gfxs->Aop[0];
     u16  Cop = gfxs->Cop | 0x0001;

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (0x80 >> (i & 7))) {
               D[i] = Cop;
          }
     }
}

static void
Bop_a1_set_alphapixel_Aop_y444( GenefxState *gfxs )
{
     int  i;
     int  w  = gfxs->length;
     u8  *S  = gfxs->Bop[0];
     u8  *Dy = gfxs->Aop[0];
     u8  *Du = gfxs->Aop[1];
     u8  *Dv = gfxs->Aop[2];

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (0x80 >> (i & 7))) {
               Dy[i] = gfxs->YCop;
               Du[i] = gfxs->CbCop;
               Dv[i] = gfxs->CrCop;
          }
     }
}

static void
Bop_a1_set_alphapixel_Aop_argb8565( GenefxState *gfxs )
{
     int  i;
     int  w   = gfxs->length;
     u8  *S   = gfxs->Bop[0];
     u8  *D   = gfxs->Aop[0];
     u32  Cop = gfxs->Cop | 0xff0000;

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (0x80 >> (i & 7))) {
#ifdef WORDS_BIGENDIAN
               D[0] = (Cop >> 16) & 0xff;
               D[1] = (Cop >>  8) & 0xff;
               D[2] =  Cop        & 0xff;
#else
               D[0] =  Cop        & 0xff;
               D[1] = (Cop >>  8) & 0xff;
               D[2] = (Cop >> 16) & 0xff;
#endif
          }

          D += 3;
     }
}

static void
Bop_a1_set_alphapixel_Aop_bgr24( GenefxState *gfxs )
{
     int       i;
     int       w     = gfxs->length;
     u8       *S     = gfxs->Bop[0];
     u8       *D     = gfxs->Aop[0];
     DFBColor  color = gfxs->color;

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (0x80 >> (i & 7))) {
               D[0] = color.r;
               D[1] = color.g;
               D[2] = color.b;
          }

          D += 3;
     }
}

static GenefxFunc Bop_a1_set_alphapixel_Aop_PFI[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1555)]   = Bop_a1_set_alphapixel_Aop_argb1555,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB16)]      = Bop_a1_set_alphapixel_Aop_rgb16,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB24)]      = Bop_a1_set_alphapixel_Aop_rgb24,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB32)]      = Bop_a1_set_alphapixel_Aop_rgb32,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]       = Bop_a1_set_alphapixel_Aop_argb,
     [DFB_PIXELFORMAT_INDEX(DSPF_A8)]         = Bop_a1_set_alphapixel_Aop_a8,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUY2)]       = Bop_a1_set_alphapixel_Aop_yuy2,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB332)]     = Bop_a1_set_alphapixel_Aop_rgb332,
     [DFB_PIXELFORMAT_INDEX(DSPF_UYVY)]       = Bop_a1_set_alphapixel_Aop_uyvy,
     [DFB_PIXELFORMAT_INDEX(DSPF_I420)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT8)]       = Bop_a1_set_alphapixel_Aop_lut8,
     [DFB_PIXELFORMAT_INDEX(DSPF_ALUT44)]     = Bop_a1_set_alphapixel_Aop_alut44,
     [DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)]      = Bop_a1_set_alphapixel_Aop_airgb,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB2554)]   = Bop_a1_set_alphapixel_Aop_argb2554,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)]   = Bop_a1_set_alphapixel_Aop_argb4444,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA4444)]   = Bop_a1_set_alphapixel_Aop_rgba4444,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV21)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AYUV)]       = Bop_a1_set_alphapixel_Aop_argb,
     [DFB_PIXELFORMAT_INDEX(DSPF_A4)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1666)]   = Bop_a1_set_alphapixel_Aop_argb1666,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB6666)]   = Bop_a1_set_alphapixel_Aop_argb6666,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB18)]      = Bop_a1_set_alphapixel_Aop_rgb18,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB444)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB555)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR555)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA5551)]   = Bop_a1_set_alphapixel_Aop_rgba5551,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y444)]       = Bop_a1_set_alphapixel_Aop_y444,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB8565)]   = Bop_a1_set_alphapixel_Aop_argb8565,
     [DFB_PIXELFORMAT_INDEX(DSPF_AVYU)]       = Bop_a1_set_alphapixel_Aop_argb,
     [DFB_PIXELFORMAT_INDEX(DSPF_VYU)]        = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1_LSB)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ABGR)]       = Bop_a1_set_alphapixel_Aop_argb,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBAF88871)] = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT1)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV61)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y42B)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV24)]       = Bop_a1_set_alphapixel_Aop_y444,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV24)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV42)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR24)]      = Bop_a1_set_alphapixel_Aop_bgr24,
};

/**********************************************************************************************************************
 ********************************* Bop_a1_lsb_set_alphapixel_Aop_PFI **************************************************
 **********************************************************************************************************************/

static void
Bop_a1_lsb_set_alphapixel_Aop_argb1555( GenefxState *gfxs )
{
     int  i;
     int  w   = gfxs->length;
     u8  *S   = gfxs->Bop[0];
     u16 *D   = gfxs->Aop[0];
     u16  Cop = gfxs->Cop | 0x8000;

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (1 << (i & 7))) {
               D[i] = Cop;
          }
     }
}

static void
Bop_a1_lsb_set_alphapixel_Aop_rgb16( GenefxState *gfxs )
{
     int  i;
     int  w   = gfxs->length;
     u8  *S   = gfxs->Bop[0];
     u16 *D   = gfxs->Aop[0];
     u16  Cop = gfxs->Cop;

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (1 << (i & 7))) {
               D[i] = Cop;
          }
     }
}

static void
Bop_a1_lsb_set_alphapixel_Aop_rgb24( GenefxState *gfxs )
{
     int       i;
     int       w     = gfxs->length;
     u8       *S     = gfxs->Bop[0];
     u8       *D     = gfxs->Aop[0];
     DFBColor  color = gfxs->color;

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (1 << (i & 7))) {
               D[0] = color.b;
               D[1] = color.g;
               D[2] = color.r;
          }

          D += 3;
     }
}

static void
Bop_a1_lsb_set_alphapixel_Aop_rgb32( GenefxState *gfxs )
{
     int  i;
     int  w   = gfxs->length;
     u8  *S   = gfxs->Bop[0];
     u32 *D   = gfxs->Aop[0];
     u32  Cop = gfxs->Cop;

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (1 << (i & 7))) {
               D[i] = Cop;
          }
     }
}

static void
Bop_a1_lsb_set_alphapixel_Aop_argb( GenefxState *gfxs )
{
     int  i;
     int  w   = gfxs->length;
     u8  *S   = gfxs->Bop[0];
     u32 *D   = gfxs->Aop[0];
     u32  Cop = gfxs->Cop | 0xff000000;

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (1 << (i & 7))) {
               D[i] = Cop;
          }
     }
}

static void
Bop_a1_lsb_set_alphapixel_Aop_a8( GenefxState *gfxs )
{
     int  i;
     int  w = gfxs->length;
     u8  *S = gfxs->Bop[0];
     u8  *D = gfxs->Aop[0];

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (1 << (i & 7))) {
               D[i] = 0xff;
          }
     }
}

static void
Bop_a1_lsb_set_alphapixel_Aop_yuy2( GenefxState *gfxs )
{
     int  i;
     int  w    = gfxs->length;
     u8  *S    = gfxs->Bop[0];
     u16 *D    = gfxs->Aop[0];
     u16  Cop0 = gfxs->YCop | (gfxs->CbCop << 8);
     u16  Cop1 = gfxs->YCop | (gfxs->CrCop << 8);

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (1 << (i & 7))) {
               D[i] = ((long) &D[i] & 2) ? Cop1 : Cop0;
          }
     }
}

static void
Bop_a1_lsb_set_alphapixel_Aop_rgb332( GenefxState *gfxs )
{
     int  i;
     int  w   = gfxs->length;
     u8  *S   = gfxs->Bop[0];
     u8  *D   = gfxs->Aop[0];
     u8   Cop = gfxs->Cop;

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (1 << (i & 7))) {
               D[i] = Cop;
          }
     }
}

static void
Bop_a1_lsb_set_alphapixel_Aop_uyvy( GenefxState *gfxs )
{
     int  i;
     int  w    = gfxs->length;
     u8  *S    = gfxs->Bop[0];
     u16 *D    = gfxs->Aop[0];
     u16  Cop0 = gfxs->CbCop | (gfxs->YCop << 8);
     u16  Cop1 = gfxs->CrCop | (gfxs->YCop << 8);

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (1 << (i & 7))) {
               D[i] = ((long) &D[i] & 2) ? Cop1 : Cop0;
          }
     }
}

static void
Bop_a1_lsb_set_alphapixel_Aop_lut8( GenefxState *gfxs )
{
     int  i;
     int  w   = gfxs->length;
     u8  *S   = gfxs->Bop[0];
     u8  *D   = gfxs->Aop[0];
     u8   Cop = gfxs->Cop;

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (1 << (i & 7))) {
               D[i] = Cop;
          }
     }
}

static void
Bop_a1_lsb_set_alphapixel_Aop_alut44( GenefxState *gfxs )
{
     int  i;
     int  w   = gfxs->length;
     u8  *S   = gfxs->Bop[0];
     u8  *D   = gfxs->Aop[0];
     u8   Cop = gfxs->Cop;

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (1 << (i & 7))) {
               D[i] = Cop;
          }
     }
}

static void
Bop_a1_lsb_set_alphapixel_Aop_airgb( GenefxState *gfxs )
{
     int  i;
     int  w   = gfxs->length;
     u8  *S   = gfxs->Bop[0];
     u32 *D   = gfxs->Aop[0];
     u32  Cop = gfxs->Cop & 0x00ffffff;

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (1 << (i & 7))) {
               D[i] = Cop;
          }
     }
}

static void
Bop_a1_lsb_set_alphapixel_Aop_argb2554( GenefxState *gfxs )
{
     int  i;
     int  w   = gfxs->length;
     u8  *S   = gfxs->Bop[0];
     u16 *D   = gfxs->Aop[0];
     u16  Cop = gfxs->Cop | 0xc000;

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (1 << (i & 7))) {
               D[i] = Cop;
          }
     }
}

static void
Bop_a1_lsb_set_alphapixel_Aop_argb4444( GenefxState *gfxs )
{
     int  i;
     int  w   = gfxs->length;
     u8  *S   = gfxs->Bop[0];
     u16 *D   = gfxs->Aop[0];
     u16  Cop = gfxs->Cop | 0xf000;

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (1 << (i & 7))) {
               D[i] = Cop;
          }
     }
}

static void
Bop_a1_lsb_set_alphapixel_Aop_argb1666( GenefxState *gfxs )
{
     int       i;
     int       w     = gfxs->length;
     u8       *S     = gfxs->Bop[0];
     u8       *D     = gfxs->Aop[0];
     DFBColor  color = gfxs->color;

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (1 << (i & 7))) {
               u32 pixel = PIXEL_ARGB1666( color.a, color.r, color.g, color.b );

               D[0] = pixel;
               D[1] = pixel >> 8;
               D[2] = pixel >> 16;
          }

          D += 3;
     }
}

static void
Bop_a1_lsb_set_alphapixel_Aop_argb6666( GenefxState *gfxs )
{
     int       i;
     int       w     = gfxs->length;
     u8       *S     = gfxs->Bop[0];
     u8       *D     = gfxs->Aop[0];
     DFBColor  color = gfxs->color;

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (1 << (i & 7))) {
               u32 pixel = PIXEL_ARGB6666( color.a, color.r, color.g, color.b );

               D[0] = pixel;
               D[1] = pixel >> 8;
               D[2] = pixel >> 16;
          }

          D += 3;
     }
}

static void
Bop_a1_lsb_set_alphapixel_Aop_rgb18( GenefxState *gfxs )
{
     int       i;
     int       w     = gfxs->length;
     u8       *S     = gfxs->Bop[0];
     u8       *D     = gfxs->Aop[0];
     DFBColor  color = gfxs->color;

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (1 << (i & 7))) {
               u32 pixel = PIXEL_RGB18( color.r, color.g, color.b );

               D[0] = pixel;
               D[1] = pixel >> 8;
               D[2] = pixel >> 16;
          }

          D += 3;
     }
}

static void
Bop_a1_lsb_set_alphapixel_Aop_bgr24( GenefxState *gfxs )
{
     int       i;
     int       w     = gfxs->length;
     u8       *S     = gfxs->Bop[0];
     u8       *D     = gfxs->Aop[0];
     DFBColor  color = gfxs->color;

     for (i = 0; i < w; ++i) {
          if (S[i>>3] & (1 << (i & 7))) {
               D[0] = color.r;
               D[1] = color.g;
               D[2] = color.b;
          }

          D += 3;
     }
}

static GenefxFunc Bop_a1_lsb_set_alphapixel_Aop_PFI[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1555)]   = Bop_a1_lsb_set_alphapixel_Aop_argb1555,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB16)]      = Bop_a1_lsb_set_alphapixel_Aop_rgb16,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB24)]      = Bop_a1_lsb_set_alphapixel_Aop_rgb24,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB32)]      = Bop_a1_lsb_set_alphapixel_Aop_rgb32,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]       = Bop_a1_lsb_set_alphapixel_Aop_argb,
     [DFB_PIXELFORMAT_INDEX(DSPF_A8)]         = Bop_a1_lsb_set_alphapixel_Aop_a8,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUY2)]       = Bop_a1_lsb_set_alphapixel_Aop_yuy2,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB332)]     = Bop_a1_lsb_set_alphapixel_Aop_rgb332,
     [DFB_PIXELFORMAT_INDEX(DSPF_UYVY)]       = Bop_a1_lsb_set_alphapixel_Aop_uyvy,
     [DFB_PIXELFORMAT_INDEX(DSPF_I420)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT8)]       = Bop_a1_lsb_set_alphapixel_Aop_lut8,
     [DFB_PIXELFORMAT_INDEX(DSPF_ALUT44)]     = Bop_a1_lsb_set_alphapixel_Aop_alut44,
     [DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)]      = Bop_a1_lsb_set_alphapixel_Aop_airgb,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB2554)]   = Bop_a1_lsb_set_alphapixel_Aop_argb2554,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)]   = Bop_a1_lsb_set_alphapixel_Aop_argb4444,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA4444)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV21)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AYUV)]       = Bop_a1_lsb_set_alphapixel_Aop_argb,
     [DFB_PIXELFORMAT_INDEX(DSPF_A4)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1666)]   = Bop_a1_lsb_set_alphapixel_Aop_argb1666,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB6666)]   = Bop_a1_lsb_set_alphapixel_Aop_argb6666,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB18)]      = Bop_a1_lsb_set_alphapixel_Aop_rgb18,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB444)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB555)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR555)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA5551)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y444)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB8565)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AVYU)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_VYU)]        = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1_LSB)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ABGR)]       = Bop_a1_lsb_set_alphapixel_Aop_argb,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBAF88871)] = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT1)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV61)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y42B)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV24)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV24)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV42)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR24)]      = Bop_a1_lsb_set_alphapixel_Aop_bgr24,
};

/**********************************************************************************************************************
 ********************************* Bop_lut2_translate_to_Aop_lut8 *****************************************************
 **********************************************************************************************************************/

static void
Bop_lut2_translate_to_Aop_lut8_C( GenefxState *gfxs )
{
     int  i;
     int  w = gfxs->length;
     int  l = (w + 3) / 4;
     u8  *S = gfxs->Bop[0];
     u8  *D = gfxs->Aop[0];

     for (i = 0; i < l; ++i, D += 4, w -= 4) {
          u8 index;
          u8 pixels = S[i];

          switch (w) {
               default:
                    index = (pixels & 3);
                    if (index < gfxs->num_trans && gfxs->trans[index] >= 0)
                         D[3] = gfxs->trans[index];

               case 3:
                    index = (pixels >> 2) & 3;
                    if (index < gfxs->num_trans && gfxs->trans[index] >= 0)
                         D[2] = gfxs->trans[index];

               case 2:
                    index = (pixels >> 4) & 3;
                    if (index < gfxs->num_trans && gfxs->trans[index] >= 0)
                         D[1] = gfxs->trans[index];

               case 1:
                    index = pixels >> 6;
                    if (index < gfxs->num_trans && gfxs->trans[index] >= 0)
                         D[0] = gfxs->trans[index];
          }
     }
}

static GenefxFunc Bop_lut2_translate_to_Aop_lut8 = Bop_lut2_translate_to_Aop_lut8_C;

/**********************************************************************************************************************
 ********************************* Xacc_blend *************************************************************************
 **********************************************************************************************************************/

static void
Xacc_blend_zero( GenefxState *gfxs )
{
     int                i;
     int                w = gfxs->length;
     GenefxAccumulator *X = gfxs->Xacc;
     GenefxAccumulator *Y = gfxs->Yacc;

     for (i = 0; i < w; ++i) {
          if (!(Y[i].RGB.a & 0xf000))
               X[i].RGB.a = X[i].RGB.r = X[i].RGB.g = X[i].RGB.b = 0;
          else
               X[i] = Y[i];
     }
}

static void
Xacc_blend_one( GenefxState *gfxs )
{
     int                i;
     int                w = gfxs->length;
     GenefxAccumulator *X = gfxs->Xacc;
     GenefxAccumulator *Y = gfxs->Yacc;

     for (i = 0; i < w; ++i) {
          X[i] = Y[i];
     }
}

static void
Xacc_blend_srccolor( GenefxState *gfxs )
{
     int                w = gfxs->length + 1;
     GenefxAccumulator *X = gfxs->Xacc;
     GenefxAccumulator *Y = gfxs->Yacc;

     if (gfxs->Sacc) {
          GenefxAccumulator *S = gfxs->Sacc;

          while (--w) {
               if (!(Y->RGB.a & 0xf000)) {
                    X->RGB.r = ((S->RGB.r + 1) * Y->RGB.r) >> 8;
                    X->RGB.g = ((S->RGB.g + 1) * Y->RGB.g) >> 8;
                    X->RGB.b = ((S->RGB.b + 1) * Y->RGB.b) >> 8;
                    X->RGB.a = ((S->RGB.a + 1) * Y->RGB.a) >> 8;
               }
               else
                    *X = *Y;

               ++S;
               ++X; ++Y;
          }
     }
     else {
          GenefxAccumulator Cacc = gfxs->Cacc;
          Cacc.RGB.r = Cacc.RGB.r + 1;
          Cacc.RGB.g = Cacc.RGB.g + 1;
          Cacc.RGB.b = Cacc.RGB.b + 1;
          Cacc.RGB.a = Cacc.RGB.a + 1;

          while (--w) {
               if (!(Y->RGB.a & 0xf000)) {
                    X->RGB.r = (Cacc.RGB.r * Y->RGB.r) >> 8;
                    X->RGB.g = (Cacc.RGB.g * Y->RGB.g) >> 8;
                    X->RGB.b = (Cacc.RGB.b * Y->RGB.b) >> 8;
                    X->RGB.a = (Cacc.RGB.a * Y->RGB.a) >> 8;
               }
               else
                    *X = *Y;

               ++X; ++Y;
          }
     }
}

static void
Xacc_blend_invsrccolor( GenefxState *gfxs )
{
     int                w = gfxs->length + 1;
     GenefxAccumulator *X = gfxs->Xacc;
     GenefxAccumulator *Y = gfxs->Yacc;

     if (gfxs->Sacc) {
          GenefxAccumulator *S = gfxs->Sacc;

          while (--w) {
               if (!(Y->RGB.a & 0xf000)) {
                    X->RGB.r = ((0x100 - S->RGB.r) * Y->RGB.r) >> 8;
                    X->RGB.g = ((0x100 - S->RGB.g) * Y->RGB.g) >> 8;
                    X->RGB.b = ((0x100 - S->RGB.b) * Y->RGB.b) >> 8;
                    X->RGB.a = ((0x100 - S->RGB.a) * Y->RGB.a) >> 8;
               }
               else
                    *X = *Y;

               ++S;
               ++X; ++Y;
          }
     }
     else {
          GenefxAccumulator Cacc = gfxs->Cacc;
          Cacc.RGB.r = 0x100 - Cacc.RGB.r;
          Cacc.RGB.g = 0x100 - Cacc.RGB.g;
          Cacc.RGB.b = 0x100 - Cacc.RGB.b;
          Cacc.RGB.a = 0x100 - Cacc.RGB.a;

          while (--w) {
               if (!(Y->RGB.a & 0xf000)) {
                    X->RGB.r = (Cacc.RGB.r * Y->RGB.r) >> 8;
                    X->RGB.g = (Cacc.RGB.g * Y->RGB.g) >> 8;
                    X->RGB.b = (Cacc.RGB.b * Y->RGB.b) >> 8;
                    X->RGB.a = (Cacc.RGB.a * Y->RGB.a) >> 8;
               }
               else
                    *X = *Y;

               ++X; ++Y;
          }
     }
}

static void
Xacc_blend_srcalpha( GenefxState *gfxs )
{
     int                w = gfxs->length + 1;
     GenefxAccumulator *X = gfxs->Xacc;
     GenefxAccumulator *Y = gfxs->Yacc;

     if (gfxs->Sacc) {
          GenefxAccumulator *S = gfxs->Sacc;

          while (--w) {
               if (!(Y->RGB.a & 0xf000)) {
                    u16 Sa = S->RGB.a + 1;

                    X->RGB.r = (Sa * Y->RGB.r) >> 8;
                    X->RGB.g = (Sa * Y->RGB.g) >> 8;
                    X->RGB.b = (Sa * Y->RGB.b) >> 8;
                    X->RGB.a = (Sa * Y->RGB.a) >> 8;
               }
               else
                    *X = *Y;

               ++S;
               ++X; ++Y;
          }
     }
     else {
          u16 Sa = gfxs->color.a + 1;

          while (--w) {
               if (!(Y->RGB.a & 0xf000)) {
                    X->RGB.r = (Sa * Y->RGB.r) >> 8;
                    X->RGB.g = (Sa * Y->RGB.g) >> 8;
                    X->RGB.b = (Sa * Y->RGB.b) >> 8;
                    X->RGB.a = (Sa * Y->RGB.a) >> 8;
               }
               else
                    *X = *Y;

               ++X; ++Y;
          }
     }
}

static void
Xacc_blend_invsrcalpha( GenefxState *gfxs )
{
     int                w = gfxs->length + 1;
     GenefxAccumulator *X = gfxs->Xacc;
     GenefxAccumulator *Y = gfxs->Yacc;

     if (gfxs->Sacc) {
          GenefxAccumulator *S = gfxs->Sacc;

          while (--w) {
               if (!(Y->RGB.a & 0xf000)) {
                    u16 Sa = 0x100 - S->RGB.a;

                    X->RGB.r = (Sa * Y->RGB.r) >> 8;
                    X->RGB.g = (Sa * Y->RGB.g) >> 8;
                    X->RGB.b = (Sa * Y->RGB.b) >> 8;
                    X->RGB.a = (Sa * Y->RGB.a) >> 8;
               }
               else
                    *X = *Y;

               ++S;
               ++X; ++Y;
          }
     }
     else {
          u16 Sa = 0x100 - gfxs->color.a;

          while (--w) {
               if (!(Y->RGB.a & 0xf000)) {
                    X->RGB.a = (Sa * Y->RGB.a) >> 8;
                    X->RGB.r = (Sa * Y->RGB.r) >> 8;
                    X->RGB.g = (Sa * Y->RGB.g) >> 8;
                    X->RGB.b = (Sa * Y->RGB.b) >> 8;
               }
               else
                    *X = *Y;

               ++X; ++Y;
          }
     }
}

static void
Xacc_blend_dstalpha( GenefxState *gfxs )
{
     int                w = gfxs->length + 1;
     GenefxAccumulator *D = gfxs->Dacc;
     GenefxAccumulator *X = gfxs->Xacc;
     GenefxAccumulator *Y = gfxs->Yacc;

     while (--w) {
          if (!(Y->RGB.a & 0xf000)) {
               u16 Da = D->RGB.a + 1;

               X->RGB.r = (Da * Y->RGB.r) >> 8;
               X->RGB.g = (Da * Y->RGB.g) >> 8;
               X->RGB.b = (Da * Y->RGB.b) >> 8;
               X->RGB.a = (Da * Y->RGB.a) >> 8;
          }
          else
               *X = *Y;

          ++D;
          ++X; ++Y;
     }
}

static void
Xacc_blend_invdstalpha( GenefxState *gfxs )
{
     int                w = gfxs->length + 1;
     GenefxAccumulator *D = gfxs->Dacc;
     GenefxAccumulator *X = gfxs->Xacc;
     GenefxAccumulator *Y = gfxs->Yacc;

     while (--w) {
          if (!(Y->RGB.a & 0xf000)) {
               u16 Da = 0x100 - D->RGB.a;

               X->RGB.r = (Da * Y->RGB.r) >> 8;
               X->RGB.g = (Da * Y->RGB.g) >> 8;
               X->RGB.b = (Da * Y->RGB.b) >> 8;
               X->RGB.a = (Da * Y->RGB.a) >> 8;
          }
          else
               *X = *Y;

          ++D;
          ++X; ++Y;
     }
}

static void
Xacc_blend_destcolor( GenefxState *gfxs )
{
     int                w = gfxs->length + 1;
     GenefxAccumulator *D = gfxs->Dacc;
     GenefxAccumulator *X = gfxs->Xacc;
     GenefxAccumulator *Y = gfxs->Yacc;

     while (--w) {
          if (!(Y->RGB.a & 0xf000)) {
               X->RGB.r = ((D->RGB.r + 1) * Y->RGB.r) >> 8;
               X->RGB.g = ((D->RGB.g + 1) * Y->RGB.g) >> 8;
               X->RGB.b = ((D->RGB.b + 1) * Y->RGB.b) >> 8;
               X->RGB.a = ((D->RGB.a + 1) * Y->RGB.a) >> 8;
          }
          else
               *X = *Y;

          ++D;
          ++X; ++Y;
     }
}

static void
Xacc_blend_invdestcolor( GenefxState *gfxs )
{
     int                w = gfxs->length + 1;
     GenefxAccumulator *D = gfxs->Dacc;
     GenefxAccumulator *X = gfxs->Xacc;
     GenefxAccumulator *Y = gfxs->Yacc;

     while (--w) {
          if (!(Y->RGB.a & 0xf000)) {
               X->RGB.r = ((0x100 - D->RGB.r) * Y->RGB.r) >> 8;
               X->RGB.g = ((0x100 - D->RGB.g) * Y->RGB.g) >> 8;
               X->RGB.b = ((0x100 - D->RGB.b) * Y->RGB.b) >> 8;
               X->RGB.a = ((0x100 - D->RGB.a) * Y->RGB.a) >> 8;
          }
          else
               *X = *Y;

          ++D;
          ++X; ++Y;
     }
}

static void
Xacc_blend_srcalphasat( GenefxState *gfxs )
{
     int                w = gfxs->length + 1;
     GenefxAccumulator *X = gfxs->Xacc;
     GenefxAccumulator *Y = gfxs->Yacc;
     GenefxAccumulator *D = gfxs->Dacc;

     if (gfxs->Sacc) {
          GenefxAccumulator *S = gfxs->Sacc;

          while (--w) {
               if (!(Y->RGB.a & 0xf000)) {
                    u16 Sa = MIN( S->RGB.a + 1, 0x100 - D->RGB.a );

                    X->RGB.a =       Y->RGB.a;
                    X->RGB.r = (Sa * Y->RGB.r) >> 8;
                    X->RGB.g = (Sa * Y->RGB.g) >> 8;
                    X->RGB.b = (Sa * Y->RGB.b) >> 8;
               }
               else
                    *X = *Y;

               ++S;
               ++D;
               ++X; ++Y;
          }
     }
     else {
          while (--w) {
               if (!(Y->RGB.a & 0xf000)) {
                    u16 Sa = MIN( gfxs->color.a + 1, 0x100 - D->RGB.a );

                    X->RGB.a =       Y->RGB.a;
                    X->RGB.r = (Sa * Y->RGB.r) >> 8;
                    X->RGB.g = (Sa * Y->RGB.g) >> 8;
                    X->RGB.b = (Sa * Y->RGB.b) >> 8;
               }
               else
                    *X = *Y;

               ++D;
               ++X; ++Y;
          }
     }
}

static GenefxFunc Xacc_blend[] = {
     [DSBF_ZERO-1]         = Xacc_blend_zero,
     [DSBF_ONE-1]          = Xacc_blend_one,
     [DSBF_SRCCOLOR-1]     = Xacc_blend_srccolor,
     [DSBF_INVSRCCOLOR-1]  = Xacc_blend_invsrccolor,
     [DSBF_SRCALPHA-1]     = Xacc_blend_srcalpha,
     [DSBF_INVSRCALPHA-1]  = Xacc_blend_invsrcalpha,
     [DSBF_DESTALPHA-1]    = Xacc_blend_dstalpha,
     [DSBF_INVDESTALPHA-1] = Xacc_blend_invdstalpha,
     [DSBF_DESTCOLOR-1]    = Xacc_blend_destcolor,
     [DSBF_INVDESTCOLOR-1] = Xacc_blend_invdestcolor,
     [DSBF_SRCALPHASAT-1]  = Xacc_blend_srcalphasat,
};

/**********************************************************************************************************************
 ********************************* Dacc_modulation ********************************************************************
 **********************************************************************************************************************/

static void
Dacc_set_alpha( GenefxState *gfxs )
{
     int                w = gfxs->length + 1;
     GenefxAccumulator *D = gfxs->Dacc;
     int                a = gfxs->color.a;

     while (--w) {
          if (!(D->RGB.a & 0xf000)) {
               D->RGB.a = a;
          }

          ++D;
     }
}

static void
Dacc_modulate_alpha( GenefxState *gfxs )
{
     int                w = gfxs->length + 1;
     GenefxAccumulator *D = gfxs->Dacc;
     int                a = gfxs->Cacc.RGB.a;

     while (--w) {
          if (!(D->RGB.a & 0xf000)) {
               D->RGB.a = (a * D->RGB.a) >> 8;
          }

          ++D;
     }
}

static void
Dacc_modulate_rgb( GenefxState *gfxs )
{
     int                w    = gfxs->length + 1;
     GenefxAccumulator *D    = gfxs->Dacc;
     GenefxAccumulator  Cacc = gfxs->Cacc;

     while (--w) {
          if (!(D->RGB.a & 0xf000)) {
               D->RGB.r = (Cacc.RGB.r * D->RGB.r) >> 8;
               D->RGB.g = (Cacc.RGB.g * D->RGB.g) >> 8;
               D->RGB.b = (Cacc.RGB.b * D->RGB.b) >> 8;
          }

          ++D;
     }
}

static void
Dacc_modulate_rgb_set_alpha( GenefxState *gfxs )
{
     int                w    = gfxs->length + 1;
     GenefxAccumulator *D    = gfxs->Dacc;
     GenefxAccumulator  Cacc = gfxs->Cacc;
     int                a    = gfxs->color.a;

     while (--w) {
          if (!(D->RGB.a & 0xf000)) {
               D->RGB.a = a;
               D->RGB.r = (Cacc.RGB.r * D->RGB.r) >> 8;
               D->RGB.g = (Cacc.RGB.g * D->RGB.g) >> 8;
               D->RGB.b = (Cacc.RGB.b * D->RGB.b) >> 8;
          }

          ++D;
     }
}

static void
Dacc_modulate_argb( GenefxState *gfxs )
{
     int                w    = gfxs->length + 1;
     GenefxAccumulator *D    = gfxs->Dacc;
     GenefxAccumulator  Cacc = gfxs->Cacc;

     while (--w) {
          if (!(D->RGB.a & 0xf000)) {
               D->RGB.a = (Cacc.RGB.a * D->RGB.a) >> 8;
               D->RGB.r = (Cacc.RGB.r * D->RGB.r) >> 8;
               D->RGB.g = (Cacc.RGB.g * D->RGB.g) >> 8;
               D->RGB.b = (Cacc.RGB.b * D->RGB.b) >> 8;
          }

          ++D;
     }
}

static GenefxFunc Dacc_modulation[] = {
     [DSBLIT_NOFX]                                                           = NULL,
     [DSBLIT_BLEND_ALPHACHANNEL]                                             = NULL,
     [DSBLIT_BLEND_COLORALPHA]                                               = Dacc_set_alpha,
     [DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA]                   = Dacc_modulate_alpha,
     [DSBLIT_COLORIZE]                                                       = Dacc_modulate_rgb,
     [DSBLIT_COLORIZE | DSBLIT_BLEND_ALPHACHANNEL]                           = Dacc_modulate_rgb,
     [DSBLIT_COLORIZE | DSBLIT_BLEND_COLORALPHA]                             = Dacc_modulate_rgb_set_alpha,
     [DSBLIT_COLORIZE | DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA] = Dacc_modulate_argb
};

/**********************************************************************************************************************
 ********************************* Dacc_modulate_mask_alpha_from_PFI **************************************************
 **********************************************************************************************************************/

static void
Dacc_modulate_mask_alpha_ARGB( GenefxState *gfxs )
{
     int                w = gfxs->length + 1;
     GenefxAccumulator *D = gfxs->Dacc;
     u32               *m = gfxs->Mop[0];

     while (--w) {
          if (!(D->RGB.a & 0xf000)) {
               D->RGB.a = (((*m >> 24) + 1) * D->RGB.a) >> 8;
          }

          ++D;
          ++m;
     }
}

static void
Dacc_modulate_mask_alpha_A8( GenefxState *gfxs )
{
     int                w = gfxs->length + 1;
     GenefxAccumulator *D = gfxs->Dacc;
     u8                *m = gfxs->Mop[0];

     while (--w) {
          if (!(D->RGB.a & 0xf000)) {
               D->RGB.a = ((*m + 1) * D->RGB.a) >> 8;
          }

          ++D;
          ++m;
     }
}

static GenefxFunc Dacc_modulate_mask_alpha_from_PFI[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1555)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB16)]      = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB24)]      = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB32)]      = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]       = Dacc_modulate_mask_alpha_ARGB,
     [DFB_PIXELFORMAT_INDEX(DSPF_A8)]         = Dacc_modulate_mask_alpha_A8,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUY2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB332)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_UYVY)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_I420)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT8)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ALUT44)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)]      = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB2554)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA4444)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV21)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AYUV)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A4)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1666)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB6666)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB18)]      = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB444)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB555)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR555)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA5551)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y444)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB8565)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AVYU)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_VYU)]        = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1_LSB)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ABGR)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBAF88871)] = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT1)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV61)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y42B)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV24)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV24)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV42)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR24)]      = NULL,
};

/**********************************************************************************************************************
 ********************************* Dacc_modulate_mask_rgb_from_PFI ****************************************************
 **********************************************************************************************************************/

static void
Dacc_modulate_mask_rgb_ARGB( GenefxState *gfxs )
{
     int                w = gfxs->length + 1;
     GenefxAccumulator *D = gfxs->Dacc;
     u32               *m = gfxs->Mop[0];

     while (--w) {
          if (!(D->RGB.a & 0xf000)) {
               D->RGB.r = ((((*m >> 16) & 0xff) + 1) * D->RGB.r) >> 8;
               D->RGB.g = ((((*m >>  8) & 0xff) + 1) * D->RGB.g) >> 8;
               D->RGB.b = ((( *m        & 0xff) + 1) * D->RGB.b) >> 8;
          }

          ++D;
          ++m;
     }
}

static GenefxFunc Dacc_modulate_mask_rgb_from_PFI[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1555)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB16)]      = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB24)]      = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB32)]      = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]       = Dacc_modulate_mask_rgb_ARGB,
     [DFB_PIXELFORMAT_INDEX(DSPF_A8)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUY2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB332)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_UYVY)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_I420)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT8)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ALUT44)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)]      = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB2554)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA4444)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV21)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AYUV)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A4)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1666)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB6666)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB18)]      = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB444)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB555)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR555)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA5551)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y444)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB8565)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AVYU)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_VYU)]        = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1_LSB)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ABGR)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBAF88871)] = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT1)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV61)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y42B)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV24)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV24)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV42)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR24)]      = NULL,
};

/**********************************************************************************************************************
 ********************************* Dacc_modulate_mask_argb_from_PFI ***************************************************
 **********************************************************************************************************************/

static void
Dacc_modulate_mask_argb_ARGB( GenefxState *gfxs )
{
     int                w = gfxs->length + 1;
     GenefxAccumulator *D = gfxs->Dacc;
     u8                *m = gfxs->Mop[0];

     while (--w) {
          if (!(D->RGB.a & 0xf000)) {
               D->RGB.a = (( (*m >> 24)         + 1) * D->RGB.a) >> 8;
               D->RGB.r = ((((*m >> 16) & 0xff) + 1) * D->RGB.r) >> 8;
               D->RGB.g = ((((*m >>  8) & 0xff) + 1) * D->RGB.g) >> 8;
               D->RGB.b = ((((*m      ) & 0xff) + 1) * D->RGB.b) >> 8;
          }

          ++D;
          ++m;
     }
}

static GenefxFunc Dacc_modulate_mask_argb_from_PFI[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1555)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB16)]      = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB24)]      = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB32)]      = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]       = Dacc_modulate_mask_argb_ARGB,
     [DFB_PIXELFORMAT_INDEX(DSPF_A8)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUY2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB332)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_UYVY)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_I420)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT8)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ALUT44)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)]      = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV12)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB2554)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA4444)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV21)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AYUV)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A4)]         = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1666)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB6666)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB18)]      = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT2)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB444)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB555)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR555)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA5551)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y444)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB8565)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AVYU)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_VYU)]        = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1_LSB)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV16)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ABGR)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBAF88871)] = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT1)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV61)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_Y42B)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV24)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV24)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV42)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR24)]      = NULL,
};

/**********************************************************************************************************************
 ********************************* Misc accumulator operations ********************************************************
 **********************************************************************************************************************/

static void
Dacc_premultiply_C( GenefxState *gfxs )
{
     int                w = gfxs->length + 1;
     GenefxAccumulator *D = gfxs->Dacc;

     while (--w) {
          if (!(D->RGB.a & 0xf000)) {
               u16 Da = D->RGB.a + 1;

               D->RGB.r = (Da * D->RGB.r) >> 8;
               D->RGB.g = (Da * D->RGB.g) >> 8;
               D->RGB.b = (Da * D->RGB.b) >> 8;
          }

          ++D;
     }
}

static GenefxFunc Dacc_premultiply = Dacc_premultiply_C;

/**********************************************************************************************************************/

static void
Dacc_premultiply_color_alpha_C( GenefxState *gfxs )
{
     int                w  = gfxs->length + 1;
     GenefxAccumulator *D  = gfxs->Dacc;
     u16                Ca = gfxs->Cacc.RGB.a;

     while (--w) {
          if (!(D->RGB.a & 0xf000)) {
               D->RGB.r = (Ca * D->RGB.r) >> 8;
               D->RGB.g = (Ca * D->RGB.g) >> 8;
               D->RGB.b = (Ca * D->RGB.b) >> 8;
          }

          ++D;
     }
}

static GenefxFunc Dacc_premultiply_color_alpha = Dacc_premultiply_color_alpha_C;

/**********************************************************************************************************************/

static void
Dacc_demultiply_C( GenefxState *gfxs )
{
     int                w = gfxs->length + 1;
     GenefxAccumulator *D = gfxs->Dacc;

     while (--w) {
          if (!(D->RGB.a & 0xf000)) {
               u16 Da = D->RGB.a + 1;

               D->RGB.r = (D->RGB.r << 8) / Da;
               D->RGB.g = (D->RGB.g << 8) / Da;
               D->RGB.b = (D->RGB.b << 8) / Da;
          }

          ++D;
     }
}

static GenefxFunc Dacc_demultiply = Dacc_demultiply_C;

/**********************************************************************************************************************/

static void
Dacc_xor_C( GenefxState *gfxs )
{
     int                w     = gfxs->length + 1;
     GenefxAccumulator *D     = gfxs->Dacc;
     DFBColor           color = gfxs->color;

     while (--w) {
          if (!(D->RGB.a & 0xf000)) {
               D->RGB.a ^= color.a;
               D->RGB.r ^= color.r;
               D->RGB.g ^= color.g;
               D->RGB.b ^= color.b;
          }

          ++D;
     }
}

static GenefxFunc Dacc_xor = Dacc_xor_C;

/**********************************************************************************************************************/

static void
Dacc_clamp_C( GenefxState *gfxs )
{
     int                w = gfxs->length + 1;
     GenefxAccumulator *D = gfxs->Dacc;

     while (--w) {
          if (!(D->RGB.a & 0xf000)) {
               if (D->RGB.a > 0xff)
                    D->RGB.a = 0xff;
               if (D->RGB.r > 0xff)
                    D->RGB.r = 0xff;
               if (D->RGB.g > 0xff)
                    D->RGB.g = 0xff;
               if (D->RGB.b > 0xff)
                    D->RGB.b = 0xff;
          }

          ++D;
     }
}

static GenefxFunc Dacc_clamp = Dacc_clamp_C;

/**********************************************************************************************************************/

static void
Sacc_xor_Dacc_C( GenefxState *gfxs )
{
     int                w = gfxs->length + 1;
     GenefxAccumulator *S = gfxs->Sacc;
     GenefxAccumulator *D = gfxs->Dacc;

     while (--w) {
          if (!(D->RGB.a & 0xf000)) {
               D->RGB.a ^= S->RGB.a;
               D->RGB.r ^= S->RGB.r;
               D->RGB.g ^= S->RGB.g;
               D->RGB.b ^= S->RGB.b;
          }

          ++S;
          ++D;
     }
}

static GenefxFunc Sacc_xor_Dacc = Sacc_xor_Dacc_C;

/**********************************************************************************************************************/

static void
Cacc_to_Dacc_C( GenefxState *gfxs )
{
     int                w    = gfxs->length + 1;
     GenefxAccumulator *D    = gfxs->Dacc;
     GenefxAccumulator  Cacc = gfxs->Cacc;

     while (--w)
          *D++ = Cacc;
}

static GenefxFunc Cacc_to_Dacc = Cacc_to_Dacc_C;

/**********************************************************************************************************************/

static void
SCacc_add_to_Dacc_C( GenefxState *gfxs )
{
     int                w     = gfxs->length + 1;
     GenefxAccumulator *D     = gfxs->Dacc;
     GenefxAccumulator  SCacc = gfxs->SCacc;

     while (--w) {
          if (!(D->RGB.a & 0xf000)) {
               D->RGB.a += SCacc.RGB.a;
               D->RGB.r += SCacc.RGB.r;
               D->RGB.g += SCacc.RGB.g;
               D->RGB.b += SCacc.RGB.b;
          }

          ++D;
     }
}

static GenefxFunc SCacc_add_to_Dacc = SCacc_add_to_Dacc_C;

/**********************************************************************************************************************/

static void
Sacc_add_to_Dacc_C( GenefxState *gfxs )
{
     int                w = gfxs->length + 1;
     GenefxAccumulator *S = gfxs->Sacc;
     GenefxAccumulator *D = gfxs->Dacc;

     while (--w) {
          if (!(D->RGB.a & 0xf000)) {
               D->RGB.a += S->RGB.a;
               D->RGB.r += S->RGB.r;
               D->RGB.g += S->RGB.g;
               D->RGB.b += S->RGB.b;
          }

          ++S;
          ++D;
     }
}

static GenefxFunc Sacc_add_to_Dacc = Sacc_add_to_Dacc_C;

/**********************************************************************************************************************/

/* change the last value to adjust the size of the device (1-4) */
#define SET_PIXEL_DUFFS_DEVICE(D,S,w) \
     SET_PIXEL_DUFFS_DEVICE_N( D, S, w, 3 )

static void
Dacc_RGB_to_YCbCr_BT601_C( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *S = gfxs->Dacc;
     GenefxAccumulator *D = gfxs->Dacc;

#define SET_PIXEL(d,s)                                                                \
     if (!(s.RGB.a & 0xf000)) {                                                       \
          RGB_TO_YCBCR_BT601( s.RGB.r, s.RGB.g, s.RGB.b, d.YUV.y, d.YUV.u, d.YUV.v ); \
     }

     SET_PIXEL_DUFFS_DEVICE( D, S, w );

#undef SET_PIXEL
}

static void
Dacc_RGB_to_YCbCr_BT709_C( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *S = gfxs->Dacc;
     GenefxAccumulator *D = gfxs->Dacc;

#define SET_PIXEL(d,s)                                                                \
     if (!(s.RGB.a & 0xf000)) {                                                       \
          RGB_TO_YCBCR_BT709( s.RGB.r, s.RGB.g, s.RGB.b, d.YUV.y, d.YUV.u, d.YUV.v ); \
     }

     SET_PIXEL_DUFFS_DEVICE( D, S, w );

#undef SET_PIXEL
}

static void
Dacc_RGB_to_YCbCr_BT2020_C( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *S = gfxs->Dacc;
     GenefxAccumulator *D = gfxs->Dacc;

#define SET_PIXEL(d,s)                                                                 \
     if (!(s.RGB.a & 0xf000)) {                                                        \
          RGB_TO_YCBCR_BT2020( s.RGB.r, s.RGB.g, s.RGB.b, d.YUV.y, d.YUV.u, d.YUV.v ); \
     }

     SET_PIXEL_DUFFS_DEVICE( D, S, w );

#undef SET_PIXEL
}

#undef SET_PIXEL_DUFFS_DEVICE

static GenefxFunc Dacc_RGB_to_YCbCr_BT601  = Dacc_RGB_to_YCbCr_BT601_C;
static GenefxFunc Dacc_RGB_to_YCbCr_BT709  = Dacc_RGB_to_YCbCr_BT709_C;
static GenefxFunc Dacc_RGB_to_YCbCr_BT2020 = Dacc_RGB_to_YCbCr_BT2020_C;

/**********************************************************************************************************************/

/* change the last value to adjust the size of the device (1-4) */
#define SET_PIXEL_DUFFS_DEVICE(D,S,w) \
     SET_PIXEL_DUFFS_DEVICE_N( D, S, w, 2 )

static void
Dacc_YCbCr_to_RGB_BT601_C( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *S = gfxs->Dacc;
     GenefxAccumulator *D = gfxs->Dacc;

#define SET_PIXEL(d,s)                                                                \
     if (!(s.YUV.a & 0xf000)) {                                                       \
          YCBCR_TO_RGB_BT601( s.YUV.y, s.YUV.u, s.YUV.v, d.RGB.r, d.RGB.g, d.RGB.b ); \
     }

     SET_PIXEL_DUFFS_DEVICE( D, S, w );

#undef SET_PIXEL
}

static void
Dacc_YCbCr_to_RGB_BT709_C( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *S = gfxs->Dacc;
     GenefxAccumulator *D = gfxs->Dacc;

#define SET_PIXEL(d,s)                                                                \
     if (!(s.YUV.a & 0xf000)) {                                                       \
          YCBCR_TO_RGB_BT709( s.YUV.y, s.YUV.u, s.YUV.v, d.RGB.r, d.RGB.g, d.RGB.b ); \
     }

     SET_PIXEL_DUFFS_DEVICE( D, S, w );

#undef SET_PIXEL
}

static void
Dacc_YCbCr_to_RGB_BT2020_C( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *S = gfxs->Dacc;
     GenefxAccumulator *D = gfxs->Dacc;

#define SET_PIXEL(d,s)                                                                 \
     if (!(s.YUV.a & 0xf000)) {                                                        \
          YCBCR_TO_RGB_BT2020( s.YUV.y, s.YUV.u, s.YUV.v, d.RGB.r, d.RGB.g, d.RGB.b ); \
     }

     SET_PIXEL_DUFFS_DEVICE( D, S, w );

#undef SET_PIXEL
}

#undef SET_PIXEL_DUFFS_DEVICE

static GenefxFunc Dacc_YCbCr_to_RGB_BT601  = Dacc_YCbCr_to_RGB_BT601_C;
static GenefxFunc Dacc_YCbCr_to_RGB_BT709  = Dacc_YCbCr_to_RGB_BT709_C;
static GenefxFunc Dacc_YCbCr_to_RGB_BT2020 = Dacc_YCbCr_to_RGB_BT2020_C;

/**********************************************************************************************************************/

static void
Dacc_Alpha_to_YCbCr_C( GenefxState *gfxs )
{
     int                w = gfxs->length + 1;
     GenefxAccumulator *D = gfxs->Dacc;

     while (--w) {
          if (!(D->RGB.a & 0xf000)) {
               D->YUV.y = 235;
               D->YUV.u = 128;
               D->YUV.v = 128;
          }

          ++D;
     }
}

static GenefxFunc Dacc_Alpha_to_YCbCr = Dacc_Alpha_to_YCbCr_C;

/**********************************************************************************************************************/

static void Sop_is_Aop  ( GenefxState *gfxs ) { gfxs->Sop = gfxs->Aop; gfxs->Ostep = gfxs->Astep; }
static void Sop_is_Bop  ( GenefxState *gfxs ) { gfxs->Sop = gfxs->Bop; gfxs->Ostep = gfxs->Bstep; }

static void Slut_is_Alut( GenefxState *gfxs ) { gfxs->Slut = gfxs->Alut; }
static void Slut_is_Blut( GenefxState *gfxs ) { gfxs->Slut = gfxs->Blut; }

static void Sacc_is_NULL( GenefxState *gfxs ) { gfxs->Sacc = NULL; }
static void Sacc_is_Aacc( GenefxState *gfxs ) { gfxs->Sacc = gfxs->Aacc; }
static void Sacc_is_Bacc( GenefxState *gfxs ) { gfxs->Sacc = gfxs->Bacc; }
static void Sacc_is_Tacc( GenefxState *gfxs ) { gfxs->Sacc = gfxs->Tacc; }

static void Dacc_is_Aacc( GenefxState *gfxs ) { gfxs->Dacc = gfxs->Aacc; }
static void Dacc_is_Bacc( GenefxState *gfxs ) { gfxs->Dacc = gfxs->Bacc; }
static void Dacc_is_Tacc( GenefxState *gfxs ) { gfxs->Dacc = gfxs->Tacc; }

static void Xacc_is_Bacc( GenefxState *gfxs ) { gfxs->Xacc = gfxs->Bacc; }
static void Xacc_is_Tacc( GenefxState *gfxs ) { gfxs->Xacc = gfxs->Tacc; }

static void Yacc_is_Aacc( GenefxState *gfxs ) { gfxs->Yacc = gfxs->Aacc; }
static void Yacc_is_Bacc( GenefxState *gfxs ) { gfxs->Yacc = gfxs->Bacc; }

static void Len_is_Slen ( GenefxState *gfxs ) { gfxs->length = gfxs->Slen; }
static void Len_is_Dlen ( GenefxState *gfxs ) { gfxs->length = gfxs->Dlen; }

#define MODULATION_FLAGS (DSBLIT_BLEND_ALPHACHANNEL | \
                          DSBLIT_BLEND_COLORALPHA   | \
                          DSBLIT_COLORIZE           | \
                          DSBLIT_DST_PREMULTIPLY    | \
                          DSBLIT_SRC_PREMULTIPLY    | \
                          DSBLIT_SRC_PREMULTCOLOR   | \
                          DSBLIT_DEMULTIPLY         | \
                          DSBLIT_XOR)

/**********************************************************************************************************************/

#ifndef WORDS_BIGENDIAN

#define BGR_TO_RGB16(pixel) ((((pixel) <<  8) & 0xf800) | (((pixel) >>  5) & 0x07e0) | (((pixel) >> 19) & 0x001f))

/*
 * Fast RGB24 to RGB16 conversion.
 */
static void
Bop_rgb24_to_Aop_rgb16_LE( GenefxState *gfxs )
{
     int  w = gfxs->length;
     u8  *S = gfxs->Bop[0];
     u16 *D = gfxs->Aop[0];

     while ((unsigned long) S & 3) {
          *D++ = PIXEL_RGB16( S[0], S[1], S[2] );

          S += 3;
          --w;
     }

     if ((unsigned long) D & 2) {
          *D++ = PIXEL_RGB16( S[0], S[1], S[2] );

          S += 3;
          --w;

          while (w > 1) {
               *(u32*) D = PIXEL_RGB16( S[0], S[1], S[2] ) | (PIXEL_RGB16( S[3], S[4], S[5] ) << 16);

               S += 6;
               D += 2;
               w -= 2;
          }
     }
     else {
          u32 *S32 = (u32*) S;
          u32 *D32 = (u32*) D;

          while (w > 3) {
               D32[0] = BGR_TO_RGB16( S32[0] ) | (BGR_TO_RGB16( (S32[0] >> 24) | (S32[1] << 8) ) << 16);
               D32[1] = BGR_TO_RGB16( (S32[1] >> 16) | (S32[2] << 16) ) | (BGR_TO_RGB16( S32[2] >> 8 ) << 16);

               S32 += 3;
               D32 += 2;
               w   -= 4;
          }

          S = (u8*)  S32;
          D = (u16*) D32;
     }

     while (w > 0) {
          *D++ = PIXEL_RGB16( S[0], S[1], S[2] );

          S += 3;
          --w;
     }
}

/*
 * Fast RGB32 to RGB16 conversion.
 */
static void
Bop_rgb32_to_Aop_rgb16_LE( GenefxState *gfxs )
{
     int  w = gfxs->length;
     u32 *S = gfxs->Bop[0];
     u32 *D = gfxs->Aop[0];

     if ((unsigned long) D & 2) {
          u16 *d = (u16*) D;

          d[0] = ARGB_TO_RGB16( S[0] );
          D = (u32*) (d + 1);

          ++S;
          --w;
     }

     while (w > 1) {
          D[0] = ARGB_TO_RGB16( S[0] ) | (ARGB_TO_RGB16( S[1] ) << 16);

          S += 2;
          ++D;
          w -= 2;
     }

     if (w > 0) {
          u16 *d = (u16*) D;

          d[0] = ARGB_TO_RGB16( S[0] );
     }
}

#endif  /* #ifndef WORDS_BIGENDIAN */

/**********************************************************************************************************************/

static int use_mmx = 0;

#ifdef USE_MMX

#include "generic_mmx.h"

/*
 * patches function pointers to MMX functions
 */
static void
gInit_MMX()
{
     use_mmx = 1;

/********************************* Xacc_blend *********************************/
     Xacc_blend[DSBF_SRCALPHA-1]    = Xacc_blend_srcalpha_MMX;
     Xacc_blend[DSBF_INVSRCALPHA-1] = Xacc_blend_invsrcalpha_MMX;
/********************************* Dacc_modulation ****************************/
     Dacc_modulation[DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA | DSBLIT_COLORIZE] = Dacc_modulate_argb_MMX;
/********************************* Misc accumulator operations ****************/
     SCacc_add_to_Dacc = SCacc_add_to_Dacc_MMX;
     Sacc_add_to_Dacc  = Sacc_add_to_Dacc_MMX;
}

#endif

static int use_neon = 0;

#ifdef USE_NEON

#include "generic_neon.h"

/*
 * patches function pointers to NEON functions
 */
static void
gInit_NEON()
{
     use_neon = 1;
/********************************* Sop_PFI_to_Dacc ****************************/
     Sop_PFI_to_Dacc[DFB_PIXELFORMAT_INDEX(DSPF_RGB16)] = Sop_rgb16_to_Dacc_NEON;
     Sop_PFI_to_Dacc[DFB_PIXELFORMAT_INDEX(DSPF_ARGB )] = Sop_argb_to_Dacc_NEON;
/********************************* Sacc_to_Aop_PFI ****************************/
     Sacc_to_Aop_PFI[DFB_PIXELFORMAT_INDEX(DSPF_RGB16)] = Sacc_to_Aop_rgb16_NEON;
/********************************* misc accumulator operations ****************/
     SCacc_add_to_Dacc = SCacc_add_to_Dacc_NEON;
     Sacc_add_to_Dacc = Sacc_add_to_Dacc_NEON;
/********************************* Xacc_blend *********************************/
     Xacc_blend[DSBF_INVSRCALPHA-1] = Xacc_blend_invsrcalpha_NEON;
     Xacc_blend[DSBF_SRCALPHA-1] = Xacc_blend_srcalpha_NEON;
/********************************* Dacc_modulation ****************************/
     Dacc_modulation[DSBLIT_BLEND_ALPHACHANNEL |
                     DSBLIT_BLEND_COLORALPHA |
                     DSBLIT_COLORIZE] = Dacc_modulate_argb_NEON;
     Dacc_modulation[DSBLIT_COLORIZE] = Dacc_modulate_rgb_NEON;
     Dacc_modulation[DSBLIT_COLORIZE |
                     DSBLIT_BLEND_ALPHACHANNEL] = Dacc_modulate_rgb_NEON;
/********************************* Bopp blend alphachannel ********************/
     Bop_argb_blend_alphachannel_src_invsrc_Aop_PFI[DFB_PIXELFORMAT_INDEX(DSPF_RGB16)] \
                     = Bop_argb_blend_alphachannel_src_invsrc_Aop_rgb16_NEON;
}

#endif

#if SIZEOF_LONG == 8

#include "generic_64.h"

/*
 * patches function pointers to 64 bits functions
 */
static void
gInit_64bit()
{
/********************************* Cop_to_Aop_PFI ********************************/
     Cop_to_Aop_PFI[DFB_PIXELFORMAT_INDEX(DSPF_RGB32)] = Cop_to_Aop_32_64;
     Cop_to_Aop_PFI[DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]  = Cop_to_Aop_32_64;
     Cop_to_Aop_PFI[DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)] = Cop_to_Aop_32_64;
/********************************* Bop_PFI_toK_Aop_PFI ***************************/
     Bop_PFI_toK_Aop_PFI[DFB_PIXELFORMAT_INDEX(DSPF_RGB32)] = Bop_rgb32_toK_Aop_64;
     Bop_PFI_toK_Aop_PFI[DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]  = Bop_rgb32_toK_Aop_64;
     Bop_PFI_toK_Aop_PFI[DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)] = Bop_rgb32_toK_Aop_64;
/********************************* Bop_PFI_Kto_Aop_PFI ***************************/
     Bop_PFI_Kto_Aop_PFI[DFB_PIXELFORMAT_INDEX(DSPF_RGB32)] = Bop_rgb32_Kto_Aop_64;
     Bop_PFI_Kto_Aop_PFI[DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]  = Bop_rgb32_Kto_Aop_64;
     Bop_PFI_Kto_Aop_PFI[DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)] = Bop_rgb32_Kto_Aop_64;
/********************************* Bop_PFI_Sto_Aop_PFI ***************************/
     Bop_PFI_Sto_Aop_PFI[DFB_PIXELFORMAT_INDEX(DSPF_RGB32)] = Bop_32_Sto_Aop_64;
     Bop_PFI_Sto_Aop_PFI[DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]  = Bop_32_Sto_Aop_64;
     Bop_PFI_Sto_Aop_PFI[DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)] = Bop_32_Sto_Aop_64;
/********************************* Misc accumulator operations *******************/
     Dacc_xor = Dacc_xor_64;
}

#endif

#ifdef WORDS_BIGENDIAN

/*
 * patches function pointers to big endian functions
 */
static void
gInit_BigEndian()
{
/********************************* Cop_to_Aop_PFI ********************************/
     Cop_to_Aop_PFI[DFB_PIXELFORMAT_INDEX(DSPF_NV12)] = Cop_to_Aop_nv21;
     Cop_to_Aop_PFI[DFB_PIXELFORMAT_INDEX(DSPF_NV16)] = Cop_to_Aop_nv61;
     Cop_to_Aop_PFI[DFB_PIXELFORMAT_INDEX(DSPF_NV24)] = Cop_to_Aop_nv42;
     Cop_to_Aop_PFI[DFB_PIXELFORMAT_INDEX(DSPF_NV21)] = Cop_to_Aop_nv12;
     Cop_to_Aop_PFI[DFB_PIXELFORMAT_INDEX(DSPF_NV61)] = Cop_to_Aop_nv16;
     Cop_to_Aop_PFI[DFB_PIXELFORMAT_INDEX(DSPF_NV42)] = Cop_to_Aop_nv24;
/********************************* Sop_PFI_to_Dacc *******************************/
     Sop_PFI_to_Dacc[DFB_PIXELFORMAT_INDEX(DSPF_NV12)] = Sop_nv21_to_Dacc;
     Sop_PFI_to_Dacc[DFB_PIXELFORMAT_INDEX(DSPF_NV16)] = Sop_nv21_to_Dacc;
     Sop_PFI_to_Dacc[DFB_PIXELFORMAT_INDEX(DSPF_NV24)] = Sop_nv42_to_Dacc;
     Sop_PFI_to_Dacc[DFB_PIXELFORMAT_INDEX(DSPF_NV21)] = Sop_nv12_to_Dacc;
     Sop_PFI_to_Dacc[DFB_PIXELFORMAT_INDEX(DSPF_NV61)] = Sop_nv12_to_Dacc;
     Sop_PFI_to_Dacc[DFB_PIXELFORMAT_INDEX(DSPF_NV42)] = Sop_nv24_to_Dacc;
/********************************* Sop_PFI_Sto_Dacc ******************************/
     Sop_PFI_Sto_Dacc[DFB_PIXELFORMAT_INDEX(DSPF_NV12)] = Sop_nv21_Sto_Dacc;
     Sop_PFI_Sto_Dacc[DFB_PIXELFORMAT_INDEX(DSPF_NV16)] = Sop_nv21_Sto_Dacc;
     Sop_PFI_Sto_Dacc[DFB_PIXELFORMAT_INDEX(DSPF_NV24)] = Sop_nv42_Sto_Dacc;
     Sop_PFI_Sto_Dacc[DFB_PIXELFORMAT_INDEX(DSPF_NV21)] = Sop_nv12_Sto_Dacc;
     Sop_PFI_Sto_Dacc[DFB_PIXELFORMAT_INDEX(DSPF_NV61)] = Sop_nv12_Sto_Dacc;
     Sop_PFI_Sto_Dacc[DFB_PIXELFORMAT_INDEX(DSPF_NV42)] = Sop_nv24_Sto_Dacc;
/********************************* Sacc_to_Aop_PFI *******************************/
     Sacc_to_Aop_PFI[DFB_PIXELFORMAT_INDEX(DSPF_NV12)] = Sacc_to_Aop_nv21;
     Sacc_to_Aop_PFI[DFB_PIXELFORMAT_INDEX(DSPF_NV16)] = Sacc_to_Aop_nv61;
     Sacc_to_Aop_PFI[DFB_PIXELFORMAT_INDEX(DSPF_NV24)] = Sacc_to_Aop_nv42;
     Sacc_to_Aop_PFI[DFB_PIXELFORMAT_INDEX(DSPF_NV21)] = Sacc_to_Aop_nv12;
     Sacc_to_Aop_PFI[DFB_PIXELFORMAT_INDEX(DSPF_NV61)] = Sacc_to_Aop_nv16;
     Sacc_to_Aop_PFI[DFB_PIXELFORMAT_INDEX(DSPF_NV42)] = Sacc_to_Aop_nv24;
/********************************* Sacc_Sto_Aop_PFI ******************************/
     Sacc_Sto_Aop_PFI[DFB_PIXELFORMAT_INDEX(DSPF_NV12)] = Sacc_Sto_Aop_nv21;
     Sacc_Sto_Aop_PFI[DFB_PIXELFORMAT_INDEX(DSPF_NV16)] = Sacc_Sto_Aop_nv61;
     Sacc_Sto_Aop_PFI[DFB_PIXELFORMAT_INDEX(DSPF_NV24)] = Sacc_Sto_Aop_nv42;
     Sacc_Sto_Aop_PFI[DFB_PIXELFORMAT_INDEX(DSPF_NV21)] = Sacc_Sto_Aop_nv12;
     Sacc_Sto_Aop_PFI[DFB_PIXELFORMAT_INDEX(DSPF_NV61)] = Sacc_Sto_Aop_nv16;
     Sacc_Sto_Aop_PFI[DFB_PIXELFORMAT_INDEX(DSPF_NV42)] = Sacc_Sto_Aop_nv24;
}

#endif

/**********************************************************************************************************************/

void
gGetDriverInfo( GraphicsDriverInfo *driver_info )
{
     snprintf( driver_info->name, DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH, "Software Driver" );

#if SIZEOF_LONG == 8
     gInit_64bit();
#endif

#ifdef WORDS_BIGENDIAN
     gInit_BigEndian();
#endif

#ifdef USE_MMX
     if (!dfb_config->mmx) {
          D_INFO( "DirectFB/Genefx: MMX disabled by option 'no-mmx'\n" );
     }
     else {
          gInit_MMX();

          snprintf( driver_info->name, DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH, "MMX Software Driver" );

          D_INFO( "DirectFB/Genefx: MMX enabled\n" );
     }
#endif

#ifdef USE_NEON
     if (!dfb_config->neon) {
          D_INFO( "DirectFB/Genefx: NEON disabled by option 'no-neon'\n" );
     }
     else {
          gInit_NEON();

          snprintf( driver_info->name, DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH, "NEON Software Driver" );

          D_INFO( "DirectFB/Genefx: NEON enabled\n" );
     }
#endif

     snprintf( driver_info->vendor, DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH, "DirectFB" );

     driver_info->version.major = 0;
     driver_info->version.minor = 7;
}

void
gGetDeviceInfo( GraphicsDeviceInfo *device_info )
{
     snprintf( device_info->name, DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "Software Rasterizer" );

     snprintf( device_info->vendor, DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, use_mmx ? "MMX" : "Generic" );

     device_info->caps.flags    = 0;
     device_info->caps.accel    = DFXL_NONE;
     device_info->caps.blitting = DSBLIT_NOFX;
     device_info->caps.drawing  = DSDRAW_NOFX;
     device_info->caps.clip     = 0;
}

static bool
gAcquireCheck( CardState           *state,
               DFBAccelerationMask  accel )
{
     GenefxState *gfxs;
     CoreSurface *destination = state->destination;
     CoreSurface *source      = state->source;
     CoreSurface *source_mask = state->source_mask;

     if (dfb_config->hardware_only) {
          if (dfb_config->software_warn) {
               if (DFB_BLITTING_FUNCTION( accel ))
                    D_WARN( "ignoring blit (%x) from %s to %s, flags 0x%08x, funcs %u %u", accel,
                            source ? dfb_pixelformat_name( source->config.format ) : "NULL SOURCE",
                            destination ? dfb_pixelformat_name( destination->config.format ) : "NULL DESTINATION",
                            state->blittingflags, state->src_blend, state->dst_blend );
               else
                    D_WARN( "ignoring draw (%x) to %s, flags 0x%08x", accel,
                            destination ? dfb_pixelformat_name( destination->config.format ) : "NULL DESTINATION",
                            state->drawingflags );
          }

          return false;
     }

     if (!state->gfxs) {
          gfxs = D_CALLOC( 1, sizeof(GenefxState) );
          if (!gfxs) {
               D_ERROR( "DirectFB/Genefx: Could not allocate Genefx state!\n" );
               return false;
          }

          state->gfxs = gfxs;
     }

     gfxs = state->gfxs;

     /* Destination may have been destroyed. */
     if (!destination)
          return false;

     /* Destination buffer may have been destroyed (suspended), i.e. by vt-switching. */
     if (destination->num_buffers == 0)
          return false;

     /* Source may have been destroyed. */
     if (DFB_BLITTING_FUNCTION( accel )) {
          if (!source)
               return false;

          if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) {
               if (!source_mask)
                    return false;
          }
     }

     return true;
}

static DFBResult
gAcquireLockBuffers( CardState           *state,
                     DFBAccelerationMask  accel )
{
     DFBResult               ret;
     CoreSurface            *destination = state->destination;
     CoreSurface            *source      = state->source;
     CoreSurface            *source_mask = state->source_mask;
     CoreSurfaceAccessFlags  access      = CSAF_WRITE;

     if (core_dfb->shutdown_running)
          return DFB_DEAD;

     /*
      * Destination setup
      */

     if (DFB_BLITTING_FUNCTION( accel )) {
          if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA   | DSBLIT_DST_COLORKEY))
               access |= CSAF_READ;
     }
     else if (state->drawingflags & (DSDRAW_BLEND | DSDRAW_DST_COLORKEY))
          access |= CSAF_READ;

     /* Lock destination. */
     ret = dfb_surface_lock_buffer2( destination, state->to, destination->flips, state->to_eye, CSAID_CPU, access,
                                     &state->dst );
     if (ret) {
          D_DERROR( ret, "DirectFB/Genefx: Could not lock destination!\n" );
          return ret;
     }

     /*
      * Source setup
      */

     if (DFB_BLITTING_FUNCTION( accel )) {
          /* Lock source. */
          ret = dfb_surface_lock_buffer2( source, state->from, source->flips, state->from_eye, CSAID_CPU, CSAF_READ,
                                          &state->src );
          if (ret) {
               D_DERROR( ret, "DirectFB/Genefx: Could not lock source!\n" );
               dfb_surface_unlock_buffer( destination, &state->dst );
               return ret;
          }

          state->flags |= CSF_SOURCE_LOCKED;

          /*
           * Mask setup
           */

          if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) {
               /* Lock source. */
               ret = dfb_surface_lock_buffer2( source_mask, state->from, source_mask->flips, state->from_eye, CSAID_CPU,
                                               CSAF_READ, &state->src_mask );
               if (ret) {
                    D_DERROR( ret, "DirectFB/Genefx: Could not lock source mask!\n" );
                    dfb_surface_unlock_buffer( destination, &state->dst );

                    if (state->flags & CSF_SOURCE_LOCKED) {
                         dfb_surface_unlock_buffer( state->source, &state->src );

                         state->flags &= ~CSF_SOURCE_LOCKED;
                    }

                    return ret;
               }

               state->flags |= CSF_SOURCE_MASK_LOCKED;
          }
     }

     return DFB_OK;
}

static DFBResult
gAcquireUnlockBuffers( CardState *state )
{
     dfb_surface_unlock_buffer( state->destination, &state->dst );

     if (state->flags & CSF_SOURCE_LOCKED) {
          dfb_surface_unlock_buffer( state->source, &state->src );

          state->flags &= ~CSF_SOURCE_LOCKED;
     }

     if (state->flags & CSF_SOURCE_MASK_LOCKED) {
          dfb_surface_unlock_buffer( state->source_mask, &state->src_mask );

          state->flags &= ~CSF_SOURCE_MASK_LOCKED;
     }

     return DFB_OK;
}

static bool
gAcquireSetup( CardState           *state,
               DFBAccelerationMask  accel )
{
     GenefxState             *gfxs;
     GenefxFunc              *funcs;
     int                      dst_pfi;
     int                      src_pfi              = 0;
     int                      mask_pfi             = 0;
     CoreSurface             *destination          = state->destination;
     CoreSurface             *source               = state->source;
     DFBColor                 color                = state->color;
     bool                     src_ycbcr            = false;
     bool                     dst_ycbcr            = false;
     DFBSurfaceBlittingFlags  simpld_blittingflags = state->blittingflags;
     u16                      ca;

     dfb_simplify_blittingflags( &simpld_blittingflags );

     if (!state->gfxs) {
          gfxs = D_CALLOC( 1, sizeof(GenefxState) );
          if (!gfxs) {
               D_ERROR( "DirectFB/Genefx: Could not allocate Genefx state!\n" );
               return false;
          }

          state->gfxs = gfxs;
     }

     gfxs  = state->gfxs;
     funcs = gfxs->funcs;

     /*
      * Destination setup
      */

     gfxs->dst_caps         = destination->config.caps;
     gfxs->dst_height       = destination->config.size.h;
     gfxs->dst_format       = destination->config.format;
     gfxs->dst_bpp          = DFB_BYTES_PER_PIXEL( gfxs->dst_format );
     gfxs->dst_org[0]       = state->dst.addr;
     gfxs->dst_pitch        = state->dst.pitch;
     gfxs->dst_field_offset = gfxs->dst_height / 2 * gfxs->dst_pitch;

     dst_pfi = DFB_PIXELFORMAT_INDEX( gfxs->dst_format );

     switch (gfxs->dst_format) {
          case DSPF_I420:
               gfxs->dst_org[1] = gfxs->dst_org[0] + gfxs->dst_height * gfxs->dst_pitch;
               gfxs->dst_org[2] = gfxs->dst_org[1] + gfxs->dst_height / 2 * gfxs->dst_pitch / 2;
               break;
          case DSPF_YV12:
               gfxs->dst_org[2] = gfxs->dst_org[0] + gfxs->dst_height * gfxs->dst_pitch;
               gfxs->dst_org[1] = gfxs->dst_org[2] + gfxs->dst_height / 2 * gfxs->dst_pitch / 2;
               break;
          case DSPF_Y42B:
               gfxs->dst_org[1] = gfxs->dst_org[0] + gfxs->dst_height * gfxs->dst_pitch;
               gfxs->dst_org[2] = gfxs->dst_org[1] + gfxs->dst_height * gfxs->dst_pitch / 2;
               break;
          case DSPF_YV16:
               gfxs->dst_org[2] = gfxs->dst_org[0] + gfxs->dst_height * gfxs->dst_pitch;
               gfxs->dst_org[1] = gfxs->dst_org[2] + gfxs->dst_height * gfxs->dst_pitch / 2;
               break;
          case DSPF_Y444:
               gfxs->dst_org[1] = gfxs->dst_org[0] + gfxs->dst_height * gfxs->dst_pitch;
               gfxs->dst_org[2] = gfxs->dst_org[1] + gfxs->dst_height * gfxs->dst_pitch;
               break;
          case DSPF_YV24:
               gfxs->dst_org[2] = gfxs->dst_org[0] + gfxs->dst_height * gfxs->dst_pitch;
               gfxs->dst_org[1] = gfxs->dst_org[2] + gfxs->dst_height * gfxs->dst_pitch;
               break;
          case DSPF_NV12:
          case DSPF_NV21:
          case DSPF_NV16:
          case DSPF_NV61:
          case DSPF_NV24:
          case DSPF_NV42:
               gfxs->dst_org[1] = gfxs->dst_org[0] + gfxs->dst_height * gfxs->dst_pitch;
               break;
          default:
               break;
     }

     /*
      * Source setup
      */

     if (DFB_BLITTING_FUNCTION( accel )) {
          CoreSurface *source_mask = state->source_mask;

          gfxs->src_caps         = source->config.caps;
          gfxs->src_height       = source->config.size.h;
          gfxs->src_format       = source->config.format;
          gfxs->src_bpp          = DFB_BYTES_PER_PIXEL( gfxs->src_format );
          gfxs->src_org[0]       = state->src.addr;
          gfxs->src_pitch        = state->src.pitch;
          gfxs->src_field_offset = gfxs->src_height / 2 * gfxs->src_pitch;

          src_pfi = DFB_PIXELFORMAT_INDEX( gfxs->src_format );

          switch (gfxs->src_format) {
               case DSPF_I420:
                    gfxs->src_org[1] = gfxs->src_org[0] + gfxs->src_height * gfxs->src_pitch;
                    gfxs->src_org[2] = gfxs->src_org[1] + gfxs->src_height / 2 * gfxs->src_pitch / 2;
                    break;
               case DSPF_YV12:
                    gfxs->src_org[2] = gfxs->src_org[0] + gfxs->src_height * gfxs->src_pitch;
                    gfxs->src_org[1] = gfxs->src_org[2] + gfxs->src_height / 2 * gfxs->src_pitch / 2;
                    break;
               case DSPF_Y42B:
                    gfxs->src_org[1] = gfxs->src_org[0] + gfxs->src_height * gfxs->src_pitch;
                    gfxs->src_org[2] = gfxs->src_org[1] + gfxs->src_height * gfxs->src_pitch / 2;
                    break;
               case DSPF_YV16:
                    gfxs->src_org[2] = gfxs->src_org[0] + gfxs->src_height * gfxs->src_pitch;
                    gfxs->src_org[1] = gfxs->src_org[2] + gfxs->src_height * gfxs->src_pitch / 2;
                    break;
               case DSPF_Y444:
                    gfxs->src_org[1] = gfxs->src_org[0] + gfxs->src_height * gfxs->src_pitch;
                    gfxs->src_org[2] = gfxs->src_org[1] + gfxs->src_height * gfxs->src_pitch;
                    break;
               case DSPF_YV24:
                    gfxs->src_org[2] = gfxs->src_org[0] + gfxs->src_height * gfxs->src_pitch;
                    gfxs->src_org[1] = gfxs->src_org[2] + gfxs->src_height * gfxs->src_pitch;
                    break;
               case DSPF_NV12:
               case DSPF_NV21:
               case DSPF_NV16:
               case DSPF_NV61:
               case DSPF_NV24:
               case DSPF_NV42:
                    gfxs->src_org[1] = gfxs->src_org[0] + gfxs->src_height * gfxs->src_pitch;
                    break;
               default:
                    break;
          }

          /*
           * Mask setup
           */

          if (simpld_blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) {
               gfxs->mask_caps         = source_mask->config.caps;
               gfxs->mask_height       = source_mask->config.size.h;
               gfxs->mask_format       = source_mask->config.format;
               gfxs->mask_bpp          = DFB_BYTES_PER_PIXEL( gfxs->mask_format );
               gfxs->mask_org[0]       = state->src_mask.addr;
               gfxs->mask_pitch        = state->src_mask.pitch;
               gfxs->mask_field_offset = gfxs->mask_height / 2 * gfxs->mask_pitch;

               mask_pfi = DFB_PIXELFORMAT_INDEX( gfxs->mask_format );

               switch (gfxs->mask_format) {
                    case DSPF_I420:
                         gfxs->mask_org[1] = gfxs->mask_org[0] + gfxs->mask_height * gfxs->mask_pitch;
                         gfxs->mask_org[2] = gfxs->mask_org[1] + gfxs->mask_height / 2 * gfxs->mask_pitch / 2;
                         break;
                    case DSPF_YV12:
                         gfxs->mask_org[2] = gfxs->mask_org[0] + gfxs->mask_height * gfxs->mask_pitch;
                         gfxs->mask_org[1] = gfxs->mask_org[2] + gfxs->mask_height / 2 * gfxs->mask_pitch / 2;
                         break;
                    case DSPF_Y42B:
                         gfxs->mask_org[1] = gfxs->mask_org[0] + gfxs->mask_height * gfxs->mask_pitch;
                         gfxs->mask_org[2] = gfxs->mask_org[1] + gfxs->mask_height * gfxs->mask_pitch / 2;
                         break;
                    case DSPF_YV16:
                         gfxs->mask_org[2] = gfxs->mask_org[0] + gfxs->mask_height * gfxs->mask_pitch;
                         gfxs->mask_org[1] = gfxs->mask_org[2] + gfxs->mask_height * gfxs->mask_pitch / 2;
                         break;
                    case DSPF_Y444:
                         gfxs->mask_org[1] = gfxs->mask_org[0] + gfxs->mask_height * gfxs->mask_pitch;
                         gfxs->mask_org[2] = gfxs->mask_org[1] + gfxs->mask_height * gfxs->mask_pitch;
                         break;
                    case DSPF_YV24:
                         gfxs->mask_org[2] = gfxs->mask_org[0] + gfxs->mask_height * gfxs->mask_pitch;
                         gfxs->mask_org[1] = gfxs->mask_org[2] + gfxs->mask_height * gfxs->mask_pitch;
                         break;
                    case DSPF_NV12:
                    case DSPF_NV21:
                    case DSPF_NV16:
                    case DSPF_NV61:
                    case DSPF_NV24:
                    case DSPF_NV42:
                         gfxs->mask_org[1] = gfxs->mask_org[0] + gfxs->mask_height * gfxs->mask_pitch;
                         break;
                    default:
                         break;
               }
          }
     }

     /* Premultiply source (color). */
     if (DFB_DRAWING_FUNCTION(accel) && (state->drawingflags & DSDRAW_SRC_PREMULTIPLY)) {
          ca = color.a + 1;
          color.r = (color.r * ca) >> 8;
          color.g = (color.g * ca) >> 8;
          color.b = (color.b * ca) >> 8;
     }

     gfxs->color = color;

#define RGB_TO_YCBCR(r,g,b,y,cb,cr)                          \
     if (destination->config.colorspace == DSCS_BT601)       \
          RGB_TO_YCBCR_BT601(r,g,b,y,cb,cr);                 \
     else if (destination->config.colorspace == DSCS_BT709)  \
          RGB_TO_YCBCR_BT709(r,g,b,y,cb,cr);                 \
     else if (destination->config.colorspace == DSCS_BT2020) \
          RGB_TO_YCBCR_BT2020(r,g,b,y,cb,cr);                \
     else {                                                  \
          y = 16;                                            \
          cb = cr = 128;                                     \
     }

     switch (gfxs->dst_format) {
          case DSPF_ARGB1555:
               gfxs->Cop = PIXEL_ARGB1555( color.a, color.r, color.g, color.b );
               break;
          case DSPF_ARGB8565:
               gfxs->Cop = PIXEL_ARGB8565( color.a, color.r, color.g, color.b );
               break;
          case DSPF_RGB16:
               gfxs->Cop = PIXEL_RGB16( color.r, color.g, color.b );
               break;
          case DSPF_RGB18:
               gfxs->Cop = PIXEL_RGB18( color.r, color.g, color.b );
               break;
          case DSPF_RGB24:
               gfxs->Cop = PIXEL_RGB32( color.r, color.g, color.b );
               break;
          case DSPF_BGR24:
               gfxs->Cop = PIXEL_RGB32( color.b, color.g, color.r );
               break;
          case DSPF_RGB32:
               gfxs->Cop = PIXEL_RGB32( color.r, color.g, color.b );
               break;
          case DSPF_ARGB:
               gfxs->Cop = PIXEL_ARGB( color.a, color.r, color.g, color.b );
               break;
          case DSPF_ABGR:
               gfxs->Cop = PIXEL_ABGR( color.a, color.r, color.g, color.b );
               break;
          case DSPF_AiRGB:
               gfxs->Cop = PIXEL_AiRGB( color.a, color.r, color.g, color.b );
               break;
          case DSPF_ARGB6666:
               gfxs->Cop = PIXEL_ARGB6666( color.a, color.r, color.g, color.b );
               break;
          case DSPF_ARGB1666:
               gfxs->Cop = PIXEL_ARGB1666( color.a, color.r, color.g, color.b );
               break;
          case DSPF_A1:
               gfxs->Cop = color.a >> 7;
               break;
          case DSPF_A4:
               gfxs->Cop = color.a >> 4;
               break;
          case DSPF_A8:
               gfxs->Cop = color.a;
               break;
          case DSPF_YUY2:
               RGB_TO_YCBCR( color.r, color.g, color.b, gfxs->YCop, gfxs->CbCop, gfxs->CrCop );
#ifdef WORDS_BIGENDIAN
               gfxs->Cop = PIXEL_YUY2_BE( gfxs->YCop, gfxs->CbCop, gfxs->CrCop );
#else
               gfxs->Cop = PIXEL_YUY2_LE( gfxs->YCop, gfxs->CbCop, gfxs->CrCop );
#endif
               break;
          case DSPF_RGB332:
               gfxs->Cop = PIXEL_RGB332( color.r, color.g, color.b );
               break;
          case DSPF_UYVY:
               RGB_TO_YCBCR( color.r, color.g, color.b, gfxs->YCop, gfxs->CbCop, gfxs->CrCop );
#ifdef WORDS_BIGENDIAN
               gfxs->Cop = PIXEL_UYVY_BE( gfxs->YCop, gfxs->CbCop, gfxs->CrCop );
#else
               gfxs->Cop = PIXEL_UYVY_LE( gfxs->YCop, gfxs->CbCop, gfxs->CrCop );
#endif
               break;
          case DSPF_I420:
          case DSPF_YV12:
          case DSPF_NV12:
          case DSPF_NV21:
          case DSPF_Y42B:
          case DSPF_YV16:
          case DSPF_NV16:
          case DSPF_NV61:
          case DSPF_Y444:
          case DSPF_YV24:
          case DSPF_NV24:
          case DSPF_NV42:
               RGB_TO_YCBCR( color.r, color.g, color.b, gfxs->YCop, gfxs->CbCop, gfxs->CrCop );
               gfxs->Cop = gfxs->YCop;
               break;
          case DSPF_LUT1:
          case DSPF_LUT2:
          case DSPF_LUT8:
               gfxs->Cop  = state->color_index;
               gfxs->Alut = destination->palette;
               break;
          case DSPF_ALUT44:
               gfxs->Cop  = (color.a & 0xf0) + state->color_index;
               gfxs->Alut = destination->palette;
               break;
          case DSPF_ARGB2554:
               gfxs->Cop = PIXEL_ARGB2554( color.a, color.r, color.g, color.b );
               break;
          case DSPF_ARGB4444:
               gfxs->Cop = PIXEL_ARGB4444( color.a, color.r, color.g, color.b );
               break;
          case DSPF_RGBA4444:
               gfxs->Cop = PIXEL_RGBA4444( color.a, color.r, color.g, color.b );
               break;
          case DSPF_AYUV:
               RGB_TO_YCBCR( color.r, color.g, color.b, gfxs->YCop, gfxs->CbCop, gfxs->CrCop );
               gfxs->Cop = PIXEL_AYUV( color.a, gfxs->YCop, gfxs->CbCop, gfxs->CrCop );
               break;
          case DSPF_RGB444:
               gfxs->Cop = PIXEL_RGB444( color.r, color.g, color.b );
               break;
          case DSPF_RGB555:
               gfxs->Cop = PIXEL_RGB555( color.r, color.g, color.b );
               break;
          case DSPF_BGR555:
               gfxs->Cop = PIXEL_BGR555( color.r, color.g, color.b );
               break;
          case DSPF_RGBA5551:
               gfxs->Cop = PIXEL_RGBA5551( color.a, color.r, color.g, color.b );
               break;
          case DSPF_RGBAF88871:
               gfxs->Cop = PIXEL_RGBAF88871( color.a, color.r, color.g, color.b );
               break;
          case DSPF_AVYU:
               RGB_TO_YCBCR( color.r, color.g, color.b, gfxs->YCop, gfxs->CbCop, gfxs->CrCop );
               gfxs->Cop = PIXEL_AVYU( color.a, gfxs->YCop, gfxs->CbCop, gfxs->CrCop );
               break;
          case DSPF_VYU:
               RGB_TO_YCBCR( color.r, color.g, color.b, gfxs->YCop, gfxs->CbCop, gfxs->CrCop );
               gfxs->Cop = PIXEL_VYU( gfxs->YCop, gfxs->CbCop, gfxs->CrCop );
               break;
          case DSPF_A1_LSB:
               gfxs->Cop = color.a & 1;
               break;
          default:
               D_ONCE( "unsupported destination format" );
               return false;
     }

#undef RGB_TO_YCBCR

     dst_ycbcr = is_ycbcr[DFB_PIXELFORMAT_INDEX(gfxs->dst_format)];

     if (DFB_BLITTING_FUNCTION( accel )) {
          switch (gfxs->src_format) {
               case DSPF_LUT1:
               case DSPF_LUT2:
               case DSPF_LUT8:
               case DSPF_ALUT44:
                    gfxs->Blut = source->palette;
               case DSPF_ARGB1555:
               case DSPF_RGBA5551:
               case DSPF_ARGB2554:
               case DSPF_ARGB4444:
               case DSPF_RGBA4444:
               case DSPF_ARGB1666:
               case DSPF_ARGB6666:
               case DSPF_ARGB8565:
               case DSPF_RGB16:
               case DSPF_RGB18:
               case DSPF_RGB24:
               case DSPF_BGR24:
               case DSPF_RGB32:
               case DSPF_ARGB:
               case DSPF_ABGR:
               case DSPF_AiRGB:
               case DSPF_RGB332:
               case DSPF_RGB444:
               case DSPF_RGB555:
               case DSPF_BGR555:
               case DSPF_RGBAF88871:
                    if (dst_ycbcr && simpld_blittingflags & (DSBLIT_COLORIZE | DSBLIT_SRC_PREMULTCOLOR))
                         return false;
               case DSPF_A1:
               case DSPF_A1_LSB:
               case DSPF_A4:
               case DSPF_A8:
                    if (DFB_PLANAR_PIXELFORMAT(gfxs->dst_format) && simpld_blittingflags & DSBLIT_DST_COLORKEY)
                         return false;
                    break;
               case DSPF_I420:
               case DSPF_YV12:
               case DSPF_NV12:
               case DSPF_NV21:
               case DSPF_Y42B:
               case DSPF_YV16:
               case DSPF_NV16:
               case DSPF_NV61:
               case DSPF_Y444:
               case DSPF_YV24:
               case DSPF_NV24:
               case DSPF_NV42:
                    if (simpld_blittingflags & DSBLIT_SRC_COLORKEY)
                         return false;
               case DSPF_YUY2:
               case DSPF_UYVY:
               case DSPF_AYUV:
               case DSPF_AVYU:
               case DSPF_VYU:
                    if (dst_ycbcr && simpld_blittingflags & (DSBLIT_COLORIZE | DSBLIT_SRC_PREMULTCOLOR))
                         return false;
                    if (DFB_PLANAR_PIXELFORMAT(gfxs->dst_format) && simpld_blittingflags & DSBLIT_DST_COLORKEY)
                         return false;
                    break;
               default:
                    D_ONCE( "unsupported source format" );
                    return false;
          }
     }

     src_ycbcr = is_ycbcr[DFB_PIXELFORMAT_INDEX(gfxs->src_format)];

     gfxs->need_accumulator = true;

     gfxs->Astep = gfxs->Bstep = gfxs->Ostep = 1;

     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_DRAWLINE:
          case DFXL_FILLTRIANGLE:
               if (state->drawingflags & ~(DSDRAW_DST_COLORKEY | DSDRAW_SRC_PREMULTIPLY | DSDRAW_DST_PREMULTIPLY)) {
                    GenefxAccumulator Cacc, SCacc;

                    if (state->drawingflags & DSDRAW_BLEND) {
                         if (state->src_blend == DSBF_ZERO) {
                              if (state->dst_blend == DSBF_ZERO) {
                                   gfxs->Cop = 0;
                                   if (state->drawingflags & DSDRAW_DST_COLORKEY) {
                                        gfxs->Dkey = state->dst_colorkey;
                                        *funcs++ = Cop_toK_Aop_PFI[dst_pfi];
                                   }
                                   else
                                        *funcs++ = Cop_to_Aop_PFI[dst_pfi];
                                   break;
                              }
                              else if (state->dst_blend == DSBF_ONE) {
                                   break;
                              }
                         }
                         else if (state->src_blend == DSBF_ONE && state->dst_blend == DSBF_ZERO) {
                              if (state->drawingflags & DSDRAW_DST_COLORKEY) {
                                   gfxs->Dkey = state->dst_colorkey;
                                   *funcs++ = Cop_toK_Aop_PFI[dst_pfi];
                              }
                              else
                                   *funcs++ = Cop_to_Aop_PFI[dst_pfi];
                              break;
                         }
                    }

                    /* Load from destination. */
                    *funcs++ = Sop_is_Aop;
                    if (DFB_PIXELFORMAT_IS_INDEXED( gfxs->dst_format ))
                         *funcs++ = Slut_is_Alut;
                    *funcs++ = Dacc_is_Aacc;
                    *funcs++ = Sop_PFI_to_Dacc[dst_pfi];

                    if (dst_ycbcr) {
                         if (destination->config.colorspace == DSCS_BT601)
                              *funcs++ = Dacc_YCbCr_to_RGB_BT601;
                         else if (destination->config.colorspace == DSCS_BT709)
                              *funcs++ = Dacc_YCbCr_to_RGB_BT709;
                         else if (destination->config.colorspace == DSCS_BT2020)
                              *funcs++ = Dacc_YCbCr_to_RGB_BT2020;
                    }

                    /* Premultiply destination. */
                    if (state->drawingflags & DSDRAW_DST_PREMULTIPLY)
                         *funcs++ = Dacc_premultiply;

                    /* Load source (color). */
                    Cacc.RGB.a = color.a;
                    Cacc.RGB.r = color.r;
                    Cacc.RGB.g = color.g;
                    Cacc.RGB.b = color.b;

                    if (state->drawingflags & DSDRAW_BLEND) {
                         /* Source blending. */
                         switch (state->src_blend) {
                              case DSBF_ZERO:
                                   break;
                              case DSBF_ONE:
                                   SCacc = Cacc;
                                   break;
                              case DSBF_SRCCOLOR:
                                   SCacc.RGB.a = (Cacc.RGB.a * (Cacc.RGB.a + 1)) >> 8;
                                   SCacc.RGB.r = (Cacc.RGB.r * (Cacc.RGB.r + 1)) >> 8;
                                   SCacc.RGB.g = (Cacc.RGB.g * (Cacc.RGB.g + 1)) >> 8;
                                   SCacc.RGB.b = (Cacc.RGB.b * (Cacc.RGB.b + 1)) >> 8;
                                   break;
                              case DSBF_INVSRCCOLOR:
                                   SCacc.RGB.a = (Cacc.RGB.a * (0x100 - Cacc.RGB.a)) >> 8;
                                   SCacc.RGB.r = (Cacc.RGB.r * (0x100 - Cacc.RGB.r)) >> 8;
                                   SCacc.RGB.g = (Cacc.RGB.g * (0x100 - Cacc.RGB.g)) >> 8;
                                   SCacc.RGB.b = (Cacc.RGB.b * (0x100 - Cacc.RGB.b)) >> 8;
                                   break;
                              case DSBF_SRCALPHA:
                                   ca = color.a + 1;
                                   SCacc.RGB.a = (Cacc.RGB.a * ca) >> 8;
                                   SCacc.RGB.r = (Cacc.RGB.r * ca) >> 8;
                                   SCacc.RGB.g = (Cacc.RGB.g * ca) >> 8;
                                   SCacc.RGB.b = (Cacc.RGB.b * ca) >> 8;
                                   break;
                              case DSBF_INVSRCALPHA:
                                   ca = 0x100 - color.a;
                                   SCacc.RGB.a = (Cacc.RGB.a * ca) >> 8;
                                   SCacc.RGB.r = (Cacc.RGB.r * ca) >> 8;
                                   SCacc.RGB.g = (Cacc.RGB.g * ca) >> 8;
                                   SCacc.RGB.b = (Cacc.RGB.b * ca) >> 8;
                                   break;
                              case DSBF_SRCALPHASAT:
                                   *funcs++ = Sacc_is_NULL;
                              case DSBF_DESTALPHA:
                              case DSBF_INVDESTALPHA:
                              case DSBF_DESTCOLOR:
                              case DSBF_INVDESTCOLOR:
                                   *funcs++ = Dacc_is_Bacc;
                                   *funcs++ = Cacc_to_Dacc;
                                   *funcs++ = Dacc_is_Aacc;
                                   *funcs++ = Xacc_is_Bacc;
                                   *funcs++ = Yacc_is_Bacc;
                                   *funcs++ = Xacc_blend[state->src_blend-1];
                                   break;
                              default:
                                   D_BUG( "unknown src_blend %u", state->src_blend );
                         }

                         /* Destination blending. */
                         *funcs++ = Sacc_is_NULL;
                         *funcs++ = Xacc_is_Tacc;
                         *funcs++ = Yacc_is_Aacc;

                         if (state->dst_blend > D_ARRAY_SIZE(Xacc_blend) || state->dst_blend < 1)
                              D_BUG( "unknown dst_blend %u", state->dst_blend );
                         else
                              *funcs++ = Xacc_blend[state->dst_blend-1];

                         /* Add source to destination accumulator. */
                         switch (state->src_blend) {
                              case DSBF_ZERO:
                                   break;
                              case DSBF_ONE:
                              case DSBF_SRCCOLOR:
                              case DSBF_INVSRCCOLOR:
                              case DSBF_SRCALPHA:
                              case DSBF_INVSRCALPHA:
                                   if (SCacc.RGB.a || SCacc.RGB.r || SCacc.RGB.g || SCacc.RGB.b) {
                                        *funcs++ = Dacc_is_Tacc;
                                        *funcs++ = SCacc_add_to_Dacc;
                                   }
                                   break;
                              case DSBF_DESTALPHA:
                              case DSBF_INVDESTALPHA:
                              case DSBF_DESTCOLOR:
                              case DSBF_INVDESTCOLOR:
                              case DSBF_SRCALPHASAT:
                                   *funcs++ = Sacc_is_Bacc;
                                   *funcs++ = Dacc_is_Tacc;
                                   *funcs++ = Sacc_add_to_Dacc;
                                   break;
                              default:
                                   D_BUG( "unknown src_blend %u", state->src_blend );
                         }
                    }

                    /* Demultiply result. */
                    if (state->drawingflags & DSDRAW_DEMULTIPLY)
                         *funcs++ = Dacc_demultiply;

                    /* XOR destination. */
                    if (state->drawingflags & DSDRAW_XOR) {
                         if (state->drawingflags & DSDRAW_BLEND) {
                              *funcs++ = Sacc_is_Aacc;
                              *funcs++ = Sacc_xor_Dacc;
                              *funcs++ = Sacc_is_Tacc;

                              if (dst_ycbcr)
                                   *funcs++ = Dacc_is_Tacc;
                         }
                         else {
                              *funcs++ = Dacc_xor;
                              *funcs++ = Sacc_is_Aacc;

                              if (dst_ycbcr)
                                   *funcs++ = Dacc_is_Aacc;
                         }
                    }
                    else if (state->drawingflags & DSDRAW_BLEND) {
                         *funcs++ = Sacc_is_Tacc;

                         if (dst_ycbcr)
                              *funcs++ = Dacc_is_Tacc;
                    }
                    else {
                         *funcs++ = Sacc_is_Aacc;

                         if (dst_ycbcr)
                              *funcs++ = Dacc_is_Aacc;
                    }

                    if (dst_ycbcr) {
                         if (destination->config.colorspace == DSCS_BT601)
                              *funcs++ = Dacc_RGB_to_YCbCr_BT601;
                         else if (destination->config.colorspace == DSCS_BT709)
                              *funcs++ = Dacc_RGB_to_YCbCr_BT709;
                         else if (destination->config.colorspace == DSCS_BT2020)
                              *funcs++ = Dacc_RGB_to_YCbCr_BT2020;
                    }

                    /* Write to destination. */
                    if (state->drawingflags & DSDRAW_DST_COLORKEY) {
                         gfxs->Dkey = state->dst_colorkey;
                         *funcs++ = Sacc_toK_Aop_PFI[dst_pfi];
                    }
                    else
                         *funcs++ = Sacc_to_Aop_PFI[dst_pfi];

                    /* Store computed Cacc. */
                    gfxs->Cacc  = Cacc;
                    gfxs->SCacc = SCacc;
               }
               else {
                    gfxs->need_accumulator = false;

                    if (state->drawingflags & DSDRAW_DST_COLORKEY) {
                         gfxs->Dkey = state->dst_colorkey;
                         *funcs++ = Cop_toK_Aop_PFI[dst_pfi];
                    }
                    else
                         *funcs++ = Cop_to_Aop_PFI[dst_pfi];
               }
               break;
          case DFXL_BLIT:
               if (simpld_blittingflags == DSBLIT_BLEND_ALPHACHANNEL &&
                   state->src_blend == DSBF_SRCALPHA &&
                   state->dst_blend == DSBF_INVSRCALPHA) {
                    if (gfxs->src_format == DSPF_ARGB &&
                        Bop_argb_blend_alphachannel_src_invsrc_Aop_PFI[dst_pfi]) {
                         *funcs++ = Bop_argb_blend_alphachannel_src_invsrc_Aop_PFI[dst_pfi];
                         break;
                    }
               }
               if (simpld_blittingflags == DSBLIT_BLEND_ALPHACHANNEL &&
                   state->src_blend == DSBF_ONE &&
                   state->dst_blend == DSBF_INVSRCALPHA) {
                    if (gfxs->src_format == DSPF_ARGB &&
                        Bop_argb_blend_alphachannel_one_invsrc_Aop_PFI[dst_pfi]) {
                         *funcs++ = Bop_argb_blend_alphachannel_one_invsrc_Aop_PFI[dst_pfi];
                         break;
                    }
               }
               if (simpld_blittingflags == (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_SRC_PREMULTIPLY) &&
                   state->src_blend == DSBF_ONE &&
                   state->dst_blend == DSBF_INVSRCALPHA) {
                    if (gfxs->src_format == DSPF_ARGB &&
                        Bop_argb_blend_alphachannel_one_invsrc_premultiply_Aop_PFI[dst_pfi]) {
                         *funcs++ = Bop_argb_blend_alphachannel_one_invsrc_premultiply_Aop_PFI[dst_pfi];
                         break;
                    }
               }
               if (((simpld_blittingflags == (DSBLIT_COLORIZE | DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_SRC_PREMULTIPLY) &&
                     state->src_blend == DSBF_ONE) ||
                    (simpld_blittingflags == (DSBLIT_COLORIZE | DSBLIT_BLEND_ALPHACHANNEL) &&
                     state->src_blend == DSBF_SRCALPHA)) &&
                   state->dst_blend   == DSBF_INVSRCALPHA) {
                    if (gfxs->src_format == DSPF_A8 && Bop_a8_set_alphapixel_Aop_PFI[dst_pfi]) {
                         *funcs++ = Bop_a8_set_alphapixel_Aop_PFI[dst_pfi];
                         break;
                    }
                    if (gfxs->src_format == DSPF_A1 && Bop_a1_set_alphapixel_Aop_PFI[dst_pfi]) {
                         *funcs++ = Bop_a1_set_alphapixel_Aop_PFI[dst_pfi];
                         break;
                    }
                    if (gfxs->src_format == DSPF_A1_LSB && Bop_a1_lsb_set_alphapixel_Aop_PFI[dst_pfi]) {
                         *funcs++ = Bop_a1_lsb_set_alphapixel_Aop_PFI[dst_pfi];
                         break;
                    }
               }
#ifndef WORDS_BIGENDIAN
               if (simpld_blittingflags       == DSBLIT_NOFX &&
                   source->config.format      == DSPF_RGB24 &&
                   destination->config.format == DSPF_RGB16) {
                    *funcs++ = Bop_rgb24_to_Aop_rgb16_LE;
                    break;
               }
               if (simpld_blittingflags       == DSBLIT_NOFX &&
                   (source->config.format     == DSPF_RGB32 || source->config.format == DSPF_ARGB) &&
                   destination->config.format == DSPF_RGB16) {
                    *funcs++ = Bop_rgb32_to_Aop_rgb16_LE;
                    break;
               }
#endif
               /* fall through */
          case DFXL_TEXTRIANGLES:
          case DFXL_STRETCHBLIT: {
               int modulation = simpld_blittingflags & MODULATION_FLAGS;

               if (modulation                                                                   ||
                   (accel == DFXL_TEXTRIANGLES && (src_pfi != dst_pfi || simpld_blittingflags)) ||
                   (simpld_blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR))     ||
                   ((simpld_blittingflags & DSBLIT_ROTATE90) && accel == DFXL_STRETCHBLIT)) {
                    bool read_destination         = false;
                    bool source_needs_destination = false;
                    bool scale_from_accumulator;

                    /* Check if destination has to be read. */
                    if (simpld_blittingflags & (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA)) {
                         switch (state->src_blend) {
                              case DSBF_DESTALPHA:
                              case DSBF_DESTCOLOR:
                              case DSBF_INVDESTALPHA:
                              case DSBF_INVDESTCOLOR:
                              case DSBF_SRCALPHASAT:
                                   source_needs_destination = true;
                              default:
                                   ;
                         }

                         read_destination = source_needs_destination || (state->dst_blend != DSBF_ZERO) ||
                                            (simpld_blittingflags & DSBLIT_XOR);
                    }
                    else if (simpld_blittingflags & DSBLIT_XOR) {
                         read_destination = true;
                    }

                    scale_from_accumulator = !read_destination && (accel == DFXL_STRETCHBLIT);

                    /* Read the destination if needed. */
                    if (read_destination) {
                         *funcs++ = Sop_is_Aop;
                         if (DFB_PIXELFORMAT_IS_INDEXED( gfxs->dst_format ))
                              *funcs++ = Slut_is_Alut;
                         *funcs++ = Dacc_is_Aacc;
                         *funcs++ = Sop_PFI_to_Dacc[dst_pfi];

                         if (dst_ycbcr) {
                              if (destination->config.colorspace == DSCS_BT601)
                                   *funcs++ = Dacc_YCbCr_to_RGB_BT601;
                              else if (destination->config.colorspace == DSCS_BT709)
                                   *funcs++ = Dacc_YCbCr_to_RGB_BT709;
                              else if (destination->config.colorspace == DSCS_BT2020)
                                   *funcs++ = Dacc_YCbCr_to_RGB_BT2020;
                         }

                         if (simpld_blittingflags & DSBLIT_DST_PREMULTIPLY)
                              *funcs++ = Dacc_premultiply;
                    }
                    else if (scale_from_accumulator) {
                         *funcs++ = Len_is_Slen;
                    }

                    /* Read the source */
                    *funcs++ = Sop_is_Bop;
                    if (DFB_PIXELFORMAT_IS_INDEXED( gfxs->src_format ))
                         *funcs++ = Slut_is_Blut;
                    *funcs++ = Dacc_is_Bacc;
                    if (accel == DFXL_TEXTRIANGLES) {
                         if (simpld_blittingflags & DSBLIT_SRC_COLORKEY) {
                              gfxs->Skey = state->src_colorkey;

                              *funcs++ = Sop_PFI_TEX_Kto_Dacc[src_pfi];
                         }
                         else {
                              *funcs++ = Sop_PFI_TEX_to_Dacc[src_pfi];
                         }
                    }
                    else {
                         if (simpld_blittingflags & DSBLIT_SRC_COLORKEY) {
                              gfxs->Skey = state->src_colorkey;
                              if (accel == DFXL_BLIT || scale_from_accumulator)
                                   *funcs++ = Sop_PFI_Kto_Dacc[src_pfi];
                              else
                                   *funcs++ = Sop_PFI_SKto_Dacc[src_pfi];
                         }
                         else {
                              if (accel == DFXL_BLIT || scale_from_accumulator)
                                   *funcs++ = Sop_PFI_to_Dacc[src_pfi];
                              else
                                   *funcs++ = Sop_PFI_Sto_Dacc[src_pfi];
                         }
                    }

                    if (src_ycbcr) {
                         if (source->config.colorspace == DSCS_BT601)
                              *funcs++ = Dacc_YCbCr_to_RGB_BT601;
                         else if (source->config.colorspace == DSCS_BT709)
                              *funcs++ = Dacc_YCbCr_to_RGB_BT709;
                         else if (source->config.colorspace == DSCS_BT2020)
                              *funcs++ = Dacc_YCbCr_to_RGB_BT2020;
                    }

                    /* Premultiply color alpha. */
                    if (simpld_blittingflags & DSBLIT_SRC_PREMULTCOLOR) {
                         gfxs->Cacc.RGB.a = color.a + 1;
                         *funcs++ = Dacc_premultiply_color_alpha;
                    }

                    /* Modulate the source if requested. */
                    if (Dacc_modulation[modulation & (DSBLIT_COLORIZE           |
                                                      DSBLIT_BLEND_ALPHACHANNEL |
                                                      DSBLIT_BLEND_COLORALPHA)]) {
                         /* Modulation source. */
                         gfxs->Cacc.RGB.a = color.a + 1;
                         gfxs->Cacc.RGB.r = color.r + 1;
                         gfxs->Cacc.RGB.g = color.g + 1;
                         gfxs->Cacc.RGB.b = color.b + 1;

                         *funcs++ = Dacc_modulation[modulation & (DSBLIT_COLORIZE           |
                                                                  DSBLIT_BLEND_ALPHACHANNEL |
                                                                  DSBLIT_BLEND_COLORALPHA)];
                    }

                    /* Modulate the source with mask if requested. */
                    if (simpld_blittingflags & DSBLIT_SRC_MASK_ALPHA) {
                         if (simpld_blittingflags & DSBLIT_SRC_MASK_COLOR) {
                              if (Dacc_modulate_mask_argb_from_PFI[mask_pfi])
                                   *funcs++ = Dacc_modulate_mask_argb_from_PFI[mask_pfi];
                         }
                         else {
                              if (Dacc_modulate_mask_alpha_from_PFI[mask_pfi])
                                   *funcs++ = Dacc_modulate_mask_alpha_from_PFI[mask_pfi];
                         }
                    }
                    else if (simpld_blittingflags & DSBLIT_SRC_MASK_COLOR) {
                         if (Dacc_modulate_mask_rgb_from_PFI[mask_pfi])
                              *funcs++ = Dacc_modulate_mask_rgb_from_PFI[mask_pfi];
                    }

                    /* Premultiply (modulated) source alpha. */
                    if (simpld_blittingflags & DSBLIT_SRC_PREMULTIPLY)
                         *funcs++ = Dacc_premultiply;

                    /* Do blend functions and combine both accumulators. */
                    if (simpld_blittingflags & (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA)) {
                         *funcs++ = Sacc_is_Bacc;
                         *funcs++ = Dacc_is_Aacc;

                         if (source_needs_destination &&
                             state->dst_blend != DSBF_ONE) {
                              /* Blend the destination. */
                              *funcs++ = Yacc_is_Aacc;
                              *funcs++ = Xacc_is_Tacc;
                              *funcs++ = Xacc_blend[state->dst_blend-1];

                              /* Blend the source. */
                              *funcs++ = Xacc_is_Bacc;
                              *funcs++ = Yacc_is_Bacc;
                              *funcs++ = Xacc_blend[state->src_blend-1];
                         }
                         else {
                              /* Blend the destination if needed. */
                              if (read_destination) {
                                   *funcs++ = Yacc_is_Aacc;
                                   *funcs++ = Xacc_is_Tacc;
                                   *funcs++ = Xacc_blend[state->dst_blend-1];
                              }

                              /* Blend the source. */
                              *funcs++ = Xacc_is_Bacc;
                              *funcs++ = Yacc_is_Bacc;
                              *funcs++ = Xacc_blend[state->src_blend-1];
                         }

                         /* Add the destination to the source. */
                         if (read_destination) {
                              *funcs++ = Sacc_is_Tacc;
                              *funcs++ = Dacc_is_Bacc;
                              *funcs++ = Sacc_add_to_Dacc;
                         }
                    }

                    if (simpld_blittingflags & DSBLIT_DEMULTIPLY) {
                         *funcs++ = Dacc_is_Bacc;
                         *funcs++ = Dacc_demultiply;
                    }

                    /* XOR source with destination. */
                    if (simpld_blittingflags & DSBLIT_XOR) {
                         *funcs++ = Sacc_is_Aacc;
                         *funcs++ = Dacc_is_Bacc;
                         *funcs++ = Dacc_clamp;
                         *funcs++ = Sacc_xor_Dacc;
                    }

                    if (dst_ycbcr) {
                         *funcs++ = Dacc_is_Bacc;
                         if (destination->config.colorspace == DSCS_BT601)
                              *funcs++ = Dacc_RGB_to_YCbCr_BT601;
                         else if (destination->config.colorspace == DSCS_BT709)
                              *funcs++ = Dacc_RGB_to_YCbCr_BT709;
                         else if (destination->config.colorspace == DSCS_BT2020)
                              *funcs++ = Dacc_RGB_to_YCbCr_BT2020;
                    }

                    /* Write source to destination. */
                    *funcs++ = Sacc_is_Bacc;
                    if (scale_from_accumulator) {
                         *funcs++ = Len_is_Dlen;
                         if (simpld_blittingflags & DSBLIT_DST_COLORKEY) {
                              gfxs->Dkey = state->dst_colorkey;
                              *funcs++ = Sacc_StoK_Aop_PFI[dst_pfi];
                         }
                         else
                              *funcs++ = Sacc_Sto_Aop_PFI[dst_pfi];
                    }
                    else {
                         if (simpld_blittingflags & DSBLIT_DST_COLORKEY) {
                              gfxs->Dkey = state->dst_colorkey;
                              *funcs++ = Sacc_toK_Aop_PFI[dst_pfi];
                         }
                         else
                              *funcs++ = Sacc_to_Aop_PFI[dst_pfi];
                    }
               }
               else if (simpld_blittingflags == DSBLIT_INDEX_TRANSLATION &&
                        DFB_PIXELFORMAT_IS_INDEXED( gfxs->src_format ) &&
                        DFB_PIXELFORMAT_IS_INDEXED( gfxs->dst_format )) {
                    gfxs->trans     = state->index_translation;
                    gfxs->num_trans = state->num_translation;

                    switch (gfxs->src_format) {
                         case DSPF_LUT2:
                              switch (gfxs->dst_format) {
                                   case DSPF_LUT8:
                                        *funcs++ = Bop_lut2_translate_to_Aop_lut8;
                                        break;

                                   default:
                                        D_ONCE( "no index translation to %s implemented",
                                                dfb_pixelformat_name( gfxs->dst_format ) );
                                        break;
                              }
                              break;
                         default:
                              D_ONCE( "no index translation from %s implemented",
                                      dfb_pixelformat_name( gfxs->src_format ) );
                              break;
                    }
               }
               else if (((gfxs->src_format == gfxs->dst_format &&
                          (!DFB_PIXELFORMAT_IS_INDEXED( gfxs->src_format ) ||
                           dfb_palette_equal( gfxs->Alut, gfxs->Blut ))) ||
                         ((gfxs->src_format == DSPF_I420 ||
                           gfxs->src_format == DSPF_YV12 ||
                           gfxs->src_format == DSPF_Y42B ||
                           gfxs->src_format == DSPF_YV16) &&
                          (gfxs->dst_format == DSPF_I420 ||
                           gfxs->dst_format == DSPF_YV12 ||
                           gfxs->dst_format == DSPF_Y42B ||
                           gfxs->dst_format == DSPF_YV16))) &&
                        (accel == DFXL_BLIT || !(simpld_blittingflags & (DSBLIT_ROTATE90 | DSBLIT_FLIP_HORIZONTAL)))) {
                    gfxs->need_accumulator = false;

                    switch (accel) {
                         case DFXL_BLIT:
                              if (simpld_blittingflags & DSBLIT_SRC_COLORKEY &&
                                  simpld_blittingflags & DSBLIT_DST_COLORKEY) {
                                   gfxs->Skey = state->src_colorkey;
                                   gfxs->Dkey = state->dst_colorkey;
                                   *funcs++ = Bop_PFI_KtoK_Aop_PFI[dst_pfi];
                              }
                              else if (simpld_blittingflags & DSBLIT_SRC_COLORKEY) {
                                   gfxs->Skey = state->src_colorkey;
                                   *funcs++ = Bop_PFI_Kto_Aop_PFI[dst_pfi];
                              }
                              else if (simpld_blittingflags & DSBLIT_DST_COLORKEY) {
                                   gfxs->Dkey = state->dst_colorkey;
                                   *funcs++ = Bop_PFI_toK_Aop_PFI[dst_pfi];
                              }
                              else if (simpld_blittingflags & (DSBLIT_ROTATE90 | DSBLIT_FLIP_HORIZONTAL)) {
                                   *funcs++ = Bop_PFI_toR_Aop_PFI[dst_pfi];
                              }
                              else
                                   *funcs++ = Bop_PFI_to_Aop_PFI[dst_pfi];
                              break;
                         case DFXL_STRETCHBLIT:
                              if (simpld_blittingflags & DSBLIT_SRC_COLORKEY &&
                                  simpld_blittingflags & DSBLIT_DST_COLORKEY) {
                                   gfxs->Skey = state->src_colorkey;
                                   gfxs->Dkey = state->dst_colorkey;
                                   *funcs++ = Bop_PFI_SKtoK_Aop_PFI[dst_pfi];
                              }
                              else if (simpld_blittingflags & DSBLIT_SRC_COLORKEY) {
                                   gfxs->Skey = state->src_colorkey;
                                   *funcs++ = Bop_PFI_SKto_Aop_PFI[dst_pfi];
                              }
                              else if (simpld_blittingflags & DSBLIT_DST_COLORKEY) {
                                   gfxs->Dkey = state->dst_colorkey;
                                   *funcs++ = Bop_PFI_StoK_Aop_PFI[dst_pfi];
                              }
                              else
                                   *funcs++ = Bop_PFI_Sto_Aop_PFI[dst_pfi];
                              break;
                         case DFXL_TEXTRIANGLES:
                              *funcs++ = Bop_PFI_TEX_to_Aop_PFI[dst_pfi];
                              break;
                         default:
                              break;
                    }
               }
               else {
                    bool scale_from_accumulator = (src_ycbcr != dst_ycbcr) && (accel == DFXL_STRETCHBLIT);

                    if (scale_from_accumulator)
                         *funcs++ = Len_is_Slen;

                    gfxs->Sop = gfxs->Bop;

                    if (DFB_PIXELFORMAT_IS_INDEXED( gfxs->src_format ))
                         *funcs++ = Slut_is_Blut;

                    if (accel == DFXL_BLIT || scale_from_accumulator) {
                         if (simpld_blittingflags & DSBLIT_SRC_COLORKEY) {
                              gfxs->Skey = state->src_colorkey;
                              *funcs++ = Sop_PFI_Kto_Dacc[src_pfi];
                         }
                         else
                              *funcs++ = Sop_PFI_to_Dacc[src_pfi];
                    }
                    else { /* DFXL_STRETCHBLIT */
                         if (simpld_blittingflags & DSBLIT_SRC_COLORKEY) {
                              gfxs->Skey = state->src_colorkey;
                              *funcs++ = Sop_PFI_SKto_Dacc[src_pfi];
                         }
                         else
                              *funcs++ = Sop_PFI_Sto_Dacc[src_pfi];

                    }

                    if (!src_ycbcr && dst_ycbcr) {
                         if (DFB_COLOR_BITS_PER_PIXEL( gfxs->src_format )) {
                              if (destination->config.colorspace == DSCS_BT601)
                                   *funcs++ = Dacc_RGB_to_YCbCr_BT601;
                              else if (destination->config.colorspace == DSCS_BT709)
                                   *funcs++ = Dacc_RGB_to_YCbCr_BT709;
                              else if (destination->config.colorspace == DSCS_BT2020)
                                   *funcs++ = Dacc_RGB_to_YCbCr_BT2020;
                         }
                         else
                              *funcs++ = Dacc_Alpha_to_YCbCr;
                    }
                    else if (src_ycbcr && !dst_ycbcr) {
                         if (DFB_COLOR_BITS_PER_PIXEL( gfxs->dst_format )) {
                              if (source->config.colorspace == DSCS_BT601)
                                   *funcs++ = Dacc_YCbCr_to_RGB_BT601;
                              else if (source->config.colorspace == DSCS_BT709)
                                   *funcs++ = Dacc_YCbCr_to_RGB_BT709;
                              else if (source->config.colorspace == DSCS_BT2020)
                                   *funcs++ = Dacc_YCbCr_to_RGB_BT2020;
                         }
                    }

                    if (scale_from_accumulator) {
                         *funcs++ = Len_is_Dlen;
                         if (simpld_blittingflags & DSBLIT_DST_COLORKEY) {
                              gfxs->Dkey = state->dst_colorkey;
                              *funcs++ = Sacc_StoK_Aop_PFI[dst_pfi];
                         }
                         else
                              *funcs++ = Sacc_Sto_Aop_PFI[dst_pfi];
                    }
                    else {
                         if (simpld_blittingflags & DSBLIT_DST_COLORKEY) {
                              gfxs->Dkey = state->dst_colorkey;
                              *funcs++ = Sacc_toK_Aop_PFI[dst_pfi];
                         }
                         else
                              *funcs++ = Sacc_to_Aop_PFI[dst_pfi];
                    }
               }
               break;
          }
          default:
               D_ONCE( "unimplemented drawing/blitting function" );
               return false;
     }

     *funcs = NULL;

     dfb_state_update( state, state->flags & CSF_SOURCE_LOCKED );

     return true;
}

bool
gAcquire( CardState           *state,
          DFBAccelerationMask  accel )
{
     DFBResult ret;

     if (!gAcquireCheck( state, accel ))
          return false;

     /* Push our own identity for buffer locking calls (locality of accessor). */
     Core_PushIdentity( 0 );

     ret = gAcquireLockBuffers( state, accel );
     if (ret) {
          Core_PopIdentity();
          return false;
     }

     if (!gAcquireSetup( state, accel )) {
          gAcquireUnlockBuffers( state );
          Core_PopIdentity();
          return false;
     }

     return true;
}

void
gRelease( CardState *state )
{
     gAcquireUnlockBuffers( state );

     Core_PopIdentity();
}

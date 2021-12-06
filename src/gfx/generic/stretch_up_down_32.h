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

#define STRETCH_HVX_N_H  "stretch_hvx_32.h"
#define uN u32

/**********************************************************************************************************************/
/* Upscaling */

#define POINT_0              0
#define LINE_0               0
#define MINUS_1              1
#define POINT_TO_RATIO(p,ps) ( ((p) & 0x3ffff) >> (18 - SHIFT_L8) )
#define LINE_TO_RATIO(l,ls)  ( ((l) & 0x3ffff) >> (18 - SHIFT_L8) )
#define POINT_L(p,ps)        (  (p) >> 18 )
#define POINT_R(p,ps)        ( ((p) >> 18) + 1 )
#define LINE_T(l,ls)         (  (l) >> 18 )
#define LINE_B(l,ls)         ( ((l) >> 18) + 1 )

#define UPDOWN 1
#include "stretch_hvx_N.h"
#undef UPDOWN

#undef POINT_0
#undef LINE_0
#undef MINUS_1
#undef POINT_TO_RATIO
#undef LINE_TO_RATIO
#undef POINT_L
#undef POINT_R
#undef LINE_T
#undef LINE_B

/**********************************************************************************************************************/
/* Downscaling */

#define POINT_0              hfraq
#define LINE_0               vfraq
#define MINUS_1              0
#define POINT_TO_RATIO(p,ps) ( ((((p) & 0x3ffff) ?: 0x40000) << SHIFT_L8) / (ps) )
#define LINE_TO_RATIO(l,ls)  ( ((((l) & 0x3ffff) ?: 0x40000) << SHIFT_L8) / (ls) )
#define POINT_L(p,ps)        (  (((p) - 1) >> 18) - 1 )
#define POINT_R(p,ps)        (   ((p) - 1) >> 18 )
#define LINE_T(l,ls)         (  (((l) - 1) >> 18) - 1 )
#define LINE_B(l,ls)         (   ((l) - 1) >> 18 )

#define UPDOWN 0
#include "stretch_hvx_N.h"
#undef UPDOWN

#undef POINT_0
#undef LINE_0
#undef MINUS_1
#undef POINT_TO_RATIO
#undef LINE_TO_RATIO
#undef POINT_L
#undef POINT_R
#undef LINE_T
#undef LINE_B

/**********************************************************************************************************************/

#undef STRETCH_HVX_N_H
#undef uN

#include "stretch_up_down_table.h"

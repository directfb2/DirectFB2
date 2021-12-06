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

static const StretchFunctionTable TABLE_NAME = {
#define FUNC_NAME_(K,P,F) FUNC_NAME(up,K,P,F)
     .f[DFB_PIXELFORMAT_INDEX(DST_FORMAT)].up[STRETCH_NONE]                = FUNC_NAME_(_,_,DST_FORMAT),
     .f[DFB_PIXELFORMAT_INDEX(DST_FORMAT)].up[STRETCH_PROTECT]             = FUNC_NAME_(_,P,DST_FORMAT),
     .f[DFB_PIXELFORMAT_INDEX(DST_FORMAT)].up[STRETCH_SRCKEY]              = FUNC_NAME_(K,_,DST_FORMAT),
     .f[DFB_PIXELFORMAT_INDEX(DST_FORMAT)].up[STRETCH_SRCKEY_PROTECT]      = FUNC_NAME_(K,P,DST_FORMAT),
     .f[DFB_PIXELFORMAT_INDEX(DSPF_LUT8)].up[STRETCH_NONE]                 = FUNC_NAME_(_,_,DSPF_LUT8),
     .f[DFB_PIXELFORMAT_INDEX(DSPF_LUT8)].up[STRETCH_PROTECT]              = FUNC_NAME_(_,P,DSPF_LUT8),
     .f[DFB_PIXELFORMAT_INDEX(DSPF_LUT8)].up[STRETCH_SRCKEY]               = FUNC_NAME_(K,_,DSPF_LUT8),
     .f[DFB_PIXELFORMAT_INDEX(DSPF_LUT8)].up[STRETCH_SRCKEY_PROTECT]       = FUNC_NAME_(K,P,DSPF_LUT8),
#ifdef FORMAT_RGB16
     .f[DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)].up[STRETCH_NONE]             = FUNC_NAME_(_,_,DSPF_ARGB4444),
     .f[DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)].up[STRETCH_PROTECT]          = FUNC_NAME_(_,P,DSPF_ARGB4444),
     .f[DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)].up[STRETCH_SRCKEY]           = FUNC_NAME_(K,_,DSPF_ARGB4444),
     .f[DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)].up[STRETCH_SRCKEY_PROTECT]   = FUNC_NAME_(K,P,DSPF_ARGB4444),
#endif
#ifdef FORMAT_ARGB4444
     .f[DFB_PIXELFORMAT_INDEX(DSPF_RGB16)].up[STRETCH_NONE]                = FUNC_NAME_(_,_,DSPF_RGB16),
     .f[DFB_PIXELFORMAT_INDEX(DSPF_RGB16)].up[STRETCH_PROTECT]             = FUNC_NAME_(_,P,DSPF_RGB16),
     .f[DFB_PIXELFORMAT_INDEX(DSPF_RGB16)].up[STRETCH_SRCKEY]              = FUNC_NAME_(K,_,DSPF_RGB16),
     .f[DFB_PIXELFORMAT_INDEX(DSPF_RGB16)].up[STRETCH_SRCKEY_PROTECT]      = FUNC_NAME_(K,P,DSPF_RGB16),
#endif
#undef FUNC_NAME_

#define FUNC_NAME_(K,P,F) FUNC_NAME(down,K,P,F)
     .f[DFB_PIXELFORMAT_INDEX(DST_FORMAT)].down[STRETCH_NONE]              = FUNC_NAME_(_,_,DST_FORMAT),
     .f[DFB_PIXELFORMAT_INDEX(DST_FORMAT)].down[STRETCH_PROTECT]           = FUNC_NAME_(_,P,DST_FORMAT),
     .f[DFB_PIXELFORMAT_INDEX(DST_FORMAT)].down[STRETCH_SRCKEY]            = FUNC_NAME_(K,_,DST_FORMAT),
     .f[DFB_PIXELFORMAT_INDEX(DST_FORMAT)].down[STRETCH_SRCKEY_PROTECT]    = FUNC_NAME_(K,P,DST_FORMAT),
     .f[DFB_PIXELFORMAT_INDEX(DSPF_LUT8)].down[STRETCH_NONE]               = FUNC_NAME_(_,_,DSPF_LUT8),
     .f[DFB_PIXELFORMAT_INDEX(DSPF_LUT8)].down[STRETCH_PROTECT]            = FUNC_NAME_(_,P,DSPF_LUT8),
     .f[DFB_PIXELFORMAT_INDEX(DSPF_LUT8)].down[STRETCH_SRCKEY]             = FUNC_NAME_(K,_,DSPF_LUT8),
     .f[DFB_PIXELFORMAT_INDEX(DSPF_LUT8)].down[STRETCH_SRCKEY_PROTECT]     = FUNC_NAME_(K,P,DSPF_LUT8),
#ifdef FORMAT_RGB16
     .f[DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)].down[STRETCH_NONE]           = FUNC_NAME_(_,_,DSPF_ARGB4444),
     .f[DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)].down[STRETCH_PROTECT]        = FUNC_NAME_(_,P,DSPF_ARGB4444),
     .f[DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)].down[STRETCH_SRCKEY]         = FUNC_NAME_(K,_,DSPF_ARGB4444),
     .f[DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)].down[STRETCH_SRCKEY_PROTECT] = FUNC_NAME_(K,P,DSPF_ARGB4444),
#endif
#ifdef FORMAT_ARGB4444
     .f[DFB_PIXELFORMAT_INDEX(DSPF_RGB16)].down[STRETCH_NONE]              = FUNC_NAME_(_,_,DSPF_RGB16),
     .f[DFB_PIXELFORMAT_INDEX(DSPF_RGB16)].down[STRETCH_PROTECT]           = FUNC_NAME_(_,P,DSPF_RGB16),
     .f[DFB_PIXELFORMAT_INDEX(DSPF_RGB16)].down[STRETCH_SRCKEY]            = FUNC_NAME_(K,_,DSPF_RGB16),
     .f[DFB_PIXELFORMAT_INDEX(DSPF_RGB16)].down[STRETCH_SRCKEY_PROTECT]    = FUNC_NAME_(K,P,DSPF_RGB16),
#endif
#undef FUNC_NAME_
};

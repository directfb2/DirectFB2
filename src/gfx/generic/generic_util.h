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

#ifndef __GENERIC_UTIL_H__
#define __GENERIC_UTIL_H__

#include <core/coretypes.h>
#include <direct/log.h>
#include <direct/trace.h>
#include <misc/conf.h>

/**********************************************************************************************************************/

#define CHECK_PIPELINE()                                                                                      \
     {                                                                                                        \
          if (!gfxs->funcs[0])                                                                                \
               return;                                                                                        \
                                                                                                              \
          if (dfb_config->software_trace) {                                                                   \
               int         i;                                                                                 \
               GenefxFunc *funcs = gfxs->funcs;                                                               \
               DirectLog  *log   = direct_log_default();                                                      \
                                                                                                              \
               direct_log_lock( log );                                                                        \
               direct_log_printf( log, "  Software Fallback Pipeline:\n" );                                   \
                                                                                                              \
               for (i = 0; funcs[i]; ++i)                                                                     \
                    direct_log_printf( log, "    [%2d] %s\n", i, direct_trace_lookup_symbol_at( funcs[i] ) ); \
                                                                                                              \
               direct_log_printf( log, "\n" );                                                                \
               direct_log_unlock( log );                                                                      \
          }                                                                                                   \
     }

#define RUN_PIPELINE()                     \
     {                                     \
          int         i;                   \
          GenefxFunc *funcs = gfxs->funcs; \
                                           \
          for (i = 0; funcs[i]; ++i)       \
               funcs[i]( gfxs );           \
     }

/**********************************************************************************************************************/

void Genefx_Aop_crab     ( GenefxState *gfxs );

void Genefx_Aop_prev_crab( GenefxState *gfxs );

void Genefx_Aop_xy       ( GenefxState *gfxs,
                           int          x,
                           int          y );

void Genefx_Aop_next     ( GenefxState *gfxs );

void Genefx_Aop_prev     ( GenefxState *gfxs );

void Genefx_Bop_xy       ( GenefxState *gfxs,
                           int          x,
                           int          y );

void Genefx_Bop_next     ( GenefxState *gfxs );

void Genefx_Bop_prev     ( GenefxState *gfxs );

void Genefx_Mop_xy       ( GenefxState *gfxs,
                           int          x,
                           int          y );

void Genefx_Mop_next     ( GenefxState *gfxs );

void Genefx_Mop_prev     ( GenefxState *gfxs );

bool Genefx_ABacc_prepare( GenefxState *gfxs,
                           int          width );

void Genefx_ABacc_flush  ( GenefxState *gfxs );

#endif

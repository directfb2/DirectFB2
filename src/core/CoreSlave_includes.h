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

#ifndef __CORE__CORESLAVE_INCLUDES_H__
#define __CORE__CORESLAVE_INCLUDES_H__

#include <fusion/call.h>
#include <misc/conf.h>

typedef struct {
     FusionCall call;
} CoreSlave;

static __inline__ DirectResult
CoreSlave_Call( CoreSlave           *slave,
                FusionCallExecFlags  flags,
                int                  call_arg,
                void                *ptr,
                unsigned int         length,
                void                *ret_ptr,
                unsigned int         ret_size,
                unsigned int        *ret_length )
{
     return fusion_call_execute3( &slave->call, dfb_config->call_nodirect | flags, call_arg, ptr, length,
                                  ret_ptr, ret_size, ret_length );
}

#endif

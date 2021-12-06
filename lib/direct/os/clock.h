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

#ifndef __DIRECT__OS__CLOCK_H__
#define __DIRECT__OS__CLOCK_H__

#include <direct/os/types.h>

/**********************************************************************************************************************/

typedef enum {
     DIRECT_CLOCK_SESSION            = 0x53551011, /* Derived from monotonic clock. */

     DIRECT_CLOCK_REALTIME           = 0x00000000,
     DIRECT_CLOCK_MONOTONIC          = 0x00000001,
     DIRECT_CLOCK_PROCESS_CPUTIME_ID = 0x00000002,
     DIRECT_CLOCK_THREAD_CPUTIME_ID  = 0x00000003
} DirectClockType;

long long    DIRECT_API direct_clock_get_time  ( DirectClockType type );

DirectResult DIRECT_API direct_clock_set_time  ( DirectClockType type,
                                                 long long       micros );

long long    DIRECT_API direct_clock_resolution( DirectClockType type );

#endif

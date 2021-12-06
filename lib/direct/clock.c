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

#include <direct/clock.h>
#include <direct/debug.h>

D_DEBUG_DOMAIN( Direct_Clock, "Direct/Clock", "Direct Clock time measurement" );

/**********************************************************************************************************************/

__dfb_no_instrument_function__
long long
direct_clock_get_micros()
{
     return direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );
}

__dfb_no_instrument_function__
long long
direct_clock_get_millis()
{
     return direct_clock_get_time( DIRECT_CLOCK_MONOTONIC ) / 1000LL;
}

__dfb_no_instrument_function__
long long
direct_clock_get_abs_micros()
{
     return direct_clock_get_time( DIRECT_CLOCK_REALTIME );
}

__dfb_no_instrument_function__
long long
direct_clock_get_abs_millis()
{
     return direct_clock_get_time( DIRECT_CLOCK_REALTIME ) / 1000LL;
}

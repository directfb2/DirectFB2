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

#include <direct/os/clock.h>
#include <direct/messages.h>
#include <direct/util.h>

/**********************************************************************************************************************/

static long long session_clock_offset;

/**********************************************************************************************************************/

__attribute__((no_instrument_function))
long long
direct_clock_get_time( DirectClockType type )
{
     long long       micros;
     struct timespec spec;
     clockid_t       clock_id;

     switch (type) {
          case DIRECT_CLOCK_REALTIME:
               clock_id = CLOCK_REALTIME;
               break;

          case DIRECT_CLOCK_SESSION:
          case DIRECT_CLOCK_MONOTONIC:
               clock_id = CLOCK_MONOTONIC;
               break;

          default:
               D_BUG( "invalid clock type %u", type );
               return DR_INVARG;
     }

     if (clock_gettime( clock_id, &spec ) < 0) {
          if (clock_id != CLOCK_REALTIME) {
               D_WARN( "clock with id %d not supported by system", clock_id );
               return direct_clock_get_time( DIRECT_CLOCK_REALTIME );
          }

          D_PERROR( "Direct/Clock: Could not get real time clock!\n" );
          return 0;
     }

     micros = spec.tv_sec * 1000000LL + spec.tv_nsec / 1000LL;

     if (type == DIRECT_CLOCK_SESSION)
          micros -= session_clock_offset;

     return micros;
}

__attribute__((no_instrument_function))
DirectResult
direct_clock_set_time( DirectClockType type,
                       long long       micros )
{
     DirectResult    ret;
     clockid_t       clock_id;
     struct timespec spec;

     switch (type) {
          case DIRECT_CLOCK_SESSION:
               session_clock_offset = micros;
               return DR_OK;

          case DIRECT_CLOCK_REALTIME:
               clock_id = CLOCK_REALTIME;
               break;

          case DIRECT_CLOCK_MONOTONIC:
               clock_id = CLOCK_MONOTONIC;
               break;

          default:
               D_BUG( "invalid clock type %u", type );
               return DR_INVARG;
     }

     spec.tv_sec  = micros / 1000000LL;
     spec.tv_nsec = micros % 1000000LL * 1000LL;

     if (clock_settime( clock_id, &spec ) < 0) {
          ret = errno2result( errno );
          D_PERROR( "Direct/Clock: Could not set clock with id %d!\n", clock_id );
          return ret;
     }

     return DR_OK;
}

__attribute__((no_instrument_function))
long long
direct_clock_resolution( DirectClockType type )
{
     struct timespec spec;
     clockid_t       clock_id;

     switch (type) {
          case DIRECT_CLOCK_SESSION:
          case DIRECT_CLOCK_REALTIME:
               clock_id = CLOCK_REALTIME;
               break;

          case DIRECT_CLOCK_MONOTONIC:
               clock_id = CLOCK_MONOTONIC;
               break;

          default:
               D_BUG( "invalid clock type %u", type );
               return DR_INVARG;
     }

     if (clock_getres( clock_id, &spec ) < 0) {
          if (clock_id != CLOCK_REALTIME) {
               D_WARN( "clock with id %d not supported by system", clock_id );
               return direct_clock_resolution( DIRECT_CLOCK_REALTIME );
          }

          D_PERROR( "Direct/Clock: Could not get real time clock resolution!\n" );
          return 0;
     }

     return spec.tv_sec * 1000000LL + spec.tv_nsec / 1000LL;
}

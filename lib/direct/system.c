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

#include <direct/debug.h>
#include <direct/messages.h>
#include <direct/system.h>

D_DEBUG_DOMAIN( Direct_Futex, "Direct/Futex", "Direct Futex" );

/**********************************************************************************************************************/

DirectResult
direct_futex_wait( int *uaddr,
                   int  val )
{
     DirectResult ret;

     D_ASSERT( uaddr != NULL );

     D_DEBUG_AT( Direct_Futex, "%s( %p, %d ) <- %d\n", __FUNCTION__, uaddr, val, *uaddr );

     if (*uaddr != val) {
          D_DEBUG_AT( Direct_Futex, "  -> value changed!\n" );
          return DR_OK;
     }

     while ((ret = direct_futex( uaddr, FUTEX_WAIT, val, NULL, NULL, 0 ))) {
          switch (ret) {
               case DR_SIGNALLED:
                    continue;

               case DR_BUSY:
                    return DR_OK;

               default:
                    D_DERROR( ret, "Direct/Futex: FUTEX_WAIT( %p, %d ) failed!\n", uaddr, val );
                    return ret;
          }
     }

     return DR_OK;
}

DirectResult
direct_futex_wait_timed( int *uaddr,
                         int  val,
                         int  ms )
{
     DirectResult    ret;
     struct timespec timeout;

     D_ASSERT( uaddr != NULL );

     D_DEBUG_AT( Direct_Futex, "%s( %p, %d, %d ) <- %d\n", __FUNCTION__, uaddr, val, ms, *uaddr );

     if (*uaddr != val) {
          D_DEBUG_AT( Direct_Futex, "  -> value changed!\n" );
          return DR_OK;
     }

     timeout.tv_sec  =  ms / 1000;
     timeout.tv_nsec = (ms % 1000) * 1000000;

     while ((ret = direct_futex( uaddr, FUTEX_WAIT, val, &timeout, NULL, 0 ))) {
          switch (ret) {
               case DR_SIGNALLED:
                    continue;

               case DR_BUSY:
                    return DR_OK;

               default:
                    D_DERROR( ret, "Direct/Futex: FUTEX_WAIT( %p, %d ) failed!\n", uaddr, val );

               case DR_TIMEOUT:
                    return ret;
          }
     }

     return DR_OK;
}

DirectResult
direct_futex_wake( int *uaddr,
                   int  num )
{
     DirectResult ret;

     D_ASSERT( uaddr != NULL );
     D_ASSERT( num > 0 );

     D_DEBUG_AT( Direct_Futex, "%s( %p, %d ) <- %d\n", __FUNCTION__, uaddr, num, *uaddr );

     while ((ret = direct_futex( uaddr, FUTEX_WAKE, num, NULL, NULL, 0 ))) {
          switch (ret) {
               case DR_BUSY:
                    continue;

               default:
                    D_DERROR( ret, "Direct/Futex: FUTEX_WAKE( %p, %d ) failed!\n", uaddr, num );
                    return ret;
          }
     }

     return DR_OK;
}

unsigned int __Direct_Futex_Wait_Count = 0;
unsigned int __Direct_Futex_Wake_Count = 0;

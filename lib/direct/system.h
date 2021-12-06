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

#ifndef __DIRECT__SYSTEM_H__
#define __DIRECT__SYSTEM_H__

#include <direct/os/system.h>

/**********************************************************************************************************************/

DirectResult DIRECT_API direct_futex_wait      ( int *uaddr,
                                                 int  val );

DirectResult DIRECT_API direct_futex_wait_timed( int *uaddr,
                                                 int  val,
                                                 int  ms );

DirectResult DIRECT_API direct_futex_wake      ( int *uaddr,
                                                 int  num );

#endif

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

#ifndef __DIRECT__COMPILER_H__
#define __DIRECT__COMPILER_H__

#include <direct/build.h>

/**********************************************************************************************************************/

#define D_FORMAT_PRINTF(n)             __attribute__((__format__(__printf__,n,n+1)))
#define D_FORMAT_VPRINTF(n)            __attribute__((__format__(__printf__,n,0)))

#define D_UNUSED                       __attribute__((unused))

#define __dfb_no_instrument_function__ __attribute__((no_instrument_function))

#if DIRECT_BUILD_CTORS
#define __dfb_constructor__            __attribute__((constructor))
#define __dfb_destructor__             __attribute__((destructor))
#else
#define __dfb_constructor__
#define __dfb_destructor__
#endif

#endif

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

#ifndef __DIRECT__MEMCPY_H__
#define __DIRECT__MEMCPY_H__

#include <direct/types.h>

/**********************************************************************************************************************/

void        DIRECT_API  direct_find_best_memcpy     ( void );

void        DIRECT_API  direct_print_memcpy_routines( void );

extern void DIRECT_API *(*direct_memcpy)            ( void       *to,
                                                      const void *from,
                                                      size_t      len );

/**********************************************************************************************************************/

static __inline__ void *
direct_memmove( void       *to,
                const void *from,
                size_t      len )
{
     if ((from < to && ((const char*) from + len) < ((char*) to)) || (((char*) to + len) < ((const char*) from)))
          return direct_memcpy( to, from, len );
     else
          return memmove( to, from, len );
}

#endif

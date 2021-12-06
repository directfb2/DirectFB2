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

#ifndef __DIRECT__RESULT_H__
#define __DIRECT__RESULT_H__

#include <direct/types.h>

/**********************************************************************************************************************/

typedef struct {
     int            magic;
     int            refs;

     unsigned int   base;

     const char   **result_strings;
     unsigned int   result_count;
} DirectResultType;

/**********************************************************************************************************************/

DirectResult DIRECT_API  DirectResultTypeRegister  ( DirectResultType *type );

DirectResult DIRECT_API  DirectResultTypeUnregister( DirectResultType *type );

const char   DIRECT_API *DirectResultString        ( DirectResult      result );

/**********************************************************************************************************************/

void __D_result_init  ( void );
void __D_result_deinit( void );

#endif

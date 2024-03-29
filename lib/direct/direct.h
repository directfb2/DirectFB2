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

#ifndef __DIRECT__DIRECT_H__
#define __DIRECT__DIRECT_H__

#include <direct/types.h>

/**********************************************************************************************************************/

typedef void (*DirectCleanupHandlerFunc)( void *ctx );

/**********************************************************************************************************************/

DirectResult DIRECT_API direct_initialize            ( void );

DirectResult DIRECT_API direct_shutdown              ( void );

DirectResult DIRECT_API direct_cleanup_handler_add   ( DirectCleanupHandlerFunc   func,
                                                       void                      *ctx,
                                                       DirectCleanupHandler     **ret_handler );

DirectResult DIRECT_API direct_cleanup_handler_remove( DirectCleanupHandler      *handler );

/**********************************************************************************************************************/

void __D_direct_init  ( void );
void __D_direct_deinit( void );

#endif

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

#ifndef __DIRECT__INTERFACE_IMPLEMENTATION_H__
#define __DIRECT__INTERFACE_IMPLEMENTATION_H__

#include <direct/interface.h>

/**********************************************************************************************************************/

static const char   *GetType          ( void );
static const char   *GetImplementation( void );
static DirectResult  Allocate         ( void **ret_interface );
static DirectResult  Deallocate       ( void  *interface_ptr );

static DirectInterfaceFuncs interface_funcs = {
     GetType,
     GetImplementation,
     Allocate,
     Deallocate,
     (void*) Probe,
     (void*) Construct
};

#define DIRECT_INTERFACE_IMPLEMENTATION(type,impl)           \
                                                             \
static const char *                                          \
GetType()                                                    \
{                                                            \
     return #type;                                           \
}                                                            \
                                                             \
static const char *                                          \
GetImplementation()                                          \
{                                                            \
     return #impl;                                           \
}                                                            \
                                                             \
static DirectResult                                          \
Allocate( void **ret_interface )                             \
{                                                            \
     DIRECT_ALLOCATE_INTERFACE( *ret_interface, type );      \
     return DR_OK;                                           \
}                                                            \
                                                             \
static DirectResult                                          \
Deallocate( void *interface_ptr )                            \
{                                                            \
     DIRECT_DEALLOCATE_INTERFACE( (IAny*) (interface_ptr) ); \
     return DR_OK;                                           \
}                                                            \
                                                             \
__dfb_constructor__                                          \
void                                                         \
type##_##impl##_ctor( void )                                 \
{                                                            \
     DirectRegisterInterface( &interface_funcs );            \
}                                                            \
                                                             \
__dfb_destructor__                                           \
void                                                         \
type##_##impl##_dtor( void )                                 \
{                                                            \
     DirectUnregisterInterface( &interface_funcs );          \
}

#endif

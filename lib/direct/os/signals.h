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

#ifndef __DIRECT__OS__SIGNALS_H__
#define __DIRECT__OS__SIGNALS_H__

#include <direct/os/types.h>

/**********************************************************************************************************************/

typedef enum {
     DSHR_OK     = 0x00000000,
     DSHR_REMOVE = 0x00000001,
     DSHR_RESUME = 0x00000002
} DirectSignalHandlerResult;

typedef DirectSignalHandlerResult (*DirectSignalHandlerFunc)( int num, void *addr, void *ctx );

/*
 * Signal number to use when registering a handler for any interrupt.
 */
#define DIRECT_SIGNAL_ANY        -1
#define DIRECT_SIGNAL_DUMP_STACK -2

/**********************************************************************************************************************/

DirectResult DIRECT_API direct_signals_initialize   ( void );

DirectResult DIRECT_API direct_signals_shutdown     ( void );

/*
 * Modifies the current thread's signal mask to block everything.
 */
void         DIRECT_API direct_signals_block_all    ( void );

DirectResult DIRECT_API direct_signal_handler_add   ( int                       num,
                                                      DirectSignalHandlerFunc   func,
                                                      void                     *ctx,
                                                      DirectSignalHandler     **ret_handler );

DirectResult DIRECT_API direct_signal_handler_remove( DirectSignalHandler      *handler );

#endif

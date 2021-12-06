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

#ifndef __DIRECT__TRACE_H__
#define __DIRECT__TRACE_H__

#include <direct/types.h>

/**********************************************************************************************************************/

/*
 * Look up a symbol by filename and offset.
 *
 * Returns symbol name on success or NULL.
 */
const char        DIRECT_API *direct_trace_lookup_symbol( const char         *filename,
                                                          long                offset );

/*
 * Returns filename on success or NULL.
 *
 * Stores load address of object in 'ret_base' on success.
 */
const char        DIRECT_API *direct_trace_lookup_file  ( void               *address,
                                                          void              **ret_base );

/*
 * Print stack in 'buffer' or current if NULL.
 */
void              DIRECT_API  direct_trace_print_stack  ( DirectTraceBuffer  *buffer );

/*
 * Print stack of each known thread.
 */
void              DIRECT_API  direct_trace_print_stacks ( void );

/*
 * Returns indent level for debug output.
 */
int               DIRECT_API  direct_trace_debug_indent ( void );

/*
 * Retrieve pointer to calling function if present, otherwise NULL.
 */
void              DIRECT_API *direct_trace_get_caller   ( void );

/*
 * Create a copy of a stack in 'buffer' or of current if NULL.
 */
DirectTraceBuffer DIRECT_API *direct_trace_copy_buffer  ( DirectTraceBuffer  *buffer );

/*
 * Free a (copied) stack buffer.
 */
void              DIRECT_API  direct_trace_free_buffer  ( DirectTraceBuffer  *buffer );

/**********************************************************************************************************************/

/*
 * Convenience function combining direct_trace_lookup_file() and direct_trace_lookup_symbol().
 */
static __inline__ const char *
direct_trace_lookup_symbol_at( void *address )
{
     void       *base;
     const char *filename;

     filename = direct_trace_lookup_file( address, &base );

     return direct_trace_lookup_symbol( filename, (unsigned long) address - (unsigned long) base );
}

#endif

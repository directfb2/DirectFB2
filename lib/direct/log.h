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

#ifndef __DIRECT__LOG_H__
#define __DIRECT__LOG_H__

#include <direct/compiler.h>
#include <direct/os/log.h>

/**********************************************************************************************************************/

/*
 * Creates a logging facility.
 *
 * For each 'type' the 'param' has a different meaning:
 *   DLT_STDERR     ignored (leave NULL)
 *   DLT_FILE       file name
 *   DLT_UDP        <ip>:<port>
 */
DirectResult DIRECT_API  direct_log_create     ( DirectLogType   type,
                                                 const char     *param,
                                                 DirectLog     **ret_log );

/*
 * Destroys a logging facility.
 */
DirectResult DIRECT_API  direct_log_destroy    ( DirectLog      *log );

/*
 * Write to the log in a plain fashion.
 *
 * If log is NULL, the default log is used if it's valid, otherwise stderr is used as a fallback until now.
 */
DirectResult DIRECT_API  direct_log_write      ( DirectLog      *log,
                                                 const char     *buffer,
                                                 size_t          bytes );

/*
 * Write to the log in a printf fashion.
 *
 * If log is NULL, the default log is used if it's valid, otherwise stderr is used as a fallback until now.
 */
DirectResult DIRECT_API  direct_log_printf     ( DirectLog      *log,
                                                 const char     *format, ... ) D_FORMAT_PRINTF(2);

/*
 * Set the default log that is used when no valid log is passed.
 */
DirectResult DIRECT_API  direct_log_set_default( DirectLog      *log );

/*
 * Locks a logging facility for non-intermixed output of multiple calls in multiple threads.
 */
void         DIRECT_API  direct_log_lock       ( DirectLog      *log );

/*
 * Unlocks a logging facility.
 */
void         DIRECT_API  direct_log_unlock     ( DirectLog      *log );

/*
 * Set a buffer to be used for the log data.
 */
DirectResult DIRECT_API  direct_log_set_buffer ( DirectLog      *log,
                                                 char           *buffer,
                                                 size_t          bytes );

/*
 * Flush the log data and optionally synchronize with the output.
 */
DirectResult DIRECT_API  direct_log_flush      ( DirectLog      *log,
                                                 bool            sync );

/*
 * Returns the default log.
 */
DirectLog    DIRECT_API *direct_log_default    ( void );

/*
 * Enable delay between log messages.
 */
void         DIRECT_API  direct_log_debug_delay( bool            min );

/**********************************************************************************************************************/

void __D_log_init  ( void );
void __D_log_deinit( void );

#endif

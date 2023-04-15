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

#ifndef __DIRECT__CONF_H__
#define __DIRECT__CONF_H__

#include <direct/log_domain.h>

/**********************************************************************************************************************/

typedef enum {
     DCFL_NONE   = 0x00000000, /* None is fatal. */
     DCFL_ASSERT = 0x00000001, /* ASSERT is fatal. */
     DCFL_ASSUME = 0x00000002  /* ASSERT and ASSUME are fatal. */
} DirectConfigFatalLevel;

typedef enum {
     DCTS_OTHER    = 0x00000000, /* Other scheduling. */
     DCTS_FIFO     = 0x00000001, /* First in, first out scheduling. */
     DCTS_RR       = 0x00000002, /* Round-robin scheduling. */
     DCTS_SPORADIC = 0x00000003  /* Sporadic scheduling. */
} DirectConfigThreadScheduler;

typedef enum {
     DMT_NONE          = 0x00000000, /* No message type. */

     DMT_BANNER        = 0x00000001, /* Startup banner. */
     DMT_INFO          = 0x00000002, /* Info messages. */
     DMT_WARNING       = 0x00000004, /* Warnings. */
     DMT_ERROR         = 0x00000008, /* Error messages: regular, bugs, system call errors, dlopen errors. */
     DMT_UNIMPLEMENTED = 0x00000010, /* Messages notifying unimplemented functionality. */
     DMT_ONCE          = 0x00000020, /* One-shot messages. */

     DMT_BUG           = 0x00000080, /* A bug occurred. */
     DMT_OOM           = 0x00000100, /* Out of memory. */
     DMT_OOSHM         = 0x00000200, /* Out of shared memory. */

     DMT_ALL           = 0x000003BF  /* All types. */
} DirectMessageType;

typedef struct {
     char                        **disable_module;
     char                         *module_dir;
     char                         *memcpy;
     DirectMessageType             quiet;
     DirectMessageType             fatal_messages;
     DirectLogLevel                log_level;
     bool                          log_all;
     bool                          log_none;
     bool                          debugmem;
     bool                          trace;
     bool                          nm_for_trace;
     DirectLog                    *log;
     DirectConfigFatalLevel        fatal;
     bool                          sighandler;
     bool                          sighandler_thread;
     sigset_t                      dont_catch;
     bool                          thread_block_signals;
     int                           thread_priority_scale;
     int                           thread_priority;
     DirectConfigThreadScheduler   thread_scheduler;
     int                           thread_stack_size;
     char                        **default_interface_implementation_types;
     char                        **default_interface_implementation_names;
     int                           log_delay_rand_loops;
     int                           log_delay_rand_us;
     int                           log_delay_min_loops;
     int                           log_delay_min_us;
     int                           delay_trap_ms;
} DirectConfig;

/**********************************************************************************************************************/

extern DirectConfig DIRECT_API *direct_config;

extern const char   DIRECT_API *direct_config_usage;

/*
 * Set indiviual option.
 */
DirectResult        DIRECT_API  direct_config_set                       ( const char  *name,
                                                                          const char  *value );
/*
 * Retrieve all values set on option 'name'.
 * Pass an array of char* pointers in 'values' and number of pointers in 'values_len'.
 * The actual returned number of values gets returned in 'ret_num'.
 * The returned pointers are not extra allocated so do not free them.
 */
DirectResult        DIRECT_API  direct_config_get                       ( const char  *name,
                                                                          char       **values,
                                                                          const int    values_len,
                                                                          int         *ret_num );

/*
 * Check for an occurrence of the passed option.
 */
bool                DIRECT_API  direct_config_has_name                  ( const char  *name );

/*
 * Return the value for the last occurrence of the passed option's setting.
 */
const char          DIRECT_API *direct_config_get_value                 ( const char  *name );

/*
 * Return the integer value for the last occurrence of the passed option's setting.
 */
long long           DIRECT_API  direct_config_get_int_value             ( const char  *name );

long long           DIRECT_API  direct_config_get_int_value_with_default( const char  *name,
                                                                          long long    def );

/**********************************************************************************************************************/

void __D_conf_init  ( void );
void __D_conf_deinit( void );

#endif

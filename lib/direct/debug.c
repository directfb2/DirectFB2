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

#include <direct/debug.h>

#if DIRECT_BUILD_TEXT

#include <direct/conf.h>

#if DIRECT_BUILD_DEBUGS
#include <direct/log.h>
#include <direct/system.h>
#include <direct/thread.h>
#include <direct/trace.h>
#endif /* DIRECT_BUILD_DEBUGS */

#endif /* DIRECT_BUILD_TEXT */

/**********************************************************************************************************************/

#if DIRECT_BUILD_TEXT

__dfb_no_instrument_function__
void
direct_debug_at_always( DirectLogDomain *domain,
                        const char      *format, ... )
{
     if (direct_config->log_level >= DIRECT_LOG_DEBUG) {
          va_list ap;

          va_start( ap, format );
          direct_log_domain_vprintf( domain, DIRECT_LOG_NONE, format, ap );
          va_end( ap );
     }
}

#if DIRECT_BUILD_DEBUGS

__dfb_no_instrument_function__
void
direct_debug_log( DirectLogDomain *domain,
                  unsigned int     debug_level,
                  const char      *format, ... )
{
     va_list ap;

     debug_level += DIRECT_LOG_DEBUG_0;

     va_start( ap, format );
     direct_log_domain_vprintf( domain, debug_level > DIRECT_LOG_DEBUG_9 ? DIRECT_LOG_DEBUG_9 : debug_level, format,
                                ap );
     va_end( ap );
}

__dfb_no_instrument_function__
void
direct_debug_at( DirectLogDomain *domain,
                 const char      *format, ... )
{
     va_list ap;

     va_start( ap, format );
     direct_log_domain_vprintf( domain, DIRECT_LOG_DEBUG, format, ap );
     va_end( ap );
}

__dfb_no_instrument_function__
void
direct_assertion( const char *exp,
                  const char *func,
                  const char *file,
                  int         line )
{
     long long   millis = direct_clock_get_millis();
     const char *name   = direct_thread_self_name();

     direct_log_printf( NULL, "(!) [%-15s %3lld.%03lld] (%5d) *** Assertion [%s] failed *** [%s:%d in %s()]\n",
                        name ?: "  NO NAME  ", millis / 1000LL, millis % 1000LL, direct_gettid(), exp,
                        file, line, func );

     direct_trace_print_stack( NULL );

     if (direct_config->fatal >= DCFL_ASSERT)
          direct_trap( "Assertion", SIGTRAP );
}

__dfb_no_instrument_function__
void
direct_assumption( const char *exp,
                   const char *func,
                   const char *file,
                   int         line )
{
     long long   millis = direct_clock_get_millis();
     const char *name   = direct_thread_self_name();

     direct_log_printf( NULL, "(!) [%-15s %3lld.%03lld] (%5d) *** Assumption [%s] failed *** [%s:%d in %s()]\n",
                        name ?: "  NO NAME  ", millis / 1000LL, millis % 1000LL, direct_gettid(), exp,
                        file, line, func );

     direct_trace_print_stack( NULL );

     if (direct_config->fatal >= DCFL_ASSUME)
          direct_trap( "Assumption", SIGTRAP );
}

#else /* DIRECT_BUILD_DEBUGS */

void
direct_debug_log( DirectLogDomain *domain,
                  unsigned int     debug_level,
                  const char      *format, ... )
{
}

void
direct_debug_at( DirectLogDomain *domain,
                 const char      *format, ... )
{
}

void
direct_assertion( const char *exp,
                  const char *func,
                  const char *file,
                  int         line )
{
}

void
direct_assumption( const char *exp,
                   const char *func,
                   const char *file,
                   int         line )
{
}

#endif /* DIRECT_BUILD_DEBUGS */

#endif /* DIRECT_BUILD_TEXT */

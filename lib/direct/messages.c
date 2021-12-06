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

#include <direct/messages.h>

/**********************************************************************************************************************/

#if DIRECT_BUILD_TEXT

#include <direct/log.h>
#include <direct/mem.h>
#include <direct/result.h>
#include <direct/system.h>
#include <direct/trace.h>
#include <direct/util.h>

__dfb_no_instrument_function__
void
direct_messages_info( const char *format, ... )
{
     char  buf[512];
     char *ptr = buf;
     int   len;

     va_list ap;

     va_start( ap, format );
     len = vsnprintf( buf, sizeof(buf), format, ap );
     va_end( ap );

     if (len < 0)
          return;

     if (len >= sizeof(buf)) {
          ptr = direct_malloc( len + 1 );
          if (!ptr)
               return;

          va_start( ap, format );
          len = vsnprintf( ptr, len + 1, format, ap );
          va_end( ap );
          if (len < 0) {
               direct_free( ptr );
               return;
          }
     }

     direct_log_printf( NULL, "(*) %s", ptr );

     if (ptr != buf)
          direct_free( ptr );
}

__dfb_no_instrument_function__
void
direct_messages_error( const char *format, ... )
{
     char  buf[512];
     char *ptr = buf;
     int   len;

     va_list ap;

     va_start( ap, format );
     len = vsnprintf( buf, sizeof(buf), format, ap );
     va_end( ap );

     if (len < 0)
          return;

     if (len >= sizeof(buf)) {
          ptr = direct_malloc( len + 1 );
          if (!ptr)
               return;

          va_start( ap, format );
          len = vsnprintf( ptr, len + 1, format, ap );
          va_end( ap );
          if (len < 0) {
               direct_free( ptr );
               return;
          }
     }

     direct_log_printf( NULL, "(!) %s", ptr );

     direct_trace_print_stack( NULL );

     if (direct_config->fatal_messages & DMT_ERROR)
          direct_trap( "Error", SIGABRT );

     if (ptr != buf)
          direct_free( ptr );
}

__dfb_no_instrument_function__
void
direct_messages_derror( DirectResult result,
                        const char *format, ... )
{
     char buf[512];

     va_list ap;

     va_start( ap, format );

     vsnprintf( buf, sizeof(buf), format, ap );

     va_end( ap );

     direct_log_printf( NULL, "(!) %s    --> %s\n", buf, DirectResultString( result ) );

     direct_trace_print_stack( NULL );

     if (direct_config->fatal_messages & DMT_ERROR)
          direct_trap( "DError", SIGABRT );
}

__dfb_no_instrument_function__
void
direct_messages_perror( int erno,
                        const char *format, ... )
{
     char buf[512];

     va_list ap;

     va_start( ap, format );

     vsnprintf( buf, sizeof(buf), format, ap );

     va_end( ap );

     direct_log_printf( NULL, "(!) %s    --> %s\n", buf, direct_strerror( erno ) );

     direct_trace_print_stack( NULL );

     if (direct_config->fatal_messages & DMT_ERROR)
          direct_trap( "PError", SIGABRT );
}

__dfb_no_instrument_function__
void
direct_messages_dlerror( const char *dlerr,
                         const char *format, ... )
{
     char buf[512];

     va_list ap;

     va_start( ap, format );

     vsnprintf( buf, sizeof(buf), format, ap );

     va_end( ap );

     direct_log_printf( NULL, "(!) %s    --> %s\n", buf, dlerr );

     direct_trace_print_stack( NULL );

     if (direct_config->fatal_messages & DMT_ERROR)
          direct_trap( "DlError", SIGABRT );
}

__dfb_no_instrument_function__
void
direct_messages_once( const char *func,
                      const char *file,
                      int         line,
                      const char *format, ... )
{
     char buf[512];

     va_list ap;

     va_start( ap, format );

     vsnprintf( buf, sizeof(buf), format, ap );

     va_end( ap );

     direct_log_printf( NULL, "(!) *** ONCE [%s] *** [%s:%d in %s()]\n", buf, file, line, func );

     direct_trace_print_stack( NULL );

     if (direct_config->fatal_messages & DMT_ONCE)
          direct_trap( "Once", SIGABRT );
}

__dfb_no_instrument_function__
void
direct_messages_unimplemented( const char *func,
                               const char *file,
                               int         line )
{
     direct_log_printf( NULL, "(+) *** UNIMPLEMENTED [%s] *** [%s:%d]\n", func, file, line );

     direct_trace_print_stack( NULL );

     if (direct_config->fatal_messages & DMT_UNIMPLEMENTED)
          direct_trap( "Unimplemented", SIGABRT );
}

__dfb_no_instrument_function__
void
direct_messages_bug( const char *func,
                     const char *file,
                     int         line,
                     const char *format, ... )
{
     char buf[512];

     va_list ap;

     va_start( ap, format );

     vsnprintf( buf, sizeof(buf), format, ap );

     va_end( ap );

     direct_log_printf( NULL, "(+) *** BUG [%s] *** [%s:%d in %s()]\n", buf, file, line, func );

     direct_trace_print_stack( NULL );

     if (direct_config->fatal_messages & DMT_BUG)
          direct_trap( "Bug", SIGABRT );
}

__dfb_no_instrument_function__
void
direct_messages_warn( const char *func,
                      const char *file,
                      int         line,
                      const char *format, ... )
{
     char buf[512];

     va_list ap;

     va_start( ap, format );

     vsnprintf( buf, sizeof(buf), format, ap );

     va_end( ap );

     direct_log_printf( NULL, "(#) *** WARNING [%s] *** [%s:%d in %s()]\n", buf, file, line, func );

     direct_trace_print_stack( NULL );

     if (direct_config->fatal_messages & DMT_WARNING)
          direct_trap( "Warning", SIGABRT );
}

__dfb_no_instrument_function__
int
direct_messages_oom( const char *func,
                     const char *file,
                     int         line )
{
     direct_log_printf( NULL, "(=) *** OOM [out of memory] *** [%s:%d in %s()]\n", file, line, func );

     direct_trace_print_stack( NULL );

     if (direct_config->fatal_messages & DMT_OOM)
          direct_trap( "OOM", SIGABRT );

     return DR_NOLOCALMEMORY;
}

__dfb_no_instrument_function__
int
direct_messages_ooshm( const char *func,
                       const char *file,
                       int         line )
{
     direct_log_printf( NULL, "(=) *** OOSHM [out of shared memory] *** [%s:%d in %s()]\n", file, line, func );

     direct_trace_print_stack( NULL );

     if (direct_config->fatal_messages & DMT_OOSHM)
          direct_trap( "OOSHM", SIGABRT );

     return DR_NOSHAREDMEMORY;
}

#else /* DIRECT_BUILD_TEXT */

void
direct_messages_info( const char *format, ... )
{
}

void
direct_messages_error( const char *format, ... )
{
}

void
direct_messages_derror( DirectResult result,
                        const char *format, ... )
{
}

void
direct_messages_perror( int erno,
                        const char *format, ... )
{
}

void
direct_messages_dlerror( const char *dlerr,
                         const char *format, ... )
{
}

void
direct_messages_once( const char *func,
                      const char *file,
                      int         line,
                      const char *format, ... )
{
}

void
direct_messages_unimplemented( const char *func,
                               const char *file,
                               int         line )
{
}

void
direct_messages_bug( const char *func,
                     const char *file,
                     int         line,
                     const char *format, ... )
{
}

void
direct_messages_warn( const char *func,
                      const char *file,
                      int         line,
                      const char *format, ... )
{
}

int
direct_messages_oom( const char *func,
                     const char *file,
                     int         line )
{
     return DR_NOLOCALMEMORY;
}

int
direct_messages_ooshm( const char *func,
                       const char *file,
                       int         line )
{
     return DR_NOSHAREDMEMORY;
}

#endif /* DIRECT_BUILD_TEXT */

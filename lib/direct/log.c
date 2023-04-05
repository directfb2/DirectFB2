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
#include <direct/log.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/thread.h>

/**********************************************************************************************************************/

static DirectLog  fallback_log;
static DirectLog *default_log;

/**********************************************************************************************************************/

void
__D_log_init()
{
     DirectLog *fb = &fallback_log;

     fb->type = DLT_STDERR;

     direct_recursive_mutex_init( &fb->lock );

     direct_log_init( fb, NULL );

     D_MAGIC_SET( fb, DirectLog );
}

void
__D_log_deinit()
{
     DirectLog *fb = &fallback_log;

     direct_log_deinit( fb );

     direct_mutex_deinit( &fb->lock );

     D_MAGIC_CLEAR( fb );

     default_log = NULL;
}

/**********************************************************************************************************************/

DirectResult
direct_log_create( DirectLogType   type,
                   const char     *param,
                   DirectLog     **ret_log )
{
     DirectResult  ret;
     DirectLog    *log;

     log = direct_calloc( 1, sizeof(DirectLog) );
     if (!log)
          return D_OOM();

     log->type = type;

     direct_recursive_mutex_init( &log->lock );

     ret = direct_log_init( log, param );
     if (ret) {
          direct_mutex_deinit( &log->lock );
          direct_free( log );
          return ret;
     }

     D_ASSERT( log->write != NULL );

     D_MAGIC_SET( log, DirectLog );

     *ret_log = log;

     return DR_OK;
}

DirectResult
direct_log_destroy( DirectLog *log )
{
     D_MAGIC_ASSERT( log, DirectLog );

     D_ASSERT( &fallback_log != log );

     if (log == default_log)
          default_log = NULL;

     direct_log_deinit( log );

     direct_mutex_deinit( &log->lock );

     D_MAGIC_CLEAR( log );

     direct_free( log );

     return DR_OK;
}

__dfb_no_instrument_function__
DirectResult
direct_log_printf( DirectLog  *log,
                   const char *format, ... )
{
     DirectResult  ret = 0;
     va_list       args;
     int           len;
     char          buf[2000];
     char         *ptr = buf;

     /* Don't use D_MAGIC_ASSERT or any other macros/functions that might cause an endless loop. */

     /* Use the default log if passed log is invalid. */
     if (!D_MAGIC_CHECK( log, DirectLog ))
          log = direct_log_default();

     if (!D_MAGIC_CHECK( log, DirectLog ))
          return DR_BUG;

     va_start( args, format );
     len = vsnprintf( buf, sizeof(buf), format, args );
     va_end( args );

     if (len < 0)
          return DR_FAILURE;

     if (len >= sizeof(buf)) {
          ptr = direct_malloc( len + 1 );
          if (!ptr)
               return DR_NOLOCALMEMORY;

          va_start( args, format );
          len = vsnprintf( ptr, len + 1, format, args );
          va_end( args );

          if (len < 0) {
               direct_free( ptr );
               return DR_FAILURE;
          }
     }

     ret = log->write( log, ptr, len );

     if (ptr != buf)
          direct_free( ptr );

     direct_log_debug_delay( true );

     return ret;
}

DirectResult
direct_log_set_default( DirectLog *log )
{
     D_MAGIC_ASSERT_IF( log, DirectLog );

     default_log = log;

     return DR_OK;
}

__dfb_no_instrument_function__
void
direct_log_lock( DirectLog *log )
{
     D_MAGIC_ASSERT( log, DirectLog );

     direct_mutex_lock( &log->lock );
}

__dfb_no_instrument_function__
void
direct_log_unlock( DirectLog *log )
{
     D_MAGIC_ASSERT( log, DirectLog );

     direct_mutex_unlock( &log->lock );
}

DirectResult
direct_log_flush( DirectLog *log )
{
     /* Don't use D_MAGIC_ASSERT or any other macros/functions that might cause an endless loop. */

     /* Use the default log if passed log is invalid. */
     if (!D_MAGIC_CHECK( log, DirectLog ))
          log = direct_log_default();

     if (!D_MAGIC_CHECK( log, DirectLog ))
          return DR_BUG;

     if (!log->flush)
          return DR_UNSUPPORTED;

     return log->flush( log );
}

__dfb_no_instrument_function__
DirectLog *
direct_log_default()
{
     return default_log ?: &fallback_log;
}

__dfb_no_instrument_function__
void
direct_log_debug_delay( bool min )
{
     unsigned int us = min ? direct_config->log_delay_min_us : 0;

     if (direct_config->log_delay_rand_us) {
          unsigned int r = rand() % direct_config->log_delay_rand_us;

          if (us < r)
               us = r;
     }

     if (us)
          direct_thread_sleep( us );


     unsigned int loops = min ? direct_config->log_delay_min_loops : 0;

     if (direct_config->log_delay_rand_loops) {
          unsigned int r = rand() % direct_config->log_delay_rand_loops;

          if (loops < r)
               loops = r;
     }

     if (loops) {
          volatile long val = 0;
          unsigned int  loop;

          for (loop = 0; loop < loops; loop++)
               val++;
     }
}

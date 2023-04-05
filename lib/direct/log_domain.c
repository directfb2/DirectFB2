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

#include <direct/log_domain.h>

#if DIRECT_BUILD_TEXT
#include <direct/list.h>
#include <direct/log.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/system.h>
#include <direct/thread.h>
#include <direct/trace.h>
#endif /* DIRECT_BUILD_TEXT */

/**********************************************************************************************************************/

#if DIRECT_BUILD_TEXT

typedef struct {
     DirectLink             link;

     char                  *name;
     DirectLogDomainConfig  config;
} LogDomainEntry;

static DirectMutex   domains_lock;
static unsigned int  domains_age;
static DirectLink   *domains;

#endif /* DIRECT_BUILD_TEXT */

/**********************************************************************************************************************/

#if DIRECT_BUILD_TEXT

void
__D_log_domain_init()
{
     domains_age = 1;

     direct_mutex_init( &domains_lock );
}

void
__D_log_domain_deinit()
{
     direct_mutex_deinit( &domains_lock );
}

#else /* DIRECT_BUILD_TEXT */

void
__D_log_domain_init()
{
}

void
__D_log_domain_deinit()
{
}

#endif /* DIRECT_BUILD_TEXT */

/**********************************************************************************************************************/

#if DIRECT_BUILD_TEXT

__dfb_no_instrument_function__
static __inline__ LogDomainEntry *
lookup_domain( const char *name, bool sub )
{
     LogDomainEntry *entry;

     direct_list_foreach (entry, domains) {
          if (!strcasecmp( entry->name, name ))
               return entry;
     }

     /* If the domain being registered contains a slash, but didn't exactly match an entry in directfbrc,
        check to see if the domain is descended from an entry in directfbrc. */
     if (sub && strchr(name, '/')) {
          int passed_name_len = strlen( name );

          direct_list_foreach (entry, domains) {
               int entry_len = strlen( entry->name );
               if ((passed_name_len > entry_len) && (name[entry_len] == '/') &&
                   (!strncasecmp( entry->name, name, entry_len ))) {
                    return entry;
               }
          }
     }

     return NULL;
}

__dfb_no_instrument_function__
static DirectLogLevel
check_domain( DirectLogDomain *domain )
{
     if (direct_config->log_none)
          return DIRECT_LOG_NONE;

     if (direct_config->log_all)
          return DIRECT_LOG_ALL;

     if (domain->age != domains_age) {
          LogDomainEntry *entry;

          if (direct_mutex_lock( &domains_lock ))
               return DIRECT_LOG_ALL;

          entry = lookup_domain( domain->name, true );

          domain->age = domains_age;

          if (entry) {
               domain->registered = true;
               domain->config     = entry->config;
          }
          else {
               domain->config.level = direct_config->log_level;
          }

          direct_mutex_unlock( &domains_lock );
     }

     return domain->config.level;
}

void
direct_log_domain_configure( const char *name, const DirectLogDomainConfig *config )
{
     LogDomainEntry *entry;

     if (direct_mutex_lock( &domains_lock ))
          return;

     entry = lookup_domain( name, false );
     if (!entry) {
          direct_mutex_unlock( &domains_lock );

          entry = (LogDomainEntry*) direct_calloc( 1, sizeof(LogDomainEntry) );
          if (!entry) {
               return;
          }

          entry->name = direct_strdup( name );
          if (!entry->name) {
               direct_free( entry );
               return;
          }

          if (direct_mutex_lock( &domains_lock )) {
               direct_free( entry->name );
               direct_free( entry );
               return;
          }

          direct_list_prepend( &domains, &entry->link );
     }

     entry->config = *config;

     if (!++domains_age)
          domains_age++;

     direct_mutex_unlock( &domains_lock );
}

bool
direct_log_domain_check( DirectLogDomain *domain )
{
     return check_domain( domain ) >= DIRECT_LOG_DEBUG;
}

bool
direct_log_domain_check_level( DirectLogDomain *domain,
                               DirectLogLevel   level )
{
     return check_domain( domain ) >= level;
}

__dfb_no_instrument_function__
DirectResult
direct_log_domain_vprintf( DirectLogDomain *domain,
                           DirectLogLevel   level,
                           const char      *format,
                           va_list          ap )
{
     if (check_domain( domain ) >= level) {
          char       buf[200];
          char      *ptr = buf;
          long long  micros = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );
          long long  millis = micros / 1000LL;
          int        indent = direct_trace_debug_indent() * 4;
          int        len;
          va_list    ap2;

          /* Prepare user message. */
          va_copy( ap2, ap );
          len = vsnprintf( buf, sizeof(buf), format, ap2 );
          va_end( ap2 );

          if (len < 0)
               return DR_FAILURE;

          if (len >= sizeof(buf)) {
               ptr = direct_malloc( len + 1 );
               if (!ptr)
                    return DR_NOLOCALMEMORY;

               len = vsnprintf( ptr, len + 1, format, ap );
               if (len < 0) {
                    direct_free( ptr );
                    return DR_FAILURE;
               }
          }

          /* Wrap around when too high. */
          indent &= 0x7f;

          /* Fill up domain name column after the colon, prepending remaining space (excl. ': ') to indent. */
          indent += (domain->name_len < 34 ? 34 : 50) - domain->name_len - 2;

          /* Print full message. */
          direct_log_printf( domain->config.log, "(-) [%-16.16s %3lld.%03lld,%03lld] (%5d) %s: %*s%s",
                             direct_thread_self_name() ?: "  NO NAME",
                             millis / 1000LL, millis % 1000LL, micros % 1000LL,
                             direct_gettid(), domain->name, indent, "", ptr );

          direct_log_flush( domain->config.log );

          if (ptr != buf)
               direct_free( ptr );
     }
     else
          direct_log_debug_delay( false );

     return DR_OK;
}

__dfb_no_instrument_function__
DirectResult
direct_log_domain_log( DirectLogDomain *domain,
                       DirectLogLevel   level,
                       const char      *func,
                       const char      *file,
                       int              line,
                       const char      *format, ... )
{
     if (check_domain( domain ) >= level) {
          char          buf[200];
          char         *ptr = buf;
          long long     micros = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );
          long long     millis = micros / 1000LL;
          int           indent = direct_trace_debug_indent() * 4;
          int           len;
          va_list       ap;

          /* Prepare user message. */
          va_start( ap, format );
          len = vsnprintf( buf, sizeof(buf), format, ap );
          va_end( ap );

          if (len < 0)
               return DR_FAILURE;

          if (len >= sizeof(buf)) {
               ptr = direct_malloc( len + 1 );
               if (!ptr)
                    return DR_NOLOCALMEMORY;

               va_start( ap, format );
               len = vsnprintf( ptr, len + 1, format, ap );
               va_end( ap );

               if (len < 0) {
                    direct_free( ptr );
                    return DR_FAILURE;
               }
          }

          /* Wrap around when too high. */
          indent &= 0x7f;

          /* Fill up domain name column after the colon, prepending remaining space (excl. ': ') to indent. */
          indent += (domain->name_len < 27 ? 27 : 42) - domain->name_len - 2;

          /* Print full message. */
          direct_log_printf( domain->config.log,
                             "(%c) [%-16.16s %3lld.%03lld,%03lld] (%5d) %s: %*s [%s:%d in %s()] %s",
                             level > DIRECT_LOG_INFO    ? '-' :
                             level > DIRECT_LOG_NOTICE  ? '*' :
                             level > DIRECT_LOG_WARNING ? '+' :
                             level > DIRECT_LOG_ERROR   ? '#' :
                             level > DIRECT_LOG_FATAL   ? '!' :
                             level > DIRECT_LOG_NONE    ? '=' : ' ',
                             direct_thread_self_name() ?: "  NO NAME",
                             millis / 1000LL, millis % 1000LL, micros % 1000LL,
                             direct_gettid(), domain->name, indent, "", file, line, func, ptr );

          direct_log_flush( domain->config.log );

          if (ptr != buf)
               direct_free( ptr );
     }
     else
          direct_log_debug_delay( false );

     return DR_OK;
}

#else /* DIRECT_BUILD_TEXT */

void
direct_log_domain_configure( const char                  *name,
                             const DirectLogDomainConfig *config )
{
}

bool
direct_log_domain_check( DirectLogDomain *domain )
{
     return false;
}

bool
direct_log_domain_check_level( DirectLogDomain *domain,
                               DirectLogLevel   level )
{
     return false;
}

DirectResult
direct_log_domain_vprintf( DirectLogDomain *domain,
                           DirectLogLevel   level,
                           const char      *format,
                           va_list          ap )
{
     return DR_OK;
}

DirectResult
direct_log_domain_log( DirectLogDomain *domain,
                       DirectLogLevel   level,
                       const char      *func,
                       const char      *file,
                       int              line,
                       const char      *format, ... )
{
     return DR_OK;
}

#endif /* DIRECT_BUILD_TEXT */

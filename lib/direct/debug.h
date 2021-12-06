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

#ifndef __DIRECT__DEBUG_H__
#define __DIRECT__DEBUG_H__

#include <direct/log_domain.h>

#if DIRECT_BUILD_TEXT

#if !DIRECT_BUILD_DEBUGS
#ifdef DIRECT_ENABLE_DEBUG
#define DIRECT_MINI_DEBUG
#endif
#undef DIRECT_ENABLE_DEBUG
#endif /* DIRECT_BUILD_DEBUGS */

#ifdef DIRECT_MINI_DEBUG
#include <direct/conf.h>
#include <direct/log.h>
#include <direct/system.h>
#include <direct/trace.h>
#include <direct/thread.h>
#endif /* DIRECT_MINI_DEBUG */

#endif /* DIRECT_BUILD_TEXT */

/**********************************************************************************************************************/

#define D_DEBUG_DOMAIN(_identifier,_name,_description)                             \
     static DirectLogDomain _identifier D_UNUSED = {                               \
          _description, _name, sizeof(_name) - 1, 0, false, { DIRECT_LOG_NONE, 0 } \
     }

static __inline__ void
direct_debug_config_domain( const char *name,
                            bool        enable )
{
     direct_log_domain_config_level( name, enable ? DIRECT_LOG_ALL : DIRECT_LOG_NONE );
}

#if DIRECT_BUILD_TEXT

void DIRECT_API direct_debug_at_always( DirectLogDomain *domain,
                                        const char      *format, ... ) D_FORMAT_PRINTF(2);

/*
 * debug level 1-9 (0 = verbose)
 */
void DIRECT_API direct_debug_log      ( DirectLogDomain *domain,
                                        unsigned int     debug_level,
                                        const char      *format, ... ) D_FORMAT_PRINTF(3);

void DIRECT_API direct_debug_at       ( DirectLogDomain *domain,
                                        const char      *format, ... ) D_FORMAT_PRINTF(2);

void DIRECT_API direct_assertion      ( const char      *exp,
                                        const char      *func,
                                        const char      *file,
                                        int              line );

void DIRECT_API direct_assumption     ( const char      *exp,
                                        const char      *func,
                                        const char      *file,
                                        int              line );

#if DIRECT_BUILD_DEBUG || defined(DIRECT_ENABLE_DEBUG)

#define D_DEBUG_ENABLED 1

#define D_DEBUG_LOG(d,l,...)                                                                                      \
     do {                                                                                                         \
          direct_debug_log( &d, l, __VA_ARGS__ );                                                                 \
     } while (0)

#define D_DEBUG_AT(d,...)                                                                                         \
     do {                                                                                                         \
          direct_debug_at( &d, __VA_ARGS__ );                                                                     \
     } while (0)

#define D_ASSERT(exp)                                                                                             \
     do {                                                                                                         \
          if (!(exp))                                                                                             \
               direct_assertion( #exp, __FUNCTION__, __FILE__, __LINE__ );                                        \
     } while (0)

#define D_ASSUME(exp)                                                                                             \
     do {                                                                                                         \
          if (!(exp))                                                                                             \
               direct_assumption( #exp, __FUNCTION__, __FILE__, __LINE__ );                                       \
     } while (0)

#elif defined(DIRECT_MINI_DEBUG)

/*
 * mini debug mode
 */

#define D_DEBUG_ENABLED 2

#define D_DEBUG_LOG(d,l,...)                                                                                      \
     do {                                                                                                         \
          if (direct_config->log_level >= DIRECT_LOG_DEBUG)                                                       \
               direct_debug_at_always( &d, __VA_ARGS__ );                                                         \
     } while (0)

#define D_DEBUG_AT(d,...)                                                                                         \
     do {                                                                                                         \
          if (direct_config->log_level >= DIRECT_LOG_DEBUG)                                                       \
               direct_debug_at_always( &d, __VA_ARGS__ );                                                         \
     } while (0)

#define D_CHECK(exp,aa)                                                                                           \
     do {                                                                                                         \
          if (!(exp)) {                                                                                           \
               long long   millis = direct_clock_get_millis();                                                    \
               const char *name   = direct_thread_self_name();                                                    \
                                                                                                                  \
               direct_log_printf( NULL,                                                                           \
                                  "(!) [%-15s %3lld.%03lld] (%5d) *** " #aa " [%s] failed *** [%s:%d in %s()]\n", \
                                  name ?: "  NO NAME  ", millis / 1000LL, millis % 1000LL, direct_gettid(), #exp, \
                                  __FILE__, __LINE__, __FUNCTION__ );                                             \
                                                                                                                  \
               direct_trace_print_stack( NULL );                                                                  \
          }                                                                                                       \
     } while (0)

#define D_ASSERT(exp)                                                                                             \
     D_CHECK( exp, Assertion )

#define D_ASSUME(exp)                                                                                             \
     D_CHECK( exp, Assumption )

#endif

#endif /* DIRECT_BUILD_TEXT */

/*
 * Fallback definitions, e.g. without DIRECT_BUILD_TEXT or DIRECT_BUILD_DEBUG or DIRECT_ENABLE_DEBUG
 */

#ifndef D_DEBUG_ENABLED
#define D_DEBUG_ENABLED 0
#endif

#ifndef D_DEBUG_LOG
#define D_DEBUG_LOG(d,l,...)    do {} while (0)
#endif

#ifndef D_DEBUG_AT
#define D_DEBUG_AT(d,...)       do {} while (0)
#endif

#ifndef D_ASSERT
#define D_ASSERT(exp)           do {} while (0)
#endif

#ifndef D_ASSUME
#define D_ASSUME(exp)           do {} while (0)
#endif

/*
 * Magic Assertions & Utilities
 */

#if DIRECT_BUILD_DEBUGS

#define D_MAGIC(spell)          ( ( ((spell)[sizeof(spell)*8/9] << 24) |         \
                                    ((spell)[sizeof(spell)*7/9] << 16) |         \
                                    ((spell)[sizeof(spell)*6/9] <<  8) |         \
                                    ((spell)[sizeof(spell)*5/9]      )   ) ^     \
                                  ( ((spell)[sizeof(spell)*4/9] << 24) |         \
                                    ((spell)[sizeof(spell)*3/9] << 16) |         \
                                    ((spell)[sizeof(spell)*2/9] <<  8) |         \
                                    ((spell)[sizeof(spell)*1/9]      )   ) )

#define D_MAGIC_CHECK(o,m)      ((o) != NULL && (o)->magic == D_MAGIC(#m))

#define D_MAGIC_SET(o,m)        do {                                             \
                                     D_ASSERT( (o) != NULL );                    \
                                     D_ASSUME( (o)->magic != D_MAGIC(#m) );      \
                                                                                 \
                                     (o)->magic = D_MAGIC(#m);                   \
                                } while (0)

#define D_MAGIC_SET_ONLY(o,m)   do {                                             \
                                     D_ASSERT( (o) != NULL );                    \
                                                                                 \
                                     (o)->magic = D_MAGIC(#m);                   \
                                } while (0)

#define D_MAGIC_ASSERT(o,m)     do {                                             \
                                     D_ASSERT( (o) != NULL );                    \
                                     D_ASSERT( (o)->magic == D_MAGIC(#m) );      \
                                } while (0)

#define D_MAGIC_ASSUME(o,m)     do {                                             \
                                     D_ASSUME( (o) != NULL );                    \
                                     if (o)                                      \
                                          D_ASSUME( (o)->magic == D_MAGIC(#m) ); \
                                } while (0)

#define D_MAGIC_ASSERT_IF(o,m)  do {                                             \
                                     if (o)                                      \
                                          D_ASSERT( (o)->magic == D_MAGIC(#m) ); \
                                } while (0)

#define D_MAGIC_CLEAR(o)        do {                                             \
                                     D_ASSERT( (o) != NULL );                    \
                                     D_ASSUME( (o)->magic != 0 );                \
                                                                                 \
                                     (o)->magic = 0;                             \
                                } while (0)

#else /* DIRECT_BUILD_DEBUGS */

#define D_MAGIC_CHECK(o,m)      ((o) != NULL)

#define D_MAGIC_SET(o,m)        do {} while (0)

#define D_MAGIC_SET_ONLY(o,m)   do {} while (0)

#define D_MAGIC_ASSERT(o,m)     do {} while (0)

#define D_MAGIC_ASSUME(o,m)     do {} while (0)

#define D_MAGIC_ASSERT_IF(o,m)  do {} while (0)

#define D_MAGIC_CLEAR(o)        do {} while (0)

#endif /* DIRECT_BUILD_DEBUGS */

#define D_FLAGS_ASSERT(flags,f) D_ASSERT( D_FLAGS_ARE_IN( flags, f ) )

#endif

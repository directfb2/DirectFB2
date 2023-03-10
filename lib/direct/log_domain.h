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

#ifndef __DIRECT__LOG_DOMAIN_H__
#define __DIRECT__LOG_DOMAIN_H__

#include <direct/compiler.h>
#include <direct/types.h>

/**********************************************************************************************************************/

typedef enum {
     DIRECT_LOG_NONE    = 0x00000000,
     DIRECT_LOG_FATAL   = 0x00000001,
     DIRECT_LOG_ERROR   = 0x00000002,
     DIRECT_LOG_WARNING = 0x00000003,
     DIRECT_LOG_NOTICE  = 0x00000004,
     DIRECT_LOG_INFO    = 0x00000005,
     DIRECT_LOG_VERBOSE = 0x00000006,

     DIRECT_LOG_DEBUG_0 = DIRECT_LOG_VERBOSE,

     DIRECT_LOG_DEBUG_1 = 0x00000007,
     DIRECT_LOG_DEBUG_2 = 0x00000008,
     DIRECT_LOG_DEBUG_3 = 0x00000009,
     DIRECT_LOG_DEBUG_4 = 0x0000000A,
     DIRECT_LOG_DEBUG_5 = 0x0000000B,
     DIRECT_LOG_DEBUG_6 = 0x0000000C,
     DIRECT_LOG_DEBUG_7 = 0x0000000D,
     DIRECT_LOG_DEBUG_8 = 0x0000000E,
     DIRECT_LOG_DEBUG_9 = 0x0000000F,

     DIRECT_LOG_ALL     = 0x00000010,

     DIRECT_LOG_DEBUG   = DIRECT_LOG_DEBUG_8, /* Default debug level */
} DirectLogLevel;

typedef struct {
     DirectLogLevel  level;
     DirectLog      *log;
} DirectLogDomainConfig;

typedef struct {
     const char            *description;
     const char            *name;
     int                    name_len;

     unsigned int           age;
     bool                   registered;

     DirectLogDomainConfig  config;
} DirectLogDomain;

/**********************************************************************************************************************/

void         DIRECT_API direct_log_domain_configure  ( const char                  *name,
                                                       const DirectLogDomainConfig *config );

bool         DIRECT_API direct_log_domain_check      ( DirectLogDomain             *domain );

bool         DIRECT_API direct_log_domain_check_level( DirectLogDomain             *domain,
                                                       DirectLogLevel               level );

DirectResult DIRECT_API direct_log_domain_vprintf    ( DirectLogDomain             *domain,
                                                       DirectLogLevel               level,
                                                       const char                  *format,
                                                       va_list                      ap )          D_FORMAT_VPRINTF(3);

DirectResult DIRECT_API direct_log_domain_log        ( DirectLogDomain             *domain,
                                                       DirectLogLevel               level,
                                                       const char                  *func,
                                                       const char                  *file,
                                                       int                          line,
                                                       const char                  *format, ... ) D_FORMAT_PRINTF(6);

/**********************************************************************************************************************/

static __inline__ void
direct_log_domain_config_level( const char     *name,
                                DirectLogLevel  level )
{
     DirectLogDomainConfig config;

     config.level = level;
     config.log   = NULL;

     direct_log_domain_configure( name, &config );
}

#define D_LOG(d,l,...)                                                                                  \
     do {                                                                                               \
          direct_log_domain_log( &(d), DIRECT_LOG_##l, __FUNCTION__, __FILE__, __LINE__, __VA_ARGS__ ); \
     } while (0)

/**********************************************************************************************************************/

void __D_log_domain_init  ( void );
void __D_log_domain_deinit( void );

#endif

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

#ifndef __DIRECT__OS__SYSTEM_H__
#define __DIRECT__OS__SYSTEM_H__

#include <direct/os/types.h>

/**********************************************************************************************************************/

void          DIRECT_API  direct_sched_yield( void );

long          DIRECT_API  direct_pagesize   ( void );

unsigned long DIRECT_API  direct_page_align ( unsigned long          value );

pid_t         DIRECT_API  direct_getpid     ( void );

pid_t         DIRECT_API  direct_gettid     ( void );

void          DIRECT_API  direct_trap       ( const char            *domain,
                                              int                    sig );

DirectResult  DIRECT_API  direct_kill       ( pid_t                  pid,
                                              int                    sig );

void          DIRECT_API  direct_sync       ( void );

DirectResult  DIRECT_API  direct_sigprocmask( int                    how,
                                              const sigset_t        *set,
                                              sigset_t              *oset );

uid_t         DIRECT_API  direct_geteuid    ( void );

char          DIRECT_API *direct_getenv     ( const char            *name );

DirectResult  DIRECT_API  direct_futex      ( int                   *uaddr,
                                              int                    op,
                                              int                    val,
                                              const struct timespec *timeout,
                                              int                   *uaddr2,
                                              int                    val3 );

bool          DIRECT_API  direct_madvise    ( void );

/**********************************************************************************************************************/

#define FUTEX_WAIT 0
#define FUTEX_WAKE 1

extern unsigned int DIRECT_API __Direct_Futex_Wait_Count;
extern unsigned int DIRECT_API __Direct_Futex_Wake_Count;

#endif

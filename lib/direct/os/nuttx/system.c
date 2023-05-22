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

#include <direct/os/system.h>
#include <direct/util.h>

/**********************************************************************************************************************/

__attribute__((no_instrument_function))
void
direct_sched_yield()
{
     sched_yield();
}

long
direct_pagesize()
{
     return sysconf( _SC_PAGESIZE );
}

unsigned long
direct_page_align( unsigned long value )
{
     unsigned long mask = sysconf( _SC_PAGESIZE ) - 1;

     return (value + mask) & ~mask;
}

__attribute__((no_instrument_function))
pid_t
direct_getpid()
{
     return getpid();
}

__attribute__((no_instrument_function))
pid_t
direct_gettid()
{
     return gettid();
}

void
direct_trap( const char *domain,
             int         sig )
{
     exit(0);
}

DirectResult
direct_kill( pid_t pid,
             int   sig )
{
     if (kill( pid, sig ) < 0) {
          if (errno == ESRCH)
               return DR_NOSUCHINSTANCE;
          else
               return errno2result( errno );
     }

     return DR_OK;
}

void
direct_sync()
{
}

uid_t
direct_geteuid()
{
     return geteuid();
}

char *
direct_getenv( const char *name )
{
     return getenv( name );
}

DirectResult
direct_futex( int                   *uaddr,
              int                    op,
              int                    val,
              const struct timespec *timeout,
              int                   *uaddr2,
              int                    val3 )
{
     return DR_UNIMPLEMENTED;
}

bool
direct_madvise()
{
     return false;
}

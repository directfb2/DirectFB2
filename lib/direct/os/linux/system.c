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

#include <direct/conf.h>
#include <direct/debug.h>
#if D_DEBUG_ENABLED
#include <direct/atomic.h>
#endif
#include <direct/messages.h>
#include <direct/os/system.h>
#include <direct/os/thread.h>

D_DEBUG_DOMAIN( Direct_Futex, "Direct/Futex", "Direct Futex" );
D_DEBUG_DOMAIN( Direct_Trap,  "Direct/Trap",  "Direct Trap" );

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
     return syscall( __NR_gettid );
}

void
direct_trap( const char *domain,
             int         sig )
{
     sigval_t val;

     if (direct_config->delay_trap_ms) {
          D_DEBUG_AT( Direct_Trap, "Not raising signal %d from %s, waiting for %dms... attach gdb --pid=%d\n",
                      sig, domain, direct_config->delay_trap_ms, getpid() );
          direct_thread_sleep( direct_config->delay_trap_ms * 1000LL );
          return;
     }

     D_DEBUG_AT( Direct_Trap, "Raising signal %d from %s...\n", sig, domain );

     val.sival_int = direct_gettid();

     sigqueue( direct_gettid(), sig, val );

     abort();

     D_DEBUG_AT( Direct_Trap, "...abort() returned as well, calling exit_group()\n" );

     syscall( __NR_exit_group, DR_BUG );
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
     sync();
}

DirectResult
direct_sigprocmask( int             how,
                    const sigset_t *set,
                    sigset_t       *oset )
{
     int erno = pthread_sigmask( how, set, oset );

     return errno2result( erno );
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
#if D_DEBUG_ENABLED
     unsigned int count;

     switch (op) {
          case FUTEX_WAIT:
               count = D_SYNC_ADD_AND_FETCH( &__Direct_Futex_Wait_Count, 1 );
               D_DEBUG_AT( Direct_Futex, "## ## WAIT FOR --> %p <--  %d (<-%d) ## ## ## ## * %u\n", uaddr, *uaddr, val, count );
               break;

          case FUTEX_WAKE:
               count = D_SYNC_ADD_AND_FETCH( &__Direct_Futex_Wake_Count, 1 );
               D_DEBUG_AT( Direct_Futex, "###   WAKE UP =--> %p <--= %d (->%d) ### ### ### * %u\n", uaddr, *uaddr, val, count );
               break;

          default:
               D_DEBUG_AT( Direct_Futex, "# #  UNKNOWN FUTEX OP  # #\n" );
     }
#endif

     if (syscall( __NR_futex, uaddr, op, val, timeout, uaddr2, val3 ) < 0)
          return errno2result( errno );

     return DR_OK;
}

bool
direct_madvise()
{
     struct utsname uts;
     int            i, j, k, l;

     if (uname( &uts ) < 0) {
          D_PERROR( "Direct/System: uname() failed!\n" );
          return false;
     }

     switch (sscanf( uts.release, "%d.%d.%d.%d", &i, &j, &k, &l )) {
          case 3:
               l = 0;
          case 4:
               if (((i << 24) | (j << 16) | (k << 8) | l) >= 0x02061302)
                    return true;
               break;

          default:
               D_WARN( "could not parse kernel version '%s'", uts.release );
     }

     return false;
}

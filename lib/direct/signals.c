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

#include <direct/interface.h>
#include <direct/list.h>
#include <direct/signals.h>
#include <direct/system.h>
#include <direct/thread.h>
#include <direct/trace.h>

D_DEBUG_DOMAIN( Direct_Signals, "Direct/Signals", "Direct Signals handling" );

/**********************************************************************************************************************/

struct __D_DirectSignalHandler {
     DirectLink               link;

     int                      magic;

     int                      num;
     DirectSignalHandlerFunc  func;
     void                    *ctx;

     bool                     removed;
};

/**********************************************************************************************************************/

typedef struct {
     int              signum;
     struct sigaction old_action;
     struct sigaction new_action;
} SigHandled;

static int sigs_to_handle[] = {
     SIGHUP, SIGINT, SIGQUIT, SIGILL, SIGTRAP, SIGABRT, SIGBUS, SIGFPE, SIGSEGV, SIGPIPE, SIGTERM, SIGXCPU, SIGXFSZ,
     SIGSYS
};

#define NUM_SIGS_TO_HANDLE (D_ARRAY_SIZE(sigs_to_handle))

static SigHandled    sigs_handled[NUM_SIGS_TO_HANDLE];

static DirectLink   *handlers = NULL;
static DirectMutex   handlers_lock;

static DirectThread *sighandler_thread = NULL;

static void *handle_signals( DirectThread *thread, void *arg );

static void install_handlers( void );
static void remove_handlers ( void );

/**********************************************************************************************************************/

DirectResult
direct_signals_initialize()
{
     sigset_t mask;
     int      i;

     D_DEBUG_AT( Direct_Signals, "%s() initializing...\n", __FUNCTION__ );

     direct_recursive_mutex_init( &handlers_lock );
     if (direct_config->sighandler) {
          if (direct_config->sighandler_thread) {
               sigemptyset( &mask );
               for (i = 0; i < NUM_SIGS_TO_HANDLE; i++)
                    sigaddset( &mask, sigs_to_handle[i] );

               direct_sigprocmask( SIG_BLOCK, &mask, NULL );

               sighandler_thread = direct_thread_create( DTT_CRITICAL, handle_signals, NULL, "SigHandler" );

               D_ASSERT( sighandler_thread != NULL );
          }
          else
               install_handlers();
     }

     return DR_OK;
}

DirectResult
direct_signals_shutdown()
{
     D_DEBUG_AT( Direct_Signals, "%s() shutting down...\n", __FUNCTION__ );

     if (direct_config->sighandler_thread) {
          if (sighandler_thread) {
               direct_thread_kill( sighandler_thread, SIGSYS );
               direct_thread_join( sighandler_thread );
               direct_thread_destroy( sighandler_thread );
               sighandler_thread = NULL;
          }
     }
     else
          remove_handlers();

     direct_mutex_deinit( &handlers_lock );

     return DR_OK;
}

void
direct_signals_block_all()
{
     sigset_t signals;

     D_DEBUG_AT( Direct_Signals, "Blocking all signals from now on\n" );

     sigfillset( &signals );

     direct_sigprocmask( SIG_BLOCK, &signals, NULL );
}

DirectResult
direct_signal_handler_add( int                       num,
                           DirectSignalHandlerFunc   func,
                           void                     *ctx,
                           DirectSignalHandler     **ret_handler )
{
     DirectSignalHandler *handler;

     D_ASSERT( func != NULL );
     D_ASSERT( ret_handler != NULL );

     D_DEBUG_AT( Direct_Signals, "Adding handler %p for signal %d with context %p...\n", func, num, ctx );

     handler = D_CALLOC( 1, sizeof(DirectSignalHandler) );
     if (!handler) {
          return DR_NOLOCALMEMORY;
     }

     handler->num  = num;
     handler->func = func;
     handler->ctx  = ctx;

     D_MAGIC_SET( handler, DirectSignalHandler );

     direct_mutex_lock( &handlers_lock );
     direct_list_append( &handlers, &handler->link );
     direct_mutex_unlock( &handlers_lock );

     *ret_handler = handler;

     return DR_OK;
}

DirectResult
direct_signal_handler_remove( DirectSignalHandler *handler )
{
     D_MAGIC_ASSERT( handler, DirectSignalHandler );

     D_DEBUG_AT( Direct_Signals, "Removing handler %p for signal %d with context %p...\n",
                 handler->func, handler->num, handler->ctx );

     /* Mark the handler for removal, actually freeing it will happen later */
     handler->removed = true;

     return DR_OK;
}

/**********************************************************************************************************************/

static bool
show_segv( const siginfo_t *info )
{
     switch (info->si_code) {
          case SEGV_MAPERR:
               D_LOG( Direct_Signals, FATAL, "  --> Caught signal %d (at %p, invalid address) <--\n",
                      info->si_signo, info->si_addr );
               return true;
          case SEGV_ACCERR:
               D_LOG( Direct_Signals, FATAL, "  --> Caught signal %d (at %p, invalid permissions) <--\n",
                      info->si_signo, info->si_addr );
               return true;
     }
     return false;
}

static bool
show_bus( const siginfo_t *info )
{
     switch (info->si_code) {
          case BUS_ADRALN:
               D_LOG( Direct_Signals, FATAL, "  --> Caught signal %d (at %p, invalid address alignment) <--\n",
                      info->si_signo, info->si_addr );
               return true;
          case BUS_ADRERR:
               D_LOG( Direct_Signals, FATAL, "  --> Caught signal %d (at %p, non-existent physical address) <--\n",
                      info->si_signo, info->si_addr );
               return true;
          case BUS_OBJERR:
               D_LOG( Direct_Signals, FATAL, "  --> Caught signal %d (at %p, object specific hardware error) <--\n",
                      info->si_signo, info->si_addr );
               return true;
     }

     return false;
}

static bool
show_ill( const siginfo_t *info )
{
     switch (info->si_code) {
          case ILL_ILLOPC:
               D_LOG( Direct_Signals, FATAL, "  --> Caught signal %d (at %p, illegal opcode) <--\n",
                      info->si_signo, info->si_addr );
               return true;
          case ILL_ILLOPN:
               D_LOG( Direct_Signals, FATAL, "  --> Caught signal %d (at %p, illegal operand) <--\n",
                      info->si_signo, info->si_addr );
               return true;
          case ILL_ILLADR:
               D_LOG( Direct_Signals, FATAL, "  --> Caught signal %d (at %p, illegal addressing mode) <--\n",
                      info->si_signo, info->si_addr );
               return true;
          case ILL_ILLTRP:
               D_LOG( Direct_Signals, FATAL, "  --> Caught signal %d (at %p, illegal trap) <--\n",
                      info->si_signo, info->si_addr );
               return true;
          case ILL_PRVOPC:
               D_LOG( Direct_Signals, FATAL, "  --> Caught signal %d (at %p, privileged opcode) <--\n",
                      info->si_signo, info->si_addr );
               return true;
          case ILL_PRVREG:
               D_LOG( Direct_Signals, FATAL, "  --> Caught signal %d (at %p, privileged register) <--\n",
                      info->si_signo, info->si_addr );
               return true;
          case ILL_COPROC:
               D_LOG( Direct_Signals, FATAL, "  --> Caught signal %d (at %p, coprocessor error) <--\n",
                      info->si_signo, info->si_addr );
               return true;
          case ILL_BADSTK:
               D_LOG( Direct_Signals, FATAL, "  --> Caught signal %d (at %p, internal stack error) <--\n",
                      info->si_signo, info->si_addr );
               return true;
     }

     return false;
}

static bool
show_fpe( const siginfo_t *info )
{
     switch (info->si_code) {
          case FPE_INTDIV:
               D_LOG( Direct_Signals, FATAL, "  --> Caught signal %d (at %p, integer divide by zero) <--\n",
                      info->si_signo, info->si_addr );
               return true;
          case FPE_FLTDIV:
               D_LOG( Direct_Signals, FATAL, "  --> Caught signal %d (at %p, floating point divide by zero) <--\n",
                      info->si_signo, info->si_addr );
               return true;
     }

     D_LOG( Direct_Signals, FATAL, "  --> Caught signal %d (at %p) <--\n", info->si_signo, info->si_addr );

     return true;
}

static bool
show_any( const siginfo_t *info )
{
     switch (info->si_code) {
          case SI_USER:
               D_LOG( Direct_Signals, FATAL, "  --> Caught signal %d (sent by pid %d, uid %u) <--\n",
                      info->si_signo, info->si_pid, info->si_uid );
               return true;
          case SI_QUEUE:
               D_LOG( Direct_Signals, FATAL, "  --> Caught signal %d (queued by pid %d, uid %u, val %d) <--\n",
                      info->si_signo, info->si_pid, info->si_uid, info->si_int );
               return true;
          case SI_KERNEL:
               D_LOG( Direct_Signals, FATAL, "  --> Caught signal %d (sent by the kernel) <--\n",
                      info->si_signo );
               return true;
     }

     return false;
}

static void
call_handlers( int   num,
               void *addr )
{
     DirectSignalHandler *handler, *temp, *dead = NULL;
     DirectLink *garbage = NULL;

     if (num == SIGPIPE)
          num = DIRECT_SIGNAL_DUMP_STACK;

     /* Loop through all handlers. */
     direct_mutex_lock( &handlers_lock );

     direct_list_foreach_safe (handler, temp, handlers) {
          if (handler->removed) {
               direct_list_remove( &handlers, &handler->link );
               direct_list_append( &garbage, &handler->link );
               continue;
          }

          D_LOG( Direct_Signals, FATAL, "  --> %d\n", handler->num );

          if (handler->num != num && handler->num != DIRECT_SIGNAL_ANY)
               continue;

          if (handler->num == DIRECT_SIGNAL_ANY && num == DIRECT_SIGNAL_DUMP_STACK)
               continue;

          switch (handler->func( num, addr, handler->ctx )) {
               case DSHR_OK:
                    break;

               case DSHR_REMOVE:
                    direct_list_remove( &handlers, &handler->link );
                    direct_list_append( &garbage, &handler->link );
                    break;

               case DSHR_RESUME:
                    D_LOG( Direct_Signals, FATAL, "    '-> cured!\n" );
                    direct_mutex_unlock( &handlers_lock );
                    return;

               default:
                    D_BUG( "unknown result" );
                    break;
          }
     }

     /*
      * looping through the garbage accesses the current element
      * after executing the loop body so it isn't safe to free the
      * memory of the current element in the body.
      * Instead set dead to the current element and free it's memory
      * in the next loop.
      */
     direct_list_foreach (handler, garbage) {
             if (dead) {
                 D_MAGIC_CLEAR( dead );
                 D_FREE( dead );
             }
             dead = handler;
     }
     /* Free the last dead handler */
     if (dead) {
         D_MAGIC_CLEAR( dead );
         D_FREE( dead );
     }

     direct_mutex_unlock( &handlers_lock );
}

static void
signal_handler( int        num,
                siginfo_t *info,
                void      *uctx )
{
     void     *addr = NULL;
     sigset_t  mask;

     if (info && info > (siginfo_t*) 0x100) {
          bool shown = false;

          /* Kernel generated signal. */
          if (info->si_code > 0 && info->si_code < 0x80) {
               addr = info->si_addr;

               switch (num) {
                    case SIGSEGV:
                         shown = show_segv( info );
                         break;

                    case SIGBUS:
                         shown = show_bus( info );
                         break;

                    case SIGILL:
                         shown = show_ill( info );
                         break;

                    case SIGFPE:
                         shown = show_fpe( info );
                         break;

                    default:
                         D_LOG( Direct_Signals, FATAL, "  --> Caught signal %d <--\n", info->si_signo );
                         addr  = NULL;
                         shown = true;
                         break;
               }
          }
          else
               shown = show_any( info );

          if (!shown)
               D_LOG( Direct_Signals, FATAL, "  --> Caught signal %d (unknown origin) <--\n", info->si_signo );
     }
     else
          D_LOG( Direct_Signals, FATAL, "  --> Caught signal %d, no siginfo available <--\n", num );

     direct_trace_print_stacks();

     call_handlers( num, addr );

     sigemptyset( &mask );
     sigaddset( &mask, num );
     direct_sigprocmask( SIG_UNBLOCK, &mask, NULL );

     direct_trap( "SigHandler", num );

     direct_sigprocmask( SIG_BLOCK, &mask, NULL );
}

/**********************************************************************************************************************/

static void *
handle_signals( DirectThread *thread,
                void         *arg )
{
     int       i;
     int       res;
     siginfo_t info;
     sigset_t  mask;

     D_DEBUG_AT( Direct_Signals, "%s()\n", __FUNCTION__ );

     sigemptyset( &mask );

     for (i = 0; i < NUM_SIGS_TO_HANDLE; i++) {
          if (direct_config->sighandler && !sigismember( &direct_config->dont_catch, sigs_to_handle[i] ))
               sigaddset( &mask, sigs_to_handle[i] );
     }

     sigaddset( &mask, SIGSYS );
     sigaddset( &mask, SIGPIPE );

     direct_sigprocmask( SIG_BLOCK, &mask, NULL );

     while (1) {
          D_DEBUG_AT( Direct_Signals, "%s() -> waiting for a signal...\n", __FUNCTION__ );

          res = sigwaitinfo( &mask, &info );

          if (res == -1) {
               D_DEBUG_AT( Direct_Signals, "%s() -> got error %s\n", __FUNCTION__, direct_strerror( errno ) );
          }
          else {
               if (SIGSYS == info.si_signo) {
                    D_DEBUG_AT( Direct_Signals, "  -> got close signal %d (me %d, from %d)\n", SIGSYS,
                                direct_getpid(), info.si_pid );

                    if (direct_getpid() == info.si_pid)
                         break;

                    D_DEBUG_AT( Direct_Signals, "  -> not stopping signal handler from other process signal\n" );
               }
               else if (SIGPIPE == info.si_signo) {
                    D_DEBUG_AT( Direct_Signals, "  -> got dump signal %d (me %d, from %d)\n", SIGPIPE,
                                direct_getpid(), info.si_pid );

                    direct_trace_print_stacks();

                    direct_print_interface_leaks();

                    direct_print_memleaks();

                    call_handlers( info.si_signo, NULL );
               }
               else {
                    signal_handler( info.si_signo, &info, NULL );
               }
          }
     }

     D_DEBUG_AT( Direct_Signals, "Returning from signal handler thread\n" );

     return NULL;
}

static void
install_handlers()
{
     int i;

     D_DEBUG_AT( Direct_Signals, "%s()\n", __FUNCTION__ );

     for (i = 0; i < NUM_SIGS_TO_HANDLE; i++) {
          sigs_handled[i].signum = -1;

          if (direct_config->sighandler && !sigismember( &direct_config->dont_catch,
                                                         sigs_to_handle[i] )) {
               struct sigaction action;
               int              signum = sigs_to_handle[i];

               action.sa_sigaction = signal_handler;
               action.sa_flags     = SA_SIGINFO;

               if (signum != SIGSEGV)
                    action.sa_flags |= SA_NODEFER;

               sigemptyset( &action.sa_mask );

               if (sigaction( signum, &action, &sigs_handled[i].old_action )) {
                    D_PERROR( "Direct/Signals: Unable to install signal handler for signal %d!\n", signum );
                    continue;
               }

               sigs_handled[i].signum = signum;
          }
     }
}

static void
remove_handlers()
{
     int i;

     D_DEBUG_AT( Direct_Signals, "%s()\n", __FUNCTION__ );

     for (i = 0; i < NUM_SIGS_TO_HANDLE; i++) {
          if (sigs_handled[i].signum != -1) {
               int signum = sigs_handled[i].signum;

               if (sigaction( signum, &sigs_handled[i].old_action, NULL )) {
                    D_PERROR( "Direct/Signals: Unable to restore previous handler for signal %d!\n", signum );
               }

               sigs_handled[i].signum = -1;
          }
     }
}

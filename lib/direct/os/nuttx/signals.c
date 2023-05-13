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
#include <direct/list.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/os/mutex.h>
#include <direct/os/signals.h>
#include <direct/trace.h>
#include <direct/util.h>

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

static SigHandled   sigs_handled[NUM_SIGS_TO_HANDLE];

static DirectLink  *handlers = NULL;
static DirectMutex  handlers_lock;

static void install_handlers( void );
static void remove_handlers ( void );

/**********************************************************************************************************************/

DirectResult
direct_signals_initialize()
{
     D_DEBUG_AT( Direct_Signals, "%s() initializing...\n", __FUNCTION__ );

     direct_recursive_mutex_init( &handlers_lock );

     install_handlers();

     return DR_OK;
}

DirectResult
direct_signals_shutdown()
{
     D_DEBUG_AT( Direct_Signals, "%s() shutting down...\n", __FUNCTION__ );

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

     sigprocmask( SIG_BLOCK, &signals, NULL );
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

     /* Mark the handler for removal, freeing will actually come later. */
     handler->removed = true;

     return DR_OK;
}

/**********************************************************************************************************************/

static void
call_handlers( int   num,
               void *addr )
{
     DirectSignalHandler *handler, *temp;
     DirectLink          *garbage = NULL;

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

     direct_list_foreach_safe (handler, temp, handlers) {
          D_MAGIC_CLEAR( handler );
          D_FREE( handler );
     }

     direct_mutex_unlock( &handlers_lock );
}

static void
signal_handler( int        num,
                siginfo_t *info,
                void      *uctx )
{
     if (info && info > (siginfo_t*) 0x100)
          D_LOG( Direct_Signals, FATAL, "  --> Caught signal %d <--\n", info->si_signo );
     else
          D_LOG( Direct_Signals, FATAL, "  --> Caught signal %d, no siginfo available <--\n", num );

     direct_trace_print_stacks();

     call_handlers( num, NULL );

     remove_handlers();

     exit( -num );
}

/**********************************************************************************************************************/

static void
install_handlers()
{
     int i;

     D_DEBUG_AT( Direct_Signals, "%s()\n", __FUNCTION__ );

     for (i = 0; i < NUM_SIGS_TO_HANDLE; i++) {
          sigs_handled[i].signum = -1;

          if (direct_config->sighandler && !sigismember( &direct_config->dont_catch, sigs_to_handle[i] )) {
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

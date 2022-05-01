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
#include <direct/messages.h>
#include <direct/os/mem.h>
#include <direct/os/system.h>
#include <direct/os/thread.h>
#include <direct/trace.h>

D_DEBUG_DOMAIN( Direct_Thread,     "Direct/Thread",      "Direct Thread Management" );
D_DEBUG_DOMAIN( Direct_ThreadInit, "Direct/Thread/Init", "Direct Thread Init" );

/**********************************************************************************************************************/

static pthread_key_t thread_key;
static DirectOnce    thread_init_once = DIRECT_ONCE_INIT();

/*
 * Wrapper around pthread's main routine to pass additional arguments
 * and setup things like signal masks and scheduling priorities.
 */
__attribute__((no_instrument_function))
static void *direct_thread_main( void *arg );

/**********************************************************************************************************************/

__attribute__((no_instrument_function))
DirectResult
direct_once( DirectOnce            *once,
             DirectOnceInitHandler  handler )
{
     int erno = pthread_once( &once->once, handler );

     return errno2result( erno );
}

__attribute__((no_instrument_function))
static void
init_once()
{
     /* Create the key for the TSD (thread specific data). */
     pthread_key_create( &thread_key, NULL );
}

DirectResult
direct_thread_init( DirectThread *thread )
{
     int                erno;
     pthread_attr_t     attr;
     struct sched_param param;
     int                policy;
     int                priority;

     direct_once( &thread_init_once, init_once );

     /* Initialize scheduling and other parameters. */
     pthread_attr_init( &attr );

     pthread_attr_setinheritsched( &attr, PTHREAD_EXPLICIT_SCHED );

     /* Select scheduler. */
     switch (direct_config->thread_scheduler) {
          case DCTS_FIFO:
               policy = SCHED_FIFO;
               break;

          case DCTS_RR:
               policy = SCHED_RR;
               break;

          default:
               policy = SCHED_OTHER;
               break;
     }

     if (pthread_attr_setschedpolicy( &attr, policy ))
          D_PERROR( "Direct/Thread: Could not set scheduling policy to %s!\n", direct_thread_policy_name( policy ) );

     /* Read (back) value. */
     pthread_attr_getschedpolicy( &attr, &policy );
     thread->policy = policy;

     /* Select priority. */
     switch (thread->type) {
          case DTT_CLEANUP:
          case DTT_INPUT:
          case DTT_OUTPUT:
          case DTT_MESSAGING:
          case DTT_CRITICAL:
               priority = thread->type * direct_config->thread_priority_scale / 100;
               break;

          default:
               priority = direct_config->thread_priority;
               break;
     }

     D_DEBUG_AT( Direct_ThreadInit, "  -> %s (%d) [%d;%d]\n", direct_thread_policy_name( policy ), priority,
                 sched_get_priority_min( policy ), sched_get_priority_max( policy ) );

     if (priority < sched_get_priority_min( policy ))
          priority = sched_get_priority_min( policy );

     if (priority > sched_get_priority_max( policy ))
          priority = sched_get_priority_max( policy );

     param.sched_priority = priority;

     if (pthread_attr_setschedparam( &attr, &param ))
          D_PERROR( "Direct/Thread: Could not set scheduling priority to %d!\n", priority );

     /* Select stack size. */
     if (direct_config->thread_stack_size > 0) {
          if (pthread_attr_setstacksize( &attr, direct_config->thread_stack_size ))
               D_PERROR( "Direct/Thread: Could not set stack size to %d!\n", direct_config->thread_stack_size );
     }

     erno = pthread_create( &thread->handle, &attr, direct_thread_main, thread );
     if (erno)
          return errno2result( erno );

     pthread_attr_destroy( &attr );

     /* Read (back) value. */
     pthread_getattr_np( thread->handle, &attr );
     pthread_attr_getstacksize( &attr, &thread->stack_size );
     pthread_attr_getschedparam( &attr, &param );
     thread->priority = param.sched_priority;
     pthread_attr_destroy( &attr );

     return DR_OK;
}

void
direct_thread_deinit( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );

     D_ASSUME( !pthread_equal( thread->handle, pthread_self() ) );
     D_ASSUME( !thread->detached );

     D_DEBUG_AT( Direct_Thread, "%s( %p, '%s' %d )\n", __FUNCTION__, thread->main, thread->name, thread->tid );

     if (thread->detached) {
          D_DEBUG_AT( Direct_Thread, "  -> detached!\n" );
          return;
     }

     if (!thread->joined && !pthread_equal( thread->handle, pthread_self() )) {
          if (thread->canceled)
               D_DEBUG_AT( Direct_Thread, "  -> cancled but not joined!\n" );
          else {
               D_DEBUG_AT( Direct_Thread, "  -> still running!\n" );

               if (thread->name)
                    D_ERROR( "Direct/Thread: Canceling '%s' (%d)!\n", thread->name, thread->tid );
               else
                    D_ERROR( "Direct/Thread: Canceling %d!\n", thread->tid );

               pthread_cancel( thread->handle );
          }

          pthread_join( thread->handle, NULL );
     }
}

__attribute__((no_instrument_function))
void
direct_thread_setcancelstate( DirectThreadCancelState state )
{
     switch (state) {
          case DIRECT_THREAD_CANCEL_ENABLE:
               pthread_setcancelstate( PTHREAD_CANCEL_ENABLE, NULL );

          case DIRECT_THREAD_CANCEL_DISABLE:
               pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, NULL );
     }
}

__attribute__((no_instrument_function))
DirectThread *
direct_thread_self()
{
     DirectThread *thread;

     direct_once( &thread_init_once, init_once );

     thread = pthread_getspecific( thread_key );

     /* Support this function for non-direct threads. */
     if (!thread) {
          thread = calloc( 1, sizeof(DirectThread) );
          if (!thread) {
               D_OOM();
               return NULL;
          }

          thread->handle = pthread_self();
          thread->tid    = direct_gettid();

          D_MAGIC_SET( thread, DirectThread );

          pthread_setspecific( thread_key, thread );
     }

     return thread;
}

__attribute__((no_instrument_function))
const char *
direct_thread_self_name()
{
     DirectThread *thread = direct_thread_self();
     char name[16];

     /* This function is called by debugging functions, e.g. debug messages, assertions etc.
        Therefore no assertions are made here, because they would loop forever if they fail. */

     if (!thread)
          return NULL;

     if (!thread->name) {
          prctl( PR_GET_NAME, name, 0, 0, 0 );
          thread->name = strdup( name );
     }

     return thread->name;
}

void
direct_thread_set_name( const char *name )
{
     char         *copy;
     DirectThread *thread;

     D_DEBUG_AT( Direct_Thread, "%s( '%s' )\n", __FUNCTION__, name );

     thread = direct_thread_self();
     if (!thread)
          return;

     /* Duplicate string. */
     copy = strdup( name );
     if (!copy) {
          D_OOM();
          return;
     }

     /* Free old string. */
     if (thread->name)
          free( thread->name );

     /* Keep the copy. */
     thread->name = copy;
}

void
direct_thread_cancel( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );
     D_ASSERT( thread->handle != -1 );
     D_ASSERT( !pthread_equal( thread->handle, pthread_self() ) );

     D_ASSUME( !thread->canceled );

     D_DEBUG_AT( Direct_Thread, "%s( %p, '%s' %d )\n", __FUNCTION__, thread->main, thread->name, thread->tid );

     thread->canceled = true;

     pthread_cancel( thread->handle );
}

void
direct_thread_detach( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );
     D_ASSERT( thread->handle != -1 );
     D_ASSERT( !pthread_equal( thread->handle, pthread_self() ) );

     D_ASSUME( !thread->canceled );

     D_DEBUG_AT( Direct_Thread, "%s( %p, '%s' %d )\n", __FUNCTION__, thread->main, thread->name, thread->tid );

     thread->detached = true;

     pthread_detach( thread->handle );
}

void
direct_thread_testcancel( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );
     D_ASSERT( thread->handle != -1 );
     D_ASSERT( pthread_equal( thread->handle, pthread_self() ) );

     /* Quick check before calling the pthread function. */
     if (thread->canceled)
          pthread_testcancel();
}

void
direct_thread_join( DirectThread *thread )
{
     D_MAGIC_ASSERT( thread, DirectThread );
     D_ASSERT( thread->handle != -1 );

     D_ASSUME( !pthread_equal( thread->handle, pthread_self() ) );
     D_ASSUME( !thread->joining );
     D_ASSUME( !thread->joined );
     D_ASSUME( !thread->detached );

     D_DEBUG_AT( Direct_Thread, "%s( %p, '%s' %d )\n", __FUNCTION__, thread->main, thread->name, thread->tid );

     if (thread->detached) {
          D_DEBUG_AT( Direct_Thread, "  -> detached\n" );
          return;
     }

     if (!thread->joining && !pthread_equal( thread->handle, pthread_self() )) {
          thread->joining = true;

          D_DEBUG_AT( Direct_Thread, "  -> joining...\n" );

          pthread_join( thread->handle, NULL );

          thread->joined = true;

          D_DEBUG_AT( Direct_Thread, "  -> joined\n" );
     }
}

void
direct_thread_kill( DirectThread *thread,
                    int           signal )
{
     D_MAGIC_ASSERT( thread, DirectThread );
     D_ASSERT( thread->handle != -1 );

     D_DEBUG_AT( Direct_Thread, "%s( %p, '%s' %d, signal %d )\n", __FUNCTION__,
                 thread->main, thread->name, thread->tid, signal );

     pthread_kill( thread->handle, signal );
}

void
direct_thread_sleep( long long micros )
{
     usleep( micros );
}

void
direct_thread_atfork( DirectThreadPrepareHandler prepare,
                      DirectThreadParentHandler  parent,
                      DirectThreadChildHandler   child )
{
     pthread_atfork( prepare, parent, child );
}

/**********************************************************************************************************************/

const char *
direct_thread_type_name( DirectThreadType type )
{
     switch (type) {
          case DTT_DEFAULT:
               return "DEFAULT";

          case DTT_CLEANUP:
               return "CLEANUP";

          case DTT_INPUT:
               return "INPUT";

          case DTT_OUTPUT:
               return "OUTPUT";

          case DTT_MESSAGING:
               return "MESSAGING";

          case DTT_CRITICAL:
               return "CRITICAL";
     }

     return "<unknown>";
}

const char *
direct_thread_policy_name( int policy )
{
     switch (policy) {
          case SCHED_OTHER:
               return "OTHER";

          case SCHED_FIFO:
               return "FIFO";

          case SCHED_RR:
               return "RR";
     }

     return "<unknown>";
}

/**********************************************************************************************************************/

__attribute__((no_instrument_function))
DirectResult
direct_tls_register( DirectTLS *tls,
                     DirectTLSDestructor  destructor )
{
     int erno = pthread_key_create( &tls->key, destructor );

     return errno2result( erno );
}

__attribute__((no_instrument_function))
DirectResult
direct_tls_unregister( DirectTLS *tls )
{
     int erno;

     erno = pthread_key_delete( tls->key );
     if (erno)
          return errno2result( erno );

     tls->key = (pthread_key_t) -1;

     return DR_OK;
}

__attribute__((no_instrument_function))
void *
direct_tls_get( DirectTLS *tls )
{
     void *value;

     value = pthread_getspecific( tls->key );

     return value;
}

__attribute__((no_instrument_function))
DirectResult
direct_tls_set( DirectTLS *tls,
                void      *value )
{
     int erno = pthread_setspecific( tls->key, value );

     return errno2result( erno );
}

/**********************************************************************************************************************/

static void
direct_thread_cleanup( void *arg )
{
     DirectThread *thread = arg;

     D_MAGIC_ASSERT( thread, DirectThread );

     D_DEBUG_AT( Direct_Thread, "%s( %p, '%s' %d )\n", __FUNCTION__, thread->main, thread->name, thread->tid );

     if (thread->trace_buffer)
          direct_trace_free_buffer( thread->trace_buffer );

     if (thread->detached) {
          D_MAGIC_CLEAR( thread );

          if (thread->name)
               free( thread->name );

          free( thread );
     }
}

__attribute__((no_instrument_function))
static void *
direct_thread_main( void *arg )
{
     void         *res;
     DirectThread *thread = arg;

     prctl( PR_SET_NAME, thread->name, 0, 0, 0 );

     pthread_setspecific( thread_key, thread );

     D_DEBUG_AT( Direct_ThreadInit, "%s( %p )\n", __FUNCTION__, arg );

     D_DEBUG_AT( Direct_ThreadInit, "  -> starting...\n" );

     D_MAGIC_ASSERT( thread, DirectThread );

     pthread_cleanup_push( direct_thread_cleanup, thread );

     thread->tid = direct_gettid();

     D_DEBUG_AT( Direct_ThreadInit, "  -> tid %d\n", thread->tid );

     _direct_thread_call_init_handlers( thread );

     /* Have all signals handled by the main thread. */
     if (direct_config->thread_block_signals) {
          sigset_t signals;
          sigfillset( &signals );
          direct_sigprocmask( SIG_BLOCK, &signals, NULL );
     }

     /* Lock the thread mutex. */
     D_DEBUG_AT( Direct_ThreadInit, "  -> locking...\n" );
     direct_mutex_lock( &thread->lock );

     /* Indicate that our initialization has completed. */
     thread->init = true;

     D_DEBUG_AT( Direct_ThreadInit, "  -> signalling...\n" );
     direct_waitqueue_signal( &thread->cond );

     /* Unlock the thread mutex. */
     D_DEBUG_AT( Direct_ThreadInit, "  -> unlocking...\n" );
     direct_mutex_unlock( &thread->lock );

     if (thread->joining) {
          D_DEBUG_AT( Direct_Thread, "  -> being joined before entering main routine!\n" );
          return NULL;
     }

     /* Call main routine. */
     D_DEBUG_AT( Direct_ThreadInit, "  -> running...\n" );
     res = thread->main( thread, thread->arg );

     D_DEBUG_AT( Direct_Thread, "  -> returning %p from '%s' (%s, %d)...\n",
                 res, thread->name, direct_thread_type_name( thread->type ), thread->tid );

     pthread_cleanup_pop( 1 );

     return res;
}

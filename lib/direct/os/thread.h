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

#ifndef __DIRECT__OS__THREAD_H__
#define __DIRECT__OS__THREAD_H__

#include <direct/os/types.h>
#include <direct/os/waitqueue.h>

/**********************************************************************************************************************/

typedef enum {
     DTT_DEFAULT   =   0,
     DTT_CLEANUP   =  -5,
     DTT_INPUT     = -10,
     DTT_OUTPUT    = -12,
     DTT_MESSAGING = -15,
     DTT_CRITICAL  = -20
} DirectThreadType;

typedef void *(*DirectThreadMainFunc)( DirectThread *thread, void *arg );

struct __D_DirectThread {
     int                   magic;

     char                 *name;

     DirectThreadType      type;         /* The thread's type. */
     DirectThreadMainFunc  main;         /* The thread's main routine (or entry point). */
     void                 *arg;          /* Custom argument passed to the main routine. */

     DirectThreadHandle    handle;       /* The thread's handle. */
     pid_t                 tid;          /* The thread's ID. */

     bool                  canceled;     /* Set when direct_thread_cancel() is called. */
     bool                  joining;      /* Set when direct_thread_join() is called. */
     bool                  joined;       /* Set when direct_thread_join() has finished. */
     bool                  detached;     /* Set when direct_thread_detach() is called. */
     bool                  terminated;   /* Set when direct_thread_terminate() is called. */

     bool                  init;         /* Set to true before the main routine is called. */

     DirectMutex           lock;
     DirectWaitQueue       cond;

     unsigned int          counter;

     int                   policy;
     int                   priority;
     size_t                stack_size;

     void                 *trace_buffer;
};

/**********************************************************************************************************************/

typedef enum {
     DIRECT_THREAD_CANCEL_ENABLE  = 0x00000000,
     DIRECT_THREAD_CANCEL_DISABLE = 0x00000001
} DirectThreadCancelState;

typedef void (*DirectOnceInitHandler)( void );

typedef void (*DirectThreadPrepareHandler)( void );
typedef void (*DirectThreadParentHandler)( void );
typedef void (*DirectThreadChildHandler)( void );

typedef void (*DirectTLSDestructor)( void* );

/**********************************************************************************************************************/

/*
 * Thread initialization/deinitialization.
 */

DirectResult DIRECT_API  direct_once                     ( DirectOnce                 *once,
                                                           DirectOnceInitHandler       handler );

DirectResult DIRECT_API  direct_thread_init              ( DirectThread               *thread );

void         DIRECT_API  direct_thread_deinit            ( DirectThread               *thread );

/*
 * Sets the cancelability state of the calling thread.
 */
void         DIRECT_API  direct_thread_setcancelstate    ( DirectThreadCancelState     state );

/*
 * Returns the thread of the caller.
 */
DirectThread DIRECT_API *direct_thread_self              ( void );

/*
 * Returns the name of the calling thread.
 */
const char   DIRECT_API *direct_thread_self_name         ( void );

/*
 * Changes the name of the calling thread.
 */
void         DIRECT_API  direct_thread_set_name          ( const char                 *name );

/*
 * Cancel a running thread.
 */
void         DIRECT_API  direct_thread_cancel            ( DirectThread               *thread );

/*
 * Detach a thread.
 */
void         DIRECT_API  direct_thread_detach            ( DirectThread               *thread );

/*
 * Check if the calling thread is canceled.
 */
void         DIRECT_API  direct_thread_testcancel        ( DirectThread               *thread );

/*
 * Wait until a running thread is terminated.
 */
void         DIRECT_API  direct_thread_join              ( DirectThread               *thread );

/*
 * Send a signal to a thread.
 */
void         DIRECT_API  direct_thread_kill              ( DirectThread               *thread,
                                                           int                         signal );

/*
 * Sleep a thread.
 */
void         DIRECT_API  direct_thread_sleep             ( long long                   micros );

/*
 * Register fork handlers.
 */
void         DIRECT_API  direct_thread_atfork            ( DirectThreadPrepareHandler  prepare,
                                                           DirectThreadParentHandler   parent,
                                                           DirectThreadChildHandler    child );

/*
 * Utilities for stringification.
 */

const char   DIRECT_API *direct_thread_type_name         ( DirectThreadType            type );

const char   DIRECT_API *direct_thread_policy_name       ( int                         policy );

/*
 * Thread-specific data.
 */

DirectResult DIRECT_API  direct_tls_register             ( DirectTLS                  *tls,
                                                           DirectTLSDestructor         destructor );

DirectResult DIRECT_API  direct_tls_unregister           ( DirectTLS                  *tls );

void         DIRECT_API *direct_tls_get                  ( DirectTLS                  *tls );

DirectResult DIRECT_API  direct_tls_set                  ( DirectTLS                  *tls,
                                                           void                       *value);

/*
 * Call all init handlers.
 */
void         DIRECT_API _direct_thread_call_init_handlers( DirectThread               *thread );

#endif

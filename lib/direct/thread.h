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

#ifndef __DIRECT__THREAD_H__
#define __DIRECT__THREAD_H__

#include <direct/os/thread.h>

/**********************************************************************************************************************/

typedef void (*DirectThreadInitFunc)( DirectThread *thread, void *arg );

/**********************************************************************************************************************/

/*
 * Add a handler being called at the beginning of new threads.
 */
DirectThreadInitHandler DIRECT_API *direct_thread_add_init_handler   ( DirectThreadInitFunc  func,
                                                                       void                 *arg );

/*
 * Remove the specified handler.
 */
void                    DIRECT_API  direct_thread_remove_init_handler( DirectThreadInitHandler *handler );

/*
 * Create a new thread and start it. The thread type is relevant for the scheduling priority.
 */
DirectThread            DIRECT_API *direct_thread_create             ( DirectThreadType      thread_type,
                                                                       DirectThreadMainFunc  thread_main,
                                                                       void                 *arg,
                                                                       const char           *name );

/*
 * Wait on the thread object to be notified via direct_thread_notify().
 */
DirectResult            DIRECT_API  direct_thread_wait               ( DirectThread         *thread,
                                                                       int                   timeout_ms );

/*
 * Notify the thread object waking up callers of direct_thread_wait().
 */
DirectResult            DIRECT_API  direct_thread_notify             ( DirectThread         *thread );

/*
 * The thread acquires the lock.
 */
DirectResult            DIRECT_API  direct_thread_lock               ( DirectThread         *thread );

/*
 * The thread releases the lock.
 */
DirectResult            DIRECT_API  direct_thread_unlock             ( DirectThread         *thread );

/*
 * Kindly ask the thread to terminate (for joining without thread cancellation).
 */
DirectResult            DIRECT_API  direct_thread_terminate          ( DirectThread         *thread );

/*
 * Free resources allocated by direct_thread_create. If the thread is still running it will be killed.
 */
void                    DIRECT_API  direct_thread_destroy            ( DirectThread         *thread );

/*
 * Return the thread's ID.
 */
pid_t                   DIRECT_API  direct_thread_get_tid            ( const DirectThread   *thread );

/*
 * True if the thread is canceled.
 */
bool                    DIRECT_API  direct_thread_is_canceled        ( const DirectThread   *thread );

/*
 * True if the thread is joined.
 */
bool                    DIRECT_API  direct_thread_is_joined          ( const DirectThread   *thread );

/**********************************************************************************************************************/

void __D_thread_init  ( void );
void __D_thread_deinit( void );

#endif

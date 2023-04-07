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

#ifndef __DIRECT__OS__NUTTX__WAITQUEUE_H__
#define __DIRECT__OS__NUTTX__WAITQUEUE_H__

#include <direct/os/mutex.h>
#include <direct/util.h>

/**********************************************************************************************************************/

typedef struct {
     pthread_cond_t cond;
} DirectWaitQueue;

static inline DirectResult
direct_waitqueue_init( DirectWaitQueue *queue )
{
     int erno = pthread_cond_init( &queue->cond, NULL );

     return errno2result( erno );
}

static inline DirectResult
direct_waitqueue_wait( DirectWaitQueue *queue,
                       DirectMutex     *mutex )
{
     int erno = pthread_cond_wait( &queue->cond, &mutex->lock );

     return errno2result( erno );
}

static inline DirectResult
direct_waitqueue_wait_timeout( DirectWaitQueue *queue,
                               DirectMutex     *mutex,
                               unsigned long    micros )
{
     struct timeval  now;
     struct timespec timeout;
     long            seconds      = micros / 1000000;
     long            nano_seconds = (micros % 1000000) * 1000;

     gettimeofday( &now, NULL );

     timeout.tv_sec  = now.tv_sec + seconds;
     timeout.tv_nsec = (now.tv_usec * 1000) + nano_seconds;

     timeout.tv_sec  += timeout.tv_nsec / 1000000000;
     timeout.tv_nsec %= 1000000000;

     if (pthread_cond_timedwait( &queue->cond, &mutex->lock, &timeout ) == ETIMEDOUT)
          return DR_TIMEOUT;

     return DR_OK;
}

static inline DirectResult
direct_waitqueue_signal( DirectWaitQueue *queue )
{
     int erno = pthread_cond_signal( &queue->cond );

     return errno2result( erno );
}

static inline DirectResult
direct_waitqueue_broadcast( DirectWaitQueue *queue )
{
     int erno = pthread_cond_broadcast( &queue->cond );

     return errno2result( erno );
}

static inline DirectResult
direct_waitqueue_deinit( DirectWaitQueue *queue )
{
     int erno = pthread_cond_destroy( &queue->cond );

     return errno2result( erno );
}

#endif

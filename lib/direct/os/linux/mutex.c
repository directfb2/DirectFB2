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

#include <direct/messages.h>
#include <direct/os/mutex.h>
#include <direct/util.h>

/**********************************************************************************************************************/

DirectResult
direct_mutex_init( DirectMutex *mutex )
{
     int erno = pthread_mutex_init( &mutex->lock, NULL );

     return errno2result( erno );
}

DirectResult
direct_recursive_mutex_init( DirectMutex *mutex )
{
     DirectResult        ret = DR_OK;
     int                 erno;
     pthread_mutexattr_t attr;

     pthread_mutexattr_init( &attr );
     pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_RECURSIVE );
     erno = pthread_mutex_init( &mutex->lock, &attr );
     if (erno) {
          ret = errno2result( erno );
          D_PERROR( "Direct/Mutex: Could not initialize recursive mutex!\n" );
     }

     pthread_mutexattr_destroy( &attr );

     return ret;
}

__attribute__((no_instrument_function))
DirectResult
direct_mutex_lock( DirectMutex *mutex )
{
     int erno = pthread_mutex_lock( &mutex->lock );

     return errno2result( erno );
}

__attribute__((no_instrument_function))
DirectResult
direct_mutex_unlock( DirectMutex *mutex )
{
     int erno = pthread_mutex_unlock( &mutex->lock );

     return errno2result( erno );
}

__attribute__((no_instrument_function))
DirectResult
direct_mutex_trylock( DirectMutex *mutex )
{
     int erno = pthread_mutex_trylock( &mutex->lock );

     return errno2result( erno );
}

DirectResult
direct_mutex_deinit( DirectMutex *mutex )
{
     int erno = pthread_mutex_destroy( &mutex->lock );

     return errno2result( erno );
}

DirectResult
direct_rwlock_init( DirectRWLock *rwlock )
{
     int erno = pthread_rwlock_init( &rwlock->lock, NULL );

     return errno2result( erno );
}

__attribute__((no_instrument_function))
DirectResult
direct_rwlock_rdlock( DirectRWLock *rwlock )
{
     int erno = pthread_rwlock_rdlock( &rwlock->lock );

     return errno2result( erno );
}

__attribute__((no_instrument_function))
DirectResult
direct_rwlock_wrlock( DirectRWLock *rwlock )
{
     int erno = pthread_rwlock_wrlock( &rwlock->lock );

     return errno2result( erno );
}

__attribute__((no_instrument_function))
DirectResult
direct_rwlock_unlock( DirectRWLock *rwlock )
{
     int erno = pthread_rwlock_unlock( &rwlock->lock );

     return errno2result( erno );
}

__attribute__((no_instrument_function))
DirectResult
direct_rwlock_trywrlock( DirectRWLock *rwlock )
{
     int erno = pthread_rwlock_trywrlock( &rwlock->lock );

     return errno2result( erno );
}

DirectResult
direct_rwlock_deinit( DirectRWLock *rwlock )
{
     int erno = pthread_rwlock_destroy( &rwlock->lock );

     return errno2result( erno );
}

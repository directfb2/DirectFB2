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

#ifndef __DIRECT__OS__WAITQUEUE_H__
#define __DIRECT__OS__WAITQUEUE_H__

#include <direct/os/types.h>

/**********************************************************************************************************************/

DirectResult DIRECT_API direct_waitqueue_init        ( DirectWaitQueue *queue );

DirectResult DIRECT_API direct_waitqueue_wait        ( DirectWaitQueue *queue,
                                                       DirectMutex     *mutex );

DirectResult DIRECT_API direct_waitqueue_wait_timeout( DirectWaitQueue *queue,
                                                       DirectMutex     *mutex,
                                                       unsigned long    micros );

DirectResult DIRECT_API direct_waitqueue_signal      ( DirectWaitQueue *queue );

DirectResult DIRECT_API direct_waitqueue_broadcast   ( DirectWaitQueue *queue );

DirectResult DIRECT_API direct_waitqueue_deinit      ( DirectWaitQueue *queue );

#endif

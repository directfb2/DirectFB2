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

#ifndef __DIRECT__OS__TYPES_H__
#define __DIRECT__OS__TYPES_H__

#include <direct/build.h>

#endif

#if defined(DIRECT_BUILD_OS_LINUX)

#include <direct/os/linux/types.h>

#ifdef __DIRECT__OS__FILESYSTEM_H__
#include <direct/os/linux/filesystem.h>
#endif

#ifdef __DIRECT__OS__MUTEX_H__
#include <direct/os/linux/mutex.h>
#endif

#ifdef __DIRECT__OS__THREAD_H__
#include <direct/os/linux/thread.h>
#endif

#ifdef __DIRECT__OS__WAITQUEUE_H__
#include <direct/os/linux/waitqueue.h>
#endif

#else
#error Unsupported OS!
#endif

#ifndef __DIRECT__TYPES_H__
#include <direct/types.h>
#endif

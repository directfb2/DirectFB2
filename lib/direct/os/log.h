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

#ifndef __DIRECT__OS__LOG_H__
#define __DIRECT__OS__LOG_H__

#include <direct/os/mutex.h>

/**********************************************************************************************************************/

typedef enum {
     DLT_STDERR = 0x00000000, /* Simply print out log on stderr. */
     DLT_FILE   = 0x00000001, /* Write log into a file. */
     DLT_UDP    = 0x00000002  /* Send out log via UDP. */
} DirectLogType;

typedef DirectResult (*DirectLogWriteFunc)    ( DirectLog *log, const char *buffer, size_t bytes );
typedef DirectResult (*DirectLogFlushFunc)    ( DirectLog *log, bool sync );
typedef DirectResult (*DirectLogSetBufferFunc)( DirectLog *log, char *buffer, size_t bytes );

struct __D_DirectLog {
     int                     magic;

     DirectLogType           type;

     DirectMutex             lock;

     void                   *data;

     DirectLogWriteFunc      write;
     DirectLogFlushFunc      flush;
     DirectLogSetBufferFunc  set_buffer;
};

/**********************************************************************************************************************/

/*
 * Initializes a logging facility.
 *
 * For each 'log->type' the 'param' has a different meaning:
 *   DLT_STDERR     ignored (leave NULL)
 *   DLT_FILE       file name
 *   DLT_UDP        <ip>:<port>
 */
DirectResult DIRECT_API direct_log_init  ( DirectLog  *log,
                                           const char *param );

/*
 * Destroys a logging facility.
 */
DirectResult DIRECT_API direct_log_deinit( DirectLog  *log );

#endif

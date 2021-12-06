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

#ifndef __DIRECT__STREAM_H__
#define __DIRECT__STREAM_H__

#include <direct/types.h>

/**********************************************************************************************************************/

/*
 * Create a stream wrapper.
 *
 * 'filename' can be a plain file name or one of the following:
 *   file://<path>
 *   http://<host>[:<port>]/<path>
 *   ftp://<host>[:<port>]/<path>
 *   tcp://<host>:<port>
 *   udp://<host>:<port>
 */
DirectResult DIRECT_API  direct_stream_create  ( const char      *filename,
                                                 DirectStream   **ret_stream );

/*
 * Duplicate the stream.
 */
DirectStream DIRECT_API *direct_stream_dup     ( DirectStream    *stream );

/*
 * Return the file descriptor associated to the stream.
 */
int          DIRECT_API  direct_stream_filenoi ( DirectStream    *stream );

/*
 * True if stream is seekable.
 */
bool         DIRECT_API  direct_stream_seekable( DirectStream    *stream );

/*
 * True if stream originates from a remote host.
 */
bool         DIRECT_API  direct_stream_remote  ( DirectStream    *stream );

/*
 * Get the mime description of the stream.
 */
const char   DIRECT_API *direct_stream_mime    ( DirectStream    *stream );

/*
 * Get stream position.
 */
unsigned int DIRECT_API  direct_stream_offset  ( DirectStream    *stream );

/*
 * Get stream length.
 */
unsigned int DIRECT_API  direct_stream_length  ( DirectStream    *stream );

/*
 * Wait for data to be available.
 * If 'timeout' is NULL, the function blocks indefinitely.
 * Set the 'timeout' value to 0 to make the function return immediatly.
 */
DirectResult DIRECT_API  direct_stream_wait    ( DirectStream    *stream,
                                                 unsigned int     length,
                                                 struct timeval  *timeout );

/*
 * Peek 'length' bytes of data at offset 'offset' from the stream.
 */
DirectResult DIRECT_API  direct_stream_peek    ( DirectStream    *stream,
                                                 unsigned int     length,
                                                 int              offset,
                                                 void            *buf,
                                                 unsigned int    *read_out );

/*
 * Fetch 'length' bytes of data from the stream.
 */
DirectResult DIRECT_API  direct_stream_read    ( DirectStream    *stream,
                                                 unsigned int     length,
                                                 void            *buf,
                                                 unsigned int    *read_out );

/*
 * Seek to the specified absolute offset within the stream.
 */
DirectResult DIRECT_API  direct_stream_seek    ( DirectStream    *stream,
                                                 unsigned int     offset );

/*
 * Destroy the stream wrapper.
 */
void         DIRECT_API  direct_stream_destroy ( DirectStream    *stream );

#endif

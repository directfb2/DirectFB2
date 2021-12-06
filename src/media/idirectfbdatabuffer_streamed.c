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

#include <direct/list.h>
#include <direct/memcpy.h>
#include <direct/waitqueue.h>
#include <media/idirectfbdatabuffer.h>
#include <media/idirectfbdatabuffer_streamed.h>

D_DEBUG_DOMAIN( DataBuffer, "IDirectFBDataBufferS", "IDirectFBDataBuffer_Streamed Interface" );

/**********************************************************************************************************************/

/*
 * private data struct of IDirectFBDataBuffer_Streamed
 */
typedef struct {
     IDirectFBDataBuffer_data  base;           /* base databuffer implementation */

     DirectLink               *chunks;         /* data chunks */

     unsigned int              length;         /* total length of all chunks */

     bool                      finished;       /* whether Finish() has been called */

     DirectMutex               chunks_mutex;   /* mutex lock for accessing the chunk list */

     DirectWaitQueue           wait_condition; /* condition used for idle wait */
} IDirectFBDataBuffer_Streamed_data;

typedef struct {
     DirectLink    link;

     void         *data;   /* actual data hold */
     unsigned int  length; /* length of chunk */

     unsigned int  done;   /* number of bytes already consumed */
} DataChunk;

static DataChunk *create_chunk ( const void *data, int length );
static void       destroy_chunk( DataChunk *chunk );

static void DestroyAllChunks( IDirectFBDataBuffer_Streamed_data *data );
static void ReadChunkData   ( IDirectFBDataBuffer_Streamed_data *data,
                              void                              *buffer,
                              unsigned int                       offset,
                              unsigned int                       length,
                              bool                               flush );

/**********************************************************************************************************************/

static void
IDirectFBDataBuffer_Streamed_Destruct( IDirectFBDataBuffer *thiz )
{
     IDirectFBDataBuffer_Streamed_data *data = thiz->priv;

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     direct_waitqueue_deinit( &data->wait_condition );
     direct_mutex_deinit( &data->chunks_mutex );

     IDirectFBDataBuffer_Destruct( thiz );
}

static DirectResult
IDirectFBDataBuffer_Streamed_Release( IDirectFBDataBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDataBuffer )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     if (--data->ref == 0)
          IDirectFBDataBuffer_Streamed_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_Streamed_Flush( IDirectFBDataBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDataBuffer_Streamed )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     direct_mutex_lock( &data->chunks_mutex );

     DestroyAllChunks( data );

     direct_mutex_unlock( &data->chunks_mutex );

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_Streamed_Finish( IDirectFBDataBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDataBuffer_Streamed )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->finished) {
          data->finished = true;

          direct_mutex_lock( &data->chunks_mutex );
          direct_waitqueue_broadcast( &data->wait_condition );
          direct_mutex_unlock( &data->chunks_mutex );
     }

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_Streamed_SeekTo( IDirectFBDataBuffer *thiz,
                                     unsigned int         offset )
{
     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     return DFB_UNSUPPORTED;
}

static DFBResult
IDirectFBDataBuffer_Streamed_GetPosition( IDirectFBDataBuffer *thiz,
                                          unsigned int        *ret_offset )
{
     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     return DFB_UNSUPPORTED;
}

static DFBResult
IDirectFBDataBuffer_Streamed_GetLength( IDirectFBDataBuffer *thiz,
                                        unsigned int        *ret_length )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDataBuffer_Streamed )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_length)
          return DFB_INVARG;

     *ret_length = data->length;

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_Streamed_WaitForData( IDirectFBDataBuffer *thiz,
                                          unsigned int         length )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDataBuffer_Streamed )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     if (data->finished && !data->chunks)
          return DFB_EOF;

     direct_mutex_lock( &data->chunks_mutex );

     while (data->length < length && !data->finished)
          direct_waitqueue_wait( &data->wait_condition, &data->chunks_mutex );

     direct_mutex_unlock( &data->chunks_mutex );

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_Streamed_WaitForDataWithTimeout( IDirectFBDataBuffer *thiz,
                                                     unsigned int         length,
                                                     unsigned int         seconds,
                                                     unsigned int         milli_seconds )
{
     DirectResult ret    = DR_OK;
     bool         locked = false;

     DIRECT_INTERFACE_GET_DATA( IDirectFBDataBuffer_Streamed )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     if (data->finished && !data->chunks)
          return DFB_EOF;

     if (direct_mutex_trylock( &data->chunks_mutex ) == 0) {
          if (data->length >= length) {
               direct_mutex_unlock( &data->chunks_mutex );

               return DFB_OK;
          }

          locked = true;
     }

     if (!locked)
          direct_mutex_lock( &data->chunks_mutex );

     while (data->length < length && !data->finished) {
          ret = direct_waitqueue_wait_timeout( &data->wait_condition, &data->chunks_mutex,
                                               seconds * 1000000 + milli_seconds * 1000 );
          if (ret == DR_TIMEOUT)
               break;
     }

     direct_mutex_unlock( &data->chunks_mutex );

     return ret;
}

static DFBResult
IDirectFBDataBuffer_Streamed_GetData( IDirectFBDataBuffer *thiz,
                                      unsigned int         length,
                                      void                *ret_data_ptr,
                                      unsigned int        *ret_read )
{
     unsigned int len;

     DIRECT_INTERFACE_GET_DATA( IDirectFBDataBuffer_Streamed )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_data_ptr || !length)
          return DFB_INVARG;

     direct_mutex_lock( &data->chunks_mutex );

     if (!data->chunks) {
          direct_mutex_unlock( &data->chunks_mutex );
          return data->finished ? DFB_EOF : DFB_BUFFEREMPTY;
     }

     /* Calculate maximum number of bytes to be read. */
     len = MIN( length, data->length );

     /* Read data from chunks (destructive). */
     ReadChunkData( data, ret_data_ptr, 0, len, true );

     /* Decrease total number of bytes. */
     data->length -= len;

     /* Return number of bytes read. */
     if (ret_read)
          *ret_read = len;

     direct_mutex_unlock( &data->chunks_mutex );

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_Streamed_PeekData( IDirectFBDataBuffer *thiz,
                                       unsigned int         length,
                                       int                  offset,
                                       void                *ret_data_ptr,
                                       unsigned int        *ret_read )
{
     unsigned int len;

     DIRECT_INTERFACE_GET_DATA( IDirectFBDataBuffer_Streamed )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_data_ptr || !length || offset < 0)
          return DFB_INVARG;

     direct_mutex_lock( &data->chunks_mutex );

     if (!data->chunks || (unsigned int) offset >= data->length) {
          direct_mutex_unlock( &data->chunks_mutex );
          return data->finished ? DFB_EOF : DFB_BUFFEREMPTY;
     }

     /* Calculate maximum number of bytes to be read. */
     len = MIN( length, data->length - offset );

     /* Read data from chunks (non-destructive). */
     ReadChunkData( data, ret_data_ptr, offset, len, false );

     /* Return number of bytes read. */
     if (ret_read)
          *ret_read = len;

     direct_mutex_unlock( &data->chunks_mutex );

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_Streamed_HasData( IDirectFBDataBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDataBuffer_Streamed )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     /* If there's no chunk there's no data. */
     if (!data->chunks)
          return data->finished ? DFB_EOF : DFB_BUFFEREMPTY;

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_Streamed_PutData( IDirectFBDataBuffer *thiz,
                                      const void          *data_ptr,
                                      unsigned int         length )
{
     DataChunk *chunk;

     DIRECT_INTERFACE_GET_DATA( IDirectFBDataBuffer_Streamed )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data_ptr || !length)
          return DFB_INVARG;

     /* Fail if Finish() has been called. */
     if (data->finished)
          return DFB_UNSUPPORTED;

     /* Create a chunk containing a copy of the provided data. */
     chunk = create_chunk( data_ptr, length );
     if (!chunk)
          return DFB_NOSYSTEMMEMORY;

     direct_mutex_lock( &data->chunks_mutex );

     /* Append new chunk. */
     direct_list_append( &data->chunks, &chunk->link );

     /* Increase total length. */
     data->length += length;

     direct_waitqueue_broadcast( &data->wait_condition );

     direct_mutex_unlock( &data->chunks_mutex );

     return DFB_OK;
}

DFBResult
IDirectFBDataBuffer_Streamed_Construct( IDirectFBDataBuffer *thiz,
                                        CoreDFB             *core,
                                        IDirectFB           *idirectfb )
{
     DFBResult ret;

     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBDataBuffer_Streamed )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     ret = IDirectFBDataBuffer_Construct( thiz, NULL, NULL, 0, core, idirectfb );
     if (ret)
          return ret;

     direct_mutex_init( &data->chunks_mutex );
     direct_waitqueue_init( &data->wait_condition );

     thiz->Release                = IDirectFBDataBuffer_Streamed_Release;
     thiz->Flush                  = IDirectFBDataBuffer_Streamed_Flush;
     thiz->Finish                 = IDirectFBDataBuffer_Streamed_Finish;
     thiz->SeekTo                 = IDirectFBDataBuffer_Streamed_SeekTo;
     thiz->GetPosition            = IDirectFBDataBuffer_Streamed_GetPosition;
     thiz->GetLength              = IDirectFBDataBuffer_Streamed_GetLength;
     thiz->WaitForData            = IDirectFBDataBuffer_Streamed_WaitForData;
     thiz->WaitForDataWithTimeout = IDirectFBDataBuffer_Streamed_WaitForDataWithTimeout;
     thiz->GetData                = IDirectFBDataBuffer_Streamed_GetData;
     thiz->PeekData               = IDirectFBDataBuffer_Streamed_PeekData;
     thiz->HasData                = IDirectFBDataBuffer_Streamed_HasData;
     thiz->PutData                = IDirectFBDataBuffer_Streamed_PutData;

     return DFB_OK;
}

/**********************************************************************************************************************/

static void
DestroyAllChunks( IDirectFBDataBuffer_Streamed_data *data )
{
     DataChunk *chunk, *next;

     D_ASSERT( data != NULL );

     /* Loop through list. */
     direct_list_foreach_safe (chunk, next, data->chunks) {
          /* Deallocate chunk. */
          destroy_chunk( chunk );
     }

     /* Clear lists. */
     data->chunks = NULL;
}

static void
ReadChunkData( IDirectFBDataBuffer_Streamed_data *data,
               void                              *buffer,
               unsigned int                       offset,
               unsigned int                       length,
               bool                               flush )
{
     DataChunk *chunk, *next;

     D_ASSERT( data != NULL );
     D_ASSERT( buffer != NULL );

     /* Loop through list. */
     direct_list_foreach_safe (chunk, next, data->chunks) {
          unsigned int  len;
          unsigned int  off   = 0;

          /* Data to be skipped. */
          if (offset) {
               /* Calculate number of bytes to be skipped from this chunk. */
               off = MIN( offset, chunk->length - chunk->done );

               /* Decrease number of bytes to skipped. */
               offset -= off;
          }

          /* Calculate number of bytes to be read from this chunk. */
          len = MIN( length, chunk->length - chunk->done - off );

          /* Read from this chunk. */
          if (len) {
               /* Copy as many bytes as possible. */
               direct_memcpy( buffer, (char*) chunk->data + chunk->done + off, len );

               /* Increase write pointer. */
               buffer = (char*) buffer + len;

               /* Decrease number of bytes to read. */
               length -= len;
          }

          /* Destructive read. */
          if (flush) {
               /* Increase number of consumed bytes. */
               chunk->done += len + off;

               /* Chunk completely consumed. */
               if (chunk->done == chunk->length) {
                    /* Remove the chunk from the list. */
                    direct_list_remove( &data->chunks, &chunk->link );

                    /* Deallocate chunk. */
                    destroy_chunk( chunk );
               }
          }
     }
}

static DataChunk *
create_chunk( const void *data,
              int         length )
{
     DataChunk *chunk;

     D_ASSERT( data != NULL );
     D_ASSERT( length > 0 );

     /* Allocate chunk information. */
     chunk = D_CALLOC( 1, sizeof(DataChunk) );
     if (!chunk)
          return NULL;

     /* Allocate chunk data. */
     chunk->data = D_MALLOC( length );
     if (!chunk->data) {
          D_FREE( chunk );
          return NULL;
     }

     /* Fill chunk data. */
     direct_memcpy( chunk->data, data, length );

     /* Remember chunk length. */
     chunk->length = length;

     return chunk;
}

static void
destroy_chunk( DataChunk *chunk )
{
     D_ASSERT( chunk != NULL );
     D_ASSERT( chunk->data != NULL );

     /* Deallocate chunk data. */
     D_FREE( chunk->data );

     /* Deallocate chunk information. */
     D_FREE( chunk );
}

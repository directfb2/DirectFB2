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

#include <direct/mutex.h>
#include <direct/stream.h>
#include <media/idirectfbdatabuffer.h>
#include <media/idirectfbdatabuffer_file.h>

D_DEBUG_DOMAIN( DataBuffer, "IDirectFBDataBufferF", "IDirectFBDataBuffer_File Interface" );

/**********************************************************************************************************************/

/*
 * private data struct of IDirectFBDataBuffer_File
 */
typedef struct {
     IDirectFBDataBuffer_data  base;   /* base databuffer implementation */

     DirectStream             *stream;

     DirectMutex               mutex;
} IDirectFBDataBuffer_File_data;

/**********************************************************************************************************************/

static void
IDirectFBDataBuffer_File_Destruct( IDirectFBDataBuffer *thiz )
{
     IDirectFBDataBuffer_File_data *data = thiz->priv;

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     direct_stream_destroy( data->stream );

     direct_mutex_deinit( &data->mutex );

     IDirectFBDataBuffer_Destruct( thiz );
}

static DirectResult
IDirectFBDataBuffer_File_Release( IDirectFBDataBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDataBuffer )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     if (--data->ref == 0)
          IDirectFBDataBuffer_File_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_File_Flush( IDirectFBDataBuffer *thiz )
{
     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     return DFB_UNSUPPORTED;
}

static DFBResult
IDirectFBDataBuffer_File_Finish( IDirectFBDataBuffer *thiz )
{
     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     return DFB_UNSUPPORTED;
}

static DFBResult
IDirectFBDataBuffer_File_SeekTo( IDirectFBDataBuffer *thiz,
                                 unsigned int         offset )
{
     DFBResult ret;

     DIRECT_INTERFACE_GET_DATA( IDirectFBDataBuffer_File )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!direct_stream_seekable( data->stream ))
          return DFB_UNSUPPORTED;

     direct_mutex_lock( &data->mutex );

     ret = direct_stream_seek( data->stream, offset );

     direct_mutex_unlock( &data->mutex );

     return ret;
}

static DFBResult
IDirectFBDataBuffer_File_GetPosition( IDirectFBDataBuffer *thiz,
                                      unsigned int        *ret_offset )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDataBuffer_File )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_offset)
          return DFB_INVARG;

     *ret_offset = direct_stream_offset( data->stream );

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_File_GetLength( IDirectFBDataBuffer *thiz,
                                    unsigned int        *ret_length )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDataBuffer_File )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_length)
          return DFB_INVARG;

     *ret_length = direct_stream_length( data->stream );

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_File_WaitForData( IDirectFBDataBuffer *thiz,
                                      unsigned int         length )
{
     DFBResult ret;

     DIRECT_INTERFACE_GET_DATA( IDirectFBDataBuffer_File )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     direct_mutex_lock( &data->mutex );

     ret = direct_stream_wait( data->stream, length, NULL );

     direct_mutex_unlock( &data->mutex );

     return ret;
}

static DFBResult
IDirectFBDataBuffer_File_WaitForDataWithTimeout( IDirectFBDataBuffer *thiz,
                                                 unsigned int         length,
                                                 unsigned int         seconds,
                                                 unsigned int         milli_seconds )
{
     DirectResult   ret;
     struct timeval tv;

     DIRECT_INTERFACE_GET_DATA( IDirectFBDataBuffer_File )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     tv.tv_sec  = seconds;
     tv.tv_usec = milli_seconds*1000;

     while ((ret = direct_mutex_trylock( &data->mutex ))) {
          struct timespec t, r;

          if (ret != DR_BUSY)
               return ret;

          t.tv_sec  = 0;
          t.tv_nsec = 10000;
          nanosleep( &t, &r );

          tv.tv_usec -= (t.tv_nsec - r.tv_nsec + 500) / 1000;
          if (tv.tv_usec < 0) {
               if (tv.tv_sec < 1)
                    return DFB_TIMEOUT;

               tv.tv_sec--;
               tv.tv_usec += 999999;
          }
     }

     ret = direct_stream_wait( data->stream, length, &tv );

     direct_mutex_unlock( &data->mutex );

     return ret;
}

static DFBResult
IDirectFBDataBuffer_File_GetData( IDirectFBDataBuffer *thiz,
                                  unsigned int         length,
                                  void                *ret_data_ptr,
                                  unsigned int        *ret_read )
{
     DFBResult ret;

     DIRECT_INTERFACE_GET_DATA( IDirectFBDataBuffer_File )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_data_ptr || !length)
          return DFB_INVARG;

     direct_mutex_lock( &data->mutex );

     ret = direct_stream_read( data->stream, length, ret_data_ptr, ret_read );

     direct_mutex_unlock( &data->mutex );

     return ret;
}

static DFBResult
IDirectFBDataBuffer_File_PeekData( IDirectFBDataBuffer *thiz,
                                   unsigned int         length,
                                   int                  offset,
                                   void                *ret_data_ptr,
                                   unsigned int        *ret_read )
{
     DFBResult ret;

     DIRECT_INTERFACE_GET_DATA( IDirectFBDataBuffer_File )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_data_ptr || !length)
          return DFB_INVARG;

     direct_mutex_lock( &data->mutex );

     ret = direct_stream_peek( data->stream, length, offset, ret_data_ptr, ret_read );

     direct_mutex_unlock( &data->mutex );

     return ret;
}

static DFBResult
IDirectFBDataBuffer_File_HasData( IDirectFBDataBuffer *thiz )
{
     struct timeval tv = { 0, 0 };

     DIRECT_INTERFACE_GET_DATA( IDirectFBDataBuffer_File )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     return direct_stream_wait( data->stream, 1, &tv );
}

static DFBResult
IDirectFBDataBuffer_File_PutData( IDirectFBDataBuffer *thiz,
                                  const void          *data_ptr,
                                  unsigned int         length )
{
     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     return DFB_UNSUPPORTED;
}

DFBResult
IDirectFBDataBuffer_File_Construct( IDirectFBDataBuffer *thiz,
                                    const char          *filename,
                                    CoreDFB             *core,
                                    IDirectFB           *idirectfb )
{
     DFBResult ret;

     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBDataBuffer_File )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     IDirectFBDataBuffer_Construct( thiz, filename, NULL, 0, core, idirectfb );

     ret = direct_stream_create( filename, &data->stream );
     if (ret) {
          D_DERROR( ret, "IDirectFBDataBufferF: Failed to create stream '%s'!\n", filename );
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     direct_mutex_init( &data->mutex );

     thiz->Release                = IDirectFBDataBuffer_File_Release;
     thiz->Flush                  = IDirectFBDataBuffer_File_Flush;
     thiz->Finish                 = IDirectFBDataBuffer_File_Finish;
     thiz->SeekTo                 = IDirectFBDataBuffer_File_SeekTo;
     thiz->GetPosition            = IDirectFBDataBuffer_File_GetPosition;
     thiz->GetLength              = IDirectFBDataBuffer_File_GetLength;
     thiz->WaitForData            = IDirectFBDataBuffer_File_WaitForData;
     thiz->WaitForDataWithTimeout = IDirectFBDataBuffer_File_WaitForDataWithTimeout;
     thiz->GetData                = IDirectFBDataBuffer_File_GetData;
     thiz->PeekData               = IDirectFBDataBuffer_File_PeekData;
     thiz->HasData                = IDirectFBDataBuffer_File_HasData;
     thiz->PutData                = IDirectFBDataBuffer_File_PutData;

     return DFB_OK;
}

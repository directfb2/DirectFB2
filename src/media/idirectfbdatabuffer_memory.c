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

#include <direct/memcpy.h>
#include <direct/util.h>
#include <media/idirectfbdatabuffer.h>
#include <media/idirectfbdatabuffer_memory.h>

D_DEBUG_DOMAIN( DataBuffer, "IDirectFBDataBufferM", "IDirectFBDataBuffer_Memory Interface" );

/**********************************************************************************************************************/

/*
 * private data struct of IDirectFBDataBuffer_Memory
 */
typedef struct {
     IDirectFBDataBuffer_data  base;

     const void               *buffer;
     unsigned int              length;

     unsigned int              pos;
} IDirectFBDataBuffer_Memory_data;

/**********************************************************************************************************************/

static void
IDirectFBDataBuffer_Memory_Destruct( IDirectFBDataBuffer *thiz )
{
     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     IDirectFBDataBuffer_Destruct( thiz );
}

static DirectResult
IDirectFBDataBuffer_Memory_Release( IDirectFBDataBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDataBuffer )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     if (--data->ref == 0)
          IDirectFBDataBuffer_Memory_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_Memory_Flush( IDirectFBDataBuffer *thiz )
{
     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     return DFB_UNSUPPORTED;
}

static DFBResult
IDirectFBDataBuffer_Memory_Finish( IDirectFBDataBuffer *thiz )
{
     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     return DFB_UNSUPPORTED;
}

static DFBResult
IDirectFBDataBuffer_Memory_SeekTo( IDirectFBDataBuffer *thiz,
                                   unsigned int         offset )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDataBuffer_Memory )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     if (offset >= data->length)
          return DFB_INVARG;

     data->pos = offset;

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_Memory_GetPosition( IDirectFBDataBuffer *thiz,
                                        unsigned int        *ret_offset )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDataBuffer_Memory )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_offset)
          return DFB_INVARG;

     *ret_offset = data->pos;

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_Memory_GetLength( IDirectFBDataBuffer *thiz,
                                      unsigned int        *ret_length )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDataBuffer_Memory )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_length)
          return DFB_INVARG;

     *ret_length = data->length;

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_Memory_WaitForData( IDirectFBDataBuffer *thiz,
                                        unsigned int         length )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDataBuffer_Memory )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     if (data->pos + length > data->length)
          return DFB_EOF;

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_Memory_WaitForDataWithTimeout( IDirectFBDataBuffer *thiz,
                                                   unsigned int         length,
                                                   unsigned int         seconds,
                                                   unsigned int         milli_seconds )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDataBuffer_Memory )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     if (data->pos + length > data->length)
          return DFB_EOF;

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_Memory_GetData( IDirectFBDataBuffer *thiz,
                                    unsigned int         length,
                                    void                *ret_data_ptr,
                                    unsigned int        *ret_read )
{
     unsigned int size;

     DIRECT_INTERFACE_GET_DATA( IDirectFBDataBuffer_Memory )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_data_ptr || !length)
          return DFB_INVARG;

     if (data->pos >= data->length)
          return DFB_EOF;

     size = MIN( length, data->length - data->pos );

     direct_memcpy( ret_data_ptr, (char*) data->buffer + data->pos, size );

     data->pos += size;

     if (ret_read)
          *ret_read = size;

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_Memory_PeekData( IDirectFBDataBuffer *thiz,
                                     unsigned int         length,
                                     int                  offset,
                                     void                *ret_data_ptr,
                                     unsigned int        *ret_read )
{
     unsigned int size;

     DIRECT_INTERFACE_GET_DATA( IDirectFBDataBuffer_Memory )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_data_ptr || !length)
          return DFB_INVARG;

     if (data->pos + offset >= data->length)
          return DFB_EOF;

     size = MIN( length, data->length - data->pos - offset );

     direct_memcpy( ret_data_ptr, (char*) data->buffer + data->pos + offset, size );

     if (ret_read)
          *ret_read = size;

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_Memory_HasData( IDirectFBDataBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDataBuffer_Memory )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     if (data->pos >= data->length)
          return DFB_EOF;

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_Memory_PutData( IDirectFBDataBuffer *thiz,
                                    const void          *data_ptr,
                                    unsigned int         length )
{
     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     return DFB_UNSUPPORTED;
}

DFBResult
IDirectFBDataBuffer_Memory_Construct( IDirectFBDataBuffer *thiz,
                                      const void          *buffer,
                                      unsigned int         length,
                                      CoreDFB             *core,
                                      IDirectFB           *idirectfb )
{
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBDataBuffer_Memory )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     IDirectFBDataBuffer_Construct( thiz, NULL, buffer, length, core, idirectfb );

     data->buffer = buffer;
     data->length = length;

     thiz->Release                = IDirectFBDataBuffer_Memory_Release;
     thiz->Flush                  = IDirectFBDataBuffer_Memory_Flush;
     thiz->Finish                 = IDirectFBDataBuffer_Memory_Finish;
     thiz->SeekTo                 = IDirectFBDataBuffer_Memory_SeekTo;
     thiz->GetPosition            = IDirectFBDataBuffer_Memory_GetPosition;
     thiz->GetLength              = IDirectFBDataBuffer_Memory_GetLength;
     thiz->WaitForData            = IDirectFBDataBuffer_Memory_WaitForData;
     thiz->WaitForDataWithTimeout = IDirectFBDataBuffer_Memory_WaitForDataWithTimeout;
     thiz->GetData                = IDirectFBDataBuffer_Memory_GetData;
     thiz->PeekData               = IDirectFBDataBuffer_Memory_PeekData;
     thiz->HasData                = IDirectFBDataBuffer_Memory_HasData;
     thiz->PutData                = IDirectFBDataBuffer_Memory_PutData;

     return DFB_OK;
}

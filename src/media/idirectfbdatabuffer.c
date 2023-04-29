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

#include <media/idirectfbdatabuffer.h>
#include <media/idirectfbfont.h>
#include <media/idirectfbimageprovider.h>
#include <media/idirectfbvideoprovider.h>

D_DEBUG_DOMAIN( DataBuffer, "IDirectFBDataBuffer", "IDirectFBDataBuffer Interface" );

/**********************************************************************************************************************/

void
IDirectFBDataBuffer_Destruct( IDirectFBDataBuffer *thiz )
{
     IDirectFBDataBuffer_data *data = thiz->priv;

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     if (data->filename)
          D_FREE( data->filename );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DirectResult
IDirectFBDataBuffer_AddRef( IDirectFBDataBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDataBuffer )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     data->ref++;

     return DFB_OK;
}

static DirectResult
IDirectFBDataBuffer_Release( IDirectFBDataBuffer *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDataBuffer )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     if (--data->ref == 0)
          IDirectFBDataBuffer_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBDataBuffer_Flush( IDirectFBDataBuffer *thiz )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_Finish( IDirectFBDataBuffer *thiz )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_SeekTo( IDirectFBDataBuffer *thiz,
                            unsigned int         offset )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_GetPosition( IDirectFBDataBuffer *thiz,
                                 unsigned int        *ret_offset )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_GetLength( IDirectFBDataBuffer *thiz,
                               unsigned int        *ret_length )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_WaitForData( IDirectFBDataBuffer *thiz,
                                 unsigned int         length )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_WaitForDataWithTimeout( IDirectFBDataBuffer *thiz,
                                            unsigned int         length,
                                            unsigned int         seconds,
                                            unsigned int         milli_seconds )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_GetData( IDirectFBDataBuffer *thiz,
                             unsigned int         length,
                             void                *ret_data_ptr,
                             unsigned int        *ret_read )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_PeekData( IDirectFBDataBuffer *thiz,
                              unsigned int         length,
                              int                  offset,
                              void                *ret_data_ptr,
                              unsigned int        *ret_read )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_HasData( IDirectFBDataBuffer *thiz )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_PutData( IDirectFBDataBuffer *thiz,
                             const void          *data_ptr,
                             unsigned int         length )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBDataBuffer_CreateImageProvider( IDirectFBDataBuffer     *thiz,
                                         IDirectFBImageProvider **ret_interface )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDataBuffer )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     /* Check arguments. */
     if (!ret_interface)
          return DFB_INVARG;

     return IDirectFBImageProvider_CreateFromBuffer( thiz, data->core, data->idirectfb, ret_interface );
}

static DFBResult
IDirectFBDataBuffer_CreateVideoProvider( IDirectFBDataBuffer     *thiz,
                                         IDirectFBVideoProvider **ret_interface )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDataBuffer )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     /* Check arguments. */
     if (!ret_interface)
          return DFB_INVARG;

     return IDirectFBVideoProvider_CreateFromBuffer( thiz, data->core, data->idirectfb, ret_interface );
}

static DFBResult
IDirectFBDataBuffer_CreateFont( IDirectFBDataBuffer       *thiz,
                                const DFBFontDescription  *desc,
                                IDirectFBFont            **ret_interface )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDataBuffer )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     /* Check arguments. */
     if (!desc || !ret_interface)
          return DFB_INVARG;

     return IDirectFBFont_CreateFromBuffer( thiz, data->core, desc, ret_interface );
}

DFBResult
IDirectFBDataBuffer_Construct( IDirectFBDataBuffer *thiz,
                               const char          *filename,
                               const void          *buffer,
                               unsigned int         length,
                               CoreDFB             *core,
                               IDirectFB           *idirectfb )
{
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBDataBuffer )

     D_DEBUG_AT( DataBuffer, "%s( %p )\n", __FUNCTION__, thiz );

     data->ref       = 1;
     data->core      = core;
     data->idirectfb = idirectfb;

     if (filename)
          data->filename = D_STRDUP( filename );

     if (buffer) {
          data->buffer = (void*) buffer;
          data->length = length;
     }

     thiz->AddRef                 = IDirectFBDataBuffer_AddRef;
     thiz->Release                = IDirectFBDataBuffer_Release;
     thiz->Flush                  = IDirectFBDataBuffer_Flush;
     thiz->Finish                 = IDirectFBDataBuffer_Finish;
     thiz->SeekTo                 = IDirectFBDataBuffer_SeekTo;
     thiz->GetPosition            = IDirectFBDataBuffer_GetPosition;
     thiz->GetLength              = IDirectFBDataBuffer_GetLength;
     thiz->WaitForData            = IDirectFBDataBuffer_WaitForData;
     thiz->WaitForDataWithTimeout = IDirectFBDataBuffer_WaitForDataWithTimeout;
     thiz->GetData                = IDirectFBDataBuffer_GetData;
     thiz->PeekData               = IDirectFBDataBuffer_PeekData;
     thiz->HasData                = IDirectFBDataBuffer_HasData;
     thiz->PutData                = IDirectFBDataBuffer_PutData;
     thiz->CreateImageProvider    = IDirectFBDataBuffer_CreateImageProvider;
     thiz->CreateVideoProvider    = IDirectFBDataBuffer_CreateVideoProvider;
     thiz->CreateFont             = IDirectFBDataBuffer_CreateFont;

     return DFB_OK;
}

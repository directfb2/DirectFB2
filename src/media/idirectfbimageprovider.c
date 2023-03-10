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
#include <media/idirectfbimageprovider.h>

D_DEBUG_DOMAIN( ImageProvider, "IDirectFBImageProvider", "IDirectFBImageProvider Interface" );

/**********************************************************************************************************************/

static DirectResult
IDirectFBImageProvider_AddRef( IDirectFBImageProvider *thiz )
{
     return DFB_UNIMPLEMENTED;
}

static DirectResult
IDirectFBImageProvider_Release( IDirectFBImageProvider *thiz )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBImageProvider_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                              DFBSurfaceDescription  *ret_desc )
{
     if (!ret_desc)
          return DFB_INVARG;

     ret_desc->flags = DSDESC_NONE;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBImageProvider_GetImageDescription( IDirectFBImageProvider *thiz,
                                            DFBImageDescription    *ret_desc )
{
     if (!ret_desc)
          return DFB_INVARG;

     ret_desc->caps = DICAPS_NONE;

     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBImageProvider_RenderTo( IDirectFBImageProvider *thiz,
                                 IDirectFBSurface       *destination,
                                 const DFBRectangle     *destination_rect )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBImageProvider_SetRenderCallback( IDirectFBImageProvider *thiz,
                                          DIRenderCallback        callback,
                                          void                   *ctx )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBImageProvider_SetRenderFlags( IDirectFBImageProvider *thiz,
                                       DIRenderFlags           flags )
{
     return DFB_UNIMPLEMENTED;
}

static void
IDirectFBImageProvider_Construct( IDirectFBImageProvider *thiz )
{
     D_DEBUG_AT( ImageProvider, "%s( %p )\n", __FUNCTION__, thiz );

     thiz->AddRef                = IDirectFBImageProvider_AddRef;
     thiz->Release               = IDirectFBImageProvider_Release;
     thiz->GetSurfaceDescription = IDirectFBImageProvider_GetSurfaceDescription;
     thiz->GetImageDescription   = IDirectFBImageProvider_GetImageDescription;
     thiz->RenderTo              = IDirectFBImageProvider_RenderTo;
     thiz->SetRenderCallback     = IDirectFBImageProvider_SetRenderCallback;
     thiz->SetRenderFlags        = IDirectFBImageProvider_SetRenderFlags;
}

DFBResult
IDirectFBImageProvider_CreateFromBuffer( IDirectFBDataBuffer     *buffer,
                                         CoreDFB                 *core,
                                         IDirectFB               *idirectfb,
                                         IDirectFBImageProvider **ret_interface )
{
     DFBResult                            ret;
     DirectInterfaceFuncs                *funcs = NULL;
     IDirectFBDataBuffer_data            *buffer_data;
     IDirectFBImageProvider              *iface;
     IDirectFBImageProvider_ProbeContext  ctx;

     D_DEBUG_AT( ImageProvider, "%s( %p )\n", __FUNCTION__, buffer );

     /* Get the private information of the data buffer. */
     buffer_data = buffer->priv;
     if (!buffer_data)
          return DFB_DEAD;

     /* Clear probe context header. */
     memset( ctx.header, 0, sizeof(ctx.header) );

     /* Provide a fallback for image providers without data buffer support. */
     ctx.filename = buffer_data->filename;

     /* Wait until 32 bytes are available. */
     ret = buffer->WaitForData( buffer, sizeof(ctx.header) );
     if (ret)
          return ret;

     /* Read the first 32 bytes. */
     buffer->PeekData( buffer, sizeof(ctx.header), 0, ctx.header, NULL );

     /* Find a suitable implementation. */
     ret = DirectGetInterface( &funcs, "IDirectFBImageProvider", NULL, DirectProbeInterface, &ctx );
     if (ret)
          return ret;

     DIRECT_ALLOCATE_INTERFACE( iface, IDirectFBImageProvider );

     /* Initialize interface pointers. */
     IDirectFBImageProvider_Construct( iface );

     /* Construct the interface. */
     ret = funcs->Construct( iface, buffer, core, idirectfb );
     if (ret)
          return ret;

     *ret_interface = iface;

     return DFB_OK;
}

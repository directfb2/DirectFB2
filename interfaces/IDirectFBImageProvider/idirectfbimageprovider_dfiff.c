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

#include <dfiff.h>
#include <direct/filesystem.h>
#include <display/idirectfbsurface.h>
#include <media/idirectfbdatabuffer.h>
#include <media/idirectfbimageprovider.h>

D_DEBUG_DOMAIN( ImageProvider_DFIFF, "ImageProvider/DFIFF", "DFIFF Image Provider" );

static DFBResult Probe    ( IDirectFBImageProvider_ProbeContext *ctx );

static DFBResult Construct( IDirectFBImageProvider              *thiz,
                            IDirectFBDataBuffer                 *buffer,
                            CoreDFB                             *core,
                            IDirectFB                           *idirectfb );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBImageProvider, DFIFF )

/**********************************************************************************************************************/

typedef struct {
     int                    ref;                     /* reference counter */

     IDirectFB             *idirectfb;

     void                  *ptr;                     /* pointer to raw file data (mapped) */
     int                    len;                     /* data length, i.e. file size */

     DFBSurfaceDescription  desc;

     DIRenderCallback       render_callback;
     void                  *render_callback_context;
} IDirectFBImageProvider_DFIFF_data;

#define DFIFF_FLAG_PREMULTIPLIED 0x02

/**********************************************************************************************************************/

static void
IDirectFBImageProvider_DFIFF_Destruct( IDirectFBImageProvider *thiz )
{
     IDirectFBImageProvider_DFIFF_data *data = thiz->priv;

     D_DEBUG_AT( ImageProvider_DFIFF, "%s( %p )\n", __FUNCTION__, thiz );

     direct_file_unmap( data->ptr, data->len );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DirectResult
IDirectFBImageProvider_DFIFF_AddRef( IDirectFBImageProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_DFIFF )

     D_DEBUG_AT( ImageProvider_DFIFF, "%s( %p )\n", __FUNCTION__, thiz );

     data->ref++;

     return DFB_OK;
}

static DirectResult
IDirectFBImageProvider_DFIFF_Release( IDirectFBImageProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_DFIFF )

     D_DEBUG_AT( ImageProvider_DFIFF, "%s( %p )\n", __FUNCTION__, thiz );

     if (--data->ref == 0)
          IDirectFBImageProvider_DFIFF_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_DFIFF_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                    DFBSurfaceDescription  *ret_desc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_DFIFF )

     D_DEBUG_AT( ImageProvider_DFIFF, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_desc)
          return DFB_INVARG;

     *ret_desc = data->desc;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_DFIFF_GetImageDescription( IDirectFBImageProvider *thiz,
                                                  DFBImageDescription    *ret_desc )
{
     const DFIFFHeader *header;

     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_DFIFF )

     D_DEBUG_AT( ImageProvider_DFIFF, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_desc)
          return DFB_INVARG;

     header = data->ptr;

     ret_desc->caps = DICAPS_NONE;

     if (DFB_PIXELFORMAT_HAS_ALPHA( header->format ))
          ret_desc->caps |= DICAPS_ALPHACHANNEL;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_DFIFF_RenderTo( IDirectFBImageProvider *thiz,
                                       IDirectFBSurface       *destination,
                                       const DFBRectangle     *dest_rect )
{
     DFBResult              ret;
     IDirectFBSurface_data *dst_data;
     const DFIFFHeader     *header;
     DFBRectangle           rect;
     DFBRectangle           clipped;
     DFBSurfaceCapabilities caps;
     DFBSurfacePixelFormat  format;
     bool                   dfiff_premultiplied = false;
     bool                   dest_premultiplied  = false;

     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_DFIFF )

     D_DEBUG_AT( ImageProvider_DFIFF, "%s( %p )\n", __FUNCTION__, thiz );

     if (!destination)
          return DFB_INVARG;

     dst_data = destination->priv;
     if (!dst_data)
          return DFB_DEAD;

     if (dest_rect) {
          if (dest_rect->w < 1 || dest_rect->h < 1)
               return DFB_INVARG;

          rect = *dest_rect;
          rect.x += dst_data->area.wanted.x;
          rect.y += dst_data->area.wanted.y;
     }
     else
          rect = dst_data->area.wanted;

     clipped = rect;

     if (!dfb_rectangle_intersect( &clipped, &dst_data->area.current ))
          return DFB_INVAREA;

     header = data->ptr;

     if (header->flags & DFIFF_FLAG_PREMULTIPLIED)
          dfiff_premultiplied = true;

     ret = destination->GetCapabilities( destination, &caps );
     if (ret)
          return ret;

     if (caps & DSCAPS_PREMULTIPLIED)
          dest_premultiplied = true;

     ret = destination->GetPixelFormat( destination, &format );
     if (ret)
          return ret;

     if (DFB_RECTANGLE_EQUAL( rect, clipped )                                             &&
         rect.w == header->width  && rect.h == header->height && format == header->format &&
         dfiff_premultiplied == dest_premultiplied) {
          ret = destination->Write( destination, &rect, data->ptr + sizeof(DFIFFHeader), header->pitch );
          if (ret)
               return ret;
     }
     else {
          IDirectFBSurface      *source;
          DFBSurfaceDescription  desc;
          DFBRegion              clip = DFB_REGION_INIT_FROM_RECTANGLE( &clipped );
          DFBRegion              old_clip;

          desc = data->desc;

          desc.flags                 |= DSDESC_PREALLOCATED;
          desc.preallocated[0].data   = data->ptr + sizeof(DFIFFHeader);
          desc.preallocated[0].pitch  = header->pitch;

          ret = data->idirectfb->CreateSurface( data->idirectfb, &desc, &source );
          if (ret)
               return ret;

          if (DFB_PIXELFORMAT_HAS_ALPHA( desc.pixelformat )) {
               if (dest_premultiplied && !dfiff_premultiplied)
                    destination->SetBlittingFlags( destination, DSBLIT_SRC_PREMULTIPLY );
               else if (!dest_premultiplied && dfiff_premultiplied)
                    destination->SetBlittingFlags( destination, DSBLIT_DEMULTIPLY );
          }

          destination->GetClip( destination, &old_clip );

          destination->SetClip( destination, &clip );

          destination->StretchBlit( destination, source, NULL, &rect );

          destination->SetClip( destination, &old_clip );

          destination->SetBlittingFlags( destination, DSBLIT_NOFX );

          destination->ReleaseSource( destination );

          source->Release( source );
     }

     if (data->render_callback) {
          DFBRectangle rect = { 0, 0, clipped.w, clipped.h };

          data->render_callback( &rect, data->render_callback_context );
     }

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_DFIFF_SetRenderCallback( IDirectFBImageProvider *thiz,
                                                DIRenderCallback        callback,
                                                void                   *ctx )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_DFIFF )

     D_DEBUG_AT( ImageProvider_DFIFF, "%s( %p )\n", __FUNCTION__, thiz );

     data->render_callback         = callback;
     data->render_callback_context = ctx;

     return DFB_OK;
}

/**********************************************************************************************************************/

static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx )
{
     if (!strncmp( (const char*) ctx->header, "DFIFF", 5 ))
          return DFB_OK;

     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           IDirectFBDataBuffer    *buffer,
           CoreDFB                *core,
           IDirectFB              *idirectfb )
{
     DFBResult                 ret;
     DirectFile                fd;
     DirectFileInfo            info;
     void                     *ptr;
     const DFIFFHeader        *header;
     IDirectFBDataBuffer_data *buffer_data = buffer->priv;

     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBImageProvider_DFIFF )

     D_DEBUG_AT( ImageProvider_DFIFF, "%s( %p )\n", __FUNCTION__, thiz );

     /* Check for valid filename. */
     if (!buffer_data->filename)
          return DFB_UNSUPPORTED;

     /* Open the file. */
     ret = direct_file_open( &fd, buffer_data->filename, O_RDONLY, 0 );
     if (ret) {
          D_DERROR( ret, "ImageProvider/DFIFF: Failed to open '%s'!\n", buffer_data->filename );
          return ret;
     }

     /* Query file size. */
     ret = direct_file_get_info( &fd, &info );
     if (ret) {
          D_DERROR( ret, "ImageProvider/DFIFF: Failed during get_info() of '%s'!\n", buffer_data->filename );
          goto error;
     }

     /* Memory-mapped file. */
     ret = direct_file_map( &fd, NULL, 0, info.size, DFP_READ, &ptr );
     if (ret) {
          D_DERROR( ret, "ImageProvider/DFIFF: Failed during mmap() of '%s'!\n", buffer_data->filename );
          goto error;
     }

     direct_file_close( &fd );

     header = ptr;

     data->ref              = 1;
     data->idirectfb        = idirectfb;
     data->ptr              = ptr;
     data->len              = info.size;
     data->desc.flags       = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
     data->desc.width       = header->width;
     data->desc.height      = header->height;
     data->desc.pixelformat = header->format;

     thiz->AddRef                = IDirectFBImageProvider_DFIFF_AddRef;
     thiz->Release               = IDirectFBImageProvider_DFIFF_Release;
     thiz->GetSurfaceDescription = IDirectFBImageProvider_DFIFF_GetSurfaceDescription;
     thiz->GetImageDescription   = IDirectFBImageProvider_DFIFF_GetImageDescription;
     thiz->RenderTo              = IDirectFBImageProvider_DFIFF_RenderTo;
     thiz->SetRenderCallback     = IDirectFBImageProvider_DFIFF_SetRenderCallback;

     return DFB_OK;

error:
     direct_file_close( &fd );

     DIRECT_DEALLOCATE_INTERFACE( thiz );

     return ret;
}

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

#include <dfvff.h>
#include <direct/filesystem.h>
#include <direct/list.h>
#include <direct/memcpy.h>
#include <direct/thread.h>
#include <display/idirectfbsurface.h>
#include <media/idirectfbdatabuffer.h>
#include <media/idirectfbvideoprovider.h>

D_DEBUG_DOMAIN( VideoProvider_DFVFF, "VideoProvider/DFVFF", "DFVFF Video Provider" );

static DFBResult Probe    ( IDirectFBVideoProvider_ProbeContext *ctx );

static DFBResult Construct( IDirectFBVideoProvider              *thiz,
                            IDirectFBDataBuffer                 *buffer,
                            CoreDFB                             *core,
                            IDirectFB                           *idirectfb );


#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBVideoProvider, DFVFF )

/**********************************************************************************************************************/

typedef struct {
     DirectLink            link;
     IDirectFBEventBuffer *buffer;
} EventLink;

typedef struct {
     int                            ref;                    /* reference counter */

     IDirectFB                     *idirectfb;

     void                          *ptr;                    /* pointer to raw file data (mapped) */
     int                            len;                    /* data length, i.e. file size */

     DFBSurfaceDescription          desc;
     double                         rate;

     DFBVideoProviderStatus         status;
     double                         speed;
     DFBVideoProviderPlaybackFlags  flags;
     int                            frame_size;
     long                           nb_frames;
     long                           frame;

     DirectThread                  *thread;
     DirectMutex                    lock;
     DirectWaitQueue                cond;

     bool                           seeked;

     IDirectFBSurface              *dest;
     DFBRectangle                   rect;

     DVFrameCallback                frame_callback;
     void                          *frame_callback_context;

     DirectLink                    *events;
     DFBVideoProviderEventType      events_mask;
     DirectMutex                    events_lock;
} IDirectFBVideoProvider_DFVFF_data;

/**********************************************************************************************************************/

static void
dispatch_event( IDirectFBVideoProvider_DFVFF_data *data,
                DFBVideoProviderEventType          type )
{
     EventLink             *link;
     DFBVideoProviderEvent  event;

     if (!data->events || !(data->events_mask & type))
          return;

     event.clazz = DFEC_VIDEOPROVIDER;
     event.type  = type;

     direct_mutex_lock( &data->events_lock );

     direct_list_foreach (link, data->events) {
          link->buffer->PostEvent( link->buffer, DFB_EVENT(&event) );
     }

     direct_mutex_unlock( &data->events_lock );
}

static void *
DFVFFVideo( DirectThread *thread,
            void         *arg )
{
     DFBResult                          ret;
     IDirectFBSurface                  *source;
     void                              *source_ptr;
     int                                source_pitch;
     void                              *frame_ptr;
     long                               start_frame;
     long long                          start;
     int                                drop = 0;
     IDirectFBVideoProvider_DFVFF_data *data = arg;

     ret = data->idirectfb->CreateSurface( data->idirectfb, &data->desc, &source );
     if (ret)
          return NULL;

     frame_ptr = data->ptr + sizeof(DFVFFHeader);

     start_frame = 0;
     start = direct_clock_get_abs_millis();

     dispatch_event( data, DVPET_STARTED );

     while (data->status != DVSTATE_STOP) {
          direct_mutex_lock( &data->lock );

          if (drop) {
               data->frame += drop;
               data->frame  = MIN( data->frame, data->nb_frames - 1 );

               drop = 0;

               if (data->seeked) {
                    direct_mutex_unlock( &data->lock );
                    continue;
               }
          }
          else {
               if (data->seeked) {
                    frame_ptr = data->ptr + sizeof(DFVFFHeader) + data->frame * data->frame_size;

                    start_frame = data->frame;
                    start = direct_clock_get_abs_millis();

                    if (data->status == DVSTATE_FINISHED)
                         data->status = DVSTATE_PLAY;

                    data->seeked = false;
               }

               source->Lock( source, DSLF_WRITE, &source_ptr, &source_pitch );
               direct_memcpy( source_ptr, frame_ptr, data->frame_size );
               source->Unlock( source );

               data->dest->StretchBlit( data->dest, source, NULL, &data->rect );

               if (data->frame_callback)
                    data->frame_callback( data->frame_callback_context );
          }

          if (!data->speed) {
               direct_waitqueue_wait( &data->cond, &data->lock );

               if (data->seeked || data->status == DVSTATE_STOP) {
                    direct_mutex_unlock( &data->lock );
                    continue;
               }

               start_frame = data->frame + 1;
               start = direct_clock_get_abs_millis();
          }
          else {
               double rate  = data->rate / 1000.0;
               long   delay = direct_clock_get_abs_millis() - start;
               long   frame = delay * rate + start_frame;

               if ( data->frame < frame ) {
                    drop = frame - data->frame;
                    direct_mutex_unlock( &data->lock );
                    continue;
               }

               delay = (data->frame - start_frame + 1) / rate - delay;

               direct_waitqueue_wait_timeout( &data->cond, &data->lock, delay * 1000 );

               if (data->seeked) {
                    direct_mutex_unlock( &data->lock );
                    continue;
               }
          }

          data->frame++;

          if (data->frame == data->nb_frames) {
               if (data->flags & DVPLAY_LOOPING) {
                    data->frame = 0;

                    frame_ptr = data->ptr + sizeof(DFVFFHeader);

                    start_frame = 0;
                    start = direct_clock_get_abs_millis();
               }
               else {
                    data->status = DVSTATE_FINISHED;
                    dispatch_event( data, DVPET_FINISHED );
                    direct_waitqueue_wait( &data->cond, &data->lock );
               }
          }
          else
               frame_ptr = data->ptr + sizeof(DFVFFHeader) + data->frame * data->frame_size;

          direct_mutex_unlock( &data->lock );
     }

     source->Release( source );

     return NULL;
}

/**********************************************************************************************************************/

static void
IDirectFBVideoProvider_DFVFF_Destruct( IDirectFBVideoProvider *thiz )
{
     EventLink                         *link, *tmp;
     IDirectFBVideoProvider_DFVFF_data *data = thiz->priv;

     D_DEBUG_AT( VideoProvider_DFVFF, "%s( %p )\n", __FUNCTION__, thiz );

     thiz->Stop( thiz );

     direct_waitqueue_deinit( &data->cond );
     direct_mutex_deinit( &data->lock );

     direct_list_foreach_safe (link, tmp, data->events) {
          direct_list_remove( &data->events, &link->link );
          link->buffer->Release( link->buffer );
          D_FREE( link );
     }

     direct_mutex_deinit( &data->events_lock );

     direct_file_unmap( data->ptr, data->len );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DirectResult
IDirectFBVideoProvider_DFVFF_AddRef( IDirectFBVideoProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_DFVFF )

     D_DEBUG_AT( VideoProvider_DFVFF, "%s( %p )\n", __FUNCTION__, thiz );

     data->ref++;

     return DFB_OK;
}

static DirectResult
IDirectFBVideoProvider_DFVFF_Release( IDirectFBVideoProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_DFVFF )

     D_DEBUG_AT( VideoProvider_DFVFF, "%s( %p )\n", __FUNCTION__, thiz );

     if (--data->ref == 0)
          IDirectFBVideoProvider_DFVFF_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_DFVFF_GetCapabilities( IDirectFBVideoProvider       *thiz,
                                              DFBVideoProviderCapabilities *ret_caps )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_DFVFF )

     D_DEBUG_AT( VideoProvider_DFVFF, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_caps)
          return DFB_INVARG;

     *ret_caps = DVCAPS_BASIC | DVCAPS_SEEK | DVCAPS_SCALE | DVCAPS_SPEED;

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_DFVFF_GetSurfaceDescription( IDirectFBVideoProvider *thiz,
                                                    DFBSurfaceDescription  *ret_desc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_DFVFF )

     D_DEBUG_AT( VideoProvider_DFVFF, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_desc)
          return DFB_INVARG;

     *ret_desc = data->desc;

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_DFVFF_GetStreamDescription( IDirectFBVideoProvider *thiz,
                                                   DFBStreamDescription   *ret_desc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_DFVFF )

     D_DEBUG_AT( VideoProvider_DFVFF, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_desc)
          return DFB_INVARG;

     ret_desc->caps = DVSCAPS_VIDEO;

     snprintf( ret_desc->video.encoding, DFB_STREAM_DESC_ENCODING_LENGTH, "rawvideo" );

     ret_desc->video.framerate = data->rate;
     ret_desc->video.aspect    = (double) data->desc.width / data->desc.height;
     ret_desc->video.bitrate   = data->rate * data->frame_size;

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_DFVFF_PlayTo( IDirectFBVideoProvider *thiz,
                                     IDirectFBSurface       *destination,
                                     const DFBRectangle     *dest_rect,
                                     DVFrameCallback         callback,
                                     void                   *ctx )
{
     IDirectFBSurface_data *dst_data;
     DFBRectangle           rect;

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_DFVFF )

     D_DEBUG_AT( VideoProvider_DFVFF, "%s( %p )\n", __FUNCTION__, thiz );

     if (!destination)
          return DFB_INVARG;

     dst_data = destination->priv;
     if (!dst_data)
          return DFB_DEAD;

     if (dest_rect) {
          if (dest_rect->w < 1 || dest_rect->h < 1)
               return DFB_INVARG;

          rect = *dest_rect;
     }
     else
          rect = dst_data->area.wanted;

     if (data->thread)
          return DFB_OK;

     direct_mutex_lock( &data->lock );

     data->dest                   = destination;
     data->rect                   = rect;
     data->frame_callback         = callback;
     data->frame_callback_context = ctx;

     data->status = DVSTATE_PLAY;

     data->thread = direct_thread_create( DTT_DEFAULT, DFVFFVideo, data, "DFVFF Video" );

     direct_mutex_unlock( &data->lock );

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_DFVFF_Stop( IDirectFBVideoProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_DFVFF )

     D_DEBUG_AT( VideoProvider_DFVFF, "%s( %p )\n", __FUNCTION__, thiz );

     if (data->status == DVSTATE_STOP)
          return DFB_OK;

     data->status = DVSTATE_STOP;

     if (data->thread) {
          direct_waitqueue_signal( &data->cond );
          direct_thread_join( data->thread );
          direct_thread_destroy( data->thread );
          data->thread = NULL;
     }

     data->frame = 0;

     dispatch_event( data, DVPET_STOPPED );

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_DFVFF_GetStatus( IDirectFBVideoProvider *thiz,
                                        DFBVideoProviderStatus *ret_status )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_DFVFF )

     D_DEBUG_AT( VideoProvider_DFVFF, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_status)
          return DFB_INVARG;

     *ret_status = data->status;

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_DFVFF_SeekTo( IDirectFBVideoProvider *thiz,
                                     double                  seconds )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_DFVFF )

     D_DEBUG_AT( VideoProvider_DFVFF, "%s( %p )\n", __FUNCTION__, thiz );

     if (seconds < 0.0)
          return DFB_INVARG;

     direct_mutex_lock( &data->lock );

     data->frame = data->rate * seconds;
     data->frame = MIN( data->frame, data->nb_frames - 1 );

     data->seeked = true;

     direct_waitqueue_signal( &data->cond );

     direct_mutex_unlock( &data->lock );

     return DFB_UNSUPPORTED;
}

static DFBResult
IDirectFBVideoProvider_DFVFF_GetPos( IDirectFBVideoProvider *thiz,
                                     double                 *ret_seconds )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_DFVFF )

     D_DEBUG_AT( VideoProvider_DFVFF, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_seconds)
          return DFB_INVARG;

     direct_mutex_lock( &data->lock );

     *ret_seconds = data->frame / data->rate;

     direct_mutex_unlock( &data->lock );

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_DFVFF_GetLength( IDirectFBVideoProvider *thiz,
                                        double                 *ret_seconds )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_DFVFF )

     D_DEBUG_AT( VideoProvider_DFVFF, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_seconds)
          return DFB_INVARG;

     *ret_seconds = data->nb_frames / data->rate;

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_DFVFF_SetPlaybackFlags( IDirectFBVideoProvider        *thiz,
                                               DFBVideoProviderPlaybackFlags  flags )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_DFVFF )

     D_DEBUG_AT( VideoProvider_DFVFF, "%s( %p )\n", __FUNCTION__, thiz );

     if (flags & ~DVPLAY_LOOPING)
          return DFB_UNSUPPORTED;

     data->flags = flags;

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_DFVFF_SetSpeed( IDirectFBVideoProvider *thiz,
                                       double                  multiplier )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_DFVFF )

     D_DEBUG_AT( VideoProvider_DFVFF, "%s( %p )\n", __FUNCTION__, thiz );

     if (multiplier != 0.0 && multiplier != 1.0)
          return DFB_UNSUPPORTED;

     if (multiplier == data->speed)
          return DFB_OK;

     direct_mutex_lock( &data->lock );

     if (multiplier && data->status != DVSTATE_FINISHED)
        direct_waitqueue_signal( &data->cond );

     data->speed = multiplier;

     dispatch_event( data, DVPET_SPEEDCHANGE );

     direct_mutex_unlock( &data->lock );

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_DFVFF_GetSpeed( IDirectFBVideoProvider *thiz,
                                       double                 *ret_multiplier )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_DFVFF )

     D_DEBUG_AT( VideoProvider_DFVFF, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_multiplier)
          return DFB_INVARG;

     *ret_multiplier = data->speed;

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_DFVFF_CreateEventBuffer( IDirectFBVideoProvider  *thiz,
                                                IDirectFBEventBuffer   **ret_interface )
{
     DFBResult             ret;
     IDirectFBEventBuffer *buffer;

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_DFVFF )

     D_DEBUG_AT( VideoProvider_DFVFF, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_interface)
          return DFB_INVARG;

     ret = data->idirectfb->CreateEventBuffer( data->idirectfb, &buffer );
     if (ret)
          return ret;

     ret = thiz->AttachEventBuffer( thiz, buffer );

     buffer->Release( buffer );

     *ret_interface = (ret == DFB_OK) ? buffer : NULL;

     return ret;
}

static DFBResult
IDirectFBVideoProvider_DFVFF_AttachEventBuffer( IDirectFBVideoProvider *thiz,
                                                IDirectFBEventBuffer   *buffer )
{
     DFBResult  ret;
     EventLink *link;

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_DFVFF )

     D_DEBUG_AT( VideoProvider_DFVFF, "%s( %p )\n", __FUNCTION__, thiz );

     if (!buffer)
          return DFB_INVARG;

     ret = buffer->AddRef( buffer );
     if (ret)
          return ret;

     link = D_MALLOC( sizeof(EventLink) );
     if (!link) {
          buffer->Release( buffer );
          return D_OOM();
     }

     link->buffer = buffer;

     direct_mutex_lock( &data->events_lock );

     direct_list_append( &data->events, &link->link );

     direct_mutex_unlock( &data->events_lock );

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_DFVFF_EnableEvents( IDirectFBVideoProvider    *thiz,
                                           DFBVideoProviderEventType  mask )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_DFVFF )

     D_DEBUG_AT( VideoProvider_DFVFF, "%s( %p )\n", __FUNCTION__, thiz );

     if (mask & ~DVPET_ALL)
          return DFB_INVARG;

     data->events_mask |= mask;

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_DFVFF_DisableEvents( IDirectFBVideoProvider    *thiz,
                                            DFBVideoProviderEventType  mask )
{

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_DFVFF )

     D_DEBUG_AT( VideoProvider_DFVFF, "%s( %p )\n", __FUNCTION__, thiz );

     if (mask & ~DVPET_ALL)
          return DFB_INVARG;

     data->events_mask &= ~mask;

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_DFVFF_DetachEventBuffer( IDirectFBVideoProvider *thiz,
                                                IDirectFBEventBuffer   *buffer )
{
     DFBResult  ret = DFB_ITEMNOTFOUND;
     EventLink *link;

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_DFVFF )

     D_DEBUG_AT( VideoProvider_DFVFF, "%s( %p )\n", __FUNCTION__, thiz );

     if (!buffer)
          return DFB_INVARG;

     direct_mutex_lock( &data->events_lock );

     direct_list_foreach (link, data->events) {
          if (link->buffer == buffer) {
               direct_list_remove( &data->events, &link->link );
               link->buffer->Release( link->buffer );
               D_FREE( link );
               ret = DFB_OK;
               break;
          }
     }

     direct_mutex_unlock( &data->events_lock );

     return ret;
}

static DFBResult
IDirectFBVideoProvider_DFVFF_SetDestination( IDirectFBVideoProvider *thiz,
                                             IDirectFBSurface       *destination,
                                             const DFBRectangle     *dest_rect )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_DFVFF )

     D_DEBUG_AT( VideoProvider_DFVFF, "%s( %p, %4d,%4d-%4dx%4d )\n", __FUNCTION__,
                 thiz, dest_rect->x, dest_rect->y, dest_rect->w, dest_rect->h );

     if (!dest_rect)
          return DFB_INVARG;

     if (dest_rect->w < 1 || dest_rect->h < 1)
          return DFB_INVARG;

     data->rect = *dest_rect;

     return DFB_OK;
}

/**********************************************************************************************************************/

static DFBResult
Probe( IDirectFBVideoProvider_ProbeContext *ctx )
{
     if (!strncmp( (const char*) ctx->header, "DFVFF", 5 ))
          return DFB_OK;

     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBVideoProvider *thiz,
           IDirectFBDataBuffer    *buffer,
           CoreDFB                *core,
           IDirectFB              *idirectfb )
{
     DFBResult                 ret;
     DirectFile                fd;
     DirectFileInfo            info;
     void                     *ptr;
     const DFVFFHeader        *header;
     IDirectFBDataBuffer_data *buffer_data = buffer->priv;

     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBVideoProvider_DFVFF )

     D_DEBUG_AT( VideoProvider_DFVFF, "%s( %p )\n", __FUNCTION__, thiz );

     /* Check for valid filename. */
     if (!buffer_data->filename) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return DFB_UNSUPPORTED;
     }

     /* Open the file. */
     ret = direct_file_open( &fd, buffer_data->filename, O_RDONLY, 0 );
     if (ret) {
          D_DERROR( ret, "VideoProvider/DFVFF: Failed to open '%s'!\n", buffer_data->filename );
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     /* Query file size. */
     ret = direct_file_get_info( &fd, &info );
     if (ret) {
          D_DERROR( ret, "VideoProvider/DFVFF: Failed during get_info() of '%s'!\n", buffer_data->filename );
          goto error;
     }

     /* Memory-mapped file. */
     ret = direct_file_map( &fd, NULL, 0, info.size, DFP_READ, &ptr );
     if (ret) {
          D_DERROR( ret, "VideoProvider/DFVFF: Failed during mmap() of '%s'!\n", buffer_data->filename );
          goto error;
     }

     direct_file_close( &fd );

     header = ptr;

     data->ref              = 1;
     data->idirectfb        = idirectfb;
     data->ptr              = ptr;
     data->len              = info.size;
     data->desc.flags       = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT | DSDESC_COLORSPACE;
     data->desc.width       = header->width;
     data->desc.height      = header->height;
     data->desc.pixelformat = header->format;
     data->desc.colorspace  = header->colorspace;
     data->rate             = (double) header->framerate_num / header->framerate_den;
     data->status           = DVSTATE_STOP;
     data->speed            = 1.0;
     data->frame_size       = DFB_BYTES_PER_LINE( data->desc.pixelformat, data->desc.width ) *
                              DFB_PLANE_MULTIPLY( data->desc.pixelformat, data->desc.height );
     data->nb_frames        = (data->len - sizeof(DFVFFHeader)) / data->frame_size;
     data->events_mask      = DVPET_ALL;

     direct_mutex_init( &data->events_lock );

     direct_mutex_init( &data->lock );
     direct_waitqueue_init( &data->cond );

     thiz->AddRef                = IDirectFBVideoProvider_DFVFF_AddRef;
     thiz->Release               = IDirectFBVideoProvider_DFVFF_Release;
     thiz->GetCapabilities       = IDirectFBVideoProvider_DFVFF_GetCapabilities;
     thiz->GetSurfaceDescription = IDirectFBVideoProvider_DFVFF_GetSurfaceDescription;
     thiz->GetStreamDescription  = IDirectFBVideoProvider_DFVFF_GetStreamDescription;
     thiz->PlayTo                = IDirectFBVideoProvider_DFVFF_PlayTo;
     thiz->Stop                  = IDirectFBVideoProvider_DFVFF_Stop;
     thiz->GetStatus             = IDirectFBVideoProvider_DFVFF_GetStatus;
     thiz->SeekTo                = IDirectFBVideoProvider_DFVFF_SeekTo;
     thiz->GetPos                = IDirectFBVideoProvider_DFVFF_GetPos;
     thiz->GetLength             = IDirectFBVideoProvider_DFVFF_GetLength;
     thiz->SetPlaybackFlags      = IDirectFBVideoProvider_DFVFF_SetPlaybackFlags;
     thiz->SetSpeed              = IDirectFBVideoProvider_DFVFF_SetSpeed;
     thiz->GetSpeed              = IDirectFBVideoProvider_DFVFF_GetSpeed;
     thiz->CreateEventBuffer     = IDirectFBVideoProvider_DFVFF_CreateEventBuffer;
     thiz->AttachEventBuffer     = IDirectFBVideoProvider_DFVFF_AttachEventBuffer;
     thiz->EnableEvents          = IDirectFBVideoProvider_DFVFF_EnableEvents;
     thiz->DisableEvents         = IDirectFBVideoProvider_DFVFF_DisableEvents;
     thiz->DetachEventBuffer     = IDirectFBVideoProvider_DFVFF_DetachEventBuffer;
     thiz->SetDestination        = IDirectFBVideoProvider_DFVFF_SetDestination;

     return DFB_OK;

error:
     direct_file_close( &fd );

     DIRECT_DEALLOCATE_INTERFACE( thiz );

     return ret;
}

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

#include <core/CoreDFB.h>
#include <core/CoreSurface.h>
#include <core/core.h>
#include <core/layer_region.h>
#include <core/palette.h>
#include <core/surface.h>
#include <core/surface_allocation.h>
#include <core/surface_buffer.h>
#include <core/surface_client.h>
#include <core/surface_pool.h>
#include <core/windowstack.h>
#include <direct/filesystem.h>
#include <direct/memcpy.h>
#include <directfb_util.h>
#include <fusion/conf.h>
#include <fusion/hash.h>
#include <fusion/shmalloc.h>
#include <gfx/convert.h>
#include <gfx/util.h>

D_DEBUG_DOMAIN( Core_Surface,         "Core/Surface",         "DirectFB Core Surface" );
D_DEBUG_DOMAIN( Core_Surface_Updates, "Core/Surface/Updates", "DirectFB Core Surface Updates" );

/**********************************************************************************************************************/

static __inline__ void
dfb_surface_set_stereo_eye( CoreSurface         *surface,
                            DFBSurfaceStereoEye  eye )
{
     D_MAGIC_ASSERT( surface, CoreSurface );
     D_ASSERT( eye & (DSSE_LEFT | DSSE_RIGHT) );

     if (eye & DSSE_LEFT)
          surface->buffers = surface->left_buffers;
     else
          surface->buffers = surface->right_buffers;
}

static __inline__ DFBSurfaceStereoEye
dfb_surface_get_stereo_eye( CoreSurface *surface )
{
     D_MAGIC_ASSERT( surface, CoreSurface );

     if (surface->buffers == surface->left_buffers)
          return DSSE_LEFT;
     else
          return DSSE_RIGHT;
}

static void
keep_frame( CoreSurface *surface )
{
     CoreSurfaceBuffer *buffer = surface->left_buffers[surface->buffer_indices[surface->flips % surface->num_buffers]];

     D_DEBUG_AT( Core_Surface, "%s( %p )\n", __FUNCTION__, surface );

     D_DEBUG_AT( Core_Surface, "  -> buffer %p\n", buffer );

     if (!buffer->busy++) {
          CoreSurfaceBuffer *old = NULL;

          fusion_hash_replace( surface->frames, (void*)(long) (surface->flips * 2), buffer, NULL,
                               (void**) &old );

          D_ASSERT( old == NULL );
     }

     if (surface->config.caps & DSCAPS_STEREO) {
          buffer = surface->right_buffers[surface->buffer_indices[surface->flips % surface->num_buffers]];

          D_DEBUG_AT( Core_Surface, "  -> buffer %p\n", buffer );

          if (!buffer->busy++) {
               CoreSurfaceBuffer *old = NULL;

               fusion_hash_replace( surface->frames, (void*)(long) (surface->flips * 2 + 1), buffer, NULL,
                                    (void**) &old );

               D_ASSERT( old == NULL );
          }
     }
}

static void
release_frame( CoreSurface *surface,
               u32          flip_count )
{
     CoreSurfaceBuffer *buffer;

     D_DEBUG_AT( Core_Surface, "%s( %p, flip_count %u )\n", __FUNCTION__, surface, flip_count );

     buffer = fusion_hash_lookup( surface->frames, (void*)(long) (flip_count * 2) );
     if (buffer) {
          D_DEBUG_AT( Core_Surface, "  -> buffer %p\n", buffer );

          if (buffer->busy-- == 1 && (buffer->flags & CSBF_DECOUPLE))
               dfb_surface_buffer_decouple( buffer );

          fusion_hash_remove( surface->frames, (void*)(long) (flip_count * 2), NULL, NULL );
     }

     buffer = fusion_hash_lookup( surface->frames, (void*)(long) (flip_count * 2 + 1) );
     if (buffer) {
          D_DEBUG_AT( Core_Surface, "  -> buffer %p\n", buffer );

          if (buffer->busy-- == 1 && (buffer->flags & CSBF_DECOUPLE))
               dfb_surface_buffer_decouple( buffer );

          fusion_hash_remove( surface->frames, (void*)(long) (flip_count * 2 + 1), NULL, NULL );
     }
}

/**********************************************************************************************************************/

static const ReactionFunc dfb_surface_globals[] = {
     _dfb_layer_region_surface_listener,
     _dfb_windowstack_background_image_listener,
     NULL
};

static bool
surface_destructor_buffers_iterator( FusionHash *hash,
                                     void       *key,
                                     void       *value,
                                     void       *ctx )
{
     CoreSurface       *surface = ctx;
     CoreSurfaceBuffer *buffer  = value;
     int                i;

     buffer->busy = 0;

     dfb_surface_buffer_decouple( buffer );

     for (i = 0; i < surface->num_buffers; i++) {
          if (surface->buffers[i] == buffer)
               surface->buffers[i] = NULL;
     }

     fusion_hash_remove( surface->frames, key, NULL, NULL );

     return true;
}

static void
surface_destructor( FusionObject *object,
                    bool          zombie,
                    void         *ctx )
{
     int                  i;
     int                  num_eyes;
     DFBSurfaceStereoEye  eye;
     CoreSurface         *surface = (CoreSurface*) object;

     D_MAGIC_ASSERT( surface, CoreSurface );

     D_DEBUG_AT( Core_Surface, "Destroying surface %p (%dx%d%s)\n",
                 surface, surface->config.size.w, surface->config.size.h, zombie ? " ZOMBIE" : "" );

     Core_Resource_RemoveSurface( surface );

     CoreSurface_Deinit_Dispatch( &surface->call );

     dfb_surface_lock( surface );

     surface->state |= CSSF_DESTROYED;

     /* Announce surface destruction. */
     dfb_surface_notify( surface, CSNF_DESTROY );

     dfb_surface_dispatch_event( surface, DSEVT_DESTROYED );

     /* Unlink palette. */
     if (surface->palette) {
          dfb_palette_detach_global( surface->palette, &surface->palette_reaction );
          dfb_palette_unlink( &surface->palette );
     }

     while (fusion_hash_size( surface->frames ) > 0)
          fusion_hash_iterate( surface->frames, surface_destructor_buffers_iterator, surface );

     /* Destroy the surface buffers. */
     num_eyes = surface->config.caps & DSCAPS_STEREO ? 2 : 1;
     for (eye = DSSE_LEFT; num_eyes > 0; num_eyes--, eye = DSSE_RIGHT) {
          dfb_surface_set_stereo_eye( surface, eye );
          for (i = 0; i < surface->num_buffers; i++) {
               if (surface->buffers[i]) {
                    dfb_surface_buffer_decouple( surface->buffers[i] );
                    surface->buffers[i] = NULL;
               }
          }
     }

     dfb_surface_set_stereo_eye( surface, DSSE_LEFT );

     /* Release the system driver specific surface data. */
     if (surface->data) {
          SHFREE( surface->shmpool, surface->data );
          surface->data = NULL;
     }

     direct_serial_deinit( &surface->serial );
     direct_serial_deinit( &surface->config_serial );

     dfb_surface_unlock( surface );

     fusion_vector_destroy( &surface->clients );

     fusion_skirmish_destroy( &surface->lock );

     fusion_hash_destroy( surface->frames );

     D_MAGIC_CLEAR( surface );

     /* Destroy the object. */
     fusion_object_destroy( object );
}

FusionObjectPool *
dfb_surface_pool_create( const FusionWorld *world )
{
     FusionObjectPool *pool;

     pool = fusion_object_pool_create( "Surface Pool",
                                       sizeof(CoreSurface), sizeof(CoreSurfaceNotification),
                                       surface_destructor, NULL, world );

     return pool;
}

/**********************************************************************************************************************/

DFBResult
dfb_surface_create( CoreDFB                  *core,
                    const CoreSurfaceConfig  *config,
                    CoreSurfaceTypeFlags      type,
                    unsigned long             resource_id,
                    CorePalette              *palette,
                    CoreSurface             **ret_surface )
{
     DFBResult            ret = DFB_BUG;
     int                  i;
     int                  buffers;
     CoreSurface         *surface;
     char                 buf[64];
     int                  num_eyes;
     DFBSurfaceStereoEye  eye;

     D_ASSERT( core != NULL );
     D_FLAGS_ASSERT( type, CSTF_ALL );
     D_MAGIC_ASSERT_IF( palette, CorePalette );
     D_ASSERT( ret_surface != NULL );

     D_DEBUG_AT( Core_Surface, "%s( %p, %p, %p )\n", __FUNCTION__, core, config, ret_surface );

     /* Create the surface object. */
     surface = dfb_core_create_surface( core );
     if (!surface)
          return DFB_FUSION;

     surface->data = NULL;

     if (config) {
          D_FLAGS_ASSERT( config->flags, CSCONF_ALL );

          surface->config.flags = config->flags;

          if (config->flags & CSCONF_SIZE) {
               D_DEBUG_AT( Core_Surface, "  -> %dx%d\n", config->size.w, config->size.h );

               surface->config.size = config->size;
          }

          if (config->flags & CSCONF_FORMAT) {
               D_DEBUG_AT( Core_Surface, "  -> %s\n", dfb_pixelformat_name( config->format ) );

               surface->config.format = config->format;
          }

          if (config->flags & CSCONF_COLORSPACE) {
               D_DEBUG_AT( Core_Surface, "  -> %s\n", dfb_colorspace_name( config->colorspace ) );

               surface->config.colorspace = config->colorspace;
          }

          if (config->flags & CSCONF_CAPS) {
               D_DEBUG_AT( Core_Surface, "  -> caps 0x%08x\n", config->caps );

               surface->config.caps = config->caps & ~DSCAPS_ROTATED;
          }

          if (config->flags & CSCONF_PREALLOCATED) {
               D_DEBUG_AT( Core_Surface, "  -> prealloc %p [%u]\n",
                           config->preallocated[0].addr, config->preallocated[0].pitch );

               direct_memcpy( surface->config.preallocated, config->preallocated, sizeof(config->preallocated) );

               surface->config.preallocated_pool_id = config->preallocated_pool_id;

               type |= CSTF_PREALLOCATED;
          }
     }

     if (surface->config.caps & DSCAPS_SYSTEMONLY)
          surface->type = (type & ~CSTF_EXTERNAL) | CSTF_INTERNAL;
     else if (surface->config.caps & DSCAPS_VIDEOONLY)
          surface->type = (type & ~CSTF_INTERNAL) | CSTF_EXTERNAL;
     else
          surface->type = type & ~(CSTF_INTERNAL | CSTF_EXTERNAL);

     if (surface->config.caps & DSCAPS_SHARED)
          surface->type |= CSTF_SHARED;

     surface->resource_id = resource_id;

     if (surface->config.caps & DSCAPS_TRIPLE)
          buffers = 3;
     else if (surface->config.caps & DSCAPS_DOUBLE)
          buffers = 2;
     else {
          buffers = 1;

          surface->config.caps &= ~DSCAPS_ROTATED;
     }

     surface->notifications = CSNF_ALL & ~CSNF_FLIP;

     surface->alpha_ramp[0] = 0x00;
     surface->alpha_ramp[1] = 0x55;
     surface->alpha_ramp[2] = 0xaa;
     surface->alpha_ramp[3] = 0xff;

     if (surface->config.caps & DSCAPS_STATIC_ALLOC)
          surface->config.min_size = surface->config.size;

     surface->shmpool = dfb_core_shmpool( core );

     direct_serial_init( &surface->serial );
     direct_serial_init( &surface->config_serial );
     direct_serial_increase( &surface->config_serial );

     fusion_vector_init( &surface->clients, 2, surface->shmpool );

     snprintf( buf, sizeof(buf), "Surface %dx%d %s %s", surface->config.size.w, surface->config.size.h,
               dfb_pixelformat_name( surface->config.format ), dfb_colorspace_name( surface->config.colorspace ) );

     fusion_ref_set_name( &surface->object.ref, buf );

     fusion_skirmish_init2( &surface->lock, buf, dfb_core_world( core ), fusion_config->secure_fusion );

     fusion_reactor_direct( surface->object.reactor, false );

     fusion_hash_create( surface->shmpool, HASH_INT, HASH_PTR, 7, &surface->frames );

     D_MAGIC_SET( surface, CoreSurface );

     if (dfb_config->warn.flags & DCWF_CREATE_SURFACE                         &&
         dfb_config->warn.create_surface.min_size.w <= surface->config.size.w &&
         dfb_config->warn.create_surface.min_size.h <= surface->config.size.h)
          D_WARN( "create-surface %4dx%4d %6s, buffers %d, caps 0x%08x, type 0x%08x",
                  surface->config.size.w, surface->config.size.h, dfb_pixelformat_name( surface->config.format ),
                  buffers, surface->config.caps, surface->type );

     if (palette) {
          dfb_surface_set_palette( surface, palette );
     }
     else if (DFB_PIXELFORMAT_IS_INDEXED( surface->config.format )) {
          ret = dfb_surface_init_palette( core, surface );
          if (ret)
               goto error;
     }

     dfb_surface_lock( surface );

     /* Create the surface buffers. */
     num_eyes = config->caps & DSCAPS_STEREO ? 2 : 1;
     for (eye = DSSE_LEFT; num_eyes > 0; num_eyes--, eye = DSSE_RIGHT) {
          dfb_surface_set_stereo_eye( surface, eye );
          for (i = 0; i < buffers; i++) {
               ret = dfb_surface_buffer_create( core, surface, (eye == DSSE_RIGHT) ? CSBF_RIGHT : CSBF_NONE, i,
                                                &surface->buffers[i] );
               if (ret) {
                    D_DERROR( ret, "Core/Surface: Error creating surface buffer!\n" );
                    dfb_surface_unlock( surface );
                    goto error;
               }

               dfb_surface_buffer_globalize( surface->buffers[i] );

               if (eye == DSSE_LEFT)
                    surface->num_buffers++;

               switch (i) {
                    case 0:
                         surface->buffer_indices[DSBR_FRONT] = i;
                    case 1:
                         surface->buffer_indices[DSBR_BACK] = i;
                    case 2:
                         surface->buffer_indices[DSBR_IDLE] = i;
               }
          }
     }
     dfb_surface_set_stereo_eye( surface, DSSE_LEFT );

     dfb_surface_unlock( surface );

     CoreSurface_Init_Dispatch( core, surface, &surface->call );

     /* Activate the object. */
     fusion_object_activate( &surface->object );

     if (dfb_config->surface_clear)
          dfb_surface_clear_buffers( surface );

     /* Return the new surface. */
     *ret_surface = surface;

     D_DEBUG_AT( Core_Surface, "  -> %p\n", surface );

     return DFB_OK;

error:
     num_eyes = config->caps & DSCAPS_STEREO ? 2 : 1;
     for (eye = DSSE_LEFT; num_eyes > 0; num_eyes--, eye = DSSE_RIGHT) {
          dfb_surface_set_stereo_eye( surface, eye );
          for (i = 0; i < MAX_SURFACE_BUFFERS; i++) {
               if (surface->buffers[i]) {
                    dfb_surface_buffer_decouple( surface->buffers[i] );
                    surface->buffers[i] = NULL;
               }
          }
     }
     dfb_surface_set_stereo_eye( surface, DSSE_LEFT );

     /* Release the system driver specific surface data. */
     if (surface->data) {
         SHFREE( surface->shmpool, surface->data );
         surface->data = NULL;
     }

     fusion_skirmish_destroy( &surface->lock );

     direct_serial_deinit( &surface->serial );
     direct_serial_deinit( &surface->config_serial );

     fusion_hash_destroy( surface->frames );

     D_MAGIC_CLEAR( surface );

     fusion_object_destroy( &surface->object );

     return ret;
}

DFBResult
dfb_surface_create_simple ( CoreDFB                 *core,
                            int                      width,
                            int                      height,
                            DFBSurfacePixelFormat    format,
                            DFBSurfaceColorSpace     colorspace,
                            DFBSurfaceCapabilities   caps,
                            CoreSurfaceTypeFlags     type,
                            unsigned long            resource_id,
                            CorePalette             *palette,
                            CoreSurface            **ret_surface )
{
     CoreSurfaceConfig surface_config;

     D_ASSERT( core != NULL );
     D_ASSERT( ret_surface != NULL );

     D_DEBUG_AT( Core_Surface, "%s( %p, %dx%d %s, %p )\n", __FUNCTION__,
                 core, width, height, dfb_pixelformat_name( format ), ret_surface );

     surface_config.flags      = CSCONF_SIZE | CSCONF_FORMAT | CSCONF_COLORSPACE | CSCONF_CAPS;
     surface_config.size.w     = width;
     surface_config.size.h     = height;
     surface_config.format     = format;
     surface_config.colorspace = colorspace;
     surface_config.caps       = caps;

     return CoreDFB_CreateSurface( core, &surface_config, type, resource_id, palette, ret_surface );
}

DFBResult
dfb_surface_init_palette( CoreDFB     *core,
                          CoreSurface *surface )
{
     DFBResult    ret;
     CorePalette *palette;

     D_DEBUG_AT( Core_Surface, "%s( %p, %p )\n", __FUNCTION__, core, surface );

     ret = dfb_palette_create( core, 1 << DFB_COLOR_BITS_PER_PIXEL( surface->config.format ),
                               surface->config.colorspace, &palette );
     if (ret) {
          D_DERROR( ret, "Core/Surface: Error creating palette!\n" );
          return ret;
     }

     switch (surface->config.format) {
          case DSPF_LUT8:
               dfb_palette_generate_rgb332_map( palette );
               break;

          case DSPF_ALUT44:
               dfb_palette_generate_rgb121_map( palette );
               break;

          default:
               break;
     }

     dfb_surface_set_palette( surface, palette );

     dfb_palette_unref( palette );

     return DFB_OK;
}

DFBResult
dfb_surface_notify( CoreSurface                  *surface,
                    CoreSurfaceNotificationFlags  flags )
{
     DFBResult               ret;
     CoreSurfaceNotification notification;

     D_MAGIC_ASSERT( surface, CoreSurface );
     FUSION_SKIRMISH_ASSERT( &surface->lock );
     D_FLAGS_ASSERT( flags, CSNF_ALL );

     D_DEBUG_AT( Core_Surface, "%s( %p [%u] )\n", __FUNCTION__, surface, surface->object.id );

     direct_serial_increase( &surface->serial );

     if (!(surface->state & CSSF_DESTROYED)) {
          if (!(surface->notifications & flags))
               return DFB_OK;
     }

     notification.flags   = flags;
     notification.surface = surface;

     ret = dfb_surface_dispatch( surface, &notification, dfb_surface_globals );

     return ret;
}

DFBResult
dfb_surface_notify_display( CoreSurface       *surface,
                            CoreSurfaceBuffer *buffer )
{
     DFBResult ret;

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     ret = dfb_surface_notify_display2( surface, dfb_surface_buffer_index( buffer ) );

     return ret;
}

DFBResult
dfb_surface_notify_display2( CoreSurface *surface,
                             int          index )
{
     DFBResult               ret;
     CoreSurfaceNotification notification;

     D_MAGIC_ASSERT( surface, CoreSurface );
     D_ASSERT( index >= 0 );

     D_DEBUG_AT( Core_Surface, "%s( %p, %d )\n", __FUNCTION__, surface, index );

     notification.flags   = CSNF_DISPLAY;
     notification.surface = surface;
     notification.index   = index;

     ret = dfb_surface_dispatch( surface, &notification, dfb_surface_globals );

     return ret;
}

DFBResult
dfb_surface_notify_frame( CoreSurface  *surface,
                          unsigned int  flip_count )
{
     DFBResult               ret;
     CoreSurfaceNotification notification;

     D_MAGIC_ASSERT( surface, CoreSurface );
     FUSION_SKIRMISH_ASSERT( &surface->lock );

     D_DEBUG_AT( Core_Surface_Updates, "%s( %p, count %u )\n", __FUNCTION__, surface, flip_count );

     direct_serial_increase( &surface->serial );

     notification.flags      = CSNF_FRAME;
     notification.surface    = surface;
     notification.flip_count = flip_count;

     ret = dfb_surface_dispatch_channel( surface, CSCH_FRAME, &notification, sizeof(notification),
                                         dfb_surface_globals );

     return ret;
}

DFBResult
dfb_surface_pool_notify( CoreSurface                  *surface,
                         CoreSurfaceBuffer            *buffer,
                         CoreSurfaceAllocation        *allocation,
                         CoreSurfaceNotificationFlags  flags )
{
     DFBResult               ret;
     CoreSurfaceNotification notification;

     D_MAGIC_ASSERT( surface, CoreSurface );
     FUSION_SKIRMISH_ASSERT( &surface->lock );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_ASSERT( buffer->surface == surface );
     CORE_SURFACE_ALLOCATION_ASSERT( allocation );
     D_ASSERT( allocation->buffer == buffer );
     D_FLAGS_ASSERT( flags, CSNF_ALL );
     D_ASSERT( flags == CSNF_BUFFER_ALLOCATION_DESTROY );

     D_DEBUG_AT( Core_Surface, "%s( %p [%u] )\n", __FUNCTION__, surface, surface->object.id );

     if (!(surface->state & CSSF_DESTROYED)) {
          if (!(surface->notifications & flags))
               return DFB_OK;
     }

     /* Prepares and sends a notification message that a change is about to happen to the specified surface buffer
        pool allocation. The notification message will be received by all pocesses that have listeners attached to
        the associated CoreSurface's reactor. A copy of all the data needed by the listeners is done in order to wait
        for all the listeners to complete before the buffer allocation is destroyed along with all of its underlying
        data structures. */

     notification.flags             = flags;
     notification.surface           = surface;
     notification.buffer_no_access  = buffer;
     notification.surface_data      = surface->data;
     notification.surface_object_id = surface->object.id;

     D_DEBUG_AT( Core_Surface, "  -> notifying of surface buffer allocation destruction\n" );

     ret = dfb_surface_dispatch( surface, &notification, dfb_surface_globals );

     return ret;
}

DFBResult
dfb_surface_flip( CoreSurface *surface, bool swap )
{
     DFBResult ret;

     D_MAGIC_ASSERT( surface, CoreSurface );

     D_DEBUG_AT( Core_Surface, "%s( %p, %sswap )\n", __FUNCTION__, surface, swap ? "" : "no " );

     ret = dfb_surface_flip_buffers( surface, swap );

     return ret;
}

DFBResult
dfb_surface_flip_buffers( CoreSurface *surface, bool swap )
{
     unsigned int back, front;

     D_MAGIC_ASSERT( surface, CoreSurface );
     FUSION_SKIRMISH_ASSERT( &surface->lock );

     D_DEBUG_AT( Core_Surface, "%s( %p, %sswap )\n", __FUNCTION__, surface, swap ? "" : "no " );

     if (surface->num_buffers == 0)
          return DFB_SUSPENDED;

     back  = (surface->flips + DSBR_BACK)  % surface->num_buffers;
     front = (surface->flips + DSBR_FRONT) % surface->num_buffers;

     D_ASSERT( surface->buffer_indices[back]  < surface->num_buffers );
     D_ASSERT( surface->buffer_indices[front] < surface->num_buffers );

     if (surface->buffers[surface->buffer_indices[back]]->policy !=
         surface->buffers[surface->buffer_indices[front]]->policy ||
         (surface->config.caps & DSCAPS_ROTATED))
          return DFB_UNSUPPORTED;

     if (swap) {
          int tmp = surface->buffer_indices[back];
          surface->buffer_indices[back]  = surface->buffer_indices[front];
          surface->buffer_indices[front] = tmp;
     }
     else
          surface->flips++;

     D_DEBUG_AT( Core_Surface, "  -> flips %u\n", surface->flips );

     dfb_surface_notify( surface, CSNF_FLIP );

     return DFB_OK;
}

DFBResult
dfb_surface_dispatch_event( CoreSurface         *surface,
                            DFBSurfaceEventType  type )
{
     DFBResult       ret;
     DFBSurfaceEvent event;

     D_MAGIC_ASSERT( surface, CoreSurface );

     event.clazz      = DFEC_SURFACE;
     event.type       = type;
     event.surface_id = surface->object.id;
     event.time_stamp = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );

     ret = dfb_surface_dispatch_channel( surface, CSCH_EVENT, &event, sizeof(DFBSurfaceEvent), NULL );

     return ret;
}

DFBResult
dfb_surface_dispatch_update( CoreSurface         *surface,
                             const DFBRegion     *update,
                             const DFBRegion     *update_right,
                             long long            timestamp,
                             DFBSurfaceFlipFlags  flags )
{
     DFBResult       ret;
     DFBSurfaceEvent event;

     D_MAGIC_ASSERT( surface, CoreSurface );
     FUSION_SKIRMISH_ASSERT( &surface->lock );

     D_DEBUG_AT( Core_Surface_Updates, "%s( %p [%u], %p / %p, timestamp %lld )\n", __FUNCTION__,
                 surface, surface->object.id, update, update_right, timestamp );

     event.clazz      = DFEC_SURFACE;
     event.type       = DSEVT_UPDATE;
     event.surface_id = surface->object.id;
     event.flip_count = surface->flips;
     event.flip_flags = flags;
     event.time_stamp = timestamp ?: direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );

     surface->last_frame_time = event.time_stamp;

     D_DEBUG_AT( Core_Surface_Updates, "  -> flip count %u\n", event.flip_count );

     if (update) {
          D_DEBUG_AT( Core_Surface_Updates, "  -> updated %4d,%4d-%4dx%4d (left)\n",
                      DFB_RECTANGLE_VALS_FROM_REGION( update ) );

          event.update = *update;
     }
     else {
          event.update.x1 = 0;
          event.update.y1 = 0;
          event.update.x2 = surface->config.size.w - 1;
          event.update.y2 = surface->config.size.h - 1;
     }

     if (update_right) {
          D_DEBUG_AT( Core_Surface_Updates, "  -> updated %4d,%4d-%4dx%4d (right)\n",
                      DFB_RECTANGLE_VALS_FROM_REGION( update_right ) );

          event.update_right = *update_right;
     }
     else {
          event.update_right.x1 = 0;
          event.update_right.y1 = 0;
          event.update_right.x2 = surface->config.size.w - 1;
          event.update_right.y2 = surface->config.size.h - 1;
     }

     ret = dfb_surface_dispatch_channel( surface, CSCH_EVENT, &event, sizeof(DFBSurfaceEvent), NULL );
     if (ret)
          return ret;

     D_DEBUG_AT( Core_Surface_Updates, "  -> clients %d\n", fusion_vector_size( &surface->clients ) );

     if (fusion_vector_is_empty( &surface->clients )) {
          surface->flips_acked = surface->flips;

          dfb_surface_notify_frame( surface, surface->flips_acked );
     }
     else
          keep_frame( surface );

     return DFB_OK;
}

DFBResult
dfb_surface_check_acks( CoreSurface *surface )
{
     int                i;
     CoreSurfaceClient *client;
     u32                count;

     D_MAGIC_ASSERT( surface, CoreSurface );
     FUSION_SKIRMISH_ASSERT( &surface->lock );

     D_DEBUG_AT( Core_Surface_Updates, "%s( %p [%u] )\n", __FUNCTION__, surface, surface->object.id );

     count = surface->flips;

     fusion_vector_foreach (client, i, surface->clients) {
          D_DEBUG_AT( Core_Surface_Updates, "  -> client %p [%u] (acked %u)\n",
                      client, client->object.id, client->flip_count );

          if (client->flip_count < count)
               count = client->flip_count;
     }

     D_DEBUG_AT( Core_Surface_Updates, "  -> lowest count %u (acked %u)\n", count, surface->flips_acked );

     if (count > surface->flips_acked) {
          for (; surface->flips_acked < count; surface->flips_acked++)
               release_frame( surface, surface->flips_acked );

          dfb_surface_notify_frame( surface, surface->flips_acked );
     }

     return DFB_OK;
}

DFBResult
dfb_surface_reconfig( CoreSurface             *surface,
                      const CoreSurfaceConfig *config )
{
     DFBResult           ret;
     int                 i;
     int                 buffers;
     int                 num_eyes;
     DFBSurfaceStereoEye eye;
     CoreSurfaceConfig   new_config;

     D_MAGIC_ASSERT( surface, CoreSurface );
     D_ASSERT( config != NULL );

     D_DEBUG_AT( Core_Surface, "%s( %p, %dx%d %s -> %dx%d %s )\n", __FUNCTION__, surface,
                 surface->config.size.w, surface->config.size.h, dfb_pixelformat_name( surface->config.format ),
                 (config->flags & CSCONF_SIZE)   ? config->size.w : surface->config.size.w,
                 (config->flags & CSCONF_SIZE)   ? config->size.h : surface->config.size.h,
                 (config->flags & CSCONF_FORMAT) ? dfb_pixelformat_name( config->format ) :
                                                   dfb_pixelformat_name( surface->config.format ) );

     if (config->flags & CSCONF_PREALLOCATED)
          return DFB_UNSUPPORTED;

     if (fusion_skirmish_prevail( &surface->lock ))
          return DFB_FUSION;

     if (surface->type & CSTF_PREALLOCATED) {
          fusion_skirmish_dismiss( &surface->lock );
          return DFB_UNSUPPORTED;
     }

     if ((config->flags == CSCONF_SIZE ||
         ((config->flags == (CSCONF_SIZE | CSCONF_FORMAT)) && (config->format == surface->config.format))) &&
         config->size.w <= surface->config.min_size.w                                                      &&
         config->size.h <= surface->config.min_size.h) {
          surface->config.size = config->size;

          direct_serial_increase( &surface->config_serial );

          fusion_skirmish_dismiss( &surface->lock );

          return DFB_OK;
     }

     new_config = surface->config;

     if (config->flags & CSCONF_SIZE)
          new_config.size = config->size;

     if (config->flags & CSCONF_FORMAT)
          new_config.format = config->format;

     if (config->flags & CSCONF_COLORSPACE)
          new_config.colorspace = config->colorspace;

     if (config->flags & CSCONF_CAPS)
          new_config.caps = config->caps & ~DSCAPS_ROTATED;

     if (new_config.caps & DSCAPS_SYSTEMONLY)
          surface->type = (surface->type & ~CSTF_EXTERNAL) | CSTF_INTERNAL;
     else if (new_config.caps & DSCAPS_VIDEOONLY)
          surface->type = (surface->type & ~CSTF_INTERNAL) | CSTF_EXTERNAL;
     else
          surface->type = surface->type & ~(CSTF_INTERNAL | CSTF_EXTERNAL);

     if (new_config.caps & DSCAPS_TRIPLE)
          buffers = 3;
     else if (new_config.caps & DSCAPS_DOUBLE)
          buffers = 2;
     else {
          buffers = 1;

          new_config.caps &= ~DSCAPS_ROTATED;
     }

     ret = Core_Resource_CheckSurfaceUpdate( surface, &new_config );
     if (ret)
          return ret;

     direct_serial_increase( &surface->config_serial );

     /* Destroy the surface buffers. */
     num_eyes = surface->config.caps & DSCAPS_STEREO ? 2 : 1;
     for (eye = DSSE_LEFT; num_eyes > 0; num_eyes--, eye = DSSE_RIGHT) {
          dfb_surface_set_stereo_eye( surface, eye );
          for (i = 0; i < surface->num_buffers; i++) {
               if (surface->buffers[i]) {
                    dfb_surface_buffer_decouple( surface->buffers[i] );
                    surface->buffers[i] = NULL;
               }
          }
     }
     dfb_surface_set_stereo_eye( surface, DSSE_LEFT );

     surface->num_buffers = 0;
     surface->flips++;

     Core_Resource_UpdateSurface( surface, &new_config );

     surface->config = new_config;

     /* Recreate the surface buffers. */
     num_eyes = new_config.caps & DSCAPS_STEREO ? 2 : 1;
     for (eye = DSSE_LEFT; num_eyes > 0; num_eyes--, eye = DSSE_RIGHT) {
          dfb_surface_set_stereo_eye( surface, eye );
          for (i = 0; i < buffers; i++) {
               CoreSurfaceBuffer *buffer;

               ret = dfb_surface_buffer_create( core_dfb, surface, (eye == DSSE_RIGHT) ? CSBF_RIGHT : CSBF_NONE, i,
                                                &buffer );
               if (ret) {
                    D_DERROR( ret, "Core/Surface: Error creating surface buffer!\n" );
                    goto error;
               }

               dfb_surface_buffer_globalize( buffer );

               surface->buffers[i] = buffer;
               if (eye == DSSE_LEFT)
                    surface->num_buffers++;

               switch (i) {
                    case 0:
                         surface->buffer_indices[DSBR_FRONT] = i;
                    case 1:
                         surface->buffer_indices[DSBR_BACK] = i;
                    case 2:
                         surface->buffer_indices[DSBR_IDLE] = i;
               }
          }
     }
     dfb_surface_set_stereo_eye( surface, DSSE_LEFT );

     while (fusion_hash_size( surface->frames ) > 0)
          fusion_hash_iterate( surface->frames, surface_destructor_buffers_iterator, surface );

     dfb_surface_notify( surface, CSNF_SIZEFORMAT );

     if (dfb_config->surface_clear)
          dfb_surface_clear_buffers( surface );

     fusion_skirmish_dismiss( &surface->lock );

     return DFB_OK;

error:
     D_UNIMPLEMENTED();

     fusion_skirmish_dismiss( &surface->lock );

     return ret;
}

DFBResult
dfb_surface_reformat( CoreSurface           *surface,
                      int                    width,
                      int                    height,
                      DFBSurfacePixelFormat  format )
{
     CoreSurfaceConfig config;

     D_MAGIC_ASSERT( surface, CoreSurface );
     D_ASSERT( width > 0 );
     D_ASSERT( height > 0 );

     D_DEBUG_AT( Core_Surface, "%s( %p )\n", __FUNCTION__, surface );

     config.flags  = CSCONF_SIZE | CSCONF_FORMAT;
     config.size.w = width;
     config.size.h = height;
     config.format = format;

     return dfb_surface_reconfig( surface, &config );
}

DFBResult
dfb_surface_destroy_buffers( CoreSurface *surface )
{
     int                 i;
     int                 num_eyes;
     DFBSurfaceStereoEye eye;

     D_MAGIC_ASSERT( surface, CoreSurface );

     D_DEBUG_AT( Core_Surface, "%s( %p )\n", __FUNCTION__, surface );

     if (fusion_skirmish_prevail( &surface->lock ))
          return DFB_FUSION;

     if (surface->type & CSTF_PREALLOCATED) {
          fusion_skirmish_dismiss( &surface->lock );
          return DFB_UNSUPPORTED;
     }

     /* Destroy the surface buffers. */
     num_eyes = surface->config.caps & DSCAPS_STEREO ? 2 : 1;
     for (eye = DSSE_LEFT; num_eyes > 0; num_eyes--, eye = DSSE_RIGHT) {
          dfb_surface_set_stereo_eye( surface, eye );
          for (i = 0; i < surface->num_buffers; i++) {
               if (surface->buffers[i]) {
                    dfb_surface_buffer_decouple( surface->buffers[i] );
                    surface->buffers[i] = NULL;
               }
          }
     }
     dfb_surface_set_stereo_eye( surface, DSSE_LEFT );

     surface->num_buffers = 0;

     fusion_skirmish_dismiss( &surface->lock );

     return DFB_OK;
}

DFBResult
dfb_surface_deallocate_buffers( CoreSurface *surface )
{
     int                 i;
     int                 num_eyes;
     DFBSurfaceStereoEye eye;

     D_MAGIC_ASSERT( surface, CoreSurface );

     D_DEBUG_AT( Core_Surface, "%s( %p )\n", __FUNCTION__, surface );

     if (fusion_skirmish_prevail( &surface->lock ))
          return DFB_FUSION;

     if (surface->type & CSTF_PREALLOCATED) {
          fusion_skirmish_dismiss( &surface->lock );
          return DFB_UNSUPPORTED;
     }

     /* Deallocate the surface buffers. */
     num_eyes = surface->config.caps & DSCAPS_STEREO ? 2 : 1;
     for (eye = DSSE_LEFT; num_eyes > 0; num_eyes--, eye = DSSE_RIGHT) {
          dfb_surface_set_stereo_eye( surface, eye );
          for (i = 0; i < surface->num_buffers; i++)
               dfb_surface_buffer_deallocate( surface->buffers[i] );
     }
     dfb_surface_set_stereo_eye( surface, DSSE_LEFT );

     fusion_skirmish_dismiss( &surface->lock );

     return DFB_OK;
}

DFBResult
dfb_surface_destroy( CoreSurface *surface )
{
     D_MAGIC_ASSERT( surface, CoreSurface );

     D_DEBUG_AT( Core_Surface, "%s( %p )\n", __FUNCTION__, surface );

     if (fusion_skirmish_prevail( &surface->lock ))
          return DFB_FUSION;


     dfb_surface_deallocate_buffers( surface );

     surface->state |= CSSF_DESTROYED;

     fusion_skirmish_dismiss( &surface->lock );

     return DFB_OK;
}

DFBResult
dfb_surface_lock_buffer( CoreSurface            *surface,
                         DFBSurfaceBufferRole    role,
                         CoreSurfaceAccessorID   accessor,
                         CoreSurfaceAccessFlags  access,
                         CoreSurfaceBufferLock  *ret_lock )
{
     DFBResult              ret;
     CoreSurfaceAllocation *allocation;

     D_MAGIC_ASSERT( surface, CoreSurface );

     D_DEBUG_AT( Core_Surface, "%s( %p, accessor 0x%02x, access 0x%02x, role %u) <- %dx%d %s\n",
                 __FUNCTION__, surface, accessor, access, role,
                 surface->config.size.w, surface->config.size.h, dfb_pixelformat_name( surface->config.format ) );

     ret = CoreSurface_PreLockBuffer2( surface, role, dfb_surface_get_stereo_eye( surface ), accessor, access,
                                       true, &allocation );
     if (ret)
          return ret;

     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );

     D_DEBUG_AT( Core_Surface, "  -> PreLockBuffer returned allocation %p (%s)\n",
                 allocation, allocation->pool->desc.name );

     /* Lock the allocation. */
     dfb_surface_buffer_lock_init( ret_lock, accessor, access );

     ret = dfb_surface_pool_lock( allocation->pool, allocation, ret_lock );
     if (ret) {
          D_DERROR( ret, "Core/Surface: Locking allocation in '%s' failed!\n", allocation->pool->desc.name );
          dfb_surface_buffer_lock_deinit( ret_lock );
          dfb_surface_allocation_unref( allocation );
          return ret;
     }

     return DFB_OK;
}

DFBResult
dfb_surface_lock_buffer2( CoreSurface            *surface,
                          DFBSurfaceBufferRole    role,
                          u32                     flip_count,
                          DFBSurfaceStereoEye     eye,
                          CoreSurfaceAccessorID   accessor,
                          CoreSurfaceAccessFlags  access,
                          CoreSurfaceBufferLock  *ret_lock )
{
     DFBResult              ret;
     CoreSurfaceAllocation *allocation;

     D_MAGIC_ASSERT( surface, CoreSurface );

     D_DEBUG_AT( Core_Surface, "%s( %p, accessor 0x%02x, access 0x%02x, role %u, count %u, eye %u ) <- %dx%d %s\n",
                 __FUNCTION__, surface, accessor, access, role, flip_count, eye,
                 surface->config.size.w, surface->config.size.h, dfb_pixelformat_name( surface->config.format ) );

     ret = CoreSurface_PreLockBuffer3( surface, role, flip_count, eye, accessor, access, true, &allocation );
     if (ret)
          return ret;

     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );

     D_DEBUG_AT( Core_Surface, "  -> PreLockBuffer returned allocation %p (%s)\n",
                 allocation, allocation->pool->desc.name );

     /* Lock the allocation. */
     dfb_surface_buffer_lock_init( ret_lock, accessor, access );

     ret = dfb_surface_pool_lock( allocation->pool, allocation, ret_lock );
     if (ret) {
          D_DERROR( ret, "Core/Surface: Locking allocation in '%s' failed!\n", allocation->pool->desc.name );
          dfb_surface_buffer_lock_deinit( ret_lock );
          dfb_surface_allocation_unref( allocation );
          return ret;
     }

     return DFB_OK;
}

DFBResult
dfb_surface_unlock_buffer( CoreSurface           *surface,
                           CoreSurfaceBufferLock *lock )
{
     DFBResult ret;

     D_MAGIC_ASSERT( surface, CoreSurface );

     D_DEBUG_AT( Core_Surface, "%s( %p )\n", __FUNCTION__, surface );

     ret = dfb_surface_buffer_unlock( lock );

     return ret;
}

DFBResult
dfb_surface_read_buffer( CoreSurface          *surface,
                         DFBSurfaceBufferRole  role,
                         void                 *destination,
                         int                   pitch,
                         const DFBRectangle   *rect )
{
     DFBResult              ret;
     int                    y;
     int                    bytes;
     DFBRectangle           rectangle;
     DFBSurfacePixelFormat  format;
     CoreSurfaceAllocation *allocation;

     D_MAGIC_ASSERT( surface, CoreSurface );
     D_ASSERT( destination != NULL );
     D_ASSERT( pitch > 0 );
     DFB_RECTANGLE_ASSERT_IF( rect );

     D_DEBUG_AT( Core_Surface, "%s( %p, %p, %d )\n", __FUNCTION__, surface, destination, pitch );

     /* Determine area. */
     rectangle.x = 0;
     rectangle.y = 0;
     rectangle.w = surface->config.size.w;
     rectangle.h = surface->config.size.h;

     if (rect && (!dfb_rectangle_intersect( &rectangle, rect ) || !DFB_RECTANGLE_EQUAL( rectangle, *rect )))
          return DFB_INVAREA;

     /* Calculate bytes per read line. */
     format = surface->config.format;
     bytes  = DFB_BYTES_PER_LINE( format, rectangle.w );

     D_DEBUG_AT( Core_Surface, "  -> %4d,%4d-%4dx%4d (%s)\n",
                 DFB_RECTANGLE_VALS( &rectangle ), dfb_pixelformat_name( format ) );

     ret = CoreSurface_PreLockBuffer2( surface, role, dfb_surface_get_stereo_eye( surface ), CSAID_CPU, CSAF_READ,
                                       false, &allocation );
     if (ret == DFB_NOALLOCATION) {
          for (y = 0; y < rectangle.h; y++) {
               memset( destination, 0, bytes );

               destination += pitch;
          }

          return DFB_OK;
     }

     if (ret)
          return ret;

     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );

     D_DEBUG_AT( Core_Surface, "  -> PreLockBuffer returned allocation %p (%s)\n",
                 allocation, allocation->pool->desc.name );

     /* Try reading from allocation directly... */
     ret = dfb_surface_pool_read( allocation->pool, allocation, destination, pitch, &rectangle );
     if (ret) {
          /* ...otherwise use fallback method via locking if possible. */
          if (allocation->access[CSAID_CPU] & CSAF_READ) {
               CoreSurfaceBufferLock lock;

               /* Lock the allocation. */
               dfb_surface_buffer_lock_init( &lock, CSAID_CPU, CSAF_READ );

               ret = dfb_surface_pool_lock( allocation->pool, allocation, &lock );
               if (ret) {
                    D_DERROR( ret, "Core/Surface: Locking allocation in '%s' failed!\n", allocation->pool->desc.name );
                    dfb_surface_buffer_lock_deinit( &lock );
                    dfb_surface_allocation_unref( allocation );
                    return ret;
               }

               /* Move to start of read. */
               lock.addr += DFB_BYTES_PER_LINE( format, rectangle.x ) + rectangle.y * lock.pitch;

               /* Copy the data. */
               for (y = 0; y < rectangle.h; y++) {
                    direct_memcpy( destination, lock.addr, bytes );

                    destination += pitch;
                    lock.addr   += lock.pitch;
               }

               /* Unlock the allocation. */
               ret = dfb_surface_pool_unlock( allocation->pool, allocation, &lock );
               if (ret)
                    D_DERROR( ret, "Core/Surface: Unlocking allocation in '%s' failed!\n",
                              allocation->pool->desc.name );

               dfb_surface_buffer_lock_deinit( &lock );
          }
     }

     dfb_surface_allocation_unref( allocation );

     return DFB_OK;
}

DFBResult
dfb_surface_write_buffer( CoreSurface          *surface,
                          DFBSurfaceBufferRole  role,
                          const void           *source,
                          int                   pitch,
                          const DFBRectangle   *rect )
{
     DFBResult              ret;
     DFBRectangle           rectangle;
     CoreSurfaceAllocation *allocation;

     D_MAGIC_ASSERT( surface, CoreSurface );
     D_ASSERT( pitch > 0 || source == NULL );
     DFB_RECTANGLE_ASSERT_IF( rect );

     D_DEBUG_AT( Core_Surface, "%s( %p, %p, %d )\n", __FUNCTION__, surface, source, pitch );

     /* Determine area. */
     rectangle.x = 0;
     rectangle.y = 0;
     rectangle.w = surface->config.size.w;
     rectangle.h = surface->config.size.h;

     if (rect) {
          if (!dfb_rectangle_intersect( &rectangle, rect )) {
               D_DEBUG_AT( Core_Surface, "  -> no intersection!\n" );
               return DFB_INVAREA;
          }

          if (!DFB_RECTANGLE_EQUAL( rectangle, *rect )) {
               D_DEBUG_AT( Core_Surface, "  -> got clipped to %4d,%4d-%4dx%4d!\n", DFB_RECTANGLE_VALS( &rectangle ) );
               return DFB_INVAREA;
          }
     }

     D_DEBUG_AT( Core_Surface, "  -> %4d,%4d-%4dx%4d (%s)\n",
                 DFB_RECTANGLE_VALS( &rectangle ), dfb_pixelformat_name( surface->config.format ) );

     ret = CoreSurface_PreLockBuffer2( surface, role, dfb_surface_get_stereo_eye( surface ), CSAID_CPU, CSAF_WRITE,
                                       false, &allocation );
     if (ret)
          return ret;

     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );

     D_DEBUG_AT( Core_Surface, "  -> PreLockBuffer returned allocation %p (%s)\n", allocation,
                 allocation->pool->desc.name );

     /* Try writing to allocation directly... */
     ret = source ? dfb_surface_pool_write( allocation->pool, allocation, source, pitch, &rectangle ) : DFB_UNSUPPORTED;
     if (ret) {
          /* ...otherwise use fallback method via locking if possible. */
          if (allocation->access[CSAID_CPU] & CSAF_WRITE) {
               int                   y;
               int                   bytes;
               DFBSurfacePixelFormat format;
               CoreSurfaceBufferLock lock;

               /* Calculate bytes per written line. */
               format = surface->config.format;
               bytes  = DFB_BYTES_PER_LINE( format, rectangle.w );

               /* Lock the allocation. */
               dfb_surface_buffer_lock_init( &lock, CSAID_CPU, CSAF_WRITE );

               ret = dfb_surface_pool_lock( allocation->pool, allocation, &lock );
               if (ret) {
                    D_DERROR( ret, "Core/Surface: Locking allocation in '%s' failed!\n", allocation->pool->desc.name );
                    dfb_surface_buffer_lock_deinit( &lock );
                    dfb_surface_allocation_unref( allocation );
                    return ret;
               }

               /* Move to start of write. */
               lock.addr += DFB_BYTES_PER_LINE( format, rectangle.x ) + rectangle.y * lock.pitch;

               /* Copy the data. */
               for (y = 0; y < rectangle.h; y++) {
                    if (source) {
                         direct_memcpy( lock.addr, source, bytes );

                         source += pitch;
                    }
                    else
                         memset( lock.addr, 0, bytes );

                    lock.addr += lock.pitch;
               }

               /* Unlock the allocation. */
               ret = dfb_surface_pool_unlock( allocation->pool, allocation, &lock );
               if (ret)
                    D_DERROR( ret, "Core/Surface: Unlocking allocation in '%s' failed!\n",
                              allocation->pool->desc.name );

               dfb_surface_buffer_lock_deinit( &lock );
          }
     }

     dfb_surface_allocation_unref( allocation );

     return DFB_OK;
}

DFBResult
dfb_surface_clear_buffers( CoreSurface *surface )
{
     D_MAGIC_ASSERT( surface, CoreSurface );

     D_DEBUG_AT( Core_Surface, "%s( %p )\n", __FUNCTION__, surface );

     if (surface->num_buffers == 0)
          return DFB_SUSPENDED;

     if (fusion_skirmish_prevail( &surface->lock ))
          return DFB_FUSION;

     dfb_gfx_clear( surface, DSBR_FRONT );

     if (surface->config.caps & DSCAPS_FLIPPING)
          dfb_gfx_clear( surface, DSBR_BACK );

     if (surface->config.caps & DSCAPS_TRIPLE)
          dfb_gfx_clear( surface, DSBR_IDLE );

     fusion_skirmish_dismiss( &surface->lock );

     return DFB_OK;
}

DFBResult
dfb_surface_dump_buffer( CoreSurface          *surface,
                         DFBSurfaceBufferRole  role,
                         const char           *path,
                         const char           *prefix )
{
     DFBResult          ret;
     CoreSurfaceBuffer *buffer;

     D_MAGIC_ASSERT( surface, CoreSurface );
     D_ASSERT( path != NULL );

     D_DEBUG_AT( Core_Surface, "%s( %p )\n", __FUNCTION__, surface );

     if (fusion_skirmish_prevail( &surface->lock ))
          return DFB_FUSION;

     if (surface->num_buffers == 0) {
          fusion_skirmish_dismiss( &surface->lock );
          return DFB_SUSPENDED;
     }

     buffer = dfb_surface_get_buffer( surface, role );

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     ret = buffer->allocs.count ? dfb_surface_buffer_dump( buffer, path, prefix ) : DFB_BUFFEREMPTY;

     fusion_skirmish_dismiss( &surface->lock );

     return ret;
}

DFBResult
dfb_surface_dump_buffer2( CoreSurface          *surface,
                          DFBSurfaceBufferRole  role,
                          DFBSurfaceStereoEye   eye,
                          const char           *path,
                          const char           *prefix )
{
     DFBResult              ret;
     int                    num  = -1;
     int                    i, n;
     int                    len = (path ? strlen( path ) : 0) + (prefix ? strlen( prefix ) : 0) + 40;
     char                   filename[len];
     char                   head[30];
     bool                   rgb = false;
     bool                   alpha = false;
     size_t                 bytes;
     CorePalette           *palette = NULL;
     CoreSurfaceAllocation *allocation;
     CoreSurfaceBufferLock  lock;
     DirectFile             fd_p, fd_g;

     D_MAGIC_ASSERT( surface, CoreSurface );
     D_ASSERT( path != NULL );

     D_DEBUG_AT( Core_Surface, "%s( %p )\n", __FUNCTION__, surface );

     ret = CoreSurface_PreLockBuffer2( surface, role, eye, CSAID_CPU, CSAF_READ, true, &allocation );
     if (ret)
          return ret;

     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );

     D_DEBUG_AT( Core_Surface, "  -> PreLockBuffer returned allocation %p (%s)\n",
                 allocation, allocation->pool->desc.name );

     /* Lock the allocation. */
     dfb_surface_buffer_lock_init( &lock, CSAID_CPU, CSAF_READ );

     ret = dfb_surface_pool_lock( allocation->pool, allocation, &lock );
     if (ret) {
          D_DERROR( ret, "Core/Surface: Locking allocation in '%s' failed!\n", allocation->pool->desc.name );
          dfb_surface_buffer_lock_deinit( &lock );
          dfb_surface_allocation_unref( allocation );
          return ret;
     }

     /* Check pixel format. */
     switch (lock.buffer->config.format) {
          case DSPF_LUT8:
               palette = surface->palette;

               if (!palette) {
                    D_BUG( "no palette" );
                    dfb_surface_buffer_unlock( &lock );
                    return DFB_BUG;
               }

               if (dfb_palette_ref( palette )) {
                    dfb_surface_buffer_unlock( &lock );
                    return DFB_FUSION;
               }

               rgb = true;
               /* fall through */

          case DSPF_A8:
               alpha = true;
               break;

          case DSPF_ARGB:
          case DSPF_ABGR:
          case DSPF_ARGB1555:
          case DSPF_RGBA5551:
          case DSPF_ARGB2554:
          case DSPF_ARGB4444:
          case DSPF_AiRGB:
          case DSPF_ARGB8565:
          case DSPF_AYUV:
          case DSPF_AVYU:
               alpha = true;
               /* fall through */

          case DSPF_RGB332:
          case DSPF_RGB444:
          case DSPF_RGB555:
          case DSPF_BGR555:
          case DSPF_RGB16:
          case DSPF_RGB24:
          case DSPF_BGR24:
          case DSPF_RGB32:
          case DSPF_YUY2:
          case DSPF_UYVY:
          case DSPF_VYU:
          case DSPF_I420:
          case DSPF_YV12:
          case DSPF_NV12:
          case DSPF_NV21:
          case DSPF_Y42B:
          case DSPF_YV16:
          case DSPF_NV16:
          case DSPF_NV61:
          case DSPF_Y444:
          case DSPF_YV24:
          case DSPF_NV24:
          case DSPF_NV42:
               rgb = true;
               break;

          default:
               D_ERROR( "Core/Surface: Surface dump for format '%s' is not implemented!\n",
                        dfb_pixelformat_name( lock.buffer->config.format ) );
               dfb_surface_buffer_unlock( &lock );
               return DFB_UNSUPPORTED;
     }

     if (prefix) {
          /* Find the lowest unused index. */
          while (++num < 10000) {
               snprintf( filename, len, "%s/%s_%04d.ppm", path, prefix, num );

               ret = direct_access( filename, F_OK );
               if (ret) {
                    snprintf( filename, len, "%s/%s_%04d.pgm", path, prefix, num );
                    ret = direct_access( filename, F_OK );
                    if (ret)
                         break;
               }
          }

          if (num == 10000) {
               D_ERROR( "Core/Surface: Could not find an unused index for surface dump!\n" );
               dfb_surface_buffer_unlock( &lock );
               if (palette)
                    dfb_palette_unref( palette );
               return DFB_FAILURE;
          }
     }

     /* Create a file with the found index. */
     if (rgb) {
          if (prefix)
               snprintf( filename, len, "%s/%s_%04d.ppm", path, prefix, num );
          else
               snprintf( filename, len, "%s.ppm", path );

          ret = direct_file_open( &fd_p, filename, O_EXCL | O_CREAT | O_WRONLY, 0644 );
          if (ret) {
               D_DERROR( ret, "Core/Surface: Could not open '%s'!\n", filename );
               dfb_surface_buffer_unlock( &lock );
               if (palette)
                    dfb_palette_unref( palette );
               return ret;
          }
     }

     /* Create a graymap for the alpha channel using the found index. */
     if (alpha) {
          if (prefix)
               snprintf( filename, len, "%s/%s_%04d.pgm", path, prefix, num );
          else
               snprintf( filename, len, "%s.pgm", path );

          ret = direct_file_open( &fd_g, filename, O_EXCL | O_CREAT | O_WRONLY, 0644 );
          if (ret) {
               D_DERROR( ret, "Core/Surface: Could not open '%s'!\n", filename );

               dfb_surface_buffer_unlock( &lock );
               if (palette)
                    dfb_palette_unref( palette );

               if (rgb) {
                    direct_file_close( &fd_p );
                    if (prefix)
                         snprintf( filename, len, "%s/%s_%04d.ppm", path, prefix, num );
                    else
                         snprintf( filename, len, "%s.ppm", path );
                    direct_unlink( filename );
               }

               return ret;
          }
     }

     if (rgb) {
          /* Write the pixmap header. */
          snprintf( head, sizeof(head), "P6\n%d %d\n255\n", surface->config.size.w, surface->config.size.h );
          direct_file_write( &fd_p, head, strlen( head ), &bytes );
     }

     /* Write the graymap header. */
     if (alpha) {
          snprintf( head, sizeof(head), "P5\n%d %d\n255\n", surface->config.size.w, surface->config.size.h );
          direct_file_write( &fd_g, head, strlen( head ), &bytes );
     }

     /* Write the pixmap (and graymap) data. */
     for (i = 0; i < surface->config.size.h; i++) {
          int n3;

          /* Prepare one row. */
          u8  *srces[3]   = { NULL, NULL, NULL };
          int  pitches[3] = { 0, 0, 0 };
          u8  *src8;

          dfb_surface_get_data_offsets( &surface->config, lock.addr, lock.pitch, 0, i, 3, srces, pitches );
          src8 = srces[0];

          /* Write color buffer to pixmap file. */
          if (rgb) {
               u8 buf_p[surface->config.size.w*3];

               if (lock.buffer->config.format == DSPF_LUT8) {
                    for (n = 0, n3 = 0; n < surface->config.size.w; n++, n3 += 3) {
                         buf_p[n3+0] = palette->entries[src8[n]].r;
                         buf_p[n3+1] = palette->entries[src8[n]].g;
                         buf_p[n3+2] = palette->entries[src8[n]].b;
                    }
               }
               else
                    dfb_convert_to_rgb24( lock.buffer->config.format, lock.buffer->config.colorspace,
                                          srces[0], pitches[0], srces[1], pitches[1], srces[2], pitches[2],
                                          surface->config.size.h, buf_p,
                                          surface->config.size.w * 3, surface->config.size.w, 1 );

               direct_file_write( &fd_p, buf_p, surface->config.size.w * 3, &bytes );
          }

          /* Write alpha buffer to graymap file. */
          if (alpha) {
               u8 buf_g[surface->config.size.w];

               if (lock.buffer->config.format == DSPF_LUT8) {
                    for (n = 0; n < surface->config.size.w; n++)
                         buf_g[n] = palette->entries[src8[n]].a;
               }
               else
                    dfb_convert_to_a8( lock.buffer->config.format, srces[0], pitches[0], surface->config.size.h, buf_g,
                                       surface->config.size.w, surface->config.size.w, 1 );

               direct_file_write( &fd_g, buf_g, surface->config.size.w, &bytes );
          }
     }

     /* Unlock the surface buffer. */
     dfb_surface_buffer_unlock( &lock );

     /* Release the palette. */
     if (palette)
          dfb_palette_unref( palette );

     /* Close pixmap file. */
     if (rgb)
          direct_file_close( &fd_p );

     /* Close graymap file. */
     if (alpha)
          direct_file_close( &fd_g );

     return DFB_OK;
}

DFBResult
dfb_surface_dump_raw_buffer( CoreSurface          *surface,
                             DFBSurfaceBufferRole  role,
                             const char           *path,
                             const char           *prefix )
{
     DFBResult          ret;
     CoreSurfaceBuffer *buffer;

     D_MAGIC_ASSERT( surface, CoreSurface );
     D_ASSERT( path != NULL );
     D_ASSERT( prefix != NULL );

     D_DEBUG_AT( Core_Surface, "%s( %p )\n", __FUNCTION__, surface );

     if (surface->num_buffers == 0)
          return DFB_SUSPENDED;

     if (fusion_skirmish_prevail( &surface->lock ))
          return DFB_FUSION;

     buffer = dfb_surface_get_buffer( surface, role );

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     ret = dfb_surface_buffer_dump_raw( buffer, path, prefix );

     fusion_skirmish_dismiss( &surface->lock );

     return ret;
}

DFBResult
dfb_surface_set_palette( CoreSurface *surface,
                         CorePalette *palette )
{
     D_MAGIC_ASSERT( surface, CoreSurface );
     D_MAGIC_ASSERT_IF( palette, CorePalette );

     D_DEBUG_AT( Core_Surface, "%s( %p )\n", __FUNCTION__, surface );

     if (fusion_skirmish_prevail( &surface->lock ))
          return DFB_FUSION;

     if (surface->palette != palette) {
          if (surface->palette) {
               dfb_palette_detach_global( surface->palette, &surface->palette_reaction );
               dfb_palette_unlink( &surface->palette );
          }

          if (palette) {
               dfb_palette_link( &surface->palette, palette );
               dfb_palette_attach_global( palette, DFB_SURFACE_PALETTE_LISTENER, surface, &surface->palette_reaction );
          }

          dfb_surface_notify( surface, CSNF_PALETTE_CHANGE );
     }

     fusion_skirmish_dismiss( &surface->lock );

     return DFB_OK;
}

DFBResult
dfb_surface_set_field( CoreSurface *surface,
                       int          field )
{
     D_MAGIC_ASSERT( surface, CoreSurface );

     D_DEBUG_AT( Core_Surface, "%s( %p )\n", __FUNCTION__, surface );

     if (fusion_skirmish_prevail( &surface->lock ))
          return DFB_FUSION;

     surface->field = field;

     dfb_surface_notify( surface, CSNF_FIELD );

     fusion_skirmish_dismiss( &surface->lock );

     return DFB_OK;
}

DFBResult
dfb_surface_set_alpha_ramp( CoreSurface *surface,
                            u8           a0,
                            u8           a1,
                            u8           a2,
                            u8           a3 )
{
     D_MAGIC_ASSERT( surface, CoreSurface );

     D_DEBUG_AT( Core_Surface, "%s( %p )\n", __FUNCTION__, surface );

     if (fusion_skirmish_prevail( &surface->lock ))
          return DFB_FUSION;

     surface->alpha_ramp[0] = a0;
     surface->alpha_ramp[1] = a1;
     surface->alpha_ramp[2] = a2;
     surface->alpha_ramp[3] = a3;

     dfb_surface_notify( surface, CSNF_ALPHA_RAMP );

     fusion_skirmish_dismiss( &surface->lock );

     return DFB_OK;
}

CoreSurfaceBuffer *
dfb_surface_get_buffer( CoreSurface          *surface,
                        DFBSurfaceBufferRole  role )
{
     D_MAGIC_ASSERT( surface, CoreSurface );
     FUSION_SKIRMISH_ASSERT( &surface->lock );
     D_ASSERT( surface->num_buffers > 0 );
     D_ASSERT( role == DSBR_FRONT || role == DSBR_BACK || role == DSBR_IDLE );

     D_DEBUG_AT( Core_Surface, "%s( %p )\n", __FUNCTION__, surface );

     return surface->buffers[surface->buffer_indices[role % surface->num_buffers]];
}

CoreSurfaceBuffer *
dfb_surface_get_buffer2( CoreSurface          *surface,
                         DFBSurfaceBufferRole  role,
                         DFBSurfaceStereoEye   eye )
{
     D_MAGIC_ASSERT( surface, CoreSurface );
     FUSION_SKIRMISH_ASSERT( &surface->lock );
     D_ASSERT( surface->num_buffers > 0 );
     D_ASSERT( role == DSBR_FRONT || role == DSBR_BACK || role == DSBR_IDLE );
     D_ASSERT( eye == DSSE_LEFT || eye == DSSE_RIGHT );

     D_DEBUG_AT( Core_Surface, "%s( %p )\n", __FUNCTION__, surface );

     if (eye == DSSE_LEFT)
          return surface->left_buffers[surface->buffer_indices[role % surface->num_buffers]];

     return surface->right_buffers[surface->buffer_indices[role % surface->num_buffers]];
}

CoreSurfaceBuffer *
dfb_surface_get_buffer3( CoreSurface          *surface,
                         DFBSurfaceBufferRole  role,
                         DFBSurfaceStereoEye   eye,
                         u32                   flip_count )
{
     CoreSurfaceBuffer *buffer;

     D_MAGIC_ASSERT( surface, CoreSurface );
     FUSION_SKIRMISH_ASSERT( &surface->lock );
     D_ASSERT( surface->num_buffers > 0 );
     D_ASSERT( role == DSBR_FRONT || role == DSBR_BACK || role == DSBR_IDLE );
     D_ASSERT( eye == DSSE_LEFT || eye == DSSE_RIGHT );

     D_DEBUG_AT( Core_Surface, "%s( %p, role %u, eye %u, flip_count %u )\n", __FUNCTION__, surface, role, eye,
                 flip_count );

     if (eye == DSSE_LEFT) {
          buffer = fusion_hash_lookup( surface->frames, (void*)(long) ((flip_count + role) * 2) );

          return buffer ?: surface->left_buffers[surface->buffer_indices[(flip_count + role) % surface->num_buffers]];
     }

     buffer = fusion_hash_lookup( surface->frames, (void*)(long) ((flip_count + role) * 2 + 1) );

     return buffer ?: surface->right_buffers[surface->buffer_indices[(flip_count + role) % surface->num_buffers]];
}

ReactionResult
_dfb_surface_palette_listener( const void *msg_data,
                               void       *ctx )
{
     const CorePaletteNotification *notification = msg_data;
     CoreSurface                   *surface      = ctx;

     if (notification->flags & CPNF_DESTROY)
          return RS_REMOVE;

     if (notification->flags & CPNF_ENTRIES) {
          if (fusion_skirmish_prevail( &surface->lock ))
               return RS_OK;

          dfb_surface_notify( surface, CSNF_PALETTE_UPDATE );

          fusion_skirmish_dismiss( &surface->lock );
     }

     return RS_OK;
}

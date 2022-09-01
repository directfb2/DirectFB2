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

#ifndef __CORE__SURFACE_H__
#define __CORE__SURFACE_H__

#include <core/coredefs.h>
#include <core/coretypes.h>
#include <direct/serial.h>
#include <fusion/object.h>

/**********************************************************************************************************************/

typedef enum {
     CSSF_NONE      = 0x00000000, /* None of these. */

     CSSF_DESTROYED = 0x00000001, /* Surface is being or has been destroyed. */

     CSSF_ALL       = 0x00000001  /* All of these. */
} CoreSurfaceStateFlags;

typedef enum {
     CSCONF_NONE         = 0x00000000, /* none of these */

     CSCONF_SIZE         = 0x00000001, /* set size */
     CSCONF_FORMAT       = 0x00000002, /* set format */
     CSCONF_CAPS         = 0x00000004, /* set capabilities */
     CSCONF_COLORSPACE   = 0x00000008, /* set color space */
     CSCONF_PREALLOCATED = 0x00000010, /* data has been preallocated */

     CSCONF_ALL          = 0x0000001F  /* all of these */
} CoreSurfaceConfigFlags;

struct __DFB_CoreSurfaceConfig {
     CoreSurfaceConfigFlags  flags;

     DFBDimension            size;
     DFBSurfacePixelFormat   format;
     DFBSurfaceColorSpace    colorspace;
     DFBSurfaceCapabilities  caps;

     struct {
          void              *addr;
          unsigned long      phys;
          unsigned long      offset;
          unsigned int       pitch;
          void              *handle;
     } preallocated[MAX_SURFACE_BUFFERS];

     CoreSurfacePoolID       preallocated_pool_id;

     DFBDimension            min_size;
     DFBSurfaceHintFlags     hints;
};

typedef enum {
     CSTF_NONE         = 0x00000000, /* none of these */

     CSTF_LAYER        = 0x00000001, /* surface for layer */
     CSTF_WINDOW       = 0x00000002, /* surface for window */
     CSTF_CURSOR       = 0x00000004, /* surface for cursor */
     CSTF_FONT         = 0x00000008, /* surface for font */
     CSTF_SHARED       = 0x00000010, /* accessable by other processes */

     CSTF_INTERNAL     = 0x00000100, /* system memory */
     CSTF_EXTERNAL     = 0x00000200, /* video memory */
     CSTF_PREALLOCATED = 0x00000400, /* preallocated memory */

     CSTF_ALL          = 0x0000071F  /* all of these */
} CoreSurfaceTypeFlags;

typedef enum {
     CSNF_NONE                      = 0x00000000, /* none of these */

     CSNF_SIZEFORMAT                = 0x00000001, /* width, height, format */

     CSNF_DESTROY                   = 0x00000008, /* surface is about to be destroyed */
     CSNF_FLIP                      = 0x00000010, /* surface buffer pointer swapped */
     CSNF_FIELD                     = 0x00000020, /* active (displayed) field switched */
     CSNF_PALETTE_CHANGE            = 0x00000040, /* another palette has been set */
     CSNF_PALETTE_UPDATE            = 0x00000080, /* current palette has been altered */
     CSNF_ALPHA_RAMP                = 0x00000100, /* alpha ramp was modified */
     CSNF_DISPLAY                   = 0x00000200, /* surface buffer displayed */
     CSNF_FRAME                     = 0x00000400, /* flip count ack */
     CSNF_BUFFER_ALLOCATION_DESTROY = 0x00000800, /* buffer allocation about to be destroyed */

     CSNF_ALL                       = 0x00000FF9  /* all of these */
} CoreSurfaceNotificationFlags;

struct __DFB_CoreSurface
{
     FusionObject                   object;
     int                            magic;

     FusionSkirmish                 lock;

     CoreSurfaceStateFlags          state;

     CoreSurfaceConfig              config;
     CoreSurfaceTypeFlags           type;
     unsigned long                  resource_id;                         /* layer id, window id, or user specified */

     int                            rotation;

     CoreSurfaceNotificationFlags   notifications;

     DirectSerial                   serial;

     int                            field;
     u8                             alpha_ramp[4];

     CoreSurfaceBuffer            **buffers;
     CoreSurfaceBuffer             *left_buffers[MAX_SURFACE_BUFFERS];
     CoreSurfaceBuffer             *right_buffers[MAX_SURFACE_BUFFERS];
     int                            num_buffers;
     int                            buffer_indices[MAX_SURFACE_BUFFERS];

     u32                            flips;

     CorePalette                   *palette;
     GlobalReaction                 palette_reaction;

     FusionSHMPoolShared           *shmpool;

     void                          *data;                                /* shared system driver-specific data */

     FusionCall                     call;

     FusionVector                   clients;
     u32                            flips_acked;

     DFBFrameTimeConfig             frametime_config;

     long long                      last_frame_time;

     FusionHash                    *frames;

     DirectSerial                   config_serial;
};

/**********************************************************************************************************************/

typedef enum {
     CSAF_NONE   = 0x00000000, /* none of these */

     CSAF_READ   = 0x00000001, /* accessor may read */
     CSAF_WRITE  = 0x00000002, /* accessor may write */

     CSAF_SHARED = 0x00000010, /* other processes can read/write at the same time (shared mapping) */

     CSAF_ALL    = 0x00000013  /* all of these */
} CoreSurfaceAccessFlags;

typedef enum {
     CSCH_EVENT = 0x00000001, /* DFEC_SURFACE DFBSurfaceEvent */
     CSCH_FRAME = 0x00000002, /* CSNF_FRAME CoreSurfaceNotification */
} CoreSurfaceChannel;

typedef struct {
     CoreSurfaceNotificationFlags  flags;
     CoreSurface                  *surface;

     /* The following field is used only by the CSNF_DISPLAY message. */
     int                           index;

     /* The following fields are used only by the CSNF_BUFFER_ALLOCATION_DESTROY message. */
     CoreSurfaceBuffer            *buffer_no_access;  /* Pointer to associated CoreSurfaceBuffer being destroyed. */
     void                         *surface_data;      /* CoreSurface's shared driver specific data. */
     int                           surface_object_id; /* CoreSurface's Fusion ID. */

     unsigned int                  flip_count;
} CoreSurfaceNotification;

/**********************************************************************************************************************/

/*
 * Creates a pool of surface objects.
 */
FusionObjectPool  *dfb_surface_pool_create       ( const FusionWorld             *world );

/*
 * Generates dfb_surface_ref(), dfb_surface_attach() etc.
 */
FUSION_OBJECT_METHODS( CoreSurface, dfb_surface )

typedef enum {
     DFB_LAYER_REGION_SURFACE_LISTENER         = 0x00000000,
     DFB_WINDOWSTACK_BACKGROUND_IMAGE_LISTENER = 0x00000001
} DFB_SURFACE_GLOBALS;

/**********************************************************************************************************************/

DFBResult          dfb_surface_create            ( CoreDFB                       *core,
                                                   const CoreSurfaceConfig       *config,
                                                   CoreSurfaceTypeFlags           type,
                                                   unsigned long                  resource_id,
                                                   CorePalette                   *palette,
                                                   CoreSurface                  **ret_surface );

DFBResult          dfb_surface_create_simple     ( CoreDFB                       *core,
                                                   int                            width,
                                                   int                            height,
                                                   DFBSurfacePixelFormat          format,
                                                   DFBSurfaceColorSpace           colorspace,
                                                   DFBSurfaceCapabilities         caps,
                                                   CoreSurfaceTypeFlags           type,
                                                   unsigned long                  resource_id,
                                                   CorePalette                   *palette,
                                                   CoreSurface                  **ret_surface );

DFBResult          dfb_surface_init_palette      ( CoreDFB                       *core,
                                                   CoreSurface                   *surface );

DFBResult          dfb_surface_notify            ( CoreSurface                   *surface,
                                                   CoreSurfaceNotificationFlags   flags );

DFBResult          dfb_surface_notify_display    ( CoreSurface                   *surface,
                                                  CoreSurfaceBuffer              *buffer );

DFBResult          dfb_surface_notify_display2   ( CoreSurface                   *surface,
                                                   int                            index );

DFBResult          dfb_surface_notify_frame      ( CoreSurface                   *surface,
                                                   unsigned int                   flip_count );

DFBResult          dfb_surface_pool_notify       ( CoreSurface                   *surface,
                                                   CoreSurfaceBuffer             *buffer,
                                                   CoreSurfaceAllocation         *allocation,
                                                   CoreSurfaceNotificationFlags   flags );

DFBResult          dfb_surface_flip              ( CoreSurface                   *surface,
                                                   bool                           swap );

DFBResult          dfb_surface_flip_buffers      ( CoreSurface                   *surface,
                                                   bool                           swap );

DFBResult          dfb_surface_dispatch_event    ( CoreSurface                   *surface,
                                                   DFBSurfaceEventType            type );

DFBResult          dfb_surface_dispatch_update   ( CoreSurface                   *surface,
                                                   const DFBRegion               *update,
                                                   const DFBRegion               *update_right,
                                                   long long                      timestamp,
                                                   DFBSurfaceFlipFlags            flags );

DFBResult          dfb_surface_check_acks        ( CoreSurface                   *surface );

DFBResult          dfb_surface_reconfig          ( CoreSurface                   *surface,
                                                   const CoreSurfaceConfig       *config );

DFBResult          dfb_surface_reformat          ( CoreSurface                   *surface,
                                                   int                            width,
                                                   int                            height,
                                                   DFBSurfacePixelFormat          format );

DFBResult          dfb_surface_destroy_buffers   ( CoreSurface                   *surface );

DFBResult          dfb_surface_deallocate_buffers( CoreSurface                   *surface );

DFBResult          dfb_surface_destroy           ( CoreSurface                   *surface );

DFBResult          dfb_surface_lock_buffer       ( CoreSurface                   *surface,
                                                   DFBSurfaceBufferRole           role,
                                                   CoreSurfaceAccessorID          accessor,
                                                   CoreSurfaceAccessFlags         access,
                                                   CoreSurfaceBufferLock         *ret_lock );

DFBResult          dfb_surface_lock_buffer2      ( CoreSurface                   *surface,
                                                   DFBSurfaceBufferRole           role,
                                                   u32                            flip_count,
                                                   DFBSurfaceStereoEye            eye,
                                                   CoreSurfaceAccessorID          accessor,
                                                   CoreSurfaceAccessFlags         access,
                                                   CoreSurfaceBufferLock         *ret_lock );

DFBResult          dfb_surface_unlock_buffer     ( CoreSurface                   *surface,
                                                   CoreSurfaceBufferLock         *lock );

DFBResult          dfb_surface_read_buffer       ( CoreSurface                   *surface,
                                                   DFBSurfaceBufferRole           role,
                                                   void                          *destination,
                                                   int                            pitch,
                                                   const DFBRectangle            *rect );

DFBResult          dfb_surface_write_buffer      ( CoreSurface                   *surface,
                                                   DFBSurfaceBufferRole           role,
                                                   const void                    *source,
                                                   int                            pitch,
                                                   const DFBRectangle            *rect );

DFBResult          dfb_surface_clear_buffers     ( CoreSurface                   *surface );

DFBResult          dfb_surface_dump_buffer       ( CoreSurface                   *surface,
                                                   DFBSurfaceBufferRole           role,
                                                   const char                    *path,
                                                   const char                    *prefix );

DFBResult          dfb_surface_dump_buffer2      ( CoreSurface                   *surface,
                                                   DFBSurfaceBufferRole           role,
                                                   DFBSurfaceStereoEye            eye,
                                                   const char                    *path,
                                                   const char                    *prefix );

DFBResult          dfb_surface_dump_raw_buffer   ( CoreSurface                   *surface,
                                                   DFBSurfaceBufferRole           role,
                                                   const char                    *path,
                                                   const char                    *prefix );

DFBResult          dfb_surface_set_palette       ( CoreSurface                   *surface,
                                                   CorePalette                   *palette );

DFBResult          dfb_surface_set_field         ( CoreSurface                   *surface,
                                                   int                            field );

DFBResult          dfb_surface_set_alpha_ramp    ( CoreSurface                   *surface,
                                                   u8                             a0,
                                                   u8                             a1,
                                                   u8                             a2,
                                                   u8                             a3 );

CoreSurfaceBuffer *dfb_surface_get_buffer        ( CoreSurface                   *surface,
                                                   DFBSurfaceBufferRole           role );

CoreSurfaceBuffer *dfb_surface_get_buffer2       ( CoreSurface                   *surface,
                                                   DFBSurfaceBufferRole           role,
                                                   DFBSurfaceStereoEye            eye );

CoreSurfaceBuffer *dfb_surface_get_buffer3       ( CoreSurface                   *surface,
                                                   DFBSurfaceBufferRole           role,
                                                   DFBSurfaceStereoEye            eye,
                                                   u32                            flip_count );

/*
 * Global reaction, listen to the palette's surface.
 */

ReactionResult    _dfb_surface_palette_listener  ( const void                    *msg_data,
                                                   void                          *ctx );

/**********************************************************************************************************************/

static __inline__ DirectResult
dfb_surface_lock( CoreSurface *surface )
{
     D_MAGIC_ASSERT( surface, CoreSurface );

     return fusion_skirmish_prevail( &surface->lock );
}

static __inline__ DirectResult
dfb_surface_trylock( CoreSurface *surface )
{
     D_MAGIC_ASSERT( surface, CoreSurface );

     return fusion_skirmish_swoop( &surface->lock );
}

static __inline__ DirectResult
dfb_surface_unlock( CoreSurface *surface )
{
     D_MAGIC_ASSERT( surface, CoreSurface );

     return fusion_skirmish_dismiss( &surface->lock );
}

static __inline__ void
dfb_surface_get_data_offsets( const CoreSurfaceConfig  *config,
                              const void               *data,
                              int                       pitch,
                              int                       x,
                              int                       y,
                              unsigned int              num,
                              u8                      **pointers,
                              int                      *pitches )
{
     D_ASSERT( config != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( pitch > 0 );
     D_ASSERT( x >= 0 );
     D_ASSERT( x < config->size.w );
     D_ASSERT( y >= 0 );
     D_ASSERT( y < config->size.h );
     D_ASSERT( !num || (num && pointers && pitches) );

     if (!num)
          return;

     switch (config->format) {
          case DSPF_NV12:
          case DSPF_NV21:
          case DSPF_NV16:
          case DSPF_NV61:
               if (num < 2)
                    return;
               break;

          case DSPF_I420:
          case DSPF_YV12:
          case DSPF_Y42B:
          case DSPF_YV16:
          case DSPF_Y444:
          case DSPF_YV24:
               if (num < 3)
                    return;
               break;

          default:
               if (num < 1)
                    return;
               break;
     }

     if (config->caps & DSCAPS_SEPARATED) {
          if (y & 1)
               y += config->size.h;

          y >>= 1;
     }

     switch (config->format) {
          case DSPF_NV12:
          case DSPF_NV21:
               pitches[1]  = pitch;
               pointers[1] = (u8*) data
                             + pitch * config->size.h
                             + pitches[1] * y / 2
                             + DFB_BYTES_PER_LINE( config->format, x / 2 );
               break;

          case DSPF_NV16:
          case DSPF_NV61:
               pitches[1]  = pitch;
               pointers[1] = (u8*) data
                             + pitch * config->size.h
                             + pitches[1] * y
                             + DFB_BYTES_PER_LINE( config->format, x / 2 );
               break;

          case DSPF_I420:
               pitches[1]  = pitches[2] = pitch / 2;
               pointers[1] = (u8*) data
                             + pitch * config->size.h
                             + pitches[1] * y / 2
                             + DFB_BYTES_PER_LINE( config->format, x / 2 );
               pointers[2] = (u8*) data
                             + pitch * config->size.h
                             + pitches[1] * config->size.h / 2
                             + pitches[2] * y / 2
                             + DFB_BYTES_PER_LINE( config->format, x / 2 );
               break;

          case DSPF_YV12:
               pitches[1]  = pitches[2] = pitch / 2;
               pointers[2] = (u8*) data
                             + pitch * config->size.h
                             + pitches[2] * y / 2
                             + DFB_BYTES_PER_LINE( config->format, x / 2 );
               pointers[1] = (u8*) data
                             + pitch * config->size.h
                             + pitches[2] * config->size.h / 2
                             + pitches[1] * y / 2
                             + DFB_BYTES_PER_LINE( config->format, x / 2 );
               break;

          case DSPF_Y42B:
               pitches[1]  = pitches[2] = pitch / 2;
               pointers[1] = (u8*) data
                             + pitch * config->size.h
                             + pitches[1] * y
                             + DFB_BYTES_PER_LINE( config->format, x / 2 );
               pointers[2] = (u8*) data
                             + pitch * config->size.h
                             + pitches[1] * config->size.h
                             + pitches[2] * y
                             + DFB_BYTES_PER_LINE( config->format, x / 2 );
               break;

          case DSPF_YV16:
               pitches[1]  = pitches[2] = pitch / 2;
               pointers[2] = (u8*) data
                             + pitch * config->size.h
                             + pitches[2] * y
                             + DFB_BYTES_PER_LINE( config->format, x / 2 );
               pointers[1] = (u8*) data
                             + pitch * config->size.h
                             + pitches[2] * config->size.h
                             + pitches[1] * y
                             + DFB_BYTES_PER_LINE( config->format, x / 2 );
               break;

          case DSPF_Y444:
               pitches[1]  = pitches[2] = pitch;
               pointers[1] = (u8*) data
                             + pitch * config->size.h
                             + pitches[1] * y
                             + DFB_BYTES_PER_LINE( config->format, x );
               pointers[2] = (u8*) data
                             + pitch * config->size.h
                             + pitches[1] * config->size.h
                             + pitches[2] * y
                             + DFB_BYTES_PER_LINE( config->format, x );
               break;

          case DSPF_YV24:
               pitches[1]  = pitches[2] = pitch;
               pointers[2] = (u8*) data
                             + pitch * config->size.h
                             + pitches[2] * y
                             + DFB_BYTES_PER_LINE( config->format, x );
               pointers[1] = (u8*) data
                             + pitch * config->size.h
                             + pitches[2] * config->size.h
                             + pitches[1] * y
                             + DFB_BYTES_PER_LINE( config->format, x );
               break;

          default:
               break;
     }

     pointers[0] = (u8*) data + pitch * y + DFB_BYTES_PER_LINE( config->format, x );
     pitches[0]  = pitch;
}

static __inline__ void
dfb_surface_calc_buffer_size( CoreSurface *surface,
                              int          byte_align,
                              int          pixel_align,
                              int         *ret_pitch,
                              int         *ret_size )
{
     DFBSurfacePixelFormat format;
     int                   width;
     int                   pitch;

     D_MAGIC_ASSERT( surface, CoreSurface );

     format = surface->config.format;

     width = direct_util_align( surface->config.size.w, pixel_align );
     pitch = direct_util_align( DFB_BYTES_PER_LINE( format, width ), byte_align );

     if (ret_pitch)
          *ret_pitch = pitch;

     if (ret_size)
          *ret_size = pitch * DFB_PLANE_MULTIPLY( format, surface->config.size.h );
}

#endif

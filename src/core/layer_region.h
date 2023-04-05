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

#ifndef __CORE__LAYER_REGION_H__
#define __CORE__LAYER_REGION_H__

#include <core/coretypes.h>
#include <fusion/object.h>

/**********************************************************************************************************************/

typedef enum {
     CLRSF_NONE       = 0x00000000,

     CLRSF_CONFIGURED = 0x00000001,
     CLRSF_ENABLED    = 0x00000002,
     CLRSF_ACTIVE     = 0x00000004,
     CLRSF_REALIZED   = 0x00000008,
     CLRSF_FROZEN     = 0x00000010,

     CLRSF_ALL        = 0x0000001F
} CoreLayerRegionStateFlags;

typedef struct {
     int                        width;         /* width of the source in pixels */
     int                        height;        /* height of the source in pixels */
     DFBSurfacePixelFormat      format;        /* pixel format of the source surface */
     DFBSurfaceColorSpace       colorspace;    /* color space of the source surface */
     DFBSurfaceCapabilities     surface_caps;  /* capabilities of the source surface */
     DFBDisplayLayerBufferMode  buffermode;    /* surface buffer configuration */

     DFBDisplayLayerOptions     options;       /* various configuration options */

     DFBDisplayLayerSourceID    source_id;     /* selected source */

     DFBRectangle               source;        /* viewport within source (input) */
     DFBRectangle               dest;          /* viewport on screen (output) */

     u8                         opacity;       /* global region alpha */

     DFBColorKey                src_key;       /* source color key */
     DFBColorKey                dst_key;       /* destination color key */

     int                        parity;        /* field parity (for interlaced) */

     u8                         alpha_ramp[4]; /* alpha values for 1 or 2 bit lookup */

     DFBRegion                 *clips;         /* clip regions */
     int                        num_clips;     /* number of clip regions */
     DFBBoolean                 positive;      /* show or cut out regions */

     bool                       keep_buffers;  /* keep buffers */
} CoreLayerRegionConfig;

#if D_DEBUG_ENABLED
#define DFB_CORE_LAYER_REGION_CONFIG_DEBUG_AT(domain,config)                                                           \
     do {                                                                                                              \
          D_DEBUG_AT( domain, "  -> size       %dx%d\n",                   (config)->width, (config)->height );        \
          D_DEBUG_AT( domain, "  -> format     %s\n",                      dfb_pixelformat_name( (config)->format ) ); \
          D_DEBUG_AT( domain, "  -> color spc  %u\n",                      (config)->colorspace );                     \
          D_DEBUG_AT( domain, "  -> surf caps  0x%08x\n",                  (config)->surface_caps );                   \
          D_DEBUG_AT( domain, "  -> buffermode %u\n",                      (config)->buffermode );                     \
          D_DEBUG_AT( domain, "  -> options    0x%08x\n",                  (config)->options );                        \
          D_DEBUG_AT( domain, "  -> source     %4d,%4d-%4dx%4d\n",         DFB_RECTANGLE_VALS( &(config)->source ) );  \
          D_DEBUG_AT( domain, "  -> dest       %4d,%4d-%4dx%4d\n",         DFB_RECTANGLE_VALS( &(config)->dest ) );    \
          D_DEBUG_AT( domain, "  -> opacity    %d\n",                      (config)->opacity );                        \
          D_DEBUG_AT( domain, "  -> src_key    %02x%02x%02x (index %d)\n", DFB_COLORKEY_VALS( &(config)->src_key ) );  \
          D_DEBUG_AT( domain, "  -> dst_key    %02x%02x%02x (index %d)\n", DFB_COLORKEY_VALS( &(config)->dst_key ) );  \
     } while (0)
#else
#define DFB_CORE_LAYER_REGION_CONFIG_DEBUG_AT(domain,config)                                                           \
     do {                                                                                                              \
     } while (0)
#endif

struct __DFB_CoreLayerRegion {
     FusionObject               object;

     FusionObjectID             context_id;

     FusionSkirmish             lock;

     CoreLayerRegionStateFlags  state;

     CoreLayerRegionConfig      config;

     CoreSurface               *surface;
     GlobalReaction             surface_reaction;

     CoreSurfaceClient         *surface_client;
     Reaction                   surface_event_reaction;

     void                      *region_data;

     CoreSurfaceAccessorID      surface_accessor;

     FusionCall                 call;

     DFBDisplayLayerID          layer_id;

     u32                        surface_flip_count;
};

/**********************************************************************************************************************/

typedef enum {
     CLRCF_NONE         = 0x00000000,

     CLRCF_WIDTH        = 0x00000001,
     CLRCF_HEIGHT       = 0x00000002,
     CLRCF_FORMAT       = 0x00000004,
     CLRCF_SURFACE_CAPS = 0x00000008,
     CLRCF_BUFFERMODE   = 0x00000010,
     CLRCF_OPTIONS      = 0x00000020,
     CLRCF_SOURCE_ID    = 0x00000040,
     CLRCF_COLORSPACE   = 0x00000080,
     CLRCF_SOURCE       = 0x00000100,
     CLRCF_DEST         = 0x00000200,
     CLRCF_CLIPS        = 0x00000400,

     CLRCF_OPACITY      = 0x00001000,
     CLRCF_ALPHA_RAMP   = 0x00002000,

     CLRCF_SRCKEY       = 0x00010000,
     CLRCF_DSTKEY       = 0x00020000,

     CLRCF_PARITY       = 0x00100000,

     CLRCF_SURFACE      = 0x10000000,
     CLRCF_PALETTE      = 0x20000000,
     CLRCF_FREEZE       = 0x40000000,

     CLRCF_ALL          = 0x701337FF
} CoreLayerRegionConfigFlags;

typedef enum {
     CLRNF_NONE = 0x00000000
} CoreLayerRegionNotificationFlags;

typedef struct {
     CoreLayerRegionNotificationFlags  flags;
     CoreLayerRegion                  *region;
} CoreLayerRegionNotification;

/**********************************************************************************************************************/

/*
 * Creates a pool of layer region objects.
 */
FusionObjectPool *dfb_layer_region_pool_create       ( const FusionWorld           *world );

/*
 * Generates dfb_layer_region_ref(), dfb_layer_region_attach() etc.
 */
FUSION_OBJECT_METHODS( CoreLayerRegion, dfb_layer_region )

/**********************************************************************************************************************/

DFBResult         dfb_layer_region_create            ( CoreLayerContext             *context,
                                                       CoreLayerRegion             **ret_region );

DirectResult      dfb_layer_region_lock              ( CoreLayerRegion              *region );

DirectResult      dfb_layer_region_unlock            ( CoreLayerRegion              *region );

DFBResult         dfb_layer_region_activate          ( CoreLayerRegion              *region );

DFBResult         dfb_layer_region_deactivate        ( CoreLayerRegion              *region );

DFBResult         dfb_layer_region_enable            ( CoreLayerRegion              *region );

DFBResult         dfb_layer_region_disable           ( CoreLayerRegion              *region );

DFBResult         dfb_layer_region_set_surface       ( CoreLayerRegion              *region,
                                                       CoreSurface                  *surface,
                                                       bool                          update );

DFBResult         dfb_layer_region_get_surface       ( CoreLayerRegion              *region,
                                                       CoreSurface                 **ret_surface );

DFBResult         dfb_layer_region_flip_update       ( CoreLayerRegion              *region,
                                                       const DFBRegion              *update,
                                                       DFBSurfaceFlipFlags           flags );

DFBResult         dfb_layer_region_flip_update_stereo( CoreLayerRegion              *region,
                                                       const DFBRegion              *left_update,
                                                       const DFBRegion              *right_update,
                                                       DFBSurfaceFlipFlags           flags );

DFBResult         dfb_layer_region_flip_update2      ( CoreLayerRegion              *region,
                                                       const DFBRegion              *left_update,
                                                       const DFBRegion              *right_update,
                                                       DFBSurfaceFlipFlags           flags,
                                                       unsigned int                  flip_count,
                                                       long long                     pts );

/*
 * Configuration setting/getting.
 */

DFBResult         dfb_layer_region_set_configuration ( CoreLayerRegion              *region,
                                                       const CoreLayerRegionConfig  *config,
                                                       CoreLayerRegionConfigFlags    flags );

DFBResult         dfb_layer_region_get_configuration ( CoreLayerRegion              *region,
                                                       CoreLayerRegionConfig        *ret_config );

/*
 * Global reaction, listen to the layer's surface.
 */
ReactionResult   _dfb_layer_region_surface_listener  ( const void                   *msg_data,
                                                       void                         *ctx );

#endif

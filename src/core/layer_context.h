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

#ifndef __CORE__LAYER_CONTEXT_H__
#define __CORE__LAYER_CONTEXT_H__

#include <core/layer_region.h>

/**********************************************************************************************************************/

typedef enum {
     CLLM_LOCATION  = 0x00000000, /* Keep normalized area. */
     CLLM_CENTER    = 0x00000001, /* Center layer after resizing destination area. */
     CLLM_POSITION  = 0x00000002, /* Keep pixel position, but resize area. */
     CLLM_RECTANGLE = 0x00000003  /* Keep pixel based area. */
} CoreLayerLayoutMode;

struct __DFB_CoreLayerContext {
     FusionObject                object;

     int                         magic;

     DFBDisplayLayerID           layer_id;     /* layer id */

     FusionSkirmish              lock;         /* Lock for layer context handling. */

     bool                        active;       /* Active context. */

     DFBDisplayLayerConfig       config;       /* Current layer configuration. */
     int                         rotation;     /* Rotation. */

     FusionVector                regions;      /* All regions created within this context. */

     struct {
          CoreLayerRegion       *region;       /* Region of layer config if buffer mode is not DLBM_WINDOWS. */
          CoreLayerRegionConfig  config;       /* Region config used to implement layer config and settings. */
     } primary;

     struct {
          DFBLocation            location;     /* Normalized screen location. */
          DFBRectangle           rectangle;    /* Pixel based position and size. */

          CoreLayerLayoutMode    mode;         /* Resizing influences them. */
     } screen;

     DFBColorAdjustment          adjustment;   /* Color adjustment of the layer.*/

     bool                        follow_video; /* Stereo ofset is determined by video metadata. */
     int                         z;            /* Stereo offset to use when the layer is mixed. */

     CoreWindowStack            *stack;        /* Every layer has its own window stack as every layer has its own
                                                  pixel buffer. */

     FusionSHMPoolShared        *shmpool;      /* Shared memory pool. */

     FusionCall                  call;         /* dispatch */

     struct {
          int                    hot_x;        /* x position of cursor hot spot */
          int                    hot_y;        /* y position of cursor hot spot */
          CoreSurface           *surface;      /* cursor shape surface */
     } cursor;
};

/**********************************************************************************************************************/

typedef enum {
     CLCNF_ACTIVATED   = 0x00000001,
     CLCNF_DEACTIVATED = 0x00000002
} CoreLayerContextNotificationFlags;

typedef struct {
     CoreLayerContextNotificationFlags  flags;
     CoreLayerContext                  *context;
} CoreLayerContextNotification;

/**********************************************************************************************************************/

/*
 * Creates a pool of layer context objects.
 */
FusionObjectPool *dfb_layer_context_pool_create        ( const FusionWorld            *world );

/*
 * Generates dfb_layer_context_ref(), dfb_layer_context_attach() etc.
 */
FUSION_OBJECT_METHODS( CoreLayerContext, dfb_layer_context )

/**********************************************************************************************************************/

DFBResult         dfb_layer_context_init               ( CoreLayerContext             *context,
                                                         CoreLayer                    *layer,
                                                         bool                          stack );

DirectResult      dfb_layer_context_lock               ( CoreLayerContext             *context );

DirectResult      dfb_layer_context_unlock             ( CoreLayerContext             *context );

bool              dfb_layer_context_active             ( const CoreLayerContext       *context );

DFBResult         dfb_layer_context_activate           ( CoreLayerContext             *context );

DFBResult         dfb_layer_context_deactivate         ( CoreLayerContext             *context );

DFBResult         dfb_layer_context_add_region         ( CoreLayerContext             *context,
                                                         CoreLayerRegion              *region );

DFBResult         dfb_layer_context_remove_region      ( CoreLayerContext             *context,
                                                         CoreLayerRegion              *region );

DFBResult         dfb_layer_context_get_primary_region ( CoreLayerContext             *context,
                                                         bool                          create,
                                                         CoreLayerRegion             **ret_region );

/*
 * Configuration testing/setting/getting.
 */

DFBResult         dfb_layer_context_test_configuration ( CoreLayerContext             *context,
                                                         const DFBDisplayLayerConfig  *config,
                                                         DFBDisplayLayerConfigFlags   *ret_failed );

DFBResult         dfb_layer_context_set_configuration  ( CoreLayerContext             *context,
                                                         const DFBDisplayLayerConfig  *config );

DFBResult         dfb_layer_context_get_configuration  ( CoreLayerContext             *context,
                                                         DFBDisplayLayerConfig        *ret_config );

/*
 * Configuration details.
 */

DFBResult         dfb_layer_context_set_src_colorkey   ( CoreLayerContext             *context,
                                                         u8                            r,
                                                         u8                            g,
                                                         u8                            b,
                                                         int                           index );

DFBResult         dfb_layer_context_set_dst_colorkey   ( CoreLayerContext             *context,
                                                         u8                            r,
                                                         u8                            g,
                                                         u8                            b,
                                                         int                           index );

DFBResult         dfb_layer_context_set_sourcerectangle( CoreLayerContext             *context,
                                                         const DFBRectangle           *source );

DFBResult         dfb_layer_context_set_screenlocation ( CoreLayerContext             *context,
                                                         const DFBLocation            *location );

DFBResult         dfb_layer_context_set_screenrectangle( CoreLayerContext             *context,
                                                         const DFBRectangle           *rect );

DFBResult         dfb_layer_context_set_screenposition ( CoreLayerContext             *context,
                                                         int                           x,
                                                         int                           y );

DFBResult         dfb_layer_context_set_opacity        ( CoreLayerContext             *context,
                                                         u8                            opacity );

DFBResult         dfb_layer_context_set_rotation       ( CoreLayerContext             *context,
                                                         int                           rotation );

DFBResult         dfb_layer_context_set_coloradjustment( CoreLayerContext             *context,
                                                         const DFBColorAdjustment     *adjustment );

DFBResult         dfb_layer_context_get_coloradjustment( CoreLayerContext             *context,
                                                         DFBColorAdjustment           *ret_adjustment );

DFBResult         dfb_layer_context_set_stereo_depth   ( CoreLayerContext             *context,
                                                         bool                          follow_video,
                                                         int                           z );

DFBResult         dfb_layer_context_get_stereo_depth   ( CoreLayerContext             *context,
                                                         bool                         *ret_follow_video,
                                                         int                          *ret_z );

DFBResult         dfb_layer_context_set_field_parity   ( CoreLayerContext             *context,
                                                         int                           field );

DFBResult         dfb_layer_context_set_clip_regions   ( CoreLayerContext             *context,
                                                         const DFBRegion              *regions,
                                                         int                           num_regions,
                                                         DFBBoolean                    positive );

DFBResult         dfb_layer_context_set_cursor_shape   ( CoreLayerContext             *context,
                                                         CoreSurface                  *shape,
                                                         int                           hot_x,
                                                         int                           hot_y );

DFBResult         dfb_layer_context_get_cursor_shape   ( CoreLayerContext             *context,
                                                         CoreSurface                 **ret_shape,
                                                         int                          *ret_hot_x,
                                                         int                          *ret_hot_y );

/*
 * Window control.
 */

DFBResult         dfb_layer_context_create_window      ( CoreDFB                      *core,
                                                         CoreLayerContext             *context,
                                                         const DFBWindowDescription   *desc,
                                                         CoreWindow                  **ret_window );

CoreWindow       *dfb_layer_context_find_window        ( CoreLayerContext             *context,
                                                         DFBWindowID                   id );

CoreWindowStack  *dfb_layer_context_windowstack        ( const CoreLayerContext       *context );

/*
 * Region surface (re/de)allocation.
 */

DFBResult         dfb_layer_context_allocate_surface   ( CoreLayer                    *layer,
                                                         CoreLayerContext             *context,
                                                         CoreLayerRegion              *region,
                                                         CoreLayerRegionConfig        *config );

DFBResult         dfb_layer_context_reallocate_surface ( CoreLayer                    *layer,
                                                         CoreLayerContext             *context,
                                                         CoreLayerRegion              *region,
                                                         CoreLayerRegionConfig        *config );

DFBResult         dfb_layer_context_deallocate_surface ( CoreLayer                    *layer,
                                                         CoreLayerContext             *context,
                                                         CoreLayerRegion              *region );

#endif

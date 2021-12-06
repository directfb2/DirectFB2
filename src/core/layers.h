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

#ifndef __CORE__LAYERS_H__
#define __CORE__LAYERS_H__

#include <core/state.h>
#include <core/layer_region.h>

/**********************************************************************************************************************/

typedef struct {
     /*
      * Return size of layer data (shared memory).
      */
     int       (*LayerDataSize)        ( void );

     /*
      * Return size of region data (shared memory).
      */
     int       (*RegionDataSize)       ( void );

     /*
      * Called once by the master to initialize layer data and reset hardware.
      * Return layer description, default configuration and color adjustment.
      */
     DFBResult (*InitLayer)            ( CoreLayer                         *layer,
                                         void                              *driver_data,
                                         void                              *layer_data,
                                         DFBDisplayLayerDescription        *description,
                                         DFBDisplayLayerConfig             *config,
                                         DFBColorAdjustment                *adjustment );

     /*
      * Called once by the master to shutdown the layer.
      * Use this function to free any resources that were taken during init.
      */
     DFBResult (*ShutdownLayer)        ( CoreLayer                         *layer,
                                         void                              *driver_data,
                                         void                              *layer_data );

     /*
      * Called once by the master for each source.
      * Driver fills description.
      */
     DFBResult (*InitSource)           ( CoreLayer                         *layer,
                                         void                              *driver_data,
                                         void                              *layer_data,
                                         int                                source,
                                         DFBDisplayLayerSourceDescription  *description );

     /*
      * Return the currently displayed field (interlaced only).
      */
     DFBResult (*GetCurrentOutputField)( CoreLayer                         *layer,
                                         void                              *driver_data,
                                         void                              *layer_data,
                                         int                               *field );

     /*
      * Return the z position of the layer.
      */
     DFBResult (*GetLevel)             ( CoreLayer                         *layer,
                                         void                              *driver_data,
                                         void                              *layer_data,
                                         int                               *level );

     /*
      * Move the layer below or on top of others (z position).
      */
     DFBResult (*SetLevel)             ( CoreLayer                         *layer,
                                         void                              *driver_data,
                                         void                              *layer_data,
                                         int                                level );

     /*
      * Adjust brightness, contrast, saturation etc.
      */
     DFBResult (*SetColorAdjustment)   ( CoreLayer                         *layer,
                                         void                              *driver_data,
                                         void                              *layer_data,
                                         DFBColorAdjustment                *adjustment );

     /*
      * Set the stereo depth for L/R mono and stereo layers.
      */
     DFBResult (*SetStereoDepth)       ( CoreLayer                         *layer,
                                         void                              *driver_data,
                                         void                              *layer_data,
                                         bool                               follow_video,
                                         int                                z );

     /*
      * Check all parameters and return if this region is supported.
      */
     DFBResult (*TestRegion)           ( CoreLayer                         *layer,
                                         void                              *driver_data,
                                         void                              *layer_data,
                                         CoreLayerRegionConfig             *config,
                                         CoreLayerRegionConfigFlags        *ret_failed );

     /*
      * Add a new region to the layer, but don't program hardware, yet.
      */
     DFBResult (*AddRegion)            ( CoreLayer                         *layer,
                                         void                              *driver_data,
                                         void                              *layer_data,
                                         void                              *region_data,
                                         CoreLayerRegionConfig             *config );

     /*
      * Setup hardware, called once after AddRegion() or when parameters have changed.
      * Surface and palette are only set if updated or new.
      */
     DFBResult (*SetRegion)            ( CoreLayer                         *layer,
                                         void                              *driver_data,
                                         void                              *layer_data,
                                         void                              *region_data,
                                         CoreLayerRegionConfig             *config,
                                         CoreLayerRegionConfigFlags         updated,
                                         CoreSurface                       *surface,
                                         CorePalette                       *palette,
                                         CoreSurfaceBufferLock             *left_lock,
                                         CoreSurfaceBufferLock             *right_lock );

     /*
      * Remove a region from the layer.
      */
     DFBResult (*RemoveRegion)         ( CoreLayer                         *layer,
                                         void                              *driver_data,
                                         void                              *layer_data,
                                         void                              *region_data );

     /*
      * Flip the surface of the region.
      */
     DFBResult (*FlipRegion)           ( CoreLayer                         *layer,
                                         void                              *driver_data,
                                         void                              *layer_data,
                                         void                              *region_data,
                                         CoreSurface                       *surface,
                                         DFBSurfaceFlipFlags                flags,
                                         const DFBRegion                   *left_update,
                                         CoreSurfaceBufferLock             *left_lock,
                                         const DFBRegion                   *right_update,
                                         CoreSurfaceBufferLock             *right_lock );

     /*
      * Indicate updates to the front buffer content.
      */
     DFBResult (*UpdateRegion)         ( CoreLayer                         *layer,
                                         void                              *driver_data,
                                         void                              *layer_data,
                                         void                              *region_data,
                                         CoreSurface                       *surface,
                                         const DFBRegion                   *left_update,
                                         CoreSurfaceBufferLock             *left_lock,
                                         const DFBRegion                   *right_update,
                                         CoreSurfaceBufferLock             *right_lock );

     /*
      * Control hardware deinterlacing.
      */
     DFBResult (*SetInputField)        ( CoreLayer                         *layer,
                                         void                              *driver_data,
                                         void                              *layer_data,
                                         void                              *region_data,
                                         int                                field );

     /*
      * Allocate the surface of the region.
      */
     DFBResult (*AllocateSurface)      ( CoreLayer                         *layer,
                                         void                              *driver_data,
                                         void                              *layer_data,
                                         void                              *region_data,
                                         CoreLayerRegionConfig             *config,
                                         CoreSurface                      **ret_surface );

     /*
      * Reallocate the surface of the region.
      */
     DFBResult (*ReallocateSurface)    ( CoreLayer                         *layer,
                                         void                              *driver_data,
                                         void                              *layer_data,
                                         void                              *region_data,
                                         CoreLayerRegionConfig             *config,
                                         CoreSurface                       *surface );

     /*
      * Deallocate the surface of the region.
      */
     DFBResult (*DeallocateSurface)    ( CoreLayer                         *layer,
                                         void                              *driver_data,
                                         void                              *layer_data,
                                         void                              *region_data,
                                         CoreSurface                       *surface );
} DisplayLayerFuncs;

typedef struct {
     int                              index;
     DFBDisplayLayerSourceDescription description;
} CoreLayerSource;

typedef struct {
     FusionVector      stack;
     int               active;

     CoreLayerContext *primary;
} CoreLayerContexts;

typedef struct {
     DFBDisplayLayerID           layer_id;

     DFBDisplayLayerDescription  description;
     DFBDisplayLayerConfig       default_config;
     DFBColorAdjustment          default_adjustment;

     CoreLayerSource            *sources;

     FusionSHMPoolShared        *shmpool;

     void                       *layer_data;         /* local data (impl) */

     FusionSkirmish              lock;

     CoreLayerContexts           contexts;

     bool                        suspended;

     FusionVector                added_regions;

     FusionCall                  call;               /* dispatch */

     DFBSurfacePixelFormat       pixelformat;
} CoreLayerShared;

struct __DFB_CoreLayer {
     CoreLayerShared         *shared;

     CoreDFB                 *core;

     CoreScreen              *screen;

     void                    *driver_data;
     void                    *layer_data;  /* copy of shared->layer_data */

     CardState                state;

     const DisplayLayerFuncs *funcs;
};

/**********************************************************************************************************************/

typedef DFBEnumerationResult (*DisplayLayerCallback)( CoreLayer *layer, void *ctx );

/**********************************************************************************************************************/

/*
 * Add a layer to a graphics device by pointing to a table containing driver functions.
 * The supplied 'driver_data' will be passed to these functions.
 */
CoreLayer             *dfb_layers_register          ( CoreScreen                  *screen,
                                                      void                        *driver_data,
                                                      const DisplayLayerFuncs     *funcs );

/*
 * Replace functions of the primary layer implementation by passing an alternative driver function table.
 * All non-NULL functions in the new table replace the functions in the original function table.
 * The original function table is written to 'primary_funcs' before to allow drivers to use existing functionality
 * from the original implementation.
 */
CoreLayer             *dfb_layers_hook_primary      ( void                        *driver_data,
                                                      DisplayLayerFuncs           *funcs,
                                                      DisplayLayerFuncs           *primary_funcs,
                                                      void                       **primary_driver_data );

/*
 * Get the description of the specified layer.
 */
void                   dfb_layer_get_description    ( const CoreLayer             *layer,
                                                      DFBDisplayLayerDescription  *ret_desc );

/*
 * Return the pixel format of the primary layer.
 */
DFBSurfacePixelFormat  dfb_primary_layer_pixelformat( void );

/*
 * Enumerate all registered layers by invoking the callback for each layer.
 */
void                   dfb_layers_enumerate         ( DisplayLayerCallback         callback,
                                                      void                        *ctx );
/*
 * Return the number of layers.
 */
int                    dfb_layers_num               ( void );

/*
 * Return the layer with the specified ID.
 */
CoreLayer             *dfb_layer_at                 ( DFBDisplayLayerID            id );

/*
 * Return the (translated) layer with the specified ID.
 */
CoreLayer             *dfb_layer_at_translated      ( DFBDisplayLayerID            id );

/*
 * Return the ID of the specified layer.
 */
DFBDisplayLayerID      dfb_layer_id                 ( const CoreLayer             *layer );

/*
 * Return the (translated) ID of the specified layer.
 */
DFBDisplayLayerID      dfb_layer_id_translated      ( const CoreLayer             *layer );

#endif

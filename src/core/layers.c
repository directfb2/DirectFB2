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

#include <core/CoreLayer.h>
#include <core/core.h>
#include <core/core_parts.h>
#include <core/layers.h>
#include <core/layer_control.h>
#include <direct/memcpy.h>
#include <fusion/conf.h>
#include <fusion/shmalloc.h>

D_DEBUG_DOMAIN( Core_Layers, "Core/Layers", "DirectFB Core Display Layers" );

/**********************************************************************************************************************/

typedef struct {
     int              magic;

     int              num;
     CoreLayerShared *layers[MAX_LAYERS];
} DFBLayerCoreShared;

typedef struct {
     int                 magic;

     CoreDFB            *core;

     DFBLayerCoreShared *shared;
} DFBLayerCore;

DFB_CORE_PART( layer_core, LayerCore );

/**********************************************************************************************************************/

static int        num_layers = 0;
static CoreLayer *layers[MAX_LAYERS];

static DFBResult
dfb_layer_core_initialize( CoreDFB            *core,
                           DFBLayerCore       *data,
                           DFBLayerCoreShared *shared )
{
     DFBResult            ret;
     FusionSHMPoolShared *pool;
     int                  i;

     D_DEBUG_AT( Core_Layers, "%s( %p, %p, %p )\n", __FUNCTION__, core, data, shared );

     D_ASSERT( data != NULL );
     D_ASSERT( shared != NULL );

     data->core   = core;
     data->shared = shared;

     pool = dfb_core_shmpool( core );

     /* Initialize all registered layers. */
     for (i = 0; i < num_layers; i++) {
          CoreLayer               *layer = layers[i];
          CoreLayerShared         *lshared;
          const DisplayLayerFuncs *funcs = layer->funcs;
          char                     buf[24];

          /* Allocate shared data. */
          lshared = SHCALLOC( pool, 1, sizeof(CoreLayerShared) );

          /* Assign ID (zero based index). */
          lshared->layer_id = i;
          lshared->shmpool  = pool;

          snprintf( buf, sizeof(buf), "Display Layer %d", i );

          /* Initialize the lock. */
          ret = fusion_skirmish_init2( &lshared->lock, buf, dfb_core_world( core ), fusion_config->secure_fusion );
          if (ret) {
               SHFREE( pool, lshared );
               return DFB_FUSION;
          }

          /* Allocate driver's layer data. */
          if (funcs->LayerDataSize) {
               int size = funcs->LayerDataSize();

               if (size > 0) {
                    lshared->layer_data = SHCALLOC( pool, 1, size );
                    if (!lshared->layer_data) {
                         fusion_skirmish_destroy( &lshared->lock );
                         SHFREE( pool, lshared );
                         return D_OOSHM();
                    }
               }
          }

          /* Initialize the layer, get the layer description, the default configuration and default color adjustment. */
          ret = funcs->InitLayer( layer, layer->driver_data, lshared->layer_data, &lshared->description,
                                  &lshared->default_config, &lshared->default_adjustment );
          if (ret) {
               D_DERROR( ret, "Core/Layers: Failed to initialize layer %u!\n", lshared->layer_id );

               fusion_skirmish_destroy( &lshared->lock );

               if (lshared->layer_data)
                    SHFREE( pool, lshared->layer_data );

               SHFREE( pool, lshared );

               return ret;
          }

          if (lshared->description.caps & DLCAPS_SOURCES) {
               int n;

               lshared->sources = SHCALLOC( pool, lshared->description.sources, sizeof(CoreLayerSource) );

               for (n = 0; n < lshared->description.sources; n++) {
                    CoreLayerSource *source = &lshared->sources[n];

                    source->index = n;

                    funcs->InitSource( layer, layer->driver_data, lshared->layer_data, n, &source->description );
               }
          }

          if (D_FLAGS_IS_SET( lshared->description.caps, DLCAPS_SCREEN_LOCATION ))
               D_FLAGS_SET( lshared->description.caps, DLCAPS_SCREEN_POSITION | DLCAPS_SCREEN_SIZE );

          if (D_FLAGS_ARE_SET( lshared->description.caps, DLCAPS_SCREEN_POSITION | DLCAPS_SCREEN_SIZE ))
               D_FLAGS_SET( lshared->description.caps, DLCAPS_SCREEN_LOCATION );

          /* Initialize the vector for the contexts. */
          fusion_vector_init( &lshared->contexts.stack, 4, pool );

          /* Initialize the vector for realized (added) regions. */
          fusion_vector_init( &lshared->added_regions, 4, pool );

          /* No active context by default. */
          lshared->contexts.active = -1;

          /* Store layer data. */
          layer->layer_data = lshared->layer_data;

          /* Store pointer to shared data and core. */
          layer->shared = lshared;
          layer->core   = core;

          CoreLayer_Init_Dispatch( core, layer, &lshared->call );

          fusion_call_add_permissions( &lshared->call, 0, FUSION_CALL_PERMIT_EXECUTE );

          /* Add the layer to the shared list. */
          shared->layers[shared->num++] = lshared;
     }

     D_MAGIC_SET( data, DFBLayerCore );
     D_MAGIC_SET( shared, DFBLayerCoreShared );

     return DFB_OK;
}

static DFBResult
dfb_layer_core_join( CoreDFB            *core,
                     DFBLayerCore       *data,
                     DFBLayerCoreShared *shared )
{
     int i;

     D_DEBUG_AT( Core_Layers, "%s( %p, %p, %p )\n", __FUNCTION__, core, data, shared );

     D_ASSERT( data != NULL );
     D_MAGIC_ASSERT( shared, DFBLayerCoreShared );

     data->core   = core;
     data->shared = shared;

     if (num_layers != shared->num) {
          D_ERROR( "Core/Layers: Number of layers does not match!\n" );
          return DFB_BUG;
     }

     for (i = 0; i < num_layers; i++) {
          CoreLayer       *layer   = layers[i];
          CoreLayerShared *lshared = shared->layers[i];

          /* Make a copy for faster access. */
          layer->layer_data = lshared->layer_data;

          /* Store pointer to shared data and core. */
          layer->shared = lshared;
          layer->core   = core;
     }

     D_MAGIC_SET( data, DFBLayerCore );

     return DFB_OK;
}

static DFBResult
dfb_layer_core_shutdown( DFBLayerCore *data,
                         bool          emergency )
{
     DFBResult           ret;
     DFBLayerCoreShared *shared;
     int                 i;

     D_UNUSED_P( shared );

     D_DEBUG_AT( Core_Layers, "%s( %p, %semergency )\n", __FUNCTION__, data, emergency ? "" : "no " );

     D_MAGIC_ASSERT( data, DFBLayerCore );
     D_MAGIC_ASSERT( data->shared, DFBLayerCoreShared );

     shared = data->shared;

     /* Begin with the most recently added layer. */
     for (i = num_layers - 1; i >= 0; i--) {
          CoreLayer               *layer   = layers[i];
          CoreLayerShared         *lshared = layer->shared;
          const DisplayLayerFuncs *funcs   = layer->funcs;

          D_ASSUME( emergency || fusion_vector_is_empty( &lshared->added_regions ) );

          /* Remove all regions during emergency shutdown. */
          if (emergency && funcs->RemoveRegion) {
               int              n;
               CoreLayerRegion *region;

               fusion_vector_foreach (region, n, lshared->added_regions) {
                   D_DEBUG_AT( Core_Layers, "  -> removing region (%4d,%4d-%4dx%4d) from '%s'\n",
                               DFB_RECTANGLE_VALS( &region->config.dest ), lshared->description.name );

                   ret = funcs->RemoveRegion( layer, layer->driver_data, layer->layer_data, region->region_data );
                   if (ret)
                        D_DERROR( ret, "Core/Layers: Could not remove region!\n" );
               }
          }

          /* Shut the layer down. */
          if (funcs->ShutdownLayer) {
               ret = funcs->ShutdownLayer( layer, layer->driver_data, lshared->layer_data );
               if (ret)
                    D_DERROR( ret, "Core/Layers: Failed to shutdown layer %u!\n", lshared->layer_id );
          }

          CoreLayer_Deinit_Dispatch( &lshared->call );

          /* Deinitialize the lock. */
          fusion_skirmish_destroy( &lshared->lock );

          /* Deinitialize the state for window stack repaints. */
          dfb_state_destroy( &layer->state );

          /* Deinitialize the vector for the contexts. */
          fusion_vector_destroy( &lshared->contexts.stack );

          /* Deinitialize the vector for the realized (added) regions. */
          fusion_vector_destroy( &lshared->added_regions );

          /* Free the driver's layer data. */
          if (lshared->layer_data)
               SHFREE( lshared->shmpool, lshared->layer_data );

          /* Free the shared layer data. */
          SHFREE( lshared->shmpool, lshared );

          /* Free the local layer data. */
          D_FREE( layer );
     }

     num_layers = 0;

     D_MAGIC_CLEAR( data );
     D_MAGIC_CLEAR( shared );

     return DFB_OK;
}

static DFBResult
dfb_layer_core_leave( DFBLayerCore *data,
                      bool          emergency )
{
     int i;

     D_DEBUG_AT( Core_Layers, "%s( %p, %semergency )\n", __FUNCTION__, data, emergency ? "" : "no " );

     D_MAGIC_ASSERT( data, DFBLayerCore );
     D_MAGIC_ASSERT( data->shared, DFBLayerCoreShared );

     /* Deinitialize all local stuff. */
     for (i = 0; i < num_layers; i++) {
          CoreLayer *layer = layers[i];

          /* Deinitialize the state for window stack repaints. */
          dfb_state_destroy( &layer->state );

          /* Free local layer data. */
          D_FREE( layer );
     }

     num_layers = 0;

     D_MAGIC_CLEAR( data );

     return DFB_OK;
}

static DFBResult
dfb_layer_core_suspend( DFBLayerCore *data )
{
     int i;

     D_DEBUG_AT( Core_Layers, "%s( %p )\n", __FUNCTION__, data );

     D_MAGIC_ASSERT( data, DFBLayerCore );
     D_MAGIC_ASSERT( data->shared, DFBLayerCoreShared );

     for (i = num_layers - 1; i >= 0; i--)
          dfb_layer_suspend( layers[i] );

     return DFB_OK;
}

static DFBResult
dfb_layer_core_resume( DFBLayerCore *data )
{
     int i;

     D_DEBUG_AT( Core_Layers, "%s( %p )\n", __FUNCTION__, data );

     D_MAGIC_ASSERT( data, DFBLayerCore );
     D_MAGIC_ASSERT( data->shared, DFBLayerCoreShared );

     for (i = 0; i < num_layers; i++)
          dfb_layer_resume( layers[i] );

     return DFB_OK;
}

/**********************************************************************************************************************/

CoreLayer *
dfb_layers_register( CoreScreen              *screen,
                     void                    *driver_data,
                     const DisplayLayerFuncs *funcs )
{
     CoreLayer *layer;

     D_ASSERT( screen != NULL );
     D_ASSERT( funcs != NULL );

     if (num_layers == MAX_LAYERS) {
          D_ERROR( "Core/Layers: Maximum number of layers reached!\n" );
          return NULL;
     }

     /* Allocate local data. */
     layer = D_CALLOC( 1, sizeof(CoreLayer) );

     /* Assign local pointers. */
     layer->screen      = screen;
     layer->driver_data = driver_data;
     layer->funcs       = funcs;

     /* Initialize the state for window stack repaints. */
     dfb_state_init( &layer->state, NULL );

     /* Add it to the local list. */
     layers[num_layers++] = layer;

     return layer;
}

typedef void (*AnyFunc)( void );

CoreLayer *
dfb_layers_hook_primary( void               *driver_data,
                         DisplayLayerFuncs  *funcs,
                         DisplayLayerFuncs  *primary_funcs,
                         void              **primary_driver_data )
{
     int        i;
     int        entries;
     CoreLayer *primary = layers[0];

     D_ASSERT( primary != NULL );
     D_ASSERT( funcs != NULL );

     /* Copy content of original function table. */
     if (primary_funcs)
          direct_memcpy( primary_funcs, primary->funcs, sizeof(DisplayLayerFuncs) );

     /* Copy pointer to original driver data. */
     if (primary_driver_data)
          *primary_driver_data = primary->driver_data;

     /* Replace all entries in the old table that aren't NULL in the new one. */
     entries = sizeof(DisplayLayerFuncs) / sizeof(void (*)( void ));
     for (i = 0; i < entries; i++) {
          AnyFunc *newfuncs = (AnyFunc*) funcs;
          AnyFunc *oldfuncs = (AnyFunc*) primary->funcs;

          if (newfuncs[i])
               oldfuncs[i] = newfuncs[i];
     }

     /* Replace driver data pointer. */
     primary->driver_data = driver_data;

     return primary;
}

void
dfb_layer_get_description( const CoreLayer            *layer,
                           DFBDisplayLayerDescription *ret_desc )
{
     CoreLayerShared *shared;

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->shared != NULL );
     D_ASSERT( ret_desc != NULL );

     shared = layer->shared;

     *ret_desc = shared->description;
}

DFBSurfacePixelFormat
dfb_primary_layer_pixelformat()
{
     CoreLayerShared *shared;
     CoreLayer       *layer = dfb_layer_at_translated( DLID_PRIMARY );

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->shared != NULL );

     shared = layer->shared;

     return shared->pixelformat;
}

void
dfb_layers_enumerate( DisplayLayerCallback  callback,
                      void                 *ctx )
{
     int i;

     D_ASSERT( callback != NULL );

     for (i = 0; i < num_layers; i++) {
          if (callback( layers[i], ctx ) == DFENUM_CANCEL)
               break;
     }
}

int
dfb_layers_num()
{
     return num_layers;
}

CoreLayer *
dfb_layer_at( DFBDisplayLayerID id )
{
     D_ASSERT( id >= 0 );
     D_ASSERT( id < num_layers );

     return layers[id];
}

CoreLayer *
dfb_layer_at_translated( DFBDisplayLayerID id )
{
     D_ASSERT( id >= 0 );
     D_ASSERT( id < num_layers );
     D_ASSERT( dfb_config != NULL );

     if (dfb_config->primary_layer > 0 && dfb_config->primary_layer < num_layers) {
          if (id == DLID_PRIMARY)
               return dfb_layer_at( dfb_config->primary_layer );

          if (id == dfb_config->primary_layer)
               return dfb_layer_at( DLID_PRIMARY );
     }

     return dfb_layer_at( id );
}

DFBDisplayLayerID
dfb_layer_id( const CoreLayer *layer )
{
     CoreLayerShared *shared;

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->shared != NULL );

     shared = layer->shared;

     return shared->layer_id;
}

DFBDisplayLayerID
dfb_layer_id_translated( const CoreLayer *layer )
{
     CoreLayerShared *shared;

     D_ASSERT( layer != NULL );
     D_ASSERT( layer->shared != NULL );
     D_ASSERT( dfb_config != NULL );

     shared = layer->shared;

     if (dfb_config->primary_layer > 0 && dfb_config->primary_layer < num_layers) {
          if (shared->layer_id == DLID_PRIMARY)
               return dfb_config->primary_layer;

          if (shared->layer_id == dfb_config->primary_layer)
               return DLID_PRIMARY;
     }

     return shared->layer_id;
}

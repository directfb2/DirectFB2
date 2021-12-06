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
#include <core/CoreLayerContext.h>
#include <core/CoreLayerRegion.h>
#include <core/CoreSurface.h>
#include <core/CoreWindowStack.h>
#include <core/layers.h>
#include <core/layer_context.h>
#include <core/layer_control.h>
#include <display/idirectfbdisplaylayer.h>
#include <display/idirectfbscreen.h>
#include <display/idirectfbsurface.h>
#include <display/idirectfbsurface_layer.h>
#include <windows/idirectfbwindow.h>

D_DEBUG_DOMAIN( Layer, "IDirectFBDisplayLayer", "IDirectFBDisplayLayer Interface" );

/**********************************************************************************************************************/

/*
 * private data struct of IDirectFBDisplayLayer
 */
typedef struct {
     int                              ref;              /* reference counter */

     DFBDisplayLayerDescription       desc;             /* description of the layer's caps */

     DFBDisplayLayerCooperativeLevel  level;            /* current cooperative level */

     CoreScreen                      *screen;           /* layer's screen */
     CoreLayer                       *layer;            /* the layer object */
     CoreLayerContext                *context;          /* shared or exclusive context */
     CoreLayerRegion                 *region;           /* primary region of the context */
     CoreWindowStack                 *stack;            /* stack of shared context */

     DFBBoolean                       switch_exclusive; /* switch to exclusive context after creation */

     CoreDFB                         *core;
     IDirectFB                       *idirectfb;
} IDirectFBDisplayLayer_data;

/**********************************************************************************************************************/

static void
IDirectFBDisplayLayer_Destruct( IDirectFBDisplayLayer *thiz )
{
     IDirectFBDisplayLayer_data *data = thiz->priv;

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     D_DEBUG_AT( Layer, "  -> unref region...\n" );

     dfb_layer_region_unref( data->region );

     D_DEBUG_AT( Layer, "  -> unref context...\n" );

     dfb_layer_context_unref( data->context );

     DIRECT_DEALLOCATE_INTERFACE( thiz );

     D_DEBUG_AT( Layer, "  -> done\n" );
}

static DirectResult
IDirectFBDisplayLayer_AddRef( IDirectFBDisplayLayer *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     data->ref++;

     return DFB_OK;
}

static DirectResult
IDirectFBDisplayLayer_Release( IDirectFBDisplayLayer *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (--data->ref == 0)
          IDirectFBDisplayLayer_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBDisplayLayer_GetID( IDirectFBDisplayLayer *thiz,
                             DFBDisplayLayerID     *ret_layer_id )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_layer_id)
          return DFB_INVARG;

     *ret_layer_id = dfb_layer_id_translated( data->layer );

     return DFB_OK;
}

static DFBResult
IDirectFBDisplayLayer_GetDescription( IDirectFBDisplayLayer      *thiz,
                                      DFBDisplayLayerDescription *ret_desc )
{

     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_desc)
          return DFB_INVARG;

     *ret_desc = data->desc;

     return DFB_OK;
}

static DFBResult
IDirectFBDisplayLayer_GetSourceDescriptions( IDirectFBDisplayLayer            *thiz,
                                             DFBDisplayLayerSourceDescription *ret_descriptions )
{
     int i;

     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_descriptions)
          return DFB_INVARG;

     if (!D_FLAGS_IS_SET( data->desc.caps, DLCAPS_SOURCES ))
          return DFB_UNSUPPORTED;

     for (i = 0; i < data->desc.sources; i++)
          dfb_layer_get_source_info( data->layer, i, &ret_descriptions[i] );

     return DFB_OK;
}

static DFBResult
IDirectFBDisplayLayer_GetCurrentOutputField( IDirectFBDisplayLayer *thiz,
                                             int                   *ret_field )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     return CoreLayer_GetCurrentOutputField( data->layer, ret_field );
}

static DFBResult
IDirectFBDisplayLayer_GetSurface( IDirectFBDisplayLayer  *thiz,
                                  IDirectFBSurface      **ret_interface )
{
     DFBResult              ret;
     CoreLayerRegion       *region;
     IDirectFBSurface      *iface;
     IDirectFBSurface_data *surface_data;

     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_interface)
          return DFB_INVARG;

     if (data->level == DLSCL_SHARED) {
          D_WARN( "letting unprivileged GetSurface() call pass until cooperative level handling is finished" );
     }

     ret = CoreLayerContext_GetPrimaryRegion( data->context, true, &region );
     if (ret)
          return ret;

     DIRECT_ALLOCATE_INTERFACE( iface, IDirectFBSurface );

     ret = IDirectFBSurface_Layer_Construct( iface, NULL, NULL, NULL, region, DSCAPS_NONE, data->core,
                                             data->idirectfb );

     surface_data = iface->priv;
     if (!surface_data)
          return DFB_DEAD;

     /* Only perform single buffered clearing using a background when configured to do so and when the display layer
        region is frozen. */
     if (region->config.buffermode == DLBM_FRONTONLY && data->level != DLSCL_SHARED &&
         D_FLAGS_IS_SET( region->state, CLRSF_FROZEN )) {
          /* If a window stack is available, give it the opportunity to render the background (optionally based on
             configuration) and flip the display layer so it is visible.
             Otherwise, just directly flip the display layer and make it visible. */
          if (data->stack) {
               CoreWindowStack_RepaintAll( data->stack );
          }
          else {
               CoreSurface_Flip2( surface_data->surface, DFB_FALSE, NULL, NULL, DSFLIP_NONE, -1 );
          }
     }

     *ret_interface = ret ? NULL : iface;

     dfb_layer_region_unref( region );

     return ret;
}

static DFBResult
IDirectFBDisplayLayer_GetScreen( IDirectFBDisplayLayer  *thiz,
                                 IDirectFBScreen       **ret_interface )
{
     DFBResult        ret;
     IDirectFBScreen *iface;

     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_interface)
          return DFB_INVARG;

     DIRECT_ALLOCATE_INTERFACE( iface, IDirectFBScreen );

     ret = IDirectFBScreen_Construct( iface, data->screen );

     *ret_interface = ret ? NULL : iface;

     return ret;
}

static DFBResult
IDirectFBDisplayLayer_SetCooperativeLevel( IDirectFBDisplayLayer           *thiz,
                                           DFBDisplayLayerCooperativeLevel  level )
{
     DFBResult         ret;
     CoreLayerContext *context;
     CoreLayerRegion  *region;

     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (data->level == level)
          return DFB_OK;

     switch (level) {
          case DLSCL_SHARED:
          case DLSCL_ADMINISTRATIVE:
               if (data->level == DLSCL_EXCLUSIVE) {
                    ret = CoreLayer_GetPrimaryContext( data->layer, false, &context );
                    if (ret)
                         return ret;

                    ret = CoreLayerContext_GetPrimaryRegion( context, true, &region );
                    if (ret) {
                         dfb_layer_context_unref( context );
                         return ret;
                    }

                    dfb_layer_region_unref( data->region );
                    dfb_layer_context_unref( data->context );

                    data->context = context;
                    data->region  = region;
                    data->stack   = dfb_layer_context_windowstack( data->context );
               }

               break;

          case DLSCL_EXCLUSIVE:
               ret = CoreLayer_CreateContext( data->layer, &context );
               if (ret)
                    return ret;

               if (data->switch_exclusive) {
                    ret = CoreLayer_ActivateContext( data->layer, context );
                    if (ret) {
                         dfb_layer_context_unref( context );
                         return ret;
                    }
               }

               ret = CoreLayerContext_GetPrimaryRegion( context, true, &region );
               if (ret) {
                    dfb_layer_context_unref( context );
                    return ret;
               }

               dfb_layer_region_unref( data->region );
               dfb_layer_context_unref( data->context );

               data->context = context;
               data->region  = region;
               data->stack   = dfb_layer_context_windowstack( data->context );

               break;

          default:
               return DFB_INVARG;
     }

     data->level = level;

     return DFB_OK;
}

static DFBResult
IDirectFBDisplayLayer_GetConfiguration( IDirectFBDisplayLayer *thiz,
                                        DFBDisplayLayerConfig *ret_config )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_config)
          return DFB_INVARG;

     return dfb_layer_context_get_configuration( data->context, ret_config );
}

static DFBResult
IDirectFBDisplayLayer_TestConfiguration( IDirectFBDisplayLayer       *thiz,
                                         const DFBDisplayLayerConfig *config,
                                         DFBDisplayLayerConfigFlags  *ret_failed )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!config)
          return DFB_INVARG;

     if (((config->flags & DLCONF_WIDTH) && (config->width < 0)) ||
         ((config->flags & DLCONF_HEIGHT) && (config->height < 0)))
          return DFB_INVARG;

     return CoreLayerContext_TestConfiguration( data->context, config, ret_failed );
}

static DFBResult
IDirectFBDisplayLayer_SetConfiguration( IDirectFBDisplayLayer       *thiz,
                                        const DFBDisplayLayerConfig *config )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!config)
          return DFB_INVARG;

     if (((config->flags & DLCONF_WIDTH) && (config->width < 0)) ||
         ((config->flags & DLCONF_HEIGHT) && (config->height < 0)))
          return DFB_INVARG;

     switch (data->level) {
          case DLSCL_EXCLUSIVE:
          case DLSCL_ADMINISTRATIVE:
               return CoreLayerContext_SetConfiguration( data->context, config );

          default:
               break;
     }

     return DFB_ACCESSDENIED;
}

static DFBResult
IDirectFBDisplayLayer_SetScreenLocation( IDirectFBDisplayLayer *thiz,
                                         float                  x,
                                         float                  y,
                                         float                  width,
                                         float                  height )
{
     DFBLocation location = { x, y, width, height };

     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!D_FLAGS_IS_SET( data->desc.caps, DLCAPS_SCREEN_LOCATION ))
          return DFB_UNSUPPORTED;

     if (width <= 0 || height <= 0)
          return DFB_INVARG;

     if (data->level == DLSCL_SHARED)
          return DFB_ACCESSDENIED;

     return CoreLayerContext_SetScreenLocation( data->context, &location );
}

static DFBResult
IDirectFBDisplayLayer_SetScreenPosition( IDirectFBDisplayLayer *thiz,
                                         int                    x,
                                         int                    y )
{
     DFBPoint position;

     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p, %d,%d )\n", __FUNCTION__, thiz, x, y );

     if (!D_FLAGS_IS_SET( data->desc.caps, DLCAPS_SCREEN_POSITION ))
          return DFB_UNSUPPORTED;

     if (data->level == DLSCL_SHARED)
          return DFB_ACCESSDENIED;

     position.x = x;
     position.y = y;

     return CoreLayerContext_SetScreenPosition( data->context, &position );
}

static DFBResult
IDirectFBDisplayLayer_SetScreenRectangle( IDirectFBDisplayLayer *thiz,
                                          int                    x,
                                          int                    y,
                                          int                    width,
                                          int                    height )
{
     DFBRectangle rect = { x, y, width, height };

     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!D_FLAGS_IS_SET( data->desc.caps, DLCAPS_SCREEN_LOCATION ))
          return DFB_UNSUPPORTED;

     if (width <= 0 || height <= 0)
          return DFB_INVARG;

     if (data->level == DLSCL_SHARED)
          return DFB_ACCESSDENIED;

     return CoreLayerContext_SetScreenRectangle( data->context, &rect );
}

static DFBResult
IDirectFBDisplayLayer_GetStereoDepth( IDirectFBDisplayLayer *thiz,
                                      bool                  *follow_video,
                                      int                   *ret_z )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!(data->context->config.options & DLOP_LR_MONO) && !(data->context->config.options & DLOP_STEREO))
          return DFB_INVARG;

     if (!ret_z || !follow_video)
          return DFB_INVARG;

     return dfb_layer_context_get_stereo_depth( data->context, follow_video, ret_z );
}

static DFBResult
IDirectFBDisplayLayer_SetStereoDepth( IDirectFBDisplayLayer *thiz,
                                      bool                   follow_video,
                                      int                    z )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!follow_video && (z < -DLSO_FIXED_LIMIT || z > DLSO_FIXED_LIMIT))
          return DFB_INVARG;

     if (!(data->context->config.options & DLOP_LR_MONO) && !(data->context->config.options & DLOP_STEREO))
          return DFB_INVARG;

     if (data->level == DLSCL_SHARED)
          return DFB_ACCESSDENIED;

     return CoreLayerContext_SetStereoDepth( data->context, follow_video, z );
}

static DFBResult
IDirectFBDisplayLayer_SetOpacity( IDirectFBDisplayLayer *thiz,
                                  u8                     opacity )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (data->level == DLSCL_SHARED)
          return DFB_ACCESSDENIED;

     return CoreLayerContext_SetOpacity( data->context, opacity );
}

static DFBResult
IDirectFBDisplayLayer_SetSourceRectangle( IDirectFBDisplayLayer *thiz,
                                          int                    x,
                                          int                    y,
                                          int                    width,
                                          int                    height )
{
     DFBRectangle source = { x, y, width, height };

     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (x < 0 || y < 0 || width <= 0 || height <= 0)
          return DFB_INVARG;

     if (data->level == DLSCL_SHARED)
          return DFB_ACCESSDENIED;

     return CoreLayerContext_SetSourceRectangle( data->context, &source );
}

static DFBResult
IDirectFBDisplayLayer_SetFieldParity( IDirectFBDisplayLayer *thiz,
                                      int                    field )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (data->level != DLSCL_EXCLUSIVE)
          return DFB_ACCESSDENIED;

     return CoreLayerContext_SetFieldParity( data->context, field );
}

static DFBResult
IDirectFBDisplayLayer_SetClipRegions( IDirectFBDisplayLayer *thiz,
                                      const DFBRegion       *regions,
                                      int                    num_regions,
                                      DFBBoolean             positive )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!regions || num_regions < 1)
          return DFB_INVARG;

     if (num_regions > data->desc.clip_regions)
          return DFB_UNSUPPORTED;

     if (data->level != DLSCL_EXCLUSIVE)
          return DFB_ACCESSDENIED;

     return CoreLayerContext_SetClipRegions( data->context, regions, num_regions, positive );
}

static DFBResult
IDirectFBDisplayLayer_SetSrcColorKey( IDirectFBDisplayLayer *thiz,
                                      u8                     r,
                                      u8                     g,
                                      u8                     b )
{
     DFBColorKey key;

     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (data->level == DLSCL_SHARED)
          return DFB_ACCESSDENIED;

     key.r     = r;
     key.g     = g;
     key.b     = b;
     key.index = -1;

     return CoreLayerContext_SetSrcColorKey( data->context, &key );
}

static DFBResult
IDirectFBDisplayLayer_SetDstColorKey( IDirectFBDisplayLayer *thiz,
                                      u8                     r,
                                      u8                     g,
                                      u8                     b )
{
     DFBColorKey key;

     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (data->level == DLSCL_SHARED)
          return DFB_ACCESSDENIED;

     key.r     = r;
     key.g     = g;
     key.b     = b;
     key.index = -1;

     return CoreLayerContext_SetDstColorKey( data->context, &key );
}

static DFBResult
IDirectFBDisplayLayer_GetLevel( IDirectFBDisplayLayer *thiz,
                                int                   *ret_level )
{
     DFBResult ret;
     int       lvl;

     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_level)
          return DFB_INVARG;

     ret = dfb_layer_get_level( data->layer, &lvl );
     if (ret)
          return ret;

     *ret_level = lvl;

     return DFB_OK;
}

static DFBResult
IDirectFBDisplayLayer_SetLevel( IDirectFBDisplayLayer *thiz,
                                int                    level )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!D_FLAGS_IS_SET( data->desc.caps, DLCAPS_LEVELS ))
          return DFB_UNSUPPORTED;

     if (data->level == DLSCL_SHARED)
          return DFB_ACCESSDENIED;

     return CoreLayer_SetLevel( data->layer, level );
}

static DFBResult
IDirectFBDisplayLayer_SetBackgroundMode( IDirectFBDisplayLayer         *thiz,
                                         DFBDisplayLayerBackgroundMode  mode )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (data->level == DLSCL_SHARED)
          return DFB_ACCESSDENIED;

     switch (mode) {
          case DLBM_DONTCARE:
          case DLBM_COLOR:
          case DLBM_IMAGE:
          case DLBM_TILE:
               break;

          default:
               return DFB_INVARG;
     }

     if (!data->stack)
          return DFB_OK;

     return CoreWindowStack_BackgroundSetMode( data->stack, mode );
}

static DFBResult
IDirectFBDisplayLayer_SetBackgroundImage( IDirectFBDisplayLayer *thiz,
                                          IDirectFBSurface      *surface )
{
     IDirectFBSurface_data *surface_data;

     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!surface)
          return DFB_INVARG;

     if (data->level == DLSCL_SHARED)
          return DFB_ACCESSDENIED;

     surface_data = surface->priv;
     if (!surface_data)
          return DFB_DEAD;

     if (!surface_data->surface)
          return DFB_DESTROYED;

     if (!data->stack)
          return DFB_OK;

     CoreGraphicsStateClient_Flush( &surface_data->state_client );

     return CoreWindowStack_BackgroundSetImage( data->stack, surface_data->surface );
}

static DFBResult
IDirectFBDisplayLayer_SetBackgroundColor( IDirectFBDisplayLayer *thiz,
                                          u8                     r,
                                          u8                     g,
                                          u8                     b,
                                          u8                     a )
{
     DFBColor color = { a, r, g, b };

     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (data->level == DLSCL_SHARED)
          return DFB_ACCESSDENIED;

     if (!data->stack)
          return DFB_OK;

     return CoreWindowStack_BackgroundSetColor( data->stack, &color );
}

static DFBResult
IDirectFBDisplayLayer_GetColorAdjustment( IDirectFBDisplayLayer *thiz,
                                          DFBColorAdjustment    *ret_adj )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_adj)
          return DFB_INVARG;

     return dfb_layer_context_get_coloradjustment( data->context, ret_adj );
}

static DFBResult
IDirectFBDisplayLayer_SetColorAdjustment( IDirectFBDisplayLayer    *thiz,
                                          const DFBColorAdjustment *adj )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!adj || (adj->flags & ~DCAF_ALL))
          return DFB_INVARG;

     if (data->level == DLSCL_SHARED)
          return DFB_ACCESSDENIED;

     if (!adj->flags)
          return DFB_OK;

     return CoreLayerContext_SetColorAdjustment( data->context, adj );
}

static DFBResult
IDirectFBDisplayLayer_CreateWindow( IDirectFBDisplayLayer       *thiz,
                                    const DFBWindowDescription  *desc,
                                    IDirectFBWindow            **ret_interface )
{
     CoreWindow           *window;
     DFBResult             ret;
     DFBWindowDescription  wd;

     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     memset( &wd, 0, sizeof(wd) );

     wd.flags  = DWDESC_WIDTH | DWDESC_HEIGHT | DWDESC_POSX | DWDESC_POSY;
     wd.width  = (desc->flags & DWDESC_WIDTH)  ? desc->width  : 480;
     wd.height = (desc->flags & DWDESC_HEIGHT) ? desc->height : 300;
     wd.posx   = (desc->flags & DWDESC_POSX)   ? desc->posx   : 100;
     wd.posy   = (desc->flags & DWDESC_POSY)   ? desc->posy   : 100;

     D_DEBUG_AT( Layer, "%s( %p ) <- %4d,%4d-%4dx%4d )\n", __FUNCTION__, thiz, wd.posx, wd.posy, wd.width, wd.height );

     if (wd.width < 1 || wd.width > 4096 || wd.height < 1 || wd.height > 4096)
          return DFB_INVARG;

     if (desc->flags & DWDESC_CAPS) {
          if ((desc->caps & ~DWCAPS_ALL) || !ret_interface)
               return DFB_INVARG;

          wd.flags |= DWDESC_CAPS;
          wd.caps   = desc->caps;
     }

     if (desc->flags & DWDESC_PIXELFORMAT) {
          wd.flags       |= DWDESC_PIXELFORMAT;
          wd.pixelformat  = desc->pixelformat;
     }

     if (desc->flags & DWDESC_COLORSPACE) {
          wd.flags      |= DWDESC_COLORSPACE;
          wd.colorspace  = desc->colorspace;
     }

     if (desc->flags & DWDESC_SURFACE_CAPS) {
          wd.flags        |= DWDESC_SURFACE_CAPS;
          wd.surface_caps  = desc->surface_caps;
     }

     if (desc->flags & DWDESC_PARENT) {
          wd.flags     |= DWDESC_PARENT;
          wd.parent_id  = desc->parent_id;
     }

     if (desc->flags & DWDESC_OPTIONS) {
          wd.flags   |= DWDESC_OPTIONS;
          wd.options  = desc->options;
     }

     if (desc->flags & DWDESC_STACKING) {
          wd.flags    |= DWDESC_STACKING;
          wd.stacking  = desc->stacking;
     }

     if (desc->flags & DWDESC_RESOURCE_ID) {
          wd.flags       |= DWDESC_RESOURCE_ID;
          wd.resource_id  = desc->resource_id;
     }

     if (desc->flags & DWDESC_TOPLEVEL_ID) {
          wd.flags       |= DWDESC_TOPLEVEL_ID;
          wd.toplevel_id  = desc->toplevel_id;
     }

     ret = CoreLayerContext_CreateWindow( data->context, &wd, &window );
     if (ret)
          return ret;

     DIRECT_ALLOCATE_INTERFACE( *ret_interface, IDirectFBWindow );

     return IDirectFBWindow_Construct( *ret_interface, window, data->layer, data->core, data->idirectfb, true );
}

static DFBResult
IDirectFBDisplayLayer_GetWindow( IDirectFBDisplayLayer  *thiz,
                                 DFBWindowID             window_id,
                                 IDirectFBWindow       **ret_interface )
{
     DFBResult   ret;
     CoreWindow *window;

     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p, id %u )\n", __FUNCTION__, thiz, window_id );

     if (!ret_interface)
          return DFB_INVARG;

     /* IDirectFBWindow_Construct won't ref it, so we don't unref it */
     ret = CoreLayerContext_FindWindow( data->context, window_id, &window );
     if (ret)
          return ret;

     DIRECT_ALLOCATE_INTERFACE( *ret_interface, IDirectFBWindow );

     return IDirectFBWindow_Construct( *ret_interface, window, data->layer, data->core, data->idirectfb, false );
}

static DFBResult
IDirectFBDisplayLayer_EnableCursor( IDirectFBDisplayLayer *thiz,
                                    int                    enable )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (data->level == DLSCL_SHARED)
          return DFB_ACCESSDENIED;

     if (!data->stack)
          return DFB_OK;

     return CoreWindowStack_CursorEnable( data->stack, enable );
}

static DFBResult
IDirectFBDisplayLayer_GetCursorPosition( IDirectFBDisplayLayer *thiz,
                                         int                   *ret_x,
                                         int                   *ret_y )
{
     DFBResult ret;
     DFBPoint  point;

     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_x && !ret_y)
          return DFB_INVARG;

     if (!data->stack)
          return DFB_ACCESSDENIED;

     ret = CoreWindowStack_CursorGetPosition( data->stack, &point );
     if (ret)
          return ret;

     if (ret_x)
          *ret_x = point.x;

     if (ret_y)
          *ret_y = point.y;

     return ret;
}

static DFBResult
IDirectFBDisplayLayer_WarpCursor( IDirectFBDisplayLayer *thiz,
                                  int                    x,
                                  int                    y )
{
     DFBPoint point = { x, y };

     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (data->level == DLSCL_SHARED)
          return DFB_ACCESSDENIED;

     if (!data->stack)
          return DFB_OK;

     return CoreWindowStack_CursorWarp( data->stack, &point );
}

static DFBResult
IDirectFBDisplayLayer_SetCursorAcceleration( IDirectFBDisplayLayer *thiz,
                                             int                    numerator,
                                             int                    denominator,
                                             int                    threshold )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (numerator < 0 || denominator < 1 || threshold < 0)
          return DFB_INVARG;

     if (data->level == DLSCL_SHARED)
          return DFB_ACCESSDENIED;

     if (!data->stack)
          return DFB_OK;

     return CoreWindowStack_CursorSetAcceleration( data->stack, numerator, denominator, threshold );
}

static DFBResult
IDirectFBDisplayLayer_SetCursorShape( IDirectFBDisplayLayer *thiz,
                                      IDirectFBSurface      *shape,
                                      int                    hot_x,
                                      int                    hot_y )
{
     DFBPoint               hotspot = { hot_x, hot_y };
     IDirectFBSurface_data *shape_data;

     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!shape)
          return DFB_INVARG;

     if (data->level == DLSCL_SHARED)
          return DFB_ACCESSDENIED;

     if (!data->stack)
          return DFB_OK;

     shape_data = shape->priv;

     if (hot_x < 0                                    || hot_y < 0 ||
         hot_x >= shape_data->surface->config.size.w  || hot_y >= shape_data->surface->config.size.h)
          return DFB_INVARG;

     return CoreWindowStack_CursorSetShape( data->stack, shape_data->surface, &hotspot );
}

static DFBResult
IDirectFBDisplayLayer_SetCursorOpacity( IDirectFBDisplayLayer *thiz,
                                        u8                     opacity )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (data->level == DLSCL_SHARED)
          return DFB_ACCESSDENIED;

     if (!data->stack)
          return DFB_OK;

     return CoreWindowStack_CursorSetOpacity( data->stack, opacity );
}

static DFBResult
IDirectFBDisplayLayer_WaitForSync( IDirectFBDisplayLayer *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     return CoreLayer_WaitVSync( data->layer );
}

static DFBResult
IDirectFBDisplayLayer_SwitchContext( IDirectFBDisplayLayer *thiz,
                                     DFBBoolean             exclusive )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!exclusive && data->level == DLSCL_EXCLUSIVE) {
          DFBResult         ret;
          CoreLayerContext *context;

          ret = CoreLayer_GetPrimaryContext( data->layer, false, &context );
          if (ret)
               return ret;

          CoreLayer_ActivateContext( data->layer, context );

          dfb_layer_context_unref( context );
     }
     else
          CoreLayer_ActivateContext( data->layer, data->context );

     data->switch_exclusive = exclusive;

     return DFB_OK;
}

static DFBResult
IDirectFBDisplayLayer_SetRotation( IDirectFBDisplayLayer *thiz,
                                   int                    rotation )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (data->level == DLSCL_SHARED)
          return DFB_ACCESSDENIED;

     return CoreLayerContext_SetRotation( data->context, rotation );
}

static DFBResult
IDirectFBDisplayLayer_GetRotation( IDirectFBDisplayLayer *thiz,
                                   int                   *ret_rotation )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_rotation)
          return DFB_INVARG;

     *ret_rotation = data->context->rotation;

     return DFB_OK;
}

static DFBResult
IDirectFBDisplayLayer_GetWindowByResourceID( IDirectFBDisplayLayer  *thiz,
                                             unsigned long           resource_id,
                                             IDirectFBWindow       **ret_interface )
{
     DFBResult   ret;
     CoreWindow *window;

     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_interface)
          return DFB_INVARG;

     ret = CoreLayerContext_FindWindowByResourceID( data->context, resource_id, &window );
     if (ret)
          return ret;

     DIRECT_ALLOCATE_INTERFACE( *ret_interface, IDirectFBWindow );

     return IDirectFBWindow_Construct( *ret_interface, window, data->layer, data->core, data->idirectfb, false );
}

static DFBResult
IDirectFBDisplayLayer_SetSurface( IDirectFBDisplayLayer *thiz,
                                  IDirectFBSurface      *surface )
{
     IDirectFBSurface_data *surface_data;

     DIRECT_INTERFACE_GET_DATA( IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     if (!surface)
          return DFB_INVARG;

     if (data->level != DLSCL_EXCLUSIVE)
          return DFB_ACCESSDENIED;

     surface_data = surface->priv;
     if (!surface_data)
          return DFB_DEAD;

     return CoreLayerRegion_SetSurface( data->region, surface_data->surface );
}

DFBResult
IDirectFBDisplayLayer_Construct( IDirectFBDisplayLayer *thiz,
                                 CoreLayer             *layer,
                                 CoreDFB               *core,
                                 IDirectFB             *idirectfb )
{
     DFBResult         ret;
     CoreLayerContext *context;
     CoreLayerRegion  *region;

     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBDisplayLayer )

     D_DEBUG_AT( Layer, "%s( %p )\n", __FUNCTION__, thiz );

     ret = CoreLayer_GetPrimaryContext( layer, true, &context );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz )
          return ret;
     }

     ret = CoreLayerContext_GetPrimaryRegion( context, true, &region );
     if (ret) {
          dfb_layer_context_unref( context );
          DIRECT_DEALLOCATE_INTERFACE( thiz )
          return ret;
     }

     data->ref              = 1;
     data->layer            = layer;
     data->core             = core;
     data->idirectfb        = idirectfb;
     data->screen           = layer->screen;
     data->context          = context;
     data->region           = region;
     data->stack            = dfb_layer_context_windowstack( context );
     data->switch_exclusive = DFB_TRUE;

     dfb_layer_get_description( data->layer, &data->desc );

     thiz->AddRef                = IDirectFBDisplayLayer_AddRef;
     thiz->Release               = IDirectFBDisplayLayer_Release;
     thiz->GetID                 = IDirectFBDisplayLayer_GetID;
     thiz->GetDescription        = IDirectFBDisplayLayer_GetDescription;
     thiz->GetSourceDescriptions = IDirectFBDisplayLayer_GetSourceDescriptions;
     thiz->GetCurrentOutputField = IDirectFBDisplayLayer_GetCurrentOutputField;
     thiz->GetSurface            = IDirectFBDisplayLayer_GetSurface;
     thiz->GetScreen             = IDirectFBDisplayLayer_GetScreen;
     thiz->SetCooperativeLevel   = IDirectFBDisplayLayer_SetCooperativeLevel;
     thiz->GetConfiguration      = IDirectFBDisplayLayer_GetConfiguration;
     thiz->TestConfiguration     = IDirectFBDisplayLayer_TestConfiguration;
     thiz->SetConfiguration      = IDirectFBDisplayLayer_SetConfiguration;
     thiz->SetScreenLocation     = IDirectFBDisplayLayer_SetScreenLocation;
     thiz->SetScreenPosition     = IDirectFBDisplayLayer_SetScreenPosition;
     thiz->SetScreenRectangle    = IDirectFBDisplayLayer_SetScreenRectangle;
     thiz->GetStereoDepth        = IDirectFBDisplayLayer_GetStereoDepth;
     thiz->SetStereoDepth        = IDirectFBDisplayLayer_SetStereoDepth;
     thiz->SetOpacity            = IDirectFBDisplayLayer_SetOpacity;
     thiz->SetSourceRectangle    = IDirectFBDisplayLayer_SetSourceRectangle;
     thiz->SetFieldParity        = IDirectFBDisplayLayer_SetFieldParity;
     thiz->SetClipRegions        = IDirectFBDisplayLayer_SetClipRegions;
     thiz->SetSrcColorKey        = IDirectFBDisplayLayer_SetSrcColorKey;
     thiz->SetDstColorKey        = IDirectFBDisplayLayer_SetDstColorKey;
     thiz->GetLevel              = IDirectFBDisplayLayer_GetLevel;
     thiz->SetLevel              = IDirectFBDisplayLayer_SetLevel;
     thiz->SetBackgroundMode     = IDirectFBDisplayLayer_SetBackgroundMode;
     thiz->SetBackgroundImage    = IDirectFBDisplayLayer_SetBackgroundImage;
     thiz->SetBackgroundColor    = IDirectFBDisplayLayer_SetBackgroundColor;
     thiz->GetColorAdjustment    = IDirectFBDisplayLayer_GetColorAdjustment;
     thiz->SetColorAdjustment    = IDirectFBDisplayLayer_SetColorAdjustment;
     thiz->CreateWindow          = IDirectFBDisplayLayer_CreateWindow;
     thiz->GetWindow             = IDirectFBDisplayLayer_GetWindow;
     thiz->EnableCursor          = IDirectFBDisplayLayer_EnableCursor;
     thiz->GetCursorPosition     = IDirectFBDisplayLayer_GetCursorPosition;
     thiz->WarpCursor            = IDirectFBDisplayLayer_WarpCursor;
     thiz->SetCursorAcceleration = IDirectFBDisplayLayer_SetCursorAcceleration;
     thiz->SetCursorShape        = IDirectFBDisplayLayer_SetCursorShape;
     thiz->SetCursorOpacity      = IDirectFBDisplayLayer_SetCursorOpacity;
     thiz->WaitForSync           = IDirectFBDisplayLayer_WaitForSync;
     thiz->SwitchContext         = IDirectFBDisplayLayer_SwitchContext;
     thiz->SetRotation           = IDirectFBDisplayLayer_SetRotation;
     thiz->GetRotation           = IDirectFBDisplayLayer_GetRotation;
     thiz->GetWindowByResourceID = IDirectFBDisplayLayer_GetWindowByResourceID;
     thiz->SetSurface            = IDirectFBDisplayLayer_SetSurface;

     return DFB_OK;
}

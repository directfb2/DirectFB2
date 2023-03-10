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

#include <core/CoreLayerRegion.h>
#include <core/CoreSurface.h>
#include <core/layer_region.h>
#include <display/idirectfbsurface.h>
#include <display/idirectfbsurface_layer.h>

D_DEBUG_DOMAIN( Surface, "IDirectFBSurfaceL", "IDirectFBSurface_Layer Interface" );

/**********************************************************************************************************************/

/*
 * private data struct of IDirectFBSurface_Layer
 */
typedef struct {
     IDirectFBSurface_data  base;   /* base surface implementation */

     CoreLayerRegion       *region; /* the region this surface belongs to */
} IDirectFBSurface_Layer_data;

/**********************************************************************************************************************/

static void
IDirectFBSurface_Layer_Destruct( IDirectFBSurface *thiz )
{
     IDirectFBSurface_Layer_data *data = thiz->priv;

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     dfb_layer_region_unref( data->region );

     IDirectFBSurface_Destruct( thiz );
}

static DirectResult
IDirectFBSurface_Layer_Release( IDirectFBSurface *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface_Layer )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (--data->base.ref == 0)
          IDirectFBSurface_Layer_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_Layer_Flip( IDirectFBSurface    *thiz,
                             const DFBRegion     *region,
                             DFBSurfaceFlipFlags  flags )
{
     DFBResult ret;
     DFBRegion reg;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface_Layer )

     D_DEBUG_AT( Surface, "%s( %p, %p, 0x%08x )\n", __FUNCTION__, thiz, region, flags );

     if (!data->base.surface)
          return DFB_DESTROYED;

     if (data->base.locked)
          return DFB_LOCKED;

     if (!data->base.area.current.w || !data->base.area.current.h ||
         (region && (region->x1 > region->x2 || region->y1 > region->y2)))
          return DFB_INVAREA;

     IDirectFBSurface_StopAll( &data->base );

     if (data->base.parent) {
          IDirectFBSurface_data *parent_data;

          parent_data = data->base.parent->priv;
          if (!parent_data)
               return DFB_DEAD;

          if (parent_data) {
               /* Signal end of sequence of operations. */
               dfb_state_lock( &parent_data->state );
               dfb_state_stop_drawing( &parent_data->state );
               dfb_state_unlock( &parent_data->state );
          }
     }

     dfb_region_from_rectangle( &reg, &data->base.area.current );

     if (region) {
          DFBRegion clip = DFB_REGION_INIT_TRANSLATED( region, data->base.area.wanted.x, data->base.area.wanted.y );

          if (!dfb_region_region_intersect( &reg, &clip ))
               return DFB_INVAREA;
     }

     D_DEBUG_AT( Surface, "  -> flip %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS_FROM_REGION( &reg ) );

     CoreGraphicsStateClient_Flush( &data->base.state_client );

     switch (data->region->config.buffermode) {
          case DLBM_TRIPLE:
          case DLBM_BACKVIDEO:
               if ((flags & DSFLIP_SWAP) ||
                   (!(flags & DSFLIP_BLIT) &&
                    reg.x1 == 0                                     && reg.y1 == 0 &&
                    reg.x2 == data->base.surface->config.size.w - 1 && reg.y2 == data->base.surface->config.size.h - 1))
                    if (!(flags & DSFLIP_UPDATE))
                         ++data->base.local_flip_count;
               break;

          default:
               break;
     }

     ret = CoreSurface_Flip2( data->base.surface, DFB_FALSE, &reg, &reg, flags, data->base.current_frame_time );
     if (ret) {
          ret = CoreLayerRegion_FlipUpdate2( data->region, &reg, &reg, data->region->surface->flips, flags,
                                             data->base.current_frame_time );
          if (ret)
               return ret;
     }

     IDirectFBSurface_WaitForBackBuffer( &data->base );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_Layer_FlipStereo( IDirectFBSurface    *thiz,
                                   const DFBRegion     *left_region,
                                   const DFBRegion     *right_region,
                                   DFBSurfaceFlipFlags  flags )
{
     DFBResult ret;
     DFBRegion l_reg, r_reg;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface_Layer )

     D_DEBUG_AT( Surface, "%s( %p, %p, %p, 0x%08x )\n", __FUNCTION__, thiz, left_region, right_region, flags );

     if (!data->base.surface)
          return DFB_DESTROYED;

     if (!(data->base.surface->config.caps & DSCAPS_STEREO))
          return DFB_UNSUPPORTED;

     if (data->base.locked)
          return DFB_LOCKED;

     if (!data->base.area.current.w || !data->base.area.current.h ||
         (left_region && (left_region->x1 > left_region->x2 || left_region->y1 > left_region->y2)) ||
         (right_region && (right_region->x1 > right_region->x2 || right_region->y1 > right_region->y2)))
          return DFB_INVAREA;

     IDirectFBSurface_StopAll( &data->base );

     if (data->base.parent) {
          IDirectFBSurface_data *parent_data;

          parent_data = data->base.parent->priv;
          if (!parent_data)
               return DFB_DEAD;

          if (parent_data) {
               /* Signal end of sequence of operations. */
               dfb_state_lock( &parent_data->state );
               dfb_state_stop_drawing( &parent_data->state );
               dfb_state_unlock( &parent_data->state );
          }
     }

     dfb_region_from_rectangle( &l_reg, &data->base.area.current );
     dfb_region_from_rectangle( &r_reg, &data->base.area.current );

     if (left_region) {
          DFBRegion clip;
          clip = DFB_REGION_INIT_TRANSLATED( left_region, data->base.area.wanted.x, data->base.area.wanted.y );

          if (!dfb_region_region_intersect( &l_reg, &clip ))
               return DFB_INVAREA;
     }
     if (right_region) {
          DFBRegion clip;
          clip = DFB_REGION_INIT_TRANSLATED( right_region, data->base.area.wanted.x, data->base.area.wanted.y );

          if (!dfb_region_region_intersect( &r_reg, &clip ))
               return DFB_INVAREA;
     }

     D_DEBUG_AT( Surface, "  -> flip stereo left: %4d,%4d-%4dx%4d right: %4d,%4d-%4dx%4d\n",
                 DFB_RECTANGLE_VALS_FROM_REGION( &l_reg ), DFB_RECTANGLE_VALS_FROM_REGION( &r_reg ) );

     CoreGraphicsStateClient_Flush( &data->base.state_client );

     if (data->base.surface->config.caps & DSCAPS_FLIPPING) {
          if ((flags & DSFLIP_SWAP) ||
              (!(flags & DSFLIP_BLIT) &&
               l_reg.x1 == 0                                     && l_reg.y1 == 0                                     &&
               l_reg.x2 == data->base.surface->config.size.w - 1 && l_reg.y2 == data->base.surface->config.size.h - 1 &&
               r_reg.x1 == 0                                     && r_reg.y1 == 0                                     &&
               r_reg.x2 == data->base.surface->config.size.w - 1 && r_reg.y2 == data->base.surface->config.size.h - 1))
               if (!(flags & DSFLIP_UPDATE))
                    ++data->base.local_flip_count;
     }

     ret = CoreSurface_Flip2( data->base.surface, DFB_FALSE, &l_reg, &r_reg, flags, data->base.current_frame_time );
     if (ret) {
          ret = CoreLayerRegion_FlipUpdate2( data->region, &l_reg, &r_reg, data->region->surface->flips, flags,
                                             data->base.current_frame_time );
          if (ret)
               return ret;
     }

     IDirectFBSurface_WaitForBackBuffer( &data->base );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_Layer_GetSubSurface( IDirectFBSurface    *thiz,
                                      const DFBRectangle  *rect,
                                      IDirectFBSurface   **ret_interface )
{
     DFBResult ret;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface_Layer )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->base.surface)
          return DFB_DESTROYED;

     if (!ret_interface)
          return DFB_INVARG;

     direct_mutex_lock( &data->base.children_lock );

     if (data->base.children_free) {
          IDirectFBSurface_data *child_data;

          child_data = (IDirectFBSurface_data*) data->base.children_free;

          direct_list_remove( &data->base.children_free, &child_data->link );
          direct_list_append( &data->base.children_data, &child_data->link );

          direct_mutex_unlock( &data->base.children_lock );

          *ret_interface = child_data->thiz;

          ret = (*ret_interface)->MakeSubSurface( *ret_interface, thiz, rect );
          if (ret) {
               direct_mutex_unlock( &data->base.children_lock );
               return ret;
          }

          return DFB_OK;
     }

     direct_mutex_unlock( &data->base.children_lock );

     DIRECT_ALLOCATE_INTERFACE( *ret_interface, IDirectFBSurface );

     if (rect || data->base.limit_set) {
          DFBRectangle wanted, granted;

          /* Compute wanted rectangle. */
          if (rect) {
               wanted = *rect;

               wanted.x += data->base.area.wanted.x;
               wanted.y += data->base.area.wanted.y;

               if (wanted.w <= 0 || wanted.h <= 0) {
                    wanted.w = 0;
                    wanted.h = 0;
               }
          }
          else {
               wanted = data->base.area.wanted;
          }

          /* Compute granted rectangle. */
          granted = wanted;

          dfb_rectangle_intersect( &granted, &data->base.area.granted );

          ret = IDirectFBSurface_Layer_Construct( *ret_interface, thiz, &wanted, &granted, data->region,
                                                  data->base.caps | DSCAPS_SUBSURFACE, data->base.core,
                                                  data->base.idirectfb );
     }
     else {
          ret = IDirectFBSurface_Layer_Construct( *ret_interface, thiz, NULL, NULL, data->region,
                                                  data->base.caps | DSCAPS_SUBSURFACE, data->base.core,
                                                  data->base.idirectfb );
     }

     return ret;
}

DFBResult
IDirectFBSurface_Layer_Construct( IDirectFBSurface       *thiz,
                                  IDirectFBSurface       *parent,
                                  DFBRectangle           *wanted,
                                  DFBRectangle           *granted,
                                  CoreLayerRegion        *region,
                                  DFBSurfaceCapabilities  caps,
                                  CoreDFB                *core,
                                  IDirectFB              *dfb )
{
     DFBResult    ret;
     CoreSurface *surface;

     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBSurface_Layer );

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     ret = dfb_layer_region_ref( region );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     ret = CoreLayerRegion_GetSurface( region, &surface );
     if (ret) {
          dfb_layer_region_unref( region );
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     ret = IDirectFBSurface_Construct( thiz, parent, wanted, granted, NULL, surface, surface->config.caps | caps, core,
                                       dfb );
     if (ret) {
          dfb_surface_unref( surface );
          dfb_layer_region_unref( region );
          return ret;
     }

     dfb_surface_unref( surface );

     data->region = region;

     thiz->Release       = IDirectFBSurface_Layer_Release;
     thiz->Flip          = IDirectFBSurface_Layer_Flip;
     thiz->FlipStereo    = IDirectFBSurface_Layer_FlipStereo;
     thiz->GetSubSurface = IDirectFBSurface_Layer_GetSubSurface;

     return DFB_OK;
}

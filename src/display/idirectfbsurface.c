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
#include <core/CoreSurfaceClient.h>
#include <core/core.h>
#include <core/fonts.h>
#include <core/palette.h>
#include <core/surface_allocation.h>
#include <core/surface_client.h>
#include <core/surface_pool.h>
#include <direct/memcpy.h>
#include <direct/thread.h>
#include <display/idirectfbpalette.h>
#include <display/idirectfbsurface.h>
#include <display/idirectfbsurfaceallocation.h>
#include <gfx/convert.h>
#include <input/idirectfbeventbuffer.h>
#include <media/idirectfbfont.h>

D_DEBUG_DOMAIN( Surface,         "IDirectFBSurface",         "IDirectFBSurface Interface" );
D_DEBUG_DOMAIN( Surface_Updates, "IDirectFBSurface/Updates", "IDirectFBSurface Interface Updates" );

/**********************************************************************************************************************/

static DFBResult
register_prealloc( IDirectFBSurface_data *data )
{
     DFBResult    ret;
     unsigned int i;

     if (data->surface->config.caps & DSCAPS_TRIPLE)
          data->memory_permissions_count = 3;
     else if (data->surface->config.caps & DSCAPS_DOUBLE)
          data->memory_permissions_count = 2;
     else
          data->memory_permissions_count = 1;

     for (i = 0; i < data->memory_permissions_count; i++) {
          ret = dfb_core_memory_permissions_add( data->core, CMPF_READ | CMPF_WRITE,
                                                 data->surface->config.preallocated[i].addr,
                                                 data->surface->config.preallocated[i].pitch *
                                                 DFB_PLANE_MULTIPLY( data->surface->config.format,
                                                                     data->surface->config.size.h ),
                                                 &data->memory_permissions[i] );
          if (ret)
               goto error;
     }

     return DFB_OK;

error:
     for (--i; i >= 0; i--)
          dfb_core_memory_permissions_remove( data->core, data->memory_permissions[i] );

     data->memory_permissions_count = 0;

     return ret;
}

static DFBResult
unregister_prealloc( IDirectFBSurface_data *data )
{
     unsigned int i;

     for (i = 0; i < data->memory_permissions_count; i++)
          dfb_core_memory_permissions_remove( data->core, data->memory_permissions[i] );

     data->memory_permissions_count = 0;

     return DFB_OK;
}

/**********************************************************************************************************************/

void
IDirectFBSurface_Destruct( IDirectFBSurface *thiz )
{
     IDirectFBSurface_data *data = thiz->priv;
     IDirectFBSurface      *parent;
     unsigned int           i;

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     direct_mutex_lock( &data->children_lock );

     while (data->children_free) {
          IDirectFBSurface_data *child_data;
          IDirectFBSurface      *child;

          child_data = (IDirectFBSurface_data*) data->children_free;

          direct_list_remove( &data->children_free, &child_data->link );

          direct_mutex_unlock( &data->children_lock );

          child = child_data->thiz;

          child->Release( child );

          direct_mutex_lock( &data->children_lock );
     }

     direct_mutex_unlock( &data->children_lock );

     if (data->memory_permissions_count) {
          CoreDFB_WaitIdle( data->core );

          unregister_prealloc( data );
     }
     else
          CoreGraphicsStateClient_Flush( &data->state_client );

     if (data->surface_client)
          dfb_surface_client_unref( data->surface_client );

     parent = data->parent;
     if (parent) {
          IDirectFBSurface_data *parent_data;

          D_MAGIC_ASSERT( parent, DirectInterface );

          parent_data = parent->priv;

          D_ASSERT( parent_data != NULL );

          direct_mutex_lock( &parent_data->children_lock );

          direct_list_remove( &parent_data->children_data, &data->link );

          direct_mutex_unlock( &parent_data->children_lock );
     }

     if (data->surface) {
          dfb_surface_detach( data->surface, &data->reaction );
          dfb_surface_detach( data->surface, &data->reaction_frame );
     }

     CoreGraphicsStateClient_Deinit( &data->state_client );

     dfb_state_stop_drawing( &data->state );

     dfb_state_set_destination( &data->state, NULL );
     dfb_state_set_source( &data->state, NULL );
     dfb_state_set_source_mask( &data->state, NULL );
     dfb_state_set_source2( &data->state, NULL );

     dfb_state_destroy( &data->state );

     if (data->font)
          data->font->Release( data->font );

     if (data->surface) {
          if (data->locked)
               dfb_surface_unlock_buffer( data->surface, &data->lock );

          dfb_surface_unref( data->surface );
     }

     for (i = 0; i < data->local_buffer_count; i++) {
          if (data->allocations[i]) {
               dfb_surface_allocation_unref( data->allocations[i] );
               data->allocations[i] = NULL;
          }
     }

     direct_mutex_deinit( &data->children_lock );

     direct_waitqueue_deinit( &data->back_buffer_wq );
     direct_mutex_deinit( &data->back_buffer_lock );

     direct_mutex_deinit( &data->surface_client_lock );

     DIRECT_DEALLOCATE_INTERFACE( thiz );

     if (parent)
          parent->Release( parent );
}

static DirectResult
IDirectFBSurface_AddRef( IDirectFBSurface *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     data->ref++;

     return DFB_OK;
}

static DirectResult
IDirectFBSurface_Release( IDirectFBSurface *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (data->ref == 1 && data->parent && dfb_config->subsurface_caching) {
          IDirectFBSurface_data *parent_data;

          D_MAGIC_ASSERT( data->parent, DirectInterface );

          parent_data = data->parent->priv;

          D_ASSERT( parent_data != NULL );

          direct_mutex_lock( &parent_data->children_lock );

          direct_list_remove( &parent_data->children_data, &data->link );
          direct_list_append( &parent_data->children_free, &data->link );

          direct_mutex_unlock( &parent_data->children_lock );
     }

     if (--data->ref == 0)
          IDirectFBSurface_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetCapabilities( IDirectFBSurface       *thiz,
                                  DFBSurfaceCapabilities *ret_caps )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_caps)
          return DFB_INVARG;

     *ret_caps = data->caps;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetPosition( IDirectFBSurface *thiz,
                              int              *ret_x,
                              int              *ret_y )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_x && !ret_y)
          return DFB_INVARG;

     if (ret_x)
          *ret_x = data->area.wanted.x;

     if (ret_y)
          *ret_y = data->area.wanted.y;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetSize( IDirectFBSurface *thiz,
                          int              *ret_width,
                          int              *ret_height )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_width && !ret_height)
          return DFB_INVARG;

     if (ret_width)
          *ret_width = data->area.wanted.w;

     if (ret_height)
          *ret_height = data->area.wanted.h;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetVisibleRectangle( IDirectFBSurface *thiz,
                                      DFBRectangle     *ret_rect )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_rect)
          return DFB_INVARG;

     ret_rect->x = data->area.current.x - data->area.wanted.x;
     ret_rect->y = data->area.current.y - data->area.wanted.y;
     ret_rect->w = data->area.current.w;
     ret_rect->h = data->area.current.h;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetPixelFormat( IDirectFBSurface      *thiz,
                                 DFBSurfacePixelFormat *ret_pixelformat )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!ret_pixelformat)
          return DFB_INVARG;

     *ret_pixelformat = data->surface->config.format;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetColorSpace( IDirectFBSurface     *thiz,
                                DFBSurfaceColorSpace *ret_colorspace )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!ret_colorspace)
          return DFB_INVARG;

     *ret_colorspace = data->surface->config.colorspace;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetAccelerationMask( IDirectFBSurface    *thiz,
                                      IDirectFBSurface    *source,
                                      DFBAccelerationMask *ret_mask )
{
     DFBResult           ret;
     DFBAccelerationMask mask;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!ret_mask)
          return DFB_INVARG;

     if (source) {
          IDirectFBSurface_data *src_data = source->priv;

          dfb_state_set_source( &data->state, src_data->surface );
          dfb_state_set_source2( &data->state, data->surface );
     }

     ret = CoreGraphicsStateClient_GetAccelerationMask( &data->state_client, &mask );
     if (ret)
          return ret;

     /* Check text rendering function. */
     if (data->font) {
          IDirectFBFont_data *font_data = data->font->priv;

          if (dfb_gfxcard_drawstring_check_state( font_data->font, &data->state, &data->state_client, DSTF_NONE ))
               mask |= DFXL_DRAWSTRING;
     }

     *ret_mask = mask;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetPalette( IDirectFBSurface  *thiz,
                             IDirectFBPalette **ret_interface )
{
     DFBResult         ret;
     CorePalette      *palette;
     IDirectFBPalette *iface;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!data->surface->palette)
          return DFB_UNSUPPORTED;

     if (!ret_interface)
          return DFB_INVARG;

     ret = CoreSurface_GetPalette( data->surface, &palette );
     if (ret)
          return ret;

     DIRECT_ALLOCATE_INTERFACE( iface, IDirectFBPalette );

     ret = IDirectFBPalette_Construct( iface, palette, data->core );
     if (ret)
          goto out;

     *ret_interface = iface;

out:
     dfb_palette_unref( palette );

     return ret;
}

static DFBResult
IDirectFBSurface_SetPalette( IDirectFBSurface *thiz,
                             IDirectFBPalette *palette )
{
     IDirectFBPalette_data *palette_data;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!palette)
          return DFB_INVARG;

     if (!DFB_PIXELFORMAT_IS_INDEXED( data->surface->config.format ))
          return DFB_UNSUPPORTED;

     palette_data = palette->priv;
     if (!palette_data)
          return DFB_DEAD;

     if (!palette_data->palette)
          return DFB_DESTROYED;

     CoreSurface_SetPalette( data->surface, palette_data->palette );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetAlphaRamp( IDirectFBSurface *thiz,
                               u8                a0,
                               u8                a1,
                               u8                a2,
                               u8                a3 )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     CoreSurface_SetAlphaRamp( data->surface, a0, a1, a2, a3 );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetStereoEye( IDirectFBSurface    *thiz,
                               DFBSurfaceStereoEye *ret_eye )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p, %p )\n", __FUNCTION__, thiz, ret_eye );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!(data->surface->config.caps & DSCAPS_STEREO))
          return DFB_UNSUPPORTED;

     *ret_eye = data->src_eye;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetStereoEye( IDirectFBSurface    *thiz,
                               DFBSurfaceStereoEye  eye )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p, %u )\n", __FUNCTION__, thiz, eye );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!(data->surface->config.caps & DSCAPS_STEREO))
          return DFB_UNSUPPORTED;

     dfb_state_set_to( &data->state, DSBR_BACK, eye );

     data->src_eye = eye;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_Lock( IDirectFBSurface     *thiz,
                       DFBSurfaceLockFlags   flags,
                       void                **ret_ptr,
                       int                  *ret_pitch )
{
     DFBResult              ret;
     long long              ts, ts2;
     DFBSurfaceBufferRole   role   = DSBR_FRONT;
     CoreSurfaceAccessFlags access = CSAF_NONE;

     D_UNUSED_P( ts );
     D_UNUSED_P( ts2 );

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (data->locked)
          return DFB_LOCKED;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (!flags || !ret_ptr || !ret_pitch)
          return DFB_INVARG;

     if (flags & DSLF_READ)
          access |= CSAF_READ;

     if (flags & DSLF_WRITE) {
          access |= CSAF_WRITE;
          role = DSBR_BACK;
     }

     CoreGraphicsStateClient_Flush( &data->state_client );

     if (direct_log_domain_check( &Surface ))
          ts = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );

     unsigned int           index      = (data->local_flip_count + role) % data->local_buffer_count;
     CoreSurfaceAllocation *allocation = data->allocations[index];

     if (allocation) {
          D_DEBUG_AT( Surface, "  -> having allocation %p\n", allocation );

          if (!allocation->buffer || !direct_serial_check( &allocation->serial, &allocation->buffer->serial )) {
               D_DEBUG_AT( Surface, "    -> outdated!\n" );

               dfb_surface_allocation_ref( allocation );

               data->allocations[index] = allocation = NULL;
          }
     }

     if (!allocation) {
          D_DEBUG_AT( Surface, "  -> getting allocation from %p\n", data->surface );

          ret = CoreSurface_PreLockBuffer3( data->surface, role, data->local_flip_count, data->src_eye, CSAID_CPU,
                                            access, true, &allocation );
          if (ret)
               return ret;

          data->allocations[index] = allocation;
     }

     ret = dfb_surface_allocation_ref( allocation );
     if (ret) {
          D_DERROR( ret, "IDirectFBSurface: Ref'ing allocation in '%s' failed!\n", allocation->pool->desc.name );
          return ret;
     }

     /* Lock the allocation. */
     dfb_surface_buffer_lock_init( &data->lock, CSAID_CPU, access );

     D_DEBUG_AT( Surface, "  -> locking %p\n", allocation );

     ret = dfb_surface_pool_lock( allocation->pool, allocation, &data->lock );
     if (ret) {
          D_DERROR( ret, "IDirectFBSurface: Locking allocation in '%s' failed!\n", allocation->pool->desc.name );
          dfb_surface_buffer_lock_deinit( &data->lock );
          return ret;
     }

     if (direct_log_domain_check( &Surface )) {
          ts2 = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );
          D_DEBUG_AT( Surface, "  -> locking took %lld us\n", ts2 - ts );
     }

     data->locked = true;

     *ret_ptr   = data->lock.addr + data->lock.pitch * data->area.current.y +
                  DFB_BYTES_PER_LINE( data->surface->config.format, data->area.current.x );
     *ret_pitch = data->lock.pitch;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetFramebufferOffset( IDirectFBSurface *thiz,
                                       int              *offset )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBSurface)

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!data->locked)
          return DFB_ACCESSDENIED;

     if (!data->lock.phys) {
          /* The surface is probably in a system buffer if there's no physical address. */
          return DFB_UNSUPPORTED;
     }

     if (!offset)
          return DFB_INVARG;

     *offset = data->lock.offset;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_Unlock( IDirectFBSurface *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (data->locked) {
          dfb_surface_unlock_buffer( data->surface, &data->lock );

          data->locked = false;
     }

     return DFB_OK;
}

DFBResult
IDirectFBSurface_Flip( IDirectFBSurface    *thiz,
                       const DFBRegion     *region,
                       DFBSurfaceFlipFlags  flags )
{
     DFBResult ret = DFB_OK;
     DFBRegion reg;
     bool      dispatched = false;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p, %p, 0x%08x )\n", __FUNCTION__, thiz, region, flags );

     if (!data->surface)
          return DFB_DESTROYED;

     if (data->locked)
          return DFB_LOCKED;

     if (!data->area.current.w || !data->area.current.h ||
         (region && (region->x1 > region->x2 || region->y1 > region->y2)))
          return DFB_INVAREA;

     IDirectFBSurface_StopAll( data );

     if (data->parent) {
          IDirectFBSurface_data *parent_data;

          parent_data = data->parent->priv;
          if (!parent_data)
               return DFB_DEAD;

          if (parent_data) {
               /* Signal end of sequence of operations. */
               dfb_state_lock( &parent_data->state );
               dfb_state_stop_drawing( &parent_data->state );
               dfb_state_unlock( &parent_data->state );
          }
     }

     dfb_region_from_rectangle( &reg, &data->area.current );

     if (region) {
          DFBRegion clip = DFB_REGION_INIT_TRANSLATED( region, data->area.wanted.x, data->area.wanted.y );

          if (!dfb_region_region_intersect( &reg, &clip ))
               return DFB_INVAREA;
     }

     D_DEBUG_AT( Surface, "  -> flip %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS_FROM_REGION( &reg ) );

     CoreGraphicsStateClient_Flush( &data->state_client );

     if (dfb_config->force_frametime && !data->current_frame_time)
          thiz->GetFrameTime( thiz, &data->current_frame_time );

     if (data->surface->config.caps & DSCAPS_FLIPPING) {
          if ((flags & DSFLIP_SWAP) ||
              (!(flags & DSFLIP_BLIT) &&
               reg.x1 == 0                                && reg.y1 == 0 &&
               reg.x2 == data->surface->config.size.w - 1 && reg.y2 == data->surface->config.size.h - 1)) {
               if (!(flags & DSFLIP_UPDATE))
                    ++data->local_flip_count;

               dfb_state_set_destination_2( &data->state, data->surface, data->local_flip_count );

               ret = CoreSurface_DispatchUpdate( data->surface, DFB_FALSE, &reg, NULL, flags,
                                                 data->current_frame_time, data->local_flip_count );
               dispatched = true;
          }
     }

     if (!dispatched)
          ret = CoreSurface_Flip2( data->surface, DFB_FALSE, &reg, NULL, flags, data->current_frame_time );

     data->current_frame_time = 0;

     if (ret)
          return ret;

     if (!(flags & DSFLIP_NOWAIT))
          IDirectFBSurface_WaitForBackBuffer( data );

     return DFB_OK;
}

DFBResult
IDirectFBSurface_FlipStereo( IDirectFBSurface    *thiz,
                             const DFBRegion     *left_region,
                             const DFBRegion     *right_region,
                             DFBSurfaceFlipFlags  flags )
{
     DFBResult ret = DFB_OK;
     DFBRegion l_reg, r_reg;
     bool      dispatched = false;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p, %p, %p, 0x%08x )\n", __FUNCTION__, thiz, left_region, right_region, flags );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!(data->surface->config.caps & DSCAPS_STEREO))
          return DFB_UNSUPPORTED;

     if (data->locked)
          return DFB_LOCKED;

     if (!data->area.current.w || !data->area.current.h ||
         (left_region  && (left_region->x1 > left_region->x2   || left_region->y1 > left_region->y2)) ||
         (right_region && (right_region->x1 > right_region->x2 || right_region->y1 > right_region->y2)))
          return DFB_INVAREA;

     IDirectFBSurface_StopAll( data );

     if (data->parent) {
          IDirectFBSurface_data *parent_data;

          parent_data = data->parent->priv;
          if (!parent_data)
               return DFB_DEAD;

          if (parent_data) {
               /* Signal end of sequence of operations. */
               dfb_state_lock( &parent_data->state );
               dfb_state_stop_drawing( &parent_data->state );
               dfb_state_unlock( &parent_data->state );
          }
     }

     dfb_region_from_rectangle( &l_reg, &data->area.current );
     dfb_region_from_rectangle( &r_reg, &data->area.current );

     if (left_region) {
          DFBRegion clip = DFB_REGION_INIT_TRANSLATED( left_region, data->area.wanted.x, data->area.wanted.y );

          if (!dfb_region_region_intersect( &l_reg, &clip ))
               return DFB_INVAREA;
     }

     if (right_region) {
          DFBRegion clip = DFB_REGION_INIT_TRANSLATED( right_region, data->area.wanted.x, data->area.wanted.y );

          if (!dfb_region_region_intersect( &r_reg, &clip ))
               return DFB_INVAREA;
     }

     D_DEBUG_AT( Surface, "  -> flip stereo left: %4d,%4d-%4dx%4d right: %4d,%4d-%4dx%4d\n",
                 DFB_RECTANGLE_VALS_FROM_REGION( &l_reg ), DFB_RECTANGLE_VALS_FROM_REGION( &r_reg ) );

     CoreGraphicsStateClient_Flush( &data->state_client );

     if (dfb_config->force_frametime && !data->current_frame_time)
          thiz->GetFrameTime( thiz, &data->current_frame_time );

     if (data->surface->config.caps & DSCAPS_FLIPPING) {
          if ((flags & DSFLIP_SWAP) ||
              (!(flags & DSFLIP_BLIT) &&
               l_reg.x1 == 0                                && l_reg.y1 == 0                                &&
               l_reg.x2 == data->surface->config.size.w - 1 && l_reg.y2 == data->surface->config.size.h - 1 &&
               r_reg.x1 == 0                                && r_reg.y1 == 0                                &&
               r_reg.x2 == data->surface->config.size.w - 1 && r_reg.y2 == data->surface->config.size.h - 1)) {
               if (!(flags & DSFLIP_UPDATE))
                    ++data->local_flip_count;

               ret = CoreSurface_DispatchUpdate( data->surface, DFB_FALSE, &l_reg, &r_reg, flags,
                                                 data->current_frame_time, data->local_flip_count );
               dispatched = true;
          }
     }

     if (!dispatched)
          ret = CoreSurface_Flip2( data->surface, DFB_FALSE, &l_reg, &r_reg, flags, data->current_frame_time );

     dfb_state_set_destination_2( &data->state, data->surface, data->local_flip_count );

     data->current_frame_time = 0;

     if (ret)
          return ret;

     if (!(flags & DSFLIP_NOWAIT))
          IDirectFBSurface_WaitForBackBuffer( data );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetField( IDirectFBSurface *thiz,
                           int               field )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!(data->surface->config.caps & DSCAPS_INTERLACED))
          return DFB_UNSUPPORTED;

     if (field < 0 || field > 1)
          return DFB_INVARG;

     CoreSurface_SetField( data->surface, field );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_Clear( IDirectFBSurface *thiz,
                        u8                r,
                        u8                g,
                        u8                b,
                        u8                a )
{
     DFBColor                old_color;
     unsigned int            old_index;
     DFBSurfaceDrawingFlags  old_flags;
     DFBSurfaceRenderOptions old_options;
     DFBColor                color = { a, r, g, b };

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p, 0x%08x )\n", __FUNCTION__, thiz, (unsigned int) PIXEL_ARGB( a, r, g, b ) );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     /* Save current color and drawing flags. */
     old_color   = data->state.color;
     old_index   = data->state.color_index;
     old_flags   = data->state.drawingflags;
     old_options = data->state.render_options;

     /* Set drawing flags. */
     dfb_state_set_drawing_flags( &data->state, DSDRAW_NOFX );

     /* Set render options. */
     dfb_state_set_render_options( &data->state, DSRO_NONE );

     /* Set color. */
     if (DFB_PIXELFORMAT_IS_INDEXED( data->surface->config.format ))
          dfb_state_set_color_index( &data->state, dfb_palette_search( data->surface->palette, r, g, b, a ) );

     dfb_state_set_color( &data->state, &color );

     /* Fill the visible rectangle. */
     CoreGraphicsStateClient_FillRectangles( &data->state_client, &data->area.current, 1 );

     /* Restore drawing flags. */
     dfb_state_set_drawing_flags( &data->state, old_flags );

     /* Restore render options. */
     dfb_state_set_render_options( &data->state, old_options );

     /* Restore color. */
     if (DFB_PIXELFORMAT_IS_INDEXED( data->surface->config.format ))
          dfb_state_set_color_index( &data->state, old_index );

     dfb_state_set_color( &data->state, &old_color );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetClip( IDirectFBSurface *thiz,
                          const DFBRegion  *clip )
{
     DFBRegion newclip;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p, %p )\n", __FUNCTION__, thiz, clip );

     D_DEBUG_AT( Surface, "  <- %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS( &data->area.wanted ) );

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (clip) {
          newclip = DFB_REGION_INIT_TRANSLATED( clip, data->area.wanted.x, data->area.wanted.y );

          D_DEBUG_AT( Surface, "  <- %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS_FROM_REGION( &newclip ) );

          if (!dfb_unsafe_region_rectangle_intersect( &newclip, &data->area.wanted ))
               return DFB_INVARG;

          D_DEBUG_AT( Surface, "  -> %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS_FROM_REGION( &newclip ) );

          data->clip_set    = true;
          data->clip_wanted = newclip;

          if (!dfb_region_rectangle_intersect( &newclip, &data->area.current ))
               return DFB_INVAREA;
     }
     else {
          dfb_region_from_rectangle( &newclip, &data->area.current );
          data->clip_set = false;
     }

     D_DEBUG_AT( Surface, "  -> clip %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS_FROM_REGION( &newclip ) );

     dfb_state_set_clip( &data->state, &newclip );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetClip( IDirectFBSurface *thiz,
                          DFBRegion        *ret_clip )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (!ret_clip)
          return DFB_INVARG;

     *ret_clip = DFB_REGION_INIT_TRANSLATED( &data->state.clip, -data->area.wanted.x, -data->area.wanted.y );

     D_DEBUG_AT( Surface, "  -> %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS_FROM_REGION( ret_clip ) );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetColor( IDirectFBSurface *thiz,
                           u8                r,
                           u8                g,
                           u8                b,
                           u8                a )
{
     DFBColor color = { a, r, g, b };

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p, color 0x%08x )\n", __FUNCTION__, thiz, (unsigned int) PIXEL_ARGB( a, r, g, b ) );

     if (!data->surface)
          return DFB_DESTROYED;

     dfb_state_set_color( &data->state, &color );

     if (DFB_PIXELFORMAT_IS_INDEXED( data->surface->config.format ))
          dfb_state_set_color_index( &data->state, dfb_palette_search( data->surface->palette, r, g, b, a ) );

     data->state.colors[0]        = data->state.color;
     data->state.color_indices[0] = data->state.color_index;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetColorIndex( IDirectFBSurface *thiz,
                                unsigned int      index )
{
     CorePalette *palette;
     DFBResult    ret;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p, color index %3u )\n", __FUNCTION__, thiz, index );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!DFB_PIXELFORMAT_IS_INDEXED( data->surface->config.format ))
          return DFB_UNSUPPORTED;

     palette = data->surface->palette;
     if (!palette)
          return DFB_UNSUPPORTED;

     if (index > palette->num_entries)
          return DFB_INVARG;

     ret = CoreGraphicsStateClient_SetColorAndIndex( &data->state_client, &palette->entries[index], index );
     if (ret)
          return ret;

     dfb_state_set_color( &data->state, &palette->entries[index] );
     dfb_state_set_color_index( &data->state, index );

     data->state.colors[0]        = data->state.color;
     data->state.color_indices[0] = data->state.color_index;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetSrcBlendFunction( IDirectFBSurface        *thiz,
                                      DFBSurfaceBlendFunction  function )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p, %u )\n", __FUNCTION__, thiz, function );

     switch (function) {
          case DSBF_ZERO:
          case DSBF_ONE:
          case DSBF_SRCCOLOR:
          case DSBF_INVSRCCOLOR:
          case DSBF_SRCALPHA:
          case DSBF_INVSRCALPHA:
          case DSBF_DESTALPHA:
          case DSBF_INVDESTALPHA:
          case DSBF_DESTCOLOR:
          case DSBF_INVDESTCOLOR:
          case DSBF_SRCALPHASAT:
               dfb_state_set_src_blend( &data->state, function );
               return DFB_OK;

          default:
               break;
     }

     return DFB_INVARG;
}

static DFBResult
IDirectFBSurface_SetDstBlendFunction( IDirectFBSurface        *thiz,
                                      DFBSurfaceBlendFunction  function )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p, %u )\n", __FUNCTION__, thiz, function );

     switch (function) {
          case DSBF_ZERO:
          case DSBF_ONE:
          case DSBF_SRCCOLOR:
          case DSBF_INVSRCCOLOR:
          case DSBF_SRCALPHA:
          case DSBF_INVSRCALPHA:
          case DSBF_DESTALPHA:
          case DSBF_INVDESTALPHA:
          case DSBF_DESTCOLOR:
          case DSBF_INVDESTCOLOR:
          case DSBF_SRCALPHASAT:
               dfb_state_set_dst_blend( &data->state, function );
               return DFB_OK;

          default:
               break;
     }

     return DFB_INVARG;
}

static DFBResult
IDirectFBSurface_SetPorterDuff( IDirectFBSurface         *thiz,
                                DFBSurfacePorterDuffRule  rule )
{
     DFBSurfaceBlendFunction src;
     DFBSurfaceBlendFunction dst;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p, %u )\n", __FUNCTION__, thiz, rule );

     switch (rule) {
          case DSPD_NONE:
               src = DSBF_SRCALPHA;
               dst = DSBF_INVSRCALPHA;
               break;
          case DSPD_CLEAR:
               src = DSBF_ZERO;
               dst = DSBF_ZERO;
               break;
          case DSPD_SRC:
               src = DSBF_ONE;
               dst = DSBF_ZERO;
               break;
          case DSPD_SRC_OVER:
               src = DSBF_ONE;
               dst = DSBF_INVSRCALPHA;
               break;
          case DSPD_DST_OVER:
               src = DSBF_INVDESTALPHA;
               dst = DSBF_ONE;
               break;
          case DSPD_SRC_IN:
               src = DSBF_DESTALPHA;
               dst = DSBF_ZERO;
               break;
          case DSPD_DST_IN:
               src = DSBF_ZERO;
               dst = DSBF_SRCALPHA;
               break;
          case DSPD_SRC_OUT:
               src = DSBF_INVDESTALPHA;
               dst = DSBF_ZERO;
               break;
          case DSPD_DST_OUT:
               src = DSBF_ZERO;
               dst = DSBF_INVSRCALPHA;
               break;
          case DSPD_SRC_ATOP:
               src = DSBF_DESTALPHA;
               dst = DSBF_INVSRCALPHA;
               break;
          case DSPD_DST_ATOP:
               src = DSBF_INVDESTALPHA;
               dst = DSBF_SRCALPHA;
               break;
          case DSPD_ADD:
               src = DSBF_ONE;
               dst = DSBF_ONE;
               break;
          case DSPD_XOR:
               src = DSBF_INVDESTALPHA;
               dst = DSBF_INVSRCALPHA;
               break;
          case DSPD_DST:
               src = DSBF_ZERO;
               dst = DSBF_ONE;
               break;

          default:
               return DFB_INVARG;
     }

     dfb_state_set_src_blend( &data->state, src );
     dfb_state_set_dst_blend( &data->state, dst );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetSrcColorKey( IDirectFBSurface *thiz,
                                 u8                r,
                                 u8                g,
                                 u8                b )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     data->src_key.r = r;
     data->src_key.g = g;
     data->src_key.b = b;

     if (DFB_PIXELFORMAT_IS_INDEXED( data->surface->config.format ))
          data->src_key.value = dfb_palette_search( data->surface->palette, r, g, b, 0x80 );
     else
          data->src_key.value = dfb_color_to_pixel( data->surface->config.format, r, g, b );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetSrcColorKeyIndex( IDirectFBSurface *thiz,
                                      unsigned int      index )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!DFB_PIXELFORMAT_IS_INDEXED( data->surface->config.format ))
          return DFB_UNSUPPORTED;

     if (!data->surface->palette)
          return DFB_UNSUPPORTED;

     if (index > data->surface->palette->num_entries)
          return DFB_INVARG;

     data->src_key.r     = data->surface->palette->entries[index].r;
     data->src_key.g     = data->surface->palette->entries[index].g;
     data->src_key.b     = data->surface->palette->entries[index].b;
     data->src_key.value = index;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetDstColorKey( IDirectFBSurface *thiz,
                                 u8                r,
                                 u8                g,
                                 u8                b )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     data->dst_key.r = r;
     data->dst_key.g = g;
     data->dst_key.b = b;

     if (DFB_PIXELFORMAT_IS_INDEXED( data->surface->config.format ))
          data->dst_key.value = dfb_palette_search( data->surface->palette, r, g, b, 0x80 );
     else
          data->dst_key.value = dfb_color_to_pixel( data->surface->config.format, r, g, b );

     dfb_state_set_dst_colorkey( &data->state, data->dst_key.value );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetDstColorKeyIndex( IDirectFBSurface *thiz,
                                      unsigned int      index )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!DFB_PIXELFORMAT_IS_INDEXED( data->surface->config.format ))
          return DFB_UNSUPPORTED;

     if (!data->surface->palette)
          return DFB_UNSUPPORTED;

     if (index > data->surface->palette->num_entries)
          return DFB_INVARG;

     data->dst_key.r     = data->surface->palette->entries[index].r;
     data->dst_key.g     = data->surface->palette->entries[index].g;
     data->dst_key.b     = data->surface->palette->entries[index].b;
     data->dst_key.value = index;

     dfb_state_set_dst_colorkey( &data->state, data->dst_key.value );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetBlittingFlags( IDirectFBSurface        *thiz,
                                   DFBSurfaceBlittingFlags  flags )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p, 0x%08x )\n", __FUNCTION__, thiz, flags );

     dfb_state_set_blitting_flags( &data->state, flags );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_Blit( IDirectFBSurface   *thiz,
                       IDirectFBSurface   *source,
                       const DFBRectangle *source_rect,
                       int                 x,
                       int                 y )
{
     DFBRectangle           srect;
     DFBPoint               p;
     IDirectFBSurface_data *src_data;
     int                    dx = x;
     int                    dy = y;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p, source %p, source_rect %p, %d,%d )\n", __FUNCTION__,
                 thiz, source, source_rect, dx, dy );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (!source)
          return DFB_INVARG;

     src_data = source->priv;

     if (!src_data->area.current.w || !src_data->area.current.h)
          return DFB_INVAREA;

     if (source_rect) {
          D_DEBUG_AT( Surface, "  -> [%2d] %4d,%4d-%4dx%4d <- %4d,%4d\n",
                      0, dx, dy, source_rect->w, source_rect->h, source_rect->x, source_rect->y );

          if (source_rect->w < 1 || source_rect->h < 1)
               return DFB_OK;

          srect = *source_rect;

          srect.x += src_data->area.wanted.x;
          srect.y += src_data->area.wanted.y;

          if (!dfb_rectangle_intersect( &srect, &src_data->area.current ))
               return DFB_INVAREA;

          dx += srect.x - (source_rect->x + src_data->area.wanted.x);
          dy += srect.y - (source_rect->y + src_data->area.wanted.y);
     }
     else {
          srect = src_data->area.current;

          dx += srect.x - src_data->area.wanted.x;
          dy += srect.y - src_data->area.wanted.y;
     }

     CoreGraphicsStateClient_Flush( &src_data->state_client );

     if (src_data->surface_client) {
          direct_mutex_lock( &data->surface_client_lock );

          D_DEBUG_AT( Surface, "  -> blit client surface (flip count %u)\n", src_data->surface_client_flip_count );

          dfb_state_set_source_2( &data->state, src_data->surface, src_data->surface_client_flip_count );
     }
     else
          dfb_state_set_source( &data->state, src_data->surface );

     dfb_state_set_from( &data->state, DSBR_FRONT, src_data->src_eye );

     /* Fetch the source color key from the source if necessary. */
     if (data->state.blittingflags & DSBLIT_SRC_COLORKEY)
          dfb_state_set_src_colorkey( &data->state, src_data->src_key.value );

     p.x = data->area.wanted.x + dx;
     p.y = data->area.wanted.y + dy;

     CoreGraphicsStateClient_Blit( &data->state_client, &srect, &p, 1 );

     if (src_data->surface_client)
          direct_mutex_unlock( &data->surface_client_lock );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_TileBlit( IDirectFBSurface   *thiz,
                           IDirectFBSurface   *source,
                           const DFBRectangle *source_rect,
                           int                 x,
                           int                 y )
{
     DFBRectangle           srect;
     DFBPoint               p1, p2;
     IDirectFBSurface_data *src_data;
     int                    dx = x;
     int                    dy = y;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p, source %p, source_rect %p, %d,%d )\n", __FUNCTION__,
                 thiz, source, source_rect, dx, dy );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (!source)
          return DFB_INVARG;

     src_data = source->priv;

     if (!src_data->area.current.w || !src_data->area.current.h)
          return DFB_INVAREA;

     if (source_rect) {
          D_DEBUG_AT( Surface, "  -> [%2d] %4d,%4d-%4dx%4d <- %4d,%4d\n",
                      0, dx, dy, source_rect->w, source_rect->h, source_rect->x, source_rect->y );

          if (source_rect->w < 1 || source_rect->h < 1)
               return DFB_OK;

          srect = *source_rect;

          srect.x += src_data->area.wanted.x;
          srect.y += src_data->area.wanted.y;

          if (!dfb_rectangle_intersect( &srect, &src_data->area.current ))
               return DFB_INVAREA;

          dx += srect.x - (source_rect->x + src_data->area.wanted.x);
          dy += srect.y - (source_rect->y + src_data->area.wanted.y);
     }
     else {
          srect = src_data->area.current;

          dx += srect.x - src_data->area.wanted.x;
          dy += srect.y - src_data->area.wanted.y;
     }

     CoreGraphicsStateClient_Flush( &src_data->state_client );

     dfb_state_set_source( &data->state, src_data->surface );

     dfb_state_set_from( &data->state, DSBR_FRONT, src_data->src_eye );

     /* Fetch the source color key from the source if necessary. */
     if (data->state.blittingflags & DSBLIT_SRC_COLORKEY)
          dfb_state_set_src_colorkey( &data->state, src_data->src_key.value );

     dx %= srect.w;
     if (dx > 0)
          dx -= srect.w;

     dy %= srect.h;
     if (dy > 0)
          dy -= srect.h;

     dx += data->area.wanted.x;
     dy += data->area.wanted.y;

     p1.x = dx;
     p1.y = dy;

     p2.x = dx + data->area.wanted.w + srect.w - 1;
     p2.y = dy + data->area.wanted.h + srect.h - 1;

     CoreGraphicsStateClient_TileBlit( &data->state_client, &srect, &p1, &p2, 1 );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_BatchBlit( IDirectFBSurface   *thiz,
                            IDirectFBSurface   *source,
                            const DFBRectangle *source_rects,
                            const DFBPoint     *dest_points,
                            int                 num )
{
     int                    i, dx, dy, sx, sy;
     DFBRectangle          *rects;
     DFBPoint              *points;
     IDirectFBSurface_data *src_data;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p, source %p, source_rects %p, dest_points %p, num %d )\n", __FUNCTION__,
                 thiz, source, source_rects, dest_points, num );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (!source || !source_rects || !dest_points || num < 1)
          return DFB_INVARG;

     src_data = source->priv;

     if (!src_data->area.current.w || !src_data->area.current.h) {
          return DFB_INVAREA;
     }

     dx = data->area.wanted.x;
     dy = data->area.wanted.y;

     sx = src_data->area.wanted.x;
     sy = src_data->area.wanted.y;

     rects  = alloca( sizeof(DFBRectangle) * num );
     points = alloca( sizeof(DFBPoint) * num );

     direct_memcpy( rects, source_rects, sizeof(DFBRectangle) * num );
     direct_memcpy( points, dest_points, sizeof(DFBPoint) * num );

     for (i = 0; i < num; i++) {
          rects[i].x += sx;
          rects[i].y += sy;

          points[i].x += dx;
          points[i].y += dy;

          if (!dfb_rectangle_intersect( &rects[i], &src_data->area.current ))
               rects[i].w = rects[i].h = 0;

          points[i].x += rects[i].x - (source_rects[i].x + sx);
          points[i].y += rects[i].y - (source_rects[i].y + sy);

          D_DEBUG_AT( Surface, "  -> [%3d] %4d,%4d-%dx%4d -> %4d,%4d\n",
                      i, DFB_RECTANGLE_VALS( &source_rects[i] ), dest_points[i].x, dest_points[i].y );
     }

     CoreGraphicsStateClient_Flush( &src_data->state_client );

     dfb_state_set_source( &data->state, src_data->surface );

     dfb_state_set_from( &data->state, DSBR_FRONT, src_data->src_eye );

     /* Fetch the source color key from the source if necessary. */
     if (data->state.blittingflags & DSBLIT_SRC_COLORKEY)
          dfb_state_set_src_colorkey( &data->state, src_data->src_key.value );

     CoreGraphicsStateClient_Blit( &data->state_client, rects, points, num );

     return DFB_OK;
}

static DFBResult IDirectFBSurface_BatchStretchBlit( IDirectFBSurface   *thiz,
                                                    IDirectFBSurface   *source,
                                                    const DFBRectangle *source_rects,
                                                    const DFBRectangle *dest_rects,
                                                    int                 num );

static DFBResult
IDirectFBSurface_StretchBlit( IDirectFBSurface   *thiz,
                              IDirectFBSurface   *source,
                              const DFBRectangle *source_rect,
                              const DFBRectangle *dest_rect )
{
     DFBRectangle srect, drect;

     if (!source)
          return DFB_INVARG;

     if (source_rect)
          srect = *source_rect;
     else {
          IDirectFBSurface_data *src_data = source->priv;

          srect = (DFBRectangle) { 0, 0, src_data->area.wanted.w, src_data->area.wanted.h };
     }

     if (dest_rect)
          drect = *dest_rect;
     else {
          DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

          drect = (DFBRectangle) { 0, 0, data->area.wanted.w, data->area.wanted.h };
     }

     return IDirectFBSurface_BatchStretchBlit( thiz, source, &srect, &drect, 1 );
}

#define SET_VERTEX(v,X,Y,Z,W,S,T) \
     do {                         \
          (v)->x = X;             \
          (v)->y = Y;             \
          (v)->z = Z;             \
          (v)->w = W;             \
          (v)->s = S;             \
          (v)->t = T;             \
     } while (0)

static DFBResult
IDirectFBSurface_TextureTriangles( IDirectFBSurface     *thiz,
                                   IDirectFBSurface     *texture,
                                   const DFBVertex      *vertices,
                                   const int            *indices,
                                   int                   num,
                                   DFBTriangleFormation  formation )
{
     int                    i;
     DFBVertex             *translated;
     IDirectFBSurface_data *src_data;
     bool                   src_sub;
     float                  x0 = 0;
     float                  y0 = 0;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (!texture || !vertices || num < 3)
          return DFB_INVARG;

     src_data = texture->priv;

     if ((src_sub = (src_data->caps & DSCAPS_SUBSURFACE))) {
          D_ONCE( "sub surface texture with repeated mapping" );

          x0 = data->area.wanted.x;
          y0 = data->area.wanted.y;
     }

     switch (formation) {
          case DTTF_LIST:
               if (num % 3)
                    return DFB_INVARG;
               break;

          case DTTF_STRIP:
          case DTTF_FAN:
               break;

          default:
               return DFB_INVARG;
     }

     translated = alloca( num * sizeof(DFBVertex) );
     if (!translated)
          return DFB_NOSYSTEMMEMORY;

     if (src_sub) {
          float oowidth  = 1.0f / src_data->surface->config.size.w;
          float ooheight = 1.0f / src_data->surface->config.size.h;

          float s0 = src_data->area.wanted.x * oowidth;
          float t0 = src_data->area.wanted.y * ooheight;

          float fs = src_data->area.wanted.w * oowidth;
          float ft = src_data->area.wanted.h * ooheight;

          for (i = 0; i < num; i++) {
               const DFBVertex *in  = &vertices[indices ? indices[i] : i];
               DFBVertex       *out = &translated[i];

               SET_VERTEX( out, x0 + in->x, y0 + in->y, in->z, in->w, s0 + fs * in->s, t0 + ft * in->t );
          }
     }
     else {
          if (indices) {
               for (i = 0; i < num; i++) {
                    const DFBVertex *in  = &vertices[indices[i]];
                    DFBVertex       *out = &translated[i];

                    SET_VERTEX( out, x0 + in->x, y0 + in->y, in->z, in->w, in->s, in->t );
               }
          }
          else {
               direct_memcpy( translated, vertices, num * sizeof(DFBVertex) );

               for (i = 0; i < num; i++) {
                    translated[i].x += x0;
                    translated[i].y += y0;
               }
          }
     }

     CoreGraphicsStateClient_Flush( &src_data->state_client );

     dfb_state_set_source( &data->state, src_data->surface );

     dfb_state_set_from( &data->state, DSBR_FRONT, src_data->src_eye );

     /* Fetch the source color key from the source if necessary. */
     if (data->state.blittingflags & DSBLIT_SRC_COLORKEY)
          dfb_state_set_src_colorkey( &data->state, src_data->src_key.value );

     CoreGraphicsStateClient_TextureTriangles( &data->state_client, translated, num, formation );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetDrawingFlags( IDirectFBSurface       *thiz,
                                  DFBSurfaceDrawingFlags  flags )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p, 0x%08x )\n", __FUNCTION__, thiz, flags );

     dfb_state_set_drawing_flags( &data->state, flags );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_FillRectangle( IDirectFBSurface *thiz,
                                int               x,
                                int               y,
                                int               w,
                                int               h )
{
     DFBRectangle rect = { x, y, w, h };

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );
     D_DEBUG_AT( Surface, "  -> [%2d] %4d,%4d-%4dx%4d\n", 0, x, y, w, h );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (w <= 0 || h <= 0)
          return DFB_INVARG;

     rect.x += data->area.wanted.x;
     rect.y += data->area.wanted.y;

     CoreGraphicsStateClient_FillRectangles( &data->state_client, &rect, 1 );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_DrawRectangle( IDirectFBSurface *thiz,
                                int               x,
                                int               y,
                                int               w,
                                int               h )
{
     DFBRectangle rect = { x, y, w, h };

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );
     D_DEBUG_AT( Surface, "  -> [%2d] %4d,%4d-%4dx%4d\n", 0, x, y, w, h );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (w <= 0 || h <= 0)
          return DFB_INVARG;

     rect.x += data->area.wanted.x;
     rect.y += data->area.wanted.y;

     CoreGraphicsStateClient_DrawRectangles( &data->state_client, &rect, 1 );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_DrawLine( IDirectFBSurface *thiz,
                           int               x1,
                           int               y1,
                           int               x2,
                           int               y2 )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );
     D_DEBUG_AT( Surface, "  -> [%2d] %4d,%4d-%4d,%4d\n", 0, x1, y1, x2, y2 );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if ((x1 == x2 || y1 == y2) && !(data->state.render_options & DSRO_MATRIX)) {
          DFBRectangle rect;

          if (x1 <= x2) {
               rect.x = x1;
               rect.w = x2 - x1 + 1;
          }
          else {
               rect.x = x2;
               rect.w = x1 - x2 + 1;
          }

          if (y1 <= y2) {
               rect.y = y1;
               rect.h = y2 - y1 + 1;
          }
          else {
               rect.y = y2;
               rect.h = y1 - y2 + 1;
          }

          rect.x += data->area.wanted.x;
          rect.y += data->area.wanted.y;

          CoreGraphicsStateClient_FillRectangles( &data->state_client, &rect, 1 );
     }
     else {
          DFBRegion line = { x1, y1, x2, y2 };

          line.x1 += data->area.wanted.x;
          line.x2 += data->area.wanted.x;
          line.y1 += data->area.wanted.y;
          line.y2 += data->area.wanted.y;

          CoreGraphicsStateClient_DrawLines( &data->state_client, &line, 1 );
     }

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_DrawLines( IDirectFBSurface *thiz,
                            const DFBRegion  *lines,
                            unsigned int      num_lines )
{
     unsigned int i;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p, %p [%u] )\n", __FUNCTION__, thiz, lines, num_lines );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (!lines || !num_lines)
          return DFB_INVARG;

     /* Check if all lines are either horizontal or vertical. */
     for (i = 0; i < num_lines; i++) {
          if (lines[i].x1 != lines[i].x2 && lines[i].y1 != lines[i].y2)
               break;
     }

     /* Use real line drawing. */
     if (i < num_lines) {
          DFBRegion *local_lines = alloca( sizeof(DFBRegion) * num_lines );

          if (data->area.wanted.x || data->area.wanted.y) {
               for (i = 0; i < num_lines; i++) {
                    local_lines[i].x1 = lines[i].x1 + data->area.wanted.x;
                    local_lines[i].x2 = lines[i].x2 + data->area.wanted.x;
                    local_lines[i].y1 = lines[i].y1 + data->area.wanted.y;
                    local_lines[i].y2 = lines[i].y2 + data->area.wanted.y;
               }
          }
          else
               /* Clipping may modify lines, so we copy them. */
               direct_memcpy( local_lines, lines, sizeof(DFBRegion) * num_lines );

          CoreGraphicsStateClient_DrawLines( &data->state_client, local_lines, num_lines );
     }
     /* Optimized rectangle drawing. */
     else {
          DFBRectangle *local_rects = alloca( sizeof(DFBRectangle) * num_lines );

          for (i = 0; i < num_lines; i++) {
               /* Vertical line. */
               if (lines[i].x1 == lines[i].x2) {
                    local_rects[i].x = data->area.wanted.x + lines[i].x1;
                    local_rects[i].y = data->area.wanted.y + MIN( lines[i].y1, lines[i].y2 );
                    local_rects[i].w = 1;
                    local_rects[i].h = ABS( lines[i].y2 - lines[i].y1 ) + 1;
               }
               /* Horizontal line. */
               else {
                    local_rects[i].x = data->area.wanted.x + MIN( lines[i].x1, lines[i].x2 );
                    local_rects[i].y = data->area.wanted.y + lines[i].y1;
                    local_rects[i].w = ABS( lines[i].x2 - lines[i].x1 ) + 1;
                    local_rects[i].h = 1;
               }
          }

          CoreGraphicsStateClient_FillRectangles( &data->state_client, local_rects, num_lines );
     }

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_FillTriangle( IDirectFBSurface *thiz,
                               int               x1,
                               int               y1,
                               int               x2,
                               int               y2,
                               int               x3,
                               int               y3 )
{
     DFBTriangle tri = { x1, y1, x2, y2, x3, y3 };

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );
     D_DEBUG_AT( Surface, "  -> [%2d] %4d,%4d-%4d,%4d-%4d,%4d\n", 0, x1, y1, x2, y2, x3, y3 );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     tri.x1 += data->area.wanted.x;
     tri.y1 += data->area.wanted.y;
     tri.x2 += data->area.wanted.x;
     tri.y2 += data->area.wanted.y;
     tri.x3 += data->area.wanted.x;
     tri.y3 += data->area.wanted.y;

     CoreGraphicsStateClient_FillTriangles( &data->state_client, &tri, 1 );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_FillRectangles( IDirectFBSurface   *thiz,
                                 const DFBRectangle *rects,
                                 unsigned int        num_rects )
{
     unsigned int i;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p, %p [%u] )\n", __FUNCTION__, thiz, rects, num_rects );

     DFB_RECTANGLES_DEBUG_AT( Surface, rects, num_rects );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (!rects || !num_rects)
          return DFB_INVARG;

     if (data->area.wanted.x || data->area.wanted.y) {
          DFBRectangle *local_rects;
          bool          malloced = (num_rects > 256);

          if (malloced)
               local_rects = D_MALLOC( sizeof(DFBRectangle) * num_rects );
          else
               local_rects = alloca( sizeof(DFBRectangle) * num_rects );

          for (i = 0; i < num_rects; i++) {
               local_rects[i].x = rects[i].x + data->area.wanted.x;
               local_rects[i].y = rects[i].y + data->area.wanted.y;
               local_rects[i].w = rects[i].w;
               local_rects[i].h = rects[i].h;
          }

          for (i = 0; i < num_rects; i += 200) {
               CoreGraphicsStateClient_FillRectangles( &data->state_client, &local_rects[i],
                                                       MIN( 200, num_rects - i ) );
          }

          if (malloced)
               D_FREE( local_rects );
     }
     else {
          for (i = 0; i < num_rects; i += 200) {
               CoreGraphicsStateClient_FillRectangles( &data->state_client, &rects[i],
                                                       MIN( 200, num_rects - i ) );
          }
     }

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_FillSpans( IDirectFBSurface *thiz,
                            int               y,
                            const DFBSpan    *spans,
                            unsigned int      num_spans )
{
     DFBSpan *local_spans = alloca( sizeof(DFBSpan) * num_spans );

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (!spans || !num_spans)
          return DFB_INVARG;

     if (data->area.wanted.x || data->area.wanted.y) {
          unsigned int i;

          for (i = 0; i < num_spans; i++) {
               local_spans[i].x = spans[i].x + data->area.wanted.x;
               local_spans[i].w = spans[i].w;
          }
     }
     else
          /* Clipping may modify spans, so we copy them. */
          direct_memcpy( local_spans, spans, sizeof(DFBSpan) * num_spans );

     CoreGraphicsStateClient_FillSpans( &data->state_client, y + data->area.wanted.y, local_spans, num_spans );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_FillTriangles( IDirectFBSurface  *thiz,
                                const DFBTriangle *tris,
                                unsigned int       num_tris )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (!tris || !num_tris)
          return DFB_INVARG;

     if (data->area.wanted.x || data->area.wanted.y) {
          unsigned int  i;
          DFBTriangle  *local_tris;
          bool          malloced = (num_tris > 170);

          if (malloced)
               local_tris = D_MALLOC( sizeof(DFBTriangle) * num_tris );
          else
               local_tris = alloca( sizeof(DFBTriangle) * num_tris );

          for (i = 0; i < num_tris; i++) {
               local_tris[i].x1 = tris[i].x1 + data->area.wanted.x;
               local_tris[i].y1 = tris[i].y1 + data->area.wanted.y;
               local_tris[i].x2 = tris[i].x2 + data->area.wanted.x;
               local_tris[i].y2 = tris[i].y2 + data->area.wanted.y;
               local_tris[i].x3 = tris[i].x3 + data->area.wanted.x;
               local_tris[i].y3 = tris[i].y3 + data->area.wanted.y;
          }

          CoreGraphicsStateClient_FillTriangles( &data->state_client, local_tris, num_tris );

          if (malloced)
               D_FREE( local_tris );
     }
     else
          CoreGraphicsStateClient_FillTriangles( &data->state_client, tris, num_tris );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetFont( IDirectFBSurface *thiz,
                          IDirectFBFont    *font )
{
     DFBResult ret;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p, %p )\n", __FUNCTION__, thiz, font );

     if (data->font != font) {
         if (font) {
              IDirectFBFont_data *font_data;

              ret = font->AddRef( font );
              if (ret)
                   return ret;

              font_data = font->priv;
              if (!font_data)
                   return DFB_DEAD;

              if (font_data)
                   data->encoding = font_data->encoding;
         }

         if (data->font)
              data->font->Release( data->font );

         data->font = font;
     }

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetFont( IDirectFBSurface  *thiz,
                          IDirectFBFont    **ret_interface )
{
     DFBResult ret;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_interface)
          return DFB_INVARG;

     if (!data->font) {
          *ret_interface = NULL;
          return DFB_MISSINGFONT;
     }

     ret = data->font->AddRef( data->font );
     if (ret)
          return ret;

     *ret_interface = data->font;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_DrawString( IDirectFBSurface    *thiz,
                             const char          *text,
                             int                  bytes,
                             int                  x,
                             int                  y,
                             DFBSurfaceTextFlags  flags )
{
     IDirectFBFont_data *font_data;
     unsigned int        layers = 1;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p, %d, %d,%d, 0x%x )\n", __FUNCTION__, thiz, bytes, x, y, flags );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (!data->font)
          return DFB_MISSINGFONT;

     if (!text)
          return DFB_INVARG;

     if (bytes < 0)
          bytes = strlen( text );

     if (bytes == 0)
          return DFB_OK;

     font_data = data->font->priv;
     if (!font_data)
          return DFB_DEAD;

     if (!font_data)
          return DFB_DESTROYED;

     if (core_dfb->shutdown_running)
          return DFB_OK;

     if (flags & DSTF_OUTLINE) {
          if (!(font_data->font->attributes & DFFA_OUTLINED))
               return DFB_UNSUPPORTED;

          layers = 2;
     }

     if (!(flags & DSTF_TOP)) {
          x += font_data->font->ascender * font_data->font->up_unit_x;
          y += font_data->font->ascender * font_data->font->up_unit_y;

          if (flags & DSTF_BOTTOM) {
               x -= font_data->font->descender * font_data->font->up_unit_x;
               y -= font_data->font->descender * font_data->font->up_unit_y;
          }
     }

     if (flags & (DSTF_RIGHT | DSTF_CENTER)) {
          DFBResult    ret;
          int          i, num, kx, ky;
          int          xsize = 0;
          int          ysize = 0;
          unsigned int prev  = 0;
          unsigned int indices[bytes];

          dfb_font_lock( font_data->font );

          /* Decode string to character indices. */
          ret = dfb_font_decode_text( font_data->font, data->encoding, text, bytes, indices, &num );
          if (ret) {
               dfb_font_unlock( font_data->font );
               return ret;
          }

          /* Calculate string width. */
          for (i = 0; i < num; i++) {
               unsigned int   current = indices[i];
               CoreGlyphData *glyph;

               if (dfb_font_get_glyph_data( font_data->font, current, 0, &glyph ) == DFB_OK) {
                    xsize += glyph->xadvance;
                    ysize += glyph->yadvance;

                    if (prev && font_data->font->GetKerning &&
                        font_data->font->GetKerning( font_data->font, prev, current, &kx, &ky ) == DFB_OK) {
                         xsize += kx << 8;
                         ysize += ky << 8;
                    }
               }

               prev = current;
          }

          dfb_font_unlock( font_data->font );

          /* Justify. */
          if (flags & DSTF_RIGHT) {
               x -= xsize >> 8;
               y -= ysize >> 8;
          }
          else if (flags & DSTF_CENTER) {
               x -= xsize >> 9;
               y -= ysize >> 9;
          }
     }

     dfb_gfxcard_drawstring( (const unsigned char*) text, bytes, data->encoding,
                             data->area.wanted.x + x, data->area.wanted.y + y,
                             font_data->font, layers, &data->state_client, flags );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_DrawGlyph( IDirectFBSurface    *thiz,
                            unsigned int         character,
                            int                  x,
                            int                  y,
                            DFBSurfaceTextFlags  flags )
{
     DFBResult           ret;
     int                 l;
     IDirectFBFont_data *font_data;
     CoreGlyphData      *glyph[DFB_FONT_MAX_LAYERS];
     unsigned int        index;
     unsigned int        layers = 1;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p, 0x%x, %d,%d, 0x%x )\n", __FUNCTION__, thiz, character, x, y, flags );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (!data->font)
          return DFB_MISSINGFONT;

     if (!character)
          return DFB_INVARG;

     font_data = data->font->priv;
     if (!font_data)
          return DFB_DEAD;

     if (!font_data)
          return DFB_DESTROYED;

     if (core_dfb->shutdown_running)
          return DFB_OK;

     if (flags & DSTF_OUTLINE) {
          if (!(font_data->font->attributes & DFFA_OUTLINED))
               return DFB_UNSUPPORTED;

          layers = 2;
     }

     dfb_font_lock( font_data->font );

     ret = dfb_font_decode_character( font_data->font, data->encoding, character, &index );
     if (ret) {
          dfb_font_unlock( font_data->font );
          return ret;
     }

     for (l = 0; l < layers; l++) {
          ret = dfb_font_get_glyph_data( font_data->font, index, l, &glyph[l] );
          if (ret) {
               dfb_font_unlock( font_data->font );
               return ret;
          }
     }

     if (!(flags & DSTF_TOP)) {
          x += font_data->font->ascender * font_data->font->up_unit_x;
          y += font_data->font->ascender * font_data->font->up_unit_y;

          if (flags & DSTF_BOTTOM) {
               x -= font_data->font->descender * font_data->font->up_unit_x;
               y -= font_data->font->descender * font_data->font->up_unit_y;
          }
     }

     if (flags & (DSTF_RIGHT | DSTF_CENTER)) {
          if (flags & DSTF_RIGHT) {
               x -= glyph[0]->xadvance;
               y -= glyph[0]->yadvance;
          }
          else if (flags & DSTF_CENTER) {
               x -= glyph[0]->xadvance >> 1;
               y -= glyph[0]->yadvance >> 1;
          }
     }

     dfb_gfxcard_drawglyph( glyph, data->area.wanted.x + x, data->area.wanted.y + y,
                            font_data->font, layers, &data->state_client, flags );

     dfb_font_unlock( font_data->font );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetEncoding( IDirectFBSurface  *thiz,
                              DFBTextEncodingID  encoding )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p, %u )\n", __FUNCTION__, thiz, encoding );

     data->encoding = encoding;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetSubSurface( IDirectFBSurface    *thiz,
                                const DFBRectangle  *rect,
                                IDirectFBSurface   **ret_interface )
{
     DFBResult ret;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!ret_interface)
          return DFB_INVARG;

     direct_mutex_lock( &data->children_lock );

     if (data->children_free) {
          IDirectFBSurface_data *child_data;

          child_data = (IDirectFBSurface_data*) data->children_free;

          direct_list_remove( &data->children_free, &child_data->link );
          direct_list_append( &data->children_data, &child_data->link );

          direct_mutex_unlock( &data->children_lock );

          *ret_interface = child_data->thiz;

          ret = (*ret_interface)->MakeSubSurface( *ret_interface, thiz, rect );
          if (ret) {
               direct_mutex_unlock( &data->children_lock );
               return ret;
          }

          return DFB_OK;
     }

     direct_mutex_unlock( &data->children_lock );

     DIRECT_ALLOCATE_INTERFACE( *ret_interface, IDirectFBSurface );

     if (rect || data->limit_set) {
          DFBRectangle wanted, granted;

          /* Compute wanted rectangle. */
          if (rect) {
               wanted = *rect;

               wanted.x += data->area.wanted.x;
               wanted.y += data->area.wanted.y;

               if (wanted.w <= 0 || wanted.h <= 0) {
                    wanted.w = 0;
                    wanted.h = 0;
               }
          }
          else {
               wanted = data->area.wanted;
          }

          /* Compute granted rectangle. */
          granted = wanted;

          dfb_rectangle_intersect( &granted, &data->area.granted );

          ret = IDirectFBSurface_Construct( *ret_interface, thiz, &wanted, &granted, &data->area.insets, data->surface,
                                            data->caps | DSCAPS_SUBSURFACE, data->core, data->idirectfb );
     }
     else {
          ret = IDirectFBSurface_Construct( *ret_interface, thiz, NULL, NULL, &data->area.insets, data->surface,
                                            data->caps | DSCAPS_SUBSURFACE, data->core, data->idirectfb );
     }

     return ret;
}

static DFBResult
IDirectFBSurface_GetGL( IDirectFBSurface  *thiz,
                        IDirectFBGL      **ret_interface )
{
     DFBResult             ret;
     DirectInterfaceFuncs *funcs = NULL;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (!ret_interface)
          return DFB_INVARG;

     ret = DirectGetInterface( &funcs, "IDirectFBGL", NULL, DirectProbeInterface, thiz );
     if (ret)
          return ret;

     ret = funcs->Allocate( (void**) ret_interface );
     if (ret)
          return ret;

     ret = funcs->Construct( *ret_interface, thiz, data->idirectfb );
     if (ret)
          *ret_interface = NULL;

     return ret;
}

static DFBResult
IDirectFBSurface_Dump( IDirectFBSurface *thiz,
                       const char       *directory,
                       const char       *prefix )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->caps & DSCAPS_SUBSURFACE) {
          D_ONCE( "sub surface dumping not supported" );
          return DFB_UNSUPPORTED;
     }

     if (!directory)
          return DFB_INVARG;

     CoreGraphicsStateClient_Flush( &data->state_client );

     return dfb_surface_dump_buffer2( data->surface, DSBR_FRONT, DSSE_LEFT, directory, prefix );
}

static DFBResult
IDirectFBSurface_DisableAcceleration( IDirectFBSurface    *thiz,
                                      DFBAccelerationMask  mask )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (D_FLAGS_INVALID( mask, DFXL_ALL ))
          return DFB_INVARG;

     data->state.disabled = mask;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_ReleaseSource( IDirectFBSurface *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     dfb_state_set_source( &data->state, NULL );
     dfb_state_set_source_mask( &data->state, NULL );
     dfb_state_set_source2( &data->state, NULL );

     CoreGraphicsStateClient_ReleaseSource( &data->state_client );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetIndexTranslation( IDirectFBSurface *thiz,
                                      const int        *indices,
                                      int               num_indices )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     if (!data->surface)
          return DFB_DESTROYED;

     if (!DFB_PIXELFORMAT_IS_INDEXED( data->surface->config.format ))
          return DFB_UNSUPPORTED;

     if (!indices && num_indices > 0)
          return DFB_INVAREA;

     if (num_indices < 0 || num_indices > 256)
          return DFB_INVARG;

     return dfb_state_set_index_translation( &data->state, indices, num_indices );
}

static DFBResult
IDirectFBSurface_SetRenderOptions( IDirectFBSurface        *thiz,
                                   DFBSurfaceRenderOptions  options )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     dfb_state_set_render_options( &data->state, options );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetMatrix( IDirectFBSurface *thiz,
                            const s32        *matrix )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p, %p )\n", __FUNCTION__, thiz, matrix );

     if (!matrix)
          return DFB_INVARG;

     dfb_state_set_matrix( &data->state, matrix );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetSourceMask( IDirectFBSurface    *thiz,
                                IDirectFBSurface    *mask,
                                int                  x,
                                int                  y,
                                DFBSurfaceMaskFlags  flags )
{
     DFBResult              ret;
     DFBPoint               offset = { x, y };
     IDirectFBSurface_data *mask_data;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p, %p, %d,%d, 0x%04x )\n", __FUNCTION__, thiz, mask, x, y, flags );

     if (!mask || flags & ~DSMF_ALL)
          return DFB_INVARG;

     mask_data = mask->priv;
     if (!mask_data)
          return DFB_DEAD;

     if (!mask_data)
          return DFB_DESTROYED;

     if (!mask_data->surface)
          return DFB_DESTROYED;

     CoreGraphicsStateClient_Flush( &mask_data->state_client );

     ret = dfb_state_set_source_mask( &data->state, mask_data->surface );
     if (ret)
          return ret;

     dfb_state_set_source_mask_vals( &data->state, &offset, flags );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_MakeSubSurface( IDirectFBSurface   *thiz,
                                 IDirectFBSurface   *from,
                                 const DFBRectangle *rect )
{
     DFBRectangle           wanted, granted;
     DFBRectangle           full_rect;
     IDirectFBSurface_data *from_data;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!from)
          return DFB_INVARG;

     from_data = from->priv;
     if (!from_data)
          return DFB_DEAD;

     if (!from_data)
          return DFB_DESTROYED;

     /* Check if CoreSurface is the same. */
     if (from_data->surface != data->surface)
          return DFB_UNSUPPORTED;

     full_rect.x = 0;
     full_rect.y = 0;
     full_rect.w = data->surface->config.size.w;
     full_rect.h = data->surface->config.size.h;

     if (rect || from_data->limit_set) {
          /* Compute wanted rectangle. */
          if (rect) {
               wanted = *rect;

               wanted.x += from_data->area.wanted.x;
               wanted.y += from_data->area.wanted.y;

               if (wanted.w <= 0 || wanted.h <= 0) {
                    wanted.w = 0;
                    wanted.h = 0;
               }
          }
          else {
               wanted = from_data->area.wanted;
          }

          /* Compute granted rectangle. */
          granted = wanted;

          dfb_rectangle_intersect( &granted, &from_data->area.granted );
     }
     else {
          wanted  = full_rect;
          granted = full_rect;
     }

     data->caps |= DSCAPS_SUBSURFACE;

     data->area.wanted  = wanted;
     data->area.granted = granted;
     data->area.current = data->area.granted;
     dfb_rectangle_intersect( &data->area.current, &full_rect );

     data->state.clip.x1 = data->area.current.x;
     data->state.clip.y1 = data->area.current.y;
     data->state.clip.x2 = data->area.current.x + (data->area.current.w ?: 1) - 1;
     data->state.clip.y2 = data->area.current.y + (data->area.current.h ?: 1) - 1;

     data->state.modified |= SMF_CLIP;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_Write( IDirectFBSurface   *thiz,
                        const DFBRectangle *rect,
                        const void         *ptr,
                        int                 pitch )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p, %p, %p [%d] )\n", __FUNCTION__, thiz, rect, ptr, pitch );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (!rect || !ptr || pitch < DFB_BYTES_PER_LINE(data->surface->config.format, rect->w ) )
          return DFB_INVARG;

     D_DEBUG_AT( Surface, "  -> %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS( rect ) );

     return dfb_surface_write_buffer( data->surface, DSBR_BACK, ptr, pitch, rect );
}

static DFBResult
IDirectFBSurface_Read( IDirectFBSurface   *thiz,
                       const DFBRectangle *rect,
                       void               *ptr,
                       int                 pitch )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p, %p, %p [%d] )\n", __FUNCTION__, thiz, rect, ptr, pitch );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (!rect || !ptr || pitch < DFB_BYTES_PER_LINE(data->surface->config.format, rect->w ) )
          return DFB_INVARG;

     D_DEBUG_AT( Surface, "  -> %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS( rect ) );

     return dfb_surface_read_buffer( data->surface, DSBR_FRONT, ptr, pitch, rect );
}

static DFBResult
IDirectFBSurface_SetColors( IDirectFBSurface *thiz,
                            const DFBColorID *ids,
                            const DFBColor   *colors,
                            unsigned int      num )
{
     unsigned int i;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p, %p, %p, %u )\n", __FUNCTION__, thiz, ids, colors, num );

     if (!data->surface)
          return DFB_DESTROYED;

     for (i = 0; i < num; i++) {
          D_DEBUG_AT( Surface, "  -> [%u] id %u = %02x %02x %02x %02x\n",
                      i, ids[i], colors[i].a, colors[i].r, colors[i].g, colors[i].b );

          if (ids[i] >= DFB_COLOR_IDS_MAX)
               return DFB_INVARG;

          data->state.colors[ids[i]] = colors[i];

          if (DFB_PIXELFORMAT_IS_INDEXED( data->surface->config.format ))
               data->state.color_indices[ids[i]] = dfb_palette_search( data->surface->palette,
                                                                       colors[i].r, colors[i].g, colors[i].b,
                                                                       colors[i].a );
     }

     dfb_state_set_color( &data->state, &data->state.colors[0] );
     dfb_state_set_color_index( &data->state, data->state.color_indices[0] );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_BatchBlit2( IDirectFBSurface   *thiz,
                             IDirectFBSurface   *source,
                             IDirectFBSurface   *source2,
                             const DFBRectangle *source_rects,
                             const DFBPoint     *dest_points,
                             const DFBPoint     *source2_points,
                             int                 num )
{
     int                    i, dx, dy, sx, sy, sx2, sy2;
     DFBRectangle          *rects;
     DFBPoint              *points;
     DFBPoint              *points2;
     IDirectFBSurface_data *src_data;
     IDirectFBSurface_data *src2_data;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (!source || !source2 || !source_rects || !dest_points || !source2_points || num < 1)
          return DFB_INVARG;

     src_data = source->priv;

     if (!src_data->area.current.w || !src_data->area.current.h)
          return DFB_INVAREA;

     src2_data = source2->priv;

     if (!src2_data->area.current.w || !src2_data->area.current.h)
          return DFB_INVAREA;

     dx = data->area.wanted.x;
     dy = data->area.wanted.y;

     sx = src_data->area.wanted.x;
     sy = src_data->area.wanted.y;

     sx2 = src2_data->area.wanted.x;
     sy2 = src2_data->area.wanted.y;

     rects   = alloca( sizeof(DFBRectangle) * num );
     points  = alloca( sizeof(DFBPoint) * num );
     points2 = alloca( sizeof(DFBPoint) * num );

     direct_memcpy( rects, source_rects, sizeof(DFBRectangle) * num );
     direct_memcpy( points, dest_points, sizeof(DFBPoint) * num );
     direct_memcpy( points2, source2_points, sizeof(DFBPoint) * num );

     for (i = 0; i < num; i++) {
          DFBRectangle rect2;

          rects[i].x += sx;
          rects[i].y += sy;

          points[i].x += dx;
          points[i].y += dy;

          points2[i].x += sx2;
          points2[i].y += sy2;

          if (!dfb_rectangle_intersect( &rects[i], &src_data->area.current ))
               rects[i].w = rects[i].h = 0;
          else {
               points[i].x += rects[i].x - (source_rects[i].x + sx);
               points[i].y += rects[i].y - (source_rects[i].y + sy);
               points2[i].x += rects[i].x - (source_rects[i].x + sx);
               points2[i].y += rects[i].y - (source_rects[i].y + sy);

               rect2.x = points2[i].x;
               rect2.y = points2[i].y;
               rect2.w = rects[i].w;
               rect2.h = rects[i].h;

               if (!dfb_rectangle_intersect( &rect2, &src2_data->area.current ))
                    rects[i].w = rects[i].h = 0;

               points[i].x += rect2.x - points2[i].x;
               points[i].y += rect2.y - points2[i].y;
               points2[i].x += rect2.x - points2[i].x;
               points2[i].y += rect2.y - points2[i].y;

               rects[i].w = rect2.w;
               rects[i].h = rect2.h;
          }
     }

     CoreGraphicsStateClient_Flush( &src_data->state_client );
     CoreGraphicsStateClient_Flush( &src2_data->state_client );

     dfb_state_set_source( &data->state, src_data->surface );
     dfb_state_set_source2( &data->state, src2_data->surface );

     dfb_state_set_from( &data->state, DSBR_FRONT, src_data->src_eye );

     /* Fetch the source color key from the source if necessary. */
     if (data->state.blittingflags & DSBLIT_SRC_COLORKEY)
          dfb_state_set_src_colorkey( &data->state, src_data->src_key.value );

     CoreGraphicsStateClient_Blit2( &data->state_client, rects, points, points2, num );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetPhysicalAddress( IDirectFBSurface *thiz,
                                     unsigned long    *addr )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     if (!data->surface)
          return DFB_DESTROYED;

     if (!data->locked)
          return DFB_ACCESSDENIED;

     if (!data->lock.phys) {
          /* The surface is probably in a system buffer if there's no physical address. */
          return DFB_UNSUPPORTED;
     }

     if (!addr)
          return DFB_INVARG;

     *addr = data->lock.phys;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_FillTrapezoids( IDirectFBSurface   *thiz,
                                 const DFBTrapezoid *traps,
                                 unsigned int        num_traps )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (!traps || !num_traps)
          return DFB_INVARG;

     if (data->area.wanted.x || data->area.wanted.y) {
          unsigned int  i;
          DFBTrapezoid *local_traps;
          bool          malloced = (num_traps > 170);

          if (malloced)
               local_traps = D_MALLOC( sizeof(DFBTrapezoid) * num_traps );
          else
               local_traps = alloca( sizeof(DFBTrapezoid) * num_traps );

          for (i = 0; i < num_traps; i++) {
               local_traps[i].x1 = traps[i].x1 + data->area.wanted.x;
               local_traps[i].y1 = traps[i].y1 + data->area.wanted.y;
               local_traps[i].w1 = traps[i].w1;
               local_traps[i].x2 = traps[i].x2 + data->area.wanted.x;
               local_traps[i].y2 = traps[i].y2 + data->area.wanted.y;
               local_traps[i].w2 = traps[i].w2;
          }

          CoreGraphicsStateClient_FillTrapezoids( &data->state_client, local_traps, num_traps );

          if (malloced)
               D_FREE( local_traps );
     }
     else
          CoreGraphicsStateClient_FillTrapezoids( &data->state_client, traps, num_traps );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_FillQuadrangles( IDirectFBSurface *thiz,
                                  const DFBPoint   *points,
                                  unsigned int      num_points )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (!points || !num_points)
          return DFB_INVARG;

     if (data->area.wanted.x || data->area.wanted.y) {
          unsigned int  i;
          DFBPoint     *local_points = alloca( sizeof(DFBPoint) * num_points );

          for (i = 0; i < num_points; i++) {
               local_points[i].x = points[i].x + data->area.wanted.x;
               local_points[i].y = points[i].y + data->area.wanted.y;
          }

          CoreGraphicsStateClient_FillQuadrangles( &data->state_client, local_points, num_points );
     }
     else
          CoreGraphicsStateClient_FillQuadrangles( &data->state_client, points, num_points );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetSrcColorKeyExtended( IDirectFBSurface          *thiz,
                                         const DFBColorKeyExtended *colorkey_extended )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     dfb_state_set_src_colorkey_extended( &data->state, colorkey_extended );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetDstColorKeyExtended( IDirectFBSurface          *thiz,
                                         const DFBColorKeyExtended *colorkey_extended )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     dfb_state_set_dst_colorkey_extended( &data->state, colorkey_extended );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_DrawMonoGlyphs( IDirectFBSurface             *thiz,
                                 const void                   *glyphs[],
                                 const DFBMonoGlyphAttributes *attributes,
                                 const DFBPoint               *dest_points,
                                 unsigned int                  num )
{
     int       i, dx, dy;
     DFBPoint *points;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (!glyphs || !attributes || !dest_points || num < 1)
          return DFB_INVARG;

     dx = data->area.wanted.x;
     dy = data->area.wanted.y;

     points = alloca( sizeof(DFBPoint) * num );

     for (i = 0; i < num; i++) {
          points[i].x = dest_points[i].x + dx;
          points[i].y = dest_points[i].y + dy;
     }

     dfb_gfxcard_draw_mono_glyphs( glyphs, attributes, points, num, &data->state );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetSrcColorMatrix( IDirectFBSurface *thiz,
                                    const s32        *matrix )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p, %p )\n", __FUNCTION__, thiz, matrix );

     if (!matrix)
          return DFB_INVARG;

     dfb_state_set_src_colormatrix( &data->state, matrix );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetSrcConvolution( IDirectFBSurface           *thiz,
                                    const DFBConvolutionFilter *filter )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p, %p )\n", __FUNCTION__, thiz, filter );

     if (!filter)
          return DFB_INVARG;

     dfb_state_set_src_convolution( &data->state, filter );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_GetID( IDirectFBSurface *thiz,
                        DFBSurfaceID     *ret_surface_id )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!ret_surface_id)
          return DFB_INVARG;

     *ret_surface_id = data->surface->object.id;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_AllowAccess( IDirectFBSurface *thiz,
                              const char       *executable )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     return CoreDFB_AllowSurface( data->core, data->surface, executable, strlen( executable ) + 1 );
}

static DFBResult
IDirectFBSurface_CreateEventBuffer( IDirectFBSurface      *thiz,
                                    IDirectFBEventBuffer **ret_interface )
{
     IDirectFBEventBuffer *iface;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     DIRECT_ALLOCATE_INTERFACE( iface, IDirectFBEventBuffer );

     IDirectFBEventBuffer_Construct( iface, NULL, NULL );

     IDirectFBEventBuffer_AttachSurface( iface, data->surface );

     *ret_interface = iface;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_AttachEventBuffer( IDirectFBSurface     *thiz,
                                    IDirectFBEventBuffer *buffer )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     IDirectFBEventBuffer_AttachSurface( buffer, data->surface );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_DetachEventBuffer( IDirectFBSurface     *thiz,
                                    IDirectFBEventBuffer *buffer )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     return IDirectFBEventBuffer_DetachSurface( buffer, data->surface );
}

static DFBResult
IDirectFBSurface_BatchStretchBlit( IDirectFBSurface   *thiz,
                                   IDirectFBSurface   *source,
                                   const DFBRectangle *source_rects,
                                   const DFBRectangle *dest_rects,
                                   int                 num )
{
     int                    i, dx, dy, sx, sy;
     DFBRectangle          *srects, *drects;
     IDirectFBSurface_data *src_data;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p, %d )\n", __FUNCTION__, thiz, num );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->locked)
          return DFB_LOCKED;

     if (!source || !source_rects || !dest_rects || num < 1)
          return DFB_INVARG;

     src_data = source->priv;

     if (!src_data->area.current.w || !src_data->area.current.h)
          return DFB_INVAREA;

     dx = data->area.wanted.x;
     dy = data->area.wanted.y;

     sx = src_data->area.wanted.x;
     sy = src_data->area.wanted.y;

     srects = alloca( sizeof(DFBRectangle) * num );
     drects = alloca( sizeof(DFBRectangle) * num );

     direct_memcpy( srects, source_rects, sizeof(DFBRectangle) * num );
     direct_memcpy( drects, dest_rects, sizeof(DFBRectangle) * num );

     for (i = 0; i < num; ++i) {
          DFBRectangle orig_src;

          if (drects[i].w < 1 || drects[i].h < 1) {
               drects[i].w = 0;
               drects[i].h = 0;
               continue;
          }

          drects[i].x += dx;
          drects[i].y += dy;

          if (srects[i].w < 1 || srects[i].h < 1)
               return DFB_INVARG;

          srects[i].x += sx;
          srects[i].y += sy;

          /* Clipping of the source rectangle must be applied to the destination. */
          orig_src = srects[i];

          if (!dfb_rectangle_intersect( &srects[i], &src_data->area.current )) {
               srects[i].w = srects[i].h = 0;
               drects[i].w = drects[i].h = 0;
               continue;
          }

          if (srects[i].x != orig_src.x)
               drects[i].x += (int) ( (srects[i].x - orig_src.x) * (drects[i].w / (float) orig_src.w) + 0.5f);

          if (srects[i].y != orig_src.y)
               drects[i].y += (int) ( (srects[i].y - orig_src.y) * (drects[i].h / (float) orig_src.h) + 0.5f);

          if (srects[i].w != orig_src.w)
               drects[i].w = D_ICEIL( drects[i].w * (srects[i].w / (float) orig_src.w) );

          if (srects[i].h != orig_src.h)
               drects[i].h = D_ICEIL( drects[i].h * (srects[i].h / (float) orig_src.h) );

          D_DEBUG_AT( Surface, "  -> [%2d] %4d,%4d-%4dx%4d <- %4d,%4d-%4dx%4d\n", i,
                      drects[i].x, drects[i].y, drects[i].w, drects[i].h,
                      srects[i].x, srects[i].y, srects[i].w, srects[i].h );
     }

     CoreGraphicsStateClient_Flush( &src_data->state_client );

     dfb_state_set_source( &data->state, src_data->surface );

     dfb_state_set_from( &data->state, DSBR_FRONT, src_data->src_eye );

     /* Fetch the source color key from the source if necessary. */
     if (data->state.blittingflags & DSBLIT_SRC_COLORKEY)
          dfb_state_set_src_colorkey( &data->state, src_data->src_key.value );

     CoreGraphicsStateClient_StretchBlit( &data->state_client, srects, drects, num );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_MakeClient( IDirectFBSurface *thiz )
{
     DFBResult ret;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p [%u] )\n", __FUNCTION__, data->surface, data->surface->object.id );

     if (data->surface_client) {
          D_DEBUG_AT( Surface, "  -> already client!\n" );
          return DFB_BUSY;
     }

     ret = CoreSurface_CreateClient( data->surface, &data->surface_client );
     if (ret)
          return ret;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_FrameAck( IDirectFBSurface *thiz,
                           u32               flip_count )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface_Updates, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface_client)
          return DFB_UNSUPPORTED;

     direct_mutex_lock( &data->surface_client_lock );

     D_DEBUG_AT( Surface_Updates, "  -> surface %p [%u]\n", data->surface, data->surface->object.id );
     D_DEBUG_AT( Surface_Updates, "  -> flip count %u\n", flip_count );

     data->surface_client_flip_count = flip_count;

     CoreSurfaceClient_FrameAck( data->surface_client, flip_count );

     direct_mutex_unlock( &data->surface_client_lock );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_DumpRaw( IDirectFBSurface *thiz,
                          const char       *directory,
                          const char       *prefix )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     if (!data->area.current.w || !data->area.current.h)
          return DFB_INVAREA;

     if (data->caps & DSCAPS_SUBSURFACE) {
          D_ONCE( "sub surface dumping not supported" );
          return DFB_UNSUPPORTED;
     }

     if (!directory)
          return DFB_INVARG;

     return dfb_surface_dump_raw_buffer( data->surface, DSBR_FRONT, directory, prefix );
}

static DFBResult
IDirectFBSurface_GetFrameTime( IDirectFBSurface *thiz,
                               long long        *ret_micros )
{
     long long now;
     long long interval = 0;
     long long max      = 0;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface_Updates, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->surface)
          return DFB_DESTROYED;

     interval = data->surface->frametime_config.interval;

     D_DEBUG_AT( Surface_Updates, "  -> surface interval: %lld\n", interval );

     D_DEBUG_AT( Surface_Updates, "  -> config flags: 0x%08x\n", data->frametime_config.flags );

     if (data->surface->frametime_config.flags & DFTCF_MAX_ADVANCE)
          max = data->surface->frametime_config.max_advance;

     if (data->frametime_config.flags & DFTCF_INTERVAL) {
          interval = data->frametime_config.interval;

          D_DEBUG_AT( Surface_Updates, "  -> local configured interval: %lld\n", interval );
     }

     if (data->frametime_config.flags & DFTCF_MAX_ADVANCE)
          max = data->frametime_config.max_advance;

     if (!interval) {
          interval = dfb_config->screen_frame_interval;

          D_DEBUG_AT( Surface_Updates, "  -> using fallback default interval: %lld\n", interval );
     }

     if (!max)
          max = dfb_config->max_frame_advance;

     data->current_frame_time += interval;

     now = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );

     if (now > data->current_frame_time)
          data->current_frame_time = now;
     else if (max) {
          while (data->current_frame_time - now > max) {
               D_DEBUG_AT( Surface_Updates, "  -> sleeping for %lld us...\n", data->current_frame_time - now - max );

               direct_thread_sleep( data->current_frame_time - now - max );

               now = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );
          }
     }

     D_DEBUG_AT( Surface_Updates, "  -> %lld, %lld ahead\n", data->current_frame_time, data->current_frame_time - now );

     if (ret_micros)
          *ret_micros = data->current_frame_time;

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_SetFrameTimeConfig( IDirectFBSurface         *thiz,
                                     const DFBFrameTimeConfig *config )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface_Updates, "%s( %p )\n", __FUNCTION__, thiz );

     if (config) {
          if (config->flags & DFTCF_INTERVAL)
               D_DEBUG_AT( Surface_Updates, "  -> interval: %lld\n", config->interval );

          if (config->flags & DFTCF_MAX_ADVANCE)
               D_DEBUG_AT( Surface_Updates, "  -> max_advance: %lld\n", config->max_advance );

          data->frametime_config = *config;
     }
     else
          memset( &data->frametime_config, 0, sizeof(data->frametime_config) );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_Allocate( IDirectFBSurface            *thiz,
                           DFBSurfaceBufferRole         role,
                           DFBSurfaceStereoEye          eye,
                           const char                  *key,
                           u64                          handle,
                           IDirectFBSurfaceAllocation **ret_interface )
{
     DFBResult                   ret;
     CoreSurfaceAllocation      *allocation;
     IDirectFBSurfaceAllocation *iface;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p, role %u, eye %u, key '%s', handle 0x%08llx )\n", __FUNCTION__,
                 thiz, role, eye, key, (unsigned long long) handle );

     ret = CoreSurface_Allocate( data->surface, role, eye, key, strlen( key ) + 1, handle, &allocation );
     if (ret) {
          D_DERROR( ret,
                    "IDirectFBSurface: CoreSurface_Allocate( role %u, eye %u, key '%s', handle 0x%08llx ) failed!\n",
                    role, eye, key, (unsigned long long) handle );
          return ret;
     }

     DIRECT_ALLOCATE_INTERFACE( iface, IDirectFBSurfaceAllocation );

     if (iface)
          ret = IDirectFBSurfaceAllocation_Construct( iface, allocation, thiz );
     else
          ret = DFB_NOSYSTEMMEMORY;

     dfb_surface_allocation_unref( allocation );

     *ret_interface = iface;

     return ret;
}

static DFBResult
IDirectFBSurface_GetAllocation( IDirectFBSurface            *thiz,
                                DFBSurfaceBufferRole         role,
                                DFBSurfaceStereoEye          eye,
                                const char                  *key,
                                IDirectFBSurfaceAllocation **ret_interface )
{
     DFBResult                   ret;
     CoreSurfaceAllocation      *allocation;
     IDirectFBSurfaceAllocation *iface;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p, role %u, eye %u, key '%s' )\n", __FUNCTION__, thiz, role, eye, key );

     ret = CoreSurface_GetAllocation( data->surface, role, eye, key, strlen( key ) + 1, &allocation );
     if (ret) {
          D_DERROR( ret, "IDirectFBSurface: CoreSurface_GetAllocation( role %u, eye %u, key '%s' ) failed!\n",
                    role, eye, key );
          return ret;
     }

     DIRECT_ALLOCATE_INTERFACE( iface, IDirectFBSurfaceAllocation );

     if (iface)
          ret = IDirectFBSurfaceAllocation_Construct( iface, allocation, thiz );
     else
          ret = DFB_NOSYSTEMMEMORY;

     dfb_surface_allocation_unref( allocation );

     *ret_interface = iface;

     return ret;
}

static DFBResult
IDirectFBSurface_GetAllocations( IDirectFBSurface            *thiz,
                                 const char                  *key,
                                 unsigned int                 max_num,
                                 unsigned int                *ret_num,
                                 IDirectFBSurfaceAllocation **ret_interface_left,
                                 IDirectFBSurfaceAllocation **ret_interface_right )
{
     DFBResult                   ret;
     int                         i;
     unsigned int                num;
     IDirectFBSurfaceAllocation *left[MAX_SURFACE_BUFFERS];
     IDirectFBSurfaceAllocation *right[MAX_SURFACE_BUFFERS];

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p, key '%s', max %u )\n", __FUNCTION__, thiz, key, max_num );

     if (!key || max_num < 1 || !ret_num || (!ret_interface_left && !ret_interface_right))
          return DFB_INVARG;

     num = data->surface->num_buffers;
     if (num > max_num)
          num = max_num;

     memset( left, 0, num * sizeof(left[0]) );
     memset( right, 0, num * sizeof(right[0]) );

     for (i = 0; i < num; i++) {
          if (ret_interface_left) {
               ret = thiz->GetAllocation( thiz, i, DSSE_LEFT, key, &left[i] );
               if (ret)
                    goto error;
          }

          if (ret_interface_right) {
               ret = thiz->GetAllocation( thiz, i, DSSE_RIGHT, key, &right[i] );
               if (ret)
                    goto error;
          }
     }

     for (i = 0; i < num; i++) {
          if (ret_interface_left)
               ret_interface_left[i] = left[i];

          if (ret_interface_right)
               ret_interface_right[i] = right[i];
     }

     *ret_num = num;

     return DFB_OK;

error:
     for (i = num - 1; i >= 0; i--) {
          if (right[i])
               right[i]->Release( right[i] );

          if (left[i])
               left[i]->Release( left[i] );
     }

     return ret;
}

static DFBResult
IDirectFBSurface_Flush( IDirectFBSurface     *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     CoreGraphicsStateClient_Flush( &data->state_client );

     return DFB_OK;
}

static ReactionResult
IDirectFBSurface_React( const void *msg_data,
                        void       *ctx )
{
     const CoreSurfaceNotification *notification = msg_data;
     IDirectFBSurface              *thiz         = ctx;
     IDirectFBSurface_data         *data         = thiz->priv;

     D_DEBUG_AT( Surface, "%s( %p, %p ) <- surface %p\n", __FUNCTION__, notification, thiz, data->surface );

     if (notification->flags & CSNF_DESTROY) {
          if (data->surface) {
               D_WARN( "surface destroyed" );
               data->surface = NULL;
          }

          return RS_REMOVE;
     }

     if (notification->flags & CSNF_SIZEFORMAT) {
          unsigned int i;
          DFBRectangle rect = { 0, 0, data->surface->config.size.w, data->surface->config.size.h };

          dfb_rectangle_subtract( &rect, &data->area.insets );

          if (data->limit_set) {
               data->area.current = data->area.granted;

               dfb_rectangle_intersect( &data->area.current, &rect );
          }
          else
               data->area.wanted = data->area.granted = data->area.current = rect;

          /* Reset clip. */
          if (data->clip_set)
               thiz->SetClip( thiz, &data->clip_wanted );
          else
               thiz->SetClip( thiz, NULL );

          for (i = 0; i < data->local_buffer_count; i++) {
               if (data->allocations[i]) {
                    dfb_surface_allocation_unref( data->allocations[i] );
                    data->allocations[i] = NULL;
               }
          }

          data->local_buffer_count = data->surface->num_buffers;
     }

     return RS_OK;
}

static ReactionResult
IDirectFBSurface_FrameReact( const void *msg_data,
                             void       *ctx )
{
     const CoreSurfaceNotification *notification = msg_data;
     IDirectFBSurface              *thiz         = ctx;
     IDirectFBSurface_data         *data         = thiz->priv;

     D_DEBUG_AT( Surface_Updates, "%s( %p, %p ) <- surface %p\n", __FUNCTION__, notification, thiz, data->surface );

     if (notification->flags & CSNF_FRAME) {
          direct_mutex_lock( &data->back_buffer_lock );

          D_DEBUG_AT( Surface_Updates, "  -> got frame ack %u\n", notification->flip_count );

          data->frame_ack = notification->flip_count;

          if (data->local_flip_count < notification->flip_count) {
               D_DEBUG_AT( Surface_Updates, "  -> local count (%u) lower than frame ack (%u)\n",
                           data->local_flip_count, notification->flip_count );

               data->local_flip_count = notification->flip_count;
          }

          direct_waitqueue_broadcast( &data->back_buffer_wq );

          direct_mutex_unlock( &data->back_buffer_lock );
     }

     return RS_OK;
}

DFBResult
IDirectFBSurface_Construct( IDirectFBSurface       *thiz,
                            IDirectFBSurface       *parent,
                            DFBRectangle           *wanted,
                            DFBRectangle           *granted,
                            DFBInsets              *insets,
                            CoreSurface            *surface,
                            DFBSurfaceCapabilities  caps,
                            CoreDFB                *core,
                            IDirectFB              *idirectfb )
{
     DFBResult    ret;
     DFBRectangle rect = { 0, 0, surface->config.size.w, surface->config.size.h };

     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBSurface )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (dfb_surface_ref( surface )) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return DFB_FAILURE;
     }

     data->ref                = 1;
     data->thiz               = thiz;
     data->surface            = surface;
     data->caps               = caps | surface->config.caps;
     data->core               = core;
     data->idirectfb          = idirectfb;
     data->frame_ack          = surface->flips;
     data->src_eye            = DSSE_LEFT;
     data->local_flip_count   = surface->flips;
     data->local_buffer_count = surface->num_buffers;

     if (parent) {
          IDirectFBSurface_data *parent_data;

          if (parent->AddRef( parent )) {
               dfb_surface_unref( surface );
               DIRECT_DEALLOCATE_INTERFACE( thiz );
               return DFB_FAILURE;
          }

          parent_data = parent->priv;
          if (!parent_data)
               return DFB_DEAD;

          if (!parent_data)
               return DFB_DESTROYED;

          direct_mutex_lock( &parent_data->children_lock );

          direct_list_append( &parent_data->children_data, &data->link );

          direct_mutex_unlock( &parent_data->children_lock );

          data->parent = parent;
     }

     direct_mutex_init( &data->children_lock );

     direct_waitqueue_init( &data->back_buffer_wq );
     direct_mutex_init( &data->back_buffer_lock );

     direct_mutex_init( &data->surface_client_lock );

     /* The area insets. */
     if (insets) {
          data->area.insets = *insets;
          dfb_rectangle_subtract( &rect, insets );
     }

     /* The area that was requested. */
     if (wanted)
          data->area.wanted = *wanted;
     else
          data->area.wanted = rect;

     /* The area that will never be exceeded. */
     if (granted)
          data->area.granted = *granted;
     else
          data->area.granted = data->area.wanted;

     /* The currently accessible rectangle. */
     data->area.current = data->area.granted;
     dfb_rectangle_intersect( &data->area.current, &rect );

     D_DEBUG_AT( Surface, "  -> wanted  %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS( &data->area.wanted ) );
     D_DEBUG_AT( Surface, "  -> granted %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS( &data->area.granted ) );
     D_DEBUG_AT( Surface, "  -> current %4d,%4d-%4dx%4d\n", DFB_RECTANGLE_VALS( &data->area.current ) );

     /* Whether granted rectangle is meaningful. */
     data->limit_set = (granted != NULL);

     dfb_state_init( &data->state, core );
     dfb_state_set_destination_2( &data->state, surface, data->local_flip_count );

     data->state.clip.x1  = data->area.current.x;
     data->state.clip.y1  = data->area.current.y;
     data->state.clip.x2  = data->area.current.x + (data->area.current.w ?: 1) - 1;
     data->state.clip.y2  = data->area.current.y + (data->area.current.h ?: 1) - 1;
     data->state.modified = SMF_ALL;

     ret = CoreGraphicsStateClient_Init( &data->state_client, &data->state );
     if (ret)
          return ret;

     if (data->surface->config.flags & CSCONF_PREALLOCATED) {
          ret = register_prealloc( data );
          if (ret)
               return ret;
     }

     dfb_surface_attach( surface, IDirectFBSurface_React, thiz, &data->reaction );
     dfb_surface_attach_channel( surface, CSCH_FRAME, IDirectFBSurface_FrameReact, thiz, &data->reaction_frame );

     thiz->AddRef                 = IDirectFBSurface_AddRef;
     thiz->Release                = IDirectFBSurface_Release;
     thiz->GetCapabilities        = IDirectFBSurface_GetCapabilities;
     thiz->GetPosition            = IDirectFBSurface_GetPosition;
     thiz->GetSize                = IDirectFBSurface_GetSize;
     thiz->GetVisibleRectangle    = IDirectFBSurface_GetVisibleRectangle;
     thiz->GetPixelFormat         = IDirectFBSurface_GetPixelFormat;
     thiz->GetColorSpace          = IDirectFBSurface_GetColorSpace;
     thiz->GetAccelerationMask    = IDirectFBSurface_GetAccelerationMask;
     thiz->GetPalette             = IDirectFBSurface_GetPalette;
     thiz->SetPalette             = IDirectFBSurface_SetPalette;
     thiz->SetAlphaRamp           = IDirectFBSurface_SetAlphaRamp;
     thiz->GetStereoEye           = IDirectFBSurface_GetStereoEye;
     thiz->SetStereoEye           = IDirectFBSurface_SetStereoEye;
     thiz->Lock                   = IDirectFBSurface_Lock;
     thiz->GetFramebufferOffset   = IDirectFBSurface_GetFramebufferOffset;
     thiz->Unlock                 = IDirectFBSurface_Unlock;
     thiz->Flip                   = IDirectFBSurface_Flip;
     thiz->FlipStereo             = IDirectFBSurface_FlipStereo;
     thiz->SetField               = IDirectFBSurface_SetField;
     thiz->Clear                  = IDirectFBSurface_Clear;
     thiz->SetClip                = IDirectFBSurface_SetClip;
     thiz->GetClip                = IDirectFBSurface_GetClip;
     thiz->SetColor               = IDirectFBSurface_SetColor;
     thiz->SetColorIndex          = IDirectFBSurface_SetColorIndex;
     thiz->SetSrcBlendFunction    = IDirectFBSurface_SetSrcBlendFunction;
     thiz->SetDstBlendFunction    = IDirectFBSurface_SetDstBlendFunction;
     thiz->SetPorterDuff          = IDirectFBSurface_SetPorterDuff;
     thiz->SetSrcColorKey         = IDirectFBSurface_SetSrcColorKey;
     thiz->SetSrcColorKeyIndex    = IDirectFBSurface_SetSrcColorKeyIndex;
     thiz->SetDstColorKey         = IDirectFBSurface_SetDstColorKey;
     thiz->SetDstColorKeyIndex    = IDirectFBSurface_SetDstColorKeyIndex;
     thiz->SetBlittingFlags       = IDirectFBSurface_SetBlittingFlags;
     thiz->Blit                   = IDirectFBSurface_Blit;
     thiz->TileBlit               = IDirectFBSurface_TileBlit;
     thiz->BatchBlit              = IDirectFBSurface_BatchBlit;
     thiz->StretchBlit            = IDirectFBSurface_StretchBlit;
     thiz->TextureTriangles       = IDirectFBSurface_TextureTriangles;
     thiz->SetDrawingFlags        = IDirectFBSurface_SetDrawingFlags;
     thiz->FillRectangle          = IDirectFBSurface_FillRectangle;
     thiz->DrawRectangle          = IDirectFBSurface_DrawRectangle;
     thiz->DrawLine               = IDirectFBSurface_DrawLine;
     thiz->DrawLines              = IDirectFBSurface_DrawLines;
     thiz->FillTriangle           = IDirectFBSurface_FillTriangle;
     thiz->FillRectangles         = IDirectFBSurface_FillRectangles;
     thiz->FillSpans              = IDirectFBSurface_FillSpans;
     thiz->FillTriangles          = IDirectFBSurface_FillTriangles;
     thiz->SetFont                = IDirectFBSurface_SetFont;
     thiz->GetFont                = IDirectFBSurface_GetFont;
     thiz->DrawString             = IDirectFBSurface_DrawString;
     thiz->DrawGlyph              = IDirectFBSurface_DrawGlyph;
     thiz->SetEncoding            = IDirectFBSurface_SetEncoding;
     thiz->GetSubSurface          = IDirectFBSurface_GetSubSurface;
     thiz->GetGL                  = IDirectFBSurface_GetGL;
     thiz->Dump                   = IDirectFBSurface_Dump;
     thiz->DisableAcceleration    = IDirectFBSurface_DisableAcceleration;
     thiz->ReleaseSource          = IDirectFBSurface_ReleaseSource;
     thiz->SetIndexTranslation    = IDirectFBSurface_SetIndexTranslation;
     thiz->SetRenderOptions       = IDirectFBSurface_SetRenderOptions;
     thiz->SetMatrix              = IDirectFBSurface_SetMatrix;
     thiz->SetSourceMask          = IDirectFBSurface_SetSourceMask;
     thiz->MakeSubSurface         = IDirectFBSurface_MakeSubSurface;
     thiz->Write                  = IDirectFBSurface_Write;
     thiz->Read                   = IDirectFBSurface_Read;
     thiz->SetColors              = IDirectFBSurface_SetColors;
     thiz->BatchBlit2             = IDirectFBSurface_BatchBlit2;
     thiz->GetPhysicalAddress     = IDirectFBSurface_GetPhysicalAddress;
     thiz->FillTrapezoids         = IDirectFBSurface_FillTrapezoids;
     thiz->FillQuadrangles        = IDirectFBSurface_FillQuadrangles;
     thiz->SetSrcColorKeyExtended = IDirectFBSurface_SetSrcColorKeyExtended;
     thiz->SetDstColorKeyExtended = IDirectFBSurface_SetDstColorKeyExtended;
     thiz->DrawMonoGlyphs         = IDirectFBSurface_DrawMonoGlyphs;
     thiz->SetSrcColorMatrix      = IDirectFBSurface_SetSrcColorMatrix;
     thiz->SetSrcConvolution      = IDirectFBSurface_SetSrcConvolution;
     thiz->GetID                  = IDirectFBSurface_GetID;
     thiz->AllowAccess            = IDirectFBSurface_AllowAccess;
     thiz->CreateEventBuffer      = IDirectFBSurface_CreateEventBuffer;
     thiz->AttachEventBuffer      = IDirectFBSurface_AttachEventBuffer;
     thiz->DetachEventBuffer      = IDirectFBSurface_DetachEventBuffer;
     thiz->BatchStretchBlit       = IDirectFBSurface_BatchStretchBlit;
     thiz->MakeClient             = IDirectFBSurface_MakeClient;
     thiz->FrameAck               = IDirectFBSurface_FrameAck;
     thiz->DumpRaw                = IDirectFBSurface_DumpRaw;
     thiz->GetFrameTime           = IDirectFBSurface_GetFrameTime;
     thiz->SetFrameTimeConfig     = IDirectFBSurface_SetFrameTimeConfig;
     thiz->Allocate               = IDirectFBSurface_Allocate;
     thiz->GetAllocation          = IDirectFBSurface_GetAllocation;
     thiz->GetAllocations         = IDirectFBSurface_GetAllocations;
     thiz->Flush                  = IDirectFBSurface_Flush;

     return DFB_OK;
}

void
IDirectFBSurface_StopAll( IDirectFBSurface_data *data )
{
     if (!dfb_config->startstop)
          return;

     if (data->children_data) {
          IDirectFBSurface_data *child;

          direct_mutex_lock( &data->children_lock );

          direct_list_foreach (child, data->children_data) {
               IDirectFBSurface_StopAll( child );
          }

          direct_mutex_unlock( &data->children_lock );
     }

     /* Signal end of sequence of operations. */
     dfb_state_lock( &data->state );
     dfb_state_stop_drawing( &data->state );
     dfb_state_unlock( &data->state );
}

void
IDirectFBSurface_WaitForBackBuffer( IDirectFBSurface_data *data )
{
     D_DEBUG_AT( Surface_Updates, "%s( %p [%u] )\n", __FUNCTION__, data, data->surface->object.id );
     D_DEBUG_AT( Surface_Updates, "  -> surface %u, notify %u\n", data->local_flip_count, data->frame_ack );

     direct_mutex_lock( &data->back_buffer_lock );

     if (data->surface->flips_acked > data->frame_ack)
          data->frame_ack = data->surface->flips_acked;

     while (data->local_flip_count - data->frame_ack >= data->local_buffer_count - 1) {
          D_DEBUG_AT( Surface_Updates, "  -> waiting for back buffer... (surface %u, notify %u)\n",
                      data->local_flip_count, data->frame_ack );

          if (data->local_buffer_count <= 1)
               break;

          direct_waitqueue_wait( &data->back_buffer_wq, &data->back_buffer_lock );
     }

     D_DEBUG_AT( Surface_Updates, "  -> done\n" );

     direct_mutex_unlock( &data->back_buffer_lock );
}

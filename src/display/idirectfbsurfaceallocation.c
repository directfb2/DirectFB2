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

#include <core/CoreSurfaceAllocation.h>
#include <core/surface_allocation.h>
#include <core/surface_buffer.h>
#include <core/surface_pool.h>
#include <display/idirectfbsurfaceallocation.h>

D_DEBUG_DOMAIN( SurfaceAllocation, "IDirectFBSurfaceAllocation", "IDirectFBSurfaceAllocation Interface" );

/**********************************************************************************************************************/

/*
 * private data struct of IDirectFBSurfaceAllocation
 */
typedef struct {
     int                    ref;              /* reference counter */

     CoreSurfaceAllocation *allocation;       /* the allocation object */

     IDirectFBSurface      *idirectfbsurface; /* the surface interface object */

     CoreSurfaceBufferLock  lock;             /* lock for the allocation */
} IDirectFBSurfaceAllocation_data;

/**********************************************************************************************************************/

static void
IDirectFBSurfaceAllocation_Destruct( IDirectFBSurfaceAllocation *thiz )
{
     IDirectFBSurfaceAllocation_data *data = thiz->priv;

     D_DEBUG_AT( SurfaceAllocation, "%s( %p )\n", __FUNCTION__, thiz );

     if (data->lock.allocation) {
          dfb_surface_pool_unlock( data->lock.allocation->pool, data->lock.allocation, &data->lock );

          dfb_surface_buffer_lock_reset( &data->lock );
     }

     if (data->allocation)
          dfb_surface_allocation_unref( data->allocation );

     dfb_surface_buffer_lock_deinit( &data->lock );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DirectResult
IDirectFBSurfaceAllocation_AddRef( IDirectFBSurfaceAllocation *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurfaceAllocation )

     D_DEBUG_AT( SurfaceAllocation, "%s( %p )\n", __FUNCTION__, thiz );

     data->ref++;

     return DFB_OK;
}

static DirectResult
IDirectFBSurfaceAllocation_Release( IDirectFBSurfaceAllocation *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurfaceAllocation )

     D_DEBUG_AT( SurfaceAllocation, "%s( %p )\n", __FUNCTION__, thiz );

     if (--data->ref == 0)
          IDirectFBSurfaceAllocation_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBSurfaceAllocation_GetDescription( IDirectFBSurfaceAllocation *thiz,
                                           DFBSurfaceDescription      *ret_desc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurfaceAllocation )

     D_DEBUG_AT( SurfaceAllocation, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->allocation)
          return DFB_DESTROYED;

     if (!ret_desc)
          return DFB_INVARG;

     ret_desc->flags = DSDESC_HINTS;
     ret_desc->hints = DSHF_NONE;

     if (data->allocation->type & CSTF_LAYER)
          ret_desc->hints |= DSHF_LAYER;

     return DFB_OK;
}

static DFBResult
IDirectFBSurfaceAllocation_GetHandle( IDirectFBSurfaceAllocation *thiz,
                                      u64                        *ret_handle )
{
     DFBResult ret;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurfaceAllocation )

     D_DEBUG_AT( SurfaceAllocation, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->allocation)
          return DFB_DESTROYED;

     if (!ret_handle)
          return DFB_INVARG;

     /* Lock the allocation. */
     if (!data->lock.allocation) {
          ret = dfb_surface_pool_lock( data->allocation->pool, data->allocation, &data->lock );
          if (ret) {
               D_DERROR( ret, "IDirectFBSurfaceAllocation: Locking allocation failed!\n" );
               return ret;
          }
     }

     *ret_handle = (u64)(long) data->lock.handle;

     return DFB_OK;
}

static DFBResult
IDirectFBSurfaceAllocation_GetPitch( IDirectFBSurfaceAllocation *thiz,
                                     int                        *ret_pitch )
{
     DFBResult ret;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurfaceAllocation )

     D_DEBUG_AT( SurfaceAllocation, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->allocation)
          return DFB_DESTROYED;

     if (!ret_pitch)
          return DFB_INVARG;

     /* Lock the allocation. */
     if (!data->lock.allocation) {
          ret = dfb_surface_pool_lock( data->allocation->pool, data->allocation, &data->lock );
          if (ret) {
               D_DERROR( ret, "IDirectFBSurfaceAllocation: Locking allocation failed!\n" );
               return ret;
          }
     }

     *ret_pitch = data->lock.pitch;

     return DFB_OK;
}

static DFBResult
IDirectFBSurfaceAllocation_Updated( IDirectFBSurfaceAllocation *thiz,
                                    const DFBBox               *updates,
                                    unsigned int                num_updates )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurfaceAllocation )

     D_DEBUG_AT( SurfaceAllocation, "%s( %p, updates %p, num %u )\n", __FUNCTION__, thiz, updates, num_updates );

     if (!data->allocation)
          return DFB_DESTROYED;

     if (!updates && num_updates > 0)
          return DFB_INVARG;

     return CoreSurfaceAllocation_Updated( data->allocation, updates, num_updates );
}

DFBResult
IDirectFBSurfaceAllocation_Construct( IDirectFBSurfaceAllocation *thiz,
                                      CoreSurfaceAllocation      *allocation,
                                      IDirectFBSurface           *idirectfbsurface )
{
     DFBResult ret;

     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBSurfaceAllocation )

     D_DEBUG_AT( SurfaceAllocation, "%s( %p )\n", __FUNCTION__, thiz );

     ret = dfb_surface_allocation_ref( allocation );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     data->ref              = 1;
     data->allocation       = allocation;
     data->idirectfbsurface = idirectfbsurface;

     dfb_surface_buffer_lock_init( &data->lock, CSAID_CPU, CSAF_READ | CSAF_WRITE );

     thiz->AddRef         = IDirectFBSurfaceAllocation_AddRef;
     thiz->Release        = IDirectFBSurfaceAllocation_Release;
     thiz->GetDescription = IDirectFBSurfaceAllocation_GetDescription;
     thiz->GetHandle      = IDirectFBSurfaceAllocation_GetHandle;
     thiz->GetPitch       = IDirectFBSurfaceAllocation_GetPitch;
     thiz->Updated        = IDirectFBSurfaceAllocation_Updated;

     return DFB_OK;
}

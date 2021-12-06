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

#include <core/CoreSurface.h>
#include <core/core.h>
#include <core/gfxcard.h>
#include <core/palette.h>
#include <core/surface_allocation.h>
#include <core/surface_buffer.h>
#include <core/surface_client.h>
#include <core/surface_pool.h>
#include <gfx/util.h>

D_DEBUG_DOMAIN( DirectFB_CoreSurface, "DirectFB/CoreSurface", "DirectFB CoreSurface" );

/**********************************************************************************************************************/

DFBResult
ISurface_Real__SetConfig( CoreSurface             *obj,
                          const CoreSurfaceConfig *config )
{
     D_DEBUG_AT( DirectFB_CoreSurface, "%s( %p )\n", __FUNCTION__, obj );

     D_ASSERT( config != NULL );

     return dfb_surface_reconfig( obj, config );
}

DFBResult
ISurface_Real__GetPalette( CoreSurface  *obj,
                           CorePalette **ret_palette )
{
     DFBResult ret;

     D_DEBUG_AT( DirectFB_CoreSurface, "%s( %p )\n", __FUNCTION__, obj );

     D_ASSERT( ret_palette != NULL );

     if (!obj->palette)
          return DFB_UNSUPPORTED;

     ret = dfb_palette_ref( obj->palette );
     if (ret)
          return ret;

     *ret_palette = obj->palette;

     return DFB_OK;
}

DFBResult
ISurface_Real__SetPalette( CoreSurface *obj,
                           CorePalette *palette )
{
     D_DEBUG_AT( DirectFB_CoreSurface, "%s( %p )\n", __FUNCTION__, obj );

     D_ASSERT( palette != NULL );

     return dfb_surface_set_palette( obj, palette );
}

DFBResult
ISurface_Real__SetAlphaRamp( CoreSurface *obj,
                             u8           a0,
                             u8           a1,
                             u8           a2,
                             u8           a3 )
{
     D_DEBUG_AT( DirectFB_CoreSurface, "%s( %p )\n", __FUNCTION__, obj );

     return dfb_surface_set_alpha_ramp( obj, a0, a1, a2, a3 );
}

static void
manage_interlocks( CoreSurfaceAllocation  *allocation,
                   CoreSurfaceAccessorID   accessor,
                   CoreSurfaceAccessFlags  access )
{
     int locks;

     locks = dfb_surface_allocation_locks( allocation );

     /* Software read/write access. */
     if (accessor != CSAID_GPU) {
          /* If hardware has written or is writing. */
          if (allocation->accessed[CSAID_GPU] & CSAF_WRITE) {
               /* Wait for the operation to finish. */
               dfb_gfxcard_wait_serial( &allocation->gfx_serial );

               /* Software read access after hardware write requires flush of the (bus) read cache. */
               dfb_gfxcard_flush_read_cache();

               if (!locks) {
                    /* Clear hardware write access. */
                    allocation->accessed[CSAID_GPU] &= ~CSAF_WRITE;

                    /* Clear hardware read access (to avoid syncing twice). */
                    allocation->accessed[CSAID_GPU] &= ~CSAF_READ;
               }
          }

          /* Software write access. */
          if (access & CSAF_WRITE) {
               /* If hardware has (to) read... */
               if (allocation->accessed[CSAID_GPU] & CSAF_READ) {
                    /* Wait for the operation to finish. */
                    dfb_gfxcard_wait_serial( &allocation->gfx_serial );

                    /* Clear hardware read access. */
                    if (!locks)
                         allocation->accessed[CSAID_GPU] &= ~CSAF_READ;
               }
          }
     }

     /* Hardware read or write access. */
     if (accessor == CSAID_GPU && access & (CSAF_READ | CSAF_WRITE)) {
          /* If software has read or written before. */
          if (allocation->accessed[CSAID_CPU] & (CSAF_READ | CSAF_WRITE)) {
               /* Flush texture cache. */
               dfb_gfxcard_flush_texture_cache();

               /* Clear software read and write access. */
               if (!locks)
                    allocation->accessed[CSAID_CPU] &= ~(CSAF_READ | CSAF_WRITE);
          }
     }

     allocation->accessed[accessor] |= access;
}

DFBResult
ISurface_Real__PreLockBuffer( CoreSurface             *obj,
                              CoreSurfaceBuffer       *buffer,
                              CoreSurfaceAccessorID    accessor,
                              CoreSurfaceAccessFlags   access,
                              CoreSurfaceAllocation  **ret_allocation )
{
     DFBResult              ret;
     CoreSurfaceAllocation *allocation;
     bool                   allocated = false;

     D_DEBUG_AT( DirectFB_CoreSurface, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     dfb_surface_lock( obj );

     if (obj->state & CSSF_DESTROYED) {
          dfb_surface_unlock( obj );
          return DFB_DESTROYED;
     }

     if (!buffer->surface) {
          dfb_surface_unlock( obj );
          return DFB_BUFFEREMPTY;
     }

     /* Look for allocation with proper access. */
     allocation = dfb_surface_buffer_find_allocation( buffer, accessor, access, true );
     if (!allocation) {
          /* If no allocation exists, create one. */
          ret = dfb_surface_pools_allocate( buffer, accessor, access, &allocation );
          if (ret) {
               if (ret != DFB_NOVIDEOMEMORY && ret != DFB_UNSUPPORTED)
                    D_DERROR( ret, "DirectFB/CoreSurface: Buffer allocation failed!\n" );

               goto out;
          }

          allocated = true;
     }

     CORE_SURFACE_ALLOCATION_ASSERT( allocation );

     /* Synchronize with other allocations. */
     ret = dfb_surface_allocation_update( allocation, access );
     if (ret) {
          /* Destroy if newly created. */
          if (allocated)
               dfb_surface_allocation_decouple( allocation );
          goto out;
     }

     ret = dfb_surface_pool_prelock( allocation->pool, allocation, accessor, access );
     if (ret) {
          /* Destroy if newly created. */
          if (allocated)
               dfb_surface_allocation_decouple( allocation );
          goto out;
     }

     manage_interlocks( allocation, accessor, access );

     dfb_surface_allocation_ref( allocation );

     *ret_allocation = allocation;

out:
     dfb_surface_unlock( obj );

     return ret;
}

DFBResult
ISurface_Real__PreLockBuffer2( CoreSurface             *obj,
                               DFBSurfaceBufferRole     role,
                               DFBSurfaceStereoEye      eye,
                               CoreSurfaceAccessorID    accessor,
                               CoreSurfaceAccessFlags   access,
                               DFBBoolean               lock,
                               CoreSurfaceAllocation  **ret_allocation )
{
     DFBResult              ret;
     CoreSurfaceBuffer     *buffer;
     CoreSurfaceAllocation *allocation;
     bool                   allocated  = false;

     D_DEBUG_AT( DirectFB_CoreSurface, "%s( %p, role %u, eye %u, accessor 0x%02x, access 0x%02x, %slock )\n",
                 __FUNCTION__, obj, role, eye, accessor, access, lock ? "" : "no " );

     ret = dfb_surface_lock( obj );
     if (ret)
          return ret;

     if (obj->state & CSSF_DESTROYED) {
          dfb_surface_unlock( obj );
          return DFB_DESTROYED;
     }

     if (obj->num_buffers < 1) {
          dfb_surface_unlock( obj );
          return DFB_BUFFEREMPTY;
     }

     buffer = dfb_surface_get_buffer2( obj, role, eye );

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     D_DEBUG_AT( DirectFB_CoreSurface, "  -> buffer %p\n", buffer );

     if (!lock && access & CSAF_READ) {
          if (fusion_vector_is_empty( &buffer->allocs )) {
               dfb_surface_unlock( obj );
               return DFB_NOALLOCATION;
          }
     }

     /* Look for allocation with proper access. */
     allocation = dfb_surface_buffer_find_allocation( buffer, accessor, access, lock );
     if (!allocation) {
          /* If no allocation exists, create one. */
          ret = dfb_surface_pools_allocate( buffer, accessor, access, &allocation );
          if (ret) {
               if (ret != DFB_NOVIDEOMEMORY && ret != DFB_UNSUPPORTED)
                    D_DERROR( ret, "DirectFB/CoreSurface: Buffer allocation failed!\n" );

               goto out;
          }

          allocated = true;
     }

     CORE_SURFACE_ALLOCATION_ASSERT( allocation );

     /* Synchronize with other allocations. */
     ret = dfb_surface_allocation_update( allocation, access );
     if (ret) {
          /* Destroy if newly created. */
          if (allocated)
               dfb_surface_allocation_decouple( allocation );
          goto out;
     }

     if (!lock) {
          if (access & CSAF_WRITE) {
               if (!(allocation->pool->desc.caps & CSPCAPS_WRITE))
                    lock = DFB_TRUE;
          }
          else if (access & CSAF_READ) {
               if (!(allocation->pool->desc.caps & CSPCAPS_READ))
                    lock = DFB_TRUE;
          }
     }

     if (lock) {
          ret = dfb_surface_pool_prelock( allocation->pool, allocation, accessor, access );
          if (ret) {
               /* Destroy if newly created. */
               if (allocated)
                    dfb_surface_allocation_decouple( allocation );
               goto out;
          }

          manage_interlocks( allocation, accessor, access );
     }

     dfb_surface_allocation_ref( allocation );

     *ret_allocation = allocation;

out:
     dfb_surface_unlock( obj );

     return ret;
}

DFBResult
ISurface_Real__PreLockBuffer3( CoreSurface             *obj,
                               DFBSurfaceBufferRole     role,
                               u32                      flip_count,
                               DFBSurfaceStereoEye      eye,
                               CoreSurfaceAccessorID    accessor,
                               CoreSurfaceAccessFlags   access,
                               DFBBoolean               lock,
                               CoreSurfaceAllocation  **ret_allocation )
{
     DFBResult              ret;
     CoreSurfaceBuffer     *buffer;
     CoreSurfaceAllocation *allocation;
     bool                   allocated  = false;

     D_DEBUG_AT( DirectFB_CoreSurface, "%s( %p, role %u, count %u, eye %u, accessor 0x%02x, access 0x%02x, %slock )\n",
                 __FUNCTION__, obj, role, flip_count, eye, accessor, access, lock ? "" : "no " );

     ret = dfb_surface_lock( obj );
     if (ret)
          return ret;

     if (obj->num_buffers < 1) {
          dfb_surface_unlock( obj );
          return DFB_BUFFEREMPTY;
     }

     buffer = dfb_surface_get_buffer3( obj, role, eye, flip_count );

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     D_DEBUG_AT( DirectFB_CoreSurface, "  -> buffer %p\n", buffer );

     if (!lock && access & CSAF_READ) {
          if (fusion_vector_is_empty( &buffer->allocs )) {
               dfb_surface_unlock( obj );
               return DFB_NOALLOCATION;
          }
     }

     /* Look for allocation with proper access. */
     allocation = dfb_surface_buffer_find_allocation( buffer, accessor, access, lock );
     if (!allocation) {
          /* If no allocation exists, create one. */
          ret = dfb_surface_pools_allocate( buffer, accessor, access, &allocation );
          if (ret) {
               if (ret != DFB_NOVIDEOMEMORY && ret != DFB_UNSUPPORTED)
                    D_DERROR( ret, "DirectFB/CoreSurface: Buffer allocation failed!\n" );

               goto out;
          }

          allocated = true;
     }

     CORE_SURFACE_ALLOCATION_ASSERT( allocation );

     D_DEBUG_AT( DirectFB_CoreSurface, "  -> allocation %p\n", allocation );

     /* Synchronize with other allocations. */
     ret = dfb_surface_allocation_update( allocation, access );
     if (ret) {
          /* Destroy if newly created. */
          if (allocated)
               dfb_surface_allocation_decouple( allocation );
          goto out;
     }

     if (!lock) {
          if (access & CSAF_WRITE) {
               if (!(allocation->pool->desc.caps & CSPCAPS_WRITE))
                    lock = DFB_TRUE;
          }
          else if (access & CSAF_READ) {
               if (!(allocation->pool->desc.caps & CSPCAPS_READ))
                    lock = DFB_TRUE;
          }
     }

     if (lock) {
          ret = dfb_surface_pool_prelock( allocation->pool, allocation, accessor, access );
          if (ret) {
               /* Destroy if newly created. */
               if (allocated)
                    dfb_surface_allocation_decouple( allocation );
               goto out;
          }

          manage_interlocks( allocation, accessor, access );
     }

     dfb_surface_allocation_ref( allocation );

     *ret_allocation = allocation;

out:
     dfb_surface_unlock( obj );

     return ret;
}

DFBResult
ISurface_Real__DispatchUpdate( CoreSurface         *obj,
                               DFBBoolean           swap,
                               const DFBRegion     *left,
                               const DFBRegion     *right,
                               DFBSurfaceFlipFlags  flags,
                               s64                  timestamp,
                               u32                  flip_count )
{
     DFBResult ret = DFB_OK;
     DFBRegion l, r;

     D_DEBUG_AT( DirectFB_CoreSurface, "%s( %p, timestamp %lld, flip_count %u )\n", __FUNCTION__,
                 obj, (long long) timestamp, flip_count );

     dfb_surface_lock( obj );

     if (left)
          l = *left;
     else {
          l.x1 = 0;
          l.y1 = 0;
          l.x2 = obj->config.size.w - 1;
          l.y2 = obj->config.size.h - 1;
     }

     if (right)
          r = *right;
     else
          r = l;

     if (!(flags & DSFLIP_UPDATE))
          obj->flips = flip_count;

     dfb_surface_dispatch_update( obj, &l, &r, timestamp, flags );

     dfb_surface_unlock( obj );

     return ret;
}

DFBResult
ISurface_Real__Flip2( CoreSurface         *obj,
                      DFBBoolean           swap,
                      const DFBRegion     *left,
                      const DFBRegion     *right,
                      DFBSurfaceFlipFlags  flags,
                      s64                  timestamp )
{
     DFBResult ret = DFB_OK;
     DFBRegion l, r;

     D_DEBUG_AT( DirectFB_CoreSurface, "%s( %p, timestamp %lld )\n", __FUNCTION__, obj, (long long) timestamp );

     dfb_surface_lock( obj );

     if (left)
          l = *left;
     else {
          l.x1 = 0;
          l.y1 = 0;
          l.x2 = obj->config.size.w - 1;
          l.y2 = obj->config.size.h - 1;
     }

     if (right)
          r = *right;
     else
          r = l;

     if (obj->config.caps & DSCAPS_FLIPPING) {
          if (obj->config.caps & DSCAPS_STEREO) {
               if ((flags & DSFLIP_SWAP) ||
                   (!(flags & DSFLIP_BLIT)         &&
                    l.x1 == 0 && l.y1 == 0         &&
                    l.x2 == obj->config.size.w - 1 &&
                    l.y2 == obj->config.size.h - 1 &&
                    r.x1 == 0 && r.y1 == 0         &&
                    r.x2 == obj->config.size.w - 1 &&
                    r.y2 == obj->config.size.h - 1)) {
                    ret = dfb_surface_flip_buffers( obj, swap );
                    if (ret)
                        goto out;
               }
               else {
                    if (left)
                         dfb_gfx_copy_regions_client( obj, DSBR_BACK, DSSE_LEFT, obj, DSBR_FRONT, DSSE_LEFT, &l,
                                                      1, 0, 0, NULL );
                    if (right)
                         dfb_gfx_copy_regions_client( obj, DSBR_BACK, DSSE_RIGHT, obj, DSBR_FRONT, DSSE_RIGHT, &r,
                                                      1, 0, 0, NULL );
               }
          }
          else {
               if ((flags & DSFLIP_SWAP) ||
                   (!(flags & DSFLIP_BLIT)         &&
                    l.x1 == 0 && l.y1 == 0         &&
                    l.x2 == obj->config.size.w - 1 &&
                    l.y2 == obj->config.size.h - 1)) {
                    ret = dfb_surface_flip_buffers( obj, swap );
                    if (ret)
                        goto out;
               }
               else {
                    dfb_gfx_copy_regions_client( obj, DSBR_BACK, DSSE_LEFT, obj, DSBR_FRONT, DSSE_LEFT, &l,
                                                 1, 0, 0, NULL );
               }
          }
     }

     dfb_surface_dispatch_update( obj, &l, &r, timestamp, flags );

out:
     dfb_surface_unlock( obj );

     return ret;
}

DFBResult
ISurface_Real__SetField( CoreSurface *obj,
                         s32          field )
{
     D_DEBUG_AT( DirectFB_CoreSurface, "%s( %p )\n", __FUNCTION__, obj );

     return dfb_surface_set_field( obj, field );
}

DFBResult
ISurface_Real__CreateClient( CoreSurface        *obj,
                             CoreSurfaceClient **ret_client )
{
     D_DEBUG_AT( DirectFB_CoreSurface, "%s( %p )\n", __FUNCTION__, obj );

     D_ASSERT( ret_client != NULL );

     return dfb_surface_client_create( core_dfb, obj, ret_client );
}

DFBResult
ISurface_Real__Allocate( CoreSurface            *obj,
                         DFBSurfaceBufferRole    role,
                         DFBSurfaceStereoEye     eye,
                         const char             *key,
                         u32                     key_len,
                         u64                     handle,
                         CoreSurfaceAllocation **ret_allocation )
{
     DFBResult              ret;
     CoreSurfaceBuffer     *buffer;
     CoreSurfaceAllocation *allocation;

     D_ASSERT( key != NULL );
     D_ASSERT( key_len > 0 );
     D_ASSERT( key[key_len-1] == 0 );
     D_ASSERT( ret_allocation != NULL );

     D_DEBUG_AT( DirectFB_CoreSurface, "%s( %p, role %u, eye %u, key '%s', handle 0x%08llx )\n", __FUNCTION__,
                 obj, role, eye, key, (unsigned long long) handle );

     ret = dfb_surface_lock( obj );
     if (ret)
          return ret;

     if (obj->num_buffers == 0) {
          ret = DFB_NOBUFFER;
          goto out;
     }

     buffer = dfb_surface_get_buffer3( obj, role, eye, obj->flips );

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     ret = dfb_surface_pools_allocate_key( buffer, key, handle, &allocation );
     if (ret)
          goto out;

     CORE_SURFACE_ALLOCATION_ASSERT( allocation );

     dfb_surface_allocation_update( allocation, CSAF_WRITE );

     ret = dfb_surface_allocation_ref( allocation );
     if (ret)
          goto out;

     *ret_allocation = allocation;

out:
     dfb_surface_unlock( obj );

     return ret;
}

DFBResult
ISurface_Real__GetAllocation( CoreSurface            *obj,
                              DFBSurfaceBufferRole    role,
                              DFBSurfaceStereoEye     eye,
                              const char             *key,
                              u32                     key_len,
                              CoreSurfaceAllocation **ret_allocation )
{
     DFBResult              ret;
     CoreSurfaceBuffer     *buffer;
     CoreSurfaceAllocation *allocation;

     D_ASSERT( key != NULL );
     D_ASSERT( key_len > 0 );
     D_ASSERT( key[key_len-1] == 0 );
     D_ASSERT( ret_allocation != NULL );

     D_DEBUG_AT( DirectFB_CoreSurface, "%s( %p, role %u, eye %u, key '%s' )\n", __FUNCTION__, obj, role, eye, key );

     if (eye != DSSE_LEFT && eye != DSSE_RIGHT)
          return DFB_INVARG;

     ret = dfb_surface_lock( obj );
     if (ret)
          return ret;

     if (obj->num_buffers == 0) {
          ret = DFB_NOBUFFER;
          goto out;
     }

     if (role > obj->num_buffers - 1) {
          ret = DFB_LIMITEXCEEDED;
          goto out;
     }

     if (eye == DSSE_RIGHT && !(obj->config.caps & DSCAPS_STEREO)) {
          ret = DFB_INVAREA;
          goto out;
     }

     buffer = dfb_surface_get_buffer3( obj, role, eye, obj->flips );

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     allocation = dfb_surface_buffer_find_allocation_key( buffer, key );
     if (!allocation) {
          ret = DFB_ITEMNOTFOUND;
          goto out;
     }

     CORE_SURFACE_ALLOCATION_ASSERT( allocation );

     dfb_surface_allocation_update( allocation, CSAF_WRITE );

     ret = dfb_surface_allocation_ref( allocation );
     if (ret)
          goto out;

     *ret_allocation = allocation;

out:
     dfb_surface_unlock( obj );

     return ret;
}

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
#include <core/palette.h>
#include <core/surface_allocation.h>
#include <core/surface_buffer.h>
#include <core/surface_pool.h>
#include <direct/filesystem.h>
#include <directfb_util.h>
#include <gfx/convert.h>

D_DEBUG_DOMAIN( Core_SurfBuffer, "Core/SurfBuffer", "DirectFB Core Surface Buffer" );

/**********************************************************************************************************************/

static void
surface_buffer_destructor( FusionObject *object,
                           bool          zombie,
                           void         *ctx )
{
     CoreSurfaceAllocation *allocation;
     unsigned int           i;
     CoreSurfaceBuffer     *buffer = (CoreSurfaceBuffer*) object;

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     D_DEBUG_AT( Core_SurfBuffer, "Destroying buffer %p (%dx%d%s)\n",
                 buffer, buffer->config.size.w, buffer->config.size.h, zombie ? " ZOMBIE" : "" );

     D_DEBUG_AT( Core_SurfBuffer, "  -> allocs %d\n", buffer->allocs.count );

     if (buffer->surface)
          dfb_surface_lock( buffer->surface );

     fusion_vector_foreach_reverse (allocation, i, buffer->allocs) {
          CORE_SURFACE_ALLOCATION_ASSERT( allocation );

          dfb_surface_allocation_decouple( allocation );
     }

     if (buffer->surface)
          dfb_surface_unlock( buffer->surface );

     fusion_vector_destroy( &buffer->allocs );

     direct_serial_deinit( &buffer->serial );

     D_MAGIC_CLEAR( buffer );

     /* Destroy the object. */
     fusion_object_destroy( object );
}

FusionObjectPool *
dfb_surface_buffer_pool_create( const FusionWorld *world )
{
     FusionObjectPool *pool;

     pool = fusion_object_pool_create( "Surface Buffer Pool",
                                       sizeof(CoreSurfaceBuffer), sizeof(CoreSurfaceBufferNotification),
                                       surface_buffer_destructor, NULL, world );

     return pool;
}

/**********************************************************************************************************************/

DFBResult
dfb_surface_buffer_create( CoreDFB                 *core,
                           CoreSurface             *surface,
                           CoreSurfaceBufferFlags   flags,
                           int                      index,
                           CoreSurfaceBuffer      **ret_buffer )
{
     DFBResult          ret;
     CoreSurfaceBuffer *buffer;

     D_MAGIC_ASSERT( surface, CoreSurface );
     D_FLAGS_ASSERT( flags, CSBF_ALL );
     D_ASSERT( ret_buffer != NULL );

     D_DEBUG_AT( Core_SurfBuffer, "%s( %s )\n", __FUNCTION__, dfb_pixelformat_name( surface->config.format ) );

     /* Create the buffer object. */
     buffer = dfb_core_create_surface_buffer( core );
     if (!buffer)
          return DFB_FUSION;

     direct_serial_init( &buffer->serial );
     direct_serial_increase( &buffer->serial );

     buffer->surface     = surface;
     buffer->flags       = flags;
     buffer->config      = surface->config;
     buffer->type        = surface->type;
     buffer->resource_id = surface->resource_id;
     buffer->index       = index;

     if (buffer->config.caps & DSCAPS_VIDEOONLY)
          buffer->policy = CSP_VIDEOONLY;
     else if (buffer->config.caps & DSCAPS_SYSTEMONLY)
          buffer->policy = CSP_SYSTEMONLY;
     else
          buffer->policy = CSP_VIDEOLOW;

     fusion_vector_init( &buffer->allocs, 2, buffer->surface->shmpool );

     fusion_object_set_lock( &buffer->object, &buffer->surface->lock );

     fusion_ref_add_permissions( &buffer->object.ref, 0, FUSION_REF_PERMIT_REF_UNREF_LOCAL );

     D_MAGIC_SET( buffer, CoreSurfaceBuffer );

     if (buffer->type & CSTF_PREALLOCATED) {
          CoreSurfacePool       *pool;
          CoreSurfaceAllocation *allocation;

          ret = dfb_surface_pools_lookup( buffer->config.preallocated_pool_id, &pool );
          if (ret) {
               fusion_object_destroy( &buffer->object );
               return ret;
          }

          ret = dfb_surface_pool_allocate( pool, buffer, NULL, 0, &allocation );
          if (ret) {
               fusion_object_destroy( &buffer->object );
               return ret;
          }

          dfb_surface_allocation_update( allocation, CSAF_WRITE );
     }

     /* Activate the object. */
     fusion_object_activate( &buffer->object );

     /* Return the new buffer. */
     *ret_buffer = buffer;

     D_DEBUG_AT( Core_SurfBuffer, "  -> %p\n", buffer );

     return DFB_OK;
}

DFBResult
dfb_surface_buffer_decouple( CoreSurfaceBuffer *buffer )
{
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     D_DEBUG_AT( Core_SurfBuffer, "%s( %p )\n", __FUNCTION__, buffer );

     buffer->flags |= CSBF_DECOUPLE;

     if (!buffer->busy) {
          dfb_surface_buffer_deallocate( buffer );

          buffer->surface = NULL;

          dfb_surface_buffer_unlink( &buffer );
     }

     return DFB_OK;
}

DFBResult
dfb_surface_buffer_deallocate( CoreSurfaceBuffer *buffer )
{
     CoreSurfaceAllocation *allocation;
     int                    i;

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     D_DEBUG_AT( Core_SurfBuffer, "%s( %p ) <- %dx%d\n", __FUNCTION__,
                 buffer, buffer->config.size.w, buffer->config.size.h );

     fusion_vector_foreach_reverse (allocation, i, buffer->allocs) {
           CORE_SURFACE_ALLOCATION_ASSERT( allocation );

           dfb_surface_allocation_decouple( allocation );
     }

     return DFB_OK;
}

CoreSurfaceAllocation *
dfb_surface_buffer_find_allocation( CoreSurfaceBuffer      *buffer,
                                    CoreSurfaceAccessorID   accessor,
                                    CoreSurfaceAccessFlags  flags,
                                    bool                    lock )
{
     int                    i;
     CoreSurfaceAllocation *allocation;
     CoreSurfaceAllocation *uptodate = NULL;
     CoreSurfaceAllocation *outdated = NULL;

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_MAGIC_ASSERT( buffer->surface, CoreSurface );

     D_DEBUG_AT( Core_SurfBuffer, "%s( %p )\n", __FUNCTION__, buffer );

     FUSION_SKIRMISH_ASSERT( &buffer->surface->lock );

     /* For preallocated surfaces, when the client specified DSCAPS_STATIC_ALLOC, it is forced to always get the same
        preallocated buffer again on each lock. */
     if (buffer->type & CSTF_PREALLOCATED && buffer->config.caps & DSCAPS_STATIC_ALLOC) {
          if (buffer->surface->object.identity == Core_GetIdentity()) {
               D_DEBUG_AT( Core_SurfBuffer, "  -> DSCAPS_STATIC_ALLOC, returning preallocated buffer\n" );

               D_ASSERT( buffer->allocs.count > 0 );

               allocation = buffer->allocs.elements[0];

               D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
               D_ASSERT( allocation->flags & CSALF_PREALLOCATED );

               /* Return if allocation has required flags. */
               if (D_FLAGS_ARE_SET( allocation->access[accessor], flags ))
                    return allocation;
          }
     }

     /* Prefer allocations which are up to date. */
     fusion_vector_foreach (allocation, i, buffer->allocs) {
          if (lock && allocation->flags & CSALF_PREALLOCATED) {
               if (!(allocation->access[accessor] & CSAF_SHARED)) {
                    D_DEBUG_AT( Core_SurfBuffer,
                                "  -> non-shared preallocated buffer, surface identity %lu, core identity %lu\n",
                                buffer->surface->object.identity, Core_GetIdentity() );

                    /* If this is a non-shared preallocated allocation and the lock is not for the creator, we need to
                       skip it and possibly allocate/update in a different pool. */
                    if (buffer->surface->object.identity != Core_GetIdentity())
                         continue;
               }
          }
          else if (Core_GetIdentity() != FUSION_ID_MASTER && !(allocation->access[accessor] & CSAF_SHARED)) {
               D_DEBUG_AT( Core_SurfBuffer, "    -> refusing allocation for slave from non-shared pool!\n" );
               continue;
          }

          if (direct_serial_check( &allocation->serial, &buffer->serial )) {
               /* Return immediately if up to date allocation has required flags. */
               if (D_FLAGS_ARE_SET( allocation->access[accessor], flags ))
                    return allocation;

               /* Remember up to date allocation in case none has supported flags. */
               uptodate = allocation;
          }
          else if (D_FLAGS_ARE_SET( allocation->access[accessor], flags )) {
               /* Remember outdated allocation which has supported flags. */
               outdated = allocation;
          }
     }

     /* In case of a lock the flags are mandatory and the outdated allocation has to be used. */
     if (lock)
          return outdated;

     /* Otherwise we can still prefer the up to date allocation. */
     return uptodate ?: outdated;
}

CoreSurfaceAllocation *
dfb_surface_buffer_find_allocation_key( CoreSurfaceBuffer *buffer,
                                        const char        *key )
{
     int                    i;
     CoreSurfaceAllocation *allocation;

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_MAGIC_ASSERT( buffer->surface, CoreSurface );

     D_DEBUG_AT( Core_SurfBuffer, "%s( %p )\n", __FUNCTION__, buffer );

     FUSION_SKIRMISH_ASSERT( &buffer->surface->lock );

     fusion_vector_foreach (allocation, i, buffer->allocs) {
          CORE_SURFACE_ALLOCATION_ASSERT( allocation );

          if (dfb_surface_pool_check_key( allocation->pool, buffer, key, 0 ) == DFB_OK)
               return allocation;
     }

     return NULL;
}

DFBResult
dfb_surface_buffer_lock( CoreSurfaceBuffer      *buffer,
                         CoreSurfaceAccessorID   accessor,
                         CoreSurfaceAccessFlags  access,
                         CoreSurfaceBufferLock  *lock )
{
     DFBResult              ret;
     CoreSurfaceAllocation *allocation;

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_MAGIC_ASSERT( buffer->surface, CoreSurface );
     D_ASSERT( accessor >= CSAID_CPU );
     D_FLAGS_ASSERT( access, CSAF_ALL );
     D_ASSERT( lock != NULL );

     D_DEBUG_AT( Core_SurfBuffer, "%s( %p )\n", __FUNCTION__, lock );

     FUSION_SKIRMISH_ASSERT( &buffer->surface->lock );

     D_ASSUME( accessor < CSAID_NUM );

     if (accessor >= CSAID_ANY) {
          D_UNIMPLEMENTED();
          return DFB_UNIMPLEMENTED;
     }

     if (accessor < 0 || accessor >= CSAID_NUM)
          return DFB_INVARG;

     if (direct_log_domain_check( &Core_SurfBuffer )) {
          D_DEBUG_AT( Core_SurfBuffer, "%s( %p, 0x%02x, %p ) <- %dx%d %s [%d]\n", __FUNCTION__, buffer, access, lock,
                      buffer->config.size.w, buffer->config.size.h, dfb_pixelformat_name( buffer->config.format ),
                      dfb_surface_buffer_index(buffer) );

          switch (accessor) {
               case CSAID_CPU:
                    D_DEBUG_AT( Core_SurfBuffer, "  -> CPU %s%s\n",
                                (access & CSAF_READ) ? "READ" : "", (access & CSAF_WRITE) ? "WRITE" : "" );
                    break;

               case CSAID_GPU:
                    D_DEBUG_AT( Core_SurfBuffer, "  -> GPU %s%s\n",
                                (access & CSAF_READ) ? "READ" : "", (access & CSAF_WRITE) ? "WRITE" : "" );
                    break;

               case CSAID_LAYER0:
               case CSAID_LAYER1:
               case CSAID_LAYER2:
               case CSAID_LAYER3:
               case CSAID_LAYER4:
               case CSAID_LAYER5:
               case CSAID_LAYER6:
               case CSAID_LAYER7:
               case CSAID_LAYER8:
               case CSAID_LAYER9:
               case CSAID_LAYER10:
               case CSAID_LAYER11:
               case CSAID_LAYER12:
               case CSAID_LAYER13:
               case CSAID_LAYER14:
               case CSAID_LAYER15:
                    D_DEBUG_AT( Core_SurfBuffer, "  -> LAYER %u %s%s\n", accessor - CSAID_LAYER0,
                                (access & CSAF_READ) ? "READ" : "", (access & CSAF_WRITE) ? "WRITE" : "" );
                    break;

               default:
                    D_DEBUG_AT( Core_SurfBuffer, "  -> OTHER\n" );
                    break;
          }

          if (access & CSAF_SHARED)
               D_DEBUG_AT( Core_SurfBuffer, "  -> SHARED\n" );
     }

     D_DEBUG_AT( Core_SurfBuffer, "  -> calling PreLockBuffer( %p )...\n", buffer );

     /* Run all code that modifies shared memory in master process. */
     ret = CoreSurface_PreLockBuffer( buffer->surface, buffer, accessor, access, &allocation );
     if (ret)
          return ret;

     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );

     D_DEBUG_AT( Core_SurfBuffer, "  -> PreLockBuffer() returned allocation %p ('%s')\n",
                 allocation, allocation->pool->desc.name );

     /* Lock the allocation. */
     dfb_surface_buffer_lock_init( lock, accessor, access );

     ret = dfb_surface_pool_lock( allocation->pool, allocation, lock );
     if (ret) {
          D_DERROR( ret, "Core/SurfBuffer: Locking allocation in '%s' failed!\n", allocation->pool->desc.name );
          dfb_surface_buffer_lock_deinit( lock );
          dfb_surface_allocation_unref( allocation );
          return ret;
     }

     return DFB_OK;
}

DFBResult
dfb_surface_buffer_unlock( CoreSurfaceBufferLock *lock )
{
     DFBResult              ret;
     CoreSurfacePool       *pool;
     CoreSurfaceAllocation *allocation;

     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( Core_SurfBuffer, "%s( %p )\n", __FUNCTION__, lock );

     allocation = lock->allocation;

     CORE_SURFACE_ALLOCATION_ASSERT( allocation );

     D_MAGIC_ASSERT( allocation->pool, CoreSurfacePool );

     pool = allocation->pool;

     ret = dfb_surface_pool_unlock( pool, lock->allocation, lock );
     if (ret) {
          D_DERROR( ret, "Core/SurfBuffer: Unlocking allocation in '%s' failed!\n", pool->desc.name );
          return ret;
     }

     dfb_surface_buffer_lock_reset( lock );

     dfb_surface_buffer_lock_deinit( lock );

     dfb_surface_allocation_unref( allocation );

     return DFB_OK;
}

DFBResult
dfb_surface_buffer_dump_type_locked( CoreSurfaceBuffer     *buffer,
                                     const char            *directory,
                                     const char            *prefix,
                                     bool                   raw,
                                     CoreSurfaceBufferLock *lock )
{
     CORE_SURFACE_BUFFER_LOCK_ASSERT( lock );

     CORE_SURFACE_ALLOCATION_ASSERT( lock->allocation );

     return dfb_surface_buffer_dump_type_locked2( buffer, directory, prefix, raw, lock->addr, lock->pitch );
}

DFBResult
dfb_surface_buffer_dump_type_locked2( CoreSurfaceBuffer *buffer,
                                      const char        *directory,
                                      const char        *prefix,
                                      bool               raw,
                                      void              *addr,
                                      int                pitch )
{
     DFBResult    ret;
     int          num  = -1;
     int          i, n;
     int          len = (directory ? strlen( directory ) : 0) + (prefix ? strlen( prefix ) : 0) + 40;
     char         filename[len];
     char         head[30];
     bool         rgb = false;
     bool         alpha = false;
     char         rgb_ext[4];
     size_t       bytes;
     CorePalette *palette = NULL;
     DirectFile   fd_p, fd_g;

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_MAGIC_ASSERT( buffer->surface, CoreSurface );
     D_ASSERT( directory != NULL );

     D_DEBUG_AT( Core_SurfBuffer, "%s( %p, %p, %p )\n", __FUNCTION__, buffer, directory, prefix );

     /* Check pixel format. */
     switch (buffer->config.format) {
          case DSPF_LUT8:
          case DSPF_ALUT8:
               palette = buffer->surface->palette;

               if (!palette) {
                    D_BUG( "no palette" );
                    return DFB_BUG;
               }

               if (dfb_palette_ref( palette ))
                    return DFB_FUSION;

               rgb = true;
               /* fall through */

          case DSPF_A8:
               alpha = true;
               break;

          case DSPF_ARGB:
          case DSPF_ABGR:
          case DSPF_ARGB1555:
          case DSPF_RGBA5551:
          case DSPF_ARGB2554:
          case DSPF_ARGB4444:
          case DSPF_AiRGB:
          case DSPF_ARGB8565:
          case DSPF_AYUV:
          case DSPF_AVYU:
               alpha = true;
               /* fall through */

          case DSPF_RGB332:
          case DSPF_RGB444:
          case DSPF_RGB555:
          case DSPF_BGR555:
          case DSPF_RGB16:
          case DSPF_RGB24:
          case DSPF_RGB32:
          case DSPF_YUY2:
          case DSPF_UYVY:
          case DSPF_VYU:
          case DSPF_I420:
          case DSPF_YV12:
          case DSPF_NV12:
          case DSPF_NV21:
          case DSPF_Y42B:
          case DSPF_YV16:
          case DSPF_NV16:
          case DSPF_NV61:
          case DSPF_Y444:
          case DSPF_YV24:
          case DSPF_NV24:
          case DSPF_NV42:
               rgb = true;
               break;

          default:
               D_ERROR( "Core/SurfBuffer: Surface dump for format '%s' is not implemented!\n",
                        dfb_pixelformat_name( buffer->config.format ) );
               return DFB_UNSUPPORTED;
     }

     /* Setup the file extension depending on whether we want the output in RAW format or not. */
     snprintf( rgb_ext, D_ARRAY_SIZE(rgb_ext), (raw == true) ? "raw" : "ppm" );

     if (prefix) {
          /* Find the lowest unused index. */
          while (++num < 10000) {
               snprintf( filename, len, "%s/%s_%04d.%s", directory, prefix, num, rgb_ext );

               ret = direct_access( filename, F_OK );
               if (ret) {
                    snprintf( filename, len, "%s/%s_%04d.pgm", directory, prefix, num );
                    ret = direct_access( filename, F_OK );
                    if (ret)
                         break;
               }
          }

          if (num == 10000) {
               D_ERROR( "Core/SurfBuffer: Could not find an unused index for surface dump!\n" );
               if (palette)
                    dfb_palette_unref( palette );
               return DFB_FAILURE;
          }
     }

     /* Create a file with the found index. */
     if (rgb) {
          if (prefix)
               snprintf( filename, len, "%s/%s_%04d.%s", directory, prefix, num, rgb_ext );
          else
               snprintf( filename, len, "%s.%s", directory, rgb_ext );

          ret = direct_file_open( &fd_p, filename, O_EXCL | O_CREAT | O_WRONLY, 0644 );
          if (ret) {
               D_DERROR( ret, "Core/SurfBuffer: Could not open '%s'!\n", filename );
               if (palette)
                    dfb_palette_unref( palette );
               return ret;
          }
     }

     /* Create a graymap for the alpha channel using the found index. */
     if (alpha && !raw) {
          if (prefix)
               snprintf( filename, len, "%s/%s_%04d.pgm", directory, prefix, num );
          else
               snprintf( filename, len, "%s.pgm", directory );

          ret = direct_file_open( &fd_g, filename, O_EXCL | O_CREAT | O_WRONLY, 0644 );
          if (ret) {
               D_DERROR( ret, "Core/SurfBuffer: Could not open '%s'!\n", filename );

               if (palette)
                    dfb_palette_unref( palette );

               if (rgb) {
                    direct_file_close( &fd_p );
                    snprintf( filename, len, "%s/%s_%04d.%s", directory, prefix, num, rgb_ext );
                    direct_unlink( filename );
               }

               return ret;
          }
     }

     /* Only write the header if we are not dumping a raw image. */
     if (!raw) {
          if (rgb) {
               /* Write the pixmap header. */
               snprintf( head, sizeof(head), "P6\n%d %d\n255\n", buffer->config.size.w, buffer->config.size.h );
               direct_file_write( &fd_p, head, strlen( head ), &bytes );
          }

          /* Write the graymap header. */
          if (alpha) {
               snprintf( head, sizeof(head), "P5\n%d %d\n255\n", buffer->config.size.w, buffer->config.size.h );
               direct_file_write( &fd_g, head, strlen( head ), &bytes );
          }
     }

     /* Write the pixmap (and graymap) data. */
     for (i = 0; i < buffer->config.size.h; i++) {
          int n3;

          /* Prepare one row. */
          u8  *srces[3]   = { NULL, NULL, NULL };
          int  pitches[3] = { 0, 0, 0 };
          u8  *src8;

          dfb_surface_get_data_offsets( &buffer->config, addr, pitch, 0, i, 3, srces, pitches );
          src8 = srces[0];

          /* Write color buffer to pixmap file. */
          if (rgb) {
               if (raw) {
                    u8 buf_p[buffer->config.size.w*4];

                    if (buffer->config.format == DSPF_LUT8) {
                         for (n = 0, n3 = 0; n < buffer->config.size.w; n++, n3 += 4) {
                              buf_p[n3+0] = palette->entries[src8[n]].r;
                              buf_p[n3+1] = palette->entries[src8[n]].g;
                              buf_p[n3+2] = palette->entries[src8[n]].b;
                              buf_p[n3+3] = palette->entries[src8[n]].a;
                         }
                    }
                    else
                         dfb_convert_to_argb( buffer->config.format, buffer->config.colorspace,
                                              srces[0], pitches[0], srces[1], pitches[1], srces[2], pitches[2],
                                              buffer->config.size.h, (u32*) (&buf_p[0]),
                                              buffer->config.size.w * 4, buffer->config.size.w, 1 );

                    direct_file_write( &fd_p, buf_p, buffer->config.size.w * 4, &bytes );
               }
               else {
                    u8 buf_p[buffer->config.size.w*3];

                    if (buffer->config.format == DSPF_LUT8) {
                         for (n = 0, n3 = 0; n < buffer->config.size.w; n++, n3 += 3) {
                              buf_p[n3+0] = palette->entries[src8[n]].r;
                              buf_p[n3+1] = palette->entries[src8[n]].g;
                              buf_p[n3+2] = palette->entries[src8[n]].b;
                         }
                    }
                    else
                         dfb_convert_to_rgb24( buffer->config.format, buffer->config.colorspace,
                                               srces[0], pitches[0], srces[1], pitches[1], srces[2], pitches[2],
                                               buffer->config.size.h, buf_p,
                                               buffer->config.size.w * 3, buffer->config.size.w, 1 );

                    direct_file_write( &fd_p, buf_p, buffer->config.size.w * 3, &bytes );
               }
          }

          /* Write alpha buffer to graymap file. */
          if (alpha && !raw) {
               u8 buf_g[buffer->config.size.w];

               if (buffer->config.format == DSPF_LUT8) {
                    for (n = 0; n < buffer->config.size.w; n++)
                         buf_g[n] = palette->entries[src8[n]].a;
               }
               else
                    dfb_convert_to_a8( buffer->config.format, srces[0], pitches[0], buffer->config.size.h, buf_g,
                                       buffer->config.size.w, buffer->config.size.w, 1 );

               direct_file_write( &fd_g, buf_g, buffer->config.size.w, &bytes );
          }
     }

     /* Release the palette. */
     if (palette)
          dfb_palette_unref( palette );

     /* Close pixmap file. */
     if (rgb)
          direct_file_close( &fd_p );

     /* Close graymap file. */
     if (alpha && !raw)
          direct_file_close( &fd_g );

     return DFB_OK;
}

static DFBResult
dfb_surface_buffer_dump_type( CoreSurfaceBuffer *buffer,
                              const char        *directory,
                              const char        *prefix,
                              bool               raw )
{
     DFBResult             ret;
     CoreSurfaceBufferLock lock;

     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_ASSERT( directory != NULL );

     D_DEBUG_AT( Core_SurfBuffer, "%s( %p, %p, %p )\n", __FUNCTION__, buffer, directory, prefix );

     /* Lock the surface buffer, get the data pointer and pitch. */
     ret = dfb_surface_buffer_lock( buffer, CSAID_CPU, CSAF_READ, &lock );
     if (ret)
          return ret;

     ret = dfb_surface_buffer_dump_type_locked( buffer, directory, prefix, raw, &lock );

     /* Unlock the surface buffer. */
     dfb_surface_buffer_unlock( &lock );

     return ret;
}

DFBResult
dfb_surface_buffer_dump( CoreSurfaceBuffer *buffer,
                         const char        *directory,
                         const char        *prefix )
{
     return dfb_surface_buffer_dump_type( buffer, directory, prefix, false );
}

DFBResult
dfb_surface_buffer_dump_raw( CoreSurfaceBuffer *buffer,
                             const char        *directory,
                             const char        *prefix )
{
     return dfb_surface_buffer_dump_type( buffer, directory, prefix, true );
}

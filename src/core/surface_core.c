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

#include <core/core_parts.h>
#include <core/surface_allocation.h>
#include <core/surface_buffer.h>
#include <core/surface_core.h>
#include <core/surface_pool_bridge.h>
#include <direct/signals.h>
#include <directfb_util.h>
#include <fusion/conf.h>

D_DEBUG_DOMAIN( Core_Surface, "Core/SurfaceCore", "DirectFB Surface Core" );

/**********************************************************************************************************************/

DFB_CORE_PART( surface_core, SurfaceCore );

/**********************************************************************************************************************/

#if FUSION_BUILD_MULTI
extern const SurfacePoolFuncs       sharedSurfacePoolFuncs;
extern const SurfacePoolFuncs       sharedSecureSurfacePoolFuncs;
#else /* FUSION_BUILD_MULTI */
extern const SurfacePoolFuncs       localSurfacePoolFuncs;
#endif /* FUSION_BUILD_MULTI */

extern const SurfacePoolFuncs       preallocSurfacePoolFuncs;

extern const SurfacePoolBridgeFuncs preallocSurfacePoolBridgeFuncs;

static DFBEnumerationResult
alloc_callback( CoreSurfaceAllocation *allocation,
                void                  *ctx )
{
     CoreSurfaceBuffer *buffer;
     const char        *role     = "???";
     const char        *uptodate = " ? ";
     int                allocs   = 0;

     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );

     buffer = allocation->buffer;
     if (buffer) {
          CoreSurface *surface;

          D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

          surface = buffer->surface;
          if (surface) {
               int base;

               D_MAGIC_ASSERT( surface, CoreSurface );

               base = surface->flips % surface->num_buffers;

               role = (buffer->index == (base + DSBR_FRONT) % surface->num_buffers) ? "front" :
                      (buffer->index == (base + DSBR_BACK)  % surface->num_buffers) ? "back"  :
                      (buffer->index == (base + DSBR_IDLE)  % surface->num_buffers) ? "idle"  : "";

               uptodate = direct_serial_check(&allocation->serial, &buffer->serial) ? " * " : "   ";

               allocs = fusion_vector_size( &buffer->allocs );
          }
     }

     printf( "%3u ", allocation->object.id );

     printf( "%3u ", allocation->buffer_id );

     printf( "%9lu %8d  ", allocation->offset, allocation->size );

     printf( "%4d x %4d   ", allocation->config.size.w, allocation->config.size.h );

     printf( "%8s ", dfb_pixelformat_name( allocation->config.format ) );

     printf( " %-5s %s", role, uptodate );

     printf( "%d  %2lu  ", allocs, allocation->resource_id );

     if (allocation->type & CSTF_SHARED)
          printf( "SHARED  " );
     else
          printf( "PRIVATE " );

     if (allocation->type & CSTF_LAYER)
          printf( "LAYER " );

     if (allocation->type & CSTF_WINDOW)
          printf( "WINDOW " );

     if (allocation->type & CSTF_CURSOR)
          printf( "CURSOR " );

     if (allocation->type & CSTF_FONT)
          printf( "FONT " );

     printf( " " );

     if (allocation->type & CSTF_INTERNAL)
          printf( "INTERNAL " );

     if (allocation->type & CSTF_EXTERNAL)
          printf( "EXTERNAL " );

     printf( " " );

     if (allocation->config.caps & DSCAPS_SYSTEMONLY)
          printf( "system only  " );

     if (allocation->config.caps & DSCAPS_VIDEOONLY)
          printf( "video only   " );

     if (allocation->config.caps & DSCAPS_INTERLACED)
          printf( "interlaced   " );

     if (allocation->config.caps & DSCAPS_DOUBLE)
          printf( "double       " );

     if (allocation->config.caps & DSCAPS_TRIPLE)
          printf( "triple       " );

     if (allocation->config.caps & DSCAPS_PREMULTIPLIED)
          printf( "premultiplied" );

     printf( "   ref 0x%04x\n", (unsigned int) allocation->object.ref.multi.id );

     char buf[64];
     snprintf( buf, sizeof(buf), "dfb_surface_allocation_%u_%u", allocation->object.id, allocation->buffer_id );

     dfb_surface_allocation_dump( allocation, ".", buf, false );

     return DFENUM_OK;
}

static DFBEnumerationResult
surface_pool_callback( CoreSurfacePool *pool,
                       void            *ctx )
{
     int length;

     printf( "\n" );
     printf( "--------------------[ Surface Buffer Allocations in %s ]--------------------%n\n",
             pool->desc.name, &length );
     printf( "ID  BID    Offset   Length Width Height     Format  Role  Up nA ID  Usage   Type / Storage / Caps\n" );

     while (length--)
          putc( '-', stdout );

     printf( "\n" );

     dfb_surface_pool_enumerate( pool, alloc_callback, NULL );

     return DFENUM_OK;
}

static void
dump_surface_pools( void )
{
     dfb_surface_pools_enumerate( surface_pool_callback, NULL );
}

static DirectSignalHandlerResult
dfb_surface_core_dump_handler( int   num,
                               void *addr,
                               void *ctx )
{
     dump_surface_pools();

     return DSHR_OK;
}

static DFBResult
dfb_surface_core_initialize( CoreDFB              *core,
                             DFBSurfaceCore       *data,
                             DFBSurfaceCoreShared *shared )
{
     DFBResult ret;

     D_DEBUG_AT( Core_Surface, "%s( %p, %p, %p )\n", __FUNCTION__, core, data, shared );

     D_ASSERT( data != NULL );
     D_ASSERT( shared != NULL );

     data->core   = core;
     data->shared = shared;

#if FUSION_BUILD_MULTI
     if (fusion_config->secure_fusion) {
          ret = dfb_surface_pool_initialize2( core, &sharedSecureSurfacePoolFuncs, data, &shared->surface_pool );
          if (ret) {
               D_DERROR( ret, "Core/SurfaceCore: Could not register 'shared' surface pool!\n" );
               return ret;
          }
     }
     else {
          ret = dfb_surface_pool_initialize2( core, &sharedSurfacePoolFuncs, data, &shared->surface_pool );
          if (ret) {
               D_DERROR( ret, "Core/SurfaceCore: Could not register 'shared' surface pool!\n" );
               return ret;
          }
     }
#else /* FUSION_BUILD_MULTI */
     ret = dfb_surface_pool_initialize2( core, &localSurfacePoolFuncs, data, &shared->surface_pool );
     if (ret) {
          D_DERROR( ret, "Core/SurfaceCore: Could not register 'local' surface pool!\n" );
          return ret;
     }
#endif /* FUSION_BUILD_MULTI */

     ret = dfb_surface_pool_initialize2( core, &preallocSurfacePoolFuncs, data, &shared->prealloc_pool );
     if (ret) {
          D_DERROR( ret, "Core/SurfaceCore: Could not register 'prealloc' surface pool!\n" );
          dfb_surface_pool_destroy( shared->surface_pool );
          return ret;
     }

     ret = dfb_surface_pool_bridge_initialize( core, &preallocSurfacePoolBridgeFuncs, data,
                                               &shared->prealloc_pool_bridge );
     if (ret) {
          D_DERROR( ret, "Core/SurfaceCore: Could not register 'prealloc' surface pool bridge!\n" );
          dfb_surface_pool_destroy( shared->prealloc_pool );
          dfb_surface_pool_destroy( shared->surface_pool );
          return ret;
     }

     ret = direct_signal_handler_add( DIRECT_SIGNAL_DUMP_STACK, dfb_surface_core_dump_handler, data,
                                      &data->dump_signal_handler );
     if (ret) {
          D_DERROR( ret, "Core/SurfaceCore: Could not register surface core signal handler!\n" );
          dfb_surface_pool_bridge_destroy( shared->prealloc_pool_bridge );
          dfb_surface_pool_destroy( shared->prealloc_pool );
          dfb_surface_pool_destroy( shared->surface_pool );
          return ret;
     }

     D_MAGIC_SET( data, DFBSurfaceCore );
     D_MAGIC_SET( shared, DFBSurfaceCoreShared );

     return DFB_OK;
}

static DFBResult
dfb_surface_core_join( CoreDFB              *core,
                       DFBSurfaceCore       *data,
                       DFBSurfaceCoreShared *shared )
{
     DFBResult ret;

     D_DEBUG_AT( Core_Surface, "%s( %p, %p, %p )\n", __FUNCTION__, core, data, shared );

     D_ASSERT( data != NULL );
     D_MAGIC_ASSERT( shared, DFBSurfaceCoreShared );

     data->core   = core;
     data->shared = shared;

#if FUSION_BUILD_MULTI
     if (fusion_config->secure_fusion)
          dfb_surface_pool_join2( core, shared->surface_pool, &sharedSecureSurfacePoolFuncs, data );
     else
          dfb_surface_pool_join2( core, shared->surface_pool, &sharedSurfacePoolFuncs, data );
#else /* FUSION_BUILD_MULTI */
     dfb_surface_pool_join2( core, shared->surface_pool, &localSurfacePoolFuncs, data );
#endif /* FUSION_BUILD_MULTI */

     dfb_surface_pool_join2( core, shared->prealloc_pool, &preallocSurfacePoolFuncs, data );

     dfb_surface_pool_bridge_join( core, shared->prealloc_pool_bridge, &preallocSurfacePoolBridgeFuncs, data );

     ret = direct_signal_handler_add( DIRECT_SIGNAL_DUMP_STACK, dfb_surface_core_dump_handler, data,
                                      &data->dump_signal_handler );
     if (ret) {
          D_DERROR( ret, "Core/SurfaceCore: Could not register surface core signal handler!\n" );
          dfb_surface_pool_bridge_leave( shared->prealloc_pool_bridge );
          dfb_surface_pool_leave( shared->prealloc_pool );
          dfb_surface_pool_leave( shared->surface_pool );
          return ret;
     }

     D_MAGIC_SET( data, DFBSurfaceCore );

     return DFB_OK;
}

static DFBResult
dfb_surface_core_shutdown( DFBSurfaceCore *data,
                           bool            emergency )
{
     DFBSurfaceCoreShared *shared;

     D_DEBUG_AT( Core_Surface, "%s( %p, %semergency )\n", __FUNCTION__, data, emergency ? "" : "no " );

     D_MAGIC_ASSERT( data, DFBSurfaceCore );
     D_MAGIC_ASSERT( data->shared, DFBSurfaceCoreShared );

     shared = data->shared;

     direct_signal_handler_remove( data->dump_signal_handler );

     dfb_surface_pool_bridge_destroy( shared->prealloc_pool_bridge );

     dfb_surface_pool_destroy( shared->prealloc_pool );

     dfb_surface_pool_destroy( shared->surface_pool );

     D_MAGIC_CLEAR( data );
     D_MAGIC_CLEAR( shared );

     return DFB_OK;
}

static DFBResult
dfb_surface_core_leave( DFBSurfaceCore *data,
                        bool            emergency )
{
     DFBSurfaceCoreShared *shared;

     D_DEBUG_AT( Core_Surface, "%s( %p, %semergency )\n", __FUNCTION__, data, emergency ? "" : "no " );

     D_MAGIC_ASSERT( data, DFBSurfaceCore );
     D_MAGIC_ASSERT( data->shared, DFBSurfaceCoreShared );

     shared = data->shared;

     direct_signal_handler_remove( data->dump_signal_handler );

     dfb_surface_pool_bridge_leave( shared->prealloc_pool_bridge );

     dfb_surface_pool_leave( shared->prealloc_pool );

     dfb_surface_pool_leave( shared->surface_pool );

     D_MAGIC_CLEAR( data );

     return DFB_OK;
}

static DFBResult
dfb_surface_core_suspend( DFBSurfaceCore *data )
{
     D_DEBUG_AT( Core_Surface, "%s( %p )\n", __FUNCTION__, data );

     D_MAGIC_ASSERT( data, DFBSurfaceCore );
     D_MAGIC_ASSERT( data->shared, DFBSurfaceCoreShared );

     return DFB_OK;
}

static DFBResult
dfb_surface_core_resume( DFBSurfaceCore *data )
{
     D_DEBUG_AT( Core_Surface, "%s( %p )\n", __FUNCTION__, data );

     D_MAGIC_ASSERT( data, DFBSurfaceCore );
     D_MAGIC_ASSERT( data->shared, DFBSurfaceCoreShared );

     return DFB_OK;
}

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

#include <core/CoreSurfaceClient.h>
#include <core/core.h>
#include <core/surface.h>
#include <core/surface_client.h>
#include <directfb_util.h>

D_DEBUG_DOMAIN( Core_SurfClient, "Core/SurfClient", "DirectFB Core Surface Client" );

/**********************************************************************************************************************/

static void
surface_client_destructor( FusionObject *object,
                           bool          zombie,
                           void         *ctx )
{
     CoreSurfaceClient *client = (CoreSurfaceClient*) object;
     CoreSurface       *surface;
     int                index;

     D_MAGIC_ASSERT( client, CoreSurfaceClient );
     D_MAGIC_ASSERT( client->surface, CoreSurface );

     surface = client->surface;

     D_DEBUG_AT( Core_SurfClient, "Destroying client %p (%dx%d%s)\n",
                 client, surface->config.size.w, surface->config.size.h, zombie ? " ZOMBIE" : "" );

     CoreSurfaceClient_Deinit_Dispatch( &client->call );

     dfb_surface_lock( surface );

     index = fusion_vector_index_of( &surface->clients, client );

     D_ASSERT( index >= 0 );

     fusion_vector_remove( &surface->clients, index );

     dfb_surface_check_acks( surface );

     dfb_surface_unlock( surface );

     dfb_surface_unlink( &client->surface );

     D_MAGIC_CLEAR( client );

     /* Destroy the object. */
     fusion_object_destroy( object );
}

FusionObjectPool *
dfb_surface_client_pool_create( const FusionWorld *world )
{
     FusionObjectPool *pool;

     pool = fusion_object_pool_create( "Surface Client Pool",
                                       sizeof(CoreSurfaceClient), sizeof(CoreSurfaceClientNotification),
                                       surface_client_destructor, NULL, world );

     return pool;
}

/**********************************************************************************************************************/

DFBResult
dfb_surface_client_create( CoreDFB            *core,
                           CoreSurface        *surface,
                           CoreSurfaceClient **ret_client )
{
     DFBResult          ret;
     CoreSurfaceClient *client;

     D_MAGIC_ASSERT( surface, CoreSurface );
     D_ASSERT( ret_client != NULL );

     D_DEBUG_AT( Core_SurfClient, "%s( %p %dx%d %s )\n", __FUNCTION__, surface,
                 surface->config.size.w, surface->config.size.h, dfb_pixelformat_name( surface->config.format ) );

     client = dfb_core_create_surface_client( core );
     if (!client)
          return DFB_FUSION;

     ret = dfb_surface_link( &client->surface, surface );
     if (ret) {
          fusion_object_destroy( &client->object );
          return ret;
     }

     D_MAGIC_SET( client, CoreSurfaceClient );

     CoreSurfaceClient_Init_Dispatch( core, client, &client->call );

     dfb_surface_lock( surface );

     client->flip_count = surface->flips;

     fusion_vector_add( &surface->clients, client );

     dfb_surface_unlock( surface );

     /* Activate object. */
     fusion_object_activate( &client->object );

     /* Return the new client. */
     *ret_client = client;

     D_DEBUG_AT( Core_SurfClient, "  -> %p\n", client );

     return DFB_OK;
}

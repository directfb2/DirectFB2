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

#include <core/core.h>
#include <core/surface.h>
#include <directfb_util.h>

D_DEBUG_DOMAIN( ICoreResourceManager_default, "ICoreResourceManager/default", "Default Resource Manager" );
D_DEBUG_DOMAIN( ICoreResourceClient_default,  "ICoreResourceClient/default",  "Default Resource Client" );

static DirectResult Probe    ( void                 *ctx );

static DirectResult Construct( ICoreResourceManager *thiz,
                               CoreDFB              *core );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( ICoreResourceManager, default )

/**********************************************************************************************************************/

typedef struct {
     int          ref;         /* reference counter */

     FusionID     identity;

     unsigned int surface_mem;
} ICoreResourceClient_data;

/**********************************************************************************************************************/

static void
ICoreResourceClient_Destruct( ICoreResourceClient *thiz )
{
     D_DEBUG_AT( ICoreResourceClient_default, "%s( %p )\n", __FUNCTION__, thiz );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DirectResult
ICoreResourceClient_AddRef( ICoreResourceClient *thiz )
{
     DIRECT_INTERFACE_GET_DATA( ICoreResourceClient )

     D_DEBUG_AT( ICoreResourceClient_default, "%s( %p )\n", __FUNCTION__, thiz );

     data->ref++;

     return DFB_OK;
}

static DirectResult
ICoreResourceClient_Release( ICoreResourceClient *thiz )
{
     DIRECT_INTERFACE_GET_DATA( ICoreResourceClient )

     D_DEBUG_LOG( ICoreResourceClient_default, 1, "%s( %p )\n", __FUNCTION__, thiz );

     if (--data->ref == 0) {
          D_LOG( ICoreResourceClient_default, INFO, "Removing ID %lu\n", data->identity );

          ICoreResourceClient_Destruct( thiz );
     }

     return DFB_OK;
}

static __inline__ unsigned int
surface_mem( const CoreSurfaceConfig *config )
{
     unsigned int mem;

     mem = DFB_PLANE_MULTIPLY( config->format, config->size.h ) * DFB_BYTES_PER_LINE( config->format, config->size.w );

     if (config->caps & DSCAPS_TRIPLE)
          mem *= 3;
     else if (config->caps & DSCAPS_DOUBLE)
          mem *= 2;

     return mem;
}

static DFBResult
ICoreResourceClient_CheckSurface( ICoreResourceClient     *thiz,
                                  const CoreSurfaceConfig *config,
                                  u64                      resource_id )
{
     DIRECT_INTERFACE_GET_DATA( ICoreResourceClient )

     D_DEBUG_AT( ICoreResourceClient_default, "%s( %p [%lu] )\n", __FUNCTION__, thiz, data->identity );

     D_DEBUG_AT( ICoreResourceClient_default, "  -> %dx%d %s %uk at %uk, resource id %llu\n",
                 config->size.w, config->size.h, dfb_pixelformat_name( config->format ),
                 surface_mem( config ) / 1024, data->surface_mem / 1024, (unsigned long long) resource_id );

     return DFB_OK;
}

static DFBResult
ICoreResourceClient_CheckSurfaceUpdate( ICoreResourceClient     *thiz,
                                        CoreSurface             *surface,
                                        const CoreSurfaceConfig *config )
{
     DIRECT_INTERFACE_GET_DATA( ICoreResourceClient )

     D_DEBUG_AT( ICoreResourceClient_default, "%s( %p [%lu] )\n", __FUNCTION__, thiz, data->identity );

     D_DEBUG_AT( ICoreResourceClient_default, "  -> %u bytes\n", surface_mem( &surface->config ) );

     return DFB_OK;
}

static DFBResult
ICoreResourceClient_AddSurface( ICoreResourceClient *thiz,
                                CoreSurface         *surface )
{
     unsigned int mem;

     DIRECT_INTERFACE_GET_DATA( ICoreResourceClient )

     D_DEBUG_AT( ICoreResourceClient_default, "%s( %p [%lu] )\n", __FUNCTION__, thiz, data->identity );

     mem = surface_mem( &surface->config );

     D_DEBUG_AT( ICoreResourceClient_default, "  -> %u bytes\n", mem );

     data->surface_mem += mem;

     return DFB_OK;
}

static DFBResult
ICoreResourceClient_RemoveSurface( ICoreResourceClient *thiz,
                                   CoreSurface         *surface )
{
     unsigned int mem;

     DIRECT_INTERFACE_GET_DATA( ICoreResourceClient )

     D_DEBUG_AT( ICoreResourceClient_default, "%s( %p [%lu] )\n", __FUNCTION__, thiz, data->identity );

     mem = surface_mem( &surface->config );

     D_DEBUG_AT( ICoreResourceClient_default, "  -> %u bytes\n", mem );

     data->surface_mem -= mem;

     return DFB_OK;
}

static DFBResult
ICoreResourceClient_UpdateSurface( ICoreResourceClient     *thiz,
                                   CoreSurface             *surface,
                                   const CoreSurfaceConfig *config )
{
     DIRECT_INTERFACE_GET_DATA( ICoreResourceClient )

     D_DEBUG_AT( ICoreResourceClient_default, "%s( %p [%lu] )\n", __FUNCTION__, thiz, data->identity );

     data->surface_mem -= surface_mem( &surface->config );
     data->surface_mem += surface_mem( config );

     return DFB_OK;
}

DirectResult
ICoreResourceClient_Construct( ICoreResourceClient *thiz,
                               CoreDFB             *core,
                               FusionID             identity )
{
     char   buf[512];
     size_t len;

     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, ICoreResourceClient )

     D_DEBUG_LOG( ICoreResourceClient_default, 1, "%s( %p )\n", __FUNCTION__, thiz );

     fusion_get_fusionee_path( core->world, identity, buf, sizeof(buf), &len );

     D_LOG( ICoreResourceClient_default, INFO, "Adding ID %lu - '%s'\n", identity, buf );

     data->ref      = 1;
     data->identity = identity;

     thiz->AddRef             = ICoreResourceClient_AddRef;
     thiz->Release            = ICoreResourceClient_Release;
     thiz->CheckSurface       = ICoreResourceClient_CheckSurface;
     thiz->CheckSurfaceUpdate = ICoreResourceClient_CheckSurfaceUpdate;
     thiz->AddSurface         = ICoreResourceClient_AddSurface;
     thiz->RemoveSurface      = ICoreResourceClient_RemoveSurface;
     thiz->UpdateSurface      = ICoreResourceClient_UpdateSurface;

     return DFB_OK;
}

/**********************************************************************************************************************/

typedef struct {
     int ref;       /* reference counter */

     CoreDFB *core;
} ICoreResourceManager_data;

/**********************************************************************************************************************/

static void
ICoreResourceManager_Destruct( ICoreResourceManager *thiz )
{
     D_DEBUG_AT( ICoreResourceManager_default, "%s( %p )\n", __FUNCTION__, thiz );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DirectResult
ICoreResourceManager_AddRef( ICoreResourceManager *thiz )
{
     DIRECT_INTERFACE_GET_DATA( ICoreResourceManager )

     D_DEBUG_AT( ICoreResourceManager_default, "%s( %p )\n", __FUNCTION__, thiz );

     data->ref++;

     return DFB_OK;
}

static DirectResult
ICoreResourceManager_Release( ICoreResourceManager *thiz )
{
     DIRECT_INTERFACE_GET_DATA( ICoreResourceManager )

     D_DEBUG_AT( ICoreResourceManager_default, "%s( %p )\n", __FUNCTION__, thiz );

     if (--data->ref == 0)
          ICoreResourceManager_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
ICoreResourceManager_CreateClient( ICoreResourceManager  *thiz,
                                   FusionID               identity,
                                   ICoreResourceClient  **ret_interface )
{
     DIRECT_INTERFACE_GET_DATA( ICoreResourceManager )

     D_DEBUG_LOG( ICoreResourceManager_default, 1, "%s( %p )\n", __FUNCTION__, thiz );

     DIRECT_ALLOCATE_INTERFACE( *ret_interface, ICoreResourceClient );

     return ICoreResourceClient_Construct( *ret_interface, data->core, identity );
}

/**********************************************************************************************************************/

static DirectResult
Probe( void *ctx )
{
     return DFB_OK;
}

static DirectResult
Construct( ICoreResourceManager *thiz,
           CoreDFB              *core )
{
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, ICoreResourceManager )

     D_DEBUG_AT( ICoreResourceManager_default, "%s( %p )\n", __FUNCTION__, thiz );

     D_LOG( ICoreResourceManager_default, NOTICE, "Initializing resource manager 'default'\n" );

     data->ref  = 1;
     data->core = core;

     thiz->AddRef       = ICoreResourceManager_AddRef;
     thiz->Release      = ICoreResourceManager_Release;
     thiz->CreateClient = ICoreResourceManager_CreateClient;

     return DFB_OK;
}

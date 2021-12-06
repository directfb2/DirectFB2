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

#ifndef __CORE__SURFACE_CLIENT_H__
#define __CORE__SURFACE_CLIENT_H__

#include <core/coretypes.h>
#include <fusion/object.h>

/**********************************************************************************************************************/

struct __DFB_CoreSurfaceClient {
     FusionObject             object;

     int                      magic;

     CoreSurface             *surface;

     FusionCall               call;

     u32                      flip_count;

     DFBFrameTimeConfig       frametime_config;
};

/**********************************************************************************************************************/

typedef enum {
     CSCNF_NONE = 0x00000000,
} CoreSurfaceClientNotificationFlags;

typedef struct {
     CoreSurfaceClientNotificationFlags flags;
} CoreSurfaceClientNotification;

/**********************************************************************************************************************/

/*
 * Creates a pool of surface client objects.
 */
FusionObjectPool *dfb_surface_client_pool_create( const FusionWorld  *world );

/*
 * Generates dfb_surface_client_ref(), dfb_surface_client_attach() etc.
 */
FUSION_OBJECT_METHODS( CoreSurfaceClient, dfb_surface_client )

/**********************************************************************************************************************/

DFBResult         dfb_surface_client_create     ( CoreDFB            *core,
                                                  CoreSurface        *surface,
                                                  CoreSurfaceClient **ret_client );

#endif

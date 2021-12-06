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

#ifndef __CORE__CORE_RESOURCEMANAGER_H__
#define __CORE__CORE_RESOURCEMANAGER_H__

#include <core/coretypes.h>
#include <fusion/fusion.h>

/*
 * Resource manager interface.
 */
D_DECLARE_INTERFACE( ICoreResourceManager )
D_DECLARE_INTERFACE( ICoreResourceClient )

/************************
 * ICoreResourceManager *
 ************************/

/*
 * ICoreResourceManager is the resource manager interface.
 */
D_DEFINE_INTERFACE( ICoreResourceManager,

   /** Creating client **/

     /*
      * Create a new client instance (called within master,
      * per slave).
      */
     DFBResult (*CreateClient) (
          ICoreResourceManager              *thiz,
          FusionID                           identity,
          ICoreResourceClient              **ret_client
     );
)

/*
 * ICoreResourceClient per slave resource accounting.
 */
D_DEFINE_INTERFACE( ICoreResourceClient,

   /** Tracking of the resource client instance **/

     /*
      * Check surface creation.
      */
     DFBResult (*CheckSurface) (
          ICoreResourceClient               *thiz,
          const CoreSurfaceConfig           *config,
          u64                                resource_id
     );

     /*
      * Check surface reconfig.
      */
     DFBResult (*CheckSurfaceUpdate) (
          ICoreResourceClient               *thiz,
          CoreSurface                       *surface,
          const CoreSurfaceConfig           *config
     );

     /*
      * Add surface.
      */
     DFBResult (*AddSurface) (
          ICoreResourceClient               *thiz,
          CoreSurface                       *surface
     );

     /*
      * Remove surface.
      */
     DFBResult (*RemoveSurface) (
          ICoreResourceClient               *thiz,
          CoreSurface                       *surface
     );

     /*
      * Update surface.
      */
     DFBResult (*UpdateSurface) (
          ICoreResourceClient               *thiz,
          CoreSurface                       *surface,
          const CoreSurfaceConfig           *config
     );
)

#endif

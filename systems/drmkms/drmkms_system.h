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

#ifndef __DRMKMS_SYSTEM_H__
#define __DRMKMS_SYSTEM_H__

#include <core/layer_region.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

/**********************************************************************************************************************/

typedef struct {
     int                    layer_index;
     int                    plane_index;

     drmModePlane          *plane;
     uint32_t               colorkey_propid;
     uint32_t               zpos_propid;
     uint32_t               alpha_propid;

     int                    level;

     CoreLayerRegionConfig *config;
     bool                   muted;

     CoreSurface           *surface;
     int                    surfacebuffer_index;
     bool                   flip_pending;

     DirectMutex            lock;
     DirectWaitQueue        wq_event;
} DRMKMSLayerData;

typedef struct {
     FusionSHMPoolShared *shmpool;

     CoreSurfacePool     *pool;

     char                 device_name[256];       /* DRM/KMS device name, e.g. /dev/dri/card0 */
     bool                 use_prime_fd;           /* DRM/KMS PRIME file descriptor enabled */

     bool                 vt;                     /* use VT handling */

     bool                 mirror_outputs;         /* enable mirror display */
     bool                 multihead_outputs;      /* enable multi-head display */

     int                  enabled_crtcs;          /* CRTCs enabled (limiting to 8) */
     drmModeModeInfo      mode[8];                /* current video mode (for each available CRTC) */

     DFBDimension         primary_dimension[8];
     DFBRectangle         primary_rect;
     uint32_t             primary_fb;

     int                  layer_index_count;
     int                  plane_index_count;
     int                  layerplane_index_count;
} DRMKMSDataShared;

typedef struct {
     DRMKMSDataShared   *shared;

     CoreDFB            *core;

     int                 fd;              /* DRM/KMS file descriptor */

     drmModeRes         *resources;       /* display configuration information */
     drmModePlaneRes    *plane_resources; /* planes information */

     drmModeConnector   *connector[8];
     drmModeEncoder     *encoder[8];
     drmModeCrtc        *crtc;

     DFBDisplayLayerIDs  layer_ids[8];
     DFBDisplayLayerID   layer_id_next;

     drmEventContext     event_context;
     DirectThread       *thread;
} DRMKMSData;

#endif

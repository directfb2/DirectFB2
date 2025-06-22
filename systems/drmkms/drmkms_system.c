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
#include <core/core_system.h>
#include <core/layers.h>
#include <core/screens.h>
#include <core/surface_pool.h>
#include <fusion/shmalloc.h>

#include "drmkms_system.h"
#include "drmkms_vt.h"

D_DEBUG_DOMAIN( DRMKMS_System, "DRMKMS/System", "DRM/KMS System Module" );

DFB_CORE_SYSTEM( drmkms )

/**********************************************************************************************************************/

extern const ScreenFuncs       drmkmsScreenFuncs;
extern const DisplayLayerFuncs drmkmsPrimaryLayerFuncs;
extern const DisplayLayerFuncs drmkmsPlaneLayerFuncs;
extern const SurfacePoolFuncs  drmkmsSurfacePoolFuncs;

struct DFBFourCCName {
     DFBSurfacePixelFormat  format;
     const char            *name;
};

static const struct DFBFourCCName dfb_fourcc_names[] = {
     { DSPF_ARGB,     "AR24" },
     { DSPF_RGB32,    "XR24" },
     { DSPF_RGB24,    "RG24" },
     { DSPF_RGB16,    "RG16" },
     { DSPF_ARGB1555, "AR15" },
     { DSPF_RGB555,   "XR15" },
     { DSPF_A8,       "C8"   }
};

static DFBResult
local_init( const char *device_name,
            bool        mirror_outputs,
            bool        multihead_outputs,
            DRMKMSData *drmkms )
{
     int               busy, i, j, k;
     CoreScreen       *screen;
     drmModeConnector *connector;
     drmModeEncoder   *encoder;
     uint64_t          has_dumb = 0;

     /* Open DRM/KMS device. */
     drmkms->fd = open( device_name, O_RDWR );
     if (drmkms->fd < 0) {
          D_PERROR( "DRMKMS/System: Failed to open '%s'!\n", device_name );
          return DFB_INIT;
     }

     /* Retrieve display configuration and planes information. */
     drmSetClientCap( drmkms->fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1 );

     drmkms->resources = drmModeGetResources( drmkms->fd );
     if (!drmkms->resources) {
          D_PERROR( "DRMKMS/System: Could not retrieve resources!\n" );
          return DFB_INIT;
     }

     drmkms->plane_resources = drmModeGetPlaneResources( drmkms->fd );
     if (!drmkms->plane_resources || !drmkms->plane_resources->count_planes) {
          D_PERROR( "DRMKMS/System: Could not retrieve plane resources!\n" );
          return DFB_INIT;
     }

     D_INFO( "DRMKMS/System: Found %d connectors, %d encoders, %u planes\n",
             drmkms->resources->count_connectors, drmkms->resources->count_encoders,
             drmkms->plane_resources->count_planes );

     /* Check for the dumb buffer capability. */
     if (drmGetCap( drmkms->fd, DRM_CAP_DUMB_BUFFER, &has_dumb ) < 0 || !has_dumb) {
          D_PERROR( "DRMKMS/System: Could not create dumb buffers!\n" );
          return DFB_INIT;
     }

     screen = dfb_screens_register( drmkms, &drmkmsScreenFuncs );

     dfb_layers_register( screen, drmkms, &drmkmsPrimaryLayerFuncs );

     DFB_DISPLAYLAYER_IDS_ADD( drmkms->layer_ids[0], drmkms->layer_id++ );

     for (i = 0; i < drmkms->resources->count_connectors; i++) {
          connector = drmModeGetConnector( drmkms->fd, drmkms->resources->connectors[i] );
          if (!connector)
               continue;

          encoder = NULL;

          if (connector->count_modes > 0) {
               D_DEBUG_AT( DRMKMS_System, "  -> found connector %u\n", connector->connector_id );

               if (connector->encoder_id) {
                    D_DEBUG_AT( DRMKMS_System, "  -> connector is bound to encoder %u\n", connector->encoder_id );

                    encoder = drmModeGetEncoder( drmkms->fd, connector->encoder_id );
                    if (!encoder)
                         continue;
               }
               else {
                    D_DEBUG_AT( DRMKMS_System, "  -> searching for appropriate encoder\n" );

                    for (j = 0; j < drmkms->resources->count_encoders; j++) {
                         encoder = drmModeGetEncoder( drmkms->fd, drmkms->resources->encoders[j] );
                         if (!encoder)
                              continue;

                         busy = 0;

                         for (k = 0; k < drmkms->enabled_crtcs; k++) {
                              if (drmkms->encoder[k]->encoder_id == encoder->encoder_id) {
                                   D_DEBUG_AT( DRMKMS_System, "  -> encoder %u is already in use by connector %u\n",
                                               encoder->encoder_id, drmkms->connector[k]->connector_id );
                                   busy = 1;
                              }
                         }

                         if (busy)
                              continue;

                         D_DEBUG_AT( DRMKMS_System, "  -> found encoder %u\n", encoder->encoder_id );
                         break;
                    }
               }

               if (encoder) {
                    if (encoder->crtc_id) {
                         D_DEBUG_AT( DRMKMS_System, "  -> encoder is bound to crtc %u\n", encoder->crtc_id );

                         drmkms->crtc = drmModeGetCrtc( drmkms->fd, encoder->crtc_id );
                    }
                    else {
                         D_DEBUG_AT( DRMKMS_System, "  -> searching for appropriate crtc\n" );

                         for (j = 0; j < drmkms->resources->count_crtcs; j++) {
                              if (!(encoder->possible_crtcs & (1 << j)))
                                   continue;

                              busy = 0;

                              for (k = 0; k < drmkms->enabled_crtcs; k++) {
                                   if (drmkms->encoder[k]->crtc_id == drmkms->resources->crtcs[j]) {
                                        D_DEBUG_AT( DRMKMS_System, "  -> crtc %u is already in use by encoder %u\n",
                                                    drmkms->resources->crtcs[j], drmkms->encoder[k]->encoder_id );
                                        busy = 1;
                                   }
                              }

                              if (busy)
                                   continue;

                              encoder->crtc_id = drmkms->resources->crtcs[j];

                              D_DEBUG_AT( DRMKMS_System, "  -> found crtc %u\n", encoder->crtc_id );

                              drmkms->crtc = drmModeGetCrtc( drmkms->fd, encoder->crtc_id );
                              break;
                         }

                         if (!encoder->crtc_id) {
                              D_DEBUG_AT( DRMKMS_System, "  -> cannot find crtc for encoder %u\n", encoder->encoder_id );
                              break;
                         }
                    }

                    drmkms->connector[drmkms->enabled_crtcs] = connector;
                    drmkms->encoder[drmkms->enabled_crtcs]   = encoder;

                    for (j = 0; j < connector->count_modes; j++)
                         D_DEBUG_AT( DRMKMS_System, "    => modes[%2d] is %ux%u@%uHz\n", j,
                                     connector->modes[j].hdisplay, connector->modes[j].vdisplay,
                                     connector->modes[j].vrefresh );

                    drmkms->enabled_crtcs++;

                    if ((!mirror_outputs && !multihead_outputs) || (drmkms->enabled_crtcs == 8))
                         break;

                    if (multihead_outputs && drmkms->enabled_crtcs > 1) {
                         dfb_layers_register( screen, drmkms, &drmkmsPrimaryLayerFuncs );

                         DFB_DISPLAYLAYER_IDS_ADD( drmkms->layer_ids[drmkms->enabled_crtcs-1], drmkms->layer_id++ );
                    }
               }
          }
     }

     if (dfb_core_is_master( drmkms->core )) {
          for (i = 0; i < drmkms->plane_resources->count_planes; i++) {
               drmModePlane *plane;

               plane = drmModeGetPlane( drmkms->fd, drmkms->plane_resources->planes[i] );

               if ((plane->possible_crtcs & drmkms->encoder[0]->possible_crtcs) &&
                   (plane->crtc_id != drmkms->encoder[0]->crtc_id)) {
                    drmkms->shared->layer_indices[drmkms->layer_id] = i;

                    dfb_layers_register( screen, drmkms, &drmkmsPlaneLayerFuncs );

                    DFB_DISPLAYLAYER_IDS_ADD( drmkms->layer_ids[0], drmkms->layer_id++ );
               }
          }
     }
     else {
          for (i = 0; i < drmkms->plane_resources->count_planes; i++) {
               if (i == drmkms->shared->layer_indices[drmkms->layer_id]) {
                    dfb_layers_register( screen, drmkms, &drmkmsPlaneLayerFuncs );

                    DFB_DISPLAYLAYER_IDS_ADD( drmkms->layer_ids[0], drmkms->layer_id++ );
               }
          }
     }

     return DFB_OK;
}

static DFBResult
local_deinit( DRMKMSData *drmkms )
{
     if (drmkms->plane_resources)
          drmModeFreePlaneResources( drmkms->plane_resources );

     if (drmkms->resources)
          drmModeFreeResources( drmkms->resources );

     if (drmkms->fd != -1)
          close( drmkms->fd );

     return DFB_OK;
}

/**********************************************************************************************************************/

static void
system_get_info( CoreSystemInfo *info )
{
     info->version.major = 0;
     info->version.minor = 1;

     info->caps = CSCAPS_ACCELERATION | CSCAPS_NOTIFY_DISPLAY | CSCAPS_SYSMEM_EXTERNAL;

     snprintf( info->name,   DFB_CORE_SYSTEM_INFO_NAME_LENGTH,   "DRM/KMS" );
     snprintf( info->vendor, DFB_CORE_SYSTEM_INFO_VENDOR_LENGTH, "DirectFB" );
}

static DFBResult
system_initialize( CoreDFB  *core,
                   void    **ret_data )
{
     DFBResult                ret;
     int                      i, j, p;
     uint32_t                 fourcc;
     uint64_t                 plane_type = 0;
     char                     name[5];
     DRMKMSData              *drmkms;
     DRMKMSDataShared        *shared;
     FusionSHMPoolShared     *pool;
     const char              *value;
     drmModePlane            *plane;
     drmModeObjectProperties *props;
     drmModePropertyRes      *prop;
     VideoMode               *prev_mode       = NULL;
     DFBSurfacePixelFormat    fallback_format = DSPF_UNKNOWN;

     D_DEBUG_AT( DRMKMS_System, "%s()\n", __FUNCTION__ );

     drmkms = D_CALLOC( 1, sizeof(DRMKMSData) );
     if (!drmkms)
          return D_OOM();

     drmkms->core = core;

     pool = dfb_core_shmpool( core );

     shared = SHCALLOC( pool, 1, sizeof(DRMKMSDataShared) );
     if (!shared) {
          D_FREE( drmkms );
          return D_OOSHM();
     }

     shared->shmpool = pool;

     drmkms->shared = shared;

     if ((value = direct_config_get_value( "drmkms" ))) {
          direct_snputs( shared->device_name, value, 255 );
          D_INFO( "DRMKMS/System: Using device %s as specified in DirectFB configuration\n", shared->device_name );
     }
     else if (getenv( "DRICARD" ) && *getenv( "DRICARD" ) != '\0') {
          direct_snputs( shared->device_name, getenv( "DRICARD" ), 255 );
          D_INFO( "DRMKMS/System: Using device %s as set in DRICARD environment variable\n", shared->device_name );
     }
     else {
          snprintf( shared->device_name, 255, "/dev/dri/card0" );
          D_INFO( "DRMKMS/System: Using device %s (default)\n", shared->device_name );
     }

     if (direct_config_has_name( "drmkms-use-prime-fd" ) && !direct_config_has_name( "no-drmkms-use-prime-fd" )) {
          shared->use_prime_fd = true;
          D_INFO( "DRMKMS/System: Using PRIME file descriptor\n" );
     }

     if (direct_config_has_name( "no-vt" ) && !direct_config_has_name( "vt" ))
          D_INFO( "DRMKMS/System: Don't use VT handling\n" );
     else
          shared->vt = true;

     if ((value = direct_config_get_value( "connected-outputs" ))) {
          if (strcmp( value, "mirror" ) == 0) {
               shared->mirror_outputs = true;
               D_INFO( "DRMKMS/System: Mirror display\n" );
          }
          else if (strcmp( value, "multihead" ) == 0) {
               shared->multihead_outputs = true;
               D_INFO( "DRMKMS/System: Multi-head display\n" );
          }
          else if (strcmp( value, "single" ) == 0) {
               D_INFO( "DRMKMS/System: Single display\n" );
          }
          else {
               D_ERROR( "DRMKMS/System: 'connected-outputs': Unknown connected outputs setting '%s'!\n", value );
               SHFREE( pool, shared );
               D_FREE( drmkms );
               return DFB_INIT;
          }
     }

     ret = local_init( shared->device_name, shared->mirror_outputs, shared->multihead_outputs, drmkms );
     if (ret)
          goto error;

     if (shared->vt) {
          ret = drmkms_vt_initialize( core );
          if (ret)
               goto error;
     }

     for (i = 0; i < drmkms->enabled_crtcs; i++)
          shared->mode[i] = drmkms->connector[0]->modes[0];

     for (p = 0; p < drmkms->plane_resources->count_planes; p++) {
          plane = drmModeGetPlane( drmkms->fd, drmkms->plane_resources->planes[p] );

          props = drmModeObjectGetProperties( drmkms->fd, plane->plane_id, DRM_MODE_OBJECT_PLANE );

          for (i = 0; i < props->count_props; i++) {
               prop = drmModeGetProperty( drmkms->fd, props->props[i] );
               if (!prop)
                    continue;

               if (!strcmp( prop->name, "type" )) {
                    plane_type = props->prop_values[i];
                    break;
               }

               drmModeFreeProperty( prop );
          }

          for (i = 0; i < plane->count_formats; i++) {
               fourcc = plane->formats[i];
               snprintf( name, sizeof(name), "%c%c%c%c", fourcc, fourcc >> 8, fourcc >> 16, fourcc >> 24 );

               if (!strcmp( name, "AR24" ) && plane_type == DRM_PLANE_TYPE_PRIMARY) {
                    shared->primary_format = DSPF_ARGB;
                    break;
               }
               else if (fallback_format == DSPF_UNKNOWN) {
                    for (j = 1; j < D_ARRAY_SIZE(dfb_fourcc_names); j++) {
                         if (!strcmp( name, dfb_fourcc_names[j].name ) && plane_type == DRM_PLANE_TYPE_PRIMARY) {
                              fallback_format = dfb_fourcc_names[j].format;
                              break;
                         }
                    }
               }
          }
     }

     if (shared->primary_format != DSPF_ARGB && fallback_format == DSPF_UNKNOWN) {
          D_ERROR( "DRMKMS/System: No supported format!\n" );
          ret = DFB_INIT;
          goto error;
     }
     else if (shared->primary_format != DSPF_ARGB)
          shared->primary_format = fallback_format;

     /* Initialize the mode table. */
     for (i = 0; i < drmkms->connector[0]->count_modes; i++) {
          VideoMode *mode;

          if (!prev_mode ||
              (prev_mode &&
               prev_mode->xres != drmkms->connector[0]->modes[i].hdisplay &&
               prev_mode->yres != drmkms->connector[0]->modes[i].vdisplay)) {
               mode = SHCALLOC( shared->shmpool, 1, sizeof(VideoMode) );
               if (!mode) {
                    D_OOSHM();
                    continue;
               }

               mode->xres = drmkms->connector[0]->modes[i].hdisplay;
               mode->yres = drmkms->connector[0]->modes[i].vdisplay;
               mode->bpp  = DFB_BITS_PER_PIXEL( dfb_config->mode.format ?: shared->primary_format );
          }
          else
               continue;

          if (!prev_mode)
               shared->modes = mode;
          else
               prev_mode->next = mode;

          prev_mode = mode;
     }

     *ret_data = drmkms;

     ret = dfb_surface_pool_initialize( core, &drmkmsSurfacePoolFuncs, &shared->pool );
     if (ret)
          goto error;

     ret = core_arena_add_shared_field( core, "drmkms", shared );
     if (ret)
          goto error;

     return DFB_OK;

error:
     if (shared->vt)
          drmkms_vt_shutdown( false );

     local_deinit( drmkms );

     SHFREE( pool, shared );

     D_FREE( drmkms );

     return ret;
}

static DFBResult
system_join( CoreDFB  *core,
             void    **ret_data )
{
     DFBResult         ret;
     DRMKMSData       *drmkms;
     DRMKMSDataShared *shared;

     D_DEBUG_AT( DRMKMS_System, "%s()\n", __FUNCTION__ );

     drmkms = D_CALLOC( 1, sizeof(DRMKMSData) );
     if (!drmkms)
          return D_OOM();

     drmkms->core = core;

     ret = core_arena_get_shared_field( core, "drmkms", (void**) &shared );
     if (ret) {
          D_FREE( drmkms );
          return ret;
     }

     drmkms->shared = shared;

     ret = local_init( shared->device_name, shared->mirror_outputs, shared->multihead_outputs, drmkms );
     if (ret)
          goto error;

     *ret_data = drmkms;

     ret = dfb_surface_pool_join( core, shared->pool, &drmkmsSurfacePoolFuncs );
     if (ret)
          goto error;

     return DFB_OK;

error:
     local_deinit( drmkms );

     D_FREE( drmkms );

     return ret;
}

static DFBResult
system_shutdown( bool emergency )
{
     DRMKMSData       *drmkms = dfb_system_data();
     DRMKMSDataShared *shared;
     VideoMode        *mode;

     D_DEBUG_AT( DRMKMS_System, "%s()\n", __FUNCTION__ );

     D_ASSERT( drmkms != NULL );
     D_ASSERT( drmkms->shared != NULL );

     shared = drmkms->shared;

     dfb_surface_pool_destroy( shared->pool );

     mode = shared->modes;
     while (mode) {
          VideoMode *next = mode->next;
          SHFREE( shared->shmpool, mode );
          mode = next;
     }

     if (drmkms->crtc) {
          drmModeSetCrtc( drmkms->fd, drmkms->crtc->crtc_id, drmkms->crtc->buffer_id, drmkms->crtc->x, drmkms->crtc->y,
                          &drmkms->connector[0]->connector_id, 1, &drmkms->crtc->mode );

          drmModeFreeCrtc( drmkms->crtc );
     }

     if (shared->vt)
          drmkms_vt_shutdown( emergency );

     local_deinit( drmkms );

     SHFREE( shared->shmpool, shared );

     D_FREE( drmkms );

     return DFB_OK;
}

static DFBResult
system_leave( bool emergency )
{
     DRMKMSData       *drmkms = dfb_system_data();
     DRMKMSDataShared *shared;

     D_DEBUG_AT( DRMKMS_System, "%s()\n", __FUNCTION__ );

     D_ASSERT( drmkms != NULL );
     D_ASSERT( drmkms->shared != NULL );

     shared = drmkms->shared;

     dfb_surface_pool_leave( shared->pool );

     local_deinit( drmkms );

     D_FREE( drmkms );

     return DFB_OK;
}

static DFBResult
system_suspend()
{
     return DFB_OK;
}

static DFBResult
system_resume()
{
     return DFB_OK;
}

static VideoMode *
system_get_modes()
{
     DRMKMSData       *drmkms = dfb_system_data();
     DRMKMSDataShared *shared;

     D_ASSERT( drmkms != NULL );
     D_ASSERT( drmkms->shared != NULL );

     shared = drmkms->shared;

     return shared->modes;
}

static VideoMode *
system_get_current_mode()
{
     DRMKMSData       *drmkms = dfb_system_data();
     DRMKMSDataShared *shared;
     VideoMode        *mode;

     D_ASSERT( drmkms != NULL );
     D_ASSERT( drmkms->shared != NULL );

     shared = drmkms->shared;

     mode = shared->modes;
     while (mode) {
          if (mode->xres == shared->mode[0].hdisplay && mode->yres == shared->mode[0].vdisplay)
               break;

          mode = mode->next;
     }

     return mode;
}

static DFBResult
system_thread_init()
{
     return DFB_OK;
}

static bool
system_input_filter( CoreInputDevice *device,
                     DFBInputEvent   *event )
{
     DRMKMSData       *drmkms = dfb_system_data();
     DRMKMSDataShared *shared;

     D_ASSERT( drmkms != NULL );
     D_ASSERT( drmkms->shared != NULL );

     shared = drmkms->shared;

     if (shared->vt) {
          if (DFB_KEY_TYPE( event->key_symbol ) == DIKT_FUNCTION && event->modifiers == (DIMM_CONTROL | DIMM_ALT) &&
              (event->type == DIET_KEYPRESS || event->type == DIET_KEYRELEASE))
               return drmkms_vt_switch_num( event->key_symbol - DIKS_F1 + 1, event->type == DIET_KEYPRESS );
     }

     return false;
}

static volatile void *
system_map_mmio( unsigned int offset,
                 int          length )
{
     return NULL;
}

static void
system_unmap_mmio( volatile void *addr,
                   int            length )
{
}

static unsigned int
system_get_accelerator()
{
     return direct_config_get_int_value( "accelerator" );
}

static unsigned long
system_video_memory_physical( unsigned int offset )
{
     unsigned long  video_phys;
     const char    *value;

     /* Physical start of video memory. */
     if ((value = direct_config_get_value( "video-phys" ))) {
          char *error;

          video_phys = strtoul( value, &error, 16 );

          if (*error) {
               D_ERROR( "DRMKMS/System: 'video-phys': Error in value '%s'!\n", error );
               video_phys = 0;
          }
     }
     else
          video_phys = 0;

     return video_phys + offset;
}

static void *
system_video_memory_virtual( unsigned int offset )
{
     return NULL;
}

static unsigned int
system_videoram_length()
{
     /* Length of video memory. */
     unsigned int video_length = direct_config_get_int_value( "video-length" );

     return video_length;
}

static void
system_get_busid( int *ret_bus,
                  int *ret_dev,
                  int *ret_func )
{
}

static void
system_get_deviceid( unsigned int *ret_vendor_id,
                     unsigned int *ret_device_id )
{
}

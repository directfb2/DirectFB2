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
#include <direct/thread.h>
#include <fusion/shmalloc.h>

#include "drmkms_system.h"
#include "drmkms_vt.h"

D_DEBUG_DOMAIN( DRMKMS_System, "DRMKMS/System", "DRM/KMS System" );

DFB_CORE_SYSTEM( drmkms )

/**********************************************************************************************************************/

extern const ScreenFuncs       drmkmsScreenFuncs;
extern const DisplayLayerFuncs drmkmsPrimaryLayerFuncs;
extern const DisplayLayerFuncs drmkmsPlaneLayerFuncs;
extern const SurfacePoolFuncs  drmkmsSurfacePoolFuncs;

static void
drmkms_page_flip_handler( int           fd,
                          unsigned int  frame,
                          unsigned int  sec,
                          unsigned int  usec,
                          void         *data )
{
     DRMKMSLayerData *layer_data = data;

     D_DEBUG_AT( DRMKMS_System, "%s()\n", __FUNCTION__ );

     direct_mutex_lock( &layer_data->lock );

     if (layer_data->flip_pending) {
          dfb_surface_notify_display2( layer_data->surface, layer_data->surfacebuffer_index );

          dfb_surface_unref( layer_data->surface );
     }

     layer_data->flip_pending = false;

     direct_waitqueue_broadcast( &layer_data->wq_event );

     direct_mutex_unlock( &layer_data->lock );

     D_DEBUG_AT( DRMKMS_System, "%s() done\n", __FUNCTION__ );
}

static void *
drmkms_buffer_thread( DirectThread *thread,
                      void         *arg )
{
     DRMKMSData *drmkms = arg;

     D_DEBUG_AT( DRMKMS_System, "%s()\n", __FUNCTION__ );

     while (true)
        drmHandleEvent( drmkms->fd, &drmkms->event_context );

     return NULL;
}

static DFBResult
local_init( const char *device_name,
            DRMKMSData *drmkms )
{
     CoreScreen *screen;
     int         i;

     /* Open DRM/KMS device. */
     drmkms->fd = open( device_name, O_RDWR );
     if (drmkms->fd < 0) {
          D_PERROR( "DRMKMS/System: Failed to open '%s'!\n", device_name );
          return DFB_INIT;
     }

     /* Retrieve display configuration and planes information. */
     drmkms->resources = drmModeGetResources( drmkms->fd );
     if (!drmkms->resources) {
          D_PERROR( "DRMKMS/System: Could not retrieve resources!\n" );
          return DFB_INIT;
     }
     else
          drmkms->plane_resources = drmModeGetPlaneResources( drmkms->fd );

     D_INFO( "DRMKMS/System: Found %d connectors, %d encoders, %u planes\n",
             drmkms->resources->count_connectors, drmkms->resources->count_encoders,
             drmkms->plane_resources->count_planes );

     /* Create the mode-setting memory management object. */
     if (kms_create( drmkms->fd, &drmkms->kms ) < 0 ) {
          D_PERROR( "DRMKMS/System: Could not create the mode-setting mm object!\n" );
          return DFB_INIT;
     }

     screen = dfb_screens_register( drmkms, &drmkmsScreenFuncs );

     dfb_layers_register( screen, drmkms, &drmkmsPrimaryLayerFuncs );

     DFB_DISPLAYLAYER_IDS_ADD( drmkms->layer_ids[0], drmkms->layer_id_next++ );

     if (drmkms->plane_resources) {
          for (i = 0; i < drmkms->plane_resources->count_planes; i++) {
               dfb_layers_register( screen, drmkms, &drmkmsPlaneLayerFuncs );

               DFB_DISPLAYLAYER_IDS_ADD( drmkms->layer_ids[0], drmkms->layer_id_next++ );
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

     if (drmkms->kms)
          kms_destroy( &drmkms->kms );

     if (drmkms->fd != -1) {
          drmDropMaster( drmkms->fd );
          close( drmkms->fd );
     }

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
     DFBResult            ret;
     DRMKMSData          *drmkms;
     DRMKMSDataShared    *shared;
     FusionSHMPoolShared *pool;
     const char          *value;

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
          else
               D_ERROR( "DRMKMS/System: 'connected-outputs': Unknown connected outputs setting '%s'!\n", value );
     }

     ret = local_init( shared->device_name, drmkms );
     if (ret)
          goto error;

     if (shared->vt) {
          ret = vt_initialize( core );
          if (ret)
               goto error;
     }

     drmkms->event_context.version           = DRM_EVENT_CONTEXT_VERSION;
     drmkms->event_context.vblank_handler    = drmkms_page_flip_handler;
     drmkms->event_context.page_flip_handler = drmkms_page_flip_handler;

     drmkms->thread = direct_thread_create( DTT_CRITICAL, drmkms_buffer_thread, drmkms, "DRMKMS Buffer" );

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
          vt_shutdown( false );

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

     ret = local_init( shared->device_name, drmkms );
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

     D_DEBUG_AT( DRMKMS_System, "%s()\n", __FUNCTION__ );

     D_ASSERT( drmkms != NULL );
     D_ASSERT( drmkms->shared != NULL );

     shared = drmkms->shared;

     dfb_surface_pool_destroy( shared->pool );

     if (drmkms->crtc) {
          drmModeSetCrtc( drmkms->fd, drmkms->crtc->crtc_id, drmkms->crtc->buffer_id, drmkms->crtc->x, drmkms->crtc->y,
                          &drmkms->connector[0]->connector_id, 1, &drmkms->crtc->mode );

          drmModeFreeCrtc( drmkms->crtc );
     }

     if (shared->vt)
          vt_shutdown( emergency );

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
     return NULL;
}

static VideoMode *
system_get_current_mode()
{
     return NULL;
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
               return vt_switch_num( event->key_symbol - DIKS_F1 + 1, event->type == DIET_KEYPRESS );
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

static int
system_get_accelerator()
{
     return direct_config_get_int_value("accelerator");
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
     return;
}

static void
system_get_deviceid( unsigned int *ret_vendor_id,
                     unsigned int *ret_device_id )
{
     return;
}

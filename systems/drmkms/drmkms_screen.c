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

#include <core/layers.h>
#include <core/screens.h>

#include "drmkms_mode.h"

D_DEBUG_DOMAIN( DRMKMS_Screen, "DRMKMS/Screen", "DRM/KMS Screen" );

/**********************************************************************************************************************/

extern const DisplayLayerFuncs drmkmsPrimaryLayerFuncs;

static void
drmkmsGetProp( DRMKMSData       *drmkms,
               uint32_t          prop_id,
               uint64_t          prop_value,
               void            (*callback)(drmModePropertyPtr prop, uint64_t value, void *data),
               void             *data )
{
    drmModePropertyPtr prop;

    prop = drmModeGetProperty(drmkms->fd, prop_id);

    callback(prop, prop_value, data);

    drmModeFreeProperty(prop);
}

static void
drmkmsForEachConnectorProp( DRMKMSData       *drmkms,
                            drmModeConnector *connector,
                            void            (*callback)(drmModePropertyPtr prop, uint64_t value, void *data),
                            void             *data )
{
     drmModeObjectPropertiesPtr props;
     int i;

     props = drmModeObjectGetProperties(drmkms->fd, connector->connector_id,
             DRM_MODE_OBJECT_CONNECTOR);

     if (!props)
          return;

     for (i = 0; i < props->count_props; i++)
          drmkmsGetProp(drmkms, props->props[i], props->prop_values[i], callback, data);

     drmModeFreeObjectProperties(props);
};

static void
drmkmsGetPanelOrientationCallback( drmModePropertyPtr  prop,
                                   uint64_t            value,
                                   void               *data )
{
     int i;
     int *result = (int *) data;
     int rot = 0;
     const int max_enum = 3;
     const char *orientations[max_enum + 1];
     const char *orientation;

     if(strcmp(prop->name, "panel orientation") == 0) {
          if (!drm_property_type_is(prop, DRM_MODE_PROP_ENUM)) {
               D_INFO( "DRMKMS/Screen: panel orientation property is not enum!\n" );
               return;
          }

          for (i = 0; i < prop->count_enums; i++) {
               /* The maximum enum value should be 3,.. */
               if (prop->enums[i].value > max_enum){
                    D_INFO( "DRMKMS/Screen: orientation enum out of expected range\n" );
                    return;
               }
               orientations[(int) prop->enums[i].value] = prop->enums[i].name;
          }

          if ((int) value == -1)
               orientation = "unconfigured\n";
          else {
               orientation = orientations[(int) value];
               if (strcmp(orientation, "Upside Down") == 0)
                   rot = 180;
          }

          D_INFO( "DRMKMS/Screen: configured orientation: \"%s\" (%d), rotation %d\n",
                  orientation, (int) value, rot);

          if(result)
               *result = rot;
     }
}

static int
drmkmsConnectorGetPanelOrientation( DRMKMSData       *drmkms,
                                    drmModeConnector *connector )
{
    int rotation = 0;

    drmkmsForEachConnectorProp(drmkms, connector,
             drmkmsGetPanelOrientationCallback, &rotation);

    return rotation;
}

static DFBResult
drmkmsInitScreen( CoreScreen           *screen,
                  void                 *driver_data,
                  void                 *screen_data,
                  DFBScreenDescription *description )
{
     DRMKMSData       *drmkms    = driver_data;
     DRMKMSDataShared *shared;
     drmModeRes       *resources;
     drmModeConnector *connector = NULL;
     drmModeEncoder   *encoder   = NULL;
     drmModeModeInfo  *mode;
     int               busy, i, j, k;

     D_DEBUG_AT( DRMKMS_Screen, "%s()\n", __FUNCTION__ );

     D_ASSERT( drmkms != NULL );
     D_ASSERT( drmkms->shared != NULL );
     D_ASSERT( drmkms->resources != NULL );

     shared = drmkms->shared;

     resources = drmkms->resources;

     /* Set capabilities. */
     description->caps = DSCCAPS_MIXERS | DSCCAPS_ENCODERS | DSCCAPS_OUTPUTS;

     /* Set name. */
     snprintf( description->name, DFB_SCREEN_DESC_NAME_LENGTH, "DRMKMS Screen" );

     for (i = 0; i < resources->count_connectors; i++) {
          connector = drmModeGetConnector( drmkms->fd, resources->connectors[i] );
          if (!connector)
               continue;

          if (connector->count_modes > 0) {
               D_DEBUG_AT( DRMKMS_Screen, "  -> found connector %u\n", connector->connector_id );

               if (connector->encoder_id) {
                    D_DEBUG_AT( DRMKMS_Screen, "  -> connector is bound to encoder %u\n", connector->encoder_id );

                    encoder = drmModeGetEncoder( drmkms->fd, connector->encoder_id );
                    if (!encoder)
                         continue;
               }
               else {
                    D_DEBUG_AT( DRMKMS_Screen, "  -> searching for appropriate encoder\n" );

                    for (j = 0; j < resources->count_encoders; j++) {
                         encoder = drmModeGetEncoder( drmkms->fd, resources->encoders[j] );
                         if (!encoder)
                              continue;

                         busy = 0;

                         for (k = 0; k < shared->enabled_crtcs; k++) {
                              if (drmkms->encoder[k]->encoder_id == encoder->encoder_id) {
                                   D_DEBUG_AT( DRMKMS_Screen, "  -> encoder %u is already in use by connector %u\n",
                                               encoder->encoder_id, drmkms->connector[k]->connector_id );
                                   busy = 1;
                              }
                         }

                         if (busy)
                              continue;

                         D_DEBUG_AT( DRMKMS_Screen, "  -> found encoder %u\n", encoder->encoder_id );
                         break;
                    }
               }

               if (encoder) {
                    if (encoder->crtc_id) {
                         D_DEBUG_AT( DRMKMS_Screen, "  -> encoder is bound to crtc %u\n", encoder->crtc_id );
                    }
                    else {
                         D_DEBUG_AT( DRMKMS_Screen, "  -> searching for appropriate crtc\n" );

                         for (j = 0; j < resources->count_crtcs; j++) {
                              if (!(encoder->possible_crtcs & (1 << j)))
                                   continue;

                              busy = 0;

                              for (k = 0; k < shared->enabled_crtcs; k++) {
                                   if (drmkms->encoder[k]->crtc_id == resources->crtcs[j]) {
                                        D_DEBUG_AT( DRMKMS_Screen, "  -> crtc %u is already in use by encoder %u\n",
                                                    resources->crtcs[j], drmkms->encoder[k]->encoder_id );
                                        busy = 1;
                                   }
                              }

                              if (busy)
                                   continue;

                              encoder->crtc_id = resources->crtcs[j];

                              D_DEBUG_AT( DRMKMS_Screen, "  -> found crtc %u\n", encoder->crtc_id );
                              break;
                         }

                         if (!encoder->crtc_id) {
                              D_DEBUG_AT( DRMKMS_Screen, "  -> cannot find crtc for encoder %u\n", encoder->encoder_id );
                              break;
                         }
                    }

                    drmkms->connector[shared->enabled_crtcs] = connector;
                    drmkms->encoder[shared->enabled_crtcs]   = encoder;

                    shared->mode[shared->enabled_crtcs] = connector->modes[0];

                    for (j = 0; j < connector->count_modes; j++)
                         D_DEBUG_AT( DRMKMS_Screen, "    => modes[%2d] is %ux%u@%uHz\n", j,
                                     connector->modes[j].hdisplay, connector->modes[m].vdisplay,
                                     connector->modes[j].vrefresh );

                    shared->enabled_crtcs++;

                    if ((!shared->mirror_outputs && !shared->multihead_outputs) || (shared->enabled_crtcs == 8))
                         break;

                    if (shared->multihead_outputs && shared->enabled_crtcs > 1) {
                         dfb_layers_register( screen, drmkms, &drmkmsPrimaryLayerFuncs );

                         DFB_DISPLAYLAYER_IDS_ADD( drmkms->layer_ids[shared->enabled_crtcs-1],
                                                   drmkms->layer_id_next++ );
                    }
               }
          }
     }

     drmkms->crtc = drmModeGetCrtc( drmkms->fd, drmkms->encoder[0]->crtc_id );

     if (dfb_config->mode.width && dfb_config->mode.height) {
          mode = drmkms_find_mode( drmkms, 0, dfb_config->mode.width, dfb_config->mode.height, 0 );
          if (mode)
               shared->mode[0] = *mode;

          for (i = 1; i < shared->enabled_crtcs; i++)
               shared->mode[i] = shared->mode[0];
     }

     description->mixers   = shared->enabled_crtcs;
     description->encoders = shared->enabled_crtcs;
     description->outputs  = shared->enabled_crtcs;

     D_INFO( "DRMKMS/Screen: Default mode is %ux%u (%d modes in total)\n",
             shared->mode[0].hdisplay, shared->mode[0].vdisplay, drmkms->connector[0]->count_modes );

     return DFB_OK;
}

static DFBResult
drmkmsInitMixer( CoreScreen                *screen,
                 void                      *driver_data,
                 void                      *screen_data,
                 int                        mixer,
                 DFBScreenMixerDescription *description,
                 DFBScreenMixerConfig      *config )
{
     DRMKMSData *drmkms = driver_data;

     D_DEBUG_AT( DRMKMS_Screen, "%s()\n", __FUNCTION__ );

     /* Set capabilities. */
     description->caps   = DSMCAPS_FULL;
     description->layers = drmkms->layer_ids[mixer];

     /* Set name. */
     snprintf( description->name, DFB_SCREEN_MIXER_DESC_NAME_LENGTH, "DRMKMS Mixer" );

     config->flags  = DSMCONF_LAYERS;
     config->layers = description->layers;

     return DFB_OK;
}

static DFBResult
drmkmsInitEncoder( CoreScreen                  *screen,
                   void                        *driver_data,
                   void                        *screen_data,
                   int                          encoder,
                   DFBScreenEncoderDescription *description,
                   DFBScreenEncoderConfig      *config )
{
     DRMKMSData       *drmkms = driver_data;
     DRMKMSDataShared *shared;

     D_DEBUG_AT( DRMKMS_Screen, "%s()\n", __FUNCTION__ );

     D_ASSERT( drmkms != NULL );
     D_ASSERT( drmkms->shared != NULL );

     shared = drmkms->shared;

     /* Set capabilities. */
     description->caps = DSECAPS_RESOLUTION | DSECAPS_FREQUENCY;

     /* Set name. */
     snprintf( description->name, DFB_SCREEN_ENCODER_DESC_NAME_LENGTH, "DRMKMS Encoder" );

     config->flags = DSECONF_RESOLUTION | DSECONF_FREQUENCY | DSECONF_MIXER;
     config->mixer = encoder;

     if (drmkms->encoder[encoder]) {
          drmkms_mode_to_dsor_dsef( &shared->mode[encoder], &config->resolution, &config->frequency );

          switch (drmkms->encoder[encoder]->encoder_type) {
               case DRM_MODE_ENCODER_DAC:
                    description->type = DSET_CRTC;
                    break;
               case DRM_MODE_ENCODER_LVDS:
               case DRM_MODE_ENCODER_TMDS:
                    description->type = DSET_DIGITAL;
                    break;
               case DRM_MODE_ENCODER_TVDAC:
                    description->type = DSET_TV;
                    break;
               default:
                    description->type = DSET_UNKNOWN;
          }

          description->all_resolutions = drmkms_modes_to_dsor_bitmask( drmkms, encoder );
     }
     else
          return DFB_INVARG;

     return DFB_OK;
}

static DFBResult
drmkmsInitOutput( CoreScreen                 *screen,
                  void                       *driver_data,
                  void                       *screen_data,
                  int                         output,
                  DFBScreenOutputDescription *description,
                  DFBScreenOutputConfig      *config )
{
     DRMKMSData       *drmkms = driver_data;
     DRMKMSDataShared *shared;

     D_DEBUG_AT( DRMKMS_Screen, "%s()\n", __FUNCTION__ );

     D_ASSERT( drmkms != NULL );
     D_ASSERT( drmkms->shared != NULL );

     shared = drmkms->shared;

     /* Set capabilities. */
     description->caps = DSOCAPS_RESOLUTION;

     /* Set name. */
     snprintf( description->name, DFB_SCREEN_OUTPUT_DESC_NAME_LENGTH, "DRMKMS Output" );

     config->flags   = DSOCONF_RESOLUTION | DSOCONF_ENCODER;
     config->encoder = output;

     if (drmkms->connector[output]) {
          drmkms_mode_to_dsor_dsef( &shared->mode[output], &config->resolution, NULL );

          switch (drmkms->connector[output]->connector_type) {
               case DRM_MODE_CONNECTOR_VGA:
                    description->all_connectors = DSOC_VGA;
                    description->all_signals    = DSOS_VGA;
                    break;
               case DRM_MODE_CONNECTOR_SVIDEO:
                    description->all_connectors = DSOC_YC;
                    description->all_signals    = DSOS_YC;
                    break;
               case DRM_MODE_CONNECTOR_Composite:
                    description->all_connectors = DSOC_CVBS;
                    description->all_signals    = DSOS_CVBS;
                    break;
               case DRM_MODE_CONNECTOR_Component:
                    description->all_connectors = DSOC_COMPONENT;
                    description->all_signals    = DSOS_YCBCR;
                    break;
               case DRM_MODE_CONNECTOR_HDMIA:
               case DRM_MODE_CONNECTOR_HDMIB:
                    description->all_connectors = DSOC_HDMI;
                    description->all_signals    = DSOS_HDMI;
                    break;
               case DRM_MODE_CONNECTOR_DSI:
                    description->all_connectors = DSOC_DSI;
                    description->all_signals    = DSOS_DSI;
                    break;
               default:
                    description->all_connectors = DSOC_UNKNOWN;
                    description->all_signals    = DSOC_UNKNOWN;
          }

          description->all_resolutions = drmkms_modes_to_dsor_bitmask( drmkms, output );
     }
     else
          return DFB_INVARG;

     return DFB_OK;
}

static DFBResult
drmkmsTestMixerConfig( CoreScreen                 *screen,
                       void                       *driver_data,
                       void                       *screen_data,
                       int                         mixer,
                       const DFBScreenMixerConfig *config,
                       DFBScreenMixerConfigFlags  *ret_failed )
{
     D_DEBUG_AT( DRMKMS_Screen, "%s()\n", __FUNCTION__ );

     return DFB_OK;
}

static DFBResult
drmkmsSetMixerConfig( CoreScreen                 *screen,
                      void                       *driver_data,
                      void                       *screen_data,
                      int                         mixer,
                      const DFBScreenMixerConfig *config )
{
     D_DEBUG_AT( DRMKMS_Screen, "%s()\n", __FUNCTION__ );

     return DFB_OK;
}

static DFBResult
drmkmsTestEncoderConfig( CoreScreen                   *screen,
                         void                         *driver_data,
                         void                         *screen_data,
                         int                           encoder,
                         const DFBScreenEncoderConfig *config,
                         DFBScreenEncoderConfigFlags  *ret_failed )
{
     DRMKMSData                *drmkms = driver_data;
     DRMKMSDataShared          *shared;
     drmModeModeInfo           *mode;
     DFBScreenEncoderFrequency  dsef;
     DFBScreenOutputResolution  dsor;

     D_DEBUG_AT( DRMKMS_Screen, "%s()\n", __FUNCTION__ );

     D_ASSERT( drmkms != NULL );
     D_ASSERT( drmkms->shared != NULL );

     shared = drmkms->shared;

     if (!(config->flags & (DSECONF_FREQUENCY | DSECONF_RESOLUTION)))
          return DFB_UNSUPPORTED;

     drmkms_mode_to_dsor_dsef( &shared->mode[encoder], &dsor, &dsef );

     if (config->flags & DSECONF_FREQUENCY)
          dsef = config->frequency;

     if (config->flags & DSECONF_RESOLUTION)
          dsor = config->resolution;

     mode = drmkms_dsor_dsef_to_mode( drmkms, encoder, dsor, dsef );
     if (!mode) {
          *ret_failed = config->flags & (DSECONF_RESOLUTION | DSECONF_FREQUENCY);
          return DFB_UNSUPPORTED;
     }

     if ((shared->primary_dimension[encoder].w && (shared->primary_dimension[encoder].w < mode->hdisplay) ) ||
         (shared->primary_dimension[encoder].h && (shared->primary_dimension[encoder].h < mode->vdisplay ))) {
          D_DEBUG_AT( DRMKMS_Screen, "  -> rejection of modes bigger than the current primary layer\n" );
          *ret_failed = config->flags & (DSECONF_RESOLUTION | DSECONF_FREQUENCY);
          return DFB_UNSUPPORTED;
     }

     return DFB_OK;
}

static DFBResult
drmkmsSetEncoderConfig( CoreScreen                   *screen,
                        void                         *driver_data,
                        void                         *screen_data,
                        int                           encoder,
                        const DFBScreenEncoderConfig *config )
{
     DFBResult                  ret;
     int                        err;
     DRMKMSData                *drmkms = driver_data;
     DRMKMSDataShared          *shared;
     drmModeModeInfo           *mode;
     DFBScreenEncoderFrequency  dsef;
     DFBScreenOutputResolution  dsor;

     D_DEBUG_AT( DRMKMS_Screen, "%s()\n", __FUNCTION__ );

     D_ASSERT( drmkms != NULL );
     D_ASSERT( drmkms->shared != NULL );

     shared = drmkms->shared;

     if (!(config->flags & (DSECONF_FREQUENCY | DSECONF_RESOLUTION)))
          return DFB_INVARG;

     drmkms_mode_to_dsor_dsef( &shared->mode[encoder], &dsor, &dsef );

     if (config->flags & DSECONF_FREQUENCY) {
          D_DEBUG_AT( DRMKMS_Screen, "  -> requested frequency change \n" );
          dsef = config->frequency;
     }

     if (config->flags & DSECONF_RESOLUTION) {
          D_DEBUG_AT( DRMKMS_Screen, "  -> requested resolution change \n" );
          dsor = config->resolution;
     }

     mode = drmkms_dsor_dsef_to_mode( drmkms, encoder, dsor, dsef );
     if (!mode)
          return DFB_INVARG;

     if ((shared->primary_dimension[encoder].w && (shared->primary_dimension[encoder].w < mode->hdisplay) ) ||
         (shared->primary_dimension[encoder].h && (shared->primary_dimension[encoder].h < mode->vdisplay ))) {
          D_DEBUG_AT( DRMKMS_Screen, "  -> rejection of modes bigger than the current primary layer\n" );
          return DFB_INVARG;
     }

     if (shared->primary_fb) {
          err = drmModeSetCrtc( drmkms->fd, drmkms->encoder[encoder]->crtc_id,
                                shared->primary_fb, shared->primary_rect.x, shared->primary_rect.y,
                                &drmkms->connector[encoder]->connector_id, 1, mode );
          if (err) {
               ret = errno2result( errno );
               D_PERROR( "DRMKMS/Layer: "
                         "drmModeSetCrtc( crtc_id %u, fb_id %u, xy %d,%d, connector_id %u, mode %ux%u@%uHz )"
                         " failed for encoder %d!\n", drmkms->encoder[encoder]->crtc_id,
                         shared->primary_fb, shared->primary_rect.x, shared->primary_rect.y,
                         drmkms->connector[encoder]->connector_id,
                         mode->hdisplay, mode->vdisplay, mode->vrefresh, encoder);
               return ret;
          }
     }

     shared->mode[encoder] = *mode;

     return DFB_OK;
}

static DFBResult
drmkmsTestOutputConfig( CoreScreen                  *screen,
                        void                        *driver_data,
                        void                        *screen_data,
                        int                          output,
                        const DFBScreenOutputConfig *config,
                        DFBScreenOutputConfigFlags  *ret_failed )
{
     DRMKMSData                *drmkms = driver_data;
     DRMKMSDataShared          *shared;
     drmModeModeInfo           *mode;
     DFBScreenEncoderFrequency  dsef;
     DFBScreenOutputResolution  dsor;

     D_DEBUG_AT( DRMKMS_Screen, "%s()\n", __FUNCTION__ );

     D_ASSERT( drmkms != NULL );
     D_ASSERT( drmkms->shared != NULL );

     shared = drmkms->shared;

     if (!(config->flags & DSOCONF_RESOLUTION))
          return DFB_UNSUPPORTED;

     drmkms_mode_to_dsor_dsef( &shared->mode[output], &dsor, &dsef );

     dsor = config->resolution;

     mode = drmkms_dsor_dsef_to_mode( drmkms, output, dsor, dsef );
     if (!mode) {
          *ret_failed = config->flags & DSOCONF_RESOLUTION;
          return DFB_UNSUPPORTED;
     }

     if ((shared->primary_dimension[output].w && (shared->primary_dimension[output].w < mode->hdisplay) ) ||
         (shared->primary_dimension[output].h && (shared->primary_dimension[output].h < mode->vdisplay ))) {
          D_DEBUG_AT( DRMKMS_Screen, "  -> rejection of modes bigger than the current primary layer\n" );
          *ret_failed = config->flags & DSOCONF_RESOLUTION;
          return DFB_UNSUPPORTED;
     }

     return DFB_OK;
}

static DFBResult
drmkmsSetOutputConfig( CoreScreen                  *screen,
                       void                        *driver_data,
                       void                        *screen_data,
                       int                          output,
                       const DFBScreenOutputConfig *config )
{
     DFBResult                  ret;
     int                        err;
     DRMKMSData                *drmkms = driver_data;
     DRMKMSDataShared          *shared;
     drmModeModeInfo           *mode;
     DFBScreenEncoderFrequency  dsef;
     DFBScreenOutputResolution  dsor;

     D_DEBUG_AT( DRMKMS_Screen, "%s()\n", __FUNCTION__ );

     D_ASSERT( drmkms != NULL );
     D_ASSERT( drmkms->shared != NULL );

     shared = drmkms->shared;

     if (!(config->flags & DSOCONF_RESOLUTION))
          return DFB_INVARG;

     drmkms_mode_to_dsor_dsef( &shared->mode[output], &dsor, &dsef );

     dsor = config->resolution;

     mode = drmkms_dsor_dsef_to_mode( drmkms, output, dsor, dsef );
     if (!mode)
          return DFB_INVARG;

     if ((shared->primary_dimension[output].w && (shared->primary_dimension[output].w < mode->hdisplay) ) ||
         (shared->primary_dimension[output].h && (shared->primary_dimension[output].h < mode->vdisplay ))) {
          D_DEBUG_AT( DRMKMS_Screen, "  -> rejection of modes bigger than the current primary layer\n" );
          return DFB_INVARG;
     }

     if (shared->primary_fb) {
          err = drmModeSetCrtc( drmkms->fd, drmkms->encoder[output]->crtc_id,
                                shared->primary_fb, shared->primary_rect.x, shared->primary_rect.y,
                                &drmkms->connector[output]->connector_id, 1, mode );
          if (err) {
               ret = errno2result( errno );
               D_PERROR( "DRMKMS/Layer: "
                         "drmModeSetCrtc( crtc_id %u, fb_id %u, xy %d,%d, connector_id %u, mode %ux%u@%uHz )"
                         " failed for output %d!\n", drmkms->encoder[output]->crtc_id,
                         shared->primary_fb, shared->primary_rect.x, shared->primary_rect.y,
                         drmkms->connector[output]->connector_id,
                         mode->hdisplay, mode->vdisplay, mode->vrefresh, output);
               return ret;
          }
     }

     shared->mode[output] = *mode;

     return DFB_OK;
}

static DFBResult
drmkmsGetScreenSize( CoreScreen *screen,
                     void       *driver_data,
                     void       *screen_data,
                     int        *ret_width,
                     int        *ret_height )
{
     DRMKMSData       *drmkms = driver_data;
     DRMKMSDataShared *shared;

     D_ASSERT( drmkms != NULL );
     D_ASSERT( drmkms->shared != NULL );

     shared = drmkms->shared;

     *ret_width  = shared->mode[0].hdisplay;
     *ret_height = shared->mode[0].vdisplay;

     return DFB_OK;
}

static DFBResult
drmkmsGetScreenRotation( CoreScreen *screen,
                         void       *driver_data,
                         int        *rotation )
{
     DRMKMSData       *drmkms = driver_data;

     *rotation = drmkmsConnectorGetPanelOrientation(drmkms, drmkms->connector[0]);

     return DFB_OK;
}

const ScreenFuncs drmkmsScreenFuncs = {
     .InitScreen        = drmkmsInitScreen,
     .InitMixer         = drmkmsInitMixer,
     .InitEncoder       = drmkmsInitEncoder,
     .InitOutput        = drmkmsInitOutput,
     .TestMixerConfig   = drmkmsTestMixerConfig,
     .SetMixerConfig    = drmkmsSetMixerConfig,
     .TestEncoderConfig = drmkmsTestEncoderConfig,
     .SetEncoderConfig  = drmkmsSetEncoderConfig,
     .TestOutputConfig  = drmkmsTestOutputConfig,
     .SetOutputConfig   = drmkmsSetOutputConfig,
     .GetScreenSize     = drmkmsGetScreenSize,
     .GetScreenRotation = drmkmsGetScreenRotation,
};

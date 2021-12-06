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

#include <core/CoreScreen.h>
#include <core/layers.h>
#include <core/screen.h>
#include <core/screens.h>
#include <display/idirectfbscreen.h>

D_DEBUG_DOMAIN( Screen, "IDirectFBScreen", "IDirectFBScreen Interface" );

/**********************************************************************************************************************/

/*
 * private data struct of IDirectFBScreen
 */
typedef struct {
     int                   ref;    /* reference counter */

     CoreScreen           *screen; /* the screen object */

     DFBScreenID           id;     /* screen id */
     DFBScreenDescription  desc;   /* description of the display encoder capabilities */
} IDirectFBScreen_data;

static DFBResult PatchMixerConfig  ( DFBScreenMixerConfig   *patched, const DFBScreenMixerConfig   *patch );
static DFBResult PatchEncoderConfig( DFBScreenEncoderConfig *patched, const DFBScreenEncoderConfig *patch );
static DFBResult PatchOutputConfig ( DFBScreenOutputConfig  *patched, const DFBScreenOutputConfig  *patch );

typedef struct {
     CoreScreen              *screen;
     DFBDisplayLayerCallback  callback;
     void                    *callback_ctx;
} EnumDisplayLayers_Context;

static DFBEnumerationResult EnumDisplayLayers_Callback( CoreLayer *layer, void *ctx );

/**********************************************************************************************************************/

static void
IDirectFBScreen_Destruct( IDirectFBScreen *thiz )
{
     D_DEBUG_AT( Screen, "%s( %p )\n", __FUNCTION__, thiz );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DirectResult
IDirectFBScreen_AddRef( IDirectFBScreen *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBScreen )

     D_DEBUG_AT( Screen, "%s( %p )\n", __FUNCTION__, thiz );

     data->ref++;

     return DFB_OK;
}

static DirectResult
IDirectFBScreen_Release( IDirectFBScreen *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBScreen )

     D_DEBUG_AT( Screen, "%s( %p )\n", __FUNCTION__, thiz );

     if (--data->ref == 0)
          IDirectFBScreen_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBScreen_GetID( IDirectFBScreen *thiz,
                       DFBScreenID     *ret_screen_id )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBScreen )

     D_DEBUG_AT( Screen, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_screen_id)
          return DFB_INVARG;

     *ret_screen_id = data->id;

     return DFB_OK;
}

static DFBResult
IDirectFBScreen_GetDescription( IDirectFBScreen      *thiz,
                                DFBScreenDescription *ret_desc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBScreen )

     D_DEBUG_AT( Screen, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_desc)
          return DFB_INVARG;

     *ret_desc = data->desc;

     return DFB_OK;
}

static DFBResult
IDirectFBScreen_GetSize( IDirectFBScreen *thiz,
                         int             *ret_width,
                         int             *ret_height )
{
     DFBResult    ret;
     DFBDimension size;

     DIRECT_INTERFACE_GET_DATA( IDirectFBScreen )

     D_DEBUG_AT( Screen, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_width && !ret_height)
          return DFB_INVARG;

     ret = CoreScreen_GetScreenSize( data->screen, &size );

     if (ret_width)
          *ret_width = size.w;

     if (ret_height)
          *ret_height = size.h;

     return ret;
}

static DFBResult
IDirectFBScreen_EnumDisplayLayers( IDirectFBScreen         *thiz,
                                   DFBDisplayLayerCallback  callback,
                                   void                    *callbackdata )
{
     EnumDisplayLayers_Context context;

     DIRECT_INTERFACE_GET_DATA( IDirectFBScreen )

     D_DEBUG_AT( Screen, "%s( %p )\n", __FUNCTION__, thiz );

     if (!callback)
          return DFB_INVARG;

     context.screen       = data->screen;
     context.callback     = callback;
     context.callback_ctx = callbackdata;

     dfb_layers_enumerate( EnumDisplayLayers_Callback, &context );

     return DFB_OK;
}

static DFBResult
IDirectFBScreen_SetPowerMode( IDirectFBScreen    *thiz,
                              DFBScreenPowerMode  mode )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBScreen )

     D_DEBUG_AT( Screen, "%s( %p )\n", __FUNCTION__, thiz );

     switch (mode) {
          case DSPM_ON:
          case DSPM_STANDBY:
          case DSPM_SUSPEND:
          case DSPM_OFF:
               break;

          default:
               return DFB_INVARG;
     }

     return CoreScreen_SetPowerMode( data->screen, mode );
}

static DFBResult
IDirectFBScreen_WaitForSync( IDirectFBScreen *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBScreen )

     D_DEBUG_AT( Screen, "%s( %p )\n", __FUNCTION__, thiz );

     return CoreScreen_WaitVSync( data->screen );
}

static DFBResult
IDirectFBScreen_GetMixerDescriptions( IDirectFBScreen           *thiz,
                                      DFBScreenMixerDescription *ret_descriptions )
{
     int i;

     DIRECT_INTERFACE_GET_DATA( IDirectFBScreen )

     D_DEBUG_AT( Screen, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_descriptions)
          return DFB_INVARG;

     if (!(data->desc.caps & DSCCAPS_MIXERS))
          return DFB_UNSUPPORTED;

     for (i = 0; i < data->desc.mixers; i++)
          dfb_screen_get_mixer_info( data->screen, i, &ret_descriptions[i] );

     return DFB_OK;
}

static DFBResult
IDirectFBScreen_GetMixerConfiguration( IDirectFBScreen      *thiz,
                                       int                   mixer,
                                       DFBScreenMixerConfig *ret_config )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBScreen )

     D_DEBUG_AT( Screen, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_config)
          return DFB_INVARG;

     if (!(data->desc.caps & DSCCAPS_MIXERS))
          return DFB_UNSUPPORTED;

     if (mixer < 0 || mixer >= data->desc.mixers)
          return DFB_INVARG;

     return dfb_screen_get_mixer_config( data->screen, mixer, ret_config );
}

static DFBResult
IDirectFBScreen_TestMixerConfiguration( IDirectFBScreen            *thiz,
                                        int                         mixer,
                                        const DFBScreenMixerConfig *config,
                                        DFBScreenMixerConfigFlags  *ret_failed )
{
     DFBResult            ret;
     DFBScreenMixerConfig patched;

     DIRECT_INTERFACE_GET_DATA( IDirectFBScreen )

     D_DEBUG_AT( Screen, "%s( %p )\n", __FUNCTION__, thiz );

     if (!config || (config->flags & ~DSMCONF_ALL))
          return DFB_INVARG;

     if (!(data->desc.caps & DSCCAPS_MIXERS))
          return DFB_UNSUPPORTED;

     if (mixer < 0 || mixer >= data->desc.mixers)
          return DFB_INVARG;

     /* Get the current configuration. */
     ret = dfb_screen_get_mixer_config( data->screen, mixer, &patched );
     if (ret)
          return ret;

     /* Patch the configuration. */
     ret = PatchMixerConfig( &patched, config );
     if (ret)
          return ret;

     /* Test the patched configuration. */
     return CoreScreen_TestMixerConfig( data->screen, mixer, &patched, ret_failed );
}

static DFBResult
IDirectFBScreen_SetMixerConfiguration( IDirectFBScreen            *thiz,
                                       int                         mixer,
                                       const DFBScreenMixerConfig *config )
{
     DFBResult            ret;
     DFBScreenMixerConfig patched;

     DIRECT_INTERFACE_GET_DATA( IDirectFBScreen )

     D_DEBUG_AT( Screen, "%s( %p )\n", __FUNCTION__, thiz );

     if (!config || (config->flags & ~DSMCONF_ALL))
          return DFB_INVARG;

     if (!(data->desc.caps & DSCCAPS_MIXERS))
          return DFB_UNSUPPORTED;

     if (mixer < 0 || mixer >= data->desc.mixers)
          return DFB_INVARG;

     /* Get the current configuration. */
     ret = dfb_screen_get_mixer_config( data->screen, mixer, &patched );
     if (ret)
          return ret;

     /* Patch the configuration. */
     ret = PatchMixerConfig( &patched, config );
     if (ret)
          return ret;

     /* Set the patched configuration. */
     return CoreScreen_SetMixerConfig( data->screen, mixer, &patched );
}

static DFBResult
IDirectFBScreen_GetEncoderDescriptions( IDirectFBScreen             *thiz,
                                        DFBScreenEncoderDescription *ret_descriptions )
{
     int i;

     DIRECT_INTERFACE_GET_DATA( IDirectFBScreen )

     D_DEBUG_AT( Screen, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_descriptions)
          return DFB_INVARG;

     if (!(data->desc.caps & DSCCAPS_ENCODERS))
          return DFB_UNSUPPORTED;

     for (i = 0; i < data->desc.encoders; i++)
          dfb_screen_get_encoder_info( data->screen, i, &ret_descriptions[i] );

     return DFB_OK;
}

static DFBResult
IDirectFBScreen_GetEncoderConfiguration( IDirectFBScreen        *thiz,
                                         int                     encoder,
                                         DFBScreenEncoderConfig *ret_config )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBScreen )

     D_DEBUG_AT( Screen, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_config)
          return DFB_INVARG;

     if (!(data->desc.caps & DSCCAPS_ENCODERS))
          return DFB_UNSUPPORTED;

     if (encoder < 0 || encoder >= data->desc.encoders)
          return DFB_INVARG;

     return dfb_screen_get_encoder_config( data->screen, encoder, ret_config );
}

static DFBResult
IDirectFBScreen_TestEncoderConfiguration( IDirectFBScreen              *thiz,
                                          int                           encoder,
                                          const DFBScreenEncoderConfig *config,
                                          DFBScreenEncoderConfigFlags  *ret_failed )
{
     DFBResult              ret;
     DFBScreenEncoderConfig patched;

     DIRECT_INTERFACE_GET_DATA( IDirectFBScreen )

     D_DEBUG_AT( Screen, "%s( %p )\n", __FUNCTION__, thiz );

     if (!config || (config->flags & ~DSECONF_ALL))
          return DFB_INVARG;

     if (!(data->desc.caps & DSCCAPS_ENCODERS))
          return DFB_UNSUPPORTED;

     if (encoder < 0 || encoder >= data->desc.encoders)
          return DFB_INVARG;

     /* Get the current configuration. */
     ret = dfb_screen_get_encoder_config( data->screen, encoder, &patched );
     if (ret)
          return ret;

     /* Patch the configuration. */
     ret = PatchEncoderConfig( &patched, config );
     if (ret)
          return ret;

     /* Test the patched configuration. */
     return CoreScreen_TestEncoderConfig( data->screen, encoder, &patched, ret_failed );
}

static DFBResult
IDirectFBScreen_SetEncoderConfiguration( IDirectFBScreen              *thiz,
                                         int                           encoder,
                                         const DFBScreenEncoderConfig *config )
{
     DFBResult              ret;
     DFBScreenEncoderConfig patched;

     DIRECT_INTERFACE_GET_DATA( IDirectFBScreen )

     D_DEBUG_AT( Screen, "%s( %p )\n", __FUNCTION__, thiz );

     if (!config || (config->flags & ~DSECONF_ALL))
          return DFB_INVARG;

     if (!(data->desc.caps & DSCCAPS_ENCODERS))
          return DFB_UNSUPPORTED;

     if (encoder < 0 || encoder >= data->desc.encoders)
          return DFB_INVARG;

     /* Get the current configuration. */
     ret = dfb_screen_get_encoder_config( data->screen, encoder, &patched );
     if (ret)
          return ret;

     /* Patch the configuration. */
     ret = PatchEncoderConfig( &patched, config );
     if (ret)
          return ret;

     /* Set the patched configuration. */
     return CoreScreen_SetEncoderConfig( data->screen, encoder, &patched );
}

static DFBResult
IDirectFBScreen_GetOutputDescriptions( IDirectFBScreen            *thiz,
                                       DFBScreenOutputDescription *ret_descriptions )
{
     int i;

     DIRECT_INTERFACE_GET_DATA( IDirectFBScreen )

     D_DEBUG_AT( Screen, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_descriptions)
          return DFB_INVARG;

     if (!(data->desc.caps & DSCCAPS_OUTPUTS))
          return DFB_UNSUPPORTED;

     for (i = 0; i < data->desc.outputs; i++)
          dfb_screen_get_output_info( data->screen, i, &ret_descriptions[i] );

     return DFB_OK;
}

static DFBResult
IDirectFBScreen_GetOutputConfiguration( IDirectFBScreen       *thiz,
                                        int                    output,
                                        DFBScreenOutputConfig *ret_config )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBScreen )

     D_DEBUG_AT( Screen, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_config)
          return DFB_INVARG;

     if (!(data->desc.caps & DSCCAPS_OUTPUTS))
          return DFB_UNSUPPORTED;

     if (output < 0 || output >= data->desc.outputs)
          return DFB_INVARG;

     return dfb_screen_get_output_config( data->screen, output, ret_config );
}

static DFBResult
IDirectFBScreen_TestOutputConfiguration( IDirectFBScreen             *thiz,
                                         int                          output,
                                         const DFBScreenOutputConfig *config,
                                         DFBScreenOutputConfigFlags  *ret_failed )
{
     DFBResult             ret;
     DFBScreenOutputConfig patched;

     DIRECT_INTERFACE_GET_DATA( IDirectFBScreen )

     D_DEBUG_AT( Screen, "%s( %p )\n", __FUNCTION__, thiz );

     if (!config || (config->flags & ~DSOCONF_ALL))
          return DFB_INVARG;

     if (!(data->desc.caps & DSCCAPS_OUTPUTS))
          return DFB_UNSUPPORTED;

     if (output < 0 || output >= data->desc.outputs)
          return DFB_INVARG;

     /* Get the current configuration. */
     ret = dfb_screen_get_output_config( data->screen, output, &patched );
     if (ret)
          return ret;

     /* Patch the configuration. */
     ret = PatchOutputConfig( &patched, config );
     if (ret)
          return ret;

     /* Test the patched configuration. */
     return CoreScreen_TestOutputConfig( data->screen, output, &patched, ret_failed );
}

static DFBResult
IDirectFBScreen_SetOutputConfiguration( IDirectFBScreen             *thiz,
                                        int                          output,
                                        const DFBScreenOutputConfig *config )
{
     DFBResult             ret;
     DFBScreenOutputConfig patched;

     DIRECT_INTERFACE_GET_DATA( IDirectFBScreen )

     D_DEBUG_AT( Screen, "%s( %p )\n", __FUNCTION__, thiz );

     if (!config || (config->flags & ~DSOCONF_ALL))
          return DFB_INVARG;

     if (!(data->desc.caps & DSCCAPS_OUTPUTS))
          return DFB_UNSUPPORTED;

     if (output < 0 || output >= data->desc.outputs)
          return DFB_INVARG;

     /* Get the current configuration. */
     ret = dfb_screen_get_output_config( data->screen, output, &patched );
     if (ret)
          return ret;

     /* Patch the configuration. */
     ret = PatchOutputConfig( &patched, config );
     if (ret)
          return ret;

     /* Set the patched configuration. */
     return CoreScreen_SetOutputConfig( data->screen, output, &patched );
}

static DFBResult
IDirectFBScreen_GetVSyncCount( IDirectFBScreen *thiz,
                               unsigned long   *ret_count )
{
     DFBResult ret;
     u64       count;

     DIRECT_INTERFACE_GET_DATA( IDirectFBScreen )

     D_DEBUG_AT( Screen, "%s( %p )\n", __FUNCTION__, thiz );

     D_ASSERT( ret_count != NULL );

     ret = CoreScreen_GetVSyncCount( data->screen, &count );
     if (ret)
          return ret;

     *ret_count = count;

     return DFB_OK;
}

DFBResult
IDirectFBScreen_Construct( IDirectFBScreen *thiz,
                           CoreScreen      *screen )
{
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBScreen )

     D_DEBUG_AT( Screen, "%s( %p )\n", __FUNCTION__, thiz );

     data->ref    = 1;
     data->screen = screen;
     data->id     = dfb_screen_id_translated( screen );

     dfb_screen_get_info( screen, NULL, &data->desc );

     thiz->AddRef                   = IDirectFBScreen_AddRef;
     thiz->Release                  = IDirectFBScreen_Release;
     thiz->GetID                    = IDirectFBScreen_GetID;
     thiz->GetDescription           = IDirectFBScreen_GetDescription;
     thiz->GetSize                  = IDirectFBScreen_GetSize;
     thiz->EnumDisplayLayers        = IDirectFBScreen_EnumDisplayLayers;
     thiz->SetPowerMode             = IDirectFBScreen_SetPowerMode;
     thiz->WaitForSync              = IDirectFBScreen_WaitForSync;
     thiz->GetMixerDescriptions     = IDirectFBScreen_GetMixerDescriptions;
     thiz->GetMixerConfiguration    = IDirectFBScreen_GetMixerConfiguration;
     thiz->TestMixerConfiguration   = IDirectFBScreen_TestMixerConfiguration;
     thiz->SetMixerConfiguration    = IDirectFBScreen_SetMixerConfiguration;
     thiz->GetEncoderDescriptions   = IDirectFBScreen_GetEncoderDescriptions;
     thiz->GetEncoderConfiguration  = IDirectFBScreen_GetEncoderConfiguration;
     thiz->TestEncoderConfiguration = IDirectFBScreen_TestEncoderConfiguration;
     thiz->SetEncoderConfiguration  = IDirectFBScreen_SetEncoderConfiguration;
     thiz->GetOutputDescriptions    = IDirectFBScreen_GetOutputDescriptions;
     thiz->GetOutputConfiguration   = IDirectFBScreen_GetOutputConfiguration;
     thiz->TestOutputConfiguration  = IDirectFBScreen_TestOutputConfiguration;
     thiz->SetOutputConfiguration   = IDirectFBScreen_SetOutputConfiguration;
     thiz->GetVSyncCount            = IDirectFBScreen_GetVSyncCount;

     return DFB_OK;
}

/**********************************************************************************************************************/

static DFBResult
PatchMixerConfig( DFBScreenMixerConfig       *patched,
                  const DFBScreenMixerConfig *patch )
{
     /* Check for unsupported flags. */
     if (patch->flags & ~patched->flags)
          return DFB_UNSUPPORTED;

     if (patch->flags & DSMCONF_TREE)
          patched->tree = patch->tree;

     if (patch->flags & DSMCONF_LEVEL)
          patched->level = patch->level;

     if (patch->flags & DSMCONF_LAYERS)
          patched->layers = patch->layers;

     if (patch->flags & DSMCONF_BACKGROUND)
          patched->background = patch->background;

     return DFB_OK;
}

static DFBResult
PatchEncoderConfig( DFBScreenEncoderConfig       *patched,
                    const DFBScreenEncoderConfig *patch )
{
     /* Check for unsupported flags. */
     if (patch->flags & ~patched->flags)
          return DFB_UNSUPPORTED;

     if (patch->flags & DSECONF_RESOLUTION)
          patched->resolution = patch->resolution;

     if (patch->flags & DSECONF_FREQUENCY)
          patched->frequency = patch->frequency;

     /* If you have set a DSECONF_TV_STANDARD, this will override the resolution and frequency chosen above.*/
     if (patch->flags & DSECONF_TV_STANDARD) {
          patched->tv_standard = patch->tv_standard;
          switch (patched->tv_standard) {
               case DSETV_PAL:
               case DSETV_PAL_BG:
               case DSETV_PAL_I:
               case DSETV_PAL_N:
               case DSETV_PAL_NC:
                    patched->resolution = DSOR_720_576;
                    patched->frequency = DSEF_50HZ;
                    break;

               case DSETV_PAL_60:
               case DSETV_PAL_M:
                    patched->resolution = DSOR_720_480;
                    patched->frequency = DSEF_59_94HZ;
                    break;

               case DSETV_SECAM:
                    patched->resolution = DSOR_720_576;
                    patched->frequency = DSEF_50HZ;
                    break;

               case DSETV_NTSC:
               case DSETV_NTSC_M_JPN:
               case DSETV_NTSC_443:
                    patched->resolution = DSOR_720_480;
                    patched->frequency = DSEF_59_94HZ;
                    break;

               default:
                    break;
          }
     }

     if (patch->flags & DSECONF_TEST_PICTURE)
          patched->test_picture = patch->test_picture;

     if (patch->flags & DSECONF_MIXER)
          patched->mixer = patch->mixer;

     if (patch->flags & DSECONF_OUT_SIGNALS)
          patched->out_signals = patch->out_signals;

     if (patch->flags & DSECONF_SCANMODE)
          patched->scanmode = patch->scanmode;

     if (patch->flags & DSECONF_TEST_COLOR)
          patched->test_color = patch->test_color;

     if (patch->flags & DSECONF_ADJUSTMENT)
          patched->adjustment = patch->adjustment;

     if (patch->flags & DSECONF_CONNECTORS)
          patched->out_connectors = patch->out_connectors;

     if (patch->flags & DSECONF_SLOW_BLANKING)
          patched->slow_blanking = patch->slow_blanking;

     if (patch->flags & DSECONF_FRAMING)
          patched->framing = patch->framing;

     if (patch->flags & DSECONF_ASPECT_RATIO)
          patched->aspect_ratio = patch->aspect_ratio;

     return DFB_OK;
}

static DFBResult
PatchOutputConfig( DFBScreenOutputConfig       *patched,
                   const DFBScreenOutputConfig *patch )
{
     /* Check for unsupported flags. */
     if (patch->flags & ~patched->flags)
          return DFB_UNSUPPORTED;

     if (patch->flags & DSOCONF_ENCODER)
          patched->encoder = patch->encoder;

     if (patch->flags & DSOCONF_SIGNALS)
          patched->out_signals = patch->out_signals;

     if (patch->flags & DSOCONF_CONNECTORS)
          patched->out_connectors = patch->out_connectors;

     if (patch->flags & DSOCONF_SLOW_BLANKING)
          patched->slow_blanking = patch->slow_blanking;

     if (patch->flags & DSOCONF_RESOLUTION)
          patched->resolution = patch->resolution;

     return DFB_OK;
}

static DFBEnumerationResult
EnumDisplayLayers_Callback( CoreLayer *layer,
                            void      *ctx )
{
     DFBDisplayLayerDescription  desc;
     DFBDisplayLayerID           id;
     EnumDisplayLayers_Context  *context = ctx;

     if (layer->screen != context->screen)
          return DFENUM_OK;

     id = dfb_layer_id_translated( layer );

     if (dfb_config->primary_only && id != DLID_PRIMARY)
          return DFENUM_OK;

     dfb_layer_get_description( layer, &desc );

     return context->callback( id, desc, context->callback_ctx );
}

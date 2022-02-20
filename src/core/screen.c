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
#include <core/screen.h>
#include <core/screens.h>

D_DEBUG_DOMAIN( Core_Screen, "Core/Screen", "DirectFB Core Screen" );

/**********************************************************************************************************************/

DFBResult
dfb_screen_get_info( CoreScreen           *screen,
                     DFBScreenID          *ret_id,
                     DFBScreenDescription *ret_desc )
{
     CoreScreenShared *shared;

     D_ASSERT( screen != NULL );
     D_ASSERT( screen->shared != NULL );

     shared = screen->shared;

     D_DEBUG_AT( Core_Screen, "%s() -> %u\n", __FUNCTION__, shared->screen_id );

     if (ret_id)
          *ret_id = shared->screen_id;

     if (ret_desc)
          *ret_desc = shared->description;

     return DFB_OK;
}

DFBResult
dfb_screen_suspend( CoreScreen *screen )
{
     D_ASSERT( screen != NULL );

     return DFB_OK;
}

DFBResult
dfb_screen_resume( CoreScreen *screen )
{
     D_ASSERT( screen != NULL );

     return DFB_OK;
}

DFBResult
dfb_screen_set_powermode( CoreScreen         *screen,
                          DFBScreenPowerMode  mode )
{
     const ScreenFuncs *funcs;

     D_ASSERT( screen != NULL );
     D_ASSERT( screen->funcs != NULL );

     funcs = screen->funcs;

     if (funcs->SetPowerMode)
          return funcs->SetPowerMode( screen, screen->driver_data, screen->screen_data, mode );

     return DFB_UNSUPPORTED;
}

DFBResult
dfb_screen_wait_vsync( CoreScreen *screen )
{
     const ScreenFuncs *funcs;

     D_ASSERT( screen != NULL );
     D_ASSERT( screen->funcs != NULL );

     funcs = screen->funcs;

     if (funcs->WaitVSync)
          return funcs->WaitVSync( screen, screen->driver_data, screen->screen_data );

     return DFB_UNSUPPORTED;
}

DFBResult
dfb_screen_get_vsync_count( CoreScreen    *screen,
                            unsigned long *ret_count )
{
     const ScreenFuncs *funcs;

     D_ASSERT( screen != NULL );
     D_ASSERT( screen->funcs != NULL );
     D_ASSERT( ret_count != NULL );

     funcs = screen->funcs;

     if (funcs->GetVSyncCount)
          return funcs->GetVSyncCount( screen, screen->driver_data, screen->screen_data, ret_count );

     return DFB_UNSUPPORTED;
}

/**********************************************************************************************************************/

DFBResult
dfb_screen_get_mixer_info( CoreScreen                *screen,
                           int                        mixer,
                           DFBScreenMixerDescription *ret_desc )
{
     CoreScreenShared *shared;

     D_ASSERT( screen != NULL );
     D_ASSERT( screen->shared != NULL );

     shared = screen->shared;

     D_ASSERT( mixer >= 0 );
     D_ASSERT( mixer < shared->description.mixers );
     D_ASSERT( ret_desc != NULL );

     /* Return mixer description. */
     *ret_desc = shared->mixers[mixer].description;

     return DFB_OK;
}

DFBResult
dfb_screen_get_mixer_config( CoreScreen           *screen,
                             int                   mixer,
                             DFBScreenMixerConfig *ret_config )
{
     CoreScreenShared *shared;

     D_ASSERT( screen != NULL );
     D_ASSERT( screen->shared != NULL );

     shared = screen->shared;

     D_ASSERT( mixer >= 0 );
     D_ASSERT( mixer < shared->description.mixers );
     D_ASSERT( ret_config != NULL );

     /* Return current mixer configuration. */
     *ret_config = shared->mixers[mixer].configuration;

     return DFB_OK;
}

DFBResult
dfb_screen_test_mixer_config( CoreScreen                 *screen,
                              int                         mixer,
                              const DFBScreenMixerConfig *config,
                              DFBScreenMixerConfigFlags  *ret_failed )
{
     DFBResult                  ret;
     CoreScreenShared          *shared;
     const ScreenFuncs         *funcs;
     DFBScreenMixerConfigFlags  failed = DSMCONF_NONE;

     D_UNUSED_P( shared );

     D_ASSERT( screen != NULL );
     D_ASSERT( screen->shared != NULL );
     D_ASSERT( screen->funcs != NULL );
     D_ASSERT( screen->funcs->TestMixerConfig != NULL );

     shared = screen->shared;
     funcs  = screen->funcs;

     D_ASSERT( mixer >= 0 );
     D_ASSERT( mixer < shared->description.mixers );
     D_ASSERT( config != NULL );
     D_ASSERT( config->flags == shared->mixers[mixer].configuration.flags );

     /* Test the mixer configuration. */
     ret = funcs->TestMixerConfig( screen, screen->driver_data, screen->screen_data, mixer, config, &failed );

     D_ASSUME( (ret == DFB_OK && !failed) || (ret != DFB_OK) );

     if (ret_failed)
          *ret_failed = failed;

     return ret;
}

DFBResult
dfb_screen_set_mixer_config( CoreScreen                 *screen,
                             int                         mixer,
                             const DFBScreenMixerConfig *config )
{
     DFBResult                  ret;
     CoreScreenShared          *shared;
     const ScreenFuncs         *funcs;
     DFBScreenMixerConfigFlags  failed = DSOCONF_NONE;

     D_ASSERT( screen != NULL );
     D_ASSERT( screen->shared != NULL );
     D_ASSERT( screen->funcs != NULL );
     D_ASSERT( screen->funcs->TestMixerConfig != NULL );
     D_ASSERT( screen->funcs->SetMixerConfig != NULL );

     shared = screen->shared;
     funcs  = screen->funcs;

     D_ASSERT( mixer >= 0 );
     D_ASSERT( mixer < shared->description.mixers );
     D_ASSERT( config != NULL );
     D_ASSERT( config->flags == shared->mixers[mixer].configuration.flags );

     /* Test configuration first. */
     ret = funcs->TestMixerConfig( screen, screen->driver_data, screen->screen_data, mixer, config, &failed );

     D_ASSUME( (ret == DFB_OK && !failed) || (ret != DFB_OK && failed) );

     if (ret)
          return ret;

     /* Set configuration afterwards. */
     ret = funcs->SetMixerConfig( screen, screen->driver_data, screen->screen_data, mixer, config );
     if (ret)
          return ret;

     /* Store current configuration. */
     shared->mixers[mixer].configuration = *config;

     return DFB_OK;
}

/**********************************************************************************************************************/

DFBResult
dfb_screen_get_encoder_info( CoreScreen                  *screen,
                             int                          encoder,
                             DFBScreenEncoderDescription *ret_desc )
{
     CoreScreenShared *shared;

     D_ASSERT( screen != NULL );
     D_ASSERT( screen->shared != NULL );

     shared = screen->shared;

     D_ASSERT( encoder >= 0 );
     D_ASSERT( encoder < shared->description.encoders );
     D_ASSERT( ret_desc != NULL );

     /* Return encoder description. */
     *ret_desc = shared->encoders[encoder].description;

     return DFB_OK;
}

DFBResult
dfb_screen_get_encoder_config( CoreScreen             *screen,
                               int                     encoder,
                               DFBScreenEncoderConfig *ret_config )
{
     CoreScreenShared *shared;

     D_ASSERT( screen != NULL );
     D_ASSERT( screen->shared != NULL );

     shared = screen->shared;

     D_ASSERT( encoder >= 0 );
     D_ASSERT( encoder < shared->description.encoders );
     D_ASSERT( ret_config != NULL );

     /* Return current encoder configuration. */
     *ret_config = shared->encoders[encoder].configuration;

     return DFB_OK;
}

DFBResult
dfb_screen_test_encoder_config( CoreScreen                   *screen,
                                int                           encoder,
                                const DFBScreenEncoderConfig *config,
                                DFBScreenEncoderConfigFlags  *ret_failed )
{
     DFBResult                    ret;
     CoreScreenShared            *shared;
     const ScreenFuncs           *funcs;
     DFBScreenEncoderConfigFlags  failed = DSECONF_NONE;

     D_UNUSED_P( shared );

     D_ASSERT( screen != NULL );
     D_ASSERT( screen->shared != NULL );
     D_ASSERT( screen->funcs != NULL );
     D_ASSERT( screen->funcs->TestEncoderConfig != NULL );

     shared = screen->shared;
     funcs  = screen->funcs;

     D_ASSERT( encoder >= 0 );
     D_ASSERT( encoder < shared->description.encoders );
     D_ASSERT( config != NULL );
     D_ASSERT( config->flags == shared->encoders[encoder].configuration.flags );

     /* Test the encoder configuration. */
     ret = funcs->TestEncoderConfig( screen, screen->driver_data, screen->screen_data, encoder, config, &failed );

     D_ASSUME( (ret == DFB_OK && !failed) || (ret != DFB_OK && failed) );

     if (ret_failed)
          *ret_failed = failed;

     return ret;
}

DFBResult
dfb_screen_set_encoder_config( CoreScreen                   *screen,
                               int                           encoder,
                               const DFBScreenEncoderConfig *config )
{
     DFBResult                    ret;
     CoreScreenShared            *shared;
     const ScreenFuncs           *funcs;
     DFBScreenEncoderConfigFlags  failed = DSECONF_NONE;

     D_ASSERT( screen != NULL );
     D_ASSERT( screen->shared != NULL );
     D_ASSERT( screen->funcs != NULL );
     D_ASSERT( screen->funcs->TestEncoderConfig != NULL );
     D_ASSERT( screen->funcs->SetEncoderConfig != NULL );

     shared = screen->shared;
     funcs  = screen->funcs;

     D_ASSERT( encoder >= 0 );
     D_ASSERT( encoder < shared->description.encoders );
     D_ASSERT( config != NULL );
     D_ASSERT( config->flags == shared->encoders[encoder].configuration.flags );

     /* Test configuration first. */
     ret = funcs->TestEncoderConfig( screen, screen->driver_data, screen->screen_data, encoder, config, &failed );

     D_ASSUME( (ret == DFB_OK && !failed) || (ret != DFB_OK && failed) );

     if (ret)
          return ret;

     /* Set configuration afterwards. */
     ret = funcs->SetEncoderConfig( screen, screen->driver_data, screen->screen_data, encoder, config );
     if (ret)
          return ret;

     /* Store current configuration. */
     shared->encoders[encoder].configuration = *config;

     return DFB_OK;
}

/**********************************************************************************************************************/

DFBResult
dfb_screen_get_output_info( CoreScreen                 *screen,
                            int                         output,
                            DFBScreenOutputDescription *ret_desc )
{
     CoreScreenShared *shared;

     D_ASSERT( screen != NULL );
     D_ASSERT( screen->shared != NULL );

     shared = screen->shared;

     D_ASSERT( output >= 0 );
     D_ASSERT( output < shared->description.outputs );
     D_ASSERT( ret_desc != NULL );

     /* Return output description. */
     *ret_desc = shared->outputs[output].description;

     return DFB_OK;
}

DFBResult
dfb_screen_get_output_config( CoreScreen            *screen,
                              int                    output,
                              DFBScreenOutputConfig *ret_config )
{
     CoreScreenShared *shared;

     D_ASSERT( screen != NULL );
     D_ASSERT( screen->shared != NULL );

     shared = screen->shared;

     D_ASSERT( output >= 0 );
     D_ASSERT( output < shared->description.outputs );
     D_ASSERT( ret_config != NULL );

     /* Return current output configuration. */
     *ret_config = shared->outputs[output].configuration;

     return DFB_OK;
}

DFBResult
dfb_screen_test_output_config( CoreScreen                  *screen,
                               int                          output,
                               const DFBScreenOutputConfig *config,
                               DFBScreenOutputConfigFlags  *ret_failed )
{
     DFBResult                   ret;
     CoreScreenShared           *shared;
     const ScreenFuncs          *funcs;
     DFBScreenOutputConfigFlags  failed = DSOCONF_NONE;

     D_UNUSED_P( shared );

     D_ASSERT( screen != NULL );
     D_ASSERT( screen->shared != NULL );
     D_ASSERT( screen->funcs != NULL );
     D_ASSERT( screen->funcs->TestOutputConfig != NULL );

     shared = screen->shared;
     funcs  = screen->funcs;

     D_ASSERT( output >= 0 );
     D_ASSERT( output < shared->description.outputs );
     D_ASSERT( config != NULL );
     D_ASSERT( config->flags == shared->outputs[output].configuration.flags );

     /* Test the output configuration. */
     ret = funcs->TestOutputConfig( screen, screen->driver_data, screen->screen_data, output, config, &failed );

     D_ASSUME( (ret == DFB_OK && !failed) || (ret != DFB_OK && failed) );

     if (ret_failed)
          *ret_failed = failed;

     return ret;
}

DFBResult
dfb_screen_set_output_config( CoreScreen                  *screen,
                              int                          output,
                              const DFBScreenOutputConfig *config )
{
     DFBResult                   ret;
     CoreScreenShared           *shared;
     const ScreenFuncs          *funcs;
     DFBScreenOutputConfigFlags  failed = DSOCONF_NONE;

     D_ASSERT( screen != NULL );
     D_ASSERT( screen->shared != NULL );
     D_ASSERT( screen->funcs != NULL );
     D_ASSERT( screen->funcs->TestOutputConfig != NULL );
     D_ASSERT( screen->funcs->SetOutputConfig != NULL );

     shared = screen->shared;
     funcs  = screen->funcs;

     D_ASSERT( output >= 0 );
     D_ASSERT( output < shared->description.outputs );
     D_ASSERT( config != NULL );
     D_ASSERT( config->flags == shared->outputs[output].configuration.flags );

     /* Test configuration first. */
     ret = funcs->TestOutputConfig( screen, screen->driver_data, screen->screen_data, output, config, &failed );

     D_ASSUME( (ret == DFB_OK && !failed) || (ret != DFB_OK && failed) );

     if (ret)
          return ret;

     /* Set configuration afterwards. */
     ret = funcs->SetOutputConfig( screen, screen->driver_data, screen->screen_data, output, config );
     if (ret)
          return ret;

     /* Store current configuration. */
     shared->outputs[output].configuration = *config;

     return DFB_OK;
}

/**********************************************************************************************************************/

DFBResult
dfb_screen_get_screen_size( CoreScreen *screen,
                            int        *ret_width,
                            int        *ret_height )
{
     const ScreenFuncs *funcs;

     D_ASSERT( screen != NULL );
     D_ASSERT( screen->funcs != NULL );
     D_ASSERT( screen->funcs->GetScreenSize != NULL );
     D_ASSERT( ret_width != NULL );
     D_ASSERT( ret_height != NULL );

     funcs = screen->funcs;

     return funcs->GetScreenSize( screen, screen->driver_data, screen->screen_data, ret_width, ret_height );
}

DFBResult
dfb_screen_get_layer_dimension( CoreScreen *screen,
                                CoreLayer  *layer,
                                int        *ret_width,
                                int        *ret_height )
{
     int                i;
     DFBResult          ret = DFB_UNSUPPORTED;
     CoreScreenShared  *shared;
     const ScreenFuncs *funcs;

     D_ASSERT( screen != NULL );
     D_ASSERT( screen->shared != NULL );
     D_ASSERT( screen->funcs != NULL );
     D_ASSERT( layer != NULL );
     D_ASSERT( ret_width != NULL );
     D_ASSERT( ret_height != NULL );

     shared = screen->shared;
     funcs  = screen->funcs;

     if (funcs->GetMixerState) {
          for (i = 0; i < shared->description.mixers; i++) {
               const DFBScreenMixerConfig *config = &shared->mixers[i].configuration;

               if ((config->flags & DSMCONF_LAYERS) &&
                   DFB_DISPLAYLAYER_IDS_HAVE( config->layers, dfb_layer_id( layer ) )) {
                    CoreMixerState state;

                    ret = funcs->GetMixerState( screen, screen->driver_data, screen->screen_data, i, &state );
                    if (ret == DFB_OK) {
                         if (state.flags & CMSF_DIMENSION) {
                              *ret_width  = state.dimension.w;
                              *ret_height = state.dimension.h;

                              return DFB_OK;
                         }

                         ret = DFB_UNSUPPORTED;
                    }
               }
          }

          for (i = 0; i < shared->description.mixers; i++) {
               const DFBScreenMixerDescription *desc = &shared->mixers[i].description;

               if ((desc->caps & DSMCAPS_SUB_LAYERS) &&
                   DFB_DISPLAYLAYER_IDS_HAVE( desc->sub_layers, dfb_layer_id( layer ) )) {
                    CoreMixerState state;

                    ret = funcs->GetMixerState( screen, screen->driver_data, screen->screen_data, i, &state );
                    if (ret == DFB_OK) {
                         if (state.flags & CMSF_DIMENSION) {
                              *ret_width  = state.dimension.w;
                              *ret_height = state.dimension.h;

                              return DFB_OK;
                         }

                         ret = DFB_UNSUPPORTED;
                    }
               }
          }
     }

     if (funcs->GetScreenSize)
          ret = funcs->GetScreenSize( screen, screen->driver_data, screen->screen_data, ret_width, ret_height );

     return ret;
}

DFBResult
dfb_screen_get_frame_interval( CoreScreen *screen,
                               long long  *ret_micros )
{
     CoreScreenShared *shared;
     long long         interval = dfb_config->screen_frame_interval;

     D_ASSERT( screen != NULL );
     D_ASSERT( screen->shared != NULL );
     D_ASSERT( screen->funcs != NULL );

     shared = screen->shared;

     if (shared->description.encoders) {
          const DFBScreenEncoderConfig *config = &shared->encoders[0].configuration;

          if (config->flags & DSECONF_FREQUENCY) {
               switch (config->frequency) {
                    case DSEF_25HZ:
                         interval = 1000000000LL / 25000LL;
                         break;

                    case DSEF_29_97HZ:
                         interval = 1000000000LL / 29970LL;
                         break;

                    case DSEF_50HZ:
                         interval = 1000000000LL / 50000LL;
                         break;

                    case DSEF_59_94HZ:
                         interval = 1000000000LL / 59940LL;
                         break;

                    case DSEF_60HZ:
                         interval = 1000000000LL / 60000LL;
                         break;

                    case DSEF_75HZ:
                         interval = 1000000000LL / 75000LL;
                         break;

                    case DSEF_30HZ:
                         interval = 1000000000LL / 30000LL;
                         break;

                    case DSEF_24HZ:
                         interval = 1000000000LL / 24000LL;
                         break;

                    case DSEF_23_976HZ:
                         interval = 1000000000LL / 23976LL;
                         break;

                    default:
                         break;
               }
          }
     }

     *ret_micros = interval;

     return DFB_OK;
}

DFBResult
dfb_screen_get_rotation( CoreScreen *screen,
                         int        *rotation )
{
    const ScreenFuncs *funcs;

    D_ASSERT( screen != NULL );
    D_ASSERT( screen->funcs != NULL );
    D_ASSERT( rotation != NULL );

    funcs = screen->funcs;

    if (!funcs->GetScreenRotation) {
         *rotation = 0;
         return DFB_OK;
    }

    return funcs->GetScreenRotation( screen, screen->driver_data, rotation );
}

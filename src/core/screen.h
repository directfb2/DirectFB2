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

#ifndef __CORE__SCREEN_H__
#define __CORE__SCREEN_H__

#include <core/coretypes.h>

/**********************************************************************************************************************/

/*
 * Misc.
 */

DFBResult dfb_screen_get_info           ( CoreScreen                   *screen,
                                          DFBScreenID                  *ret_id,
                                          DFBScreenDescription         *ret_desc );

DFBResult dfb_screen_suspend            ( CoreScreen                   *screen );

DFBResult dfb_screen_resume             ( CoreScreen                   *screen );

DFBResult dfb_screen_set_powermode      ( CoreScreen                   *screen,
                                          DFBScreenPowerMode            mode );

DFBResult dfb_screen_wait_vsync         ( CoreScreen                   *screen );

DFBResult dfb_screen_get_vsync_count    ( CoreScreen                   *screen,
                                          unsigned long                *ret_count );

/*
 * Mixers.
 */

DFBResult dfb_screen_get_mixer_info     ( CoreScreen                   *screen,
                                          int                           mixer,
                                          DFBScreenMixerDescription    *ret_desc );

DFBResult dfb_screen_get_mixer_config   ( CoreScreen                   *screen,
                                          int                           mixer,
                                          DFBScreenMixerConfig         *ret_config );

DFBResult dfb_screen_test_mixer_config  ( CoreScreen                   *screen,
                                          int                           mixer,
                                          const DFBScreenMixerConfig   *config,
                                          DFBScreenMixerConfigFlags    *ret_failed );

DFBResult dfb_screen_set_mixer_config   ( CoreScreen                   *screen,
                                          int                           mixer,
                                          const DFBScreenMixerConfig   *config );

/*
 * Encoders.
 */

DFBResult dfb_screen_get_encoder_info   ( CoreScreen                   *screen,
                                          int                           encoder,
                                          DFBScreenEncoderDescription  *ret_desc );

DFBResult dfb_screen_get_encoder_config ( CoreScreen                   *screen,
                                          int                           encoder,
                                          DFBScreenEncoderConfig       *ret_config );

DFBResult dfb_screen_test_encoder_config( CoreScreen                   *screen,
                                          int                           encoder,
                                          const DFBScreenEncoderConfig *config,
                                          DFBScreenEncoderConfigFlags  *ret_failed );

DFBResult dfb_screen_set_encoder_config ( CoreScreen                   *screen,
                                          int                           encoder,
                                          const DFBScreenEncoderConfig *config );

/*
 * Outputs.
 */

DFBResult dfb_screen_get_output_info    ( CoreScreen                   *screen,
                                          int                           output,
                                          DFBScreenOutputDescription   *ret_desc );

DFBResult dfb_screen_get_output_config  ( CoreScreen                   *screen,
                                          int                           output,
                                          DFBScreenOutputConfig        *ret_config );

DFBResult dfb_screen_test_output_config ( CoreScreen                   *screen,
                                          int                           output,
                                          const DFBScreenOutputConfig  *config,
                                          DFBScreenOutputConfigFlags   *ret_failed );

DFBResult dfb_screen_set_output_config  ( CoreScreen                   *screen,
                                          int                           output,
                                          const DFBScreenOutputConfig  *config );

/*
 * Screen configuration.
 */

DFBResult dfb_screen_get_screen_size    ( CoreScreen                   *screen,
                                          int                          *ret_width,
                                          int                          *ret_height );

DFBResult dfb_screen_get_layer_dimension( CoreScreen                   *screen,
                                          CoreLayer                    *layer,
                                          int                          *ret_width,
                                          int                          *ret_height );

DFBResult dfb_screen_get_frame_interval ( CoreScreen                   *screen,
                                          long long                    *ret_micros );

DFBResult dfb_screen_get_rotation       ( CoreScreen *screen,
                                          int        *rotation );

#endif

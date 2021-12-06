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
#include <core/screen.h>

D_DEBUG_DOMAIN( DirectFB_CoreScreen, "DirectFB/CoreScreen", "DirectFB CoreScreen" );

/**********************************************************************************************************************/

DFBResult
IScreen_Real__GetScreenSize( CoreScreen   *obj,
                             DFBDimension *ret_size )
{
     D_DEBUG_AT( DirectFB_CoreScreen, "%s( %p )\n", __FUNCTION__, obj );

     D_ASSERT( ret_size != NULL );

     return dfb_screen_get_screen_size( obj, &ret_size->w, &ret_size->h );
}

DFBResult
IScreen_Real__SetPowerMode( CoreScreen         *obj,
                            DFBScreenPowerMode  mode )
{
     D_DEBUG_AT( DirectFB_CoreScreen, "%s( %p )\n", __FUNCTION__, obj );

     return dfb_screen_set_powermode( obj, mode );
}

DFBResult
IScreen_Real__WaitVSync( CoreScreen *obj )
{
     D_DEBUG_AT( DirectFB_CoreScreen, "%s( %p )\n", __FUNCTION__, obj );

     return dfb_screen_wait_vsync( obj );
}

DFBResult
IScreen_Real__TestMixerConfig( CoreScreen                 *obj,
                               u32                         mixer,
                               const DFBScreenMixerConfig *config,
                               DFBScreenMixerConfigFlags  *ret_failed )
{
     D_DEBUG_AT( DirectFB_CoreScreen, "%s( %p )\n", __FUNCTION__, obj );

     D_ASSERT( config != NULL );

     return dfb_screen_test_mixer_config( obj, mixer, config, ret_failed );
}

DFBResult
IScreen_Real__SetMixerConfig( CoreScreen                 *obj,
                              u32                         mixer,
                              const DFBScreenMixerConfig *config )
{
     D_DEBUG_AT( DirectFB_CoreScreen, "%s( %p )\n", __FUNCTION__, obj );

     D_ASSERT( config != NULL );

     return dfb_screen_set_mixer_config( obj, mixer, config );
}

DFBResult
IScreen_Real__TestEncoderConfig( CoreScreen                   *obj,
                                 u32                           encoder,
                                 const DFBScreenEncoderConfig *config,
                                 DFBScreenEncoderConfigFlags  *ret_failed )
{
     D_DEBUG_AT( DirectFB_CoreScreen, "%s( %p )\n", __FUNCTION__, obj );

     D_ASSERT( config != NULL );

     return dfb_screen_test_encoder_config( obj, encoder, config, ret_failed );
}

DFBResult
IScreen_Real__SetEncoderConfig( CoreScreen                   *obj,
                                u32                           encoder,
                                const DFBScreenEncoderConfig *config )
{
     D_DEBUG_AT( DirectFB_CoreScreen, "%s( %p )\n", __FUNCTION__, obj );

     D_ASSERT( config != NULL );

     return dfb_screen_set_encoder_config( obj, encoder, config );
}

DFBResult
IScreen_Real__TestOutputConfig( CoreScreen                  *obj,
                                u32                          output,
                                const DFBScreenOutputConfig *config,
                                DFBScreenOutputConfigFlags  *ret_failed )
{
     D_DEBUG_AT( DirectFB_CoreScreen, "%s( %p )\n", __FUNCTION__, obj );

     D_ASSERT( config != NULL );

     return dfb_screen_test_output_config( obj, output, config, ret_failed );
}

DFBResult
IScreen_Real__SetOutputConfig( CoreScreen                  *obj,
                               u32                          output,
                               const DFBScreenOutputConfig *config )
{
     D_DEBUG_AT( DirectFB_CoreScreen, "%s( %p )\n", __FUNCTION__, obj );

     D_ASSERT( config != NULL );

     return dfb_screen_set_output_config( obj, output, config );
}

DFBResult
IScreen_Real__GetVSyncCount( CoreScreen *obj,
                             u64        *ret_count )
{
     DFBResult     ret;
     unsigned long count;

     D_DEBUG_AT( DirectFB_CoreScreen, "%s( %p )\n", __FUNCTION__, obj );

     ret = dfb_screen_get_vsync_count( obj, &count );
     if (ret)
          return ret;

     *ret_count = count;

     return DFB_OK;
}

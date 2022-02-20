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

#ifndef __CORE__SCREENS_H__
#define __CORE__SCREENS_H__

#include <core/coretypes.h>
#include <fusion/call.h>
#include <fusion/lock.h>

/**********************************************************************************************************************/

typedef enum {
     CMSF_NONE      = 0x00000000, /* none of these */

     CMSF_DIMENSION = 0x00000001, /* dimension is set */

     CMSF_ALL       = 0x00000001, /* all of these */
} CoreMixerStateFlags;

typedef struct {
     CoreMixerStateFlags flags;

     DFBDimension        dimension;
} CoreMixerState;

typedef struct {
     /*
      * Return size of screen data (shared memory).
      */
     int       (*ScreenDataSize)   ( void );

     /*
      * Called once by the master to initialize screen data and reset hardware.
      * Return screen description.
      */
     DFBResult (*InitScreen)       ( CoreScreen                   *screen,
                                     void                         *driver_data,
                                     void                         *screen_data,
                                     DFBScreenDescription         *description );

     /*
      * Called once by the master to shutdown the screen.
      * Use this function to free any resources that were taken during init.
      */
     DFBResult (*ShutdownScreen)   ( CoreScreen                   *screen,
                                     void                         *driver_data,
                                     void                         *screen_data );

     /*
      * Called once by the master for each mixer.
      * Driver fills description and default config.
      */
     DFBResult (*InitMixer)        ( CoreScreen                   *screen,
                                     void                         *driver_data,
                                     void                         *screen_data,
                                     int                           mixer,
                                     DFBScreenMixerDescription    *description,
                                     DFBScreenMixerConfig         *config );

     /*
      * Called once by the master for each encoder.
      * Driver fills description and default config.
      */
     DFBResult (*InitEncoder)      ( CoreScreen                   *screen,
                                     void                         *driver_data,
                                     void                         *screen_data,
                                     int                           encoder,
                                     DFBScreenEncoderDescription  *description,
                                     DFBScreenEncoderConfig       *config );

     /*
      * Called once by the master for each output.
      * Driver fills description and default config.
      */
     DFBResult (*InitOutput)       ( CoreScreen                   *screen,
                                     void                         *driver_data,
                                     void                         *screen_data,
                                     int                           output,
                                     DFBScreenOutputDescription   *description,
                                     DFBScreenOutputConfig        *config );

     /*
      * Switch between "on", "standby", "suspend" and "off".
      */
     DFBResult (*SetPowerMode)     ( CoreScreen                   *screen,
                                     void                         *driver_data,
                                     void                         *screen_data,
                                     DFBScreenPowerMode            mode );

     /*
      * Wait for the vertical retrace.
      */
     DFBResult (*WaitVSync)        ( CoreScreen                   *screen,
                                     void                         *driver_data,
                                     void                         *screen_data );

     /*
      * Test if mixer configuration is supported.
      */
     DFBResult (*TestMixerConfig)  ( CoreScreen                   *screen,
                                     void                         *driver_data,
                                     void                         *screen_data,
                                     int                           mixer,
                                     const DFBScreenMixerConfig   *config,
                                     DFBScreenMixerConfigFlags    *ret_failed );

     /*
      * Set new mixer configuration.
      */
     DFBResult (*SetMixerConfig)   ( CoreScreen                   *screen,
                                     void                         *driver_data,
                                     void                         *screen_data,
                                     int                           mixer,
                                     const DFBScreenMixerConfig   *config );

     /*
      * Test if encoder configuration is supported.
      */
     DFBResult (*TestEncoderConfig)( CoreScreen                   *screen,
                                     void                         *driver_data,
                                     void                         *screen_data,
                                     int                           encoder,
                                     const DFBScreenEncoderConfig *config,
                                     DFBScreenEncoderConfigFlags  *ret_failed );

     /*
      * Set new encoder configuration.
      */
     DFBResult (*SetEncoderConfig) ( CoreScreen                   *screen,
                                     void                         *driver_data,
                                     void                         *screen_data,
                                     int                           encoder,
                                     const DFBScreenEncoderConfig *config );

     /*
      * Test if output configuration is supported.
      */
     DFBResult (*TestOutputConfig) ( CoreScreen                   *screen,
                                     void                         *driver_data,
                                     void                         *screen_data,
                                     int                           output,
                                     const DFBScreenOutputConfig  *config,
                                     DFBScreenOutputConfigFlags   *ret_failed );

     /*
      * Set new output configuration.
      */
     DFBResult (*SetOutputConfig)  ( CoreScreen                   *screen,
                                     void                         *driver_data,
                                     void                         *screen_data,
                                     int                           output,
                                     const DFBScreenOutputConfig  *config );

     /*
      * Return the screen size, e.g. as a basis for positioning a layer.
      */
     DFBResult (*GetScreenSize)    ( CoreScreen                   *screen,
                                     void                         *driver_data,
                                     void                         *screen_data,
                                     int                          *ret_width,
                                     int                          *ret_height );

     /*
      * Return the physical screen rotation
      */
     DFBResult (*GetScreenRotation) ( CoreScreen                  *screen,
                                      void                        *driver_data,
                                      int                         *rotation );

     /*
      * Return the mixer state.
      */
     DFBResult (*GetMixerState)    ( CoreScreen                   *screen,
                                     void                         *driver_data,
                                     void                         *screen_data,
                                     int                           mixer,
                                     CoreMixerState               *ret_state );

     /*
      * Return vertical retrace count.
      */
     DFBResult (*GetVSyncCount)    ( CoreScreen                   *screen,
                                     void                         *driver_data,
                                     void                         *screen_data,
                                     unsigned long                *ret_count );
} ScreenFuncs;

typedef struct {
     DFBScreenMixerDescription description;
     DFBScreenMixerConfig      configuration;
} CoreScreenMixer;

typedef struct {
     DFBScreenEncoderDescription description;
     DFBScreenEncoderConfig      configuration;
} CoreScreenEncoder;

typedef struct {
     DFBScreenOutputDescription  description;
     DFBScreenOutputConfig       configuration;
} CoreScreenOutput;

typedef struct {
     DFBScreenID           screen_id;

     DFBScreenDescription  description;

     CoreScreenMixer      *mixers;
     CoreScreenEncoder    *encoders;
     CoreScreenOutput     *outputs;

     void                 *screen_data; /* local data (impl) */

     FusionSkirmish        lock;

     FusionCall            call;        /* dispatch */
} CoreScreenShared;

struct __DFB_CoreScreen {
     CoreScreenShared  *shared;

     CoreDFB           *core;

     const ScreenFuncs *funcs;

     void              *driver_data;
     void              *screen_data; /* copy of shared->screen_data */
};

/**********************************************************************************************************************/

typedef DFBEnumerationResult (*CoreScreenCallback)( CoreScreen *screen, void *ctx );

/**********************************************************************************************************************/

/*
 * Add a screen to a graphics device by pointing to a table containing driver functions.
 * The supplied 'driver_data' will be passed to these functions.
 */
CoreScreen   *dfb_screens_register    ( void                *driver_data,
                                        const ScreenFuncs   *funcs );

/*
 * Replace functions of the primary screen implementation by passing an alternative driver function table.
 * All non-NULL functions in the new table replace the functions in the original function table.
 * The original function table is written to 'primary_funcs' before to allow drivers to use existing functionality
 * from the original implementation.
 */
CoreScreen   *dfb_screens_hook_primary( void                *driver_data,
                                        ScreenFuncs         *funcs,
                                        ScreenFuncs         *primary_funcs,
                                        void               **primary_driver_data );

/*
 * Enumerate all registered screens by invoking the callback for each screen.
 */
void          dfb_screens_enumerate   ( CoreScreenCallback   callback,
                                        void                *ctx );

/*
 * Return the number of screens.
 */
unsigned int  dfb_screens_num         ( void );

/*
 * Return the screen with the specified ID.
 */
CoreScreen   *dfb_screen_at           ( DFBScreenID          screen_id );

/*
 * Return the (translated) screen with the specified ID.
 */
CoreScreen   *dfb_screen_at_translated( DFBScreenID          screen_id );

/*
 * Return the ID of the specified screen.
 */
DFBScreenID   dfb_screen_id           ( const CoreScreen    *screen );

/*
 * Return the (translated) ID of the specified screen.
 */
DFBScreenID   dfb_screen_id_translated( const CoreScreen    *screen );

#endif

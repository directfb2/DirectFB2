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
#include <core/core.h>
#include <core/core_parts.h>
#include <core/layers.h>
#include <core/screen.h>
#include <core/screens.h>
#include <direct/memcpy.h>
#include <fusion/conf.h>
#include <fusion/shmalloc.h>

D_DEBUG_DOMAIN( Core_Screens, "Core/Screens", "DirectFB Core Screens" );

/**********************************************************************************************************************/

typedef struct {
     int               magic;

     int               num;
     CoreScreenShared *screens[MAX_SCREENS];
} DFBScreenCoreShared;

typedef struct {
     int                  magic;

     CoreDFB             *core;

     DFBScreenCoreShared *shared;
} DFBScreenCore;

DFB_CORE_PART( screen_core, ScreenCore );

/**********************************************************************************************************************/

static int         num_screens = 0;
static CoreScreen *screens[MAX_SCREENS];

static DFBResult
dfb_screen_core_initialize( CoreDFB             *core,
                            DFBScreenCore       *data,
                            DFBScreenCoreShared *shared )
{
     DFBResult            ret;
     FusionSHMPoolShared *pool;
     int                  i;

     D_DEBUG_AT( Core_Screens, "%s( %p, %p, %p )\n", __FUNCTION__, core, data, shared );

     D_ASSERT( data != NULL );
     D_ASSERT( shared != NULL );

     data->core   = core;
     data->shared = shared;

     pool = dfb_core_shmpool( core );

     /* Initialize all registered screens. */
     for (i = 0; i < num_screens; i++) {
          CoreScreen           *screen = screens[i];
          CoreScreenShared     *sshared;
          const ScreenFuncs    *funcs  = screen->funcs;
          DFBScreenDescription  desc   = { .caps = DSCCAPS_NONE };
          char                  buf[24];

          /* Allocate shared data. */
          sshared = SHCALLOC( pool, 1, sizeof(CoreScreenShared) );

          /* Assign ID (zero based index). */
          sshared->screen_id = i;

          snprintf( buf, sizeof(buf), "Screen %d", i );

          /* Initialize the lock. */
          if (fusion_skirmish_init2( &sshared->lock, buf, dfb_core_world( core ), fusion_config->secure_fusion )) {
               SHFREE( pool, sshared );
               return DFB_FUSION;
          }

          /* Allocate driver's screen data. */
          if (funcs->ScreenDataSize) {
               int size = funcs->ScreenDataSize();

               if (size > 0) {
                    sshared->screen_data = SHCALLOC( pool, 1, size );
                    if (!sshared->screen_data) {
                         fusion_skirmish_destroy( &sshared->lock );
                         SHFREE( pool, sshared );
                         return D_OOSHM();
                    }
               }
          }

          /* Initialize the screen and get the screen description. */
          ret = funcs->InitScreen( screen, screen->driver_data, sshared->screen_data, &desc );
          if (ret) {
               D_ERROR( "Core/Screens: Failed to initialize screen %u!\n", sshared->screen_id );

               fusion_skirmish_destroy( &sshared->lock );

               if (sshared->screen_data)
                    SHFREE( pool, sshared->screen_data );

               SHFREE( pool, sshared );

               return ret;
          }

          D_ASSUME( desc.mixers  > 0 || !(desc.caps & DSCCAPS_MIXERS) );
          D_ASSUME( desc.mixers == 0 ||  (desc.caps & DSCCAPS_MIXERS) );
          D_ASSUME( desc.encoders  > 0 || !(desc.caps & DSCCAPS_ENCODERS) );
          D_ASSUME( desc.encoders == 0 ||  (desc.caps & DSCCAPS_ENCODERS) );
          D_ASSUME( desc.outputs  > 0 || !(desc.caps & DSCCAPS_OUTPUTS) );
          D_ASSUME( desc.outputs == 0 ||  (desc.caps & DSCCAPS_OUTPUTS) );

          D_ASSERT( desc.mixers >= 0 );
          D_ASSERT( desc.mixers <= 32 );
          D_ASSERT( desc.encoders >= 0 );
          D_ASSERT( desc.encoders <= 32 );
          D_ASSERT( desc.outputs >= 0 );
          D_ASSERT( desc.outputs <= 32 );

          /* Store description in sshared memory. */
          sshared->description = desc;

          /* Initialize mixers. */
          if (sshared->description.mixers) {
               int mixer;

               D_ASSERT( funcs->InitMixer != NULL );
               D_ASSERT( funcs->SetMixerConfig != NULL );

               sshared->mixers = SHCALLOC( pool, sshared->description.mixers, sizeof(CoreScreenMixer) );

               for (mixer = 0; mixer < sshared->description.mixers; mixer++) {
                    funcs->InitMixer( screen, screen->driver_data, sshared->screen_data, mixer,
                                      &sshared->mixers[mixer].description,
                                      &sshared->mixers[mixer].configuration );

                    funcs->SetMixerConfig( screen, screen->driver_data, sshared->screen_data, mixer,
                                           &sshared->mixers[mixer].configuration );
               }
          }

          /* Initialize encoders. */
          if (sshared->description.encoders) {
               int encoder;

               D_ASSERT( funcs->InitEncoder != NULL );
               D_ASSERT( funcs->SetEncoderConfig != NULL );

               sshared->encoders = SHCALLOC( pool, sshared->description.encoders, sizeof(CoreScreenEncoder) );

               for (encoder = 0; encoder < sshared->description.encoders; encoder++) {
                    funcs->InitEncoder( screen, screen->driver_data, sshared->screen_data, encoder,
                                        &sshared->encoders[encoder].description,
                                        &sshared->encoders[encoder].configuration );

                    funcs->SetEncoderConfig( screen, screen->driver_data, sshared->screen_data, encoder,
                                             &sshared->encoders[encoder].configuration );
               }
          }

          /* Initialize outputs. */
          if (sshared->description.outputs) {
               int output;

               D_ASSERT( funcs->InitOutput != NULL );
               D_ASSERT( funcs->SetOutputConfig != NULL );

               sshared->outputs = SHCALLOC( pool, sshared->description.outputs, sizeof(CoreScreenOutput) );

               for (output = 0; output < sshared->description.outputs; output++) {
                    funcs->InitOutput( screen, screen->driver_data, sshared->screen_data, output,
                                       &sshared->outputs[output].description,
                                       &sshared->outputs[output].configuration );

                    funcs->SetOutputConfig( screen, screen->driver_data, sshared->screen_data, output,
                                            &sshared->outputs[output].configuration );
               }
          }

          /* Make a copy for faster access. */
          screen->screen_data = sshared->screen_data;

          /* Store pointer to shared data and core. */
          screen->shared = sshared;
          screen->core   = core;

          CoreScreen_Init_Dispatch( core, screen, &sshared->call );

          fusion_call_add_permissions( &sshared->call, 0, FUSION_CALL_PERMIT_EXECUTE );

          /* Add the screen to the shared list. */
          shared->screens[shared->num++] = sshared;
     }

     D_MAGIC_SET( data, DFBScreenCore );
     D_MAGIC_SET( shared, DFBScreenCoreShared );

     return DFB_OK;
}

static DFBResult
dfb_screen_core_join( CoreDFB             *core,
                      DFBScreenCore       *data,
                      DFBScreenCoreShared *shared )
{
     int i;

     D_DEBUG_AT( Core_Screens, "%s( %p, %p, %p )\n", __FUNCTION__, core, data, shared );

     D_ASSERT( data != NULL );
     D_MAGIC_ASSERT( shared, DFBScreenCoreShared );

     data->core   = core;
     data->shared = shared;

     if (num_screens != shared->num) {
          D_ERROR( "Core/Screens: Number of screens does not match!\n" );
          return DFB_BUG;
     }

     for (i = 0; i < num_screens; i++) {
          CoreScreen       *screen  = screens[i];
          CoreScreenShared *sshared = shared->screens[i];

          /* Make a copy for faster access. */
          screen->screen_data = sshared->screen_data;

          /* Store pointer to shared data and core. */
          screen->shared = sshared;
          screen->core   = core;
     }

     D_MAGIC_SET( data, DFBScreenCore );

     return DFB_OK;
}

static DFBResult
dfb_screen_core_shutdown( DFBScreenCore *data,
                          bool           emergency )
{
     DFBScreenCoreShared *shared;
     FusionSHMPoolShared *pool;
     int                  i;

     D_UNUSED_P( shared );

     D_DEBUG_AT( Core_Screens, "%s( %p, %semergency )\n", __FUNCTION__, data, emergency ? "" : "no " );

     D_MAGIC_ASSERT( data, DFBScreenCore );
     D_MAGIC_ASSERT( data->shared, DFBScreenCoreShared );

     shared = data->shared;

     pool = dfb_core_shmpool( data->core );

     /* Begin with the most recently added screen. */
     for (i = num_screens - 1; i >= 0; i--) {
          CoreScreen        *screen  = screens[i];
          CoreScreenShared  *sshared = screen->shared;
          const ScreenFuncs *funcs   = screen->funcs;

          /* Shut the screen down. */
          if (funcs->ShutdownScreen)
               if (funcs->ShutdownScreen( screen, screen->driver_data, sshared->screen_data ))
                    D_ERROR( "Core/Screens: Failed to shutdown screen %u!\n", sshared->screen_id );

          CoreScreen_Deinit_Dispatch( &sshared->call );

          /* Deinitialize the lock. */
          fusion_skirmish_destroy( &sshared->lock );

          /* Free the driver's screen data. */
          if (sshared->screen_data)
               SHFREE( pool, sshared->screen_data );

          /* Free mixer data. */
          if (sshared->mixers)
               SHFREE( pool, sshared->mixers );

          /* Free encoder data. */
          if (sshared->encoders)
               SHFREE( pool, sshared->encoders );

          /* Free output data. */
          if (sshared->outputs)
               SHFREE( pool, sshared->outputs );

          /* Free the shared screen data. */
          SHFREE( pool, sshared );

          /* Free the local screen data. */
          D_FREE( screen );
     }

     num_screens  = 0;

     D_MAGIC_CLEAR( data );
     D_MAGIC_CLEAR( shared );

     return DFB_OK;
}

static DFBResult
dfb_screen_core_leave( DFBScreenCore *data,
                       bool           emergency )
{
     int i;

     D_DEBUG_AT( Core_Screens, "%s( %p, %semergency )\n", __FUNCTION__, data, emergency ? "" : "no " );

     D_MAGIC_ASSERT( data, DFBScreenCore );
     D_MAGIC_ASSERT( data->shared, DFBScreenCoreShared );

     /* Deinitialize all local stuff only. */
     for (i = 0; i < num_screens; i++) {
          CoreScreen *screen = screens[i];

          /* Free local screen data. */
          D_FREE( screen );
     }

     num_screens  = 0;

     D_MAGIC_CLEAR( data );

     return DFB_OK;
}

static DFBResult
dfb_screen_core_suspend( DFBScreenCore *data )
{
     int i;

     D_DEBUG_AT( Core_Screens, "%s( %p )\n", __FUNCTION__, data );

     D_MAGIC_ASSERT( data, DFBScreenCore );
     D_MAGIC_ASSERT( data->shared, DFBScreenCoreShared );

     for (i = num_screens - 1; i >= 0; i--)
          dfb_screen_suspend( screens[i] );

     return DFB_OK;
}

static DFBResult
dfb_screen_core_resume( DFBScreenCore *data )
{
     int i;

     D_DEBUG_AT( Core_Screens, "%s( %p )\n", __FUNCTION__, data );

     D_MAGIC_ASSERT( data, DFBScreenCore );
     D_MAGIC_ASSERT( data->shared, DFBScreenCoreShared );

     for (i = 0; i < num_screens; i++)
          dfb_screen_resume( screens[i] );

     return DFB_OK;
}

/**********************************************************************************************************************/

CoreScreen *
dfb_screens_register( void              *driver_data,
                      const ScreenFuncs *funcs )
{
     CoreScreen *screen;

     D_ASSERT( funcs != NULL );

     if (num_screens == MAX_SCREENS) {
          D_ERROR( "Core/Screens: Maximum number of screens reached!\n" );
          return NULL;
     }

     /* Allocate local data. */
     screen = D_CALLOC( 1, sizeof(CoreScreen) );

     /* Assign local pointers. */
     screen->driver_data = driver_data;
     screen->funcs       = funcs;

     /* Add it to the local list. */
     screens[num_screens++] = screen;

     return screen;
}

typedef void (*AnyFunc)( void );

CoreScreen *
dfb_screens_hook_primary( void         *driver_data,
                          ScreenFuncs  *funcs,
                          ScreenFuncs  *primary_funcs,
                          void        **primary_driver_data )
{
     int         i;
     int         entries;
     CoreScreen *primary = screens[0];

     D_ASSERT( primary != NULL );
     D_ASSERT( funcs != NULL );

     /* Copy content of original function table. */
     if (primary_funcs)
          direct_memcpy( primary_funcs, primary->funcs, sizeof(ScreenFuncs) );

     /* Copy pointer to original driver data. */
     if (primary_driver_data)
          *primary_driver_data = primary->driver_data;

     /* Replace all entries in the old table that aren't NULL in the new one. */
     entries = sizeof(ScreenFuncs) / sizeof(void (*)( void ));
     for (i = 0; i < entries; i++) {
          AnyFunc *newfuncs = (AnyFunc*) funcs;
          AnyFunc *oldfuncs = (AnyFunc*) primary->funcs;

          if (newfuncs[i])
               oldfuncs[i] = newfuncs[i];
     }

     /* Replace device and driver data pointer. */
     primary->driver_data = driver_data;

     return primary;
}

void
dfb_screens_enumerate( CoreScreenCallback  callback,
                       void               *ctx )
{
     int i;

     D_ASSERT( callback != NULL );

     for (i = 0; i < num_screens; i++) {
          if (callback( screens[i], ctx ) == DFENUM_CANCEL)
               break;
     }
}

unsigned int
dfb_screens_num()
{
     return num_screens;
}

CoreScreen *
dfb_screen_at( DFBScreenID screen_id )
{
     D_ASSERT( screen_id >= 0 );
     D_ASSERT( screen_id < num_screens );

     return screens[screen_id];
}

CoreScreen *
dfb_screen_at_translated( DFBScreenID screen_id )
{
     CoreScreen *primary;

     D_ASSERT( screen_id >= 0 );
     D_ASSERT( screen_id < num_screens );

     if (dfb_config->primary_layer > 0) {
          primary = dfb_layer_at_translated( DLID_PRIMARY )->screen ;

          if (screen_id == DSCID_PRIMARY)
               return primary;

          if (screen_id == primary->shared->screen_id)
               return dfb_screen_at( DSCID_PRIMARY );
     }

     return dfb_screen_at( screen_id );
}

DFBScreenID
dfb_screen_id( const CoreScreen *screen )
{
     CoreScreenShared *shared;

     D_ASSERT( screen != NULL );
     D_ASSERT( screen->shared != NULL );

     shared = screen->shared;

     return shared->screen_id;
}

DFBScreenID
dfb_screen_id_translated( const CoreScreen *screen )
{
     CoreScreenShared *shared;
     CoreScreen       *primary;

     D_ASSERT( screen != NULL );
     D_ASSERT( screen->shared != NULL );

     shared = screen->shared;

     if (dfb_config->primary_layer > 0) {
          primary = dfb_layer_at_translated( DLID_PRIMARY )->screen;

          if (shared->screen_id == DSCID_PRIMARY)
               return primary->shared->screen_id;

          if (shared->screen_id == primary->shared->screen_id)
               return DSCID_PRIMARY;
     }

     return shared->screen_id;
}

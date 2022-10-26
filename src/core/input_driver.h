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

#ifndef __CORE__INPUT_DRIVER_H__
#define __CORE__INPUT_DRIVER_H__

#include <core/input.h>

/**********************************************************************************************************************/

static int                   driver_get_available    ( void );

static void                  driver_get_info         ( InputDriverInfo               *info );

static DFBResult             driver_open_device      ( CoreInputDevice               *device,
                                                       unsigned int                   number,
                                                       InputDeviceInfo               *info,
                                                       void                         **driver_data );

static DFBResult             driver_get_keymap_entry ( CoreInputDevice               *device,
                                                       void                          *driver_data,
                                                       DFBInputDeviceKeymapEntry     *entry );

static void                  driver_close_device     ( void                          *driver_data );

static DFBResult             driver_suspend          ( void );

static DFBResult             driver_resume           ( void );

static DFBResult             is_created              ( int                            index,
                                                       void                          *driver_data );

static InputDriverCapability get_capability          ( void );

static DFBResult             launch_hotplug          ( CoreDFB                       *core,
                                                       void                          *input_driver );

static DFBResult             stop_hotplug            ( void );

#ifdef DFB_INPUTDRIVER_HAS_AXIS_INFO
static DFBResult             driver_get_axis_info    ( CoreInputDevice               *device,
                                                       void                          *driver_data,
                                                       DFBInputDeviceAxisIdentifier   axis,
                                                       InputDeviceAxisInfo           *ret_info );
#endif

#ifdef DFB_INPUTDRIVER_HAS_SET_CONFIGURATION
static DFBResult             driver_set_configuration( CoreInputDevice               *device,
                                                       void                          *driver_data,
                                                       const DFBInputDeviceConfig    *config );
#endif

/*
 * Add hot-plug stub function implementations for all input drivers by default.
 */
#if !defined(DISABLE_INPUT_HOTPLUG_FUNCTION_STUB)
static DFBResult
driver_suspend()
{
     return DFB_UNSUPPORTED;
}

static DFBResult
driver_resume()
{
     return DFB_UNSUPPORTED;
}

static DFBResult
is_created( int   index,
            void *driver_data )
{
     return DFB_UNSUPPORTED;
}

static InputDriverCapability
get_capability()
{
     return IDC_NONE;
}

static DFBResult
launch_hotplug( CoreDFB *core,
                void    *input_driver )
{
     return DFB_UNSUPPORTED;
}

static DFBResult
stop_hotplug()
{
     return DFB_UNSUPPORTED;
}
#endif

static const InputDriverFuncs inputdriver_funcs = {
     .GetAvailable     = driver_get_available,
     .GetDriverInfo    = driver_get_info,
     .OpenDevice       = driver_open_device,
     .GetKeymapEntry   = driver_get_keymap_entry,
     .CloseDevice      = driver_close_device,
     .Suspend          = driver_suspend,
     .Resume           = driver_resume,
     .IsCreated        = is_created,
     .GetCapability    = get_capability,
     .LaunchHotplug    = launch_hotplug,
     .StopHotplug      = stop_hotplug,
#ifdef DFB_INPUTDRIVER_HAS_AXIS_INFO
     .GetAxisInfo      = driver_get_axis_info,
#endif
#ifdef DFB_INPUTDRIVER_HAS_SET_CONFIGURATION
     .SetConfiguration = driver_set_configuration,
#endif
};

#define DFB_INPUT_DRIVER(shortname)                         \
                                                            \
__attribute__((constructor))                                \
static void                                                 \
directfb_##shortname##_ctor()                               \
{                                                           \
     direct_modules_register( &dfb_input_drivers,           \
                              DFB_INPUT_DRIVER_ABI_VERSION, \
                              #shortname,                   \
                              &inputdriver_funcs );         \
}                                                           \
                                                            \
__attribute__((destructor))                                 \
static void                                                 \
directfb_##shortname##_dtor()                               \
{                                                           \
     direct_modules_unregister( &dfb_input_drivers,         \
                                #shortname );               \
}

#endif

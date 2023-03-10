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

#ifndef __CORE__INPUT_H__
#define __CORE__INPUT_H__

#include <core/coretypes.h>
#include <direct/modules.h>
#include <fusion/call.h>
#include <fusion/reactor.h>
#include <fusion/ref.h>

DECLARE_MODULE_DIRECTORY( dfb_input_drivers );

/**********************************************************************************************************************/

#define DFB_INPUT_DRIVER_ABI_VERSION           7

#define DFB_INPUT_DRIVER_INFO_NAME_LENGTH     60
#define DFB_INPUT_DRIVER_INFO_VENDOR_LENGTH   80
#define DFB_INPUT_DRIVER_INFO_URL_LENGTH     120
#define DFB_INPUT_DRIVER_INFO_LICENSE_LENGTH  40

typedef struct {
     int major; /* major version */
     int minor; /* minor version */
} InputDriverVersion;

typedef struct {
     InputDriverVersion version;

     char               name[DFB_INPUT_DRIVER_INFO_NAME_LENGTH];       /* Name of input driver */
     char               vendor[DFB_INPUT_DRIVER_INFO_VENDOR_LENGTH];   /* Vendor (or author) of the driver */
     char               url[DFB_INPUT_DRIVER_INFO_URL_LENGTH];         /* URL for driver updates */
     char               license[DFB_INPUT_DRIVER_INFO_LICENSE_LENGTH]; /* License, e.g. 'LGPL' or 'proprietary' */
} InputDriverInfo;

typedef enum {
     IDC_NONE    = 0x00000000, /* None. */

     IDC_HOTPLUG = 0x00000001, /* Input devices support hot-plug. */

     IDC_ALL     = 0x00000001  /* All flags supported. */
} InputDriverCapability;

typedef struct {
     unsigned int              prefered_id; /* Prefered predefined input device id. */

     DFBInputDeviceDescription desc;        /* Capabilities, type, etc. */
} InputDeviceInfo;

typedef enum {
     IDAIF_NONE    = 0x00000000, /* None of these. */

     IDAIF_ABS_MIN = 0x00000001, /* Minimum possible value. */
     IDAIF_ABS_MAX = 0x00000002, /* Maximum possible value. */

     IDAIF_ALL     = 0x00000003  /* All of these. */
} InputDeviceAxisInfoFlags;

typedef struct {
     InputDeviceAxisInfoFlags flags;
     int                      abs_min;
     int                      abs_max;
} InputDeviceAxisInfo;

typedef struct {
     int                   (*GetAvailable)    ( void );

     void                  (*GetDriverInfo)   ( InputDriverInfo               *driver_info );

     DFBResult             (*OpenDevice)      ( CoreInputDevice               *device,
                                                unsigned int                   number,
                                                InputDeviceInfo               *device_info,
                                                void                         **driver_data );

     DFBResult             (*GetKeymapEntry)  ( CoreInputDevice               *device,
                                                void                          *driver_data,
                                                DFBInputDeviceKeymapEntry     *entry );

     void                  (*CloseDevice)     ( void                          *driver_data );

     DFBResult             (*Suspend)         ( void );

     DFBResult             (*Resume)          ( void );

     DFBResult             (*IsCreated)       ( int                            index,
                                                void                          *data );

     InputDriverCapability (*GetCapability)   ( void );

     DFBResult             (*LaunchHotplug)   ( CoreDFB                       *core,
                                                void                          *input_driver );

     DFBResult             (*StopHotplug)     ( void );

     DFBResult             (*GetAxisInfo)     ( CoreInputDevice               *device,
                                                void                          *driver_data,
                                                DFBInputDeviceAxisIdentifier   axis,
                                                InputDeviceAxisInfo           *ret_info );

     DFBResult             (*SetConfiguration)( CoreInputDevice               *device,
                                                void                          *driver_data,
                                                const DFBInputDeviceConfig    *config );
} InputDriverFuncs;

typedef struct {
     int                        min_keycode;
     int                        max_keycode;
     int                        num_entries;
     DFBInputDeviceKeymapEntry *entries;
} InputDeviceKeymap;

typedef struct {
     DFBInputDeviceModifierMask modifiers_l;
     DFBInputDeviceModifierMask modifiers_r;
     DFBInputDeviceLockState    locks;
     DFBInputDeviceButtonMask   buttons;
} InputDeviceState;

typedef struct {
     int                          magic;

     DFBInputDeviceID             id;          /* unique device id */

     int                          num;

     InputDeviceInfo              device_info;

     InputDeviceKeymap            keymap;

     InputDeviceState             state;

     DFBInputDeviceKeyIdentifier  last_key;    /* last key pressed */
     DFBInputDeviceKeySymbol      last_symbol; /* last symbol pressed */
     bool                         first_press; /* first press of key */

     FusionReactor               *reactor;     /* event dispatcher */
     FusionSkirmish               lock;

     unsigned int                 axis_num;
     InputDeviceAxisInfo         *axis_info;
     FusionRef                    ref;         /* ref between shared device & local device */

     FusionCall                   call;
} CoreInputDeviceShared;

typedef struct {
     DirectLink              link;

     int                     magic;

     DirectModuleEntry      *module;

     const InputDriverFuncs *funcs;

     InputDriverInfo         info;

     int                     nr_devices;
} CoreInputDriver;

struct __DFB_CoreInputDevice {
     DirectLink             link;

     int                    magic;

     CoreInputDeviceShared *shared;

     CoreInputDriver       *driver;
     void                  *driver_data;

     CoreDFB               *core;
};

/**********************************************************************************************************************/

typedef DFBEnumerationResult (*InputDeviceCallback)( CoreInputDevice *device, void *ctx );

/**********************************************************************************************************************/

DFBResult                   dfb_input_add_global                ( ReactionFunc                     func,
                                                                  int                             *ret_index );

DFBResult                   dfb_input_set_global                ( ReactionFunc                     func,
                                                                  int                              index );

void                        dfb_input_enumerate_devices         ( InputDeviceCallback              callback,
                                                                  void                            *ctx,
                                                                  DFBInputDeviceCapabilities       caps );

DirectResult                dfb_input_attach                    ( CoreInputDevice                 *device,
                                                                  ReactionFunc                     func,
                                                                  void                            *ctx,
                                                                  Reaction                        *reaction );

DirectResult                dfb_input_detach                    ( CoreInputDevice                 *device,
                                                                  Reaction                        *reaction );

DirectResult                dfb_input_attach_global             ( CoreInputDevice                 *device,
                                                                  int                              index,
                                                                  void                            *ctx,
                                                                  GlobalReaction                  *reaction );

DirectResult                dfb_input_detach_global             ( CoreInputDevice                 *device,
                                                                  GlobalReaction                  *reaction );

void                        dfb_input_dispatch                  ( CoreInputDevice                 *device,
                                                                  DFBInputEvent                   *event );

DFBInputDeviceID            dfb_input_device_id                 ( const CoreInputDevice           *device );

CoreInputDevice            *dfb_input_device_at                 ( DFBInputDeviceID                 id );

DFBInputDeviceCapabilities  dfb_input_device_caps               ( const CoreInputDevice           *device );

void                        dfb_input_device_description        ( const CoreInputDevice           *device,
                                                                  DFBInputDeviceDescription       *desc );

DFBResult                   dfb_input_device_get_keymap_entry   ( CoreInputDevice                 *device,
                                                                  int                              keycode,
                                                                  DFBInputDeviceKeymapEntry       *entry );

DFBResult                   dfb_input_device_set_keymap_entry   ( CoreInputDevice                 *device,
                                                                  int                              keycode,
                                                                  const DFBInputDeviceKeymapEntry *entry );

DFBResult                   dfb_input_device_load_keymap        ( CoreInputDevice                 *device,
                                                                  char                            *filename );

DFBResult                   dfb_input_device_reload_keymap      ( CoreInputDevice                 *device );

DFBResult                   dfb_input_device_get_state          ( CoreInputDevice                 *device,
                                                                  InputDeviceState                *ret_state );

DFBResult                   dfb_input_device_set_configuration  ( CoreInputDevice                 *device,
                                                                  const DFBInputDeviceConfig      *config );

void                        eventbuffer_containers_attach_device( CoreInputDevice                 *device );

void                        eventbuffer_containers_detach_device( CoreInputDevice                 *device );

void                        stack_containers_attach_device      ( CoreInputDevice                 *device );

void                        stack_containers_detach_device      ( CoreInputDevice                 *device );

/*
 * Create the DFB shared core input device, add the input device into the local device list and shared dev array, and
 * broadcast the hot-plug in message to all slaves.
 */
DFBResult                   dfb_input_create_device             ( int                              device_index,
                                                                  CoreDFB                         *core_in,
                                                                  void                            *driver_in );

/*
 * Remove the DFB shared core input device handling of the system input device indicated by 'device_index' and broadcast
 * the hot-plug out message to all slaves.
 */
DFBResult                   dfb_input_remove_device             ( int                              device_index,
                                                                  void                            *driver_in );

#endif

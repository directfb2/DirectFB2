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

#include <core/input_driver.h>
#include <direct/thread.h>
#include <nuttx/input/buttons.h>
#include <nuttx/input/keyboard.h>
#include <nuttx/input/touchscreen.h>

D_DEBUG_DOMAIN( NuttX_Input, "Input/NuttX", "NuttX Input Driver" );

DFB_INPUT_DRIVER( nuttx_input )

/**********************************************************************************************************************/

typedef struct {
     CoreInputDevice *device;

     int              fd;

     unsigned int     id;

     int16_t          last_x;
     int16_t          last_y;

     DirectThread    *thread;
} NuttXData;

#define MAX_NUTTX_INPUT_DEVICES 3

/* Input devices are stored in the device_names array. */
static char *device_names[MAX_NUTTX_INPUT_DEVICES];

/* Number of entries in the device_names array. */
static int   num_devices = 0;

/**********************************************************************************************************************/

static void *
devinput_event_thread( DirectThread *thread,
                       void         *arg )
{
     NuttXData *data = arg;

     D_DEBUG_AT( NuttX_Input, "%s()\n", __FUNCTION__ );

     while (1) {
          DFBInputEvent evt = { .type = DIET_UNKNOWN };
          fd_set        set;
          int           status;
          ssize_t       len;

          FD_ZERO( &set );
          FD_SET( data->fd, &set );

          status = select( data->fd + 1, &set, NULL, NULL, NULL );

          if (status < 0 && errno != EINTR)
               break;

          if (status < 0)
               continue;

          if (data->id == DIDID_BUTTONS) {
               btn_buttonset_t buttonset;

               len = read( data->fd, &buttonset, sizeof(buttonset) );
               if (len < 1)
                    continue;

               evt.type   = buttonset ? DIET_BUTTONRELEASE : DIET_BUTTONPRESS;
               evt.flags  = DIEF_NONE;
               evt.button = DIBI_FIRST;

               dfb_input_dispatch( data->device, &evt );
          }
          else if (data->id == DIDID_KEYBOARD) {
               struct keyboard_event_s keyboard_event;

               len = read( data->fd, &keyboard_event, sizeof(keyboard_event) );
               if (len < 1)
                    continue;

               evt.type       = keyboard_event.type ? DIET_KEYRELEASE : DIET_KEYPRESS;
               evt.flags      = DIEF_KEYSYMBOL;
               evt.key_symbol = keyboard_event.code;

               dfb_input_dispatch( data->device, &evt );
          }
          else if (data->id == DIDID_MOUSE) {
               struct touch_sample_s touch_sample;

               len = read( data->fd, &touch_sample, sizeof(touch_sample) );
               if (len < 1)
                    continue;

               if (touch_sample.point[0].flags & TOUCH_DOWN) {
                    evt.type   = DIET_BUTTONPRESS;
                    evt.flags  = DIEF_NONE;
                    evt.button = DIBI_LEFT;

                    dfb_input_dispatch( data->device, &evt );
               }

               if (touch_sample.point[0].flags & TOUCH_DOWN || touch_sample.point[0].flags & TOUCH_MOVE) {
                    if (touch_sample.point[0].x != data->last_x) {
                         evt.type    = DIET_AXISMOTION;
                         evt.flags   = DIEF_AXISABS | DIEF_BUTTONS;
                         evt.axis    = DIAI_X;
                         evt.axisabs = touch_sample.point[0].x;
                         evt.buttons = DIBM_LEFT;

                         dfb_input_dispatch( data->device, &evt );
                    }

                    if (touch_sample.point[0].y != data->last_y) {
                         evt.type    = DIET_AXISMOTION;
                         evt.flags   = DIEF_AXISABS | DIEF_BUTTONS;
                         evt.axis    = DIAI_Y;
                         evt.axisabs = touch_sample.point[0].y;
                         evt.buttons = DIBM_LEFT;

                         dfb_input_dispatch( data->device, &evt );
                    }
               }

               if (touch_sample.point[0].flags & TOUCH_UP) {
                    evt.type   = DIET_BUTTONRELEASE;
                    evt.flags  = DIEF_NONE;
                    evt.button = DIBI_LEFT;

                    dfb_input_dispatch( data->device, &evt );
               }

               data->last_x = touch_sample.point[0].x;
               data->last_y = touch_sample.point[0].y;
          }
     }

     D_DEBUG_AT( NuttX_Input, "DevInput Event thread terminated\n" );

     return NULL;
}

static bool
check_device( const char *device )
{
     int fd;

     D_DEBUG_AT( NuttX_Input, "%s( '%s' )\n", __FUNCTION__, device );

     /* Check if we are able to open the device. */
     fd = open( device, O_RDONLY );
     if (fd < 0) {
          D_DEBUG_AT( NuttX_Input, "  -> open failed!\n" );
          return false;
     }

     close( fd );

     return true;
}

/**********************************************************************************************************************/

static int
driver_get_available()
{
     char buf[32];
     int  i;

     if (num_devices) {
          for (i = 0; i < num_devices; i++) {
               D_FREE( device_names[i] );
               device_names[i] = NULL;
          }

          num_devices = 0;

          return 0;
     }

     /* Buttons device */
     snprintf( buf, sizeof(buf), "/dev/buttons" );

     if (check_device( buf )) {
          D_ASSERT( device_names[num_devices] == NULL );
          device_names[num_devices++] = D_STRDUP( buf );
     }

     /* Keyboard device */
     snprintf( buf, sizeof(buf), "/dev/kbd" );

     if (check_device( buf )) {
          D_ASSERT( device_names[num_devices] == NULL );
          device_names[num_devices++] = D_STRDUP( buf );
     }

     /* Touchscreen device */
     snprintf( buf, sizeof(buf), "/dev/input0" );

     if (check_device( buf )) {
          D_ASSERT( device_names[num_devices] == NULL );
          device_names[num_devices++] = D_STRDUP( buf );
     }

     return num_devices;
}

static void
driver_get_info( InputDriverInfo *driver_info )
{
     driver_info->version.major = 0;
     driver_info->version.minor = 1;

     snprintf( driver_info->name,   DFB_INPUT_DRIVER_INFO_NAME_LENGTH,   "NuttX Input" );
     snprintf( driver_info->vendor, DFB_INPUT_DRIVER_INFO_VENDOR_LENGTH, "DirectFB" );
}

static DFBResult
driver_open_device( CoreInputDevice  *device,
                    unsigned int      number,
                    InputDeviceInfo  *device_info,
                    void            **driver_data )
{
     NuttXData *data;
     int        fd;

     D_DEBUG_AT( NuttX_Input, "%s()\n", __FUNCTION__ );

     /* Open device. */
     fd = open( device_names[number], O_RDONLY );
     if (fd < 0) {
          D_PERROR( "Input/NuttX: Could not open device!\n" );
          return DFB_INIT;
     }

     /* Fill device information. */
     if (!strcmp( device_names[number], "/dev/buttons" )) {
          device_info->prefered_id = DIDID_BUTTONS;
          device_info->desc.type   = DIDTF_BUTTONS;
          device_info->desc.caps   = DICAPS_BUTTONS;
          snprintf( device_info->desc.name,   DFB_INPUT_DEVICE_DESC_NAME_LENGTH,   "Buttons" );
          snprintf( device_info->desc.vendor, DFB_INPUT_DEVICE_DESC_VENDOR_LENGTH, "NuttX" );
     }
     else if (!strcmp( device_names[number], "/dev/kbd" )) {
          device_info->prefered_id = DIDID_KEYBOARD;
          device_info->desc.type   = DIDTF_KEYBOARD;
          device_info->desc.caps   = DICAPS_KEYS;
          snprintf( device_info->desc.name,   DFB_INPUT_DEVICE_DESC_NAME_LENGTH,   "Keyboard" );
          snprintf( device_info->desc.vendor, DFB_INPUT_DEVICE_DESC_VENDOR_LENGTH, "NuttX" );
     }
     else if (!strcmp( device_names[number], "/dev/input0" )) {
          device_info->prefered_id     = DIDID_MOUSE;
          device_info->desc.type       = DIDTF_MOUSE;
          device_info->desc.caps       = DICAPS_AXES | DICAPS_BUTTONS;
          device_info->desc.max_axis   = DIAI_Y;
          device_info->desc.max_button = DIBI_LEFT;
          snprintf( device_info->desc.name,   DFB_INPUT_DEVICE_DESC_NAME_LENGTH,   "Touchscreen" );
          snprintf( device_info->desc.vendor, DFB_INPUT_DEVICE_DESC_VENDOR_LENGTH, "NuttX" );
     }

     /* Allocate and fill private data. */
     data = D_CALLOC( 1, sizeof(NuttXData) );
     if (!data) {
          close( fd );
          return D_OOM();
     }

     data->device = device;
     data->fd     = fd;
     data->id     = device_info->prefered_id;

     /* Start devinput event thread. */
     data->thread = direct_thread_create( DTT_INPUT, devinput_event_thread, data, "DevInput Event" );

     *driver_data = data;

     return DFB_OK;
}

static DFBResult
driver_get_keymap_entry( CoreInputDevice           *device,
                         void                      *driver_data,
                         DFBInputDeviceKeymapEntry *entry )
{
     return DFB_UNSUPPORTED;
}

static void
driver_close_device( void *driver_data )
{
     NuttXData *data = driver_data;

     D_DEBUG_AT( NuttX_Input, "%s()\n", __FUNCTION__ );

     /* Terminate the devinput event thread. */
     direct_thread_cancel( data->thread );

     direct_thread_join( data->thread );
     direct_thread_destroy( data->thread );

     close( data->fd );

     D_FREE( data );
}

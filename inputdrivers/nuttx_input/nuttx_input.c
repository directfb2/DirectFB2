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
#include <nuttx/input/touchscreen.h>

D_DEBUG_DOMAIN( NuttX_Input, "Input/NuttX", "NuttX Input Driver" );

DFB_INPUT_DRIVER( nuttx_input )

/**********************************************************************************************************************/

typedef struct {
     CoreInputDevice *device;

     int              fd;

     int16_t          last_x;
     int16_t          last_y;

     DirectThread    *thread;
} NuttXData;

/**********************************************************************************************************************/

static void *
devinput_event_thread( DirectThread *thread,
                       void         *arg )
{
     NuttXData *data = arg;

     D_DEBUG_AT( NuttX_Input, "%s()\n", __FUNCTION__ );

     while (1) {
          DFBInputEvent evt = { .type = DIET_UNKNOWN };
          fd_set                set;
          int                   status;
          ssize_t               len;
          struct touch_sample_s touch_sample;

          FD_ZERO( &set );
          FD_SET( data->fd, &set );

          status = select( data->fd + 1, &set, NULL, NULL, NULL );

          if (status < 0 && errno != EINTR)
               break;

          if (status < 0)
               continue;

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

     D_DEBUG_AT( NuttX_Input, "DevInput Event thread terminated\n" );

     return NULL;
}

/**********************************************************************************************************************/

static int
driver_get_available()
{
     int fd;

     /* Check if we are able to open the device. */
     fd = open( "/dev/input0", O_RDONLY );
     if (fd < 0)
          return 0;

     close( fd );

     return 1;
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
     fd = open( "/dev/input0", O_RDONLY );
     if (fd < 0) {
          D_PERROR( "Input/NuttX: Could not open device!\n" );
          return DFB_INIT;
     }

     /* Fill device information. */
     device_info->prefered_id     = DIDID_MOUSE;
     device_info->desc.type       = DIDTF_MOUSE;
     device_info->desc.caps       = DICAPS_AXES | DICAPS_BUTTONS;
     device_info->desc.max_axis   = DIAI_Y;
     device_info->desc.max_button = DIBI_LEFT;
     snprintf( device_info->desc.name,   DFB_INPUT_DEVICE_DESC_NAME_LENGTH,   "Touchscreen" );
     snprintf( device_info->desc.vendor, DFB_INPUT_DEVICE_DESC_VENDOR_LENGTH, "NuttX" );

     /* Allocate and fill private data. */
     data = D_CALLOC( 1, sizeof(NuttXData) );
     if (!data) {
          close( fd );
          return D_OOM();
     }

     data->device = device;
     data->fd     = fd;

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

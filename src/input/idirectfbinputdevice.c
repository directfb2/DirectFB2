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

#include <core/CoreInputDevice.h>
#include <input/idirectfbeventbuffer.h>
#include <input/idirectfbinputdevice.h>

D_DEBUG_DOMAIN( InputDevice, "IDirectFBInputDevice", "IDirectFBInputDevice Interface" );

/**********************************************************************************************************************/

/*
 * private data struct of IDirectFBInputDevice
 */
typedef struct {
     int                         ref;                            /* reference counter */

     CoreInputDevice            *device;                         /* the input device object */

     int                         axis[DIAI_LAST+1];              /* position of all axes */
     DFBInputDeviceKeyState      keystates[DIKI_NUMBER_OF_KEYS]; /* state of all keys */

     DFBInputDeviceDescription   desc;                           /* device description */

     Reaction                    reaction;
} IDirectFBInputDevice_data;

/**********************************************************************************************************************/

static void
IDirectFBInputDevice_Destruct( IDirectFBInputDevice *thiz )
{
     IDirectFBInputDevice_data *data = thiz->priv;

     D_DEBUG_AT( InputDevice, "%s( %p )\n", __FUNCTION__, thiz );

     dfb_input_detach( data->device, &data->reaction );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DirectResult
IDirectFBInputDevice_AddRef( IDirectFBInputDevice *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBInputDevice )

     D_DEBUG_AT( InputDevice, "%s( %p )\n", __FUNCTION__, thiz );

     data->ref++;

     return DFB_OK;
}

static DirectResult
IDirectFBInputDevice_Release( IDirectFBInputDevice *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBInputDevice )

     D_DEBUG_AT( InputDevice, "%s( %p )\n", __FUNCTION__, thiz );

     if (--data->ref == 0)
          IDirectFBInputDevice_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBInputDevice_GetID( IDirectFBInputDevice *thiz,
                            DFBInputDeviceID     *ret_device_id )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBInputDevice )

     D_DEBUG_AT( InputDevice, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_device_id)
          return DFB_INVARG;

     *ret_device_id = dfb_input_device_id( data->device );

     return DFB_OK;
}

static DFBResult
IDirectFBInputDevice_GetDescription( IDirectFBInputDevice      *thiz,
                                     DFBInputDeviceDescription *ret_desc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBInputDevice )

     D_DEBUG_AT( InputDevice, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_desc)
          return DFB_INVARG;

     *ret_desc = data->desc;

     return DFB_OK;
}

static DFBResult
IDirectFBInputDevice_GetKeymapEntry( IDirectFBInputDevice      *thiz,
                                     int                        keycode,
                                     DFBInputDeviceKeymapEntry *ret_entry )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBInputDevice )

     D_DEBUG_AT( InputDevice, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_entry)
          return DFB_INVARG;

     if (data->desc.min_keycode < 0 || data->desc.max_keycode < 0)
          return DFB_UNSUPPORTED;

     if (keycode < data->desc.min_keycode || keycode > data->desc.max_keycode)
          return DFB_INVARG;

     return dfb_input_device_get_keymap_entry( data->device, keycode, ret_entry );
}

static DFBResult
IDirectFBInputDevice_SetKeymapEntry( IDirectFBInputDevice      *thiz,
                                     int                        keycode,
                                     DFBInputDeviceKeymapEntry *entry )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBInputDevice )

     D_DEBUG_AT( InputDevice, "%s( %p )\n", __FUNCTION__, thiz );

     if (!entry)
          return DFB_INVARG;

     if (data->desc.min_keycode < 0 || data->desc.max_keycode < 0)
          return DFB_UNSUPPORTED;

     if (keycode < data->desc.min_keycode || keycode > data->desc.max_keycode)
          return DFB_INVARG;

     return CoreInputDevice_SetKeymapEntry( data->device, keycode, entry );
}

static DFBResult
IDirectFBInputDevice_LoadKeymap ( IDirectFBInputDevice *thiz,
                                  char                 *filename )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBInputDevice )

     D_DEBUG_AT( InputDevice, "%s( %p )\n", __FUNCTION__, thiz );

     if (!filename)
          return DFB_INVARG;

     return dfb_input_device_load_keymap( data->device, filename );
}

static DFBResult
IDirectFBInputDevice_CreateEventBuffer( IDirectFBInputDevice  *thiz,
                                        IDirectFBEventBuffer **ret_interface )
{
     IDirectFBEventBuffer *iface;

     DIRECT_INTERFACE_GET_DATA( IDirectFBInputDevice )

     D_DEBUG_AT( InputDevice, "%s( %p )\n", __FUNCTION__, thiz );

     DIRECT_ALLOCATE_INTERFACE( iface, IDirectFBEventBuffer );

     IDirectFBEventBuffer_Construct( iface, NULL, NULL );

     IDirectFBEventBuffer_AttachInputDevice( iface, data->device );

     *ret_interface = iface;

     return DFB_OK;
}

static DFBResult
IDirectFBInputDevice_AttachEventBuffer( IDirectFBInputDevice *thiz,
                                        IDirectFBEventBuffer *buffer )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBInputDevice )

     D_DEBUG_AT( InputDevice, "%s( %p )\n", __FUNCTION__, thiz );

     return IDirectFBEventBuffer_AttachInputDevice( buffer, data->device );
}

static DFBResult
IDirectFBInputDevice_DetachEventBuffer( IDirectFBInputDevice *thiz,
                                        IDirectFBEventBuffer *buffer )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBInputDevice )

     D_DEBUG_AT( InputDevice, "%s( %p )\n", __FUNCTION__, thiz );

     return IDirectFBEventBuffer_DetachInputDevice( buffer, data->device );
}

static DFBResult
IDirectFBInputDevice_GetKeyState( IDirectFBInputDevice        *thiz,
                                  DFBInputDeviceKeyIdentifier  key_id,
                                  DFBInputDeviceKeyState      *ret_state )
{
     unsigned int index = key_id - DFB_KEY( IDENTIFIER, 0 );

     DIRECT_INTERFACE_GET_DATA( IDirectFBInputDevice )

     D_DEBUG_AT( InputDevice, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_state || index >= DIKI_NUMBER_OF_KEYS)
          return DFB_INVARG;

     *ret_state = data->keystates[index];

     return DFB_OK;
}

static DFBResult
IDirectFBInputDevice_GetModifiers( IDirectFBInputDevice       *thiz,
                                   DFBInputDeviceModifierMask *ret_modifiers )
{
     DFBResult        ret;
     InputDeviceState state;

     DIRECT_INTERFACE_GET_DATA( IDirectFBInputDevice )

     D_DEBUG_AT( InputDevice, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_modifiers)
          return DFB_INVARG;

     ret = dfb_input_device_get_state( data->device, &state );
     if (ret)
          return ret;

     *ret_modifiers = state.modifiers_l | state.modifiers_r;

     return DFB_OK;
}

static DFBResult
IDirectFBInputDevice_GetLockState( IDirectFBInputDevice    *thiz,
                                   DFBInputDeviceLockState *ret_locks )
{
     DFBResult        ret;
     InputDeviceState state;

     DIRECT_INTERFACE_GET_DATA( IDirectFBInputDevice )

     D_DEBUG_AT( InputDevice, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_locks)
          return DFB_INVARG;

     ret = dfb_input_device_get_state( data->device, &state );
     if (ret)
          return ret;

     *ret_locks = state.locks;

     return DFB_OK;
}

static DFBResult
IDirectFBInputDevice_GetButtons( IDirectFBInputDevice     *thiz,
                                 DFBInputDeviceButtonMask *ret_buttons )
{
     DFBResult        ret;
     InputDeviceState state;

     DIRECT_INTERFACE_GET_DATA( IDirectFBInputDevice )

     D_DEBUG_AT( InputDevice, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_buttons)
          return DFB_INVARG;

     ret = dfb_input_device_get_state( data->device, &state );
     if (ret)
          return ret;

     *ret_buttons = state.buttons;

     return DFB_OK;
}

static DFBResult
IDirectFBInputDevice_GetButtonState( IDirectFBInputDevice           *thiz,
                                     DFBInputDeviceButtonIdentifier  button,
                                     DFBInputDeviceButtonState      *ret_state )
{
     DFBResult        ret;
     InputDeviceState state;

     DIRECT_INTERFACE_GET_DATA( IDirectFBInputDevice )

     D_DEBUG_AT( InputDevice, "%s( %p )\n", __FUNCTION__, thiz );

     ret = dfb_input_device_get_state( data->device, &state );
     if (ret)
          return ret;

     if (!ret_state || button < DIBI_FIRST || button > DIBI_LAST)
          return DFB_INVARG;

     *ret_state = (state.buttons & (1 << button)) ? DIBS_DOWN : DIBS_UP;

     return DFB_OK;
}

static DFBResult
IDirectFBInputDevice_GetAxis( IDirectFBInputDevice         *thiz,
                              DFBInputDeviceAxisIdentifier  axis,
                              int                          *ret_pos )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBInputDevice )

     D_DEBUG_AT( InputDevice, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_pos || axis < DIAI_FIRST || axis > DIAI_LAST)
          return DFB_INVARG;

     *ret_pos = data->axis[axis];

     return DFB_OK;
}

static DFBResult
IDirectFBInputDevice_GetXY( IDirectFBInputDevice *thiz,
                            int                  *ret_x,
                            int                  *ret_y )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBInputDevice )

     D_DEBUG_AT( InputDevice, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_x && !ret_y)
          return DFB_INVARG;

     if (ret_x)
          *ret_x = data->axis[DIAI_X];

     if (ret_y)
          *ret_y = data->axis[DIAI_Y];

     return DFB_OK;
}

static DFBResult
IDirectFBInputDevice_SetConfiguration( IDirectFBInputDevice       *thiz,
                                       const DFBInputDeviceConfig *config )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBInputDevice )

     return CoreInputDevice_SetConfiguration( data->device, config );
}

static ReactionResult
IDirectFBInputDevice_React( const void *msg_data,
                            void       *ctx )
{
     const DFBInputEvent       *evt  = msg_data;
     IDirectFBInputDevice_data *data = ctx;
     unsigned int               index;

     D_DEBUG_AT( InputDevice, "%s( %p, %p ) <- type %06x\n", __FUNCTION__, evt, data, evt->type );

     switch (evt->type) {
          case DIET_KEYPRESS:
               index = evt->key_id - DFB_KEY( IDENTIFIER, 0 );
               if (index < DIKI_NUMBER_OF_KEYS)
                    data->keystates[index] = DIKS_DOWN;
             break;

          case DIET_KEYRELEASE:
               index = evt->key_id - DFB_KEY( IDENTIFIER, 0 );
               if (index < DIKI_NUMBER_OF_KEYS)
                    data->keystates[index] = DIKS_UP;
               break;

          case DIET_AXISMOTION:
               if (evt->flags & DIEF_AXISREL)
                    data->axis[evt->axis] += evt->axisrel;
               if (evt->flags & DIEF_AXISABS)
                    data->axis[evt->axis] = evt->axisabs;
               break;

          default:
               D_DEBUG_AT( InputDevice, "  -> unknown event type, skipping!\n" );
     }

     return RS_OK;
}

DFBResult
IDirectFBInputDevice_Construct( IDirectFBInputDevice *thiz,
                                CoreInputDevice      *device )
{
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBInputDevice )

     D_DEBUG_AT( InputDevice, "%s( %p )\n", __FUNCTION__, thiz );

     data->ref    = 1;
     data->device = device;

     dfb_input_device_description( device, &data->desc );

     dfb_input_attach( data->device, IDirectFBInputDevice_React, data, &data->reaction );

     thiz->AddRef            = IDirectFBInputDevice_AddRef;
     thiz->Release           = IDirectFBInputDevice_Release;
     thiz->GetID             = IDirectFBInputDevice_GetID;
     thiz->GetDescription    = IDirectFBInputDevice_GetDescription;
     thiz->GetKeymapEntry    = IDirectFBInputDevice_GetKeymapEntry;
     thiz->SetKeymapEntry    = IDirectFBInputDevice_SetKeymapEntry;
     thiz->LoadKeymap        = IDirectFBInputDevice_LoadKeymap;
     thiz->CreateEventBuffer = IDirectFBInputDevice_CreateEventBuffer;
     thiz->AttachEventBuffer = IDirectFBInputDevice_AttachEventBuffer;
     thiz->DetachEventBuffer = IDirectFBInputDevice_DetachEventBuffer;
     thiz->GetKeyState       = IDirectFBInputDevice_GetKeyState;
     thiz->GetModifiers      = IDirectFBInputDevice_GetModifiers;
     thiz->GetLockState      = IDirectFBInputDevice_GetLockState;
     thiz->GetButtons        = IDirectFBInputDevice_GetButtons;
     thiz->GetButtonState    = IDirectFBInputDevice_GetButtonState;
     thiz->GetAxis           = IDirectFBInputDevice_GetAxis;
     thiz->GetXY             = IDirectFBInputDevice_GetXY;
     thiz->SetConfiguration  = IDirectFBInputDevice_SetConfiguration;

     return DFB_OK;
}

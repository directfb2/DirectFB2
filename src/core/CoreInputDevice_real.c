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
#include <core/input.h>

D_DEBUG_DOMAIN( DirectFB_CoreInputDevice, "DirectFB/CoreInputDevice", "DirectFB CoreInputDevice" );

/**********************************************************************************************************************/

DFBResult
IInputDevice_Real__SetKeymapEntry( CoreInputDevice                 *obj,
                                   s32                              key_code,
                                   const DFBInputDeviceKeymapEntry *entry )
{
     D_DEBUG_AT( DirectFB_CoreInputDevice, "%s( %p )\n", __FUNCTION__, obj );

     return dfb_input_device_set_keymap_entry( obj, key_code, entry );
}

DFBResult
IInputDevice_Real__SetConfiguration( CoreInputDevice            *obj,
                                     const DFBInputDeviceConfig *config )
{
     D_DEBUG_AT( DirectFB_CoreInputDevice, "%s( %p )\n", __FUNCTION__, obj );

     return dfb_input_device_set_configuration( obj, config );
}

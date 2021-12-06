#  This file is part of DirectFB.
#
#  This library is free software; you can redistribute it and/or
#  modify it under the terms of the GNU Lesser General Public
#  License as published by the Free Software Foundation; either
#  version 2.1 of the License, or (at your option) any later version.
#
#  This library is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  Lesser General Public License for more details.
#
#  You should have received a copy of the GNU Lesser General Public
#  License along with this library; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA

interface {
        name    IInputDevice
        version 1.0
        object  CoreInputDevice

        method {
                name    SetKeymapEntry

                arg {
                        name        key_code
                        direction   input
                        type        int
                        typename    s32
                }

                arg {
                        name        entry
                        direction   input
                        type        struct
                        typename    DFBInputDeviceKeymapEntry
                }
        }

        method {
                name    SetConfiguration

                arg {
                        name        config
                        direction   input
                        type        struct
                        typename    DFBInputDeviceConfig
                }
        }
}

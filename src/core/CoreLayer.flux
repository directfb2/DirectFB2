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
        name    ILayer
        version 1.0
        object  CoreLayer

        method {
                name    CreateContext

                arg {
                        name        context
                        direction   output
                        type        object
                        typename    CoreLayerContext
                }
        }

        method {
                name    GetPrimaryContext

                arg {
                        name        activate
                        direction   input
                        type        enum
                        typename    DFBBoolean
                }

                arg {
                        name        context
                        direction   output
                        type        object
                        typename    CoreLayerContext
                }
        }

        method {
                name    ActivateContext

                arg {
                        name        context
                        direction   input
                        type        object
                        typename    CoreLayerContext
                }
        }

        method {
                name    GetCurrentOutputField

                arg {
                        name        field
                        direction   output
                        type        int
                        typename    s32
                }
        }

        method {
                name    SetLevel

                arg {
                        name        level
                        direction   input
                        type        int
                        typename    s32
                }
        }

        method {
                name    WaitVSync
        }
}

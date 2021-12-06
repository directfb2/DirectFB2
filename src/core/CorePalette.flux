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
        name    IPalette
        version 1.0
        object  CorePalette

        method {
                name    SetEntries

                arg {
                        name        colors
                        direction   input
                        type        struct
                        typename    DFBColor
                        count       num
                }

                arg {
                        name        num
                        direction   input
                        type        int
                        typename    u32
                }

                arg {
                        name        offset
                        direction   input
                        type        int
                        typename    u32
                }
        }

        method {
                name    SetEntriesYUV

                arg {
                        name        colors
                        direction   input
                        type        struct
                        typename    DFBColorYUV
                        count       num
                }

                arg {
                        name        num
                        direction   input
                        type        int
                        typename    u32
                }

                arg {
                        name        offset
                        direction   input
                        type        int
                        typename    u32
                }
        }
}

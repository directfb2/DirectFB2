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
        name    ILayerRegion
        version 1.0
        object  CoreLayerRegion

        method {
                name    GetSurface

                arg {
                        name        surface
                        direction   output
                        type        object
                        typename    CoreSurface
                }
        }

        method {
                name    FlipUpdate2

                arg {
                        name        left_update
                        direction   input
                        type        struct
                        typename    DFBRegion
                        optional    yes
                }

                arg {
                        name        right_update
                        direction   input
                        type        struct
                        typename    DFBRegion
                        optional    yes
                }

                arg {
                        name        flags
                        direction   input
                        type        enum
                        typename    DFBSurfaceFlipFlags
                }

                arg {
                        name        flip_count
                        direction   input
                        type        int
                        typename    u32
                }

                arg {
                        name        pts
                        direction   input
                        type        int
                        typename    s64
                }
        }

        method {
                name    SetSurface

                arg {
                        name        surface
                        direction   input
                        type        object
                        typename    CoreSurface
                }
        }
}

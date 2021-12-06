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
        name    IWindowStack
        version 1.0
        object  CoreWindowStack

        method {
                name    RepaintAll
        }

        method {
                name    BackgroundSetMode

                arg {
                        name        mode
                        direction   input
                        type        enum
                        typename    DFBDisplayLayerBackgroundMode
                }
        }

        method {
                name    BackgroundSetImage

                arg {
                        name        image
                        direction   input
                        type        object
                        typename    CoreSurface
                }
        }

        method {
                name    BackgroundSetColor

                arg {
                        name        color
                        direction   input
                        type        struct
                        typename    DFBColor
                }
        }

        method {
                name    BackgroundSetColorIndex

                arg {
                        name        index
                        direction   input
                        type        int
                        typename    s32
                }
        }

        method {
                name    CursorEnable

                arg {
                        name        enable
                        direction   input
                        type        enum
                        typename    bool
                }
        }

        method {
                name    CursorGetPosition

                arg {
                        name        position
                        direction   output
                        type        struct
                        typename    DFBPoint
                }
        }

        method {
                name    CursorWarp

                arg {
                        name        position
                        direction   input
                        type        struct
                        typename    DFBPoint
                }
        }

        method {
                name    CursorSetAcceleration

                arg {
                        name        numerator
                        direction   input
                        type        int
                        typename    u32
                }

                arg {
                        name        denominator
                        direction   input
                        type        int
                        typename    u32
                }

                arg {
                        name        threshold
                        direction   input
                        type        int
                        typename    u32
                }
        }

        method {
                name    CursorSetShape

                arg {
                        name        shape
                        direction   input
                        type        object
                        typename    CoreSurface
                }

                arg {
                        name        hotspot
                        direction   input
                        type        struct
                        typename    DFBPoint
                }
        }

        method {
                name    CursorSetOpacity

                arg {
                        name        opacity
                        direction   input
                        type        int
                        typename    u8
                }
        }
}

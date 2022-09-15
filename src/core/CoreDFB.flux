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
        name    ICore
        version 1.0
        object  CoreDFB

        method {
                name    Initialize
        }

        method {
                name    Register

                arg {
                        name        slave_call
                        direction   input
                        type        int
                        typename    u32
                }
        }

        method {
                name    CreateSurface

                arg {
                        name        config
                        direction   input
                        type        struct
                        typename    CoreSurfaceConfig
                }

                arg {
                        name        type
                        direction   input
                        type        enum
                        typename    CoreSurfaceTypeFlags
                }

                arg {
                        name        resource_id
                        direction   input
                        type        int
                        typename    u64
                }

                arg {
                        name        palette
                        direction   input
                        type        object
                        typename    CorePalette
                        optional    yes
                }

                arg {
                        name        surface
                        direction   output
                        type        object
                        typename    CoreSurface
                }
        }

        method {
                name    CreatePalette

                arg {
                        name        size
                        direction   input
                        type        int
                        typename    u32
                }

                arg {
                        name        colorspace
                        direction   input
                        type        enum
                        typename    DFBSurfaceColorSpace
                }

                arg {
                        name        palette
                        direction   output
                        type        object
                        typename    CorePalette
                }
        }

        method {
                name    ClipboardSet

                arg {
                        name        mime_type
                        direction   input
                        type        int
                        typename    char
                        count       mime_type_size
                }

                arg {
                        name        mime_type_size
                        direction   input
                        type        int
                        typename    u32
                }

                arg {
                        name        data
                        direction   input
                        type        int
                        typename    char
                        count       data_size
                }

                arg {
                        name        data_size
                        direction   input
                        type        int
                        typename    u32
                }

                arg {
                        name        timestamp_us
                        direction   input
                        type        int
                        typename    u64
                }
        }

        method {
                name    ClipboardGet

                arg {
                        name        mime_type
                        direction   output
                        type        int
                        typename    char
                        max         MAX_CLIPBOARD_MIME_TYPE_SIZE
                        count       mime_type_size
                }

                arg {
                        name        mime_type_size
                        direction   output
                        type        int
                        typename    u32
                }

                arg {
                        name        data
                        direction   output
                        type        int
                        typename    char
                        max         MAX_CLIPBOARD_DATA_SIZE
                        count       data_size
                }

                arg {
                        name        data_size
                        direction   output
                        type        int
                        typename    u32
                }
        }

        method {
                name    ClipboardGetTimestamp

                arg {
                        name        timestamp_us
                        direction   output
                        type        int
                        typename    u64
                }
        }

        method {
                name    WaitIdle
        }

        method {
                name    GetSurface

                arg {
                        name        surface_id
                        direction   input
                        type        int
                        typename    u32
                }

                arg {
                        name        surface
                        direction   output
                        type        object
                        typename    CoreSurface
                }
        }

        method {
                name    AllowSurface

                arg {
                        name        surface
                        direction   input
                        type        object
                        typename    CoreSurface
                }

                arg {
                        name        executable
                        direction   input
                        type        int
                        typename    char
                        count       executable_length
                 }

                 arg {
                        name        executable_length
                        direction   input
                        type        int
                        typename    u32
                 }
        }

        method {
                name    CreateState

                arg {
                        name        state
                        direction   output
                        type        object
                        typename    CoreGraphicsState
                }
        }
}

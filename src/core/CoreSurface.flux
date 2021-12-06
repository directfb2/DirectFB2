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
        name    ISurface
        version 1.0
        object  CoreSurface

        method {
                name    SetConfig

                arg {
                        name        config
                        direction   input
                        type        struct
                        typename    CoreSurfaceConfig
                }
        }

        method {
                name    GetPalette

                arg {
                        name        palette
                        direction   output
                        type        object
                        typename    CorePalette
                }
        }

        method {
                name    SetPalette

                arg {
                        name        palette
                        direction   input
                        type        object
                        typename    CorePalette
                }
        }

        method {
                name    SetAlphaRamp

                arg {
                        name        a0
                        direction   input
                        type        int
                        typename    u8
                }

                arg {
                        name        a1
                        direction   input
                        type        int
                        typename    u8
                }

                arg {
                        name        a2
                        direction   input
                        type        int
                        typename    u8
                }

                arg {
                        name        a3
                        direction   input
                        type        int
                        typename    u8
                }
        }

        method {
                name    PreLockBuffer

                arg {
                        name        buffer
                        direction   input
                        type        object
                        typename    CoreSurfaceBuffer
                }

                arg {
                        name        accessor
                        direction   input
                        type        enum
                        typename    CoreSurfaceAccessorID
                }

                arg {
                        name        access
                        direction   input
                        type        enum
                        typename    CoreSurfaceAccessFlags
                }

                arg {
                        name        allocation
                        direction   output
                        type        object
                        typename    CoreSurfaceAllocation
                }
        }

        method {
                name    PreLockBuffer2

                arg {
                        name        role
                        direction   input
                        type        enum
                        typename    DFBSurfaceBufferRole
                }

                arg {
                        name        eye
                        direction   input
                        type        enum
                        typename    DFBSurfaceStereoEye
                }

                arg {
                        name        accessor
                        direction   input
                        type        enum
                        typename    CoreSurfaceAccessorID
                }

                arg {
                        name        access
                        direction   input
                        type        enum
                        typename    CoreSurfaceAccessFlags
                }

                arg {
                        name        lock
                        direction   input
                        type        enum
                        typename    DFBBoolean
                }

                arg {
                        name        allocation
                        direction   output
                        type        object
                        typename    CoreSurfaceAllocation
                }
        }

        method {
                name    PreLockBuffer3

                arg {
                        name        role
                        direction   input
                        type        enum
                        typename    DFBSurfaceBufferRole
                }

                arg {
                        name        flip_count
                        direction   input
                        type        int
                        typename    u32
                }

                arg {
                        name        eye
                        direction   input
                        type        enum
                        typename    DFBSurfaceStereoEye
                }

                arg {
                        name        accessor
                        direction   input
                        type        enum
                        typename    CoreSurfaceAccessorID
                }

                arg {
                        name        access
                        direction   input
                        type        enum
                        typename    CoreSurfaceAccessFlags
                }

                arg {
                        name        lock
                        direction   input
                        type        enum
                        typename    DFBBoolean
                }

                arg {
                        name        allocation
                        direction   output
                        type        object
                        typename    CoreSurfaceAllocation
                }
        }

        method {
                name     DispatchUpdate
                async    yes

                arg {
                        name        swap
                        direction   input
                        type        enum
                        typename    DFBBoolean
                }

                arg {
                        name        left
                        direction   input
                        type        struct
                        typename    DFBRegion
                        optional    yes
                }

                arg {
                        name        right
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
                        name        timestamp
                        direction   input
                        type        int
                        typename    s64
                }

                arg {
                        name        flip_count
                        direction   input
                        type        int
                        typename    u32
                }
        }

        method {
                name     Flip2

                arg {
                        name        swap
                        direction   input
                        type        enum
                        typename    DFBBoolean
                }

                arg {
                        name        left
                        direction   input
                        type        struct
                        typename    DFBRegion
                        optional    yes
                }

                arg {
                        name        right
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
                        name        timestamp
                        direction   input
                        type        int
                        typename    s64
                }
        }

        method {
                name    SetField

                arg {
                        name        field
                        direction   input
                        type        int
                        typename    s32
                }
        }

        method {
                name    CreateClient

                arg {
                        name        client
                        direction   output
                        type        object
                        typename    CoreSurfaceClient
                }
        }

        method {
                name    Allocate

                arg {
                        name        role
                        direction   input
                        type        enum
                        typename    DFBSurfaceBufferRole
                }

                arg {
                        name        eye
                        direction   input
                        type        enum
                        typename    DFBSurfaceStereoEye
                }

                arg {
                        name        key
                        direction   input
                        type        int
                        typename    char
                        count       key_len
                }

                arg {
                        name        key_len
                        direction   input
                        type        int
                        typename    u32
                }

                arg {
                        name        handle
                        direction   input
                        type        int
                        typename    u64
                }

                arg {
                        name        allocation
                        direction   output
                        type        object
                        typename    CoreSurfaceAllocation
                }
        }

        method {
                name    GetAllocation

                arg {
                        name        role
                        direction   input
                        type        enum
                        typename    DFBSurfaceBufferRole
                }

                arg {
                        name        eye
                        direction   input
                        type        enum
                        typename    DFBSurfaceStereoEye
                }

                arg {
                        name        key
                        direction   input
                        type        int
                        typename    char
                        count       key_len
                }

                arg {
                        name        key_len
                        direction   input
                        type        int
                        typename    u32
                }

                arg {
                        name        allocation
                        direction   output
                        type        object
                        typename    CoreSurfaceAllocation
                }
        }
}

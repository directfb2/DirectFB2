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
        name    IScreen
        version 1.0
        object  CoreScreen

        method {
                name    GetScreenSize

                arg {
                        name        size
                        direction   output
                        type        struct
                        typename    DFBDimension
                }
        }

        method {
                name    SetPowerMode

                arg {
                        name        mode
                        direction   input
                        type        enum
                        typename    DFBScreenPowerMode
                }
        }

        method {
                name    WaitVSync
        }

        method {
                name    TestMixerConfig

                arg {
                        name        mixer
                        direction   input
                        type        int
                        typename    u32
                }

                arg {
                        name        config
                        direction   input
                        type        struct
                        typename    DFBScreenMixerConfig
                }

                arg {
                        name        failed
                        direction   output
                        optional    yes
                        type        enum
                        typename    DFBScreenMixerConfigFlags
                }
        }

        method {
                name    SetMixerConfig

                arg {
                        name        mixer
                        direction   input
                        type        int
                        typename    u32
                }

                arg {
                        name        config
                        direction   input
                        type        struct
                        typename    DFBScreenMixerConfig
                }
        }

        method {
                name    TestEncoderConfig

                arg {
                        name        encoder
                        direction   input
                        type        int
                        typename    u32
                }

                arg {
                        name        config
                        direction   input
                        type        struct
                        typename    DFBScreenEncoderConfig
                }

                arg {
                        name        failed
                        direction   output
                        optional    yes
                        type        enum
                        typename    DFBScreenEncoderConfigFlags
                }
        }

        method {
                name    SetEncoderConfig

                arg {
                        name        encoder
                        direction   input
                        type        int
                        typename    u32
                }

                arg {
                        name        config
                        direction   input
                        type        struct
                        typename    DFBScreenEncoderConfig
                }
        }

        method {
                name    TestOutputConfig

                arg {
                        name        output
                        direction   input
                        type        int
                        typename    u32
                }

                arg {
                        name        config
                        direction   input
                        type        struct
                        typename    DFBScreenOutputConfig
                }

                arg {
                        name        failed
                        direction   output
                        optional    yes
                        type        enum
                        typename    DFBScreenOutputConfigFlags
                }
        }

        method {
                name    SetOutputConfig

                arg {
                        name        output
                        direction   input
                        type        int
                        typename    u32
                }

                arg {
                        name        config
                        direction   input
                        type        struct
                        typename    DFBScreenOutputConfig
                }
        }

        method {
                name    GetVSyncCount

                arg {
                        name        count
                        direction   output
                        type        int
                        typename    u64
                }
        }
}

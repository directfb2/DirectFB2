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

#ifndef __CORE__CORE_SYSTEM_H__
#define __CORE__CORE_SYSTEM_H__

#include <core/system.h>

/**********************************************************************************************************************/

static void           system_get_info             ( CoreSystemInfo   *info );

static DFBResult      system_initialize           ( CoreDFB          *core,
                                                    void            **ret_data );

static DFBResult      system_join                 ( CoreDFB          *core,
                                                    void            **ret_data );

static DFBResult      system_shutdown             ( bool              emergency );

static DFBResult      system_leave                ( bool              emergency );

static DFBResult      system_suspend              ( void );

static DFBResult      system_resume               ( void );

static VideoMode     *system_get_modes            ( void );

static VideoMode     *system_get_current_mode     ( void );

static DFBResult      system_thread_init          ( void );

static bool           system_input_filter         ( CoreInputDevice  *device,
                                                    DFBInputEvent    *event );

static volatile void *system_map_mmio             ( unsigned int      offset,
                                                    int               length );

static void           system_unmap_mmio           ( volatile void    *addr,
                                                    int               length );

static unsigned int   system_get_accelerator      ( void );

static unsigned long  system_video_memory_physical( unsigned int      offset );

static void          *system_video_memory_virtual ( unsigned int      offset );

static unsigned int   system_videoram_length      ( void );

static void           system_get_busid            ( int              *ret_bus,
                                                    int              *ret_dev,
                                                    int              *ret_func );

static void           system_get_deviceid         ( unsigned int     *ret_vendor_id,
                                                    unsigned int     *ret_device_id );

static CoreSystemFuncs system_funcs = {
     .GetSystemInfo       = system_get_info,
     .Initialize          = system_initialize,
     .Join                = system_join,
     .Shutdown            = system_shutdown,
     .Leave               = system_leave,
     .Suspend             = system_suspend,
     .Resume              = system_resume,
     .GetModes            = system_get_modes,
     .GetCurrentMode      = system_get_current_mode,
     .ThreadInit          = system_thread_init,
     .InputFilter         = system_input_filter,
     .MapMMIO             = system_map_mmio,
     .UnmapMMIO           = system_unmap_mmio,
     .GetAccelerator      = system_get_accelerator,
     .VideoMemoryPhysical = system_video_memory_physical,
     .VideoMemoryVirtual  = system_video_memory_virtual,
     .VideoRamLength      = system_videoram_length,
     .GetBusID            = system_get_busid,
     .GetDeviceID         = system_get_deviceid
};

#define DFB_CORE_SYSTEM(shortname)                         \
                                                           \
__dfb_constructor__                                        \
void                                                       \
directfb_##shortname##_ctor( void )                        \
{                                                          \
     direct_modules_register( &dfb_core_systems,           \
                              DFB_CORE_SYSTEM_ABI_VERSION, \
                              #shortname,                  \
                              &system_funcs );             \
}

#endif

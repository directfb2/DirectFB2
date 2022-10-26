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

#ifndef __CORE__GRAPHICS_DRIVER_H__
#define __CORE__GRAPHICS_DRIVER_H__

#include <core/gfxcard.h>

/**********************************************************************************************************************/

static int       driver_probe       ( void );

static void      driver_get_info    ( GraphicsDriverInfo *info );

static DFBResult driver_init_driver ( GraphicsDeviceFuncs *funcs, void *driver_data, void *device_data, CoreDFB *core );

static DFBResult driver_init_device ( GraphicsDeviceInfo *device_info, void *driver_data, void *device_data );

static void      driver_close_device( void *driver_data, void *device_data );

static void      driver_close_driver( void *driver_data );

static GraphicsDriverFuncs gfxdriver_funcs = {
     .Probe         = driver_probe,
     .GetDriverInfo = driver_get_info,
     .InitDriver    = driver_init_driver,
     .InitDevice    = driver_init_device,
     .CloseDevice   = driver_close_device,
     .CloseDriver   = driver_close_driver
};

#define DFB_GRAPHICS_DRIVER(shortname)                         \
                                                               \
__attribute__((constructor))                                   \
static void                                                    \
directfb_##shortname##_ctor()                                  \
{                                                              \
     direct_modules_register( &dfb_graphics_drivers,           \
                              DFB_GRAPHICS_DRIVER_ABI_VERSION, \
                              #shortname,                      \
                              &gfxdriver_funcs );              \
}                                                              \
                                                               \
__attribute__((destructor))                                    \
void                                                           \
directfb_##shortname##_dtor()                                  \
{                                                              \
     direct_modules_unregister( &dfb_graphics_drivers,         \
                                #shortname );                  \
}

#endif

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

#ifndef __CORE__SYSTEM_H__
#define __CORE__SYSTEM_H__

#include <core/coretypes.h>
#include <direct/modules.h>

DECLARE_MODULE_DIRECTORY( dfb_core_systems );

/**********************************************************************************************************************/

#define DFB_CORE_SYSTEM_ABI_VERSION          10

#define DFB_CORE_SYSTEM_INFO_NAME_LENGTH     60
#define DFB_CORE_SYSTEM_INFO_VENDOR_LENGTH   80
#define DFB_CORE_SYSTEM_INFO_URL_LENGTH     120
#define DFB_CORE_SYSTEM_INFO_LICENSE_LENGTH  40

typedef struct {
     int major; /* major version */
     int minor; /* minor version */
} CoreSystemVersion;

typedef enum {
     CSCAPS_NONE            = 0x00000000, /* None of these. */

     CSCAPS_ACCELERATION    = 0x00000001, /* HW acceleration supported, so probe graphics drivers. */
     CSCAPS_PREFER_SHM      = 0x00000002, /* Prefer shared memory surface pool over local memory pool. */
     CSCAPS_SECURE_FUSION   = 0x00000004, /* Fusion needs to be in secure fusion mode. */
     CSCAPS_ALWAYS_INDIRECT = 0x00000008, /* All calls need to be indirect. */
     CSCAPS_SYSMEM_EXTERNAL = 0x00000010, /* Make system memory surface pools have CSTF_EXTERNAL support. */

     CSCAPS_NOTIFY_DISPLAY  = 0x00000040, /* Call dfb_surface_notify_display2() when appropriate. */

     CSCAPS_ALL             = 0x0000005F  /* All of these. */
} CoreSystemCapabilities;

typedef struct {
     CoreSystemVersion      version;

     CoreSystemCapabilities caps;

     char                   name[DFB_CORE_SYSTEM_INFO_NAME_LENGTH];       /* Name of system driver */
     char                   vendor[DFB_CORE_SYSTEM_INFO_VENDOR_LENGTH];   /* Vendor (or author) of the driver */
     char                   url[DFB_CORE_SYSTEM_INFO_URL_LENGTH];         /* URL for driver updates */
     char                   license[DFB_CORE_SYSTEM_INFO_LICENSE_LENGTH]; /* License, e.g. 'LGPL' or 'proprietary' */
} CoreSystemInfo;

typedef struct _VideoMode {
     int xres;
     int yres;
     int bpp;

     int pixclock;
     int left_margin;
     int right_margin;
     int upper_margin;
     int lower_margin;
     int hsync_len;
     int vsync_len;
     int hsync_high;
     int vsync_high;
     int csync_high;

     int laced;
     int doubled;

     int sync_on_green;
     int external_sync;
     int broadcast;

     struct _VideoMode *next;
} VideoMode;

typedef struct {
     void           (*GetSystemInfo)      ( CoreSystemInfo   *info );

     DFBResult      (*Initialize)         ( CoreDFB          *core,
                                            void            **data );

     DFBResult      (*Join)               ( CoreDFB          *core,
                                            void            **data );

     DFBResult      (*Shutdown)           ( bool              emergency );

     DFBResult      (*Leave)              ( bool              emergency );

     DFBResult      (*Suspend)            ( void );

     DFBResult      (*Resume)             ( void );

     VideoMode     *(*GetModes)           ( void );

     VideoMode     *(*GetCurrentMode)     ( void );

     /*
      * Called at the beginning of a new thread.
      */
     DFBResult      (*ThreadInit)         ( void );

     /*
      * Called upon incoming input events.
      * Return true to drop the event, e.g. after doing special handling of it.
      */
     bool           (*InputFilter)        ( CoreInputDevice  *device,
                                            DFBInputEvent    *event );

     /*
      * Graphics drivers call this function to get access to MMIO regions:
      *   'offset': offset from MMIO base (default offset is 0)
      *   'length': length of mapped region (-1 uses default length)
      * Returns the virtual address or NULL if mapping failed.
      */
     volatile void *(*MapMMIO)            ( unsigned int      offset,
                                            int               length );

     /*
      * Graphics drivers call this function to unmap MMIO regions:
      *   'addr':   virtual address of mapped region
      *   'length': length of mapped region (-1 uses default length)
      */
     void           (*UnmapMMIO)          ( volatile void    *addr,
                                            int               length );

     int            (*GetAccelerator)     ( void );

     unsigned long  (*VideoMemoryPhysical)( unsigned int      offset );

     void          *(*VideoMemoryVirtual) ( unsigned int      offset );

     unsigned int   (*VideoRamLength)     ( void );

     void           (*GetBusID)           ( int              *ret_bus,
                                            int              *ret_dev,
                                            int              *ret_func );

     void           (*GetDeviceID)        ( unsigned int     *ret_vendor_id,
                                            unsigned int     *ret_device_id );
} CoreSystemFuncs;

/**********************************************************************************************************************/

DFBResult               dfb_system_lookup               ( void );

CoreSystemCapabilities  dfb_system_caps                 ( void );

void                   *dfb_system_data                 ( void );

VideoMode              *dfb_system_modes                ( void );

VideoMode              *dfb_system_current_mode         ( void );

DFBResult               dfb_system_thread_init          ( void );

bool                    dfb_system_input_filter         ( CoreInputDevice *device,
                                                          DFBInputEvent   *event );

volatile void          *dfb_system_map_mmio             ( unsigned int     offset,
                                                          int              length );

void                    dfb_system_unmap_mmio           ( volatile void   *addr,
                                                          int              length );

int                     dfb_system_get_accelerator      ( void );

unsigned long           dfb_system_video_memory_physical( unsigned int     offset );

void                   *dfb_system_video_memory_virtual ( unsigned int     offset );

unsigned int            dfb_system_videoram_length      ( void );

void                    dfb_system_get_busid            ( int             *ret_bus,
                                                          int             *ret_dev,
                                                          int             *ret_func );

void                    dfb_system_get_deviceid         ( unsigned int    *ret_vendor_id,
                                                          unsigned int    *ret_device_id );

#endif

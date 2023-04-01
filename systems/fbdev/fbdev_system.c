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

#include <core/core.h>
#include <core/core_system.h>
#include <core/layers.h>
#include <core/screens.h>
#include <core/surface_pool.h>
#include <direct/memcpy.h>
#include <direct/system.h>
#include <fusion/shmalloc.h>

#include "fbdev_mode.h"
#include "fbdev_system.h"
#include "fbdev_vt.h"

D_DEBUG_DOMAIN( FBDev_System, "FBDev/System", "FBDev System Module" );

DFB_CORE_SYSTEM( fbdev )

/**********************************************************************************************************************/

extern const ScreenFuncs       fbdevScreenFuncs;
extern const DisplayLayerFuncs fbdevPrimaryLayerFuncs;
extern const SurfacePoolFuncs  fbdevSurfacePoolFuncs;

#define SYS_CLASS_GRAPHICS_DEV        "/sys/class/graphics/%s/device"
#define SYS_CLASS_GRAPHICS_DEV_VENDOR "/sys/class/graphics/%s/device/vendor"
#define SYS_CLASS_GRAPHICS_DEV_MODEL  "/sys/class/graphics/%s/device/device"

static void
get_pci_info( FBDevDataShared *shared )
{
     char          buf[512];
     int           vendor = -1;
     int           model  = -1;
     FILE         *fp;
     unsigned int  bus;
     unsigned int  dev;
     unsigned int  func;
     char          devname[5] = { 'f', 'b', '0', 0, 0 };
     char          path[128];
     int           len;

     /* Try sysfs interface. */

     if (!strncmp( shared->device_name, "/dev/fb/", 8 ))
          snprintf( devname, sizeof(devname), "fb%s", shared->device_name + 8 );
     else if (!strncmp( shared->device_name, "/dev/fb", 7 ))
          snprintf( devname, sizeof(devname), "fb%s", shared->device_name + 7 );

     snprintf( path, sizeof(path), SYS_CLASS_GRAPHICS_DEV, devname );

     len = readlink( path, buf, sizeof(buf) );
     if (len != -1) {
          buf[len] = '\0';

          if (sscanf( basename( buf ), "0000:%02x:%02x.%1x", &bus, &dev, &func ) == 3) {
               shared->pci.bus  = bus;
               shared->pci.dev  = dev;
               shared->pci.func = func;
           }

          snprintf( path, sizeof(path), SYS_CLASS_GRAPHICS_DEV_VENDOR, devname );

          fp = fopen( path, "r" );
          if (fp) {
               if (fgets( buf, sizeof(buf), fp)) {
                    if (sscanf( buf, "0x%04x", (unsigned int*) &vendor ) == 1)
                         shared->device.vendor = vendor;
               }

               fclose( fp );
          }
          else
               D_DEBUG_AT( FBDev_System, "Couldn't access '%s'!\n", path );

          snprintf( path, sizeof(path), SYS_CLASS_GRAPHICS_DEV_MODEL, devname );

          fp = fopen( path, "r" );
          if (fp) {
               if (fgets( buf, sizeof(buf), fp)) {
                    if (sscanf( buf, "0x%04x", (unsigned int*) &model ) == 1)
                         shared->device.model = model;
               }

               fclose( fp );
          }
          else
               D_DEBUG_AT( FBDev_System, "Couldn't access '%s'!\n", path );
     }
     else
          D_DEBUG_AT( FBDev_System, "Couldn't access '%s'!\n", path );

     /* Try procfs interface. */

     if (vendor == -1 || model == -1) {
          const char   *value;
          int           pci_bus = 1, pci_dev = 0, pci_func = 0;
          unsigned int  id;

          fp = fopen( "/proc/bus/pci/devices", "r" );
          if (!fp) {
               D_DEBUG_AT( FBDev_System, "Couldn't access '/proc/bus/pci/devices'!\n" );
               return;
          }

          /* Get the PCI Bus ID of the graphics card (default 1:0:0) */
          if ((value = direct_config_get_value( "busid" ))) {
               if (sscanf( value, "%d:%d:%d", &pci_bus, &pci_dev, &pci_func ) != 3) {
                    D_ERROR( "FBDev/System: Couldn't parse busid!\n" );
                    return;
               }
          }

          while (fgets( buf, sizeof(buf), fp )) {
               if (sscanf( buf, "%04x\t%04x%04x", &id, (unsigned int*) &vendor, (unsigned int*) &model ) == 3) {
                    bus  = (id & 0xff00) >> 8;
                    dev  = (id & 0x00ff) >> 3;
                    func =  id & 0x0007 ;

                    if (bus == pci_bus && dev == pci_dev && func == pci_func) {
                         shared->pci.bus       = bus;
                         shared->pci.dev       = dev;
                         shared->pci.func      = func;
                         shared->device.vendor = vendor;
                         shared->device.model  = model;
                         break;
                    }
               }
          }

          fclose( fp );
     }
}

static FusionCallHandlerResult
fbdev_ioctl_call_handler( int           caller,
                          int           call_arg,
                          void         *call_ptr,
                          void         *ctx,
                          unsigned int  serial,
                          int          *ret_val )
{
     int              erno;
     FBDevData       *fbdev = ctx;
     FBDevDataShared *shared;

     D_ASSERT( fbdev != NULL );
     D_ASSERT( fbdev->shared != NULL );

     shared = fbdev->shared;

     if (shared->vt && call_arg == FBIOPUT_VSCREENINFO)
          fbdev_vt_set_graphics_mode( true );

     erno = ioctl( fbdev->fd, call_arg, call_ptr );
     if (erno < 0)
          erno = errno;

     if (shared->vt && call_arg == FBIOPUT_VSCREENINFO)
          fbdev_vt_set_graphics_mode( false );

     *ret_val = erno;

     return FCHR_RETURN;
}

int
fbdev_ioctl( FBDevData *fbdev,
             int        request,
             void      *arg,
             int        arg_size )
{
     int              erno;
     void            *ptr = NULL;
     FBDevDataShared *shared;

     D_DEBUG_AT( FBDev_System, "%s()\n", __FUNCTION__ );

     D_ASSERT( fbdev != NULL );
     D_ASSERT( fbdev->shared != NULL );

     shared = fbdev->shared;

     if (request == FBIOPAN_DISPLAY || request == FBIO_WAITFORVSYNC || request == FBIOBLANK)
          return ioctl( fbdev->fd, request, arg );

     if (dfb_core_is_master( fbdev->core )) {
          fbdev_ioctl_call_handler( 1, request, arg, fbdev, 0, &erno );
          errno = erno;
          return errno ? -1 : 0;
     }

     if (arg) {
          if (!fusion_is_shared( dfb_core_world( fbdev->core ), arg )) {
               ptr = SHMALLOC( shared->shmpool, arg_size );
               if (!ptr) {
                    errno = ENOMEM;
                    return -1;
               }

               direct_memcpy( ptr, arg, arg_size );
          }
     }

     fusion_call_execute( &shared->call, FCEF_NONE, request, ptr ?: arg, &erno );

     if (ptr) {
          direct_memcpy( arg, ptr, arg_size );
          SHFREE( shared->shmpool, ptr );
     }

     errno = erno;

     return errno ? -1 : 0;
}

static DFBResult
local_init( const char *device_name,
            FBDevData  *fbdev )
{
     CoreScreen *screen;

     /* Open framebuffer device. */
     fbdev->fd = open( device_name, O_RDWR );
     if (fbdev->fd < 0) {
          D_PERROR( "FBDev/System: Failed to open '%s'!\n", device_name );
          return DFB_INIT;
     }

     if (fcntl( fbdev->fd, F_SETFD, FD_CLOEXEC ) < 0) {
          D_PERROR( "FBDev/System: Setting FD_CLOEXEC flag failed!\n" );
          return DFB_INIT;
     }

     /* Retrieve fixed screen information. */
     fbdev->fix = D_CALLOC( 1, sizeof(struct fb_fix_screeninfo) );
     if (!fbdev->fix)
          return D_OOM();

     if (ioctl( fbdev->fd, FBIOGET_FSCREENINFO, fbdev->fix ) < 0) {
          D_PERROR( "FBDev/System: Could not retrieve fixed screen information!\n" );
          return DFB_INIT;
     }

     D_INFO( "FBDev/System: Found '%s' (ID %u) with framebuffer at 0x%08lx, %uk (MMIO 0x%08lx, %uk)\n",
             fbdev->fix->id, fbdev->fix->accel,
             fbdev->fix->smem_start, fbdev->fix->smem_len >> 10, fbdev->fix->mmio_start, fbdev->fix->mmio_len >> 10 );

     /* Map the framebuffer. */
     fbdev->addr = mmap( NULL, fbdev->fix->smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fbdev->fd, 0 );
     if (fbdev->addr == MAP_FAILED) {
          D_PERROR( "FBDev/System: Could not mmap the framebuffer!\n" );
          return DFB_INIT;
     }

     screen = dfb_screens_register( fbdev, &fbdevScreenFuncs );

     dfb_layers_register( screen, fbdev, &fbdevPrimaryLayerFuncs );

     return DFB_OK;
}

static DFBResult
local_deinit( FBDevData *fbdev )
{
     if (fbdev->addr)
          munmap( fbdev->addr, fbdev->fix->smem_len );

     if (fbdev->fix)
          D_FREE( fbdev->fix );

     if (fbdev->fd != -1)
          close( fbdev->fd );

     return DFB_OK;
}

/**********************************************************************************************************************/

static void
system_get_info( CoreSystemInfo *info )
{
     info->version.major = 0;
     info->version.minor = 1;

     info->caps = CSCAPS_ACCELERATION;

     snprintf( info->name,   DFB_CORE_SYSTEM_INFO_NAME_LENGTH,   "FBDev" );
     snprintf( info->vendor, DFB_CORE_SYSTEM_INFO_VENDOR_LENGTH, "DirectFB" );
}

static DFBResult
system_initialize( CoreDFB  *core,
                   void    **ret_data )
{
     DFBResult            ret;
     FBDevData           *fbdev;
     FBDevDataShared     *shared;
     FusionSHMPoolShared *pool;
     const char          *value;

     D_DEBUG_AT( FBDev_System, "%s()\n", __FUNCTION__ );

     fbdev = D_CALLOC( 1, sizeof(FBDevData) );
     if (!fbdev)
          return D_OOM();

     fbdev->core = core;

     pool = dfb_core_shmpool( core );

     shared = SHCALLOC( pool, 1, sizeof(FBDevDataShared) );
     if (!shared) {
          D_FREE( fbdev );
          return D_OOSHM();
     }

     shared->shmpool = pool;

     fbdev->shared = shared;

     if ((value = direct_config_get_value( "fbdev" ))) {
          direct_snputs( shared->device_name, value, 255 );
          D_INFO( "FBDev/System: Using device %s as specified in DirectFB configuration\n", shared->device_name );
     }
     else if (getenv( "FRAMEBUFFER" ) && *getenv( "FRAMEBUFFER" ) != '\0') {
          direct_snputs( shared->device_name, getenv( "FRAMEBUFFER" ), 255 );
          D_INFO( "FBDev/System: Using device %s as set in FRAMEBUFFER environment variable\n", shared->device_name );
     }
     else {
          snprintf( shared->device_name, 255, "/dev/fb0" );
          D_INFO( "FBDev/System: Using device %s (default)\n", shared->device_name );
     }

     if ((value = direct_config_get_value( "fbmodes" )))
          direct_snputs( shared->modes_file, value, 255 );
     else if (getenv( "MODES" ) && *getenv( "MODES" ) != '\0')
          direct_snputs( shared->device_name, getenv( "MODES" ), 255 );
     else
          snprintf( shared->modes_file, 255, "/etc/fb.modes" );

     if (direct_config_has_name( "no-vt" ) && !direct_config_has_name( "vt" ))
          D_INFO( "FBDev/System: Don't use VT handling\n" );
     else
          shared->vt = true;

     if (direct_config_has_name( "vsync-after" ) && !direct_config_has_name( "no-vsync-after" ))
          shared->pollvsync_after = true;

     if (direct_config_has_name( "vsync-none" ) && !direct_config_has_name( "no-vsync-none" ))
          shared->pollvsync_none = true;

     ret = local_init( shared->device_name, fbdev );
     if (ret)
          goto error;

     if (shared->vt) {
          ret = fbdev_vt_initialize( core, fbdev->fd );
          if (ret)
               goto error;
     }

     if (ioctl( fbdev->fd, FBIOGET_VSCREENINFO, &shared->orig_var ) < 0) {
          D_PERROR( "FBDev/System: Could not get variable screen information!\n" );
          ret = DFB_INIT;
          goto error;
     }

     shared->current_var = shared->orig_var;
     shared->current_var.accel_flags = 0;

     if (ioctl( fbdev->fd, FBIOPUT_VSCREENINFO, &shared->current_var ) < 0) {
          D_PERROR( "FBDev/System: Could not disable console acceleration!\n" );
          ret = DFB_INIT;
          goto error;
     }

     fbdev_var_to_mode( &shared->current_var, &shared->mode );

     shared->page_mask = direct_pagesize() < 0 ? 0 : (direct_pagesize() - 1);

     shared->shmpool_data = dfb_core_shmpool_data( core );

     shared->orig_cmap_memory = SHMALLOC( shared->shmpool_data, 256 * 2 * 4 );
     if (!shared->orig_cmap_memory) {
          ret = D_OOSHM();
          goto error;
     }

     shared->orig_cmap.start  = 0;
     shared->orig_cmap.len    = 256;
     shared->orig_cmap.red    = shared->orig_cmap_memory + 256 * 2 * 0;
     shared->orig_cmap.green  = shared->orig_cmap_memory + 256 * 2 * 1;
     shared->orig_cmap.blue   = shared->orig_cmap_memory + 256 * 2 * 2;
     shared->orig_cmap.transp = shared->orig_cmap_memory + 256 * 2 * 3;

     if (ioctl( fbdev->fd, FBIOGETCMAP, &shared->orig_cmap ) < 0) {
          D_DEBUG_AT( FBDev_System, "  -> Could not retrieve palette for backup\n" );
          shared->orig_cmap.len = 0;
     }

     shared->current_cmap_memory = SHMALLOC( shared->shmpool_data, 256 * 2 * 4 );
     if (!shared->current_cmap_memory) {
          ret = D_OOSHM();
          goto error;
     }

     shared->current_cmap.start  = 0;
     shared->current_cmap.len    = 256;
     shared->current_cmap.red    = shared->current_cmap_memory + 256 * 2 * 0;
     shared->current_cmap.green  = shared->current_cmap_memory + 256 * 2 * 1;
     shared->current_cmap.blue   = shared->current_cmap_memory + 256 * 2 * 2;
     shared->current_cmap.transp = shared->current_cmap_memory + 256 * 2 * 3;

     shared->temp_cmap_memory = SHMALLOC( shared->shmpool_data, 256 * 2 * 4 );
     if (!shared->temp_cmap_memory) {
          ret = D_OOSHM();
          goto error;
     }

     shared->temp_cmap.start  = 0;
     shared->temp_cmap.len    = 256;
     shared->temp_cmap.red    = shared->temp_cmap_memory + 256 * 2 * 0;
     shared->temp_cmap.green  = shared->temp_cmap_memory + 256 * 2 * 1;
     shared->temp_cmap.blue   = shared->temp_cmap_memory + 256 * 2 * 2;
     shared->temp_cmap.transp = shared->temp_cmap_memory + 256 * 2 * 3;

     /* Initialize the mode table. */
     ret = fbdev_init_modes( fbdev );
     if (ret)
          return ret;

     shared->device.vendor = 0xffff;
     shared->device.model  = 0xffff;
     get_pci_info( shared );

     fusion_call_init( &shared->call, fbdev_ioctl_call_handler, fbdev, dfb_core_world( core ) );

     *ret_data = fbdev;

     ret = dfb_surface_pool_initialize( core, &fbdevSurfacePoolFuncs, &shared->pool );
     if (ret)
          goto error;

     ret = core_arena_add_shared_field( core, "fbdev", shared );
     if (ret)
          goto error;

     return DFB_OK;

error:
     if (shared->orig_cmap_memory)
          SHFREE( shared->shmpool_data, shared->orig_cmap_memory );

     if (shared->temp_cmap_memory)
          SHFREE( shared->shmpool_data, shared->temp_cmap_memory );

     if (shared->current_cmap_memory)
          SHFREE( shared->shmpool_data, shared->current_cmap_memory );

     if (shared->orig_var.accel_flags)
          ioctl( fbdev->fd, FBIOPUT_VSCREENINFO, &shared->orig_var );

     if (shared->vt)
          fbdev_vt_shutdown( false, fbdev->fd );

     local_deinit( fbdev );

     SHFREE( pool, shared );

     D_FREE( fbdev );

     return ret;
}

static DFBResult
system_join( CoreDFB  *core,
             void    **ret_data )
{
     DFBResult        ret;
     FBDevData       *fbdev;
     FBDevDataShared *shared;

     D_DEBUG_AT( FBDev_System, "%s()\n", __FUNCTION__ );

     fbdev = D_CALLOC( 1, sizeof(FBDevData) );
     if (!fbdev)
          return D_OOM();

     fbdev->core = core;

     ret = core_arena_get_shared_field( core, "fbdev", (void**) &shared );
     if (ret) {
          D_FREE( fbdev );
          return ret;
     }

     fbdev->shared = shared;

     ret = local_init( shared->device_name, fbdev );
     if (ret)
          goto error;

     *ret_data = fbdev;

     ret = dfb_surface_pool_join( core, shared->pool, &fbdevSurfacePoolFuncs );
     if (ret)
          goto error;

     return DFB_OK;

error:
     local_deinit( fbdev );

     D_FREE( fbdev );

     return ret;
}

static DFBResult
system_shutdown( bool emergency )
{
     FBDevData       *fbdev = dfb_system_data();
     FBDevDataShared *shared;
     VideoMode       *mode;

     D_DEBUG_AT( FBDev_System, "%s()\n", __FUNCTION__ );

     D_ASSERT( fbdev != NULL );
     D_ASSERT( fbdev->shared != NULL );

     shared = fbdev->shared;

     dfb_surface_pool_destroy( shared->pool );

     fusion_call_destroy( &shared->call );

     mode = shared->modes;
     while (mode) {
          VideoMode *next = mode->next;
          SHFREE( shared->shmpool, mode );
          mode = next;
     }

     if (shared->temp_cmap_memory)
          SHFREE( shared->shmpool_data, shared->temp_cmap_memory );

     if (shared->current_cmap_memory)
          SHFREE( shared->shmpool_data, shared->current_cmap_memory );

     if (shared->orig_cmap.len)
          ioctl( fbdev->fd, FBIOPUTCMAP, &shared->orig_cmap );

     if (shared->orig_cmap_memory)
          SHFREE( shared->shmpool_data, shared->orig_cmap_memory );

     ioctl( fbdev->fd, FBIOPUT_VSCREENINFO, &shared->orig_var );

     if (shared->vt)
          fbdev_vt_shutdown( emergency, fbdev->fd );

     local_deinit( fbdev );

     SHFREE( shared->shmpool, shared );

     D_FREE( fbdev );

     return DFB_OK;
}

static DFBResult
system_leave( bool emergency )
{
     FBDevData       *fbdev = dfb_system_data();
     FBDevDataShared *shared;

     D_DEBUG_AT( FBDev_System, "%s()\n", __FUNCTION__ );

     D_ASSERT( fbdev != NULL );
     D_ASSERT( fbdev->shared != NULL );

     shared = fbdev->shared;

     dfb_surface_pool_leave( shared->pool );

     local_deinit( fbdev );

     D_FREE( fbdev );

     return DFB_OK;
}

static DFBResult
system_suspend()
{
     return DFB_OK;
}

static DFBResult
system_resume()
{
     return DFB_OK;
}

static VideoMode *
system_get_modes()
{
     FBDevData       *fbdev = dfb_system_data();
     FBDevDataShared *shared;

     D_ASSERT( fbdev != NULL );
     D_ASSERT( fbdev->shared != NULL );

     shared = fbdev->shared;

     return shared->modes;
}

static VideoMode *
system_get_current_mode()
{
     FBDevData       *fbdev = dfb_system_data();
     FBDevDataShared *shared;

     D_ASSERT( fbdev != NULL );
     D_ASSERT( fbdev->shared != NULL );

     shared = fbdev->shared;

     return &shared->mode;
}

static DFBResult
system_thread_init()
{
     return DFB_OK;
}

static bool
system_input_filter( CoreInputDevice *device,
                     DFBInputEvent   *event )
{
     FBDevData       *fbdev = dfb_system_data();
     FBDevDataShared *shared;

     D_ASSERT( fbdev != NULL );
     D_ASSERT( fbdev->shared != NULL );

     shared = fbdev->shared;

     if (shared->vt) {
          if (DFB_KEY_TYPE( event->key_symbol ) == DIKT_FUNCTION && event->modifiers == (DIMM_CONTROL | DIMM_ALT) &&
              (event->type == DIET_KEYPRESS || event->type == DIET_KEYRELEASE))
               return fbdev_vt_switch_num( event->key_symbol - DIKS_F1 + 1, event->type == DIET_KEYPRESS );
     }

     return false;
}

static volatile void *
system_map_mmio( unsigned int offset,
                 int          length )
{
     FBDevData       *fbdev = dfb_system_data();
     FBDevDataShared *shared;
     void            *addr;

     D_ASSERT( fbdev != NULL );
     D_ASSERT( fbdev->shared != NULL );

     shared = fbdev->shared;

     if (length <= 0)
          length = fbdev->fix->mmio_len;

     addr = mmap( NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fbdev->fd, fbdev->fix->smem_len + offset );
     if (addr == MAP_FAILED) {
          D_PERROR( "FBDev/System: Could not mmap MMIO region (offset %u, length %d)!\n", offset, length );
          return NULL;
     }

     return (u8*) addr + (fbdev->fix->mmio_start & shared->page_mask);
}

static void
system_unmap_mmio( volatile void *addr,
                   int            length )
{
     FBDevData       *fbdev = dfb_system_data();
     FBDevDataShared *shared;

     D_ASSERT( fbdev != NULL );
     D_ASSERT( fbdev->shared != NULL );

     shared = fbdev->shared;

     if (length <= 0)
          length = fbdev->fix->mmio_len;

     if (munmap( (u8*) addr - (fbdev->fix->mmio_start & shared->page_mask), length ) < 0)
          D_PERROR( "FBDev/System: Could not unmap MMIO region at %p (length %d)!\n", addr, length );
}

static int
system_get_accelerator()
{
     FBDevData *fbdev = dfb_system_data();
     int        accel;

     D_ASSERT( fbdev != NULL );

     /* Accelerator ID selecting the graphics driver. */
     accel = direct_config_get_int_value( "accelerator" );
     if (accel)
          return accel;

     return fbdev->fix->accel;
}

static unsigned long
system_video_memory_physical( unsigned int offset )
{
     FBDevData *fbdev = dfb_system_data();

     D_ASSERT( fbdev != NULL );

     return fbdev->fix->smem_start + offset;
}

static void *
system_video_memory_virtual( unsigned int offset )
{
     FBDevData *fbdev = dfb_system_data();

     D_ASSERT( fbdev != NULL );

     return (u8*) fbdev->addr + offset;
}

static unsigned int
system_videoram_length()
{
     FBDevData *fbdev = dfb_system_data();

     D_ASSERT( fbdev != NULL );

     return fbdev->fix->smem_len;
}

static void
system_get_busid( int *ret_bus,
                  int *ret_dev,
                  int *ret_func )
{
     FBDevData       *fbdev = dfb_system_data();
     FBDevDataShared *shared;

     D_ASSERT( fbdev != NULL );
     D_ASSERT( fbdev->shared != NULL );

     shared = fbdev->shared;

     *ret_bus  = shared->pci.bus;
     *ret_dev  = shared->pci.dev;
     *ret_func = shared->pci.func;
}

static void
system_get_deviceid( unsigned int *ret_vendor_id,
                     unsigned int *ret_device_id )
{
     FBDevData       *fbdev = dfb_system_data();
     FBDevDataShared *shared;

     D_ASSERT( fbdev != NULL );
     D_ASSERT( fbdev->shared != NULL );

     shared = fbdev->shared;

     *ret_vendor_id = shared->device.vendor;
     *ret_device_id = shared->device.model;
}

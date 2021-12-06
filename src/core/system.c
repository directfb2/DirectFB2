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

#include <core/core_parts.h>
#include <core/system.h>
#include <direct/util.h>
#include <misc/conf.h>

D_DEBUG_DOMAIN( Core_System, "Core/System", "DirectFB Core System" );

DEFINE_MODULE_DIRECTORY( dfb_core_systems, "systems", DFB_CORE_SYSTEM_ABI_VERSION );

/**********************************************************************************************************************/

typedef struct {
     int            magic;

     CoreSystemInfo system_info;
} DFBSystemCoreShared;

typedef struct {
     int                  magic;

     CoreDFB             *core;

     DFBSystemCoreShared *shared;
} DFBSystemCore;

DFB_CORE_PART( system_core, SystemCore );

/**********************************************************************************************************************/

static CoreSystemInfo         system_info;
static DirectModuleEntry     *system_module = NULL;
static const CoreSystemFuncs *system_funcs  = NULL;
static void                  *system_data   = NULL;

static DFBResult
dfb_system_core_initialize( CoreDFB             *core,
                            DFBSystemCore       *data,
                            DFBSystemCoreShared *shared )
{
     DFBResult ret;

     D_DEBUG_AT( Core_System, "%s( %p, %p, %p )\n", __FUNCTION__, core, data, shared );

     D_ASSERT( data != NULL );
     D_ASSERT( shared != NULL );

     data->core   = core;
     data->shared = shared;

     shared->system_info = system_info;

     /* Initialize system module. */
     ret = system_funcs->Initialize( core, &system_data );
     if (ret)
          return ret;

     D_MAGIC_SET( data, DFBSystemCore );
     D_MAGIC_SET( shared, DFBSystemCoreShared );

     return DFB_OK;
}

static DFBResult
dfb_system_core_join( CoreDFB             *core,
                      DFBSystemCore       *data,
                      DFBSystemCoreShared *shared )
{
     DFBResult ret;

     D_DEBUG_AT( Core_System, "%s( %p, %p, %p )\n", __FUNCTION__, core, data, shared );

     D_ASSERT( data != NULL );
     D_MAGIC_ASSERT( shared, DFBSystemCoreShared );

     data->core   = core;
     data->shared = shared;

     if (strcmp( shared->system_info.name, system_info.name )) {
          D_ERROR( "Core/System: Running system '%s' doesn't match system '%s'!\n",
                    shared->system_info.name, system_info.name );

          return DFB_UNSUPPORTED;
     }

     if (shared->system_info.version.major != system_info.version.major ||
         shared->system_info.version.minor != system_info.version.minor) {
          D_ERROR( "Core/System: Running system version '%d.%d' doesn't match version '%d.%d'!\n",
                    shared->system_info.version.major, shared->system_info.version.minor,
                    system_info.version.major, system_info.version.minor );

          return DFB_UNSUPPORTED;
     }

     /* Join system module. */
     ret = system_funcs->Join( core, &system_data );
     if (ret)
          return ret;

     D_MAGIC_SET( data, DFBSystemCore );

     return DFB_OK;
}

static DFBResult
dfb_system_core_shutdown( DFBSystemCore *data,
                          bool           emergency )
{
     DFBResult            ret;
     DFBSystemCoreShared *shared;

     D_UNUSED_P( shared );

     D_DEBUG_AT( Core_System, "%s( %p, %semergency )\n", __FUNCTION__, data, emergency ? "" : "no " );

     D_MAGIC_ASSERT( data, DFBSystemCore );
     D_MAGIC_ASSERT( data->shared, DFBSystemCoreShared );

     shared = data->shared;

     /* Shutdown system module. */
     ret = system_funcs->Shutdown( emergency );

     /* Unload the module. */
     direct_module_unref( system_module );

     D_MAGIC_CLEAR( data );
     D_MAGIC_CLEAR( shared );

     system_data   = NULL;
     system_funcs  = NULL;
     system_module = NULL;

     return ret;
}

static DFBResult
dfb_system_core_leave( DFBSystemCore *data,
                       bool           emergency )
{
     DFBResult ret;

     D_DEBUG_AT( Core_System, "%s( %p, %semergency )\n", __FUNCTION__, data, emergency ? "" : "no " );

     D_MAGIC_ASSERT( data, DFBSystemCore );
     D_MAGIC_ASSERT( data->shared, DFBSystemCoreShared );

     ret = system_funcs->Leave( emergency );

     /* Unload the module. */
     direct_module_unref( system_module );

     D_MAGIC_CLEAR( data );

     system_data   = NULL;
     system_funcs  = NULL;
     system_module = NULL;

     return ret;
}

static DFBResult
dfb_system_core_suspend( DFBSystemCore *data )
{
     D_DEBUG_AT( Core_System, "%s( %p )\n", __FUNCTION__, data );

     D_MAGIC_ASSERT( data, DFBSystemCore );
     D_MAGIC_ASSERT( data->shared, DFBSystemCoreShared );

     system_funcs->Suspend();

     return DFB_OK;
}

static DFBResult
dfb_system_core_resume( DFBSystemCore *data )
{
     D_DEBUG_AT( Core_System, "%s( %p )\n", __FUNCTION__, data );

     D_MAGIC_ASSERT( data, DFBSystemCore );
     D_MAGIC_ASSERT( data->shared, DFBSystemCoreShared );

     system_funcs->Resume();

     return DFB_OK;
}

/**********************************************************************************************************************/

DFBResult
dfb_system_lookup()
{
     DirectModuleEntry *module;

     D_DEBUG_AT( Core_System, "%p()\n", __FUNCTION__ );

     direct_modules_explore_directory( &dfb_core_systems );

     direct_list_foreach (module, dfb_core_systems.entries) {
          const CoreSystemFuncs *funcs;

          D_DEBUG_AT( Core_System, "module %p\n", module );
          D_DEBUG_AT( Core_System, "  name     '%s'\n", module->name );
          D_DEBUG_AT( Core_System, "  refs      %d\n",  module->refs );
          D_DEBUG_AT( Core_System, "  loaded    %d\n",  module->loaded );
          D_DEBUG_AT( Core_System, "  disabled  %d\n",  module->disabled );
          D_DEBUG_AT( Core_System, "  dynamic   %d\n",  module->dynamic );
          D_DEBUG_AT( Core_System, "  file     '%s'\n", module->file );

          funcs = direct_module_ref( module );
          if (!funcs)
               continue;

          if (!system_module || (!dfb_config->system || !strcasecmp( dfb_config->system, module->name ))) {
               if (system_module)
                    direct_module_unref( system_module );

               system_module = module;
               system_funcs  = funcs;

               memset( &system_info, 0, sizeof(system_info) );

               funcs->GetSystemInfo( &system_info );
          }
          else
               direct_module_unref( module );
     }

     if (!system_module) {
          D_ERROR( "Core/System: No system found!\n" );
          return DFB_NOIMPL;
     }

     return DFB_OK;
}

CoreSystemCapabilities
dfb_system_caps()
{
     return system_info.caps;
}

void *
dfb_system_data()
{
     return system_data;
}

VideoMode *
dfb_system_modes()
{
     D_ASSERT( system_funcs != NULL );

     return system_funcs->GetModes();
}

VideoMode *
dfb_system_current_mode()
{
     D_ASSERT( system_funcs != NULL );

     return system_funcs->GetCurrentMode();
}

DFBResult
dfb_system_thread_init()
{
     D_ASSERT( system_funcs != NULL );

     return system_funcs->ThreadInit();
}

bool
dfb_system_input_filter( CoreInputDevice *device,
                         DFBInputEvent   *event )
{
     D_ASSERT( system_funcs != NULL );

     return system_funcs->InputFilter( device, event );
}

volatile void *
dfb_system_map_mmio( unsigned int offset,
                     int          length )
{
     D_ASSERT( system_funcs != NULL );

     return system_funcs->MapMMIO( offset, length );
}

void
dfb_system_unmap_mmio( volatile void *addr,
                       int            length )
{
     D_ASSERT( system_funcs != NULL );

     system_funcs->UnmapMMIO( addr, length );
}

int
dfb_system_get_accelerator()
{
     D_ASSERT( system_funcs != NULL );

     return system_funcs->GetAccelerator();
}

unsigned long
dfb_system_video_memory_physical( unsigned int offset )
{
     D_ASSERT( system_funcs != NULL );

     return system_funcs->VideoMemoryPhysical( offset );
}

void *
dfb_system_video_memory_virtual( unsigned int offset )
{
     D_ASSERT( system_funcs != NULL );

     return system_funcs->VideoMemoryVirtual( offset );
}

unsigned int
dfb_system_videoram_length()
{
     D_ASSERT( system_funcs != NULL );

     return system_funcs->VideoRamLength();
}

void
dfb_system_get_busid( int *ret_bus,
                      int *ret_dev,
                      int *ret_func )
{
     int bus = -1, dev = -1, func = -1;

     D_ASSERT( system_funcs != NULL );

     system_funcs->GetBusID( &bus, &dev, &func );

     if (ret_bus)
          *ret_bus = bus;
     if (ret_dev)
          *ret_dev = dev;
     if (ret_func)
          *ret_func = func;
}

void
dfb_system_get_deviceid( unsigned int *ret_vendor_id,
                         unsigned int *ret_device_id )
{
     unsigned int vendor_id = 0, device_id = 0;

     D_ASSERT( system_funcs != NULL );

     system_funcs->GetDeviceID( &vendor_id, &device_id );

     if (ret_vendor_id)
          *ret_vendor_id = vendor_id;

     if (ret_device_id)
          *ret_device_id = device_id;
}

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

#include <direct/debug.h>
#include <direct/mem.h>
#include <fusion/conf.h>

D_DEBUG_DOMAIN( Fusion_Config, "Fusion/Config", "Fusion Runtime Configuration options" );

/**********************************************************************************************************************/

static FusionConfig conf = { 0 };

FusionConfig *fusion_config = &conf;
const char   *fusion_config_usage =
     "libfusion options:\n"
     "  tmpfs=<directory>              Location of the shared memory file in multi application mode (default = auto)\n"
     "  shmfile-group=<groupname>      Group that owns shared memory files\n"
     "  [no-]force-slave               Always enter as a slave, waiting for the master, if not there\n"
     "  [no-]fork-handler              Register fork handlers\n"
     "  [no-]debugshm                  Enable shared memory allocation tracking\n"
     "  [no-]madv-remove               Enable usage of MADV_REMOVE (default = auto)\n"
     "  [no-]secure-fusion             Use secure fusion, e.g. read-only shm (default enabled)\n"
     "  [no-]defer-destructors         Handle destructor calls in separate thread\n"
     "  trace-ref=<hexid>              Trace FusionRef up/down ('all' traces all)\n"
     "  call-bin-max-num=<n>           Set maximum call number for async call buffer (default = 512, 0 = disable)\n"
     "  call-bin-max-data=<n>          Set maximum call data size for async call buffer (default = 65536)\n"
     "  [no-]shutdown-info             Dump objects from all pools if some objects remain alive\n"
     "\n";

/**********************************************************************************************************************/

void
__Fusion_conf_init()
{
     fusion_config->shmfile_gid       = -1;
     fusion_config->secure_fusion     = true;
     fusion_config->call_bin_max_num  = 512;
     fusion_config->call_bin_max_data = 65536;
}

void
__Fusion_conf_deinit()
{
}

/**********************************************************************************************************************/

DirectResult
fusion_config_set( const char *name,
                   const char *value )
{
     if (strcmp( name, "tmpfs" ) == 0) {
          if (value) {
               if (fusion_config->tmpfs)
                    D_FREE( fusion_config->tmpfs );
               fusion_config->tmpfs = D_STRDUP( value );
          }
          else {
               D_ERROR( "Fusion/Config: '%s': No directory name specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp( name, "shmfile-group" ) == 0) {
          if (value) {
               struct group *group_info;

               group_info = getgrnam( value );
               if (group_info)
                    fusion_config->shmfile_gid = group_info->gr_gid;
               else
                    D_PERROR( "Fusion/Config: 'shmfile-group': Group '%s' not found!\n", value );
          }
          else {
               D_ERROR( "Fusion/Config: '%s': No file group name specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp( name, "force-slave" ) == 0) {
          fusion_config->force_slave = true;
     } else
     if (strcmp( name, "no-force-slave" ) == 0) {
          fusion_config->force_slave = false;
     } else
     if (strcmp( name, "fork-handler" ) == 0) {
          fusion_config->fork_handler = true;
     } else
     if (strcmp( name, "no-fork-handler" ) == 0) {
          fusion_config->fork_handler = false;
     } else
     if (strcmp( name, "debugshm" ) == 0) {
          fusion_config->debugshm = true;
     } else
     if (strcmp( name, "no-debugshm" ) == 0) {
          fusion_config->debugshm = false;
     } else
     if (strcmp( name, "madv-remove" ) == 0) {
          fusion_config->madv_remove       = true;
          fusion_config->madv_remove_force = true;
     } else
     if (strcmp( name, "no-madv-remove" ) == 0) {
          fusion_config->madv_remove       = false;
          fusion_config->madv_remove_force = true;
     } else
     if (strcmp( name, "secure-fusion" ) == 0) {
          fusion_config->secure_fusion = true;
     } else
     if (strcmp( name, "no-secure-fusion" ) == 0) {
          fusion_config->secure_fusion = false;
     } else
     if (strcmp( name, "defer-destructors" ) == 0) {
          fusion_config->defer_destructors = true;
     } else
     if (strcmp( name, "no-defer-destructors" ) == 0) {
          fusion_config->defer_destructors = false;
     } else
     if (strcmp( name, "trace-ref" ) == 0) {
          if (value) {
               if (!strcmp( value, "all" )) {
                    fusion_config->trace_ref = -1;
               }
               else if (sscanf( value, "%x", (unsigned int *) &fusion_config->trace_ref ) != 1) {
                    D_ERROR( "Fusion/Config: '%s': Invalid value!\n", name );
                    return DR_INVARG;
               }
          }
          else {
               D_ERROR( "Fusion/Config: '%s': No ID specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp( name, "call-bin-max-num" ) == 0) {
          if (value) {
               unsigned int max;

               if (sscanf( value, "%u", &max ) < 1) {
                    D_ERROR( "Fusion/Config: '%s': Could not parse value!\n", name );
                    return DR_INVARG;
               }

               if (max < 1) {
                    D_ERROR( "Fusion/Config: '%s': Error in value '%s' (min 1)!\n", name, value );
                    return DR_INVARG;
               }

               if (max > 16384) {
                    D_ERROR( "Fusion/Config: '%s': Error in value '%s' (max 16384)!\n", name, value );
                    return DR_INVARG;
               }

               fusion_config->call_bin_max_num = max;
          }
          else {
               D_ERROR( "Fusion/Config: '%s': No value specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp( name, "call-bin-max-data" ) == 0) {
          if (value) {
               unsigned int max;

               if (sscanf( value, "%u", &max ) < 1) {
                    D_ERROR( "Fusion/Config: '%s': Could not parse value!\n", name );
                    return DR_INVARG;
               }

               if (max < 4096) {
                    D_ERROR( "Fusion/Config: '%s': Error in value '%s' (min 4096)!\n", name, value );
                    return DR_INVARG;
               }

               if (max > 16777216) {
                    D_ERROR( "Fusion/Config: '%s': Error in value '%s' (max 16777216)!\n", name, value );
                    return DR_INVARG;
               }

               fusion_config->call_bin_max_data = max;
          }
          else {
               D_ERROR( "Fusion/Config: '%s': No value specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp( name, "shutdown-info" ) == 0) {
          fusion_config->shutdown_info = true;
     } else
     if (strcmp( name, "no-shutdown-info" ) == 0) {
          fusion_config->shutdown_info = false;
     }
     else
          return DR_INVARG;

     D_DEBUG_AT( Fusion_Config, "Set %s '%s'\n", name, value ?: "" );

     return DR_OK;
}

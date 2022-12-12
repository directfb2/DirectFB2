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

#ifndef __FUSION__CONF_H__
#define __FUSION__CONF_H__

#include <fusion/types.h>

/**********************************************************************************************************************/

typedef struct {
     char         *tmpfs;
     gid_t         shmfile_gid;
     bool          force_slave;
     bool          fork_handler;
     bool          debugshm;
     bool          madv_remove;
     bool          madv_remove_force;
     bool          secure_fusion;
     bool          defer_destructors;
     int           trace_ref;
     unsigned int  call_bin_max_num;
     unsigned int  call_bin_max_data;
     bool          shutdown_info;
} FusionConfig;

/**********************************************************************************************************************/

extern FusionConfig FUSION_API *fusion_config;

extern const char   FUSION_API *fusion_config_usage;

/*
 * Set indiviual option.
 */
DirectResult        FUSION_API  fusion_config_set( const char *name,
                                                   const char *value );

/**********************************************************************************************************************/

void __Fusion_conf_init  ( void );
void __Fusion_conf_deinit( void );

#endif

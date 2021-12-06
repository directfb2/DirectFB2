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
#include <direct/system.h>
#include <direct/thread.h>
#include <fusion/conf.h>

/**********************************************************************************************************************/

typedef enum {
     COREDFB_CALL_DENY     = 0x00000000,
     COREDFB_CALL_DIRECT   = 0x00000001,
     COREDFB_CALL_INDIRECT = 0x00000002
} CoreDFBCallMode;

static __inline__ CoreDFBCallMode
CoreDFB_CallMode( CoreDFB *core )
{
#if FUSION_BUILD_MULTI
     if (dfb_config->call_nodirect) {
          if (dfb_core_is_master( core )) {
               DirectThread *self = direct_thread_self();

               if (self && fusion_dispatcher_tid( core->world ) == direct_thread_get_tid( self ))
                    return COREDFB_CALL_DIRECT;
          }

          return COREDFB_CALL_INDIRECT;
     }

     if (core->shutdown_tid && core->shutdown_tid != direct_gettid() &&
         direct_gettid() != fusion_dispatcher_tid(core->world)       &&
         !Core_GetCalling()) {
          while (core_dfb)
               direct_thread_sleep( 10000 );

          return COREDFB_CALL_DENY;
     }

     if (dfb_core_is_master( core ) || !fusion_config->secure_fusion)
          return COREDFB_CALL_DIRECT;

     return COREDFB_CALL_INDIRECT;
#else /* FUSION_BUILD_MULTI */
     if (dfb_config->call_nodirect) {
          DirectThread *self = direct_thread_self();

          if (self && fusion_dispatcher_tid( core->world ) == direct_thread_get_tid( self ))
               return COREDFB_CALL_DIRECT;

          return COREDFB_CALL_INDIRECT;
     }

     return COREDFB_CALL_DIRECT;
#endif /* FUSION_BUILD_MULTI */
}

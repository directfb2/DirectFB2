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

#include <core/CoreSlave.h>
#include <core/core.h>
#include <direct/memcpy.h>

D_DEBUG_DOMAIN( DirectFB_CoreSlave, "DirectFB/CoreSlave", "DirectFB Core Slave" );

/**********************************************************************************************************************/

DFBResult
ICoreSlave_Real__GetData( CoreDFB *obj,
                          void    *address,
                          u32      bytes,
                          u8      *ret_data )
{
     DFBResult ret;

     D_DEBUG_AT( DirectFB_CoreSlave, "%s( %p, address %p, bytes %u ) from %lu\n", __FUNCTION__,
                 obj, address, bytes, Core_GetIdentity() );

     ret = dfb_core_memory_permissions_check( core_dfb, CMPF_READ, address, bytes );
     if (ret)
          return ret;

     direct_memcpy( ret_data, address, bytes );

     return DFB_OK;
}

DFBResult
ICoreSlave_Real__PutData( CoreDFB  *obj,
                          void     *address,
                          u32       bytes,
                          const u8 *data )
{
     DFBResult ret;

     D_DEBUG_AT( DirectFB_CoreSlave, "%s( %p, address %p, bytes %u ) from %lu\n", __FUNCTION__,
                 obj, address, bytes, Core_GetIdentity() );

     ret = dfb_core_memory_permissions_check( core_dfb, CMPF_WRITE, address, bytes );
     if (ret)
          return ret;

     direct_memcpy( address, data, bytes );

     return DFB_OK;
}

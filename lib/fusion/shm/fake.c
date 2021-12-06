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

#include <direct/mem.h>
#include <fusion/fusion_internal.h>
#include <fusion/shm/pool.h>

D_DEBUG_DOMAIN( Fusion_FakeSHMPool, "Fusion/FakeSHMPool", "Fusion Fake Shared Memory Pool" );

/**********************************************************************************************************************/

DirectResult
fusion_shm_pool_create( FusionWorld          *world,
                        const char           *name,
                        unsigned int          max_size,
                        bool                  debug,
                        FusionSHMPoolShared **ret_pool )
{
     FusionSHMPoolShared *pool;

     D_DEBUG_AT( Fusion_FakeSHMPool, "%s( %p [%d], '%s', %u, %p, %sdebug )\n", __FUNCTION__,
                 world, world->shared->world_index, name, max_size, ret_pool, debug ? "" : "no-" );

     pool = D_CALLOC( 1, sizeof(FusionSHMPoolShared) );
     if (!pool)
          return D_OOM();

     pool->debug = debug;

     D_MAGIC_SET( pool, FusionSHMPoolShared );

     *ret_pool = pool;

     D_DEBUG_AT( Fusion_FakeSHMPool, "  -> %p\n", *ret_pool );

     return DR_OK;
}

DirectResult
fusion_shm_pool_destroy( FusionWorld         *world,
                         FusionSHMPoolShared *pool )
{
     D_DEBUG_AT( Fusion_FakeSHMPool, "%s( %p, %p )\n", __FUNCTION__, world, pool );

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );

     D_MAGIC_CLEAR( pool );

     D_FREE( pool );

     return DR_OK;
}

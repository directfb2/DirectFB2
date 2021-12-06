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

#ifndef __FUSION__SHM__POOL_H__
#define __FUSION__SHM__POOL_H__

#include <fusion/types.h>

/**********************************************************************************************************************/

DirectResult fusion_shm_pool_create    ( FusionWorld          *world,
                                         const char           *name,
                                         unsigned int          max_size,
                                         bool                  debug,
                                         FusionSHMPoolShared **ret_pool );

DirectResult fusion_shm_pool_destroy   ( FusionWorld          *world,
                                         FusionSHMPoolShared  *pool );

DirectResult fusion_shm_pool_attach    ( FusionSHM            *shm,
                                         FusionSHMPoolShared  *pool );

DirectResult fusion_shm_pool_detach    ( FusionSHM            *shm,
                                         FusionSHMPoolShared  *pool );

DirectResult fusion_shm_pool_allocate  ( FusionSHMPoolShared  *pool,
                                         int                   size,
                                         bool                  clear,
                                         bool                  lock,
                                         void                **ret_data );

DirectResult fusion_shm_pool_reallocate( FusionSHMPoolShared  *pool,
                                         void                 *data,
                                         int                   size,
                                         bool                  lock,
                                         void                **ret_data );

DirectResult fusion_shm_pool_deallocate( FusionSHMPoolShared  *pool,
                                         void                 *data,
                                         bool                  lock );

#endif

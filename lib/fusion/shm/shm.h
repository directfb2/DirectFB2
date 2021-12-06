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

#ifndef __FUSION__SHM__SHM_H__
#define __FUSION__SHM__SHM_H__

#include <fusion/types.h>

/**********************************************************************************************************************/

typedef DirectEnumerationResult (*FusionSHMPoolCallback)( FusionSHMPool *pool, void *ctx );

/**********************************************************************************************************************/

int          fusion_find_tmpfs    ( char                  *name,
                                    int                    len );

DirectResult fusion_shm_init      ( FusionWorld           *world );

DirectResult fusion_shm_deinit    ( FusionWorld           *world );

DirectResult fusion_shm_enum_pools( FusionWorld           *world,
                                    FusionSHMPoolCallback  callback,
                                    void                  *ctx );

#endif

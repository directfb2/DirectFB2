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

#ifndef __FUSION__ARENA_H__
#define __FUSION__ARENA_H__

#include <fusion/types.h>

/**********************************************************************************************************************/

typedef int (*ArenaEnterFunc)( FusionArena *arena, void *ctx );
typedef int (*ArenaExitFunc) ( FusionArena *arena, void *ctx, bool emergency );

/**********************************************************************************************************************/

DirectResult FUSION_API fusion_arena_enter           ( FusionWorld     *world,
                                                       const char      *name,
                                                       ArenaEnterFunc   initialize,
                                                       ArenaEnterFunc   join,
                                                       void            *ctx,
                                                       FusionArena    **ret_arena,
                                                       int             *ret_error );

DirectResult FUSION_API fusion_arena_add_shared_field( FusionArena     *arena,
                                                       const char      *name,
                                                       void            *data );

DirectResult FUSION_API fusion_arena_get_shared_field( FusionArena     *arena,
                                                       const char      *name,
                                                       void           **data );

DirectResult FUSION_API fusion_arena_exit            ( FusionArena     *arena,
                                                       ArenaExitFunc    shutdown,
                                                       ArenaExitFunc    leave,
                                                       void            *ctx,
                                                       bool             emergency,
                                                       int             *ret_error );

#endif

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

#ifndef __CORE__GRAPHICS_STATE_H__
#define __CORE__GRAPHICS_STATE_H__

#include <core/state.h>

/**********************************************************************************************************************/

struct __DFB_CoreGraphicsState {
     FusionObject object;
     int          magic;

     FusionCall   call;

     CardState    state;
};

/**********************************************************************************************************************/

typedef enum {
     CGSNF_NONE = 0x00000000,
} CoreGraphicsStateNotificationFlags;

typedef struct {
     CoreGraphicsStateNotificationFlags flags;
} CoreGraphicsStateNotification;

/**********************************************************************************************************************/

/*
 * Creates a pool of graphics state objects.
 */
FusionObjectPool *dfb_graphics_state_pool_create( const FusionWorld  *world );

/*
 * Generates dfb_graphics_state_ref(), dfb_graphics_state_attach() etc.
 */
FUSION_OBJECT_METHODS( CoreGraphicsState, dfb_graphics_state )

/**********************************************************************************************************************/

DFBResult         dfb_graphics_state_create     ( CoreDFB            *core,
                                                  CoreGraphicsState **ret_state );

#endif

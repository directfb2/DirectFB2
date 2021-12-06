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

#include <core/CoreLayer.h>
#include <core/layer_control.h>

D_DEBUG_DOMAIN( DirectFB_CoreLayer, "DirectFB/CoreLayer", "DirectFB CoreLayer" );

/**********************************************************************************************************************/

DFBResult
ILayer_Real__CreateContext( CoreLayer         *obj,
                            CoreLayerContext **ret_context )
{
     D_DEBUG_AT( DirectFB_CoreLayer, "%s( %p )\n", __FUNCTION__, obj );

     D_ASSERT( ret_context != NULL );

     return dfb_layer_create_context( obj, false, ret_context );
}

DFBResult
ILayer_Real__GetPrimaryContext( CoreLayer         *obj,
                                DFBBoolean         activate,
                                CoreLayerContext **ret_context )
{
     D_DEBUG_AT( DirectFB_CoreLayer, "%s( %p )\n", __FUNCTION__, obj );

     D_ASSERT( ret_context != NULL );

     return dfb_layer_get_primary_context( obj, activate, ret_context );
}

DFBResult
ILayer_Real__ActivateContext( CoreLayer        *obj,
                              CoreLayerContext *context )
{
     D_DEBUG_AT( DirectFB_CoreLayer, "%s( %p )\n", __FUNCTION__, obj );

     D_ASSERT( context != NULL );

     return dfb_layer_activate_context( obj, context );
}

DFBResult
ILayer_Real__GetCurrentOutputField( CoreLayer *obj,
                                    s32       *ret_field )
{
     D_DEBUG_AT( DirectFB_CoreLayer, "%s( %p )\n", __FUNCTION__, obj );

     D_ASSERT( ret_field != NULL );

     return dfb_layer_get_current_output_field( obj, ret_field );
}

DFBResult
ILayer_Real__SetLevel( CoreLayer *obj,
                       s32        level )
{
     D_DEBUG_AT( DirectFB_CoreLayer, "%s( %p )\n", __FUNCTION__, obj );

     return dfb_layer_set_level( obj, level );
}

DFBResult
ILayer_Real__WaitVSync( CoreLayer *obj )
{
     D_DEBUG_AT( DirectFB_CoreLayer, "%s( %p )\n", __FUNCTION__, obj );

     return dfb_layer_wait_vsync( obj );
}

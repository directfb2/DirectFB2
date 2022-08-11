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

#include <core/CoreWindowStack.h>
#include <core/layer_context.h>
#include <core/windowstack.h>

D_DEBUG_DOMAIN( DirectFB_CoreWindowStack, "DirectFB/CoreWindowStack", "DirectFB CoreWindowStack" );

/**********************************************************************************************************************/

DFBResult
IWindowStack_Real__RepaintAll( CoreWindowStack *obj )
{
     D_DEBUG_AT( DirectFB_CoreWindowStack, "%s( %p )\n", __FUNCTION__, obj );

     return dfb_windowstack_repaint_all( obj );
}

DFBResult
IWindowStack_Real__BackgroundSetMode( CoreWindowStack               *obj,
                                      DFBDisplayLayerBackgroundMode  mode )
{
     D_DEBUG_AT( DirectFB_CoreWindowStack, "%s( %p )\n", __FUNCTION__, obj );

     return dfb_windowstack_set_background_mode( obj, mode );
}

DFBResult
IWindowStack_Real__BackgroundSetImage( CoreWindowStack *obj,
                                       CoreSurface     *image )
{
     D_DEBUG_AT( DirectFB_CoreWindowStack, "%s( %p )\n", __FUNCTION__, obj );

     return dfb_windowstack_set_background_image( obj, image );
}

DFBResult
IWindowStack_Real__BackgroundSetColor( CoreWindowStack *obj,
                                       const DFBColor  *color )
{
     D_DEBUG_AT( DirectFB_CoreWindowStack, "%s( %p )\n", __FUNCTION__, obj );

     return dfb_windowstack_set_background_color( obj, color );
}

DFBResult
IWindowStack_Real__BackgroundSetColorIndex( CoreWindowStack *obj,
                                            s32              index )
{
     D_DEBUG_AT( DirectFB_CoreWindowStack, "%s( %p )\n", __FUNCTION__, obj );

     return dfb_windowstack_set_background_color_index( obj, index );
}

DFBResult
IWindowStack_Real__CursorEnable( CoreWindowStack *obj,
                                 bool             enable )
{
     D_DEBUG_AT( DirectFB_CoreWindowStack, "%s( %p )\n", __FUNCTION__, obj );

     return dfb_windowstack_cursor_enable( obj, enable );
}

DFBResult
IWindowStack_Real__CursorGetPosition( CoreWindowStack *obj,
                                      DFBPoint        *ret_position )
{
     D_DEBUG_AT( DirectFB_CoreWindowStack, "%s( %p )\n", __FUNCTION__, obj );

     return dfb_windowstack_get_cursor_position( obj, &ret_position->x, &ret_position->y );
}

DFBResult
IWindowStack_Real__CursorWarp( CoreWindowStack *obj,
                               const DFBPoint  *position )
{
     D_DEBUG_AT( DirectFB_CoreWindowStack, "%s( %p )\n", __FUNCTION__, obj );

     return dfb_windowstack_cursor_warp( obj, position->x, position->y );
}

DFBResult
IWindowStack_Real__CursorSetAcceleration( CoreWindowStack *obj,
                                          u32              numerator,
                                          u32              denominator,
                                          u32              threshold )
{
     D_DEBUG_AT( DirectFB_CoreWindowStack, "%s( %p )\n", __FUNCTION__, obj );

     return dfb_windowstack_cursor_set_acceleration( obj, numerator, denominator, threshold );
}
DFBResult
IWindowStack_Real__CursorSetShape( CoreWindowStack *obj,
                                   CoreSurface     *shape,
                                   const DFBPoint  *hotspot )
{
     D_DEBUG_AT( DirectFB_CoreWindowStack, "%s( %p )\n", __FUNCTION__, obj );

     dfb_layer_context_set_cursor_shape( obj->context, shape, hotspot->x, hotspot->y );

     return dfb_windowstack_cursor_set_shape( obj, shape, hotspot->x, hotspot->y );
}

DFBResult
IWindowStack_Real__CursorSetOpacity( CoreWindowStack *obj,
                                     u8               opacity )
{
     D_DEBUG_AT( DirectFB_CoreWindowStack, "%s( %p )\n", __FUNCTION__, obj );

     return dfb_windowstack_cursor_set_opacity( obj, opacity );
}

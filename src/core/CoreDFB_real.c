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

#include <core/CoreDFB.h>
#include <core/core.h>
#include <core/clipboard.h>
#include <core/graphics_state.h>
#include <core/palette.h>
#include <direct/memcpy.h>
#include <fusion/conf.h>

D_DEBUG_DOMAIN( DirectFB_CoreDFB, "DirectFB/Core", "DirectFB Core" );

/**********************************************************************************************************************/

DFBResult
ICore_Real__Initialize( CoreDFB *obj )
{
     D_DEBUG_AT( DirectFB_CoreDFB, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreDFB );

     if (Core_GetIdentity() != FUSION_ID_MASTER)
          return DFB_ACCESSDENIED;

     return dfb_core_initialize( core_dfb );
}

DFBResult
ICore_Real__Register( CoreDFB *obj,
                      u32      slave_call )
{
     D_DEBUG_AT( DirectFB_CoreDFB, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreDFB );

     return Core_Resource_AddIdentity( Core_GetIdentity(), slave_call );
}

DFBResult
ICore_Real__CreateSurface( CoreDFB                  *obj,
                           const CoreSurfaceConfig  *config,
                           CoreSurfaceTypeFlags      type,
                           u64                       resource_id,
                           CorePalette              *palette,
                           CoreSurface             **ret_surface )
{
     DFBResult    ret;
     CoreSurface *surface;

     D_DEBUG_AT( DirectFB_CoreDFB, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreDFB );
     D_ASSERT( config != NULL );
     D_ASSERT( ret_surface != NULL );

     ret = Core_Resource_CheckSurface( config, resource_id );
     if (ret)
          return ret;

     ret = dfb_surface_create( obj, config, type, resource_id, palette, &surface );
     if (ret)
          return ret;

     Core_Resource_AddSurface( surface );

     *ret_surface = surface;

     return DFB_OK;
}

DFBResult
ICore_Real__CreatePalette( CoreDFB      *obj,
                           u32           size,
                           CorePalette **ret_palette )
{
     D_DEBUG_AT( DirectFB_CoreDFB, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreDFB );
     D_ASSERT( ret_palette != NULL );

     return dfb_palette_create( obj, size, ret_palette );
}

DFBResult
ICore_Real__ClipboardSet( CoreDFB    *obj,
                          const char *mime_type,
                          u32         mime_type_size,
                          const char *data,
                          u32         data_size,
                          u64         timestamp_us )
{
     struct timeval tv;

     D_DEBUG_AT( DirectFB_CoreDFB, "%s( %p )\n", __FUNCTION__, obj );

     tv.tv_sec  = timestamp_us / 1000000;
     tv.tv_usec = timestamp_us % 1000000;

     return dfb_clipboard_set( dfb_core_get_part( core_dfb, DFCP_CLIPBOARD ), mime_type, data, data_size, &tv );
}

DFBResult
ICore_Real__ClipboardGet( CoreDFB *obj,
                          char    *ret_mime_type,
                          u32     *ret_mime_type_size,
                          char    *ret_data,
                          u32     *ret_data_size )
{
     DFBResult     ret;
     char         *mime_type;
     void         *data;
     unsigned int  data_size;

     D_DEBUG_AT( DirectFB_CoreDFB, "%s( %p )\n", __FUNCTION__, obj );

     ret = dfb_clipboard_get( dfb_core_get_part( core_dfb, DFCP_CLIPBOARD ), &mime_type, &data, &data_size );
     if (ret)
          return ret;

     direct_memcpy( ret_mime_type, mime_type, strlen( mime_type ) + 1 );
     *ret_mime_type_size = strlen( mime_type ) + 1;

     direct_memcpy( ret_data, data, data_size );
     *ret_data_size = data_size;

     D_FREE( data );
     D_FREE( mime_type );

     return DFB_OK;
}

DFBResult
ICore_Real__ClipboardGetTimestamp( CoreDFB *obj,
                                   u64     *ret_timestamp_us )
{
     DFBResult      ret;
     struct timeval tv;

     D_DEBUG_AT( DirectFB_CoreDFB, "%s( %p )\n", __FUNCTION__, obj );

     ret = dfb_clipboard_get_timestamp( dfb_core_get_part( core_dfb, DFCP_CLIPBOARD ), &tv );
     if (ret)
          return ret;

     *ret_timestamp_us = tv.tv_sec * 1000000 + tv.tv_usec;

     return DFB_OK;
}

DFBResult
ICore_Real__WaitIdle( CoreDFB *obj )
{
     D_DEBUG_AT( DirectFB_CoreDFB, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreDFB );

     return dfb_gfxcard_sync();
}

DFBResult
ICore_Real__GetSurface( CoreDFB      *obj,
                        u32           surface_id,
                        CoreSurface **ret_surface )
{
     DFBResult    ret;
     CoreSurface *surface;
     char         path[1000];
     size_t       path_length;

     D_DEBUG_AT( DirectFB_CoreDFB, "%s( %p, %u )\n", __FUNCTION__, obj, surface_id );

     D_MAGIC_ASSERT( obj, CoreDFB );

     if (fusion_config->secure_fusion && dfb_config->ownership_check && !dfb_core_is_master( core_dfb )) {
          ret = fusion_get_fusionee_path( dfb_core_world( core_dfb ), Core_GetIdentity(), path, sizeof(path),
                                          &path_length );
          if (ret)
               return ret;

          D_DEBUG_AT( DirectFB_CoreDFB, "  -> '%s'\n", path );
     }

     ret = dfb_core_get_surface( core_dfb, surface_id, &surface );
     if (ret) {
          D_DEBUG_AT( DirectFB_CoreDFB, "  -> dfb_core_get_surface() failed!\n" );
          return ret;
     }

     if (fusion_config->secure_fusion && dfb_config->ownership_check && !dfb_core_is_master( core_dfb )) {
          ret = fusion_object_has_access( &surface->object, path );
          if (ret) {
               D_DEBUG_AT( DirectFB_CoreDFB, "  -> no access!\n" );
               dfb_surface_unref( surface );
               return ret;
          }

          fusion_object_add_owner( &surface->object, Core_GetIdentity() );
     }

     D_DEBUG_AT( DirectFB_CoreDFB, "  -> surface %p\n", surface );

     *ret_surface = surface;

     return DFB_OK;
}

DFBResult
ICore_Real__AllowSurface( CoreDFB     *obj,
                          CoreSurface *surface,
                          const char  *executable,
                          u32          executable_length )
{
     D_DEBUG_AT( DirectFB_CoreDFB, "%s( %p, %p, '%s' )\n", __FUNCTION__, obj, surface, executable );

     D_MAGIC_ASSERT( obj, CoreDFB );
     D_ASSERT( surface != NULL );
     D_ASSERT( executable != NULL );

     return fusion_object_add_access( &surface->object, executable );
}

DFBResult
ICore_Real__CreateState( CoreDFB            *obj,
                         CoreGraphicsState **ret_state )
{
     D_DEBUG_AT( DirectFB_CoreDFB, "%s( %p )\n", __FUNCTION__, obj );

     D_MAGIC_ASSERT( obj, CoreDFB );
     D_ASSERT( ret_state != NULL );

     return dfb_graphics_state_create( core_dfb, ret_state );
}

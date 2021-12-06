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

#include <core/clipboard.h>
#include <core/core.h>
#include <core/core_parts.h>
#include <direct/memcpy.h>
#include <fusion/conf.h>
#include <fusion/shmalloc.h>

D_DEBUG_DOMAIN( Core_Clipboard, "Core/Clipboard", "DirectFB Core Clipboard" );

/**********************************************************************************************************************/

DFB_CORE_PART( clipboard_core, ClipboardCore );

/**********************************************************************************************************************/

static DFBResult
dfb_clipboard_core_initialize( CoreDFB                *core,
                               DFBClipboardCore       *data,
                               DFBClipboardCoreShared *shared )
{
     D_DEBUG_AT( Core_Clipboard, "%s( %p, %p, %p )\n", __FUNCTION__, core, data, shared );

     D_ASSERT( data != NULL );
     D_ASSERT( shared != NULL );

     data->core   = core;
     data->shared = shared;

     shared->shmpool = dfb_core_shmpool( core );

     fusion_skirmish_init2( &shared->lock, "Clipboard Core", dfb_core_world( core ), fusion_config->secure_fusion );

     D_MAGIC_SET( data, DFBClipboardCore );
     D_MAGIC_SET( shared, DFBClipboardCoreShared );

     return DFB_OK;
}

static DFBResult
dfb_clipboard_core_join( CoreDFB                *core,
                         DFBClipboardCore       *data,
                         DFBClipboardCoreShared *shared )
{
     D_DEBUG_AT( Core_Clipboard, "%s( %p, %p, %p )\n", __FUNCTION__, core, data, shared );

     D_ASSERT( data != NULL );
     D_MAGIC_ASSERT( shared, DFBClipboardCoreShared );

     data->core   = core;
     data->shared = shared;

     D_MAGIC_SET( data, DFBClipboardCore );

     return DFB_OK;
}

static DFBResult
dfb_clipboard_core_shutdown( DFBClipboardCore *data,
                             bool              emergency )
{
     DFBClipboardCoreShared *shared;

     D_DEBUG_AT( Core_Clipboard, "%s( %p, %semergency )\n", __FUNCTION__, data, emergency ? "" : "no " );

     D_MAGIC_ASSERT( data, DFBClipboardCore );
     D_MAGIC_ASSERT( data->shared, DFBClipboardCoreShared );

     shared = data->shared;

     fusion_skirmish_destroy( &shared->lock );

     if (shared->data)
          SHFREE( shared->shmpool, shared->data );

     if (shared->mime_type)
          SHFREE( shared->shmpool, shared->mime_type );

     D_MAGIC_CLEAR( data );
     D_MAGIC_CLEAR( shared );

     return DFB_OK;
}

static DFBResult
dfb_clipboard_core_leave( DFBClipboardCore *data,
                          bool              emergency )
{
     D_DEBUG_AT( Core_Clipboard, "%s( %p, %semergency )\n", __FUNCTION__, data, emergency ? "" : "no " );

     D_MAGIC_ASSERT( data, DFBClipboardCore );
     D_MAGIC_ASSERT( data->shared, DFBClipboardCoreShared );

     D_MAGIC_CLEAR( data );

     return DFB_OK;
}

static DFBResult
dfb_clipboard_core_suspend( DFBClipboardCore *data )
{
     D_DEBUG_AT( Core_Clipboard, "%s( %p )\n", __FUNCTION__, data );

     D_MAGIC_ASSERT( data, DFBClipboardCore );
     D_MAGIC_ASSERT( data->shared, DFBClipboardCoreShared );

     return DFB_OK;
}

static DFBResult
dfb_clipboard_core_resume( DFBClipboardCore *data )
{
     D_DEBUG_AT( Core_Clipboard, "%s( %p )\n", __FUNCTION__, data );

     D_MAGIC_ASSERT( data, DFBClipboardCore );
     D_MAGIC_ASSERT( data->shared, DFBClipboardCoreShared );

     return DFB_OK;
}

/**********************************************************************************************************************/

DFBResult
dfb_clipboard_set( DFBClipboardCore *core,
                   const char       *mime_type,
                   const void       *data,
                   unsigned int      size,
                   struct timeval   *timestamp )
{
     DFBClipboardCoreShared *shared;

     char *new_mime;
     void *new_data;

     D_MAGIC_ASSERT( core, DFBClipboardCore );
     D_MAGIC_ASSERT( core->shared, DFBClipboardCoreShared );
     D_ASSERT( mime_type != NULL );
     D_ASSERT( data != NULL );
     D_ASSERT( size > 0 );

     shared = core->shared;

     new_mime = SHSTRDUP( shared->shmpool, mime_type );
     if (!new_mime)
          return D_OOSHM();

     new_data = SHMALLOC( shared->shmpool, size );
     if (!new_data) {
          SHFREE( shared->shmpool, new_mime );
          return D_OOSHM();
     }

     direct_memcpy( new_data, data, size );

     if (fusion_skirmish_prevail( &shared->lock )) {
          SHFREE( shared->shmpool, new_data );
          SHFREE( shared->shmpool, new_mime );
          return DFB_FUSION;
     }

     if (shared->data)
          SHFREE( shared->shmpool, shared->data );

     if (shared->mime_type)
          SHFREE( shared->shmpool, shared->mime_type );

     shared->mime_type = new_mime;
     shared->data      = new_data;
     shared->size      = size;

     shared->timestamp = *timestamp;

     fusion_skirmish_dismiss( &shared->lock );

     return DFB_OK;
}

DFBResult
dfb_clipboard_get( DFBClipboardCore  *core,
                   char             **mime_type,
                   void             **data,
                   unsigned int      *size )
{
     DFBClipboardCoreShared *shared;

     D_MAGIC_ASSERT( core, DFBClipboardCore );
     D_MAGIC_ASSERT( core->shared, DFBClipboardCoreShared );

     shared = core->shared;

     if (fusion_skirmish_prevail( &shared->lock ))
          return DFB_FUSION;

     if (!shared->mime_type || !shared->data) {
          fusion_skirmish_dismiss( &shared->lock );
          return DFB_BUFFEREMPTY;
     }

     if (mime_type)
          *mime_type = D_STRDUP( shared->mime_type );

     if (data) {
          *data = D_MALLOC( shared->size );
          direct_memcpy( *data, shared->data, shared->size );
     }

     if (size)
          *size = shared->size;

     fusion_skirmish_dismiss( &shared->lock );

     return DFB_OK;
}

DFBResult
dfb_clipboard_get_timestamp( DFBClipboardCore *core,
                             struct timeval   *timestamp )
{
     DFBClipboardCoreShared *shared;

     D_MAGIC_ASSERT( core, DFBClipboardCore );
     D_MAGIC_ASSERT( core->shared, DFBClipboardCoreShared );
     D_ASSERT( timestamp != NULL );

     shared = core->shared;

     if (fusion_skirmish_prevail( &shared->lock ))
          return DFB_FUSION;

     *timestamp = shared->timestamp;

     fusion_skirmish_dismiss( &shared->lock );

     return DFB_OK;
}

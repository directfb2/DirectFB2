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

#include <core/colorhash.h>
#include <core/core_parts.h>
#include <core/palette.h>
#include <gfx/convert.h>

D_DEBUG_DOMAIN( Core_ColorHash, "Core/ColorHash", "DirectFB Core ColorHash" );

/**********************************************************************************************************************/

DFB_CORE_PART( colorhash_core, ColorHashCore );

/**********************************************************************************************************************/

#define HASH_SIZE 823

static DFBColorHashCore *colorhash;

static DFBResult
dfb_colorhash_core_initialize( CoreDFB                *core,
                               DFBColorHashCore       *data,
                               DFBColorHashCoreShared *shared )
{
     D_DEBUG_AT( Core_ColorHash, "%s( %p, %p, %p )\n", __FUNCTION__, core, data, shared );

     D_ASSERT( data != NULL );
     D_ASSERT( shared != NULL );

     colorhash = data;

     data->core   = core;
     data->shared = shared;

     data->hash = D_CALLOC( HASH_SIZE, sizeof(Colorhash) );
     if (!data->hash)
          return D_OOM();

     direct_mutex_init( &data->hash_lock );

     D_MAGIC_SET( data, DFBColorHashCore );
     D_MAGIC_SET( shared, DFBColorHashCoreShared );

     return DFB_OK;
}

static DFBResult
dfb_colorhash_core_join( CoreDFB                *core,
                         DFBColorHashCore       *data,
                         DFBColorHashCoreShared *shared )
{
     D_DEBUG_AT( Core_ColorHash, "%s( %p, %p, %p )\n", __FUNCTION__, core, data, shared );

     D_ASSERT( data != NULL );
     D_MAGIC_ASSERT( shared, DFBColorHashCoreShared );

     colorhash = data;

     data->core   = core;
     data->shared = shared;

     data->hash = D_CALLOC( HASH_SIZE, sizeof(Colorhash) );
     if (!data->hash)
          return D_OOM();

     direct_mutex_init( &data->hash_lock );

     D_MAGIC_SET( data, DFBColorHashCore );

     return DFB_OK;
}

static DFBResult
dfb_colorhash_core_shutdown( DFBColorHashCore *data,
                             bool              emergency )
{
     D_DEBUG_AT( Core_ColorHash, "%s( %p, %semergency )\n", __FUNCTION__, data, emergency ? "" : "no " );

     D_MAGIC_ASSERT( data, DFBColorHashCore );
     D_MAGIC_ASSERT( data->shared, DFBColorHashCoreShared );

     direct_mutex_deinit( &data->hash_lock );

     D_FREE( data->hash );

     D_MAGIC_CLEAR( data );

     return DFB_OK;
}

static DFBResult
dfb_colorhash_core_leave( DFBColorHashCore *data,
                          bool              emergency )
{
     D_DEBUG_AT( Core_ColorHash, "%s( %p, %semergency )\n", __FUNCTION__, data, emergency ? "" : "no " );

     D_MAGIC_ASSERT( data, DFBColorHashCore );
     D_MAGIC_ASSERT( data->shared, DFBColorHashCoreShared );

     direct_mutex_deinit( &data->hash_lock );

     D_FREE( data->hash );

     D_MAGIC_CLEAR( data );

     return DFB_OK;
}

static DFBResult
dfb_colorhash_core_suspend( DFBColorHashCore *data )
{
     D_DEBUG_AT( Core_ColorHash, "%s( %p )\n", __FUNCTION__, data );

     D_MAGIC_ASSERT( data, DFBColorHashCore );
     D_MAGIC_ASSERT( data->shared, DFBColorHashCoreShared );

     return DFB_OK;
}

static DFBResult
dfb_colorhash_core_resume( DFBColorHashCore *data )
{
     D_DEBUG_AT( Core_ColorHash, "%s( %p )\n", __FUNCTION__, data );

     D_MAGIC_ASSERT( data, DFBColorHashCore );
     D_MAGIC_ASSERT( data->shared, DFBColorHashCoreShared );

     return DFB_OK;
}

/**********************************************************************************************************************/

unsigned int
dfb_colorhash_lookup( DFBColorHashCore *core,
                      CorePalette      *palette,
                      u8                r,
                      u8                g,
                      u8                b,
                      u8                a )
{
     unsigned int pixel = PIXEL_ARGB( a, r, g, b );
     unsigned int index = (pixel ^ (unsigned long) palette) % HASH_SIZE;

     if (core) {
          D_MAGIC_ASSERT( core, DFBColorHashCore );
          D_MAGIC_ASSERT( core->shared, DFBColorHashCoreShared );
     }
     else
          core = colorhash;

     D_ASSERT( core->hash != NULL );

     direct_mutex_lock( &core->hash_lock );

     /* Try a lookup in the hash table. */
     if (core->hash[index].palette_id == palette->object.id && core->hash[index].pixel == pixel) {
          /* Set the return value. */
          index = core->hash[index].index;
     }
     /* Look for the closest match. */
     else {
          DFBColor     *entries = palette->entries;
          int           min_diff = 0;
          unsigned int  i, min_index = 0;

          for (i = 0; i < palette->num_entries; i++) {
               int diff;

               int r_diff = (int) entries[i].r - (int) r;
               int g_diff = (int) entries[i].g - (int) g;
               int b_diff = (int) entries[i].b - (int) b;
               int a_diff = (int) entries[i].a - (int) a;

               if (a)
                    diff = (r_diff * r_diff + g_diff * g_diff +
                            b_diff * b_diff + ((a_diff * a_diff) >> 6));
               else
                    diff = (r_diff + g_diff + b_diff + (a_diff * a_diff));

               if (i == 0 || diff < min_diff) {
                    min_diff = diff;
                    min_index = i;
               }

               if (!diff)
                    break;
          }

          /* Store the matching entry in the hash table. */
          core->hash[index].pixel      = pixel;
          core->hash[index].index      = min_index;
          core->hash[index].palette_id = palette->object.id;

          /* Set the return value. */
          index = min_index;
     }

     direct_mutex_unlock( &core->hash_lock );

     return index;
}

void
dfb_colorhash_invalidate( DFBColorHashCore *core,
                          CorePalette      *palette )
{
     unsigned int index = HASH_SIZE - 1;

     if (core) {
          D_MAGIC_ASSERT( core, DFBColorHashCore );
          D_MAGIC_ASSERT( core->shared, DFBColorHashCoreShared );
     }
     else
          core = colorhash;

     D_ASSERT( core->hash != NULL );

     direct_mutex_lock( &core->hash_lock );

     /* Invalidate all entries owned by this palette. */
     do {
          if (core->hash[index].palette_id == palette->object.id)
               core->hash[index].palette_id = 0;
     } while (index--);

     direct_mutex_unlock( &core->hash_lock );
}

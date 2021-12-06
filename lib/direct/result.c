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

#include <direct/debug.h>
#include <direct/hash.h>
#include <direct/mutex.h>
#include <direct/result.h>

D_DEBUG_DOMAIN( Direct_Result, "Direct/Result", "Direct Result types" );

/**********************************************************************************************************************/

static DirectHash  result_types = DIRECT_HASH_INIT( 7, false );
static DirectMutex result_mutex;

/**********************************************************************************************************************/

void
__D_result_init()
{
     direct_mutex_init( &result_mutex );
}

void
__D_result_deinit()
{
     direct_mutex_deinit( &result_mutex );
}

/**********************************************************************************************************************/

DirectResult
DirectResultTypeRegister( DirectResultType *type )
{
     DirectResult ret;

     D_DEBUG_AT( Direct_Result, "%s( %p )\n", __FUNCTION__, type );

     D_ASSERT( type != NULL );

     D_DEBUG_AT( Direct_Result, "  -> refs    %d\n",     type->refs );
     D_DEBUG_AT( Direct_Result, "  -> base    0x%08x\n", type->base );
     D_DEBUG_AT( Direct_Result, "  -> strings %p\n",     type->result_strings );
     D_DEBUG_AT( Direct_Result, "  -> count   %u\n",     type->result_count );

     D_ASSERT( type->result_count > 0 );
     D_ASSERT( type->result_count <= D_RESULT_TYPE_SPACE );

     D_DEBUG_AT( Direct_Result, "  => %s\n", type->result_strings[0] );

     ret = direct_mutex_lock( &result_mutex );
     if (ret)
          return ret;

     if (direct_hash_lookup( &result_types, type->base )) {
          D_ASSERT( direct_hash_lookup( &result_types, type->base ) == type );

          D_MAGIC_ASSERT( type, DirectResultType );

          D_ASSERT( type->refs > 0 );

          type->refs++;
     }
     else {
          D_ASSERT( type->refs == 0 );

          D_MAGIC_SET( type, DirectResultType );

          ret = direct_hash_insert( &result_types, type->base, type );
          if (ret)
               D_MAGIC_CLEAR( type );
          else
               type->refs = 1;
     }

     direct_mutex_unlock( &result_mutex );

     return ret;
}

DirectResult
DirectResultTypeUnregister( DirectResultType *type )
{
     DirectResult ret;

     D_DEBUG_AT( Direct_Result, "%s( %p )\n", __FUNCTION__, type );

     D_MAGIC_ASSERT( type, DirectResultType );

     D_DEBUG_AT( Direct_Result, "  -> refs    %d\n",     type->refs );
     D_DEBUG_AT( Direct_Result, "  -> base    0x%08x\n", type->base );
     D_DEBUG_AT( Direct_Result, "  -> strings %p\n",     type->result_strings );
     D_DEBUG_AT( Direct_Result, "  -> count   %u\n",     type->result_count );

     D_ASSERT( type->result_count > 0 );
     D_ASSERT( type->result_count <= D_RESULT_TYPE_SPACE );

     D_DEBUG_AT( Direct_Result, "  => %s\n", type->result_strings[0] );

     ret = direct_mutex_lock( &result_mutex );
     if (ret)
          return ret;

     D_ASSERT( type->refs > 0 );

     D_ASSERT( direct_hash_lookup( &result_types, type->base ) == type );

     if (!--type->refs) {
          D_MAGIC_CLEAR( type );

          ret = direct_hash_remove( &result_types, type->base );
     }

     direct_mutex_unlock( &result_mutex );

     return ret;
}

const char *
DirectResultString( DirectResult result )
{
     DirectResult      ret;
     DirectResultType *type;

     if (result) {
          ret = direct_mutex_lock( &result_mutex );
          if (ret)
               return NULL;

          type = direct_hash_lookup( &result_types, D_RESULT_TYPE( result ) );

          direct_mutex_unlock( &result_mutex );

          if (type) {
               unsigned int index = D_RESULT_INDEX( result );

               D_MAGIC_ASSERT( type, DirectResultType );

               D_ASSERT( type->refs > 0 );

               if (index < type->result_count)
                    return type->result_strings[index];

               return type->result_strings[0];
          }

          return "UNKNOWN RESULT TYPE";
     }

     return "OK";
}

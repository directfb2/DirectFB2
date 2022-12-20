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

#include <direct/mem.h>
#include <direct/util.h>
#include <fusion/hash.h>
#include <fusion/shmalloc.h>

D_DEBUG_DOMAIN( Fusion_Hash, "Fusion/Hash", "Fusion Hash table" );

/**********************************************************************************************************************/

#define HASH_STR(h,p)                   \
{                                       \
     h = *p;                            \
     if (h)                             \
          for (p += 1; *p != '\0'; p++) \
               h = (h << 5) - h + *p;   \
}

static const unsigned int primes[] =
{
     11,
     19,
     37,
     73,
     109,
     163,
     251,
     367,
     557,
     823,
     1237,
     1861,
     2777,
     4177,
     6247,
     9371,
     14057,
     21089,
     31627,
     47431,
     71143,
     106721,
     160073,
     240101,
     360163,
     540217,
     810343,
     1215497,
     1823231,
     2734867,
     4102283,
     6153409,
     9230113,
     13845163,
};

static const unsigned int nprimes = D_ARRAY_SIZE(primes);

static unsigned int
spaced_primes_closest( unsigned int num )
{
     unsigned int i;

     for (i = 0; i < nprimes; i++)
          if (primes[i] > num)
               return primes[i];

     return primes[nprimes-1];
}

static __inline__ FusionHashNode **
fusion_hash_lookup_node( FusionHash *hash,
                         const void *key )
{
     FusionHashNode **node;

     if (hash->key_type == HASH_STRING) {
          unsigned int  h;
          const char   *p = key;
          HASH_STR(h,p)
          node = &hash->nodes[h % hash->size];
     }
     else
          node = &hash->nodes[((unsigned long) key) % hash->size];

     if (hash->key_type == HASH_STRING) {
          while (*node && strcmp( (*node)->key, key ))
               node = &(*node)->next;
     }
     else
          while (*node && (*node)->key != key)
               node = &(*node)->next;

     return node;
}

/**********************************************************************************************************************/

static DirectResult
fusion_hash_create_internal( bool                  local,
                             FusionSHMPoolShared  *pool,
                             FusionHashType        key_type,
                             FusionHashType        value_type,
                             int                   size,
                             FusionHash          **ret_hash )
{
     FusionHash *hash;

     if (!ret_hash)
          return DR_BUG;
     if (!local && !pool)
          return DR_BUG;

     if (size < FUSION_HASH_MIN_SIZE)
          size = FUSION_HASH_MIN_SIZE;

     if (local)
          hash = D_CALLOC( 1, sizeof(FusionHash) );
     else
          hash = SHCALLOC( pool, 1, sizeof(FusionHash) );

     if (!hash)
          return local ? DR_NOLOCALMEMORY : DR_NOSHAREDMEMORY;

     hash->local      = local;
     hash->pool       = pool;
     hash->key_type   = key_type;
     hash->value_type = value_type;
     hash->size       = size;
     hash->nnodes     = 0;
     if (local)
          hash->nodes = D_CALLOC( size, sizeof(FusionHashNode*) );
     else
          hash->nodes = SHCALLOC( pool, size, sizeof(FusionHashNode*) );

     if (!hash->nodes) {
          if (local)
               D_FREE( hash );
          else
               SHFREE( pool, hash );
          return local ? DR_NOLOCALMEMORY : DR_NOSHAREDMEMORY;
     }

     D_MAGIC_SET( hash, FusionHash );

     *ret_hash = hash;

     return DR_OK;
}

DirectResult
fusion_hash_create_local( FusionHashType   key_type,
                          FusionHashType   value_type,
                          int              size,
                          FusionHash     **ret_hash )
{
     D_DEBUG_AT( Fusion_Hash, "Creating local hash table with initial capacity of %d...\n", size );

     return fusion_hash_create_internal( true, NULL, key_type, value_type, size, ret_hash );
}

DirectResult
fusion_hash_create( FusionSHMPoolShared  *pool,
                    FusionHashType        key_type,
                    FusionHashType        value_type,
                    int                   size,
                    FusionHash          **ret_hash )
{
     D_DEBUG_AT( Fusion_Hash, "Creating shared hash table with initial capacity of %d...\n", size );

     return fusion_hash_create_internal( false, pool, key_type, value_type, size, ret_hash );
}

static void
fusion_hash_node_destroy( FusionHash      *hash,
                          FusionHashNode  *node,
                          void           **old_key,
                          void           **old_value )
{
     if (!node)
          return;

     if (old_key)
          *old_key = node->key;
     else if (hash->key_type != HASH_INT) {
          if (hash->free_keys) {
               if (hash->local)
                    D_FREE( node->key );
               else
                    SHFREE( hash->pool, node->key );
          }
     }

     if (old_value)
          *old_value = node->value;
     else if (hash->value_type != HASH_INT) {
          if (hash->free_values) {
               if (hash->local)
                    D_FREE( node->value );
               else
                    SHFREE( hash->pool, node->value );
          }
     }

     if (hash->local)
          D_FREE( node );
     else
          SHFREE( hash->pool, node );
}

void
fusion_hash_destroy( FusionHash *hash )
{
     FusionHashNode *node;
     FusionHashNode *next;
     int             i;

     D_MAGIC_ASSERT( hash, FusionHash );

     for (i = 0; i < hash->size; i++) {
          for (node = hash->nodes[i]; node; node = next) {
               next = node->next;
               fusion_hash_node_destroy( hash, node, NULL, NULL );
          }
     }

     if (hash->local)
          D_FREE( hash->nodes );
     else
          SHFREE( hash->pool, hash->nodes );

     D_MAGIC_CLEAR( hash );

     if (hash->local)
          D_FREE( hash );
     else
          SHFREE( hash->pool, hash );
}

void
fusion_hash_set_autofree( FusionHash *hash,
                          bool        free_keys,
                          bool        free_values )
{
     D_MAGIC_ASSERT( hash, FusionHash );

     hash->free_keys   = free_keys;
     hash->free_values = free_values;
}

void *
fusion_hash_lookup( FusionHash *hash,
                    const void *key )
{
     FusionHashNode *node;

     D_MAGIC_ASSERT( hash, FusionHash );

     node = *fusion_hash_lookup_node( hash, key );

     return node ? node->value : NULL;
}

DirectResult
fusion_hash_insert( FusionHash *hash,
                    void       *key,
                    void       *value )
{
     FusionHashNode **node;

     D_MAGIC_ASSERT( hash, FusionHash );

     node = fusion_hash_lookup_node( hash, key );

     if (*node) {
          D_BUG( "key already exists" );
          return DR_BUG;
     }
     else {
          if (hash->local)
               (*node) = D_CALLOC( 1, sizeof(FusionHashNode) );
          else
               (*node) = SHCALLOC( hash->pool, 1, sizeof(FusionHashNode) );

          if (!(*node))
               return hash->local ? DR_NOLOCALMEMORY : DR_NOSHAREDMEMORY;

          (*node)->key = key;
          (*node)->value = value;

          hash->nnodes++;

          if (fusion_hash_should_resize( hash ))
               fusion_hash_resize( hash );
     }

     return DR_OK;
}

DirectResult
fusion_hash_replace( FusionHash  *hash,
                     void        *key,
                     void        *value,
                     void       **old_key,
                     void       **old_value )
{
     FusionHashNode **node;

     D_MAGIC_ASSERT( hash, FusionHash );

     node = fusion_hash_lookup_node( hash, key );

     if (*node) {
          if (old_key)
               *old_key = (*node)->key;
          else if (hash->key_type != HASH_INT) {
               if (hash->free_keys) {
                    if (hash->local)
                         D_FREE( (*node)->key );
                    else
                         SHFREE( hash->pool, (*node)->key );
               }
          }

          if (old_value)
               *old_value = (*node)->value;
          else if (hash->value_type != HASH_INT) {
               if (hash->free_values) {
                    if (hash->local)
                         D_FREE( (*node)->value );
                    else
                         SHFREE( hash->pool, (*node)->value );
               }
          }

          (*node)->key = key;
          (*node)->value = value;
     }
     else {
          if (hash->local)
               *node = D_CALLOC( 1, sizeof(FusionHashNode) );
          else
               *node = SHCALLOC( hash->pool, 1, sizeof(FusionHashNode) );

          if (!(*node))
               return hash->local ? DR_NOLOCALMEMORY : DR_NOSHAREDMEMORY;

          (*node)->key = key;
          (*node)->value = value;

          hash->nnodes++;

          if (fusion_hash_should_resize( hash ))
               fusion_hash_resize( hash );
     }

     return DR_OK;
}

DirectResult
fusion_hash_remove( FusionHash *hash,
                    const void *key,
                    void       **old_key,
                    void       **old_value )
{
     FusionHashNode **node;
     FusionHashNode  *dest;

     D_MAGIC_ASSERT( hash, FusionHash );

     node = fusion_hash_lookup_node( hash, key );
     if (*node) {
          dest = *node;

          (*node) = dest->next;

          fusion_hash_node_destroy( hash, dest, old_key, old_value );

          hash->nnodes--;

          return DR_OK;
     }

     return DR_OK;
}

void
fusion_hash_iterate( FusionHash             *hash,
                     FusionHashIteratorFunc  func,
                     void                   *ctx )
{
     FusionHashNode *node;
     FusionHashNode *next;
     int             i;

     D_MAGIC_ASSERT( hash, FusionHash );

     for (i = 0; i < hash->size; i++) {
          for (node = hash->nodes[i]; node; node = next) {
               next = node->next;

               if (func( hash, node->key, node->value, ctx) )
                    return;
          }
     }
}

unsigned int
fusion_hash_size( FusionHash *hash )
{
     D_MAGIC_ASSERT( hash, FusionHash );

     return hash->nnodes;
}

bool fusion_hash_should_resize( FusionHash *hash )
{
     D_MAGIC_ASSERT( hash, FusionHash );

     if ((hash->size >= 3 * hash->nnodes && hash->size > FUSION_HASH_MIN_SIZE) ||
         (3 * hash->size <= hash->nnodes && hash->size < FUSION_HASH_MAX_SIZE))
          return true;

     return false;
}

DirectResult
fusion_hash_resize( FusionHash *hash )
{
     FusionHashNode **new_nodes;
     FusionHashNode *node;
     FusionHashNode *next;
     unsigned int    hash_val;
     int             new_size;
     int             i;

     D_MAGIC_ASSERT( hash, FusionHash );

     new_size = spaced_primes_closest( hash->nnodes );
     if (new_size > FUSION_HASH_MAX_SIZE )
          new_size = FUSION_HASH_MAX_SIZE;
     if (new_size <  FUSION_HASH_MIN_SIZE)
          new_size = FUSION_HASH_MIN_SIZE;

     if (hash->local)
          new_nodes = D_CALLOC( new_size, sizeof(FusionHashNode*) );
     else
          new_nodes = SHCALLOC( hash->pool, new_size, sizeof(FusionHashNode*) );

     if (!new_nodes)
          return hash->local?DR_NOLOCALMEMORY:DR_NOSHAREDMEMORY;

     for (i = 0; i < hash->size; i++) {
          for (node = hash->nodes[i]; node; node = next) {
               next = node->next;
               if (hash->key_type == HASH_STRING) {
                    unsigned int  h;
                    const char   *p = node->key;
                    HASH_STR(h, p)
                    hash_val = h % new_size;
               }
               else
                    hash_val = ((unsigned long) node->key) % new_size;

               node->next = new_nodes[hash_val];
               new_nodes[hash_val] = node;
          }
     }

     if (hash->local)
          D_FREE( hash->nodes );
     else
          SHFREE( hash->pool, hash->nodes );

     hash->nodes = new_nodes;
     hash->size = new_size;

     return true;
}

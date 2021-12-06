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

#ifndef __FUSION_HASH_H__
#define __FUSION_HASH_H__

#include <direct/debug.h>
#include <fusion/types.h>

/**********************************************************************************************************************/

typedef enum {
     HASH_PTR    = 0x00000000,
     HASH_STRING = 0x00000001,
     HASH_INT    = 0x00000002
} FusionHashType;

typedef struct _FusionHashNode {
    void                   *key;
    void                   *value;
    struct _FusionHashNode *next;
} FusionHashNode;

struct __Fusion_FusionHash {
    int                   magic;
    bool                  local;
    FusionHashType        key_type;
    FusionHashType        value_type;
    int                   size;
    int                   nnodes;
    FusionHashNode      **nodes;
    FusionSHMPoolShared  *pool;

    bool                  free_keys;
    bool                  free_values;
};

/**********************************************************************************************************************/

#define FUSION_HASH_MIN_SIZE 11
#define FUSION_HASH_MAX_SIZE 13845163

typedef bool (*FusionHashIteratorFunc)( FusionHash *hash, void *key, void *value, void *ctx );

/**********************************************************************************************************************/

/*
 * Creates a new hash that uses local memory.
 */
DirectResult FUSION_API  fusion_hash_create_local ( FusionHashType           key_type,
                                                    FusionHashType           value_type,
                                                    int                      size,
                                                    FusionHash             **ret_hash );

/*
 * Creates a new hash.
 */
DirectResult FUSION_API  fusion_hash_create       ( FusionSHMPoolShared     *pool,
                                                    FusionHashType           key_type,
                                                    FusionHashType           value_type,
                                                    int                      size,
                                                    FusionHash             **ret_hash );

/*
 * Destroys a hash.
 */
void         FUSION_API  fusion_hash_destroy      ( FusionHash              *hash );

/*
 * Frees keys or values when calling fusion_hash_replace(), fusion_hash_remove(), fusion_hash_destroy().
 */
void         FUSION_API  fusion_hash_set_autofree ( FusionHash               *hash,
                                                    bool                      free_keys,
                                                    bool                      free_values );

/*
 * Looks up a key in a hash.
 */
void         FUSION_API *fusion_hash_lookup       ( FusionHash              *hash,
                                                    const void              *key );

/*
 * Inserts a new key and value into a hash.
 * If you think a key may exist you should call fusion_hash_replace().
 */
DirectResult FUSION_API  fusion_hash_insert       ( FusionHash              *hash,
                                                    void                    *key,
                                                    void                    *value );

/*
 * Inserts a new key and value into a hash similar to hash_insert().
 * The difference is that if the key already exists in the hash, it gets replaced by the new key.
 */
DirectResult FUSION_API  fusion_hash_replace      ( FusionHash              *hash,
                                                    void                    *key,
                                                    void                    *value,
                                                    void                   **old_key,
                                                    void                   **old_value );

/*
 * Removes a key and its associated value from a hash.
 */
DirectResult FUSION_API  fusion_hash_remove       ( FusionHash              *hash,
                                                    const void              *key,
                                                    void                   **old_key,
                                                    void                   **old_value );

/*
 * Calls the given function for each of the key/value pairs in a hash.
 * The hash table can't be modified while iterating over it.
 */
void         FUSION_API  fusion_hash_iterate      ( FusionHash              *hash,
                                                    FusionHashIteratorFunc   func,
                                                    void                    *ctx );

/*
 * Returns the number of key/value pairs contained in a hash.
 */
unsigned int FUSION_API  fusion_hash_size         ( FusionHash              *hash );

/*
 * Determines if the hash has grown to large.
 */
bool         FUSION_API  fusion_hash_should_resize( FusionHash              *hash );

/*
 * Resizes the hash to minimum.
 */
DirectResult FUSION_API  fusion_hash_resize       ( FusionHash              *hash );

/**********************************************************************************************************************/

typedef struct {
     FusionHash     *hash;
     int             index;
     FusionHashNode *next;
} FusionHashIterator;

static __inline__ void *
fusion_hash_iterator_next( FusionHashIterator *iterator )
{
     FusionHashNode *node = NULL;

     if (iterator->next) {
          node           = iterator->next;
          iterator->next = node->next;
     }
     else {
          FusionHash *hash = iterator->hash;

          D_MAGIC_ASSERT( hash, FusionHash );

          for (iterator->index++; iterator->index < hash->size; iterator->index++) {
               node = hash->nodes[iterator->index];
               if (node) {
                    iterator->next = node->next;

                    break;
               }
          }
     }

     return node ? node->value : NULL;
}

static __inline__ void *
fusion_hash_iterator_init( FusionHashIterator *iterator,
                           FusionHash         *hash )
{
     D_MAGIC_ASSERT( hash, FusionHash );

     iterator->hash  = hash;
     iterator->index = -1;
     iterator->next  = NULL;

     return fusion_hash_iterator_next( iterator );
}

#define fusion_hash_foreach(elem,iterator,hash)                                     \
     for ((elem) = (__typeof__(elem)) fusion_hash_iterator_init( &iterator, hash ); \
          (elem) != NULL;                                                           \
          (elem) = (__typeof__(elem)) fusion_hash_iterator_next( &iterator ))

#endif

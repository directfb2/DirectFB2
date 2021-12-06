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

#include <fusion/arena.h>
#include <fusion/fusion_internal.h>
#include <fusion/hash.h>
#include <fusion/shmalloc.h>

D_DEBUG_DOMAIN( Fusion_Arena, "Fusion/Arena", "Fusion Arena" );

/**********************************************************************************************************************/

struct __Fusion_FusionArena {
     DirectLink         link;

     int                magic;

     FusionWorldShared *shared;

     FusionSkirmish     lock;
     FusionRef          ref;

     char              *name;

     FusionHash        *field_hash;
};

/**********************************************************************************************************************/

static FusionArena *lock_arena  ( FusionWorld *world, const char  *name, bool add );

static void         unlock_arena( FusionArena *arena );

/**********************************************************************************************************************/

DirectResult
fusion_arena_enter( FusionWorld     *world,
                    const char      *name,
                    ArenaEnterFunc   initialize,
                    ArenaEnterFunc   join,
                    void            *ctx,
                    FusionArena    **ret_arena,
                    int             *ret_error )
{
     FusionArena    *arena;
     ArenaEnterFunc  func;
     int             error = 0;

     D_MAGIC_ASSERT( world, FusionWorld );
     D_MAGIC_ASSERT( world->shared, FusionWorldShared );
     D_ASSERT( name != NULL );
     D_ASSERT( ret_arena != NULL );

     D_DEBUG_AT( Fusion_Arena, "%s( '%s' )\n", __FUNCTION__, name );

     /* Lookup arena and lock it. If it doesn't exist create it. */
     arena = lock_arena( world, name, true );
     if (!arena)
          return DR_FAILURE;

     /* Check if we are the first. */
     if (fusion_ref_zero_trylock( &arena->ref ) == DR_OK) {
          D_DEBUG_AT( Fusion_Arena, "  -> entering arena '%s' (establishing)\n", name );

          /* Call 'initialize' later. */
          func = initialize;

          /* Unlock the reference counter. */
          fusion_ref_unlock( &arena->ref );
     }
     else {
          D_DEBUG_AT( Fusion_Arena, "  -> entering arena '%s' (joining)\n", name );

          /* Call 'join' later. */
          func = join;
     }

     /* Increase reference counter. */
     fusion_ref_up( &arena->ref, false );

     /* Return the arena. */
     *ret_arena = arena;

     /* Call 'initialize' or 'join'. */
     if (func)
          error = func( arena, ctx );

     /* Return the return value of the callback. */
     if (ret_error)
          *ret_error = error;

     if (error) {
          fusion_ref_down( &arena->ref, false );

          if (func == initialize) {
               /* Destroy fields. */
               fusion_hash_destroy( arena->field_hash );

               /* Destroy reference counter. */
               fusion_ref_destroy( &arena->ref );

               /* Destroy the arena lock. This has to happen before locking the list.
                  Otherwise a dead lock with lock_arena() below could occur. */
               fusion_skirmish_destroy( &arena->lock );

               /* Lock the list and remove the arena. */
               fusion_skirmish_prevail( &world->shared->arenas_lock );
               direct_list_remove( &world->shared->arenas, &arena->link );
               fusion_skirmish_dismiss( &world->shared->arenas_lock );

               D_MAGIC_CLEAR( arena );

               /* Free allocated memory. */
               SHFREE( world->shared->main_pool, arena->name );
               SHFREE( world->shared->main_pool, arena );

               return DR_OK;
          }
     }

     /* Unlock the arena. */
     unlock_arena( arena );

     return DR_OK;
}

DirectResult
fusion_arena_add_shared_field( FusionArena *arena,
                               const char  *name,
                               void        *data )
{
     DirectResult  ret;
     char         *shname;

     D_MAGIC_ASSERT( arena, FusionArena );
     D_MAGIC_ASSERT( arena->shared, FusionWorldShared );
     D_ASSERT( data != NULL );
     D_ASSERT( name != NULL );

     D_DEBUG_AT( Fusion_Arena, "%s( '%s', '%s' -> %p )\n", __FUNCTION__, arena->name, name, data );

     /* Lock the arena. */
     ret = fusion_skirmish_prevail( &arena->lock );
     if (ret)
          return ret;

     /* Give it the requested name. */
     shname = SHSTRDUP( arena->shared->main_pool, name );
     if (shname)
          ret = fusion_hash_replace( arena->field_hash, shname, data, NULL, NULL );
     else
          ret = D_OOSHM();

     /* Unlock the arena. */
     fusion_skirmish_dismiss( &arena->lock );

     return ret;
}

DirectResult
fusion_arena_get_shared_field( FusionArena  *arena,
                               const char   *name,
                               void        **data )
{
     void *ptr;

     D_ASSERT( arena != NULL );
     D_ASSERT( name != NULL );
     D_ASSERT( data != NULL );

     D_MAGIC_ASSERT( arena, FusionArena );

     D_DEBUG_AT( Fusion_Arena, "%s( '%s', '%s' )\n", __FUNCTION__, arena->name, name );

     /* Lock the arena. */
     if (fusion_skirmish_prevail( &arena->lock ))
          return DR_FAILURE;

     /* Lookup entry. */
     ptr = fusion_hash_lookup( arena->field_hash, name );

     D_DEBUG_AT( Fusion_Arena, "  -> %p\n", ptr );

     /* Unlock the arena. */
     fusion_skirmish_dismiss( &arena->lock );

     if (!ptr)
          return DR_ITEMNOTFOUND;

     *data = ptr;

     return DR_OK;
}

DirectResult
fusion_arena_exit( FusionArena   *arena,
                   ArenaExitFunc  shutdown,
                   ArenaExitFunc  leave,
                   void          *ctx,
                   bool           emergency,
                   int           *ret_error )
{
     int error = 0;

     D_MAGIC_ASSERT( arena, FusionArena );
     D_MAGIC_ASSERT( arena->shared, FusionWorldShared );
     D_ASSERT( shutdown != NULL );

     D_DEBUG_AT( Fusion_Arena, "%s( '%s' )\n", __FUNCTION__, arena->name );

     /* Lock the arena. */
     if (fusion_skirmish_prevail( &arena->lock ))
          return DR_FAILURE;

     /* Decrease reference counter. */
     fusion_ref_down( &arena->ref, false );

     /* If we are the last... */
     if (fusion_ref_zero_trylock( &arena->ref ) == DR_OK) {
          /* Deinitialize everything. */
          error = shutdown( arena, ctx, emergency );

          /* Destroy fields. */
          fusion_hash_destroy( arena->field_hash );

          /* Destroy reference counter. */
          fusion_ref_destroy( &arena->ref );

          /* Destroy the arena lock. This has to happen before locking the list.
             Otherwise a dead lock with lock_arena() below could occur. */
          fusion_skirmish_destroy( &arena->lock );

          /* Lock the list and remove the arena. */
          fusion_skirmish_prevail( &arena->shared->arenas_lock );
          direct_list_remove( &arena->shared->arenas, &arena->link );
          fusion_skirmish_dismiss( &arena->shared->arenas_lock );

          D_MAGIC_CLEAR( arena );

          /* Free allocated memory. */
          SHFREE( arena->shared->main_pool, arena->name );
          SHFREE( arena->shared->main_pool, arena );
     }
     else {
          if (!leave) {
               fusion_ref_up( &arena->ref, false );
               fusion_skirmish_dismiss( &arena->lock );
               return DR_BUSY;
          }

          /* Simply leave the arena. */
          error = leave( arena, ctx, emergency );

          /* Unlock the arena. */
          fusion_skirmish_dismiss( &arena->lock );
     }

     /* Return the return value of the callback. */
     if (ret_error)
          *ret_error = error;

     return DR_OK;
}

/**********************************************************************************************************************/

static FusionArena *
create_arena( FusionWorld *world,
              const char  *name )
{
     DirectResult  ret;
     char          buf[64];
     FusionArena  *arena;

     D_MAGIC_ASSERT( world, FusionWorld );
     D_MAGIC_ASSERT( world->shared, FusionWorldShared );
     D_ASSERT( name != NULL );

     arena = SHCALLOC( world->shared->main_pool, 1, sizeof(FusionArena) );
     if (!arena) {
          D_OOSHM();
          return NULL;
     }

     arena->shared = world->shared;

     snprintf( buf, sizeof(buf), "Arena '%s'", name );

     /* Initialize lock and reference counter. */
     ret = fusion_skirmish_init( &arena->lock, buf, world );
     if (ret)
          goto error;

     fusion_skirmish_add_permissions( &arena->lock, 0,
                                      FUSION_SKIRMISH_PERMIT_PREVAIL | FUSION_SKIRMISH_PERMIT_DISMISS );

     ret = fusion_ref_init( &arena->ref, buf, world );
     if (ret)
          goto error_ref;

     fusion_ref_add_permissions( &arena->ref, 0,
                                 FUSION_REF_PERMIT_REF_UNREF_LOCAL | FUSION_REF_PERMIT_ZERO_LOCK_UNLOCK );

     /* Give it the requested name. */
     arena->name = SHSTRDUP( world->shared->main_pool, name );
     if (!arena->name) {
          D_OOSHM();
          goto error_prevail;
     }

     ret = fusion_hash_create( world->shared->main_pool, HASH_STRING, HASH_PTR, 7, &arena->field_hash );
     if (ret)
          goto error_hash;

     fusion_hash_set_autofree( arena->field_hash, true, false );

     /* Add it to the list. */
     direct_list_prepend( &world->shared->arenas, &arena->link );

     /* Lock the newly created arena. */
     ret = fusion_skirmish_prevail( &arena->lock );
     if (ret)
          goto error_prevail;

     D_MAGIC_SET( arena, FusionArena );

     /* Returned locked new arena. */
     return arena;

error_prevail:
     fusion_hash_destroy( arena->field_hash );

error_hash:
     if (arena->name)
          SHFREE( world->shared->main_pool, arena->name );

     fusion_ref_destroy( &arena->ref );

error_ref:
     fusion_skirmish_destroy( &arena->lock );

error:
     SHFREE( world->shared->main_pool, arena );

     return NULL;
}

static FusionArena *
lock_arena( FusionWorld *world,
            const char  *name,
            bool         add )
{
     FusionArena *arena;

     D_MAGIC_ASSERT( world, FusionWorld );
     D_MAGIC_ASSERT( world->shared, FusionWorldShared );
     D_ASSERT( name != NULL );

     /* Lock the list. */
     if (fusion_skirmish_prevail( &world->shared->arenas_lock ))
          return NULL;

     /* For each existing arena... */
     direct_list_foreach (arena, world->shared->arenas) {
          /* Lock the arena. */
          if (fusion_skirmish_prevail( &arena->lock ))
               continue;

          D_MAGIC_ASSERT( arena, FusionArena );

          /* Check if the name matches. */
          if (!strcmp( arena->name, name )) {
               /* Check for an orphaned arena. */
               if (fusion_ref_zero_trylock( &arena->ref ) == DR_OK) {
                    D_ERROR( "Fusion/Arena: Orphaned arena '%s'!\n", name );

                    fusion_ref_unlock( &arena->ref );
               }

               /* Unlock the list. */
               fusion_skirmish_dismiss( &world->shared->arenas_lock );

               /* Return locked arena. */
               return arena;
          }

          /* Unlock mismatched arena. */
          fusion_skirmish_dismiss( &arena->lock );
     }

     /* If no arena name matched, create a new arena before unlocking the list again. */
     arena = add ? create_arena( world, name ) : NULL;

     /* Unlock the list. */
     fusion_skirmish_dismiss( &world->shared->arenas_lock );

     return arena;
}

static void
unlock_arena( FusionArena *arena )
{
     D_MAGIC_ASSERT( arena, FusionArena );

     /* Unlock the arena. */
     fusion_skirmish_dismiss( &arena->lock );
}

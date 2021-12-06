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
#include <direct/memcpy.h>
#include <fusion/lock.h>

#if FUSION_BUILD_MULTI

#include <direct/thread.h>
#include <fusion/conf.h>
#include <fusion/fusion_internal.h>

#if !FUSION_BUILD_KERNEL
#include <direct/system.h>
#include <fusion/shmalloc.h>
#endif /* FUSION_BUILD_KERNEL */

#endif /* FUSION_BUILD_MULTI */

D_DEBUG_DOMAIN( Fusion_Skirmish, "Fusion/Skirmish", "Fusion's Skirmish (Mutex)" );

/**********************************************************************************************************************/

#if FUSION_BUILD_MULTI

#if FUSION_BUILD_KERNEL

DirectResult
fusion_skirmish_init( FusionSkirmish    *skirmish,
                      const char        *name,
                      const FusionWorld *world )
{
     FusionEntryInfo info;

     D_ASSERT( skirmish != NULL );
     D_ASSERT( name != NULL );
     D_MAGIC_ASSERT( world, FusionWorld );

     D_DEBUG_AT( Fusion_Skirmish, "%s( %p, '%s' )\n", __FUNCTION__, skirmish, name ?: "" );

     while (ioctl( world->fusion_fd, FUSION_SKIRMISH_NEW, &skirmish->multi.id )) {
          if (errno == EINTR)
               continue;

          D_PERROR( "Fusion/Skirmish: FUSION_SKIRMISH_NEW" );
          return DR_FUSION;
     }

     D_DEBUG_AT( Fusion_Skirmish, "  -> new skirmish %p [%d]\n", skirmish, skirmish->multi.id );

     info.type = FT_SKIRMISH;
     info.id   = skirmish->multi.id;

     direct_snputs( info.name, name, sizeof(info.name) );

     ioctl( world->fusion_fd, FUSION_ENTRY_SET_INFO, &info );

     fusion_entry_add_permissions( world, FT_SKIRMISH, skirmish->multi.id, 0, FUSION_SKIRMISH_LOCK_COUNT, 0 );

     /* Keep back pointer to shared world data. */
     skirmish->multi.shared = world->shared;

     return DR_OK;
}

DirectResult
fusion_skirmish_init2( FusionSkirmish    *skirmish,
                       const char        *name,
                       const FusionWorld *world,
                       bool               local )
{
     D_ASSERT( skirmish != NULL );
     D_ASSERT( name != NULL );
     D_MAGIC_ASSERT( world, FusionWorld );

     D_DEBUG_AT( Fusion_Skirmish, "%s( %p, '%s', %s )\n", __FUNCTION__, skirmish, name ?: "",
                 local ? "local" : "shared" );

     if (!local)
          return fusion_skirmish_init( skirmish, name, world );


     skirmish->single = D_CALLOC( 1, sizeof(FusionSkirmishSingle) + strlen( name ) + 1 );
     if (skirmish->single == 0)
          return DR_NOLOCALMEMORY;

     skirmish->single->name = (char*) (skirmish->single + 1);
     strcpy( skirmish->single->name, name );

     direct_recursive_mutex_init( &skirmish->single->lock );
     direct_waitqueue_init( &skirmish->single->cond );

     D_MAGIC_SET( skirmish->single, FusionSkirmishSingle );

     /* Keep back pointer to shared world data. */
     skirmish->multi.shared = world->shared;

     return DR_OK;
}

DirectResult
fusion_skirmish_prevail( FusionSkirmish *skirmish )
{
     DirectResult ret;

     D_ASSERT( skirmish != NULL );

     if (skirmish->single) {
          D_DEBUG_AT( Fusion_Skirmish, "%s( %p, '%s' )\n", __FUNCTION__, skirmish, skirmish->single->name );

          D_MAGIC_ASSERT( skirmish->single, FusionSkirmishSingle );

          ret= direct_mutex_lock( &skirmish->single->lock );
          if (ret)
               return ret;

          skirmish->single->count++;

          return DR_OK;
     }

     D_DEBUG_AT( Fusion_Skirmish, "%s( %p [%d] )\n", __FUNCTION__, skirmish, skirmish->multi.id );

     while (ioctl( _fusion_fd( skirmish->multi.shared ), FUSION_SKIRMISH_PREVAIL, &skirmish->multi.id )) {
          switch (errno) {
               case EINTR:
                    continue;

               case EINVAL:
                    D_ERROR( "Fusion/Skirmish: Invalid skirmish!\n" );
                    return DR_DESTROYED;
          }

          D_PERROR( "Fusion/Skirmish: FUSION_SKIRMISH_PREVAIL" );
          return DR_FUSION;
     }

     return DR_OK;
}

DirectResult
fusion_skirmish_swoop( FusionSkirmish *skirmish )
{
     DirectResult ret;

     D_ASSERT( skirmish != NULL );

     if (skirmish->single) {
          D_DEBUG_AT( Fusion_Skirmish, "%s( %p, '%s' )\n", __FUNCTION__, skirmish, skirmish->single->name );

          D_MAGIC_ASSERT( skirmish->single, FusionSkirmishSingle );

          ret = direct_mutex_trylock( &skirmish->single->lock );
          if (ret)
               return ret;

          skirmish->single->count++;

          return DR_OK;
     }

     D_DEBUG_AT( Fusion_Skirmish, "%s( %p [%d] )\n", __FUNCTION__, skirmish, skirmish->multi.id );

     while (ioctl( _fusion_fd( skirmish->multi.shared ), FUSION_SKIRMISH_SWOOP, &skirmish->multi.id )) {
          switch (errno) {
               case EINTR:
                    continue;

               case EAGAIN:
                    return DR_BUSY;

               case EINVAL:
                    D_ERROR( "Fusion/Skirmish: Invalid skirmish!\n" );
                    return DR_DESTROYED;
          }

          D_PERROR( "Fusion/Skirmish: FUSION_SKIRMISH_SWOOP" );
          return DR_FUSION;
     }

     return DR_OK;
}

DirectResult
fusion_skirmish_lock_count( FusionSkirmish *skirmish,
                            int            *lock_count )
{
     DirectResult ret;
     int          data[2];

     D_ASSERT( skirmish != NULL );

     if (skirmish->single) {
          D_DEBUG_AT( Fusion_Skirmish, "%s( %p, '%s' )\n", __FUNCTION__, skirmish, skirmish->single->name );

          D_MAGIC_ASSERT( skirmish->single, FusionSkirmishSingle );

          ret = direct_mutex_trylock( &skirmish->single->lock );
          if (ret) {
               *lock_count = 0;
               return ret;
          }

          *lock_count = skirmish->single->count;

          direct_mutex_unlock( &skirmish->single->lock );

          return DR_OK;
     }

     D_DEBUG_AT( Fusion_Skirmish, "%s( %p [%d] )\n", __FUNCTION__, skirmish, skirmish->multi.id );

     data[0] = skirmish->multi.id;
     data[1] = 0;

     while (ioctl( _fusion_fd( skirmish->multi.shared ), FUSION_SKIRMISH_LOCK_COUNT, data )) {
           switch (errno) {
               case EINTR:
                    continue;

               case EINVAL:
                    D_ERROR( "Fusion/Skirmish: Invalid skirmish!\n" );
                    return DR_DESTROYED;
           }

          D_PERROR( "Fusion/Skirmish: FUSION_SKIRMISH_LOCK_COUNT" );
          return DR_FUSION;
     }

     *lock_count = data[1];

     return DR_OK;
}

DirectResult
fusion_skirmish_dismiss( FusionSkirmish *skirmish )
{
     D_ASSERT( skirmish != NULL );

     if (skirmish->single) {
          D_DEBUG_AT( Fusion_Skirmish, "%s( %p, '%s' )\n", __FUNCTION__, skirmish, skirmish->single->name );

          D_MAGIC_ASSERT( skirmish->single, FusionSkirmishSingle );

          skirmish->single->count--;

          return direct_mutex_unlock( &skirmish->single->lock );
     }

     D_DEBUG_AT( Fusion_Skirmish, "%s( %p [%d] )\n", __FUNCTION__, skirmish, skirmish->multi.id );

     while (ioctl( _fusion_fd( skirmish->multi.shared ), FUSION_SKIRMISH_DISMISS, &skirmish->multi.id )) {
          switch (errno) {
               case EINTR:
                    continue;

               case EINVAL:
                    D_ERROR( "Fusion/Skirmish: Invalid skirmish!\n" );
                    return DR_DESTROYED;
          }

          D_PERROR( "Fusion/Skirmish: FUSION_SKIRMISH_DISMISS" );
          return DR_FUSION;
     }

     return DR_OK;
}

DirectResult
fusion_skirmish_destroy( FusionSkirmish *skirmish )
{
     DirectResult ret;

     D_ASSERT( skirmish != NULL );

     if (skirmish->single) {
          D_DEBUG_AT( Fusion_Skirmish, "%s( %p, '%s' )\n", __FUNCTION__, skirmish, skirmish->single->name );

          D_MAGIC_ASSERT( skirmish->single, FusionSkirmishSingle );

          direct_waitqueue_broadcast( &skirmish->single->cond );
          direct_waitqueue_deinit( &skirmish->single->cond );

          ret = direct_mutex_deinit( &skirmish->single->lock );

          D_MAGIC_CLEAR( skirmish->single );

          D_FREE( skirmish->single );

          return ret;
     }

     D_DEBUG_AT( Fusion_Skirmish, "%s( %p [%d] )\n", __FUNCTION__, skirmish, skirmish->multi.id );

     while (ioctl( _fusion_fd( skirmish->multi.shared ), FUSION_SKIRMISH_DESTROY, &skirmish->multi.id )) {
          switch (errno) {
               case EINTR:
                    continue;

               case EINVAL:
                    D_ERROR( "Fusion/Skirmish: Invalid skirmish!\n" );
                    return DR_DESTROYED;
          }

          D_PERROR( "Fusion/Skirmish: FUSION_SKIRMISH_DESTROY" );
          return DR_FUSION;
     }

     return DR_OK;
}

DirectResult
fusion_skirmish_wait( FusionSkirmish *skirmish,
                      unsigned int    timeout )
{
     FusionSkirmishWait wait;

     D_ASSERT( skirmish != NULL );

     if (skirmish->single) {
          D_DEBUG_AT( Fusion_Skirmish, "%s( %p, '%s' )\n", __FUNCTION__, skirmish, skirmish->single->name );

          D_MAGIC_ASSERT( skirmish->single, FusionSkirmishSingle );

          if (timeout)
               return direct_waitqueue_wait_timeout( &skirmish->single->cond,
                                                     &skirmish->single->lock, timeout * 1000 );

          return direct_waitqueue_wait( &skirmish->single->cond, &skirmish->single->lock );
     }

     D_DEBUG_AT( Fusion_Skirmish, "%s( %p [%d] )\n", __FUNCTION__, skirmish, skirmish->multi.id );

     wait.id         = skirmish->multi.id;
     wait.timeout    = timeout;
     wait.lock_count = 0;

     while (ioctl( _fusion_fd( skirmish->multi.shared ), FUSION_SKIRMISH_WAIT, &wait )) {
          switch (errno) {
               case EINTR:
                    continue;

               case ETIMEDOUT:
                    return DR_TIMEOUT;

               case EINVAL:
                    D_ERROR( "Fusion/Skirmish: Invalid skirmish!\n" );
                    return DR_DESTROYED;
          }

          D_PERROR( "Fusion/Skirmish: FUSION_SKIRMISH_WAIT" );
          return DR_FUSION;
     }

     return DR_OK;
}

DirectResult
fusion_skirmish_notify( FusionSkirmish *skirmish )
{
     D_ASSERT( skirmish != NULL );

     if (skirmish->single) {
          D_DEBUG_AT( Fusion_Skirmish, "%s( %p, '%s' )\n", __FUNCTION__, skirmish, skirmish->single->name );

          D_MAGIC_ASSERT( skirmish->single, FusionSkirmishSingle );

          direct_waitqueue_broadcast( &skirmish->single->cond );

          return DR_OK;
     }

     D_DEBUG_AT( Fusion_Skirmish, "%s( %p [%d] )\n", __FUNCTION__, skirmish, skirmish->multi.id );

     while (ioctl( _fusion_fd( skirmish->multi.shared ), FUSION_SKIRMISH_NOTIFY, &skirmish->multi.id )) {
          switch (errno) {
               case EINTR:
                    continue;

               case EINVAL:
                    D_ERROR( "Fusion/Skirmish: Invalid skirmish!\n" );
                    return DR_DESTROYED;
          }

          D_PERROR( "Fusion/Skirmish: FUSION_SKIRMISH_NOTIFY" );
          return DR_FUSION;
     }

     return DR_OK;
}

DirectResult
fusion_skirmish_add_permissions( FusionSkirmish            *skirmish,
                                 FusionID                   fusion_id,
                                 FusionSkirmishPermissions  skirmish_permissions )
{
     FusionEntryPermissions permissions;

     D_ASSERT( skirmish != NULL );

     if (skirmish->single) {
          D_DEBUG_AT( Fusion_Skirmish, "%s( %p, '%s' )\n", __FUNCTION__, skirmish, skirmish->single->name );

          D_MAGIC_ASSERT( skirmish->single, FusionSkirmishSingle );

          return DR_OK;
     }

     permissions.type        = FT_SKIRMISH;
     permissions.id          = skirmish->multi.id;
     permissions.fusion_id   = fusion_id;
     permissions.permissions = 0;

     if (skirmish_permissions & FUSION_SKIRMISH_PERMIT_PREVAIL)
          FUSION_ENTRY_PERMISSIONS_ADD( permissions.permissions, FUSION_SKIRMISH_PREVAIL );

     if (skirmish_permissions & FUSION_SKIRMISH_PERMIT_SWOOP)
          FUSION_ENTRY_PERMISSIONS_ADD( permissions.permissions, FUSION_SKIRMISH_SWOOP );

     if (skirmish_permissions & FUSION_SKIRMISH_PERMIT_DISMISS)
          FUSION_ENTRY_PERMISSIONS_ADD( permissions.permissions, FUSION_SKIRMISH_DISMISS );

     if (skirmish_permissions & FUSION_SKIRMISH_PERMIT_LOCK_COUNT)
          FUSION_ENTRY_PERMISSIONS_ADD( permissions.permissions, FUSION_SKIRMISH_LOCK_COUNT );

     if (skirmish_permissions & FUSION_SKIRMISH_PERMIT_WAIT)
          FUSION_ENTRY_PERMISSIONS_ADD( permissions.permissions, FUSION_SKIRMISH_WAIT );

     if (skirmish_permissions & FUSION_SKIRMISH_PERMIT_NOTIFY)
          FUSION_ENTRY_PERMISSIONS_ADD( permissions.permissions, FUSION_SKIRMISH_NOTIFY );

     if (skirmish_permissions & FUSION_SKIRMISH_PERMIT_DESTROY)
          FUSION_ENTRY_PERMISSIONS_ADD( permissions.permissions, FUSION_SKIRMISH_DESTROY );

     while (ioctl( _fusion_fd( skirmish->multi.shared ), FUSION_ENTRY_ADD_PERMISSIONS, &permissions ) < 0) {
          if (errno != EINTR) {
               D_PERROR( "Fusion/Skirmish: FUSION_ENTRY_ADD_PERMISSIONS( id %d )\n", skirmish->multi.id );
               return DR_FAILURE;
          }
     }

     return DR_OK;
}

#else /* FUSION_BUILD_KERNEL */

typedef struct {
     DirectLink link;

     pid_t      pid;
     bool       notified;
} WaitNode;

DirectResult
fusion_skirmish_init( FusionSkirmish    *skirmish,
                      const char        *name,
                      const FusionWorld *world )
{
     D_ASSERT( skirmish != NULL );
     D_MAGIC_ASSERT( world, FusionWorld );

     D_DEBUG_AT( Fusion_Skirmish, "%s( %p, '%s' )\n", __FUNCTION__, skirmish, name ?: "" );

     skirmish->multi.id = ++world->shared->lock_ids;

     /* Set state to unlocked. */
     skirmish->multi.builtin.locked = 0;
     skirmish->multi.builtin.owner  = 0;

     skirmish->multi.builtin.waiting = NULL;

     skirmish->multi.builtin.requested = false;
     skirmish->multi.builtin.destroyed = false;

     /* Keep back pointer to shared world data. */
     skirmish->multi.shared = world->shared;

     return DR_OK;
}

DirectResult
fusion_skirmish_init2( FusionSkirmish    *skirmish,
                       const char        *name,
                       const FusionWorld *world,
                       bool               local )
{
     D_ASSERT( skirmish != NULL );
     D_ASSERT( name != NULL );
     D_MAGIC_ASSERT( world, FusionWorld );

     D_DEBUG_AT( Fusion_Skirmish, "%s( %p, '%s', %s )\n", __FUNCTION__, skirmish, name ?: "",
                 local ? "local" : "shared" );

     if (!local)
          return fusion_skirmish_init( skirmish, name, world );


     skirmish->single = D_CALLOC( 1, sizeof(FusionSkirmishSingle) + strlen( name ) + 1 );
     if (skirmish->single == 0)
          return DR_NOLOCALMEMORY;

     skirmish->single->name = (char*) (skirmish->single + 1);
     strcpy( skirmish->single->name, name );

     direct_recursive_mutex_init( &skirmish->single->lock );
     direct_waitqueue_init( &skirmish->single->cond );

     D_MAGIC_SET( skirmish->single, FusionSkirmishSingle );

     /* Keep back pointer to shared world data. */
     skirmish->multi.shared = world->shared;

     return DR_OK;
}

DirectResult
fusion_skirmish_prevail( FusionSkirmish *skirmish )
{
     DirectResult ret;

     D_ASSERT( skirmish != NULL );

     D_DEBUG_AT( Fusion_Skirmish, "%s( %p )\n", __FUNCTION__, skirmish );

     if (skirmish->single) {
          D_MAGIC_ASSERT( skirmish->single, FusionSkirmishSingle );

          ret = direct_mutex_lock( &skirmish->single->lock );
          if (ret)
               return ret;

          skirmish->single->count++;

          return DR_OK;
     }

     if (skirmish->multi.builtin.destroyed)
          return DR_DESTROYED;

     asm( "" ::: "memory" );

     if (skirmish->multi.builtin.locked && skirmish->multi.builtin.owner != direct_gettid()) {
          int count = 0;

          while (skirmish->multi.builtin.locked) {
               /* Check whether owner exited without unlocking. */
               ret = direct_kill( skirmish->multi.builtin.owner, 0 );
               if (ret == DR_NOSUCHINSTANCE) {
                    skirmish->multi.builtin.locked = 0;
                    skirmish->multi.builtin.requested = false;
                    break;
               }

               skirmish->multi.builtin.requested = true;

               asm( "" ::: "memory" );

               if (++count > 1000) {
                    usleep( 10000 );
                    count = 0;
               }
               else {
                    direct_sched_yield();
               }

               if (skirmish->multi.builtin.destroyed)
                    return DR_DESTROYED;
          }
     }

     skirmish->multi.builtin.locked++;
     skirmish->multi.builtin.owner = direct_gettid();

     asm( "" ::: "memory" );

     return DR_OK;
}

DirectResult
fusion_skirmish_swoop( FusionSkirmish *skirmish )
{
     DirectResult ret;

     D_ASSERT( skirmish != NULL );

     if (skirmish->single) {
          D_MAGIC_ASSERT( skirmish->single, FusionSkirmishSingle );

          ret = direct_mutex_trylock( &skirmish->single->lock );
          if (ret)
               return ret;

          skirmish->single->count++;

          return DR_OK;
     }

     if (skirmish->multi.builtin.destroyed)
          return DR_DESTROYED;

     asm( "" ::: "memory" );

     if (skirmish->multi.builtin.locked && skirmish->multi.builtin.owner != direct_gettid()) {
          /* Check whether owner exited without unlocking. */
          ret = direct_kill( skirmish->multi.builtin.owner, 0 );
          if (ret == DR_NOSUCHINSTANCE) {
               skirmish->multi.builtin.locked = 0;
               skirmish->multi.builtin.requested = false;
          }
          else
               return DR_BUSY;
     }

     skirmish->multi.builtin.locked++;
     skirmish->multi.builtin.owner = direct_gettid();

     asm( "" ::: "memory" );

     return DR_OK;
}

DirectResult
fusion_skirmish_lock_count( FusionSkirmish *skirmish,
                            int            *lock_count )
{
     DirectResult ret;

     D_ASSERT( skirmish != NULL );

     if (skirmish->single) {
          D_MAGIC_ASSERT( skirmish->single, FusionSkirmishSingle );

          ret = direct_mutex_trylock( &skirmish->single->lock );
          if (ret) {
               *lock_count = 0;
               return ret;
          }

          *lock_count = skirmish->single->count;

          direct_mutex_unlock( &skirmish->single->lock );

          return DR_OK;
     }

     if (skirmish->multi.builtin.destroyed) {
          *lock_count = 0;
          return DR_DESTROYED;
     }

     *lock_count = skirmish->multi.builtin.locked;

     return DR_OK;
}

DirectResult
fusion_skirmish_dismiss( FusionSkirmish *skirmish )
{
     D_ASSERT( skirmish != NULL );

     if (skirmish->single) {
          D_MAGIC_ASSERT( skirmish->single, FusionSkirmishSingle );

          skirmish->single->count--;

          return direct_mutex_unlock( &skirmish->single->lock );
     }

     if (skirmish->multi.builtin.destroyed)
          return DR_DESTROYED;

     asm( "" ::: "memory" );

     if (skirmish->multi.builtin.locked) {
          if (skirmish->multi.builtin.owner != direct_gettid()) {
               D_ERROR( "Fusion/Skirmish: Tried to dismiss a skirmish not owned by the current process!\n" );
               return DR_ACCESSDENIED;
          }

          if (--skirmish->multi.builtin.locked == 0) {
               skirmish->multi.builtin.owner = 0;

               if (skirmish->multi.builtin.requested) {
                    skirmish->multi.builtin.requested = false;
                    direct_sched_yield();
               }
          }
     }

     asm( "" ::: "memory" );

     return DR_OK;
}

DirectResult
fusion_skirmish_destroy( FusionSkirmish *skirmish )
{
     DirectResult ret;

     D_ASSERT( skirmish != NULL );

     D_DEBUG_AT( Fusion_Skirmish, "%s( %p )\n", __FUNCTION__, skirmish );

     if (skirmish->single) {
          D_MAGIC_ASSERT( skirmish->single, FusionSkirmishSingle );

          direct_waitqueue_broadcast( &skirmish->single->cond );
          direct_waitqueue_deinit( &skirmish->single->cond );

          ret = direct_mutex_deinit( &skirmish->single->lock );

          D_MAGIC_CLEAR( skirmish->single );

          D_FREE( skirmish->single );

          return ret;
     }

     if (skirmish->multi.builtin.destroyed)
          return DR_DESTROYED;

     if (skirmish->multi.builtin.waiting)
          fusion_skirmish_notify( skirmish );

     skirmish->multi.builtin.destroyed = true;

     return DR_OK;
}

static void restart_handler( int s ) {}

DirectResult
fusion_skirmish_wait( FusionSkirmish *skirmish,
                      unsigned int    timeout )
{
     WaitNode         *node;
     long long         stop;
     struct sigaction  act, oldact;
     sigset_t          mask, set;
     DirectResult      ret = DR_OK;

     D_ASSERT( skirmish != NULL );

     if (skirmish->single) {
          D_MAGIC_ASSERT( skirmish->single, FusionSkirmishSingle );

          if (timeout)
               return direct_waitqueue_wait_timeout( &skirmish->single->cond,
                                                     &skirmish->single->lock, timeout * 1000 );

          return direct_waitqueue_wait( &skirmish->single->cond, &skirmish->single->lock );
     }

     if (skirmish->multi.builtin.destroyed)
          return DR_DESTROYED;

     /* Set timeout. */
     stop = direct_clock_get_micros() + timeout * 1000ll;

     /* Add ourself to the list of waiting processes. */
     node = SHMALLOC( skirmish->multi.shared->main_pool, sizeof(WaitNode) );
     if (!node)
          return D_OOSHM();

     node->pid      = direct_gettid();
     node->notified = false;

     direct_list_append( &skirmish->multi.builtin.waiting, &node->link );

     /* Install a (fake) signal handler for SIGRTMAX. */
     act.sa_handler = restart_handler;
     act.sa_flags   = SA_RESETHAND | SA_RESTART | SA_NOMASK;

     sigaction( SIGRTMAX, &act, &oldact );

     /* Unblock SIGRTMAX. */
     direct_sigprocmask( SIG_SETMASK, NULL, &mask );
     sigdelset( &mask, SIGRTMAX );

     fusion_skirmish_dismiss( skirmish );

     while (!node->notified) {
          if (timeout) {
               long long now = direct_clock_get_micros();

               if (now >= stop) {
                    /* Stop notifying us. */
                    node->notified = true;
                    ret = DR_TIMEOUT;
                    break;
               }

               direct_sigprocmask( SIG_SETMASK, &mask, &set );
               usleep( stop - now );
               direct_sigprocmask( SIG_SETMASK, &set, NULL );
          }
          else {
               sigsuspend( &mask );
          }
     }

     /* Flush pending signals. */
     if (!sigpending( &set ) && sigismember( &set, SIGRTMAX ) > 0)
          sigsuspend( &mask );

     if (fusion_skirmish_prevail( skirmish ))
          ret = DR_DESTROYED;

     direct_list_remove( &skirmish->multi.builtin.waiting, &node->link );

     SHFREE( skirmish->multi.shared->main_pool, node );

     sigaction( SIGRTMAX, &oldact, NULL );

     return ret;
}

DirectResult
fusion_skirmish_notify( FusionSkirmish *skirmish )
{
     DirectResult  ret;
     WaitNode     *node, *next;

     D_ASSERT( skirmish != NULL );

     if (skirmish->single) {
          D_MAGIC_ASSERT( skirmish->single, FusionSkirmishSingle );

          direct_waitqueue_broadcast( &skirmish->single->cond );

          return DR_OK;
     }

     if (skirmish->multi.builtin.destroyed)
          return DR_DESTROYED;

     direct_list_foreach_safe (node, next, skirmish->multi.builtin.waiting) {
          if (node->notified)
               continue;

          node->notified = true;

          ret = direct_kill( node->pid, SIGRTMAX );
          if (ret) {
               if (ret == DR_NOSUCHINSTANCE) {
                    /* Remove dead process. */
                    direct_list_remove( &skirmish->multi.builtin.waiting, &node->link );
                    SHFREE( skirmish->multi.shared->main_pool, node );
               }
               else {
                    D_DERROR( ret, "Fusion/Skirmish: Could not send notification signal!\n" );
               }
          }
     }

     return DR_OK;
}

DirectResult
fusion_skirmish_add_permissions( FusionSkirmish            *skirmish,
                                 FusionID                   fusion_id,
                                 FusionSkirmishPermissions  skirmish_permissions )
{
     return DR_OK;
}

#endif /* FUSION_BUILD_KERNEL */

#else /* FUSION_BUILD_MULTI */

DirectResult
fusion_skirmish_init( FusionSkirmish    *skirmish,
                      const char        *name,
                      const FusionWorld *world )
{
     D_ASSERT( skirmish != NULL );

     D_DEBUG_AT( Fusion_Skirmish, "%s( %p, '%s' )\n", __FUNCTION__, skirmish, name ?: "" );

     skirmish->single = D_CALLOC( 1, sizeof(FusionSkirmishSingle) + strlen( name ) + 1 );
     if (skirmish->single == 0)
          return DR_NOLOCALMEMORY;

     skirmish->single->name = (char*) (skirmish->single + 1);
     strcpy( skirmish->single->name, name );

     direct_recursive_mutex_init( &skirmish->single->lock );
     direct_waitqueue_init( &skirmish->single->cond );

     D_MAGIC_SET( skirmish->single, FusionSkirmishSingle );

     return DR_OK;
}

DirectResult
fusion_skirmish_init2( FusionSkirmish    *skirmish,
                       const char        *name,
                       const FusionWorld *world,
                       bool               local )
{
     return fusion_skirmish_init( skirmish, name, world );
}

DirectResult
fusion_skirmish_prevail( FusionSkirmish *skirmish )
{
     DirectResult ret;

     D_ASSERT( skirmish != NULL );
     D_MAGIC_ASSERT( skirmish->single, FusionSkirmishSingle );

     D_DEBUG_AT( Fusion_Skirmish, "%s( %p, '%s' )\n", __FUNCTION__, skirmish, skirmish->single->name );

     ret = direct_mutex_lock( &skirmish->single->lock );
     if (ret)
          return ret;

     skirmish->single->count++;

     return DR_OK;
}

DirectResult
fusion_skirmish_swoop( FusionSkirmish *skirmish )
{
     DirectResult ret;

     D_ASSERT( skirmish != NULL );
     D_MAGIC_ASSERT( skirmish->single, FusionSkirmishSingle );

     D_DEBUG_AT( Fusion_Skirmish, "%s( %p, '%s' )\n", __FUNCTION__, skirmish, skirmish->single->name );

     ret = direct_mutex_trylock( &skirmish->single->lock );
     if (ret)
          return ret;

     skirmish->single->count++;

     return DR_OK;
}

DirectResult
fusion_skirmish_lock_count( FusionSkirmish *skirmish,
                            int            *lock_count )
{
     DirectResult ret;

     D_ASSERT( skirmish != NULL );
     D_MAGIC_ASSERT( skirmish->single, FusionSkirmishSingle );
     D_ASSERT( lock_count != NULL );

     D_DEBUG_AT( Fusion_Skirmish, "%s( %p, '%s' )\n", __FUNCTION__, skirmish, skirmish->single->name );

     ret = direct_mutex_trylock( &skirmish->single->lock );
     if (ret) {
          *lock_count = 0;
          return ret;
     }

     *lock_count = skirmish->single->count;

     direct_mutex_unlock( &skirmish->single->lock );

     return DR_OK;
}

DirectResult
fusion_skirmish_dismiss( FusionSkirmish *skirmish )
{
     D_ASSERT( skirmish != NULL );
     D_MAGIC_ASSERT( skirmish->single, FusionSkirmishSingle );

     D_DEBUG_AT( Fusion_Skirmish, "%s( %p, '%s' )\n", __FUNCTION__, skirmish, skirmish->single->name );

     skirmish->single->count--;

     return direct_mutex_unlock( &skirmish->single->lock );
}

DirectResult
fusion_skirmish_destroy( FusionSkirmish *skirmish )
{
     DirectResult ret;

     D_ASSERT( skirmish != NULL );
     D_MAGIC_ASSERT( skirmish->single, FusionSkirmishSingle );

     D_DEBUG_AT( Fusion_Skirmish, "%s( %p, '%s' )\n", __FUNCTION__, skirmish, skirmish->single->name );

     direct_waitqueue_broadcast( &skirmish->single->cond );
     direct_waitqueue_deinit( &skirmish->single->cond );

     ret = direct_mutex_deinit( &skirmish->single->lock );

     D_MAGIC_CLEAR( skirmish->single );

     D_FREE( skirmish->single );

     return ret;
}

DirectResult
fusion_skirmish_wait( FusionSkirmish *skirmish,
                      unsigned int    timeout )
{
     D_ASSERT( skirmish != NULL );
     D_MAGIC_ASSERT( skirmish->single, FusionSkirmishSingle );

     D_DEBUG_AT( Fusion_Skirmish, "%s( %p, '%s' )\n", __FUNCTION__, skirmish, skirmish->single->name );

     if (timeout)
          return direct_waitqueue_wait_timeout( &skirmish->single->cond,
                                                &skirmish->single->lock, timeout * 1000 );

     return direct_waitqueue_wait( &skirmish->single->cond, &skirmish->single->lock );
}

DirectResult
fusion_skirmish_notify( FusionSkirmish *skirmish )
{
     D_ASSERT( skirmish != NULL );
     D_MAGIC_ASSERT( skirmish->single, FusionSkirmishSingle );

     D_DEBUG_AT( Fusion_Skirmish, "%s( %p, '%s' )\n", __FUNCTION__, skirmish, skirmish->single->name );

     direct_waitqueue_broadcast( &skirmish->single->cond );

     return DR_OK;
}

DirectResult
fusion_skirmish_add_permissions( FusionSkirmish            *skirmish,
                                 FusionID                   fusion_id,
                                 FusionSkirmishPermissions  skirmish_permissions )
{
     return DR_OK;
}

#endif /* FUSION_BUILD_MULTI */

static int
ptr_compare( const void *p1, const void *p2 )
{
     return *(u32*) p1 - *(u32*) p2;
}

DirectResult
fusion_skirmish_prevail_multi( FusionSkirmish **skirmishs,
                               unsigned int     num )
{
     DirectResult    ret = DR_OK;
     unsigned int    i;
     FusionSkirmish *skirmishs_sorted[num];

     D_DEBUG_AT( Fusion_Skirmish, "%s( %p, %u )\n", __FUNCTION__, skirmishs, num );

     direct_memcpy( skirmishs_sorted, skirmishs, num * sizeof(FusionSkirmish*) );

     qsort( skirmishs_sorted, num, sizeof(FusionSkirmish*), ptr_compare );

     for (i = 0; i < num; i++) {
          ret = fusion_skirmish_prevail( skirmishs_sorted[i] );
          if (ret) {
               D_DERROR( ret, "Fusion/Skirmish: Failed at index %u, skirmish_id [0x%08x]\n",
                         i, (unsigned int) skirmishs_sorted[i]->multi.id );
               break;
          }
     }

     if (ret) {
          for (--i; i >= 0; i--)
               fusion_skirmish_dismiss( skirmishs_sorted[i] );
     }

     return ret;
}

DirectResult
fusion_skirmish_dismiss_multi( FusionSkirmish **skirmishs,
                               unsigned int     num )
{
     DirectResult    ret = DR_OK, ret2;
     unsigned int    i;
     FusionSkirmish *skirmishs_sorted[num];

     D_DEBUG_AT( Fusion_Skirmish, "%s( %p, %u )\n", __FUNCTION__, skirmishs, num );

     direct_memcpy( skirmishs_sorted, skirmishs, num * sizeof(FusionSkirmish*) );

     qsort( skirmishs_sorted, num, sizeof(FusionSkirmish*), ptr_compare );

     for (i = 0; i < num; i++) {
          ret2 = fusion_skirmish_dismiss( skirmishs_sorted[i] );
          if (ret2) {
               D_DERROR( ret, "Fusion/Skirmish: Failed at index %u, skirmish_id [0x%08x]\n",
                         i, (unsigned int) skirmishs_sorted[i]->multi.id );
               ret = ret2;
          }
     }

     return ret;
}

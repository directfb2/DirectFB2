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

#ifndef __FUSION__OBJECT_H__
#define __FUSION__OBJECT_H__

#include <fusion/call.h>
#include <fusion/reactor.h>
#include <fusion/ref.h>
#include <fusion/vector.h>

/**********************************************************************************************************************/

typedef u32 FusionObjectID;

typedef enum {
     FOS_INIT   = 0x00000000,
     FOS_ACTIVE = 0x00000001,
     FOS_DEINIT = 0x00000002
} FusionObjectState;

struct __Fusion_FusionObject {
     DirectLink         link;
     FusionObjectPool  *pool;

     int                magic;

     FusionObjectID     id;
     FusionID           identity;

     FusionObjectState  state;

     FusionRef          ref;
     FusionReactor     *reactor;

     FusionWorldShared *shared;
     FusionHash        *properties;

     FusionVector       owners;

     FusionVector       access;

     DirectTraceBuffer *create_stack;

     void              *type_instance;
};

typedef void (*FusionObjectDestructor)( FusionObject *object, bool zombie, void *ctx );

struct __Fusion_FusionObjectPool {
     int                     magic;

     FusionWorldShared      *shared;

     FusionSkirmish          lock;
     FusionHash             *objects;
     FusionObjectID          id_pool;

     char                   *name;
     int                     object_size;
     int                     message_size;
     FusionObjectDestructor  destructor;
     void                   *ctx;

     FusionCall              call;

     bool                    secure;
};

/**********************************************************************************************************************/

typedef bool (*FusionObjectCallback)( FusionObjectPool *pool, FusionObject *object, void *ctx );

/**********************************************************************************************************************/

FusionObjectPool FUSION_API *fusion_object_pool_create    ( const char              *name,
                                                            int                      object_size,
                                                            int                      message_size,
                                                            FusionObjectDestructor   destructor,
                                                            void                    *ctx,
                                                            const FusionWorld       *world );

DirectResult     FUSION_API  fusion_object_pool_destroy   ( FusionObjectPool        *pool,
                                                            FusionWorld             *world,
                                                            bool                     shutdown_info );

DirectResult     FUSION_API  fusion_object_pool_enum      ( FusionObjectPool        *pool,
                                                            FusionObjectCallback     callback,
                                                            void                    *ctx );

DirectResult     FUSION_API  fusion_object_pool_size      ( FusionObjectPool        *pool,
                                                            size_t                  *ret_size );

FusionObject     FUSION_API *fusion_object_create         ( FusionObjectPool        *pool,
                                                            const FusionWorld       *world,
                                                            FusionID                 identity );

DirectResult     FUSION_API  fusion_object_get            ( FusionObjectPool        *pool,
                                                            FusionObjectID           object_id,
                                                            FusionObject           **ret_object );

DirectResult     FUSION_API  fusion_object_lookup         ( FusionObjectPool        *pool,
                                                            FusionObjectID           object_id,
                                                            FusionObject           **ret_object );

DirectResult     FUSION_API  fusion_object_set_lock       ( FusionObject            *object,
                                                            FusionSkirmish          *lock );

DirectResult     FUSION_API  fusion_object_activate       ( FusionObject            *object );

DirectResult     FUSION_API  fusion_object_destroy        ( FusionObject            *object );

DirectResult     FUSION_API  fusion_object_set_property   ( FusionObject            *object,
                                                            const char              *key,
                                                            void                    *value,
                                                            void                   **old_value );

void             FUSION_API *fusion_object_get_property   ( FusionObject            *object,
                                                            const char              *key );

void             FUSION_API  fusion_object_remove_property( FusionObject            *object,
                                                            const char              *key,
                                                            void                   **ret_val );

DirectResult     FUSION_API  fusion_object_add_access     ( FusionObject            *object,
                                                            const char              *exectuable );

DirectResult     FUSION_API  fusion_object_has_access     ( FusionObject            *object,
                                                            const char              *executable );

DirectResult     FUSION_API  fusion_object_add_owner      ( FusionObject            *object,
                                                            FusionID                 owner );

DirectResult     FUSION_API  fusion_object_check_owner    ( FusionObject            *object,
                                                            FusionID                 owner,
                                                            bool                     succeed_if_not_owned );

DirectResult     FUSION_API  fusion_object_catch          ( FusionObject            *object );

/**********************************************************************************************************************/

#define FUSION_OBJECT_METHODS(type,prefix)                                                                        \
                                                                                                                  \
static __inline__ DirectResult                                                                                    \
prefix##_attach( type         *object,                                                                            \
                 ReactionFunc  func,                                                                              \
                 void         *ctx,                                                                               \
                 Reaction     *ret_reaction )                                                                     \
{                                                                                                                 \
     D_MAGIC_ASSERT( (FusionObject*) object, FusionObject );                                                      \
     return fusion_reactor_attach( ((FusionObject*) object)->reactor, func, ctx, ret_reaction );                  \
}                                                                                                                 \
                                                                                                                  \
static __inline__ DirectResult                                                                                    \
prefix##_attach_channel( type         *object,                                                                    \
                         int           channel,                                                                   \
                         ReactionFunc  func,                                                                      \
                         void         *ctx,                                                                       \
                         Reaction     *ret_reaction )                                                             \
{                                                                                                                 \
     D_MAGIC_ASSERT( (FusionObject*) object, FusionObject );                                                      \
     return fusion_reactor_attach_channel( ((FusionObject*) object)->reactor, channel, func, ctx, ret_reaction ); \
}                                                                                                                 \
                                                                                                                  \
static __inline__ DirectResult                                                                                    \
prefix##_detach( type     *object,                                                                                \
                 Reaction *reaction )                                                                             \
{                                                                                                                 \
     D_MAGIC_ASSERT( (FusionObject*) object, FusionObject );                                                      \
     return fusion_reactor_detach( ((FusionObject*) object)->reactor, reaction );                                 \
}                                                                                                                 \
                                                                                                                  \
static __inline__ DirectResult                                                                                    \
prefix##_attach_global( type           *object,                                                                   \
                        int             index,                                                                    \
                        void           *ctx,                                                                      \
                        GlobalReaction *reaction )                                                                \
{                                                                                                                 \
     D_MAGIC_ASSERT( (FusionObject*) object, FusionObject );                                                      \
     return fusion_reactor_attach_global( ((FusionObject*) object)->reactor, index, ctx, reaction );              \
}                                                                                                                 \
                                                                                                                  \
static __inline__ DirectResult                                                                                    \
prefix##_detach_global( type           *object,                                                                   \
                        GlobalReaction *reaction )                                                                \
{                                                                                                                 \
     D_MAGIC_ASSERT( (FusionObject*) object, FusionObject );                                                      \
     return fusion_reactor_detach_global( ((FusionObject*) object)->reactor, reaction );                          \
}                                                                                                                 \
                                                                                                                  \
static __inline__ DirectResult                                                                                    \
prefix##_dispatch( type               *object,                                                                    \
                   void               *message,                                                                   \
                   const ReactionFunc *globals )                                                                  \
{                                                                                                                 \
     D_MAGIC_ASSERT( (FusionObject*) object, FusionObject );                                                      \
     return fusion_reactor_dispatch( ((FusionObject*) object)->reactor, message, true, globals );                 \
}                                                                                                                 \
                                                                                                                  \
static __inline__ DirectResult                                                                                    \
prefix##_dispatch_channel( type               *object,                                                            \
                           int                 channel,                                                           \
                           void               *message,                                                           \
                           int                 size,                                                              \
                           const ReactionFunc *globals )                                                          \
{                                                                                                                 \
     D_MAGIC_ASSERT( (FusionObject*) object, FusionObject );                                                      \
     return fusion_reactor_dispatch_channel( ((FusionObject*) object)->reactor,                                   \
                                             channel, message, size, true, globals );                             \
}                                                                                                                 \
                                                                                                                  \
static __inline__ DirectResult                                                                                    \
prefix##_ref( type *object )                                                                                      \
{                                                                                                                 \
     D_MAGIC_ASSERT( (FusionObject*) object, FusionObject );                                                      \
     return fusion_ref_up( &((FusionObject*) object)->ref, false );                                               \
}                                                                                                                 \
                                                                                                                  \
static __inline__ DirectResult                                                                                    \
prefix##_unref( type *object )                                                                                    \
{                                                                                                                 \
     D_MAGIC_ASSERT( (FusionObject*) object, FusionObject );                                                      \
     return fusion_ref_down( &((FusionObject*) object)->ref, false );                                             \
}                                                                                                                 \
                                                                                                                  \
static __inline__ DirectResult                                                                                    \
prefix##_ref_stat( type *object, int *refs )                                                                      \
{                                                                                                                 \
     D_MAGIC_ASSERT( (FusionObject*) object, FusionObject );                                                      \
     return fusion_ref_stat ( &((FusionObject*) object)->ref, refs );                                             \
}                                                                                                                 \
                                                                                                                  \
static __inline__ DirectResult                                                                                    \
prefix##_link( type **link,                                                                                       \
               type  *object )                                                                                    \
{                                                                                                                 \
     DirectResult ret;                                                                                            \
                                                                                                                  \
     D_MAGIC_ASSERT( (FusionObject*) object, FusionObject );                                                      \
                                                                                                                  \
     ret = fusion_ref_up( &((FusionObject*) object)->ref, true );                                                 \
     if (ret)                                                                                                     \
          return ret;                                                                                             \
                                                                                                                  \
     *link = object;                                                                                              \
                                                                                                                  \
     return DR_OK;                                                                                                \
}                                                                                                                 \
                                                                                                                  \
static __inline__ DirectResult                                                                                    \
prefix##_unlink( type **link )                                                                                    \
{                                                                                                                 \
     type *object = *link;                                                                                        \
                                                                                                                  \
     D_MAGIC_ASSERT( (FusionObject*) object, FusionObject );                                                      \
                                                                                                                  \
     *link = NULL;                                                                                                \
                                                                                                                  \
     return fusion_ref_down( &((FusionObject*) object)->ref, true );                                              \
}                                                                                                                 \
                                                                                                                  \
static __inline__ DirectResult                                                                                    \
prefix##_inherit( type *object,                                                                                   \
                  void *from )                                                                                    \
{                                                                                                                 \
     D_MAGIC_ASSERT( (FusionObject*) object, FusionObject );                                                      \
     D_MAGIC_ASSERT( (FusionObject*) from, FusionObject );                                                        \
                                                                                                                  \
     return fusion_ref_inherit( &((FusionObject*) object)->ref, &((FusionObject*) from)->ref );                   \
}                                                                                                                 \
                                                                                                                  \
static __inline__ DirectResult                                                                                    \
prefix##_globalize( type *object )                                                                                \
{                                                                                                                 \
     DirectResult ret;                                                                                            \
                                                                                                                  \
     D_MAGIC_ASSERT( (FusionObject*) object, FusionObject );                                                      \
                                                                                                                  \
     ret = fusion_ref_up( &((FusionObject*) object)->ref, true );                                                 \
     if (ret)                                                                                                     \
          return ret;                                                                                             \
                                                                                                                  \
     ret = fusion_ref_down( &((FusionObject*) object)->ref, false );                                              \
     if (ret)                                                                                                     \
          fusion_ref_down( &((FusionObject*) object)->ref, true );                                                \
                                                                                                                  \
     return ret;                                                                                                  \
}

FUSION_OBJECT_METHODS( void, fusion_object )

#endif

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

#ifndef __CORE__SURFACE_BUFFER_H__
#define __CORE__SURFACE_BUFFER_H__

#include <core/surface.h>

/**********************************************************************************************************************/

typedef enum {
     CSP_SYSTEMONLY = 0x00000000, /* Never try to swap into video memory. */
     CSP_VIDEOLOW   = 0x00000001, /* Try to store in video memory, low priority. */
     CSP_VIDEOHIGH  = 0x00000002, /* Try to store in video memory, high priority. */
     CSP_VIDEOONLY  = 0x00000003  /* Always and only store in video memory. */
} CoreSurfacePolicy;

typedef enum {
     CSBF_NONE     = 0x00000000, /* None of these. */

     CSBF_DECOUPLE = 0x00000002, /* Buffer is about to be deallocated and removed from surface. */
     CSBF_RIGHT    = 0x00000004, /* Buffer is for right eye. */

     CSBF_ALL      = 0x00000006  /* All of these. */
} CoreSurfaceBufferFlags;

struct __DFB_CoreSurfaceBuffer {
     FusionObject            object;

     int                     magic;

     DirectSerial            serial;      /* Increased when content is written. */
     CoreSurfaceAllocation  *written;     /* Allocation with the last write access. */
     CoreSurfaceAllocation  *read;        /* Allocation with the last read access. */

     CoreSurface            *surface;     /* Surface owning this surface buffer. */
     CoreSurfacePolicy       policy;      /* Policy of its surface. */

     CoreSurfaceBufferFlags  flags;       /* Configuration and state flags. */

     FusionVector            allocs;      /* Allocations within surface pools. */

     CoreSurfaceConfig       config;      /* Configuration of its surface at the time of the buffer creation */
     CoreSurfaceTypeFlags    type;        /* Classification of the surface. */

     unsigned long           resource_id; /* layer id, window id, or user specified */
     int                     index;       /* index of surface buffer */

     unsigned int            busy;        /* busy buffer */

     FusionObjectID          surface_id;  /* surface id */
};

struct __DFB_CoreSurfaceBufferLock {
     int                     magic;      /* Must be valid before calling dfb_surface_pool_lock(). */

     CoreSurfaceAccessorID   accessor;   /* Accessor ID. */
     CoreSurfaceAccessFlags  access;     /* Access flags. */

     CoreSurfaceBuffer      *buffer;     /* Set by dfb_surface_pool_lock(). */
     CoreSurfaceAllocation  *allocation; /* Allocation of a surface buffer. */

     void                   *addr;       /* address of buffer */
     unsigned long           phys;       /* physical address */
     unsigned long           offset;     /* framebuffer offset */
     unsigned int            pitch;      /* pitch of buffer */

     void                   *handle;     /* handle */
};

#if D_DEBUG_ENABLED
#define CORE_SURFACE_BUFFER_LOCK_ASSERT(lock)                                                                          \
     do {                                                                                                              \
          D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );                                                               \
          D_FLAGS_ASSERT( (lock)->access, CSAF_ALL );                                                                  \
          if ((lock)->allocation) {                                                                                    \
               D_ASSERT( (lock)->pitch > 0 || ((lock)->addr == NULL && (lock)->phys == 0) );                           \
               D_ASSUME( (lock)->addr != NULL || (lock)->phys != 0 || (lock)->offset != ~0 || (lock)->handle != NULL );\
               D_ASSUME( (lock)->offset == (lock)->allocation->offset || (lock)->offset == ~0 );                       \
          }                                                                                                            \
          else {                                                                                                       \
               D_ASSERT( (lock)->buffer == NULL );                                                                     \
               D_ASSERT( (lock)->addr == NULL );                                                                       \
               D_ASSERT( (lock)->phys == 0 );                                                                          \
               D_ASSERT( (lock)->offset == ~0 );                                                                       \
               D_ASSERT( (lock)->pitch == 0 );                                                                         \
               D_ASSERT( (lock)->handle == NULL );                                                                     \
          }                                                                                                            \
     } while (0)
#else
#define CORE_SURFACE_BUFFER_LOCK_ASSERT(lock)                                                                          \
     do {                                                                                                              \
     } while (0)
#endif

/**********************************************************************************************************************/

typedef enum {
     CSBNF_NONE = 0x00000000
} CoreSurfaceBufferNotificationFlags;

typedef struct {
     CoreSurfaceBufferNotificationFlags flags;
} CoreSurfaceBufferNotification;

/**********************************************************************************************************************/

/*
 * Creates a pool of surface buffer objects.
 */
FusionObjectPool      *dfb_surface_buffer_pool_create        ( const FusionWorld       *world );

/*
 * Generates dfb_surface_buffer_ref(), dfb_surface_buffer_attach() etc.
 */
FUSION_OBJECT_METHODS( CoreSurfaceBuffer, dfb_surface_buffer )

/**********************************************************************************************************************/

DFBResult              dfb_surface_buffer_create             ( CoreDFB                 *core,
                                                               CoreSurface             *surface,
                                                               CoreSurfaceBufferFlags   flags,
                                                               int                      index,
                                                               CoreSurfaceBuffer      **ret_buffer );

DFBResult              dfb_surface_buffer_decouple           ( CoreSurfaceBuffer       *buffer );

DFBResult              dfb_surface_buffer_deallocate         ( CoreSurfaceBuffer       *buffer );

CoreSurfaceAllocation *dfb_surface_buffer_find_allocation    ( CoreSurfaceBuffer       *buffer,
                                                               CoreSurfaceAccessorID    accessor,
                                                               CoreSurfaceAccessFlags   flags,
                                                               bool                     lock );

CoreSurfaceAllocation *dfb_surface_buffer_find_allocation_key( CoreSurfaceBuffer       *buffer,
                                                               const char              *key );

DFBResult              dfb_surface_buffer_lock               ( CoreSurfaceBuffer       *buffer,
                                                               CoreSurfaceAccessorID    accessor,
                                                               CoreSurfaceAccessFlags   access,
                                                               CoreSurfaceBufferLock   *ret_lock );

DFBResult              dfb_surface_buffer_unlock             ( CoreSurfaceBufferLock   *lock );

DFBResult              dfb_surface_buffer_dump_type_locked   ( CoreSurfaceBuffer       *buffer,
                                                               const char              *directory,
                                                               const char              *prefix,
                                                               bool                     raw,
                                                               CoreSurfaceBufferLock   *lock );

DFBResult              dfb_surface_buffer_dump_type_locked2  ( CoreSurfaceBuffer       *buffer,
                                                               const char              *directory,
                                                               const char              *prefix,
                                                               bool                     raw,
                                                               void                    *addr,
                                                               int                      pitch );

DFBResult              dfb_surface_buffer_dump               ( CoreSurfaceBuffer       *buffer,
                                                               const char              *directory,
                                                               const char              *prefix );

DFBResult              dfb_surface_buffer_dump_raw           ( CoreSurfaceBuffer       *buffer,
                                                               const char              *directory,
                                                               const char              *prefix );

/**********************************************************************************************************************/

static __inline__ void
dfb_surface_buffer_lock_reset( CoreSurfaceBufferLock *lock )
{
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     lock->buffer     = NULL;
     lock->allocation = NULL;
     lock->addr       = NULL;
     lock->phys       = 0;
     lock->offset     = ~0;
     lock->pitch      = 0;
     lock->handle     = 0;
}

static __inline__ void
dfb_surface_buffer_lock_init( CoreSurfaceBufferLock  *lock,
                              CoreSurfaceAccessorID   accessor,
                              CoreSurfaceAccessFlags  access )
{
     D_MAGIC_SET_ONLY( lock, CoreSurfaceBufferLock );

     lock->accessor = accessor;
     lock->access   = access;

     dfb_surface_buffer_lock_reset( lock );
}

static __inline__ void
dfb_surface_buffer_lock_deinit( CoreSurfaceBufferLock *lock )
{
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     lock->accessor = CSAID_NONE;
     lock->access   = CSAF_NONE;

     D_MAGIC_CLEAR( lock );
}

static __inline__ int
dfb_surface_buffer_index( CoreSurfaceBuffer *buffer )
{
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     return buffer->index;
}

#endif

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

#ifndef __CORE__SURFACE_ALLOCATION_H__
#define __CORE__SURFACE_ALLOCATION_H__

#include <core/surface.h>

/**********************************************************************************************************************/

typedef enum {
     CSALF_NONE         = 0x00000000, /* None of these. */

     CSALF_INITIALIZING = 0x00000001, /* Allocation is being initialized. */
     CSALF_VOLATILE     = 0x00000002, /* Allocation should be freed when no longer up to date. */
     CSALF_PREALLOCATED = 0x00000004, /* Preallocated memory, don't zap when "thrifty-surface-buffers" is active. */

     CSALF_MUCKOUT      = 0x00001000, /* Indicates surface pool being in the progress of mucking out this and possibly
                                         other allocations to have enough space for a new allocation to be made. */
     CSALF_DEALLOCATED  = 0x00002000, /* Decoupled and deallocated surface buffer allocation. */

     CSALF_ALL          = 0x00003007  /* All of these. */
} CoreSurfaceAllocationFlags;

struct __DFB_CoreSurfaceAllocation {
     FusionObject                  object;

     int                           magic;

     DirectSerial                  serial;               /* Equals serial of buffer if content is up to date. */

     CoreSurfaceBuffer            *buffer;               /* Surface buffer owning this allocation. */
     CoreSurface                  *surface;              /* Surface owning the buffer of this allocation. */
     CoreSurfacePool              *pool;                 /* Surface pool providing the allocation. */
     void                         *data;                 /* Pool's private data for this allocation. */
     int                           size;                 /* Amount of data used by this allocation. */
     unsigned long                 offset;               /* Offset within address range of pool if contiguous. */

     CoreSurfaceAllocationFlags    flags;                /* Configuration and state flags. */

     const CoreSurfaceAccessFlags *access;               /* Possible access flags (pointer to pool description). */
     CoreSurfaceAccessFlags        accessed[_CSAID_NUM]; /* Access since last synchronization. */

     CoreSurfaceConfig             config;               /* Configuration of its surface at the time of the allocation
                                                            creation. */
     CoreSurfaceTypeFlags          type;                 /* Classification of the surface. */

     unsigned long                 resource_id;          /* layer id, window id, or user specified */
     int                           index;                /* index of surface buffer */

     CoreGraphicsSerial            gfx_serial;           /* graphics serial */

     FusionCall                    call;                 /* dispatch */

     FusionObjectID                buffer_id;            /* buffer id */
};

#if D_DEBUG_ENABLED
#define CORE_SURFACE_ALLOCATION_ASSERT(alloc)                       \
     do {                                                           \
          D_MAGIC_ASSERT( alloc, CoreSurfaceAllocation );           \
          D_ASSERT( (alloc)->size >= 0 );                           \
          D_FLAGS_ASSERT( (alloc)->flags, CSALF_ALL );              \
          D_FLAGS_ASSERT( (alloc)->access[CSAID_CPU], CSAF_ALL );   \
          D_FLAGS_ASSERT( (alloc)->access[CSAID_GPU], CSAF_ALL );   \
          D_FLAGS_ASSERT( (alloc)->accessed[CSAID_CPU], CSAF_ALL ); \
          D_FLAGS_ASSERT( (alloc)->accessed[CSAID_GPU], CSAF_ALL ); \
          D_ASSUME( (alloc)->size > 0 );                            \
     } while (0)
#else
#define CORE_SURFACE_ALLOCATION_ASSERT(alloc)                       \
     do {                                                           \
     } while (0)
#endif

/**********************************************************************************************************************/

typedef enum {
     CSANF_NONE        = 0x00000000,

     CSANF_DEALLOCATED = 0x00000001,

     CSANF_ALL         = 0x00000001
} CoreSurfaceAllocationNotificationFlags;

typedef struct {
     CoreSurfaceAllocationNotificationFlags flags;
} CoreSurfaceAllocationNotification;

/**********************************************************************************************************************/

/*
 * Creates a pool of surface allocation objects.
 */
FusionObjectPool *dfb_surface_allocation_pool_create( const FusionWorld       *world );

/*
 * Generates dfb_surface_allocation_ref(), dfb_surface_allocation_attach() etc.
 */
FUSION_OBJECT_METHODS( CoreSurfaceAllocation, dfb_surface_allocation )

/**********************************************************************************************************************/

DFBResult         dfb_surface_allocation_create     ( CoreDFB                 *core,
                                                      CoreSurfaceBuffer       *buffer,
                                                      CoreSurfacePool         *pool,
                                                      CoreSurfaceAllocation  **ret_allocation );

DFBResult         dfb_surface_allocation_decouple   ( CoreSurfaceAllocation   *allocation );

DFBResult         dfb_surface_allocation_update     ( CoreSurfaceAllocation   *allocation,
                                                      CoreSurfaceAccessFlags   access );

DFBResult         dfb_surface_allocation_dump       ( CoreSurfaceAllocation   *allocation,
                                                      const char              *directory,
                                                      const char              *prefix,
                                                      bool                     raw );

/**********************************************************************************************************************/

static __inline__ int
dfb_surface_allocation_locks( CoreSurfaceAllocation *allocation )
{
     int refs;

     fusion_ref_stat( &allocation->object.ref, &refs );

     D_ASSERT( refs > 0 );

     return refs - 1;
}

#endif

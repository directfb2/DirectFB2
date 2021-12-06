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

#include <fusion/shmalloc.h>

#if FUSION_BUILD_MULTI

#include <direct/memcpy.h>
#include <direct/system.h>
#include <direct/log.h>
#include <fusion/conf.h>
#include <fusion/fusion_internal.h>
#include <fusion/shm/pool.h>

#else /* FUSION_BUILD_MULTI */

#include <direct/mem.h>
#include <fusion/shm/shm_internal.h>

#endif /* FUSION_BUILD_MULTI */

#if DIRECT_BUILD_DEBUGS

D_DEBUG_DOMAIN( Fusion_SHM, "Fusion/SHM", "Fusion Shared Memory allocation" );

#endif /* DIRECT_BUILD_DEBUGS */

/**********************************************************************************************************************/

#if FUSION_BUILD_MULTI

void
fusion_print_madvise()
{
     if (fusion_config->madv_remove_force) {
          if (fusion_config->madv_remove)
               D_INFO( "Fusion/SHM: Using MADV_REMOVE (forced)\n" );
          else
               D_INFO( "Fusion/SHM: Not using MADV_REMOVE (forced)!\n" );
     }
     else {
          if (direct_madvise())
               D_INFO( "Fusion/SHM: Using MADV_REMOVE\n" );
          else
               D_INFO( "Fusion/SHM: NOT using MADV_REMOVE!\n" );
     }
}

#endif /* FUSION_BUILD_MULTI */

#if DIRECT_BUILD_DEBUGS

#if FUSION_BUILD_MULTI

void
fusion_print_memleaks( FusionSHMPoolShared *pool )
{
     DirectResult  ret;
     SHMemDesc    *desc;
     unsigned int  total = 0;

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );

     ret = fusion_skirmish_prevail( &pool->lock );
     if (ret) {
          D_DERROR( ret, "Fusion/SHM: Could not lock shared memory pool!\n" );
          return;
     }

     if (pool->allocs) {
          direct_log_printf( NULL, "\nShared memory allocations remaining (%d) in '%s': \n",
                             direct_list_count_elements_EXPENSIVE( pool->allocs ), pool->name );

          direct_list_foreach (desc, pool->allocs) {
               direct_log_printf( NULL, " "_ZUn(9)" bytes at %p [%8lu] in %-30s [%3lx] (%s: %u)\n",
                                  desc->bytes, desc->mem, (unsigned long) desc->mem - (unsigned long) pool->heap,
                                  desc->func, desc->fid, desc->file, desc->line );

               total += desc->bytes;
          }

          direct_log_printf( NULL, "   -------\n  %7uk total\n", total >> 10 );
          direct_log_printf( NULL, "\nShared memory file size: %dk\n", pool->heap->size >> 10 );
     }

     fusion_skirmish_dismiss( &pool->lock );
}

static SHMemDesc *
fill_shmem_desc( SHMemDesc  *desc,
                 int         bytes,
                 const char *func,
                 const char *file,
                 int         line,
                 FusionID    fusion_id )
{
     D_ASSERT( desc != NULL );

     desc->mem   = desc + 1;
     desc->bytes = bytes;

     direct_snputs( desc->func, func, SHMEMDESC_FUNC_NAME_LENGTH );
     direct_snputs( desc->file, file, SHMEMDESC_FILE_NAME_LENGTH );

     desc->line = line;
     desc->fid  = fusion_id;

     return desc;
}

void *
fusion_dbg_shmalloc( FusionSHMPoolShared *pool,
                     const char          *file,
                     int                  line,
                     const char          *func,
                     size_t               size )
{
     DirectResult  ret;
     SHMemDesc    *desc;
     void         *data = NULL;

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( file != NULL );
     D_ASSERT( line > 0 );
     D_ASSERT( func != NULL );
     D_ASSERT( size > 0 );

     if (!pool->debug)
          return fusion_shmalloc( pool, size );

     /* Lock the pool. */
     ret = fusion_skirmish_prevail( &pool->lock );
     if (ret) {
          D_DERROR( ret, "Fusion/SHM: Could not lock shared memory pool!\n" );
          return NULL;
     }

     /* Allocate memory from the pool. */
     ret = fusion_shm_pool_allocate( pool, size + sizeof(SHMemDesc), false, false, &data );
     if (ret) {
          D_DERROR( ret, "Fusion/SHM: Could not allocate "_ZU" bytes from pool!\n", size + sizeof(SHMemDesc) );
          fusion_skirmish_dismiss( &pool->lock );
          return NULL;
     }

     /* Fill description. */
     desc = fill_shmem_desc( data, size, func, file, line, _fusion_id( pool->shm->world ) );

     D_DEBUG_AT( Fusion_SHM, "Allocating "_ZUn(9)" bytes at %p [%8lu] in %-30s [%3lx] (%s: %u)\n",
                 desc->bytes, desc->mem, (unsigned long) desc->mem - (unsigned long) pool->heap,
                 desc->func, desc->fid, desc->file, desc->line );

     /* Add description to list. */
     direct_list_append( &pool->allocs, &desc->link );

     /* Unlock the pool. */
     fusion_skirmish_dismiss( &pool->lock );

     return data + sizeof(SHMemDesc);
}

void *
fusion_dbg_shcalloc( FusionSHMPoolShared *pool,
                     const char          *file,
                     int                  line,
                     const char          *func,
                     size_t               nmemb,
                     size_t               size )
{
     DirectResult  ret;
     SHMemDesc    *desc;
     void         *data = NULL;

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( file != NULL );
     D_ASSERT( line > 0 );
     D_ASSERT( func != NULL );
     D_ASSERT( nmemb > 0 );
     D_ASSERT( size > 0 );

     if (!pool->debug)
          return fusion_shcalloc( pool, nmemb, size );

     /* Lock the pool. */
     ret = fusion_skirmish_prevail( &pool->lock );
     if (ret) {
          D_DERROR( ret, "Fusion/SHM: Could not lock shared memory pool!\n" );
          return NULL;
     }

     /* Allocate memory from the pool. */
     ret = fusion_shm_pool_allocate( pool, nmemb * size + sizeof(SHMemDesc), true, false, &data );
     if (ret) {
          D_DERROR( ret, "Fusion/SHM: Could not allocate "_ZU" bytes from pool!\n", nmemb * size + sizeof(SHMemDesc) );
          fusion_skirmish_dismiss( &pool->lock );
          return NULL;
     }

     /* Fill description. */
     desc = fill_shmem_desc( data, nmemb * size, func, file, line, _fusion_id( pool->shm->world ) );

     D_DEBUG_AT( Fusion_SHM, "Allocating "_ZUn(9)" bytes at %p [%8lu] in %-30s [%3lx] (%s: %u)\n",
                 desc->bytes, desc->mem, (unsigned long) desc->mem - (unsigned long) pool->heap,
                 desc->func, desc->fid, desc->file, desc->line );

     /* Add description to list. */
     direct_list_append( &pool->allocs, &desc->link );

     /* Unlock the pool. */
     fusion_skirmish_dismiss( &pool->lock );

     return data + sizeof(SHMemDesc);
}

void *
fusion_dbg_shrealloc( FusionSHMPoolShared *pool,
                      const char          *file,
                      int                  line,
                      const char          *func,
                      const char          *what,
                      void                *ptr,
                      size_t               size )
{
     DirectResult  ret;
     SHMemDesc    *desc;
     void         *data = NULL;

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( file != NULL );
     D_ASSERT( line > 0 );
     D_ASSERT( func != NULL );
     D_ASSERT( what != NULL );

     if (!pool->debug)
          return fusion_shrealloc( pool, ptr, size );

     if (!ptr)
          return fusion_dbg_shmalloc( pool, file, line, func, size );

     if (!size) {
          fusion_dbg_shfree( pool, file, line, func, what, ptr );
          return NULL;
     }

     /* Lock the pool. */
     ret = fusion_skirmish_prevail( &pool->lock );
     if (ret) {
          D_DERROR( ret, "Fusion/SHM: Could not lock shared memory pool!\n" );
          return NULL;
     }

     /* Lookup the corresponding description. */
     direct_list_foreach (desc, pool->allocs) {
          if (desc->mem == ptr)
               break;
     }

     if (!desc) {
          D_ERROR( "Fusion/SHM: Cannot reallocate unknown chunk at %p (%s) from [%s:%d in %s()]!\n",
                   ptr, what, file, line, func );
          return NULL;
     }

     /* Remove the description in case the block moves. */
     direct_list_remove( &pool->allocs, &desc->link );

     /* Reallocate the memory block. */
     ret = fusion_shm_pool_reallocate( pool, ptr - sizeof(SHMemDesc), size + sizeof(SHMemDesc), false, &data );
     if (ret) {
          D_DERROR( ret, "Fusion/SHM: Could not reallocate from "_ZU" to "_ZU" bytes!\n",
                    desc->bytes + sizeof(SHMemDesc), size + sizeof(SHMemDesc) );
          fusion_skirmish_dismiss( &pool->lock );
          return NULL;
     }

     /* Fill description. */
     desc = fill_shmem_desc( data, size, func, file, line, _fusion_id( pool->shm->world ) );

     D_DEBUG_AT( Fusion_SHM, "Reallocating "_ZUn(9)" bytes at %p [%8lu] in %-30s [%3lx] (%s: %u) '%s'\n",
                 desc->bytes, desc->mem, (unsigned long) desc->mem - (unsigned long) pool->heap,
                 desc->func, desc->fid, desc->file, desc->line, what );

     /* Add description to list. */
     direct_list_append( &pool->allocs, &desc->link );

     /* Unlock the pool. */
     fusion_skirmish_dismiss( &pool->lock );

     return data + sizeof(SHMemDesc);
}

char *
fusion_dbg_shstrdup( FusionSHMPoolShared *pool,
                     const char          *file,
                     int                  line,
                     const char          *func,
                     const char          *string )
{
     DirectResult  ret;
     SHMemDesc    *desc;
     void         *data = NULL;
     int           length;

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( file != NULL );
     D_ASSERT( line > 0 );
     D_ASSERT( func != NULL );
     D_ASSERT( string != NULL );

     if (!pool->debug)
          return fusion_shstrdup( pool, string );

     length = strlen( string ) + 1;

     /* Lock the pool. */
     ret = fusion_skirmish_prevail( &pool->lock );
     if (ret) {
          D_DERROR( ret, "Fusion/SHM: Could not lock shared memory pool!\n" );
          return NULL;
     }

     /* Allocate memory from the pool. */
     ret = fusion_shm_pool_allocate( pool, length + sizeof(SHMemDesc), false, false, &data );
     if (ret) {
          D_DERROR( ret, "Fusion/SHM: Could not allocate "_ZU" bytes from pool!\n", length + sizeof(SHMemDesc) );
          fusion_skirmish_dismiss( &pool->lock );
          return NULL;
     }

     /* Fill description. */
     desc = fill_shmem_desc( data, length, func, file, line, _fusion_id( pool->shm->world ) );

     D_DEBUG_AT( Fusion_SHM, "Allocating "_ZUn(9)" bytes at %p [%8lu] in %-30s [%3lx] (%s: %u) <- \"%s\"\n",
                 desc->bytes, desc->mem, (unsigned long) desc->mem - (unsigned long) pool->heap,
                 desc->func, desc->fid, desc->file, desc->line, string );

     D_DEBUG_AT( Fusion_SHM, "  -> allocs %p\n", pool->allocs );

     /* Add description to list. */
     direct_list_append( &pool->allocs, &desc->link );

     /* Unlock the pool. */
     fusion_skirmish_dismiss( &pool->lock );

     /* Copy string content. */
     direct_memcpy( data + sizeof(SHMemDesc), string, length );

     return data + sizeof(SHMemDesc);
}

void
fusion_dbg_shfree( FusionSHMPoolShared *pool,
                   const char          *file,
                   int                  line,
                   const char          *func,
                   const char          *what,
                   void                *ptr )
{
     DirectResult  ret;
     SHMemDesc    *desc;

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( file != NULL );
     D_ASSERT( line > 0 );
     D_ASSERT( func != NULL );
     D_ASSERT( what != NULL );
     D_ASSERT( ptr != NULL );

     if (!pool->debug)
          return fusion_shfree( pool, ptr );

     /* Lock the pool. */
     ret = fusion_skirmish_prevail( &pool->lock );
     if (ret) {
          D_DERROR( ret, "Fusion/SHM: Could not lock shared memory pool!\n" );
          return;
     }

     /* Lookup the corresponding description. */
     direct_list_foreach (desc, pool->allocs) {
          if (desc->mem == ptr)
               break;
     }

     if (!desc) {
          D_ERROR( "Fusion/SHM: Cannot free unknown chunk at %p (%s) from [%s:%d in %s()]!\n",
                   ptr, what, file, line, func );
          return;
     }

     D_DEBUG_AT( Fusion_SHM, "Freeing "_ZUn(9)" bytes at %p [%8lu] in %-30s [%3lx] (%s: %u) '%s'\n",
                 desc->bytes, desc->mem, (unsigned long) desc->mem - (unsigned long) pool->heap,
                 desc->func, desc->fid, desc->file, desc->line, what );

     /* Remove the description. */
     direct_list_remove( &pool->allocs, &desc->link );

     /* Free the memory block. */
     fusion_shm_pool_deallocate( pool, ptr - sizeof(SHMemDesc), false );

     /* Unlock the pool. */
     fusion_skirmish_dismiss( &pool->lock );
}

#else /* FUSION_BUILD_MULTI */

void *
fusion_dbg_shmalloc( FusionSHMPoolShared *pool,
                     const char          *file,
                     int                  line,
                     const char          *func,
                     size_t               size )
{
     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( size > 0 );

     D_DEBUG_AT( Fusion_SHM, "Allocating "_ZUn(9)" bytes in %-30s (%s: %d)\n", size, func, file, line );

     if (pool->debug)
          return direct_dbg_malloc( file, line, func, size );

     return malloc( size );
}

void *
fusion_dbg_shcalloc( FusionSHMPoolShared *pool,
                     const char          *file,
                     int                  line,
                     const char          *func,
                     size_t               nmemb,
                     size_t               size )
{
     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( nmemb > 0 );
     D_ASSERT( size > 0 );


     D_DEBUG_AT( Fusion_SHM, "Allocating "_ZUn(9)" bytes in %-30s (%s: %d)\n", size, func, file, line );

     if (pool->debug)
          return direct_dbg_calloc( file, line, func, nmemb, size );

     return calloc( nmemb, size );
}

void *
fusion_dbg_shrealloc( FusionSHMPoolShared *pool,
                      const char          *file,
                      int                  line,
                      const char          *func,
                      const char          *what,
                      void                *ptr,
                      size_t               size )
{
     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );

     D_DEBUG_AT( Fusion_SHM, "Reallocating "_ZUn(9)" bytes in %-30s (%s: %d) '%s'\n", size, func, file, line, what );

     if (pool->debug)
          return direct_dbg_realloc( file, line, func, what, ptr, size );

     return realloc( ptr, size );
}

char *
fusion_dbg_shstrdup( FusionSHMPoolShared *pool,
                     const char          *file,
                     int                  line,
                     const char          *func,
                     const char          *string )
{
     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( string != NULL );

     D_DEBUG_AT( Fusion_SHM, "Allocating "_ZUn(9)" bytes in %-30s (%s: %d) <- \"%s\"\n",
                 strlen( string ), func, file, line, string );

     if (pool->debug)
          return direct_dbg_strdup( file, line, func, string );

     return strdup( string );
}

void
fusion_dbg_shfree( FusionSHMPoolShared *pool,
                   const char          *file,
                   int                  line,
                   const char          *func,
                   const char          *what,
                   void                *ptr )
{
     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( ptr != NULL );

     D_DEBUG_AT( Fusion_SHM, "Freeing bytes in %-30s (%s: %d) '%s'\n", func, file, line, what );

     if (pool->debug)
          direct_dbg_free( file, line, func, what, ptr );
     else
          free( ptr );
}

#endif /* FUSION_BUILD_MULTI */

#else /* DIRECT_BUILD_DEBUGS */

#if FUSION_BUILD_MULTI

void
fusion_print_memleaks( FusionSHMPoolShared *pool )
{
}

#endif /* FUSION_BUILD_MULTI */

void *
fusion_dbg_shmalloc( FusionSHMPoolShared *pool,
                     const char          *file,
                     int                  line,
                     const char          *func,
                     size_t               size )
{
     return fusion_shmalloc( pool, size );
}

void *
fusion_dbg_shcalloc( FusionSHMPoolShared *pool,
                     const char          *file,
                     int                  line,
                     const char          *func,
                     size_t               nmemb,
                     size_t               size )
{
     return fusion_shcalloc( pool, nmemb, size );
}

void *
fusion_dbg_shrealloc( FusionSHMPoolShared *pool,
                      const char          *file,
                      int                  line,
                      const char          *func,
                      const char          *what,
                      void                *ptr,
                      size_t               size )
{
     return fusion_shrealloc( pool, ptr, size );
}

char *
fusion_dbg_shstrdup( FusionSHMPoolShared *pool,
                     const char          *file,
                     int                  line,
                     const char          *func,
                     const char          *string )
{
     return fusion_shstrdup( pool, string );
}

void
fusion_dbg_shfree( FusionSHMPoolShared *pool,
                   const char          *file,
                   int                  line,
                   const char          *func,
                   const char          *what,
                   void                *ptr )
{
     return fusion_shfree( pool, ptr );
}

#endif /* DIRECT_BUILD_DEBUGS */

#if FUSION_BUILD_MULTI

void *
fusion_shmalloc( FusionSHMPoolShared *pool,
                 size_t               size )
{
     void *data = NULL;

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( size > 0 );

     if (fusion_shm_pool_allocate( pool, size, false, true, &data ))
          return NULL;

     D_ASSERT( data != NULL );

     return data;
}

void *
fusion_shcalloc( FusionSHMPoolShared *pool,
                 size_t               nmemb,
                 size_t               size )
{
     void *data = NULL;

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( nmemb > 0 );
     D_ASSERT( size > 0 );

     if (fusion_shm_pool_allocate( pool, nmemb * size, true, true, &data ))
          return NULL;

     D_ASSERT( data != NULL );

     return data;
}

void *
fusion_shrealloc( FusionSHMPoolShared *pool,
                  void                *ptr,
                  size_t               size )
{
     void *data = NULL;

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );

     if (!ptr)
          return fusion_shmalloc( pool, size );

     if (!size) {
          fusion_shfree( pool, ptr );
          return NULL;
     }

     if (fusion_shm_pool_reallocate( pool, ptr, size, true, &data ))
          return NULL;

     D_ASSERT( data != NULL || size == 0 );

     return data;
}

char *
fusion_shstrdup( FusionSHMPoolShared *pool,
                 const char          *string )
{
     int   len;
     void *data = NULL;

     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( string != NULL );

     len = strlen( string ) + 1;

     if (fusion_shm_pool_allocate( pool, len, false, true, &data ))
          return NULL;

     D_ASSERT( data != NULL );

     direct_memcpy( data, string, len );

     return data;
}

void
fusion_shfree( FusionSHMPoolShared *pool,
               void                *ptr )
{
     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( ptr != NULL );

     fusion_shm_pool_deallocate( pool, ptr, true );
}

#else /* FUSION_BUILD_MULTI */

void *
fusion_shmalloc( FusionSHMPoolShared *pool,
                 size_t               size )
{
     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( size > 0 );

     return malloc( size );
}

void *
fusion_shcalloc( FusionSHMPoolShared *pool,
                 size_t               nmemb,
                 size_t               size )
{
     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( nmemb > 0 );
     D_ASSERT( size > 0 );

     return calloc( nmemb, size );
}

void *
fusion_shrealloc( FusionSHMPoolShared *pool,
                  void                *ptr,
                  size_t               size )
{
     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );

     return realloc( ptr, size );
}

char *
fusion_shstrdup( FusionSHMPoolShared *pool,
                 const char          *string )
{
     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( string != NULL );

     return strdup( string );
}

void
fusion_shfree( FusionSHMPoolShared *pool,
               void                *ptr )
{
     D_MAGIC_ASSERT( pool, FusionSHMPoolShared );
     D_ASSERT( ptr != NULL );

     free( ptr );
}

#endif /* FUSION_BUILD_MULTI */

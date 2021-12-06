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

#include <direct/filesystem.h>
#include <direct/memcpy.h>
#include <fusion/conf.h>
#include <fusion/shmalloc.h>
#include <fusion/shm/shm_internal.h>

D_DEBUG_DOMAIN( Fusion_SHMHeap, "Fusion/SHMHeap", "Fusion Shared Memory Heap" );

/**********************************************************************************************************************/

/* Number of contiguous free blocks allowed to build up at the end of memory before being returned to the system. */
#define FINAL_FREE_BLOCKS 8

/* Address to block number and vice versa. */
#define BLOCK(A)   (((char*) (A) - heap->heapbase) / BLOCKSIZE + 1)
#define ADDRESS(B) ((void*) (((B) - 1) * BLOCKSIZE + heap->heapbase))

/*
 * Aligned allocation.
 */
static void *
align( shmalloc_heap *heap,
       size_t         size )
{
     void          *result;
     unsigned long  adj;

     D_DEBUG_AT( Fusion_SHMHeap, "%s( %p, "_ZU" )\n", __FUNCTION__, heap, size );

     D_MAGIC_ASSERT( heap, shmalloc_heap );

     result = __shmalloc_brk( heap, size );

     adj = (unsigned long) result % BLOCKSIZE;
     if (adj != 0) {
          adj = BLOCKSIZE - adj;
          __shmalloc_brk( heap, adj );
          result = (char*) result + adj;
     }

     return result;
}

/*
 * Get neatly aligned memory, initializing or growing the heap info table as necessary.
 */
static void *
morecore( shmalloc_heap *heap,
          size_t         size )
{
     void          *result;
     shmalloc_info *newinfo, *oldinfo;
     size_t         newsize;

     D_DEBUG_AT( Fusion_SHMHeap, "%s( %p, "_ZU" )\n", __FUNCTION__, heap, size );

     D_MAGIC_ASSERT( heap, shmalloc_heap );

     result = align( heap, size );
     if (result == NULL)
          return NULL;

     /* Check if we need to grow the info table. */
     if ((size_t) BLOCK( (char*) result + size ) > heap->heapsize) {
          newsize = heap->heapsize;

          while ((size_t) BLOCK( (char*) result + size ) > newsize)
               newsize *= 2;

          newinfo = (shmalloc_info*) align( heap, newsize * sizeof(shmalloc_info) );
          if (newinfo == NULL) {
               __shmalloc_brk( heap, -size );
               return NULL;
          }

          direct_memcpy( newinfo, heap->heapinfo, heap->heapsize * sizeof(shmalloc_info) );

          memset( newinfo + heap->heapsize, 0, (newsize - heap->heapsize) * sizeof(shmalloc_info) );

          oldinfo = heap->heapinfo;

          newinfo[BLOCK( oldinfo )].busy.type = 0;
          newinfo[BLOCK( oldinfo )].busy.info.size = BLOCKIFY( heap->heapsize * sizeof(shmalloc_info) );

          heap->heapinfo = newinfo;

          _fusion_shfree( heap, oldinfo );

          heap->heapsize = newsize;
     }

     heap->heaplimit = BLOCK( (char*) result + size );

     return result;
}

/**********************************************************************************************************************/

void *
_fusion_shmalloc( shmalloc_heap *heap,
                  size_t         size )
{
     void        *result;
     size_t       i, block, blocks, lastblocks, start;
     struct list *next;

     D_DEBUG_AT( Fusion_SHMHeap, "%s( %p, "_ZU" )\n", __FUNCTION__, heap, size );

     D_MAGIC_ASSERT( heap, shmalloc_heap );

     /* Some programs will call shmalloc with size = 0. We let them pass. */
     if (size == 0)
          return NULL;

     if (size < sizeof(struct list))
          size = sizeof(struct list);

     /* Determine the allocation policy based on the request size. */
     if (size <= BLOCKSIZE / 2) {
          /* Small allocation to receive a fragment of a block.
             Determine the logarithm to base two of the fragment size. */
          size_t log = 1;
          --size;
          while ((size /= 2) != 0)
               ++log;

          /* Look in the fragment lists for a free fragment of the desired size. */
          next = heap->fraghead[log].next;
          if (next != NULL) {
               /* There are free fragments of this size.
                  Pop a fragment out of the fragment list and return it. */
               result = next;
               next->prev->next = next->next;
               if (next->next != NULL)
                    next->next->prev = next->prev;
               block = BLOCK( result );
               if (--heap->heapinfo[block].busy.info.frag.nfree != 0)
                    heap->heapinfo[block].busy.info.frag.first = ((char*) next->next - (char*) NULL) % BLOCKSIZE >> log;

               /* Update the statistics. */
               heap->chunks_used++;
               heap->bytes_used += 1 << log;
               heap->chunks_free--;
               heap->bytes_free -= 1 << log;
          }
          else {
               /* No free fragments of the desired size.
                  Get a new block and break it into fragments, returning the first. */
               result = _fusion_shmalloc( heap, BLOCKSIZE );
               if (result == NULL)
                    return NULL;

               heap->fragblocks[log]++;

               /* Link all fragments but the first into the free list. */
               for (i = 1; i < BLOCKSIZE >> log; ++i) {
                    next = (struct list*) ((char*) result + (i << log));
                    next->next = heap->fraghead[log].next;
                    next->prev = &heap->fraghead[log];
                    next->prev->next = next;
                    if (next->next != NULL)
                         next->next->prev = next;
               }

               /* Initialize the nfree and first counters for this block. */
               block = BLOCK( result );
               heap->heapinfo[block].busy.type = log;
               heap->heapinfo[block].busy.info.frag.nfree = i - 1;
               heap->heapinfo[block].busy.info.frag.first = i - 1;

               heap->chunks_free += (BLOCKSIZE >> log) - 1;
               heap->bytes_free += BLOCKSIZE - (1 << log);
               heap->bytes_used -= BLOCKSIZE - (1 << log);
          }
     }
     else {
          /* Large allocation to receive one or more blocks.
             Search the free list in a circle starting at the last place visited.
             If we loop completely around without finding a large enough space,
             we will have to get more memory from the system. */
          blocks = BLOCKIFY( size );
          start = block = heap->heapindex;
          while (heap->heapinfo[block].free.size < blocks) {
               block = heap->heapinfo[block].free.next;
               if (block == start) {
                    /* Need to get more from the system.
                       Check to see if the new core will be contiguous with the final free block.
                       If so we don't need to get as much. */
                    block = heap->heapinfo[0].free.prev;
                    lastblocks = heap->heapinfo[block].free.size;
                    if (heap->heaplimit != 0 && block + lastblocks == heap->heaplimit &&
                        __shmalloc_brk( heap, 0 ) == ADDRESS( block + lastblocks ) &&
                        (morecore( heap, (blocks - lastblocks) * BLOCKSIZE) ) != NULL) {
                         block = heap->heapinfo[0].free.prev;
                         heap->heapinfo[block].free.size += blocks - lastblocks;
                         heap->bytes_free += (blocks - lastblocks) * BLOCKSIZE;
                         continue;
                    }
                    result = morecore( heap, blocks * BLOCKSIZE );
                    if (result == NULL)
                         return NULL;
                    block = BLOCK( result );
                    heap->heapinfo[block].busy.type = 0;
                    heap->heapinfo[block].busy.info.size = blocks;
                    heap->chunks_used++;
                    heap->bytes_used += blocks * BLOCKSIZE;
                    return result;
               }
          }

          /* At this point we have found a suitable free list entry.
             Figure out how to remove what we need from the list. */
          result = ADDRESS( block );
          if (heap->heapinfo[block].free.size > blocks) {
               /* The block we found has a bit left over, so relink the tail end back into the free list. */
               heap->heapinfo[block + blocks].free.size = heap->heapinfo[block].free.size - blocks;
               heap->heapinfo[block + blocks].free.next = heap->heapinfo[block].free.next;
               heap->heapinfo[block + blocks].free.prev = heap->heapinfo[block].free.prev;
               heap->heapinfo[heap->heapinfo[block].free.next].free.prev = block + blocks;
               heap->heapinfo[heap->heapinfo[block].free.prev].free.next = block + blocks;
               heap->heapindex = block + blocks;
          }
          else {
               /* The block exactly matches our requirements, so just remove it from the list. */
               heap->heapinfo[heap->heapinfo[block].free.next].free.prev = heap->heapinfo[block].free.prev;
               heap->heapinfo[heap->heapinfo[block].free.prev].free.next = heap->heapinfo[block].free.next;
               heap->heapindex = heap->heapinfo[block].free.next;
               heap->chunks_free--;
          }

          heap->heapinfo[block].busy.type = 0;
          heap->heapinfo[block].busy.info.size = blocks;
          heap->chunks_used++;
          heap->bytes_used += blocks * BLOCKSIZE;
          heap->bytes_free -= blocks * BLOCKSIZE;
     }

     return result;
}

void *
_fusion_shrealloc( shmalloc_heap *heap,
                   void          *ptr,
                   size_t         size )
{
     void   *result;
     int     type;
     size_t  block, blocks, oldlimit;

     D_DEBUG_AT( Fusion_SHMHeap, "%s( %p, %p, "_ZU" )\n", __FUNCTION__, heap, ptr, size );

     D_MAGIC_ASSERT( heap, shmalloc_heap );

     if (ptr == NULL)
          return _fusion_shmalloc( heap, size );
     else if (size == 0) {
          _fusion_shfree( heap, ptr );
          return NULL;
     }

     block = BLOCK( ptr );

     type = heap->heapinfo[block].busy.type;
     switch (type) {
          case 0:
               /* Maybe reallocate a large block to a small fragment. */
               if (size <= BLOCKSIZE / 2) {
                    result = _fusion_shmalloc( heap, size );
                    if (result != NULL) {
                         direct_memcpy( result, ptr, size );
                         _fusion_shfree( heap, ptr );
                         return result;
                    }
               }

               /* The new size is a large allocation as well; see if we can hold it in place. */
               blocks = BLOCKIFY( size );
               if (blocks < heap->heapinfo[block].busy.info.size) {
                    /* The new size is smaller; return excess memory to the free list. */
                    heap->heapinfo[block + blocks].busy.type = 0;
                    heap->heapinfo[block + blocks].busy.info.size = heap->heapinfo[block].busy.info.size - blocks;
                    heap->heapinfo[block].busy.info.size = blocks;
                    _fusion_shfree( heap, ADDRESS( block + blocks ) );
                    result = ptr;
               }
               else if (blocks == heap->heapinfo[block].busy.info.size)
                    /* No size change necessary. */
                    result = ptr;
               else {
                    /* Won't fit, so allocate a new region that will.
                       Free the old region first in case there is sufficient adjacent space to grow without moving. */
                    blocks = heap->heapinfo[block].busy.info.size;
                    oldlimit = heap->heaplimit;
                    heap->heaplimit = 0;
                    _fusion_shfree( heap, ptr );
                    heap->heaplimit = oldlimit;
                    result = _fusion_shmalloc( heap, size );
                    if (result == NULL) {
                         /* We have to unfree the thing we just freed.
                            Unfortunately it might have been coalesced with its neighbors. */
                         if (heap->heapindex == block)
                              _fusion_shmalloc( heap, blocks * BLOCKSIZE );
                         else {
                              void *previous = _fusion_shmalloc( heap, (block - heap->heapindex) * BLOCKSIZE );
                              _fusion_shmalloc( heap, blocks * BLOCKSIZE );
                              _fusion_shfree( heap, previous );
                         }
                         return NULL;
                    }
                    if (ptr != result)
                         direct_memmove( result, ptr, blocks * BLOCKSIZE );
               }
               break;

          default:
               /* Old size is a fragment; type is logarithm to base two of the fragment size. */
               if (size > (size_t) (1 << (type - 1)) && size <= (size_t) (1 << type))
                    /* The new size is the same kind of fragment. */
                    result = ptr;
               else {
                    /* The new size is different; allocate a new space, copy the lesser of the new size and the old. */
                    result = _fusion_shmalloc( heap, size );
                    if (result == NULL)
                         return NULL;
                    direct_memcpy( result, ptr, MIN( size, (size_t) 1 << type ) );
                    _fusion_shfree( heap, ptr );
               }
               break;
     }

     return result;
}

void
_fusion_shfree( shmalloc_heap *heap,
                void          *ptr )
{
     int          type;
     size_t       i, block, blocks;
     struct list *prev, *next;

     D_DEBUG_AT( Fusion_SHMHeap, "%s( %p, %p )\n", __FUNCTION__, heap, ptr );

     D_MAGIC_ASSERT( heap, shmalloc_heap );

     if (ptr == NULL)
          return;

     block = BLOCK( ptr );

     type = heap->heapinfo[block].busy.type;
     switch (type) {
          case 0:
               /* Get as many statistics as early as we can. */
               heap->chunks_used--;
               heap->bytes_used -= heap->heapinfo[block].busy.info.size * BLOCKSIZE;
               heap->bytes_free += heap->heapinfo[block].busy.info.size * BLOCKSIZE;

               /* Find the free cluster previous to this one in the free list. */
               i = heap->heapindex;
               if (i > block)
                    while (i > block)
                         i = heap->heapinfo[i].free.prev;
               else {
                    do
                         i = heap->heapinfo[i].free.next;
                    while (i > 0 && i < block);
                    i = heap->heapinfo[i].free.prev;
               }

               /* Determine how to link this block into the free list. */
               if (block == i + heap->heapinfo[i].free.size) {
                    /* Coalesce this block with its predecessor. */
                    heap->heapinfo[i].free.size += heap->heapinfo[block].busy.info.size;
                    block = i;
               }
               else {
                    /* Really link this block back into the free list. */
                    heap->heapinfo[block].free.size = heap->heapinfo[block].busy.info.size;
                    heap->heapinfo[block].free.next = heap->heapinfo[i].free.next;
                    heap->heapinfo[block].free.prev = i;
                    heap->heapinfo[i].free.next = block;
                    heap->heapinfo[heap->heapinfo[block].free.next].free.prev = block;
                    heap->chunks_free++;
               }

               /* Now that the block is linked in, see if we can coalesce it with its successor
                  (by deleting its successor from the list and adding in its size). */
               if (block + heap->heapinfo[block].free.size == heap->heapinfo[block].free.next) {
                    heap->heapinfo[block].free.size += heap->heapinfo[heap->heapinfo[block].free.next].free.size;
                    heap->heapinfo[block].free.next = heap->heapinfo[heap->heapinfo[block].free.next].free.next;
                    heap->heapinfo[heap->heapinfo[block].free.next].free.prev = block;
                    heap->chunks_free--;
               }

               blocks = heap->heapinfo[block].free.size;

               /* Punch a hole into the tmpfs file to really free RAM. */
               if (fusion_config->madv_remove)
                    madvise( ADDRESS( block ), blocks * BLOCKSIZE, MADV_REMOVE );

               /* Now see if we can truncate the end. */
               if (blocks >= FINAL_FREE_BLOCKS && block + blocks == heap->heaplimit &&
                   __shmalloc_brk( heap, 0 ) == ADDRESS( block + blocks )) {
                    size_t bytes = blocks * BLOCKSIZE;
                    heap->heaplimit -= blocks;
                    __shmalloc_brk( heap, -bytes );
                    heap->heapinfo[heap->heapinfo[block].free.prev].free.next = heap->heapinfo[block].free.next;
                    heap->heapinfo[heap->heapinfo[block].free.next].free.prev = heap->heapinfo[block].free.prev;
                    block = heap->heapinfo[block].free.prev;
                    heap->chunks_free--;
                    heap->bytes_free -= bytes;
               }

               /* Set the next search to begin at this block. */
               heap->heapindex = block;
               break;

          default:
               /* Do some of the statistics. */
               heap->chunks_used--;
               heap->bytes_used -= 1 << type;
               heap->chunks_free++;
               heap->bytes_free += 1 << type;

               /* Get the address of the first free fragment in this block. */
               prev = (struct list*) ((char*) ADDRESS( block ) + (heap->heapinfo[block].busy.info.frag.first << type));

               if (heap->heapinfo[block].busy.info.frag.nfree == (BLOCKSIZE >> type) - 1 && heap->fragblocks[type] > 1)
               {
                    /* If all fragments of this block are free, remove them from the fragment list, free whole block. */
                    heap->fragblocks[type]--;
                    next = prev;
                    for (i = 1; i < BLOCKSIZE >> type; ++i)
                         next = next->next;
                    prev->prev->next = next;
                    if (next != NULL)
                         next->prev = prev->prev;
                    heap->heapinfo[block].busy.type = 0;
                    heap->heapinfo[block].busy.info.size = 1;

                    /* Keep the statistics accurate. */
                    heap->chunks_used++;
                    heap->bytes_used += BLOCKSIZE;
                    heap->chunks_free -= BLOCKSIZE >> type;
                    heap->bytes_free -= BLOCKSIZE;

                    _fusion_shfree( heap, ADDRESS( block ) );
               }
               else if (heap->heapinfo[block].busy.info.frag.nfree != 0) {
                    /* If some fragments of this block are free, link this fragment into the fragment list
                       after the first free fragment of this block. */
                    next = (struct list*) ptr;
                    next->next = prev->next;
                    next->prev = prev;
                    prev->next = next;
                    if (next->next != NULL)
                         next->next->prev = next;
                    heap->heapinfo[block].busy.info.frag.nfree++;
               }
               else {
                    /* No fragments of this block are free, so link this fragment into the fragment list and
                       announce that it is the first free fragment of this block. */
                    prev = (struct list*) ptr;
                    heap->heapinfo[block].busy.info.frag.nfree = 1;
                    heap->heapinfo[block].busy.info.frag.first = ((char*) ptr - (char*) NULL) % BLOCKSIZE >> type;
                    prev->next = heap->fraghead[type].next;
                    prev->prev = &heap->fraghead[type];
                    prev->prev->next = prev;
                    if (prev->next != NULL)
                         prev->next->prev = prev;
               }
               break;
     }
}

/**********************************************************************************************************************/

DirectResult
__shmalloc_init_heap( FusionSHM  *shm,
                      const char *filename,
                      void       *addr_base,
                      int         space,
                      int        *ret_size )
{
     DirectResult   ret;
     int            size;
     int            heapsize = (space + BLOCKSIZE - 1) / BLOCKSIZE;
     DirectFile     fd;
     shmalloc_heap *heap = NULL;

     D_DEBUG_AT( Fusion_SHMHeap, "%s( %p, '%s', %p, %d, %p )\n", __FUNCTION__,
                 shm, filename, addr_base, space, ret_size );

     D_MAGIC_ASSERT( shm, FusionSHM );
     D_MAGIC_ASSERT( shm->shared, FusionSHMShared );
     D_ASSERT( shm->shared->tmpfs[0] != 0 );
     D_ASSERT( filename != NULL );
     D_ASSERT( addr_base != NULL );
     D_ASSERT( ret_size != NULL );

     size = BLOCKALIGN( sizeof(shmalloc_heap) ) + BLOCKALIGN( heapsize * sizeof(shmalloc_info) );

     D_DEBUG_AT( Fusion_SHMHeap, "  -> opening shared memory file '%s'...\n", filename );

     /* Open the virtual file. */
     ret = direct_file_open( &fd, filename, O_RDWR | O_CREAT | O_TRUNC, 0660 );
     if (ret) {
          D_DERROR( ret, "Fusion/SHMHeap: Could not open shared memory file '%s'!\n", filename );
          return ret;
     }

     if (fusion_config->shmfile_gid != -1) {
          if (direct_file_chown( &fd, -1, fusion_config->shmfile_gid ))
               D_WARN( "changing owner on %s failed", filename );
     }

     direct_file_chmod( &fd, fusion_config->secure_fusion ? 0640 : 0660 );

     if (fusion_config->madv_remove)
          ret = direct_file_truncate( &fd, size + space );
     else
          ret = direct_file_truncate( &fd, size );

     if (ret) {
          D_DERROR( ret, "Fusion/SHMHeap: Could not truncate shared memory file '%s'!\n", filename );
          goto error;
     }

     D_DEBUG_AT( Fusion_SHMHeap, "  -> mapping shared memory file... (%d bytes)\n", size );

     /* Map it shared. */
     ret = direct_file_map( &fd, addr_base, 0, size + space, DFP_READ | DFP_WRITE, (void**) &heap );
     if (ret) {
          D_DERROR( ret, "Fusion/SHMHeap: Could not mmap shared memory file '%s'!\n", filename );
          goto error;
     }

     if (heap != addr_base) {
          ret = DR_FUSION;
          D_ERROR( "Fusion/SHMHeap: The mmap returned address (%p) differs from requested (%p)!\n", heap, addr_base );
          goto error;
     }

     direct_file_close( &fd );

     D_DEBUG_AT( Fusion_SHMHeap, "  -> done\n" );

     heap->size     = size;
     heap->heapsize = heapsize;
     heap->heapinfo = (void*) heap + BLOCKALIGN( sizeof(shmalloc_heap) );
     heap->heapbase = (char*) heap->heapinfo;

     direct_snputs( heap->filename, filename, sizeof(heap->filename) );

     D_MAGIC_SET( heap, shmalloc_heap );

     *ret_size = size;

     return DR_OK;

error:
     if (heap)
          direct_file_unmap( heap, size + space );

     direct_file_close( &fd );
     direct_unlink( filename );

     return ret;
}

DirectResult
__shmalloc_join_heap( FusionSHM  *shm,
                      const char *filename,
                      void       *addr_base,
                      int         size,
                      bool        write )
{
     DirectResult   ret;
     DirectFile     fd;
     shmalloc_heap *heap = NULL;
     int            open_flags = write ? O_RDWR : O_RDONLY;
     int            perms = DFP_READ;
     int            heapsize   = (size + BLOCKSIZE - 1) / BLOCKSIZE;

     if (write)
          perms |= DFP_WRITE;

     D_DEBUG_AT( Fusion_SHMHeap, "%s( %p, '%s', %p, %d )\n", __FUNCTION__, shm, filename, addr_base, size );

     D_MAGIC_ASSERT( shm, FusionSHM );
     D_MAGIC_ASSERT( shm->shared, FusionSHMShared );
     D_ASSERT( shm->shared->tmpfs[0] != 0 );
     D_ASSERT( filename != NULL );
     D_ASSERT( addr_base != NULL );

     D_DEBUG_AT( Fusion_SHMHeap, "  -> opening shared memory file '%s'...\n", filename );

     /* Open the virtual file. */
     ret = direct_file_open( &fd, filename, open_flags, 0 );
     if (ret) {
          D_DERROR( ret, "Fusion/SHMHeap: Could not open shared memory file '%s'!\n", filename );
          return ret;
     }

     size += BLOCKALIGN( sizeof(shmalloc_heap) ) + BLOCKALIGN( heapsize * sizeof(shmalloc_info) );

     D_DEBUG_AT( Fusion_SHMHeap, "  -> mapping shared memory file... (%d bytes)\n", size );

     /* Map it shared. */
     ret = direct_file_map( &fd, addr_base, 0, size, perms, (void**) &heap );
     if (ret) {
          D_DERROR( ret, "Fusion/SHMHeap: Could not mmap shared memory file '%s'!\n", filename );
          goto error;
     }

     if (heap != addr_base) {
          ret = DR_FUSION;
          D_ERROR( "Fusion/SHMHeap: The mmap returned address (%p) differs from requested (%p)!\n", heap, addr_base );
          goto error;
     }

     direct_file_close( &fd );

     D_MAGIC_ASSERT( heap, shmalloc_heap );

     D_DEBUG_AT( Fusion_SHMHeap, "  -> done\n" );

     return DR_OK;

error:
     if (heap)
          direct_file_unmap( heap, size );

     direct_file_close( &fd );

     return ret;
}

void *
__shmalloc_brk( shmalloc_heap *heap,
                int            increment )
{
     DirectResult ret;
     DirectFile   fd;

     D_DEBUG_AT( Fusion_SHMHeap, "%s( %p, %d )\n", __FUNCTION__, heap, increment );

     D_MAGIC_ASSERT( heap, shmalloc_heap );
     D_MAGIC_ASSERT( heap->pool, FusionSHMPoolShared );

     if (increment) {
          int new_size = heap->size + increment;

          if (new_size > heap->pool->max_size) {
               D_WARN( "maximum shared memory size exceeded" );
               fusion_print_memleaks( heap->pool );
               return NULL;
          }

          if (!fusion_config->madv_remove) {
               ret = direct_file_open( &fd, heap->filename, O_RDWR, 0 );
               if (ret) {
                    D_DERROR( ret, "Fusion/SHMHeap: Could not open shared memory file '%s'!\n", heap->filename );
                    return NULL;
               }

               ret = direct_file_truncate( &fd, new_size );
               if (ret) {
                    D_DERROR( ret, "Fusion/SHMHeap: Could not truncate shared memory file '%s'!\n", heap->filename );
                    return NULL;
               }

               direct_file_close( &fd );
          }

          heap->size = new_size;
     }

     return heap->pool->addr_base + heap->size - increment;
}

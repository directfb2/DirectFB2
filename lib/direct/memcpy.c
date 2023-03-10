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

#include <config.h>
#include <direct/log.h>
#include <direct/memcpy.h>
#include <direct/messages.h>

#if DIRECT_BUILD_MEMCPY_PROBING

#include <direct/clock.h>
#include <direct/debug.h>
#include <direct/mem.h>

D_DEBUG_DOMAIN( Direct_Memcpy, "Direct/Memcpy", "Direct Memcpy routines" );

#endif

/**********************************************************************************************************************/

#if SIZEOF_LONG == 8

static void *
generic64_memcpy( void       *to,
                  const void *from,
                  size_t      len )
{
     u8       *d = (u8*) to;
     const u8 *s = (const u8*) from;
     size_t    n;

     if (len >= 128) {
          unsigned long delta;

          /* Align destination to 8-byte boundary. */
          delta = (unsigned long) d & 7;
          if (delta) {
               len -= 8 - delta;

               if ((unsigned long) d & 1) {
                    *d++ = *s++;
               }
               if ((unsigned long) d & 2) {
                    *((u16*)d) = *((const u16*) s);
                    d += 2; s += 2;
               }
               if ((unsigned long) d & 4) {
                    *((u32*)d) = *((const u32*) s);
                    d += 4; s += 4;
               }
          }

          n    = len >> 6;
          len &= 63;

          for (; n; n--) {
               ((u64*) d)[0] = ((const u64*) s)[0];
               ((u64*) d)[1] = ((const u64*) s)[1];
               ((u64*) d)[2] = ((const u64*) s)[2];
               ((u64*) d)[3] = ((const u64*) s)[3];
               ((u64*) d)[4] = ((const u64*) s)[4];
               ((u64*) d)[5] = ((const u64*) s)[5];
               ((u64*) d)[6] = ((const u64*) s)[6];
               ((u64*) d)[7] = ((const u64*) s)[7];
               d += 64; s += 64;
          }
     }

     /* Now do the tail of the block. */
     if (len) {
          n = len >> 3;

          for (; n; n--) {
               *((u64*) d) = *((const u64*) s);
               d += 8; s += 8;
          }
          if (len & 4) {
               *((u32*) d) = *((const u32*) s);
               d += 4; s += 4;
          }
          if (len & 2)  {
               *((u16*) d) = *((const u16*) s);
               d += 2; s += 2;
          }
          if (len & 1)
               *d = *s;
     }

     return to;
}

#endif /* SIZEOF_LONG == 8 */

static void *
std_memcpy( void       *to,
            const void *from,
            size_t      len )
{
     return memcpy( to, from, len );
}

typedef void *(*memcpy_func)( void *to, const void *from, size_t len );

static struct {
     char                 *name;
     char                 *desc;
     memcpy_func           function;
     unsigned long long    time;
} memcpy_method[] =
{
     { NULL,        NULL,                     NULL,             0 },
     { "libc",      "libc memcpy()",          std_memcpy,       0 },
#if SIZEOF_LONG == 8
     { "generic64", "Generic 64bit memcpy()", generic64_memcpy, 0 },
#endif
     { NULL,        NULL,                     NULL,             0 }
};

memcpy_func direct_memcpy = std_memcpy;

void
direct_find_best_memcpy()
{
     int i;
#if DIRECT_BUILD_MEMCPY_PROBING
     #define BUFSIZE 1024
     #define ITERS 256
     unsigned long long t;
     char *buf1, *buf2;
     int iter, j, best = 0;
#endif

     if (direct_config->memcpy) {
          for (i = 1; memcpy_method[i].name; i++) {
               if (!strcmp( direct_config->memcpy, memcpy_method[i].name )) {
                    direct_memcpy = memcpy_method[i].function;

                    D_INFO( "Direct/Memcpy: Forced to use %s\n", memcpy_method[i].desc );

                    return;
               }
          }
     }

     /* Skipping the memcpy() probing saves library size and startup time. */

#if DIRECT_BUILD_MEMCPY_PROBING
     if (!(buf1 = D_MALLOC( BUFSIZE * 500 )))
          return;

     if (!(buf2 = D_MALLOC( BUFSIZE * 500 ))) {
          D_FREE( buf1 );
          return;
     }

     D_DEBUG_AT( Direct_Memcpy, "Benchmarking memcpy methods (smaller is better):\n" );

     for (i = 1; memcpy_method[i].name; i++) {
          t = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );

          for (iter = 0; iter < ITERS; iter++)
               for (j = 0; j < 500; j++)
                    memcpy_method[i].function( buf1 + j * BUFSIZE, buf2 + j * BUFSIZE, BUFSIZE );

          t = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC ) - t;
          memcpy_method[i].time = t;

          D_DEBUG_AT( Direct_Memcpy, "\t%-10s  %20lld\n", memcpy_method[i].name, t );

          if (best == 0 || t < memcpy_method[best].time)
               best = i;
     }

     if (best) {
          direct_memcpy = memcpy_method[best].function;

          D_INFO( "Direct/Memcpy: Using %s\n", memcpy_method[best].desc );
     }

     D_FREE( buf1 );
     D_FREE( buf2 );
#endif
}

void
direct_print_memcpy_routines()
{
     int i;

     direct_log_printf( NULL, "\nPossible values for memcpy option are:\n\n" );

     for (i = 1; memcpy_method[i].name; i++) {
          direct_log_printf( NULL, "  %-10s  %-27s\n", memcpy_method[i].name, memcpy_method[i].desc );
     }

     direct_log_printf( NULL, "\n" );
}

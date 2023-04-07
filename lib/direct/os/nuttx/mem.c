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

#include <direct/os/mem.h>

#if DIRECT_BUILD_SENTINELS

#include <direct/debug.h>

D_DEBUG_DOMAIN( Direct_Sentinels, "Direct/Sentinels", "Direct Sentinels" );

#endif /* DIRECT_BUILD_SENTINELS */

/**********************************************************************************************************************/

#if DIRECT_BUILD_SENTINELS

#define PREFIX_SENTINEL 8
#define SUFFIX_SENTINEL 8

#define TOTAL_SENTINEL  ((PREFIX_SENTINEL) + (SUFFIX_SENTINEL))

__attribute__((no_instrument_function))
static inline void
install_sentinels( void *p, size_t size )
{
     size_t  i;
     size_t *ps     = p;
     u8     *prefix = p;
     u8     *suffix = p + PREFIX_SENTINEL + size;

     D_DEBUG_AT( Direct_Sentinels, "%s( %p, "_ZU" )\n", __FUNCTION__, p, size );

     *ps = size;

     for (i = sizeof(size_t); i < PREFIX_SENTINEL; i++)
          prefix[i] = i;

     for (i = 0; i < SUFFIX_SENTINEL; i++)
          suffix[i] = i;
}

__attribute__((no_instrument_function))
static inline void
remove_sentinels( void *p )
{
     size_t  i;
     size_t *ps     = p;
     u8     *prefix = p;
     u8     *suffix = p + PREFIX_SENTINEL + *ps;

     D_DEBUG_AT( Direct_Sentinels, "%s( %p )\n", __FUNCTION__, p );

     for (i = sizeof(size_t); i < PREFIX_SENTINEL; i++)
          prefix[i] = 0;

     for (i = 0; i < SUFFIX_SENTINEL; i++)
          suffix[i] = 0;
}

__attribute__((no_instrument_function))
static inline void
check_sentinels( void *p )
{
     size_t  i;
     size_t *ps     = p;
     u8     *prefix = p;
     u8     *suffix = p + PREFIX_SENTINEL + *ps;

     for (i = sizeof(size_t); i < PREFIX_SENTINEL; i++) {
          if (prefix[i] != i)
               D_DEBUG_AT( Direct_Sentinels, "Sentinel error at prefix["_ZU"] (%u) of "_ZU" bytes allocation!\n",
                           i, prefix[i], *ps );
     }

     for (i = 0; i < SUFFIX_SENTINEL; i++) {
          if (suffix[i] != i)
               D_DEBUG_AT( Direct_Sentinels, "Sentinel error at suffix["_ZU"] (%u) of "_ZU" bytes allocation!\n",
                           i, suffix[i], *ps );
     }
}

__attribute__((no_instrument_function))
void *
direct_malloc( size_t bytes )
{
     void *p = bytes ? malloc( bytes + TOTAL_SENTINEL ) : NULL;

     if (!p)
          return NULL;

     install_sentinels( p, bytes );

     return p + PREFIX_SENTINEL;
}

__attribute__((no_instrument_function))
void *
direct_calloc( size_t count, size_t bytes)
{
     void *p = (count && bytes) ? calloc( 1, count * bytes + TOTAL_SENTINEL ) : NULL;

     if (!p)
          return NULL;

     install_sentinels( p, count * bytes );

     return p + PREFIX_SENTINEL;
}

__attribute__((no_instrument_function))
void *
direct_realloc( void *mem, size_t bytes )
{
     void *p = mem ? mem - PREFIX_SENTINEL : NULL;

     if (!mem)
          return direct_malloc( bytes );

     check_sentinels( p );

     if (!bytes) {
          direct_free( mem );
          return NULL;
     }

     p = realloc( p, bytes + TOTAL_SENTINEL );

     if (!p)
          return NULL;

     install_sentinels( p, bytes );

     return p + PREFIX_SENTINEL;
}

__attribute__((no_instrument_function))
char *
direct_strdup( const char *str )
{
     int   n = strlen( str );
     void *p = malloc( n+1 + TOTAL_SENTINEL );

     if (!p)
          return NULL;

     memcpy( p + PREFIX_SENTINEL, str, n + 1 );

     install_sentinels( p, n + 1 );

     return p + PREFIX_SENTINEL;
}

__attribute__((no_instrument_function))
void
direct_free( void *mem )
{
     void *p = mem ? mem - PREFIX_SENTINEL : NULL;

     if (p) {
          check_sentinels( p );

          remove_sentinels( p );

          free( p );
     }
}

#else /* DIRECT_BUILD_SENTINELS */

__attribute__((no_instrument_function))
void *
direct_malloc( size_t bytes )
{
     return malloc( bytes );
}

__attribute__((no_instrument_function))
void *
direct_calloc( size_t count, size_t bytes)
{
     return calloc( count, bytes );
}

__attribute__((no_instrument_function))
void *
direct_realloc( void *mem, size_t bytes )
{
     return realloc( mem, bytes );
}

__attribute__((no_instrument_function))
char *
direct_strdup( const char *str )
{
     return strdup( str );
}

__attribute__((no_instrument_function))
void
direct_free( void *mem )
{
     free( mem );
}

#endif /* DIRECT_BUILD_SENTINELS */

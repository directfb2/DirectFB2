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

#ifndef __DIRECT__OS__FILESYSTEM_H__
#define __DIRECT__OS__FILESYSTEM_H__

#include <direct/os/types.h>

/**********************************************************************************************************************/

typedef struct {
     char name[256];
} DirectEntry;

typedef enum {
     DFP_NONE  = 0x00000000,

     DFP_READ  = 0x00000001,
     DFP_WRITE = 0x00000002,

     DFP_ALL   = 0x00000003
} DirectFilePermission;

typedef enum {
     DFIF_NONE = 0x00000000,

     DFIF_SIZE = 0x00000001,

     DFIF_ALL  = 0x00000001
} DirectFileInfoFlags;

typedef struct {
     DirectFileInfoFlags flags;

     size_t              size;
} DirectFileInfo;

#define R_OK 4
#define W_OK 2
#define F_OK 0

/**********************************************************************************************************************/

DirectResult DIRECT_API direct_dir_get_current( char                  *buf,
                                                size_t                 length );

DirectResult DIRECT_API direct_dir_change     ( const char            *name );

DirectResult DIRECT_API direct_dir_create     ( const char            *name,
                                                mode_t                 mode );

DirectResult DIRECT_API direct_dir_open       ( DirectDir             *dir,
                                                const char            *name );

DirectResult DIRECT_API direct_dir_read       ( DirectDir             *dir,
                                                DirectEntry           *entry );

DirectResult DIRECT_API direct_dir_rewind     ( DirectDir             *dir );

DirectResult DIRECT_API direct_dir_close      ( DirectDir             *dir );

DirectResult DIRECT_API direct_dir_remove     ( const char            *name );

DirectResult DIRECT_API direct_file_open      ( DirectFile            *file,
                                                const char            *name,
                                                int                    flags,
                                                mode_t                 mode );

DirectResult DIRECT_API direct_file_read      ( DirectFile            *file,
                                                void                  *buffer,
                                                size_t                 bytes,
                                                size_t                *ret_bytes );

DirectResult DIRECT_API direct_file_write     ( DirectFile            *file,
                                                const void            *buffer,
                                                size_t                 bytes,
                                                size_t                *ret_bytes );

DirectResult DIRECT_API direct_file_seek      ( DirectFile            *file,
                                                off_t                  offset );

DirectResult DIRECT_API direct_file_seek_to   ( DirectFile            *file,
                                                off_t                  offset );

DirectResult DIRECT_API direct_file_close     ( DirectFile            *file );

DirectResult DIRECT_API direct_file_map       ( DirectFile            *file,
                                                void                  *addr,
                                                size_t                 offset,
                                                size_t                 bytes,
                                                DirectFilePermission   perms,
                                                void                 **ret_addr );

DirectResult DIRECT_API direct_file_unmap     ( void                  *addr,
                                                size_t                 bytes );

DirectResult DIRECT_API direct_file_get_info  ( DirectFile            *file,
                                                DirectFileInfo        *ret_info );

DirectResult DIRECT_API direct_file_chmod     ( DirectFile            *file,
                                                mode_t                 mode );

DirectResult DIRECT_API direct_file_chown     ( DirectFile            *file,
                                                uid_t                  owner,
                                                gid_t                  group );

DirectResult DIRECT_API direct_file_truncate  ( DirectFile            *file,
                                                off_t                  length );

DirectResult DIRECT_API direct_file_get_string( DirectFile            *file,
                                                char                  *buf,
                                                size_t                 length );

DirectResult DIRECT_API direct_popen          ( DirectFile            *file,
                                                const char            *name,
                                                int                    flags );

DirectResult DIRECT_API direct_pclose         ( DirectFile            *file );

DirectResult DIRECT_API direct_access         ( const char            *name,
                                                int                    flags );

DirectResult DIRECT_API direct_chmod          ( const char            *name,
                                                mode_t                 mode );

DirectResult DIRECT_API direct_chown          ( const char            *name,
                                                uid_t                  owner,
                                                gid_t                  group );

DirectResult DIRECT_API direct_readlink       ( const char            *name,
                                                char                  *buf,
                                                size_t                 length,
                                                ssize_t               *ret_length );

DirectResult DIRECT_API direct_unlink         ( const char            *name );

DirectResult DIRECT_API direct_filesystem_size( const char            *name,
                                                size_t                *size );

#endif

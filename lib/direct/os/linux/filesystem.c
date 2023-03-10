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

#include <direct/debug.h>
#include <direct/os/filesystem.h>
#include <direct/messages.h>
#include <direct/util.h>

/**********************************************************************************************************************/

DirectResult
direct_dir_get_current( char   *buf,
                        size_t  length )
{
     D_ASSERT( buf != NULL );

     if (!getcwd( buf, length ))
          return errno2result( errno );

     return DR_OK;
}

DirectResult
direct_dir_change( const char *name )
{
     D_ASSERT( name != NULL );

     if (chdir( name ) < 0)
          return errno2result( errno );

     return DR_OK;
}

DirectResult
direct_dir_create( const char *name,
                   mode_t      mode )
{
     D_ASSERT( name != NULL );

     if (mkdir( name, mode ) < 0) {
          if (errno == EEXIST)
               return DR_BUSY;
          else
               return errno2result( errno );
     }

     return DR_OK;
}

DirectResult
direct_dir_open( DirectDir  *dir,
                 const char *name )
{
     D_ASSERT( dir != NULL );
     D_ASSERT( name != NULL );

     dir->dir = opendir( name );
     if (!dir->dir)
          return errno2result( errno );

     return DR_OK;
}

DirectResult
direct_dir_read( DirectDir   *dir,
                 DirectEntry *entry )
{
     struct dirent *ent;

     D_ASSERT( dir != NULL );
     D_ASSERT( entry != NULL );

     ent = readdir( dir->dir );
     if (!ent)
          return errno ? errno2result( errno ) : DR_ITEMNOTFOUND;
     else
          strcpy( entry->name, ent->d_name );

     return DR_OK;
}

DirectResult
direct_dir_rewind( DirectDir *dir )
{
     D_ASSERT( dir != NULL );

     rewinddir( dir->dir );

     return DR_OK;
}

DirectResult
direct_dir_close( DirectDir *dir )
{
     int err;

     D_ASSERT( dir != NULL );

     err = closedir( dir->dir );

     dir->dir = NULL;

     if (err < 0)
          return errno2result( errno );

     return DR_OK;
}

DirectResult
direct_dir_remove( const char *name )
{
     D_ASSERT( name != NULL );

     if (rmdir( name ) < 0)
          return errno2result( errno );

     return DR_OK;
}

DirectResult
direct_file_open( DirectFile *file,
                  const char *name,
                  int         flags,
                  mode_t      mode )
{
     D_ASSERT( file != NULL );
     D_ASSERT( name != NULL );

     file->file = NULL;

     file->fd = open( name, flags, mode );
     if (file->fd < 0)
          return errno2result( errno );

     return DR_OK;
}

DirectResult
direct_file_read( DirectFile *file,
                  void       *buffer,
                  size_t      bytes,
                  size_t     *ret_bytes )
{
     ssize_t num;

     D_ASSERT( file != NULL );
     D_ASSERT( buffer != NULL );

     num = read( file->fd, buffer, bytes );
     if (num < 0)
          return errno2result( errno );

     if (ret_bytes)
          *ret_bytes = num;

     return DR_OK;
}

DirectResult
direct_file_write( DirectFile *file,
                   const void *buffer,
                   size_t      bytes,
                   size_t     *ret_bytes )
{
     ssize_t num;

     D_ASSERT( file != NULL );
     D_ASSERT( buffer != NULL );

     num = write( file->fd, buffer, bytes );
     if (num < 0)
          return errno2result( errno );

     if (ret_bytes)
          *ret_bytes = num;

     return DR_OK;
}

DirectResult
direct_file_seek( DirectFile *file,
                  off_t       offset )
{
     D_ASSERT( file != NULL );

     if (lseek( file->fd, offset, SEEK_CUR ) < 0) {
          if (errno == ESPIPE)
               return DR_IO;
          else
               return errno2result( errno );
     }

     return DR_OK;
}

DirectResult
direct_file_seek_to( DirectFile *file,
                     off_t       offset )
{
     D_ASSERT( file != NULL );

     if (lseek( file->fd, offset, SEEK_SET ) < 0)
          return errno2result( errno );

     return DR_OK;
}

DirectResult
direct_file_close( DirectFile *file )
{
     int err;

     D_ASSERT( file != NULL );

     if (file->file) {
          err = fclose( file->file );

          file->file = NULL;
     }
     else
          err = close( file->fd );

     file->fd = -1;

     if (err < 0)
          return errno2result( errno );

     return DR_OK;
}

DirectResult
direct_file_map( DirectFile            *file,
                 void                  *addr,
                 size_t                 offset,
                 size_t                 bytes,
                 DirectFilePermission   perms,
                 void                 **ret_addr )
{
     void *map;
     int   prot = 0;
     int   flags = MAP_SHARED;

     D_ASSERT( file != NULL );
     D_ASSERT( ret_addr != NULL );

     if (perms & DFP_READ)
          prot |= PROT_READ;

     if (perms & DFP_WRITE)
          prot |= PROT_WRITE;

     if (addr)
          flags |= MAP_FIXED;

     map = mmap( addr, bytes, prot, flags, file->fd, offset );
     if (map == MAP_FAILED)
          return errno2result( errno );

     *ret_addr = map;

     return DR_OK;
}

DirectResult
direct_file_unmap( void   *addr,
                   size_t  bytes )
{
     if (munmap( addr, bytes ) < 0)
          return errno2result( errno );

     return DR_OK;
}

DirectResult
direct_file_get_info( DirectFile     *file,
                      DirectFileInfo *ret_info )
{
     struct stat st;

     D_ASSERT( file != NULL );
     D_ASSERT( ret_info != NULL );

     if (fstat( file->fd, &st ) < 0)
          return errno2result( errno );

     ret_info->flags = DFIF_SIZE;
     ret_info->size  = st.st_size;

     return DR_OK;
}

DirectResult
direct_file_chmod( DirectFile *file,
                   mode_t      mode )
{
     D_ASSERT( file != NULL );

     if (fchmod( file->fd, mode ) < 0)
          return errno2result( errno );

     return DR_OK;
}

DirectResult
direct_file_chown( DirectFile *file,
                   uid_t       owner,
                   gid_t       group )
{
     D_ASSERT( file != NULL );

     if (fchown( file->fd, owner, group ) < 0)
          return errno2result( errno );

     return DR_OK;
}

DirectResult
direct_file_truncate( DirectFile *file,
                      off_t       length )
{
     D_ASSERT( file != NULL );

     if (ftruncate( file->fd, length ) < 0)
          return errno2result( errno );

     return DR_OK;
}

DirectResult
direct_file_get_string( DirectFile *file,
                        char       *buf,
                        size_t      length )
{
     D_ASSERT( file != NULL );
     D_ASSERT( buf != NULL );

     if (!file->file) {
          file->file = fdopen( file->fd, "r" );
          if (!file->file)
               return errno2result( errno );
     }

     if (!fgets( buf, length, file->file )) {
          if (feof( file->file ))
               return DR_EOF;

          return DR_FAILURE;
     }

     return DR_OK;
}

DirectResult
direct_popen( DirectFile *file,
              const char *name,
              int         flags )
{
     D_ASSERT( file != NULL );
     D_ASSERT( name != NULL );

     file->file = popen( name, (flags & O_WRONLY) ? "w" : (flags & O_RDWR) ? "rw" : "r" );
     if (!file->file)
          return errno2result( errno );

     file->fd = fileno( file->file );

     return DR_OK;
}

DirectResult
direct_pclose( DirectFile *file )
{
     D_ASSERT( file != NULL );
     D_ASSERT( file->file != NULL );

     if (pclose( file->file ) < 0)
          return errno2result( errno );

     return DR_OK;
}

DirectResult
direct_access( const char *name,
               int         flags )
{
     D_ASSERT( name != NULL );

     if (access( name, flags ) < 0)
          return errno2result( errno );

     return DR_OK;
}

DirectResult
direct_chmod( const char *name,
              mode_t      mode )
{
     D_ASSERT( name != NULL );

     if (chmod( name, mode ) < 0)
          return errno2result( errno );

     return DR_OK;
}

DirectResult
direct_chown( const char *name,
              uid_t       owner,
              gid_t       group )
{
     D_ASSERT( name != NULL );

     if (chown( name, owner, group ) < 0)
          return errno2result( errno );

     return DR_OK;
}

DirectResult
direct_readlink( const char *name,
                 char       *buf,
                 size_t      length,
                 ssize_t    *ret_length )
{
     ssize_t len;

     D_ASSERT( name != NULL );

     len = readlink( name, buf, length );
     if (len < 0)
          return errno2result( errno );

     if (ret_length)
          *ret_length = len;

     return DR_OK;
}

DirectResult
direct_unlink( const char *name )
{
     D_ASSERT( name != NULL );

     if (unlink( name ) < 0)
          return errno2result( errno );

     return DR_OK;
}

DirectResult
direct_filesystem_size( const char *name,
                        size_t     *size )
{
     struct statfs stat;

     D_ASSERT( name != NULL );

     if (statfs( name, &stat ) < 0)
          return errno2result( errno );

     *size = stat.f_blocks * stat.f_bsize;

     return DR_OK;
}

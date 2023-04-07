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

#include <direct/os/log.h>
#include <direct/messages.h>
#include <direct/util.h>

/**********************************************************************************************************************/

static DirectResult init_stderr( DirectLog *log );

static DirectResult init_file  ( DirectLog  *log, const char *filename );

/**********************************************************************************************************************/

DirectResult
direct_log_init( DirectLog  *log,
                 const char *param )
{
     switch (log->type) {
          case DLT_STDERR:
               return init_stderr( log );

          case DLT_FILE:
               return init_file( log, param );

          case DLT_UDP:
          default:
               break;
     }

     return DR_UNSUPPORTED;
}

DirectResult
direct_log_deinit( DirectLog *log )
{
     close( (long) log->data );

     return DR_OK;
}

/**********************************************************************************************************************/

__attribute__((no_instrument_function))
static DirectResult
common_log_write( DirectLog  *log,
                  const char *buffer,
                  size_t      bytes )
{
     ssize_t sz;

     sz = write( (long) log->data, buffer, bytes );
     if (sz < 0)
          D_PERROR( "Direct/Log: Could not write to log!\n" );

     return DR_OK;
}

__attribute__((no_instrument_function))
static DirectResult
common_log_flush( DirectLog *log )
{
     if (log->type == DLT_STDERR && fflush( stderr ))
          return errno2result( errno );

     return DR_OK;
}

__attribute__((no_instrument_function))
static DirectResult
stderr_log_write( DirectLog  *log,
                  const char *buffer,
                  size_t      bytes )
{
     fwrite( buffer, bytes, 1, stderr );

     return DR_OK;
}

static DirectResult
init_stderr( DirectLog *log )
{
     log->data = (void*)(long) dup( fileno( stderr ) );

     log->write      = stderr_log_write;
     log->flush      = common_log_flush;

     return DR_OK;
}

static DirectResult
init_file( DirectLog  *log,
           const char *filename )
{
     DirectResult ret;
     int          fd;

     fd = open( filename, O_WRONLY | O_CREAT | O_APPEND, 0664 );
     if (fd < 0) {
          ret = errno2result( errno );
          D_PERROR( "Direct/Log: Could not open '%s' for writing!\n", filename );
          return ret;
     }

     log->data = (void*)(long) fd;

     log->write = common_log_write;
     log->flush = common_log_flush;

     return DR_OK;
}

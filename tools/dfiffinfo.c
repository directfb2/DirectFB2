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

#include <dfiff.h>
#include <direct/filesystem.h>
#include <directfb_util.h>

/**********************************************************************************************************************/

int main( int argc, char *argv[] )
{
     DFBResult    ret;
     DirectFile   file;
     DFIFFHeader *header;

     /* Parse the command line. */
     if (argc != 2) {
          fprintf( stderr, "\nDirectFB Fast Image File Format Information\n\n" );
          fprintf( stderr, "Usage: %s <imagefile>\n", argv[0] );
          fprintf( stderr, "\n" );
          return 1;
     }

     /* Open the file. */
     ret = direct_file_open( &file, argv[1], O_RDONLY, 0 );
     if (ret) {
          fprintf( stderr, "Failed to open '%s'!\n", argv[1] );
          return 1;
     }

     /* Memory-mapped file. */
     ret = direct_file_map( &file, NULL, 0, sizeof(DFIFFHeader), DFP_READ, (void**) &header );
     if (ret) {
          fprintf( stderr, "Failed during mmap() of '%s'!\n", argv[1] );
          goto out;
     }

     /* Check the magic. */
     if (strncmp( (const char*) header, "DFIFF", 5 )) {
          fprintf( stderr, "Bad magic in '%s'!\n", argv[1] );
          goto out;
     }

     printf( "%s: %ux%u, %s\n", argv[1], header->width, header->height, dfb_pixelformat_name( header->format ) );

out:
     direct_file_close( &file );

     return !ret ? 0 : 1;
}

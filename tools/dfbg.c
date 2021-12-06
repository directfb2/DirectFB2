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

#include <directfb.h>

/**********************************************************************************************************************/

static IDirectFB             *dfb   = NULL;
static IDirectFBDisplayLayer *layer = NULL;

static const char *filename      = NULL;
static DFBBoolean  color         = DFB_FALSE;
static DFBBoolean  tiled         = DFB_FALSE;
static DFBBoolean  premultiplied = DFB_FALSE;

static DFBBoolean parse_command_line  ( int argc, char *argv[] );
static void       set_background_color( void );
static void       set_background_image( void );

/**********************************************************************************************************************/

int
main( int   argc, char *argv[] )
{
     DFBResult ret;

     /* Initialize DirectFB including command line parsing. */
     ret = DirectFBInit( &argc, &argv );
     if (ret) {
          DirectFBError( "DirectFBInit() failed", ret );
          return -1;
     }

     /* Parse the command line. */
     if (!parse_command_line( argc, argv ))
          return -2;

     DirectFBSetOption( "bg-none", NULL );
     DirectFBSetOption( "no-cursor", NULL );

     /* Create the main interface. */
     ret = DirectFBCreate( &dfb );
     if (ret) {
          DirectFBError( "DirectFBCreate() failed", ret );
          return -3;
     }

     /* Get the primary display layer. */
     ret = dfb->GetDisplayLayer( dfb, DLID_PRIMARY, &layer );
     if (ret) {
          DirectFBError( "IDirectFB::GetDisplayLayer() failed", ret );
          dfb->Release( dfb );
          return -4;
     }

     /* Acquire administrative cooperative level. */
     ret = layer->SetCooperativeLevel( layer, DLSCL_ADMINISTRATIVE );
     if (ret) {
          DirectFBError( "IDirectFBDisplayLayer::SetCooperativeLevel() failed", ret );
          layer->Release( layer );
          dfb->Release( dfb );
          return -5;
     }

     /* Set the background as desired by the user. */
     if (color)
          set_background_color();
     else
          set_background_image();

     /* Release the primary display layer. */
     layer->Release( layer );

     /* Release the main interface. */
     dfb->Release( dfb );

     return EXIT_SUCCESS;
}

/**********************************************************************************************************************/

static void
print_usage( const char *name )
{
     fprintf( stderr, "\nDirectFB Desktop Background Configuration\n\n" );
     fprintf( stderr, "Usage: %s [options] <imagefile>|<color>\n\n", name );
     fprintf( stderr, "Options:\n" );
     fprintf( stderr, "  -c, --color          Set <color> in AARRGGBB format (hexadecimal)\n" );
     fprintf( stderr, "  -t, --tile           Set tiled mode\n" );
     fprintf( stderr, "  -p, --premultiplied  Create the surface with DSCAPS_PREMULTIPLIED\n" );
     fprintf( stderr, "  -h, --help           Show this help message\n" );
     fprintf( stderr, "\n" );
}

static DFBBoolean
parse_command_line( int argc, char *argv[] )
{
     int n;

     for (n = 1; n < argc; n++) {
          const char *a = argv[n];

          if (*a != '-') {
               if (!filename) {
                    filename = a;
                    continue;
               }
               else {
                    print_usage( argv[0] );
                    return DFB_FALSE;
               }
          }
          if (strcmp( a, "-h" ) == 0 || strcmp( a, "--help" ) == 0) {
               print_usage( argv[0] );
               return DFB_FALSE;
          }
          if (strcmp( a, "-c" ) == 0 || strcmp( a, "--color" ) == 0) {
               color = DFB_TRUE;
               continue;
          }
          if (strcmp( a, "-t" ) == 0 || strcmp( a, "--tile" ) == 0) {
               tiled = DFB_TRUE;
               continue;
          }
          if (strcmp( a, "-p" ) == 0 || strcmp( a, "--premultiplied" ) == 0) {
               premultiplied = DFB_TRUE;
               continue;
          }
     }

     if (!filename) {
          print_usage( argv[0] );
          return DFB_FALSE;
     }

     return DFB_TRUE;
}

/**********************************************************************************************************************/

static void
set_background_color()
{
     DFBResult  ret;
     char      *error;
     u32        argb;

     if (*filename == '#')
          filename++;

     argb = strtoul( filename, &error, 16 );

     if (*error) {
          fprintf( stderr, "Invalid characters in color string: '%s'\n", error );
          return;
     }

     ret = layer->SetBackgroundColor( layer,
                                      (argb & 0xff0000)   >> 16,
                                      (argb & 0xff00)     >> 8,
                                      (argb & 0xff)       >> 0,
                                      (argb & 0xff000000) >> 24 );
     if (ret) {
          DirectFBError( "IDirectFBDisplayLayer::SetBackgroundColor() failed", ret );
          return;
     }

     ret = layer->SetBackgroundMode( layer, DLBM_COLOR );
     if (ret)
          DirectFBError( "IDirectFBDisplayLayer::SetBackgroundMode() failed", ret );
}

static void
set_background_image()
{
     DFBResult               ret;
     DFBSurfaceDescription   desc;
     IDirectFBSurface       *surface;
     IDirectFBImageProvider *provider;

     ret = dfb->CreateImageProvider( dfb, filename, &provider );
     if (ret) {
          DirectFBError( "IDirectFB::CreateImageProvider() failed", ret );
          return;
     }

     ret = provider->GetSurfaceDescription( provider, &desc );
     if (ret) {
          DirectFBError( "IDirectFBImageProvider::GetSurfaceDescription() failed", ret );
          provider->Release( provider );
          return;
     }

     desc.flags |= DSDESC_CAPS;
     desc.caps   = DSCAPS_SHARED;

     if (premultiplied)
          desc.caps |= DSCAPS_PREMULTIPLIED;

     ret = dfb->CreateSurface( dfb, &desc, &surface );
     if (ret) {
          DirectFBError( "IDirectFB::CreateSurface() failed", ret );
          provider->Release( provider );
          return;
     }

     ret = provider->RenderTo( provider, surface, NULL );
     if (ret) {
          DirectFBError( "IDirectFBImageProvider::RenderTo() failed", ret );
          surface->Release( surface );
          provider->Release( provider );
          return;
     }

     ret = layer->SetBackgroundImage( layer, surface );
     if (ret) {
          DirectFBError( "IDirectFBDisplayLayer::SetBackgroundImage() failed", ret );
          surface->Release( surface );
          provider->Release( provider );
          return;
     }

     ret = layer->SetBackgroundMode( layer, tiled ? DLBM_TILE : DLBM_IMAGE );
     if (ret)
          DirectFBError( "IDirectFBDisplayLayer::SetBackgroundMode() failed", ret );

     surface->Release( surface );
     provider->Release( provider );
}

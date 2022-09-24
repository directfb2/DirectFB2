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

#include <core/fonts.h>
#include <core/surface.h>
#include <dgiff.h>
#include <direct/filesystem.h>
#include <direct/hash.h>
#include <directfb_util.h>
#include <media/idirectfbfont.h>

D_DEBUG_DOMAIN( Font_DGIFF, "Font/DGIFF", "DGIFF Font Provider" );

static DFBResult Probe    ( IDirectFBFont_ProbeContext *ctx );

static DFBResult Construct( IDirectFBFont              *thiz,
                            CoreDFB                    *core,
                            IDirectFBFont_ProbeContext *ctx,
                            DFBFontDescription         *desc );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBFont, DGIFF )

/**********************************************************************************************************************/

typedef struct {
     void         *map;      /* memory map of the font file */
     int           size;     /* size of the memory map */

     CoreSurface **rows;     /* bitmaps of loaded glyphs */
     int           num_rows;
} DGIFFImplData;

/**********************************************************************************************************************/

static void
IDirectFBFont_DGIFF_Destruct( IDirectFBFont *thiz )
{
     DGIFFImplData *data = ((IDirectFBFont_data*) thiz->priv)->font->impl_data;

     D_DEBUG_AT( Font_DGIFF, "%s( %p )\n", __FUNCTION__, thiz );

     if (data->rows) {
          int i;

          for (i = 0; i < data->num_rows; i++) {
               if (data->rows[i])
                    dfb_surface_unref( data->rows[i] );
          }

          D_FREE( data->rows );
     }

     direct_file_unmap( data->map, data->size );

     D_FREE( data );

     IDirectFBFont_Destruct( thiz );
}

static DirectResult
IDirectFBFont_DGIFF_Release( IDirectFBFont *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBFont )

     D_DEBUG_AT( Font_DGIFF, "%s( %p )\n", __FUNCTION__, thiz );

     if (--data->ref == 0)
          IDirectFBFont_DGIFF_Destruct( thiz );

     return DFB_OK;
}

/**********************************************************************************************************************/

static DFBResult
Probe( IDirectFBFont_ProbeContext *ctx )
{
     if (!ctx->filename)
          return DFB_UNSUPPORTED;

     /* Check the magic. */
     if (!strncmp( (const char*) ctx->content, "DGIFF", 5 ))
          return DFB_OK;

     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBFont              *thiz,
           CoreDFB                    *core,
           IDirectFBFont_ProbeContext *ctx,
           DFBFontDescription         *desc )
{
     DFBResult              ret;
     int                    i;
     DirectFile             fd;
     DirectFileInfo         info;
     const DGIFFHeader     *header;
     const DGIFFFaceHeader *faceheader;
     DGIFFGlyphInfo        *glyphs;
     DGIFFGlyphRow         *row;
     void                  *ptr  = NULL;
     CoreFont              *font = NULL;
     DGIFFImplData         *data = NULL;

     D_DEBUG_AT( Font_DGIFF, "%s( %p )\n", __FUNCTION__, thiz );

     /* Check for valid description. */
     if (!(desc->flags & DFDESC_HEIGHT))
          return DFB_INVARG;

     if (desc->flags & DFDESC_ROTATION)
          return DFB_UNSUPPORTED;

     D_DEBUG_AT( Font_DGIFF, "  -> file '%s' at pixel height %d\n", ctx->filename, desc->height );

     /* Open the file. */
     ret = direct_file_open( &fd, ctx->filename, O_RDONLY, 0 );
     if (ret) {
          D_DERROR( ret, "Font/DGIFF: Failed to open '%s'!\n", ctx->filename );
          return ret;
     }

     /* Query file size. */
     ret = direct_file_get_info( &fd, &info );
     if (ret) {
          D_DERROR( ret, "Font/DGIFF: Failed during get_info() of '%s'!\n", ctx->filename );
          goto error;
     }

     /* Memory-mapped file. */
     ret = direct_file_map( &fd, NULL, 0, info.size, DFP_READ, &ptr );
     if (ret) {
          D_DERROR( ret, "Font/DGIFF: Failed during mmap() of '%s'!\n", ctx->filename );
          goto error;
     }

     direct_file_close( &fd );

     header = ptr;
     faceheader = ptr + sizeof(DGIFFHeader);

     /* Lookup requested face. */
     for (i = 0; i < header->num_faces; i++) {
          if (faceheader->size == desc->height)
               break;

          faceheader = (void*) faceheader + faceheader->next_face;
     }

     if (i == header->num_faces) {
          D_ERROR( "Font/DGIFF: Requested size %d not found in '%s'!\n", desc->height, ctx->filename );
          ret = DFB_UNSUPPORTED;
          goto error;
     }

     glyphs = (void*) (faceheader + 1);
     row    = (void*) (glyphs + faceheader->num_glyphs);

     /* Create the font object. */
     ret = dfb_font_create( core, desc, ctx->filename, &font );
     if (ret)
          goto error;

     /* Fill font information. */
     if (faceheader->blittingflags)
          font->blittingflags = faceheader->blittingflags;

     font->pixel_format = faceheader->pixelformat;
     font->surface_caps = DSCAPS_NONE;
     font->ascender     = faceheader->ascender;
     font->descender    = faceheader->descender;
     font->height       = faceheader->height;
     font->maxadvance   = faceheader->max_advance;
     font->up_unit_x    =  0.0;
     font->up_unit_y    = -1.0;
     font->flags        = CFF_SUBPIXEL_ADVANCE;

     CORE_FONT_DEBUG_AT( Font_DGIFF, font );

     /* Allocate implementation data. */
     data = D_CALLOC( 1, sizeof(DGIFFImplData) );
     if (!data) {
          ret = D_OOM();
          goto error;
     }

     data->map  = ptr;
     data->size = info.size;

     data->num_rows = faceheader->num_rows;

     /* Allocate array for glyph cache rows. */
     data->rows = D_CALLOC( data->num_rows, sizeof(void*) );
     if (!data->rows) {
          ret = D_OOM();
          goto error;
     }

     /* Build glyph cache rows. */
     for (i = 0; i < data->num_rows; i++) {
          ret = dfb_surface_create_simple( core, row->width, row->height, faceheader->pixelformat,
                                           DFB_COLORSPACE_DEFAULT( faceheader->pixelformat ), DSCAPS_NONE, CSTF_NONE, 0,
                                           NULL, &data->rows[i] );
          if (ret) {
               D_DERROR( ret, "DGIFF/Font: Could not create %s %dx%d glyph row surface!\n",
                         dfb_pixelformat_name( faceheader->pixelformat ), row->width, row->height );
               goto error;
          }

          dfb_surface_write_buffer( data->rows[i], DSBR_BACK, row + 1, row->pitch, NULL );

          /* Jump to next row. */
          row = (void*) (row + 1) + row->pitch * row->height;
     }

     /* Build glyph info. */
     for (i = 0; i < faceheader->num_glyphs; i++) {
          CoreGlyphData  *glyph_data;
          DGIFFGlyphInfo *glyph = &glyphs[i];

          glyph_data = D_CALLOC( 1, sizeof(CoreGlyphData) );
          if (!glyph_data) {
               ret = D_OOM();
               goto error;
          }

          glyph_data->surface  = data->rows[glyph->row];
          glyph_data->start    = glyph->offset;
          glyph_data->width    = glyph->width;
          glyph_data->height   = glyph->height;
          glyph_data->left     = glyph->left;
          glyph_data->top      = glyph->top;
          glyph_data->xadvance = glyph->advance << 8;
          glyph_data->yadvance = 0;

          D_MAGIC_SET( glyph_data, CoreGlyphData );

          direct_hash_insert( font->layers[0].glyph_hash, glyph->unicode, glyph_data );

          if (glyph->unicode < 128)
               font->layers[0].glyph_data[glyph->unicode] = glyph_data;
     }

     font->impl_data = data;

     IDirectFBFont_Construct( thiz, font );

     thiz->Release = IDirectFBFont_DGIFF_Release;

     return DFB_OK;

error:
     if (font) {
          if (data) {
               if (data->rows) {
                    for (i = 0; i < data->num_rows; i++) {
                         if (data->rows[i])
                              dfb_surface_unref( data->rows[i] );
                    }

                    D_FREE( data->rows );
               }

               D_FREE( data );
          }

          dfb_font_destroy( font );
     }

     if (ptr)
          direct_file_unmap( ptr, info.size );

     direct_file_close( &fd );

     return ret;
}

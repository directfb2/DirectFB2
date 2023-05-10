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
#include <direct/filesystem.h>
#include <direct/utf8.h>
#include <direct/util.h>
#include <directfb_util.h>
#include <media/idirectfbdatabuffer.h>
#include <media/idirectfbfont.h>

D_DEBUG_DOMAIN( Font, "IDirectFBFont", "IDirectFBFont Interface" );

/**********************************************************************************************************************/

static unsigned int
sqrt16( unsigned int val )
{
     unsigned short op  = 1 << 15;
     unsigned int   res = 1 << 15;

     while (1) {
          if (res * res > val)
               res = res ^ op;

          op >>= 1;

          if (op == 0) {
               res = res + ((val - res * res) > res ? 1 : 0);
               break;
          }

          res = res | op;
     }

     return res;
}

/**********************************************************************************************************************/

void
IDirectFBFont_Destruct( IDirectFBFont *thiz )
{
     IDirectFBFont_data *data = thiz->priv;

     D_DEBUG_AT( Font, "%s( %p )\n", __FUNCTION__, thiz );

     dfb_font_destroy( data->font );

     if (data->content) {
          switch (data->content_type) {
               case IDFBFONT_CONTEXT_CONTENT_TYPE_MALLOCED:
                    D_FREE( data->content );
                    break;

               case IDFBFONT_CONTEXT_CONTENT_TYPE_MAPPED:
                    direct_file_unmap( data->content, data->content_size );
                    break;

               case IDFBFONT_CONTEXT_CONTENT_TYPE_MEMORY:
                    break;

               default:
                    D_BUG( "unexpected content type %u", data->content_type );
          }
     }

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DirectResult
IDirectFBFont_AddRef( IDirectFBFont *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBFont )

     D_DEBUG_AT( Font, "%s( %p )\n", __FUNCTION__, thiz );

     data->ref++;

     return DFB_OK;
}

static DirectResult
IDirectFBFont_Release( IDirectFBFont *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBFont )

     D_DEBUG_AT( Font, "%s( %p )\n", __FUNCTION__, thiz );

     if (--data->ref == 0)
          IDirectFBFont_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBFont_GetAscender( IDirectFBFont *thiz,
                           int           *ret_ascender )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBFont )

     D_DEBUG_AT( Font, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_ascender)
          return DFB_INVARG;

     *ret_ascender = data->font->ascender;

     return DFB_OK;
}

static DFBResult
IDirectFBFont_GetDescender( IDirectFBFont *thiz,
                            int           *ret_descender )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBFont )

     D_DEBUG_AT( Font, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_descender)
          return DFB_INVARG;

     *ret_descender = data->font->descender;

     return DFB_OK;
}

static DFBResult
IDirectFBFont_GetHeight( IDirectFBFont *thiz,
                         int           *ret_height )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBFont )

     D_DEBUG_AT( Font, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_height)
          return DFB_INVARG;

     *ret_height = data->font->height;

     return DFB_OK;
}

static DFBResult
IDirectFBFont_GetMaxAdvance( IDirectFBFont *thiz,
                             int           *ret_maxadvance )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBFont )

     D_DEBUG_AT( Font, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_maxadvance)
          return DFB_INVARG;

     *ret_maxadvance = data->font->maxadvance;

     return DFB_OK;
}

static DFBResult
IDirectFBFont_GetKerning( IDirectFBFont *thiz,
                          unsigned int   prev,
                          unsigned int   current,
                          int           *ret_kern_x,
                          int           *ret_kern_y )
{
     DFBResult    ret;
     int          x = 0, y = 0;
     unsigned int prev_index, current_index;

     DIRECT_INTERFACE_GET_DATA( IDirectFBFont )

     D_DEBUG_AT( Font, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_kern_x && !ret_kern_y)
          return DFB_INVARG;

     dfb_font_lock( data->font );

     if (data->font->GetKerning) {
          ret = dfb_font_decode_character( data->font, data->encoding, prev, &prev_index );
          if (ret)
               goto error;

          ret = dfb_font_decode_character( data->font, data->encoding, current, &current_index );
          if (ret)
               goto error;

          ret = data->font->GetKerning( data->font, prev_index, current_index, &x, &y );
          if (ret)
               goto error;
     }

     dfb_font_unlock( data->font );

     if (ret_kern_x)
          *ret_kern_x = x;

     if (ret_kern_y)
          *ret_kern_y = y;

     return DFB_OK;

error:
     dfb_font_unlock( data->font );

     return ret;
}

static DFBResult
IDirectFBFont_GetStringWidth( IDirectFBFont *thiz,
                              const char    *text,
                              int            bytes,
                              int           *ret_width )
{
     DFBResult ret;
     int       xsize = 0;
     int       ysize = 0;

     DIRECT_INTERFACE_GET_DATA( IDirectFBFont )

     D_DEBUG_AT( Font, "%s( %p )\n", __FUNCTION__, thiz );

     if (!text || !ret_width)
          return DFB_INVARG;

     if (bytes < 0)
          bytes = strlen( text );

     if (bytes > 0) {
          int          i, num;
          unsigned int prev = 0;
          unsigned int indices[bytes];

          dfb_font_lock( data->font );

          /* Decode string to character indices. */
          ret = dfb_font_decode_text( data->font, data->encoding, text, bytes, indices, &num );
          if (ret) {
               dfb_font_unlock( data->font );
               return ret;
          }

          /* Calculate string width. */
          for (i = 0; i < num; i++) {
               unsigned int   current = indices[i];
               CoreGlyphData *glyph;

               if (dfb_font_get_glyph_data( data->font, current, 0, &glyph ) == DFB_OK) {
                    int kx, ky;

                    xsize += glyph->xadvance;
                    ysize += glyph->yadvance;

                    if (prev && data->font->GetKerning &&
                        data->font->GetKerning( data->font, prev, current, &kx, &ky ) == DFB_OK) {
                         xsize += kx << 8;
                         ysize += ky << 8;
                    }
               }

               prev = current;
          }

          dfb_font_unlock( data->font );
     }

     if (!ysize) {
          *ret_width = xsize >> 8;
     }
     else if (!xsize) {
          *ret_width = ysize >> 8;
     }
     else {
          *ret_width = sqrt16( xsize * xsize + ysize * ysize ) / 4096.0f;
     }

     return DFB_OK;
}

static DFBResult
IDirectFBFont_GetStringExtents( IDirectFBFont *thiz,
                                const char    *text,
                                int            bytes,
                                DFBRectangle  *ret_logical_rect,
                                DFBRectangle  *ret_ink_rect )
{
     DFBResult ret;
     int       xbaseline = 0;
     int       ybaseline = 0;

     DIRECT_INTERFACE_GET_DATA( IDirectFBFont )

     D_DEBUG_AT( Font, "%s( %p )\n", __FUNCTION__, thiz );

     if (!text)
          return DFB_INVARG;

     if (!ret_logical_rect && !ret_ink_rect)
          return DFB_INVARG;

     if (bytes < 0)
          bytes = strlen( text );

     if (ret_ink_rect)
          memset( ret_ink_rect, 0, sizeof(DFBRectangle) );

     dfb_font_lock( data->font );

     if (bytes > 0) {
          int          i, num;
          unsigned int prev  = 0;
          unsigned int indices[bytes];

          /* Decode string to character indices. */
          ret = dfb_font_decode_text( data->font, data->encoding, text, bytes, indices, &num );
          if (ret) {
               dfb_font_unlock( data->font );
               return ret;
          }

          for (i = 0; i < num; i++) {
               unsigned int   current = indices[i];
               CoreGlyphData *glyph;

               if (dfb_font_get_glyph_data( data->font, current, 0, &glyph ) == DFB_OK) {
                    int kx, ky;

                    if (prev && data->font->GetKerning &&
                        data->font->GetKerning( data->font, prev, current, &kx, &ky ) == DFB_OK) {
                         xbaseline += kx << 8;
                         ybaseline += ky << 8;
                    }

                    if (ret_ink_rect) {
                         DFBRectangle glyph_rect = { xbaseline + (glyph->left << 8), ybaseline + (glyph->top << 8),
                                                     glyph->width << 8,              glyph->height << 8 };

                         dfb_rectangle_union( ret_ink_rect, &glyph_rect );
                    }

                    xbaseline += glyph->xadvance;
                    ybaseline += glyph->yadvance;
               }

               prev = current;
          }
     }

     if (ret_logical_rect) {
          /* We already have the text baseline vector in (xbaseline,ybaseline).
             Find the ascender and descender vectors. */
          int xascender =  data->font->ascender * data->font->up_unit_x;
          int yascender =  data->font->ascender * data->font->up_unit_y;
          int xdescender = data->font->descender * data->font->up_unit_x;
          int ydescender = data->font->descender * data->font->up_unit_y;

          /* Now find top/bottom and left/right points relative to the text. */
          int top_left_x     = xascender;
          int top_left_y     = yascender;
          int bottom_left_x  = xdescender;
          int bottom_left_y  = ydescender;
          int top_right_x    = top_left_x    + (xbaseline >> 8);
          int top_right_y    = top_left_y    + (ybaseline >> 8);
          int bottom_right_x = bottom_left_x + (xbaseline >> 8);
          int bottom_right_y = bottom_left_y + (ybaseline >> 8);

          /* The logical rectangle is the bounding-box of these points. */
          #define MIN4(a,b,c,d) (MIN( MIN( a, b ), MIN( c, d ) ))
          #define MAX4(a,b,c,d) (MAX( MAX( a, b ), MAX( c, d ) ))
          ret_logical_rect->x = MIN4( top_left_x, bottom_left_x, top_right_x, bottom_right_x );
          ret_logical_rect->y = MIN4( top_left_y, bottom_left_y, top_right_y, bottom_right_y );
          ret_logical_rect->w = MAX4( top_left_x, bottom_left_x, top_right_x, bottom_right_x ) - ret_logical_rect->x;
          ret_logical_rect->h = MAX4( top_left_y, bottom_left_y, top_right_y, bottom_right_y ) - ret_logical_rect->y;
     }

     if (ret_ink_rect) {
          if (ret_ink_rect->w < 0) {
               ret_ink_rect->x +=  ret_ink_rect->w;
               ret_ink_rect->w  = -ret_ink_rect->w;
          }
          ret_ink_rect->x += (data->font->ascender * data->font->up_unit_x) / 256.0f;
          ret_ink_rect->y += (data->font->ascender * data->font->up_unit_y) / 256.0f;

          ret_ink_rect->x >>= 8;
          ret_ink_rect->y >>= 8;
          ret_ink_rect->w >>= 8;
          ret_ink_rect->h >>= 8;
     }

     dfb_font_unlock( data->font );

     return DFB_OK;
}

static DFBResult
IDirectFBFont_GetGlyphExtents( IDirectFBFont *thiz,
                               unsigned int   character,
                               DFBRectangle  *ret_rect,
                               int           *ret_advance )
{
     DFBResult      ret;
     CoreGlyphData *glyph;
     unsigned int   index;

     DIRECT_INTERFACE_GET_DATA( IDirectFBFont )

     D_DEBUG_AT( Font, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_rect && !ret_advance)
          return DFB_INVARG;

     dfb_font_lock( data->font );

     ret = dfb_font_decode_character( data->font, data->encoding, character, &index );
     if (ret) {
          dfb_font_unlock( data->font );
          return ret;
     }

     if (dfb_font_get_glyph_data( data->font, index, 0, &glyph ) != DFB_OK) {
          if (ret_rect)
               ret_rect->x = ret_rect->y = ret_rect->w = ret_rect->h = 0;

          if (ret_advance)
               *ret_advance = 0;
     }
     else {
          if (ret_rect) {
               ret_rect->x = glyph->left;
               ret_rect->y = glyph->top - data->font->ascender;
               ret_rect->w = glyph->width;
               ret_rect->h = glyph->height;
          }

          if (ret_advance)
               *ret_advance = glyph->xadvance >> 8;
     }

     dfb_font_unlock( data->font );

     return DFB_OK;
}

static DFBResult
IDirectFBFont_GetStringBreak( IDirectFBFont  *thiz,
                              const char     *text,
                              int             bytes,
                              int             max_width,
                              int            *ret_width,
                              int            *ret_str_length,
                              const char    **ret_next_line )
{
     DFBResult      ret;
     const u8      *string;
     const u8      *last;
     const u8      *end;
     CoreGlyphData *glyph;
     int            kern_x;
     int            kern_y;
     int            length = 0;
     int            xsize = 0;
     int            ysize = 0;
     int            width = 0;
     unichar        current;
     unsigned int   index;
     unsigned int   prev  = 0;

     DIRECT_INTERFACE_GET_DATA( IDirectFBFont )

     if (!text || !ret_next_line || !ret_str_length || !ret_width)
          return DFB_INVARG;

     if (data->encoding != DTEID_UTF8)
          return DFB_UNSUPPORTED;

     if (bytes < 0)
          bytes = strlen( text );

     if (!bytes) {
          *ret_next_line  = NULL;
          *ret_str_length = 0;
          *ret_width      = 0;

          return DFB_OK;
     }

     string = (const u8*) text;
     end    = string + bytes;

     *ret_next_line = NULL;

     dfb_font_lock( data->font );

     do {
          *ret_width = width >> 8;

          current = DIRECT_UTF8_GET_CHAR( string );

          last    = string;
          string += DIRECT_UTF8_SKIP( string[0] );

          if (current == ' ' || current == 0x0a) {
               *ret_next_line  = (const char*) string;
               *ret_str_length = length;
               *ret_width      = width >> 8;
          }

          length++;

          ret = dfb_font_decode_character( data->font, data->encoding, current, &index );
          if (ret)
               continue;

          ret = dfb_font_get_glyph_data( data->font, index, 0, &glyph );
          if (ret)
               continue;

          xsize += glyph->xadvance;
          ysize += glyph->yadvance;

          if (prev && data->font->GetKerning &&
              data->font->GetKerning( data->font, prev, index, &kern_x, NULL ) == DFB_OK)
               width += kern_x << 8;

          if (prev && data->font->GetKerning &&
              data->font->GetKerning( data->font, prev, index, &kern_x, &kern_y) == DFB_OK) {
               xsize += kern_x << 8;
               ysize += kern_y << 8;
          }

          if (!ysize) {
               width = xsize;
          }
          else if (!xsize) {
               width = ysize;
          }
          else {
               width = sqrt16( xsize * xsize + ysize * ysize ) / 256.0f;
          }

          prev = index;
     } while ((width >> 8) < max_width && string < end && current != 0x0a);

     dfb_font_unlock( data->font );

     if ((width >> 8) < max_width && string >= end) {
          *ret_next_line  = NULL;
          *ret_str_length = length;
          *ret_width      = width >> 8;

          return DFB_OK;
     }

     if (*ret_next_line == NULL) {
          if (length == 1) {
               *ret_str_length = length;
               *ret_next_line  = (const char*) string;
               *ret_width      = width >> 8;
          }
          else {
               *ret_str_length = length - 1;
               *ret_next_line  = (const char*) last;
          }
     }

     return DFB_OK;
}

static DFBResult
IDirectFBFont_SetEncoding( IDirectFBFont     *thiz,
                           DFBTextEncodingID  encoding )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBFont )

     D_DEBUG_AT( Font, "%s( %p, %u )\n", __FUNCTION__, thiz, encoding );

     if (encoding > data->font->last_encoding)
          return DFB_IDNOTFOUND;

     data->encoding = encoding;

     return DFB_OK;
}

static DFBResult
IDirectFBFont_EnumEncodings( IDirectFBFont           *thiz,
                             DFBTextEncodingCallback  callback,
                             void                    *callbackdata )
{
     int i;

     DIRECT_INTERFACE_GET_DATA( IDirectFBFont )

     if (!callback)
          return DFB_INVARG;

     D_DEBUG_AT( Font, "%s( %p, %p, %p )\n", __FUNCTION__, thiz, callback, callbackdata );

     if (callback( DTEID_UTF8, "UTF8", callbackdata ) == DFENUM_OK) {
          for (i = DTEID_OTHER; i <= data->font->last_encoding; i++) {
               if (callback( i, data->font->encodings[i]->name, callbackdata ) != DFENUM_OK)
                    break;
          }
     }

     return DFB_OK;
}

static DFBResult
IDirectFBFont_FindEncoding( IDirectFBFont     *thiz,
                            const char        *name,
                            DFBTextEncodingID *ret_encoding )
{
     int i;

     DIRECT_INTERFACE_GET_DATA( IDirectFBFont )

     if (!name || !ret_encoding)
          return DFB_INVARG;

     D_DEBUG_AT( Font, "%s( %p, '%s', %p )\n", __FUNCTION__, thiz, name, ret_encoding );

     if (!strcasecmp( name, "UTF8" )) {
          *ret_encoding = DTEID_UTF8;
          return DFB_OK;
     }

     for (i = DTEID_OTHER; i <= data->font->last_encoding; i++) {
          if (!strcasecmp( name, data->font->encodings[i]->name )) {
               *ret_encoding = i;
               return DFB_OK;
          }
     }

     return DFB_IDNOTFOUND;
}

static DFBResult
IDirectFBFont_Dispose( IDirectFBFont *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBFont )

     D_DEBUG_AT( Font, "%s( %p )\n", __FUNCTION__, thiz );

     return dfb_font_dispose( data->font );
}

static DFBResult
IDirectFBFont_GetLineSpacingVector( IDirectFBFont *thiz,
                                    int           *ret_xspacing,
                                    int           *ret_yspacing )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBFont )

     D_DEBUG_AT( Font, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_xspacing && !ret_yspacing)
          return DFB_INVARG;

     if (ret_xspacing) {
          *ret_xspacing = - data->font->height * data->font->up_unit_x;
     }

     if (ret_yspacing) {
          *ret_yspacing = - data->font->height * data->font->up_unit_y;
     }

     return DFB_OK;
}

static DFBResult
IDirectFBFont_GetGlyphExtentsXY( IDirectFBFont *thiz,
                                 unsigned int   character,
                                 DFBRectangle  *ret_rect,
                                 int           *ret_xadvance,
                                 int           *ret_yadvance )
{
     DFBResult      ret;
     CoreGlyphData *glyph;
     unsigned int   index;

     DIRECT_INTERFACE_GET_DATA( IDirectFBFont )

     D_DEBUG_AT( Font, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_rect && !ret_xadvance && !ret_yadvance)
          return DFB_INVARG;

     dfb_font_lock( data->font );

     ret = dfb_font_decode_character( data->font, data->encoding, character, &index );
     if (ret) {
          dfb_font_unlock( data->font );
          return ret;
     }

     if (dfb_font_get_glyph_data( data->font, index, 0, &glyph ) != DFB_OK) {
          if (ret_rect)
               ret_rect->x = ret_rect->y = ret_rect->w = ret_rect->h = 0;

          if (ret_xadvance)
               *ret_xadvance = 0;

          if (ret_yadvance)
               *ret_yadvance = 0;
     }
     else {
          if (ret_rect) {
               ret_rect->x = glyph->left + data->font->ascender * data->font->up_unit_x;
               ret_rect->y = glyph->top  + data->font->ascender * data->font->up_unit_y;
               ret_rect->w = glyph->width;
               ret_rect->h = glyph->height;
          }

          if (ret_xadvance)
               *ret_xadvance = glyph->xadvance;

          if (ret_yadvance)
               *ret_yadvance = glyph->yadvance;
     }

     dfb_font_unlock( data->font );

     return DFB_OK;
}

static DFBResult
IDirectFBFont_GetUnderline( IDirectFBFont *thiz,
                            int           *ret_underline_position,
                            int           *ret_underline_thickness )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBFont )

     D_DEBUG_AT( Font, "%s( %p )\n", __FUNCTION__, thiz );

     if (ret_underline_position)
          *ret_underline_position = data->font->underline_position;

     if (ret_underline_thickness)
          *ret_underline_thickness = data->font->underline_thickness;

     return DFB_OK;
}

static DFBResult
IDirectFBFont_GetDescription( IDirectFBFont      *thiz,
                              DFBFontDescription *ret_desc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBFont )

     D_DEBUG_AT( Font, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_desc)
          return DFB_INVARG;

     *ret_desc = data->font->description;

     return DFB_OK;
}

DFBResult
IDirectFBFont_Construct( IDirectFBFont *thiz, CoreFont *font )
{
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBFont )

     D_DEBUG_AT( Font, "%s( %p )\n", __FUNCTION__, thiz );

     data->ref  = 1;
     data->font = font;

     thiz->AddRef               = IDirectFBFont_AddRef;
     thiz->Release              = IDirectFBFont_Release;
     thiz->GetAscender          = IDirectFBFont_GetAscender;
     thiz->GetDescender         = IDirectFBFont_GetDescender;
     thiz->GetHeight            = IDirectFBFont_GetHeight;
     thiz->GetMaxAdvance        = IDirectFBFont_GetMaxAdvance;
     thiz->GetKerning           = IDirectFBFont_GetKerning;
     thiz->GetStringWidth       = IDirectFBFont_GetStringWidth;
     thiz->GetStringExtents     = IDirectFBFont_GetStringExtents;
     thiz->GetGlyphExtents      = IDirectFBFont_GetGlyphExtents;
     thiz->GetStringBreak       = IDirectFBFont_GetStringBreak;
     thiz->SetEncoding          = IDirectFBFont_SetEncoding;
     thiz->EnumEncodings        = IDirectFBFont_EnumEncodings;
     thiz->FindEncoding         = IDirectFBFont_FindEncoding;
     thiz->Dispose              = IDirectFBFont_Dispose;
     thiz->GetLineSpacingVector = IDirectFBFont_GetLineSpacingVector;
     thiz->GetGlyphExtentsXY    = IDirectFBFont_GetGlyphExtentsXY;
     thiz->GetUnderline         = IDirectFBFont_GetUnderline;
     thiz->GetDescription       = IDirectFBFont_GetDescription;

     return DFB_OK;
}

static void
unmap_or_free( IDirectFBFont_ProbeContext *ctx )
{
     if (ctx->content) {
          switch (ctx->content_type) {
               case IDFBFONT_CONTEXT_CONTENT_TYPE_MALLOCED:
                    D_FREE( ctx->content );
                    break;

               case IDFBFONT_CONTEXT_CONTENT_TYPE_MAPPED:
                    direct_file_unmap( ctx->content, ctx->content_size );
                    break;

               case IDFBFONT_CONTEXT_CONTENT_TYPE_MEMORY:
                    break;

               default:
                    D_BUG( "unexpected content type %u", ctx->content_type );
          }

          ctx->content = NULL;
     }
}

DFBResult
IDirectFBFont_CreateFromBuffer( IDirectFBDataBuffer       *buffer,
                                CoreDFB                   *core,
                                const DFBFontDescription  *desc,
                                IDirectFBFont            **ret_interface )
{
     DFBResult                   ret;
     DirectInterfaceFuncs       *funcs = NULL;
     IDirectFBDataBuffer_data   *buffer_data;
     IDirectFBFont              *iface;
     IDirectFBFont_ProbeContext  ctx = { NULL, NULL, 0, IDFBFONT_CONTEXT_CONTENT_TYPE_UNKNOWN };
     IDirectFBFont_data         *data;

     D_DEBUG_AT( Font, "%s( %p )\n", __FUNCTION__, buffer );

     /* Get the private information of the data buffer. */
     buffer_data = buffer->priv;
     if (!buffer_data)
          return DFB_DEAD;

     /* Provide a fallback for fonts without data buffer support. */
     ctx.filename = buffer_data->filename;

     if (buffer_data->buffer) {
          ctx.content      = buffer_data->buffer;
          ctx.content_size = buffer_data->length;
          ctx.content_type = IDFBFONT_CONTEXT_CONTENT_TYPE_MEMORY;
     }
     else if (buffer_data->filename) {
          DirectFile      fd;
          DirectFileInfo  info;
          void           *ptr;

          /* Open the file. */
          ret = direct_file_open( &fd, ctx.filename, O_RDONLY, 0 );
          if (ret) {
               D_DERROR( ret, "IDirectFBFont: Could not open '%s'\n", ctx.filename );
               return ret;
          }

          /* Query file size. */
          ret = direct_file_get_info( &fd, &info );
          if (ret) {
               D_DERROR( ret, "IDirectFBFont: Could not query info about '%s'\n", ctx.filename );
               direct_file_close( &fd );
               return ret;
          }

          /* Memory-mapped file. */
          ret = direct_file_map( &fd, NULL, 0, info.size, DFP_READ, &ptr );
          if (ret) {
               D_DERROR( ret, "IDirectFBFont: Could not mmap '%s'\n", ctx.filename );
               direct_file_close( &fd );
               return ret;
          }

          ctx.content      = ptr;
          ctx.content_size = info.size;
          ctx.content_type = IDFBFONT_CONTEXT_CONTENT_TYPE_MAPPED;

          direct_file_close( &fd );
     }
     else {
          ctx.content_size = 0;
          ctx.content_type = IDFBFONT_CONTEXT_CONTENT_TYPE_MALLOCED;

          while (1) {
               unsigned int bytes;

               ctx.content = D_REALLOC( ctx.content, ctx.content_size + 4096 );
               if (!ctx.content) {
                    ret = D_OOM();
                    return ret;
               }

               buffer->WaitForData( buffer, 4096 );
               if (buffer->GetData( buffer, 4096, ctx.content + ctx.content_size, &bytes ))
                    break;

               ctx.content_size += bytes;
          }
     }

     D_ASSERT( ctx.content_type != IDFBFONT_CONTEXT_CONTENT_TYPE_UNKNOWN );

     /* Find a suitable implementation. */
     ret = DirectGetInterface( &funcs, "IDirectFBFont", NULL, DirectProbeInterface, &ctx );
     if (ret) {
          unmap_or_free( &ctx );
          return ret;
     }

     DIRECT_ALLOCATE_INTERFACE( iface, IDirectFBFont );

     /* Construct the interface. */
     ret = funcs->Construct( iface, core, &ctx, desc );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( iface );
          unmap_or_free( &ctx );
          return ret;
     }

     /* Store content pointer informations for deletion. */
     data = iface->priv;
     data->content      = ctx.content;
     data->content_size = ctx.content_size;
     data->content_type = ctx.content_type;

     *ret_interface = iface;

     return DFB_OK;
}

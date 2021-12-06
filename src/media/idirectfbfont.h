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

#ifndef __MEDIA__IDIRECTFBFONT_H__
#define __MEDIA__IDIRECTFBFONT_H__

#include <core/coretypes.h>

/**********************************************************************************************************************/

typedef enum {
     IDFBFONT_CONTEXT_CONTENT_TYPE_UNKNOWN  = 0x00000000,

     IDFBFONT_CONTEXT_CONTENT_TYPE_MALLOCED = 0x00000001,
     IDFBFONT_CONTEXT_CONTENT_TYPE_MAPPED   = 0x00000002,
     IDFBFONT_CONTEXT_CONTENT_TYPE_MEMORY   = 0x00000003
} IDirectFBFont_ProbeContextContentType;

/*
 * probing context
 */
typedef struct {
     const char                            *filename;
     unsigned char                         *content;
     unsigned int                           content_size;
     IDirectFBFont_ProbeContextContentType  content_type;
} IDirectFBFont_ProbeContext;

/**********************************************************************************************************************/

/*
 * private data struct of IDirectFBFont
 */
typedef struct {
     int                                    ref;          /* reference counter */

     CoreFont                              *font;         /* the font object */

     unsigned char                         *content;
     unsigned int                           content_size;
     IDirectFBFont_ProbeContextContentType  content_type;
     DFBTextEncodingID                      encoding;
} IDirectFBFont_data;

/*
 * Common code to initialize interface struct and private data.
 */
DFBResult IDirectFBFont_Construct       ( IDirectFBFont             *thiz,
                                          CoreFont                  *font );

/*
 * Common code to destroy font and free private data.
 */
void      IDirectFBFont_Destruct        ( IDirectFBFont             *thiz );

/*
 * Create (probing) the font.
 */
DFBResult IDirectFBFont_CreateFromBuffer( IDirectFBDataBuffer       *buffer,
                                          CoreDFB                   *core,
                                          const DFBFontDescription  *desc,
                                          IDirectFBFont            **ret_interface );

#endif

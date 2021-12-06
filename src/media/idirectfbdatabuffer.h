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

#ifndef __MEDIA__IDIRECTFBDATABUFFER_H__
#define __MEDIA__IDIRECTFBDATABUFFER_H__

#include <core/coretypes.h>

/*
 * private data struct of IDirectFBDataBuffer
 */
typedef struct {
     int           ref;       /* reference counter */

     char         *filename;  /* only set if databuffer is created from file */
     void         *buffer;    /* only set if databuffer is created from memory */
     unsigned int  length;

     CoreDFB      *core;
     IDirectFB    *idirectfb;
} IDirectFBDataBuffer_data;

/*
 * initializes interface struct and private data
 */
DFBResult IDirectFBDataBuffer_Construct( IDirectFBDataBuffer *thiz,
                                         const char          *filename,
                                         const void          *buffer,
                                         unsigned int         length,
                                         CoreDFB             *core,
                                         IDirectFB           *idirectfb );

/*
 * destroys databuffer and frees private data
 */
void      IDirectFBDataBuffer_Destruct ( IDirectFBDataBuffer *thiz );

#endif

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

#ifndef __INPUT__IDIRECTFBEVENTBUFFER_H__
#define __INPUT__IDIRECTFBEVENTBUFFER_H__

#include <core/coretypes.h>

typedef bool (*EventBufferFilterCallback)( DFBEvent *evt, void *ctx );

/*
 * initializes interface struct and private data
 */
DFBResult IDirectFBEventBuffer_Construct        ( IDirectFBEventBuffer      *thiz,
                                                  EventBufferFilterCallback  filter,
                                                  void                      *filter_ctx );

DFBResult IDirectFBEventBuffer_AttachInputDevice( IDirectFBEventBuffer      *thiz,
                                                  CoreInputDevice           *device );

DFBResult IDirectFBEventBuffer_DetachInputDevice( IDirectFBEventBuffer      *thiz,
                                                  CoreInputDevice           *device );

DFBResult IDirectFBEventBuffer_AttachWindow     ( IDirectFBEventBuffer      *thiz,
                                                  CoreWindow                *window );

DFBResult IDirectFBEventBuffer_DetachWindow     ( IDirectFBEventBuffer      *thiz,
                                                  CoreWindow                *window );

DFBResult IDirectFBEventBuffer_AttachSurface    ( IDirectFBEventBuffer      *thiz,
                                                  CoreSurface               *surface );

DFBResult IDirectFBEventBuffer_DetachSurface    ( IDirectFBEventBuffer      *thiz,
                                                  CoreSurface               *surface );

#endif

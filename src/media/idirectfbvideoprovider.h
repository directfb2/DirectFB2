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

#ifndef __MEDIA__IDIRECTFBVIDEOPROVIDER_H__
#define __MEDIA__IDIRECTFBVIDEOPROVIDER_H__

#include <core/coretypes.h>

/**********************************************************************************************************************/

/*
 * probing context
 */
typedef struct {
     unsigned char        header[64];
     const char          *filename;
     IDirectFBDataBuffer *buffer;     /* Useful if provider needs more data for probing. */
} IDirectFBVideoProvider_ProbeContext;

/**********************************************************************************************************************/

/*
 * Create (probing) the video provider.
 */
DFBResult IDirectFBVideoProvider_CreateFromBuffer( IDirectFBDataBuffer     *buffer,
                                                   CoreDFB                 *core,
                                                   IDirectFB               *idirectfb,
                                                   IDirectFBVideoProvider **ret_interface );

#endif

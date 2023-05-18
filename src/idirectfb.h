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

#ifndef __IDIRECTFB_H__
#define __IDIRECTFB_H__

#include <core/coretypes.h>

void eventbuffer_containers_remove( IDirectFBEventBuffer *thiz );

/*
 * initializes interface struct and private data
 */
DFBResult IDirectFB_Construct      ( IDirectFB  *thiz );

DFBResult IDirectFB_WaitInitialised( IDirectFB  *thiz );

DFBResult IDirectFB_SetAppFocus    ( IDirectFB  *thiz,
                                     DFBBoolean  focused );

#endif

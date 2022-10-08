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

#ifndef __FBDEV_VT_H__
#define __FBDEV_VT_H__

#include <core/coretypes.h>

/**********************************************************************************************************************/

DFBResult fbdev_vt_initialize       ( CoreDFB *core,
                                      int      fbdev_fd );

DFBResult fbdev_vt_shutdown         ( bool     emergency,
                                      int      fbdev_fd );

bool      fbdev_vt_switch_num       ( int      num,
                                      bool     key_pressed );

void      fbdev_vt_set_graphics_mode( bool     set );

#endif

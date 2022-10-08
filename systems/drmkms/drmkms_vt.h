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

#ifndef __DRMKMS_VT_H__
#define __DRMKMS_VT_H__

#include <core/coretypes.h>

/**********************************************************************************************************************/

DFBResult drmkms_vt_initialize( CoreDFB *core );

DFBResult drmkms_vt_shutdown  ( bool     emergency );

bool      drmkms_vt_switch_num( int      num,
                                bool     key_pressed );

#endif

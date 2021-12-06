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

#ifndef __DRMKMS_MODE_H__
#define __DRMKMS_MODE_H__

#include "drmkms_system.h"

/**********************************************************************************************************************/

DFBScreenOutputResolution  drmkms_modes_to_dsor_bitmask( DRMKMSData                *drmkms,
                                                         int                        connector );

drmModeModeInfo           *drmkms_find_mode            ( DRMKMSData                *drmkms,
                                                         int                        connector,
                                                         int                        width,
                                                         int                        height,
                                                         unsigned int               freq );

drmModeModeInfo           *drmkms_dsor_dsef_to_mode    ( DRMKMSData                *drmkms,
                                                         int                        connector,
                                                         DFBScreenOutputResolution  dsor,
                                                         DFBScreenEncoderFrequency  dsef );

DFBResult                  drmkms_mode_to_dsor_dsef    ( const drmModeModeInfo     *videomode,
                                                         DFBScreenOutputResolution *dsor,
                                                         DFBScreenEncoderFrequency *dsef );

#endif

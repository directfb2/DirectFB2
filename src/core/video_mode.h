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

#ifndef __CORE__VIDEO_MODE_H__
#define __CORE__VIDEO_MODE_H__

/**********************************************************************************************************************/

typedef struct _VideoMode {
     int                xres;
     int                yres;
     int                bpp;

     int                pixclock;
     int                left_margin;
     int                right_margin;
     int                upper_margin;
     int                lower_margin;
     int                hsync_len;
     int                vsync_len;
     int                hsync_high;
     int                vsync_high;
     int                csync_high;

     int                laced;
     int                doubled;

     int                sync_on_green;
     int                external_sync;
     int                broadcast;

     struct _VideoMode *next;
} VideoMode;

#endif

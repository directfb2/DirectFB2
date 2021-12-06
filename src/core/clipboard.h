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

#ifndef __CORE__CLIPBOARD_H__
#define __CORE__CLIPBOARD_H__

#include <core/coretypes.h>
#include <fusion/lock.h>

/**********************************************************************************************************************/

typedef struct {
     int                  magic;

     FusionSkirmish       lock;
     char                *mime_type;
     void                *data;
     unsigned int         size;
     struct timeval       timestamp;

     FusionSHMPoolShared *shmpool;
} DFBClipboardCoreShared;

typedef struct {
     int                     magic;

     CoreDFB                *core;

     DFBClipboardCoreShared *shared;
} DFBClipboardCore;

/**********************************************************************************************************************/

DFBResult dfb_clipboard_set          ( DFBClipboardCore  *core,
                                       const char        *mime_type,
                                       const void        *data,
                                       unsigned int       size,
                                       struct timeval    *timestamp );

DFBResult dfb_clipboard_get          ( DFBClipboardCore  *core,
                                       char             **mime_type,
                                       void             **data,
                                       unsigned int      *size );

DFBResult dfb_clipboard_get_timestamp( DFBClipboardCore  *core,
                                       struct timeval    *timestamp );

#endif

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

#include <direct/result.h>
#include <direct/util.h>
#include <directfb.h>

/**********************************************************************************************************************/

static const char       *DFBResult__strings[DFB__RESULT_END - DFB__RESULT_BASE];

static DirectResultType  DFBResult__type = {
     0, 0, DFB__RESULT_BASE, DFBResult__strings, D_ARRAY_SIZE(DFBResult__strings)
};

void
DFBResult__init()
{
     DFBResult__strings[0] = "DFBResult";

     DFBResult__strings[D_RESULT_INDEX(DFB_NOVIDEOMEMORY)] = "There's not enough video memory.";
     DFBResult__strings[D_RESULT_INDEX(DFB_MISSINGFONT)] = "No font has been set.";
     DFBResult__strings[D_RESULT_INDEX(DFB_MISSINGIMAGE)] = "No image has been set.";
     DFBResult__strings[D_RESULT_INDEX(DFB_NOALLOCATION)] = "No allocation.";
     DFBResult__strings[D_RESULT_INDEX(DFB_NOBUFFER)] = "No buffer.";

     DirectResultTypeRegister( &DFBResult__type );
}

void
DFBResult__deinit()
{
     DirectResultTypeUnregister( &DFBResult__type );
}

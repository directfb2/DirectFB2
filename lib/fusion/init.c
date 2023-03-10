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

#include <direct/debug.h>
#include <direct/util.h>
#include <fusion/call.h>
#include <fusion/conf.h>

D_DEBUG_DOMAIN( Fusion_Init, "Fusion/Init", "Fusion Init" );

/**********************************************************************************************************************/

typedef void (*Func)( void );


static Func init_funcs[] = {
     __Fusion_conf_init,
     __Fusion_call_init,
};

static Func deinit_funcs[] = {
     __Fusion_call_deinit,
     __Fusion_conf_deinit,
};

/**********************************************************************************************************************/

__attribute__((constructor))
void
__Fusion_init_all()
{
     size_t i;

     D_DEBUG_AT( Fusion_Init, "%s()\n", __FUNCTION__ );

     for (i = 0; i < D_ARRAY_SIZE(init_funcs); i++)
          init_funcs[i]();
}

__attribute__((destructor))
void
__Fusion_deinit_all()
{
     size_t i;

     D_DEBUG_AT( Fusion_Init, "%s()\n", __FUNCTION__ );

     for (i = 0; i < D_ARRAY_SIZE(deinit_funcs); i++)
          deinit_funcs[i]();
}

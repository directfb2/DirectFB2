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

#include <direct/direct.h>
#include <direct/direct_result.h>
#include <direct/interface.h>
#include <direct/log.h>
#include <direct/result.h>
#include <direct/thread.h>
#include <direct/util.h>

/**********************************************************************************************************************/

typedef void (*Func)( void );

static Func init_funcs[] = {
     __D_conf_init,
     __D_direct_init,
     __D_util_init,
     __D_result_init,
     __D_direct_result_init,
     __D_mem_init,
     __D_thread_init,
     __D_log_init,
     __D_log_domain_init,
     __D_interface_init,
     __D_interface_dbg_init,
};

static Func deinit_funcs[] = {
     __D_interface_dbg_deinit,
     __D_interface_deinit,
     __D_log_domain_deinit,
     __D_thread_deinit,
     __D_mem_deinit,
     __D_direct_result_deinit,
     __D_result_deinit,
     __D_util_deinit,
     __D_direct_deinit,
     __D_log_deinit,
     __D_conf_deinit,
};

/**********************************************************************************************************************/

__dfb_constructor__
void
__D_init_all( void )
{
     size_t i;

     for (i = 0; i < D_ARRAY_SIZE(init_funcs); i++)
          init_funcs[i]();
}

__dfb_destructor__
void
__D_deinit_all( void )
{
     size_t i;

     for (i = 0; i < D_ARRAY_SIZE(deinit_funcs); i++)
          deinit_funcs[i]();
}

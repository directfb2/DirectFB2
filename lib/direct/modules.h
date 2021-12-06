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

#ifndef __DIRECT__MODULES_H__
#define __DIRECT__MODULES_H__

#include <direct/types.h>
#include <direct/list.h>

/**********************************************************************************************************************/

struct __D_DirectModuleEntry {
     DirectLink       link;

     int              magic;

     DirectModuleDir *directory;

     bool             loaded;
     bool             dynamic;
     bool             disabled;

     char            *name;
     const void      *funcs;

     int              refs;
     char            *file;
     void            *handle;
};

struct __D_DirectModuleDir {
     const char        *path;
     unsigned int       abi_version;

     DirectLink        *entries;

     DirectModuleEntry *loading;
};

#define DECLARE_MODULE_DIRECTORY(d)    \
     extern DirectModuleDir d

#define DEFINE_MODULE_DIRECTORY(d,p,n) \
     DirectModuleDir d = {             \
          p,                           \
          n,                           \
          NULL,                        \
          NULL,                        \
     }

/**********************************************************************************************************************/

void       DIRECT_API  direct_modules_register         ( DirectModuleDir   *directory,
                                                         unsigned int       abi_version,
                                                         const char        *name,
                                                         const void        *funcs );

void       DIRECT_API  direct_modules_unregister       ( DirectModuleDir   *directory,
                                                         const char        *name );

int        DIRECT_API  direct_modules_explore_directory( DirectModuleDir   *directory );

const void DIRECT_API *direct_module_ref               ( DirectModuleEntry *module );

void       DIRECT_API  direct_module_unref             ( DirectModuleEntry *module );

#endif

#  This file is part of DirectFB.
#
#  This library is free software; you can redistribute it and/or
#  modify it under the terms of the GNU Lesser General Public
#  License as published by the Free Software Foundation; either
#  version 2.1 of the License, or (at your option) any later version.
#
#  This library is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  Lesser General Public License for more details.
#
#  You should have received a copy of the GNU Lesser General Public
#  License along with this library; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA

library('icoreresourcemanager_default',
        'icoreresourcemanager_default.c',
        dependencies: directfb_dep,
        install: true,
        install_dir: join_paths(moduledir, 'interfaces/ICoreResourceManager'))

pkgconfig.generate(filebase: 'directfb-interface-resourcemanager_default',
                   variables: 'moduledir=' + moduledir,
                   name: 'DirectFB-interface-resourcemanager_default',
                   description: 'Default resource manager',
                   libraries_private: ['-L${moduledir}/interfaces/ICoreResourceManager',
                                       '-Wl,--whole-archive -licoreresourcemanager_default -Wl,--no-whole-archive'])

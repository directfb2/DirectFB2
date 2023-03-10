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

library('idirectfbimageprovider_dfiff',
        'idirectfbimageprovider_dfiff.c',
        dependencies: directfb_dep,
        install: true,
        install_dir: join_paths(moduledir, 'interfaces/IDirectFBImageProvider'))

pkgconfig.generate(filebase: 'directfb-interface-imageprovider_dfiff',
                   variables: 'moduledir=' + moduledir,
                   name: 'DirectFB-interface-imageprovider_dfiff',
                   description: 'DFIFF image provider',
                   libraries_private: ['-L${moduledir}/interfaces/IDirectFBImageProvider',
                                       '-Wl,--whole-archive -lidirectfbimageprovider_dfiff -Wl,--no-whole-archive'])

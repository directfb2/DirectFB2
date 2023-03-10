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

dummy_sources = [
  'dummy.c'
]

library('directfb_dummy',
         dummy_sources,
         dependencies: directfb_dep,
         install: true,
         install_dir: join_paths(moduledir, 'systems'))

pkgconfig.generate(filebase: 'directfb-system-dummy',
                   variables: 'moduledir=' + moduledir,
                   name: 'DirectFB-system-dummy',
                   description: 'Dummy system module',
                   libraries_private: ['-L${moduledir}/systems',
                                       '-Wl,--whole-archive -ldirectfb_dummy -Wl,--no-whole-archive'])

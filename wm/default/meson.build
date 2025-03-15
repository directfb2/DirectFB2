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

library('directfbwm_default',
        'default.c',
        include_directories: config_inc,
        build_rpath: get_option('prefix') / get_option('libdir'),
        dependencies: directfb_dep,
        install: true,
        install_dir: moduledir / 'wm')

if get_option('default_library') == 'static'
  pkgconfig.generate(filebase: 'directfb-wm-default',
                     variables: 'moduledir=' + moduledir,
                     name: 'DirectFB-wm-default',
                     description: 'Default window manager module',
                     libraries_private: ['-L${moduledir}/wm',
                                         '-Wl,--whole-archive -ldirectfbwm_default -Wl,--no-whole-archive'])
endif

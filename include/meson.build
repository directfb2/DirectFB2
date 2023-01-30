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

directfb_headers = [
  'dfiff.h',
  'dfvff.h',
  'dgiff.h',
  'directfb.h',
  'directfb_keyboard.h',
  'directfb_util.h',
  'directfb_windows.h',
  'directfbgl.h',
]

directfb_version_h = configuration_data()
directfb_version_h.set('DIRECTFB_MAJOR_VERSION', directfb_major_version)
directfb_version_h.set('DIRECTFB_MINOR_VERSION', directfb_minor_version)
directfb_version_h.set('DIRECTFB_MICRO_VERSION', directfb_micro_version)
configure_file(configuration: directfb_version_h,
               output: 'directfb_version.h',
               install: true,
               install_dir: join_paths(get_option('includedir'), 'directfb'))

directfb_build_h = configuration_data()
directfb_build_h.set('FLUXED_ARGS_BYTES', get_option('args-size'))
configure_file(configuration: directfb_build_h,
               output: 'directfb_build.h',
               install: true,
               install_dir: join_paths(get_option('includedir'), 'directfb'))

directfb_keynames = custom_target('directfb_keynames',
                                  command: ['gen_directfb_keynames.sh', '@INPUT@'],
                                  input: 'directfb_keyboard.h',
                                  output: 'directfb_keynames.h',
                                  capture: true,
                                  install: true,
                                  install_dir: join_paths(get_option('includedir'), 'directfb'))

directfb_strings = custom_target('directfb_strings',
                                 command: ['gen_directfb_strings.sh', '@INPUT@'],
                                 input: 'directfb.h',
                                 output: 'directfb_strings.h',
                                 capture: true,
                                 install: true,
                                 install_dir: join_paths(get_option('includedir'), 'directfb'))

install_headers(directfb_headers, subdir: 'directfb')

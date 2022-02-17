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

project('DirectFB2', 'c',
         version: '2.0.0',
         meson_version: '>= 0.50',
         default_options: ['buildtype=release', 'c_std=gnu99'])

directfb_version = meson.project_version()
directfb_major_version = directfb_version.split('.')[0].to_int()
directfb_minor_version = directfb_version.split('.')[1].to_int()
directfb_micro_version = directfb_version.split('.')[2].to_int()

moduledirname = get_option('moduledirname')
if moduledirname == ''
  if get_option('debug')
    moduledirname = 'directfb-@0@.@1@-0-debug'.format(directfb_major_version, directfb_minor_version)
  else
    if not get_option('debug-support')
      moduledirname = 'directfb-@0@.@1@-0-pure'.format(directfb_major_version, directfb_minor_version)
    else
      moduledirname = 'directfb-@0@.@1@-0'.format(directfb_major_version, directfb_minor_version)
    endif
  endif
endif
moduledir = join_paths(get_option('prefix'), get_option('libdir'), moduledirname)

config_h = configuration_data()

cc = meson.get_compiler('c')

config_h.set('SIZEOF_LONG', cc.sizeof('long'), description: 'The size of long, as computed by sizeof.')

config_h.set('WORDS_BIGENDIAN', host_machine.endian() == 'big', description: 'Byte ordering is bigendian.')

if host_machine.cpu_family() == 'x86' or host_machine.cpu_family() == 'x86_64'
  if get_option('mmx')
    config_h.set('USE_MMX', 1, description: 'Define to 1 if you are compiling MMX assembly support.')
  endif
endif

if host_machine.cpu_family() == 'arm' or host_machine.cpu_family() == 'aarch64'
  if get_option('neon')
    config_h.set('USE_NEON', 1, description: 'Define to 1 if you are compiling NEON assembly support.')
  endif
endif

configure_file(configuration: config_h, output: 'config.h')

config_inc = include_directories('.')
lib_inc = include_directories('lib')
directfb_inc = [include_directories('include', 'src'), lib_inc]

add_global_arguments('-D_GNU_SOURCE', language: 'c')

pkgconfig = import('pkgconfig')

subdir('include')
subdir('lib/direct')
subdir('lib/fusion')
subdir('src')

if get_option('os') == 'linux'
  if get_option('linuxinput')
    subdir('inputdrivers/linux_input')
  endif

  if get_option('drmkms')
    subdir('systems/drmkms')
  endif
  if get_option('fbdev')
    subdir('systems/fbdev')
  endif
endif

subdir('systems/dummy')

subdir('interfaces/ICoreResourceManager')
subdir('interfaces/IDirectFBFont')
subdir('interfaces/IDirectFBImageProvider')
subdir('interfaces/IDirectFBVideoProvider')
subdir('interfaces/IDirectFBWindows')

subdir('wm/default')

subdir('tools')

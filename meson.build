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

if get_option('default_library') == 'both'
  error('''Choose library type between 'shared' or 'static'.''')
elif get_option('default_library') == 'shared'
  libsuffix = '.so'
else
  libsuffix = '.a'
endif

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

configure_file(configuration: config_h, output: 'config.h')

config_inc = include_directories('.')
lib_inc = include_directories('lib')
directfb_inc = [include_directories('include', 'src'), lib_inc]

add_global_arguments('-D_GNU_SOURCE', language: 'c')

pkgconfig = import('pkgconfig')

# core libraries

subdir('include')
subdir('lib/direct')
subdir('lib/fusion')
subdir('src')

# system modules

libdirectfb_systems = []
libdirectfb_systems_private = []
subdir('systems/dummy')
if get_option('default_library') == 'static'
  libdirectfb_systems += ['-L${moduledir}/systems']
  libdirectfb_systems += ['-Wl,--whole-archive -ldirectfb_dummy']
endif
if get_option('os') == 'linux'
  if get_option('drmkms')
    subdir('systems/drmkms')
    if get_option('default_library') == 'static'
      libdirectfb_systems += ['-ldirectfb_drmkms']
      libdirectfb_systems_private += ['-ldrm']
    endif
  endif
  if get_option('fbdev')
    subdir('systems/fbdev')
    if get_option('default_library') == 'static'
      libdirectfb_systems += ['-ldirectfb_fbdev']
    endif
  endif
endif
if get_option('default_library') == 'static'
  libdirectfb_systems += ['-Wl,--no-whole-archive']
endif

pkgconfig.generate(filebase: 'directfb-systems',
                   variables: 'moduledir=' + moduledir,
                   name: 'DirectFB-systems',
                   description: 'DirectFB system modules',
                   libraries: libdirectfb_systems,
                   libraries_private: libdirectfb_systems_private)

# inputdriver modules

libdirectfb_inputdrivers = []
libdirectfb_inputdrivers_private = []
if get_option('os') == 'linux'
  if get_option('linuxinput')
    subdir('inputdrivers/linux_input')
    if get_option('default_library') == 'static'
      libdirectfb_inputdrivers += ['-L${moduledir}/inputdrivers']
      libdirectfb_inputdrivers += ['-Wl,--whole-archive -ldirectfb_linux_input -Wl,--no-whole-archive']
    endif
  endif
endif

pkgconfig.generate(filebase: 'directfb-inputdrivers',
                   variables: 'moduledir=' + moduledir,
                   name: 'DirectFB-inputdrivers',
                   description: 'DirectFB inputdriver modules',
                   libraries: libdirectfb_inputdrivers,
                   libraries_private: libdirectfb_inputdrivers_private)

# interface modules

libdirectfb_interfaces = []
libdirectfb_interfaces_private = []
subdir('interfaces/ICoreResourceManager')
if get_option('default_library') == 'static'
  libdirectfb_interfaces += ['-L${moduledir}/interfaces/ICoreResourceManager']
  libdirectfb_interfaces += ['-Wl,--whole-archive -licoreresourcemanager_default -Wl,--no-whole-archive']
endif
subdir('interfaces/IDirectFBFont')
if get_option('default_library') == 'static'
  libdirectfb_interfaces += ['-L${moduledir}/interfaces/IDirectFBFont']
  libdirectfb_interfaces += ['-Wl,--whole-archive -lidirectfbfont_dgiff -Wl,--no-whole-archive']
endif
subdir('interfaces/IDirectFBImageProvider')
if get_option('default_library') == 'static'
  libdirectfb_interfaces += ['-L${moduledir}/interfaces/IDirectFBImageProvider']
  libdirectfb_interfaces += ['-Wl,--whole-archive -lidirectfbimageprovider_dfiff -Wl,--no-whole-archive']
endif
subdir('interfaces/IDirectFBVideoProvider')
if get_option('default_library') == 'static'
  libdirectfb_interfaces += ['-L${moduledir}/interfaces/IDirectFBVideoProvider']
  libdirectfb_interfaces += ['-Wl,--whole-archive -lidirectfbvideoprovider_dfvff -Wl,--no-whole-archive']
endif
subdir('interfaces/IDirectFBWindows')
if get_option('default_library') == 'static'
  libdirectfb_interfaces += ['-L${moduledir}/interfaces/IDirectFBWindows']
  libdirectfb_interfaces += ['-Wl,--whole-archive -lidirectfbwindows_default -Wl,--no-whole-archive']
endif

pkgconfig.generate(filebase: 'directfb-interfaces',
                   variables: 'moduledir=' + moduledir,
                   name: 'DirectFB-interfaces',
                   description: 'DirectFB interface modules',
                   libraries: libdirectfb_interfaces,
                   libraries_private: libdirectfb_interfaces_private)

# wm modules

libdirectfb_wm = []
libdirectfb_wm_private = []
subdir('wm/default')
if get_option('default_library') == 'static'
  libdirectfb_wm += ['-L${moduledir}/wm']
  libdirectfb_wm += ['-Wl,--whole-archive -ldirectfbwm_default -Wl,--no-whole-archive']
endif

pkgconfig.generate(filebase: 'directfb-wm',
                   variables: 'moduledir=' + moduledir,
                   name: 'DirectFB-wm',
                   description: 'DirectFB wm modules',
                   libraries: libdirectfb_wm,
                   libraries_private: libdirectfb_wm_private)

# tools

subdir('tools')

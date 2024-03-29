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

direct_sources = [
  'clock.c',
  'conf.c',
  'debug.c',
  'direct.c',
  'direct_result.c',
  'hash.c',
  'init.c',
  'interface.c',
  'log.c',
  'log_domain.c',
  'map.c',
  'mem.c',
  'memcpy.c',
  'messages.c',
  'modules.c',
  'result.c',
  'stream.c',
  'system.c',
  'thread.c',
  'trace.c',
  'util.c'
]

direct_headers = [
  'atomic.h',
  'clock.h',
  'compiler.h',
  'conf.h',
  'debug.h',
  'direct.h',
  'filesystem.h',
  'hash.h',
  'interface.h',
  'interface_implementation.h',
  'list.h',
  'log.h',
  'log_domain.h',
  'map.h',
  'mem.h',
  'memcpy.h',
  'messages.h',
  'modules.h',
  'mutex.h',
  'result.h',
  'serial.h',
  'signals.h',
  'stream.h',
  'system.h',
  'thread.h',
  'trace.h',
  'types.h',
  'utf8.h',
  'util.h',
  'waitqueue.h'
]

os_headers = [
  'os/clock.h',
  'os/filesystem.h',
  'os/log.h',
  'os/mem.h',
  'os/mutex.h',
  'os/signals.h',
  'os/system.h',
  'os/thread.h',
  'os/types.h',
  'os/waitqueue.h'
]

build_conf = configuration_data()

if get_option('default_library') == 'shared' and not get_option('constructors')
  error('''Choose 'static' library type to disable use of constructor attribute.''')
endif

build_conf.set10('DIRECT_BUILD_CTORS', get_option('constructors'))

build_conf.set10('DIRECT_BUILD_DEBUG', get_option('debug'))

if get_option('debug')
  build_conf.set10('DIRECT_BUILD_DEBUGS', true)
else
  build_conf.set10('DIRECT_BUILD_DEBUGS', get_option('debug-support'))
endif

build_conf.set10('DIRECT_BUILD_DYNLOAD', get_option('default_library') == 'shared')

build_conf.set10('DIRECT_BUILD_MEMCPY_PROBING', get_option('memcpy-probing'))

build_conf.set10('DIRECT_BUILD_NETWORK', get_option('network'))

build_conf.set10('DIRECT_BUILD_PIPED_STREAM', get_option('piped-stream'))

build_conf.set10('DIRECT_BUILD_SENTINELS', get_option('sentinels'))

if get_option('os') == 'linux'
  os_sources = [
    'os/linux/clock.c',
    'os/linux/filesystem.c',
    'os/linux/log.c',
    'os/linux/mem.c',
    'os/linux/mutex.c',
    'os/linux/signals.c',
    'os/linux/system.c',
    'os/linux/thread.c',
  ]

  os_linux_headers = [
    'os/linux/filesystem.h',
    'os/linux/mutex.h',
    'os/linux/thread.h',
    'os/linux/types.h',
    'os/linux/waitqueue.h'
  ]

  build_conf.set('DIRECT_BUILD_OS_LINUX', true)
else
  error('Unsupported OS')
endif

build_conf.set10('DIRECT_BUILD_TEXT', get_option('text'))

if get_option('trace')
  add_global_arguments('-finstrument-functions', language: 'c')
endif
build_conf.set10('DIRECT_BUILD_TRACE', get_option('trace'))

configure_file(configuration: build_conf,
               output: 'build.h',
               install: true,
               install_dir: join_paths(get_option('includedir'), 'directfb/direct'))

libdl_dep = []
moduledir_def = []
if get_option('default_library') == 'shared'
  libdl_dep = cc.find_library('dl')
  moduledir_def = '-DMODULEDIR="@0@"'.format(moduledir)
endif

librt_dep = []
libdirect_private = []
if get_option('constructors')
  libdirect_private = ['-Wl,-u,__D_init_all']
endif
if not cc.has_function('clock_gettime')
  librt_dep = cc.find_library('rt')
  libdirect_private += ['-lrt']
endif
threads_dep = dependency('threads')
libdirect_private += ['-pthread']

libdirect = library('direct-@0@.@1@'.format(directfb_major_version, directfb_minor_version),
                    direct_sources, os_sources,
                    include_directories: [config_inc, lib_inc],
                    c_args: moduledir_def,
                    dependencies: [libdl_dep, librt_dep, threads_dep],
                    version: '0.@0@.0'.format(directfb_micro_version),
                    install: true)

install_symlink('libdirect' + libsuffix,
                pointing_to: 'libdirect-@0@.@1@'.format(directfb_major_version, directfb_minor_version) + libsuffix,
                install_dir: join_paths(get_option('prefix'), get_option('libdir')))

install_headers(direct_headers,   subdir: 'directfb/direct')
install_headers(os_headers,       subdir: 'directfb/direct/os')
if get_option('os') == 'linux'
  install_headers(os_linux_headers, subdir: 'directfb/direct/os/linux')
endif

pkgconfig.generate(filebase: 'direct',
                   name: 'Direct',
                   description: 'DirectFB low-level library',
                   libraries: '-L${libdir} -ldirect',
                   libraries_private: libdirect_private,
                   subdirs: 'directfb')

direct_dep = declare_dependency(include_directories: lib_inc, link_with: libdirect)

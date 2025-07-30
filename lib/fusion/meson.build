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

fusion_sources = [
  'arena.c',
  'call.c',
  'conf.c',
  'fusion.c',
  'hash.c',
  'init.c',
  'lock.c',
  'object.c',
  'reactor.c',
  'ref.c',
  'shmalloc.c',
  'vector.c'
]

fusion_headers = [
  'arena.h',
  'call.h',
  'conf.h',
  'fusion.h',
  'hash.h',
  'lock.h',
  'object.h',
  'protocol.h',
  'reactor.h',
  'ref.h',
  'shmalloc.h',
  'types.h',
  'vector.h',
]

if get_option('multi')
  shm_sources = [
    'shm/heap.c',
    'shm/pool.c',
    'shm/shm.c'
  ]
else
  shm_sources = 'shm/fake.c'
endif

shm_headers = [
  'shm/pool.h'
]

build_conf = configuration_data()

if get_option('multi')
  if get_option('os') == 'linux'
    if get_option('multi-kernel') and cc.has_header('linux/fusion.h')
        build_conf.set('FUSION_BUILD_KERNEL', 1)
    else
        build_conf.set('FUSION_BUILD_KERNEL', 0)
    endif
  endif
  build_conf.set('FUSION_MESSAGE_SIZE', get_option('message-size'))
endif

build_conf.set10('FUSION_BUILD_MULTI', get_option('multi'))

configure_file(configuration: build_conf,
               output: 'build.h',
               install: true,
               install_dir: get_option('includedir') / 'directfb/fusion')

fusion_private = []
if get_option('default_library') == 'static'
  fusion_private = 'direct'
endif

libfusion_private = []
if get_option('default_library') == 'static' and get_option('constructors')
  libfusion_private = '-Wl,-u,__Fusion_init_all'
endif

libfusion = library('fusion-@0@.@1@'.format(directfb_major_version, directfb_minor_version),
                    fusion_sources, shm_sources,
                    include_directories: [config_inc, lib_inc],
                    install_rpath: get_option('prefix') / get_option('libdir'),
                    dependencies: direct_dep,
                    version: '0.@0@.0'.format(directfb_micro_version),
                    install: true)

install_symlink('libfusion' + libsuffix,
                pointing_to: 'libfusion-@0@.@1@'.format(directfb_major_version, directfb_minor_version) + libsuffix,
                install_dir: get_option('prefix') / get_option('libdir'))

install_headers(fusion_headers, subdir: 'directfb/fusion')
install_headers(shm_headers,    subdir: 'directfb/fusion/shm')

pkgconfig.generate(filebase: 'fusion',
                   name: 'Fusion',
                   description: 'DirectFB IPC library',
                   requires_private: fusion_private,
                   libraries: '-L${libdir} -lfusion',
                   libraries_private: libfusion_private,
                   subdirs: 'directfb')

fusion_dep = declare_dependency(include_directories: lib_inc, link_with: libfusion)

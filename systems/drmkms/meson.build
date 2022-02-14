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

drmkms_dep = [dependency('libdrm', required: false), dependency('libkms', required: false)]

foreach dep : drmkms_dep
  if not dep.found()
    warning('Building without DRM/KMS support.')
    subdir_done()
  endif
endforeach

drmkms_sources = [
  'drmkms_layer.c',
  'drmkms_mode.c',
  'drmkms_screen.c',
  'drmkms_surface_pool.c',
  'drmkms_system.c',
  'drmkms_vt.c'
]

library('directfb_drmkms',
        drmkms_sources,
        dependencies: [directfb_dep, drmkms_dep],
        install: true,
        install_dir: join_paths(moduledir, 'systems'))
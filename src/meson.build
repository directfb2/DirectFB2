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

subdir('core')

directfb_sources = [
  'directfb.c',
  'directfb_result.c',
  'idirectfb.c',
  'init.c',
  'core/CoreDFB_real.c',
  'core/CoreGraphicsState_real.c',
  'core/CoreGraphicsStateClient.c',
  'core/CoreInputDevice_real.c',
  'core/CoreLayer_real.c',
  'core/CoreLayerContext_real.c',
  'core/CoreLayerRegion_real.c',
  'core/CorePalette_real.c',
  'core/CoreScreen_real.c',
  'core/CoreSlave_real.c',
  'core/CoreSurface_real.c',
  'core/CoreSurfaceAllocation_real.c',
  'core/CoreSurfaceClient_real.c',
  'core/CoreWindow_real.c',
  'core/CoreWindowStack_real.c',
  'core/clipboard.c',
  'core/colorhash.c',
  'core/core.c',
  'core/core_parts.c',
  'core/fonts.c',
  'core/gfxcard.c',
  'core/graphics_state.c',
  'core/input.c', directfb_keynames,
  'core/layer_context.c',
  'core/layer_control.c',
  'core/layer_region.c',
  'core/layers.c',
  'core/palette.c',
  'core/prealloc_surface_pool.c',
  'core/prealloc_surface_pool_bridge.c',
  'core/screen.c',
  'core/screens.c',
  'core/state.c',
  'core/surface.c',
  'core/surface_allocation.c',
  'core/surface_buffer.c',
  'core/surface_client.c',
  'core/surface_core.c',
  'core/surface_pool.c',
  'core/surface_pool_bridge.c',
  'core/system.c',
  'core/windows.c',
  'core/windowstack.c',
  'core/wm.c',
  'display/idirectfbdisplaylayer.c',
  'display/idirectfbpalette.c',
  'display/idirectfbscreen.c',
  'display/idirectfbsurface.c',
  'display/idirectfbsurfaceallocation.c',
  'display/idirectfbsurface_layer.c',
  'display/idirectfbsurface_window.c',
  'gfx/clip.c',
  'gfx/convert.c',
  'gfx/util.c',
  'gfx/generic/generic.c',
  'gfx/generic/generic_blit.c',
  'gfx/generic/generic_draw_line.c',
  'gfx/generic/generic_fill_rectangle.c',
  'gfx/generic/generic_stretch_blit.c',
  'gfx/generic/generic_texture_triangles.c',
  'gfx/generic/generic_util.c',
  'input/idirectfbeventbuffer.c',
  'input/idirectfbinputdevice.c',
  'media/idirectfbdatabuffer.c',
  'media/idirectfbdatabuffer_file.c',
  'media/idirectfbdatabuffer_memory.c',
  'media/idirectfbdatabuffer_streamed.c',
  'media/idirectfbfont.c',
  'media/idirectfbimageprovider.c',
  'media/idirectfbvideoprovider.c',
  'misc/conf.c',
  'misc/gfx_util.c',
  'misc/util.c', directfb_strings,
  'windows/idirectfbwindow.c'
]

core_headers = [
  'core/CoreGraphicsStateClient.h',
  'core/CoreSlave_includes.h',
  'core/core.h',
  'core/core_resourcemanager.h',
  'core/core_system.h',
  'core/coredefs.h',
  'core/coretypes.h',
  'core/fonts.h',
  'core/gfxcard.h',
  'core/graphics_driver.h',
  'core/input.h',
  'core/input_driver.h',
  'core/layer_context.h',
  'core/layer_control.h',
  'core/layer_region.h',
  'core/layers.h',
  'core/palette.h',
  'core/screen.h',
  'core/screens.h',
  'core/state.h',
  'core/surface.h',
  'core/surface_allocation.h',
  'core/surface_buffer.h',
  'core/surface_client.h',
  'core/surface_pool.h',
  'core/surface_pool_bridge.h',
  'core/system.h',
  'core/video_mode.h',
  'core/windows.h',
  'core/windowstack.h',
  'core/wm.h',
  'core/wm_module.h'
]

display_headers = [
  'display/idirectfbsurface.h',
]

gfx_headers = [
  'gfx/clip.h',
  'gfx/convert.h',
  'gfx/util.h'
]

media_headers = [
  'media/idirectfbdatabuffer.h',
  'media/idirectfbfont.h',
  'media/idirectfbimageprovider.h',
  'media/idirectfbvideoprovider.h'
]

misc_headers = [
  'misc/conf.h',
  'misc/gfx_util.h',
]

if get_option('multi')
  surface_pool_sources = [
    'core/shared_secure_surface_pool.c',
    'core/shared_surface_pool.c'
  ]
else
  surface_pool_sources = 'core/local_surface_pool.c'
endif

build_conf = configuration_data()

build_conf.set_quoted('DIRECTFB_VERSION_VENDOR', get_option('vendor-version'))

build_conf.set10('DFB_SMOOTH_SCALING', get_option('smooth-scaling'))

configure_file(configuration: build_conf,
               output: 'build.h',
               install: true,
               install_dir: get_option('includedir') / 'directfb-internal')

directfb_private = []
if get_option('default_library') == 'static'
  directfb_private = 'directfb-gfxdriver directfb-inputdriver directfb-interface directfb-system directfb-wm'
endif

libdirectfb_private = []
if get_option('default_library') == 'static' and get_option('constructors')
  libdirectfb_private = '-Wl,-u,__DFB_init_all'
endif

libdirectfb = library('directfb-@0@.@1@'.format(directfb_major_version, directfb_minor_version),
                      directfb_sources, flux_sources, surface_pool_sources,
                      include_directories: [config_inc, directfb_inc],
                      c_args: ['-DBUILDTIME="'  + run_command('date', '-u', '+%Y-%m-%d %H:%M',
                                                              check: true).stdout().strip()                  + '"',
                               '-DSYSCONFDIR="' + get_option('prefix') / get_option('sysconfdir') + '"'],
                      build_rpath: get_option('prefix') / get_option('libdir'),
                      dependencies: [direct_dep, fusion_dep],
                      version: '0.@0@.0'.format(directfb_micro_version),
                      install: true,
                      install_rpath: get_option('prefix') / get_option('libdir'))

install_symlink('libdirectfb' + libsuffix,
                pointing_to: 'libdirectfb-@0@.@1@'.format(directfb_major_version, directfb_minor_version) + libsuffix,
                install_dir: get_option('prefix') / get_option('libdir'))

install_headers('idirectfb.h',   subdir: 'directfb-internal')
install_headers(core_headers,    subdir: 'directfb-internal/core')
install_headers(display_headers, subdir: 'directfb-internal/display')
install_headers(gfx_headers,     subdir: 'directfb-internal/gfx')
install_headers(media_headers,   subdir: 'directfb-internal/media')
install_headers(misc_headers,    subdir: 'directfb-internal/misc')

pkgconfig.generate(filebase: 'directfb',
                   name: 'DirectFB',
                   description: 'Graphics and windowing library in Framebuffer memory',
                   requires: 'direct fusion',
                   requires_private: directfb_private,
                   libraries: '-L${libdir} -ldirectfb',
                   libraries_private: libdirectfb_private,
                   subdirs: 'directfb')

pkgconfig.generate(filebase: 'directfb-internal',
                   variables: ['moduledirname=' + moduledirname, 'moduledir=' + moduledir],
                   name: 'DirectFB-internal',
                   description: 'DirectFB internal module infrastructure',
                   requires: 'directfb',
                   subdirs: 'directfb-internal')

directfb_dep = declare_dependency(include_directories: directfb_inc, link_with: [libdirectfb, libdirect, libfusion])

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

include $(APPDIR)/Make.defs

CFLAGS += -I.
CFLAGS += -Isrc

CFLAGS += -DBUILDTIME=\"$(shell date -u "+%Y-%m-%d\ %H:%M")\"
CFLAGS += -DSYSCONFDIR=\"/etc\"

DIRECT_CSRCS  = lib/direct/clock.c
DIRECT_CSRCS += lib/direct/conf.c
DIRECT_CSRCS += lib/direct/debug.c
DIRECT_CSRCS += lib/direct/direct.c
DIRECT_CSRCS += lib/direct/direct_result.c
DIRECT_CSRCS += lib/direct/hash.c
DIRECT_CSRCS += lib/direct/init.c
DIRECT_CSRCS += lib/direct/interface.c
DIRECT_CSRCS += lib/direct/log.c
DIRECT_CSRCS += lib/direct/log_domain.c
DIRECT_CSRCS += lib/direct/map.c
DIRECT_CSRCS += lib/direct/mem.c
DIRECT_CSRCS += lib/direct/memcpy.c
DIRECT_CSRCS += lib/direct/messages.c
DIRECT_CSRCS += lib/direct/modules.c
DIRECT_CSRCS += lib/direct/result.c
DIRECT_CSRCS += lib/direct/stream.c
DIRECT_CSRCS += lib/direct/system.c
DIRECT_CSRCS += lib/direct/thread.c
DIRECT_CSRCS += lib/direct/trace.c
DIRECT_CSRCS += lib/direct/util.c
DIRECT_CSRCS += lib/direct/os/nuttx/clock.c
DIRECT_CSRCS += lib/direct/os/nuttx/filesystem.c
DIRECT_CSRCS += lib/direct/os/nuttx/log.c
DIRECT_CSRCS += lib/direct/os/nuttx/mem.c
DIRECT_CSRCS += lib/direct/os/nuttx/mutex.c
DIRECT_CSRCS += lib/direct/os/nuttx/signals.c
DIRECT_CSRCS += lib/direct/os/nuttx/system.c
DIRECT_CSRCS += lib/direct/os/nuttx/thread.c

FUSION_CSRCS  = lib/fusion/arena.c
FUSION_CSRCS += lib/fusion/call.c
FUSION_CSRCS += lib/fusion/conf.c
FUSION_CSRCS += lib/fusion/fusion.c
FUSION_CSRCS += lib/fusion/hash.c
FUSION_CSRCS += lib/fusion/init.c
FUSION_CSRCS += lib/fusion/lock.c
FUSION_CSRCS += lib/fusion/object.c
FUSION_CSRCS += lib/fusion/reactor.c
FUSION_CSRCS += lib/fusion/ref.c
FUSION_CSRCS += lib/fusion/shmalloc.c
FUSION_CSRCS += lib/fusion/vector.c
FUSION_CSRCS += lib/fusion/shm/fake.c

DIRECTFB_CSRCS  = src/directfb.c
DIRECTFB_CSRCS += src/directfb_result.c
DIRECTFB_CSRCS += src/idirectfb.c
DIRECTFB_CSRCS += src/init.c
DIRECTFB_CSRCS += src/core/CoreDFB_real.c
DIRECTFB_CSRCS += src/core/CoreGraphicsState_real.c
DIRECTFB_CSRCS += src/core/CoreGraphicsStateClient.c
DIRECTFB_CSRCS += src/core/CoreInputDevice_real.c
DIRECTFB_CSRCS += src/core/CoreLayer_real.c
DIRECTFB_CSRCS += src/core/CoreLayerContext_real.c
DIRECTFB_CSRCS += src/core/CoreLayerRegion_real.c
DIRECTFB_CSRCS += src/core/CorePalette_real.c
DIRECTFB_CSRCS += src/core/CoreScreen_real.c
DIRECTFB_CSRCS += src/core/CoreSlave_real.c
DIRECTFB_CSRCS += src/core/CoreSurface_real.c
DIRECTFB_CSRCS += src/core/CoreSurfaceAllocation_real.c
DIRECTFB_CSRCS += src/core/CoreSurfaceClient_real.c
DIRECTFB_CSRCS += src/core/CoreWindow_real.c
DIRECTFB_CSRCS += src/core/CoreWindowStack_real.c
DIRECTFB_CSRCS += src/core/clipboard.c
DIRECTFB_CSRCS += src/core/colorhash.c
DIRECTFB_CSRCS += src/core/core.c
DIRECTFB_CSRCS += src/core/core_parts.c
DIRECTFB_CSRCS += src/core/fonts.c
DIRECTFB_CSRCS += src/core/gfxcard.c
DIRECTFB_CSRCS += src/core/graphics_state.c
DIRECTFB_CSRCS += src/core/input.c
DIRECTFB_CSRCS += src/core/layer_context.c
DIRECTFB_CSRCS += src/core/layer_control.c
DIRECTFB_CSRCS += src/core/layer_region.c
DIRECTFB_CSRCS += src/core/layers.c
DIRECTFB_CSRCS += src/core/local_surface_pool.c
DIRECTFB_CSRCS += src/core/palette.c
DIRECTFB_CSRCS += src/core/prealloc_surface_pool_bridge.c
DIRECTFB_CSRCS += src/core/prealloc_surface_pool.c
DIRECTFB_CSRCS += src/core/screen.c
DIRECTFB_CSRCS += src/core/screens.c
DIRECTFB_CSRCS += src/core/state.c
DIRECTFB_CSRCS += src/core/surface.c
DIRECTFB_CSRCS += src/core/surface_allocation.c
DIRECTFB_CSRCS += src/core/surface_buffer.c
DIRECTFB_CSRCS += src/core/surface_client.c
DIRECTFB_CSRCS += src/core/surface_core.c
DIRECTFB_CSRCS += src/core/surface_pool.c
DIRECTFB_CSRCS += src/core/surface_pool_bridge.c
DIRECTFB_CSRCS += src/core/system.c
DIRECTFB_CSRCS += src/core/windows.c
DIRECTFB_CSRCS += src/core/windowstack.c
DIRECTFB_CSRCS += src/core/wm.c
DIRECTFB_CSRCS += src/display/idirectfbdisplaylayer.c
DIRECTFB_CSRCS += src/display/idirectfbpalette.c
DIRECTFB_CSRCS += src/display/idirectfbscreen.c
DIRECTFB_CSRCS += src/display/idirectfbsurface.c
DIRECTFB_CSRCS += src/display/idirectfbsurfaceallocation.c
DIRECTFB_CSRCS += src/display/idirectfbsurface_layer.c
DIRECTFB_CSRCS += src/display/idirectfbsurface_window.c
DIRECTFB_CSRCS += src/gfx/clip.c
DIRECTFB_CSRCS += src/gfx/convert.c
DIRECTFB_CSRCS += src/gfx/util.c
DIRECTFB_CSRCS += src/gfx/generic/generic.c
DIRECTFB_CSRCS += src/gfx/generic/generic_blit.c
DIRECTFB_CSRCS += src/gfx/generic/generic_draw_line.c
DIRECTFB_CSRCS += src/gfx/generic/generic_fill_rectangle.c
DIRECTFB_CSRCS += src/gfx/generic/generic_stretch_blit.c
DIRECTFB_CSRCS += src/gfx/generic/generic_texture_triangles.c
DIRECTFB_CSRCS += src/gfx/generic/generic_util.c
DIRECTFB_CSRCS += src/input/idirectfbeventbuffer.c
DIRECTFB_CSRCS += src/input/idirectfbinputdevice.c
DIRECTFB_CSRCS += src/media/idirectfbdatabuffer.c
DIRECTFB_CSRCS += src/media/idirectfbdatabuffer_file.c
DIRECTFB_CSRCS += src/media/idirectfbdatabuffer_memory.c
DIRECTFB_CSRCS += src/media/idirectfbdatabuffer_streamed.c
DIRECTFB_CSRCS += src/media/idirectfbfont.c
DIRECTFB_CSRCS += src/media/idirectfbimageprovider.c
DIRECTFB_CSRCS += src/media/idirectfbvideoprovider.c
DIRECTFB_CSRCS += src/misc/conf.c
DIRECTFB_CSRCS += src/misc/gfx_util.c
DIRECTFB_CSRCS += src/misc/util.c
DIRECTFB_CSRCS += src/windows/idirectfbwindow.c

FLUX_CSRCS  = src/core/CoreDFB.c
FLUX_CSRCS += src/core/CoreGraphicsState.c
FLUX_CSRCS += src/core/CoreInputDevice.c
FLUX_CSRCS += src/core/CoreLayer.c
FLUX_CSRCS += src/core/CoreLayerContext.c
FLUX_CSRCS += src/core/CoreLayerRegion.c
FLUX_CSRCS += src/core/CorePalette.c
FLUX_CSRCS += src/core/CoreSlave.c
FLUX_CSRCS += src/core/CoreScreen.c
FLUX_CSRCS += src/core/CoreSurface.c
FLUX_CSRCS += src/core/CoreSurfaceAllocation.c
FLUX_CSRCS += src/core/CoreSurfaceClient.c
FLUX_CSRCS += src/core/CoreWindow.c
FLUX_CSRCS += src/core/CoreWindowStack.c
FLUX_HDRS = $(patsubst %.c,%.h,$(FLUX_CSRCS))

CSRCS = $(DIRECT_CSRCS) $(FUSION_CSRCS) $(DIRECTFB_CSRCS) $(FLUX_CSRCS)

CORE_SYSTEM_CSRCS = systems/dummy/dummy.c
CSRCS += $(CORE_SYSTEM_CSRCS)

FONT_PROVIDER_CSRCS = interfaces/IDirectFBFont/idirectfbfont_dgiff.c
CSRCS += $(FONT_PROVIDER_CSRCS)

IMAGE_PROVIDER_CSRCS = interfaces/IDirectFBImageProvider/idirectfbimageprovider_dfiff.c
CSRCS += $(IMAGE_PROVIDER_CSRCS)

VIDEO_PROVIDER_CSRCS = interfaces/IDirectFBVideoProvider/idirectfbvideoprovider_dfvff.c
CSRCS += $(VIDEO_PROVIDER_CSRCS)

WINDOW_MANAGER_CSRCS = wm/default/default.c
CSRCS += $(WINDOW_MANAGER_CSRCS)

config.h:
	$(Q) echo "#pragma once" > $@; \
		echo "#define SIZEOF_LONG 8" >> $@

include/directfb_build.h:
	$(Q) echo "#pragma once" > $@; \
		echo "#define FLUXED_ARGS_BYTES 1024" >> $@

include/directfb_keynames.h: include/directfb_keyboard.h
	$(Q) ./include/gen_directfb_keynames.sh $^ > $@

include/directfb_strings.h: include/directfb.h
	$(Q) ./include/gen_directfb_strings.sh $^ > $@

include/directfb_version.h:
	$(Q) echo "#pragma once" > $@; \
		echo "#define DIRECTFB_MAJOR_VERSION 2" >> $@; \
		echo "#define DIRECTFB_MICRO_VERSION 0" >> $@; \
		echo "#define DIRECTFB_MINOR_VERSION 0" >> $@

lib/direct/build.h:
	$(Q) echo "#pragma once" > $@; \
		echo "#define DIRECT_BUILD_CTORS 0" >> $@; \
		echo "#define DIRECT_BUILD_DEBUG 0" >> $@; \
		echo "#define DIRECT_BUILD_DEBUGS 0" >> $@; \
		echo "#define DIRECT_BUILD_DYNLOAD 0" >> $@; \
		echo "#define DIRECT_BUILD_MEMCPY_PROBING 0" >> $@; \
		echo "#define DIRECT_BUILD_NETWORK 0" >> $@; \
		echo "#define DIRECT_BUILD_OS_NUTTX 1" >> $@; \
		echo "#define DIRECT_BUILD_PIPED_STREAM 0" >> $@; \
		echo "#define DIRECT_BUILD_SENTINELS 0" >> $@; \
		echo "#define DIRECT_BUILD_TEXT 0" >> $@; \
		echo "#define DIRECT_BUILD_TRACE 0" >> $@

lib/fusion/build.h:
	$(Q) echo "#pragma once" > $@; \
		echo "#define FUSION_BUILD_MULTI 0" >> $@

src/build.h:
	$(Q) echo "#pragma once" > $@; \
		echo "#define DFB_SMOOTH_SCALING 1" >> $@; \
		echo "#define DIRECTFB_VERSION_VENDOR \"\"" >> $@

FLUXCOMP ?= fluxcomp
FLUX_ARGS = --call-mode --dispatch-error-abort --identity --include-prefix=core --object-ptrs --static-args-bytes=FLUXED_ARGS_BYTES

src/core/%.c: src/core/%.flux
	$(FLUXCOMP) $(FLUX_ARGS) -c $^ -o=src/core

context:: config.h include/directfb_build.h include/directfb_keynames.h include/directfb_strings.h include/directfb_version.h lib/direct/build.h lib/fusion/build.h src/build.h $(FLUX_CSRCS)

distclean::
	$(call DELFILE, config.h)
	$(call DELFILE, include/directfb_build.h)
	$(call DELFILE, include/directfb_keynames.h)
	$(call DELFILE, include/directfb_strings.h)
	$(call DELFILE, include/directfb_version.h)
	$(call DELFILE, lib/direct/build.h)
	$(call DELFILE, lib/fusion/build.h)
	$(call DELFILE, src/build.h)
	$(call DELFILE, $(FLUX_CSRCS))
	$(call DELFILE, $(FLUX_HDRS))

include $(APPDIR)/Application.mk

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

flux_files = [
  'CoreDFB',
  'CoreGraphicsState',
  'CoreInputDevice',
  'CoreLayer',
  'CoreLayerContext',
  'CoreLayerRegion',
  'CorePalette',
  'CoreSlave',
  'CoreScreen',
  'CoreSurface',
  'CoreSurfaceAllocation',
  'CoreSurfaceClient',
  'CoreWindow',
  'CoreWindowStack'
]

fluxcomp = find_program('fluxcomp')

flux_sources = []
foreach flux_file : flux_files
  flux_sources += custom_target(flux_file,
                                input: flux_file + '.flux',
                                output: [flux_file + '.c', flux_file + '.h'],
                                command: [fluxcomp, '--call-mode',
                                                    '--dispatch-error-abort',
                                                    '--identity',
                                                    '--include-prefix=core',
                                                    '--object-ptrs',
                                                    '--static-args-bytes=FLUXED_ARGS_BYTES',
                                                    '-c', '@INPUT@', '-o=src/core'])
endforeach

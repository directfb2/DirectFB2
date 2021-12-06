#!/bin/sh
#
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

mknames() {
ENUM=$1
PREFIX=$2
NULL=$3
NAME=$4
VALUE=$5
HEADER=$6

cat << EOF

struct DFB${NAME}Name {
     ${ENUM} ${VALUE};
     const char *name;
};

#define DirectFB${NAME}Names(Identifier) struct DFB${NAME}Name Identifier[] = { \\
EOF

egrep "^ +${PREFIX}_[0-9A-Za-z_]+[ ,]" $HEADER | grep -v ${PREFIX}_${NULL} | perl -p -e "s/^\\s*(${PREFIX}_)([\\w_]+)[ ,].*/     \\{ \\1\\2, \\\"\\2\\\" \\}, \\\\/"

cat << EOF
     { ($ENUM) ${PREFIX}_${NULL}, "${NULL}" } \\
};
EOF
}

echo \#ifndef __DIRECTFB_STRINGS_H__
echo \#define __DIRECTFB_STRINGS_H__
echo
echo \#include \<directfb.h\>
mknames DFBSurfacePixelFormat DSPF UNKNOWN PixelFormat format  $1
mknames DFBSurfacePorterDuffRule DSPD NONE PorterDuffRule rule $1
mknames DFBSurfaceCapabilities DSCAPS NONE SurfaceCapabilities capability $1 | grep -v DSCAPS_FLIPPING | grep -v DSCAPS_ALL
mknames DFBSurfaceColorSpace DSCS UNKNOWN ColorSpace colorspace $1
mknames DFBInputDeviceTypeFlags DIDTF NONE InputDeviceTypeFlags type $1 | grep -v DIDTF_ALL
mknames DFBSurfaceDrawingFlags DSDRAW NOFX SurfaceDrawingFlags flag $1 | grep -v DSDRAW_ALL
mknames DFBSurfaceBlittingFlags DSBLIT NOFX SurfaceBlittingFlags flag $1 | grep -v DSBLIT_ALL
mknames DFBSurfaceFlipFlags DSFLIP NONE SurfaceFlipFlags flag $1 | grep -v DSFLIP_ALL
mknames DFBSurfaceBlendFunction DSBF UNKNOWN SurfaceBlendFunction function $1
mknames DFBInputDeviceCapabilities DICAPS NONE InputDeviceCapabilities capability $1 | grep -v DICAPS_ALL | grep -v DICAPS_ALPHACHANNEL | grep -v DICAPS_COLORKEY
mknames DFBDisplayLayerTypeFlags DLTF NONE DisplayLayerTypeFlags type $1 | grep -v DLTF_ALL
mknames DFBDisplayLayerCapabilities DLCAPS NONE DisplayLayerCapabilities capability $1 | grep -v DLCAPS_ALL
mknames DFBDisplayLayerBufferMode DLBM UNKNOWN DisplayLayerBufferMode mode $1 | grep -v DLBM_DONTCARE | grep -v DLBM_COLOR | grep -v DLBM_IMAGE | grep -v DLBM_TILE
mknames DFBWindowCapabilities DWCAPS NONE WindowCapabilities capability $1 | grep -v DWCAPS_ALL
mknames DFBDisplayLayerOptions DLOP NONE DisplayLayerOptions option $1 | grep -v DLOP_ALL
mknames DFBWindowOptions DWOP NONE WindowOptions option $1 | grep -v DWOP_ALL
mknames DFBScreenCapabilities DSCCAPS NONE ScreenCapabilities capability $1 | grep -v DSCCAPS_ALL
mknames DFBScreenEncoderCapabilities DSECAPS NONE ScreenEncoderCapabilities capability $1 | grep -v DSECAPS_ALL
mknames DFBScreenEncoderType DSET UNKNOWN ScreenEncoderType type $1 | grep -v DSET_ALL
mknames DFBScreenEncoderTVStandards DSETV UNKNOWN ScreenEncoderTVStandards standard $1 | grep -v DSETV_ALL
mknames DFBScreenOutputCapabilities DSOCAPS NONE ScreenOutputCapabilities capability $1 | grep -v DSOCAPS_ALL
mknames DFBScreenOutputConnectors DSOC UNKNOWN ScreenOutputConnectors connector $1 | grep -v DSOC_ALL
mknames DFBScreenOutputSignals DSOS NONE ScreenOutputSignals signal $1 | grep -v DSOS_ALL
mknames DFBScreenOutputSlowBlankingSignals DSOSB OFF ScreenOutputSlowBlankingSignals slow_signal $1 | grep -v DSOSB_ALL
mknames DFBScreenOutputResolution DSOR UNKNOWN ScreenOutputResolution resolution $1 | grep -v DSOR_ALL
mknames DFBScreenMixerCapabilities DSMCAPS NONE ScreenMixerCapabilities capability $1 | grep -v DSMCAPS_ALL
mknames DFBScreenMixerTree DSMT UNKNOWN ScreenMixerTree tree $1 | grep -v DSMT_ALL
mknames DFBScreenEncoderTestPicture DSETP OFF ScreenEncoderTestPicture test_picture $1 | grep -v DSETP_ALL
mknames DFBScreenEncoderScanMode DSESM UNKNOWN ScreenEncoderScanMode scan_mode $1 | grep -v DSESM_ALL
mknames DFBScreenEncoderConfigFlags DSECONF UNKNOWN ScreenEncoderConfigFlags config_flags $1 | grep -v DSECONF_ALL
mknames DFBScreenEncoderFrequency DSEF UNKNOWN ScreenEncoderFrequency frequency $1 | grep -v DSEF_ALL
mknames DFBScreenEncoderPictureFraming DSEPF UNKNOWN ScreenEncoderPictureFraming framing $1 | grep -v DSEPF_ALL
mknames DFBAccelerationMask DFXL NONE AccelerationMask mask $1 | grep -v DFXL_ALL
echo
echo \#endif

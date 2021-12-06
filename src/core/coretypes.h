/*
   This file is part of DirectFB.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
*/

#ifndef __CORE__CORETYPES_H__
#define __CORE__CORETYPES_H__

#include <build.h>
#include <directfb.h>

/**********************************************************************************************************************/

typedef struct __DFB_CardState               CardState;
typedef struct __DFB_CoreCleanup             CoreCleanup;
typedef struct __DFB_CoreDFB                 CoreDFB;
typedef struct __DFB_CoreGraphicsState       CoreGraphicsState;
typedef struct __DFB_CoreGraphicsStateClient CoreGraphicsStateClient;
typedef struct __DFB_CoreFont                CoreFont;
typedef struct __DFB_CoreFontCache           CoreFontCache;
typedef struct __DFB_CoreFontCacheRow        CoreFontCacheRow;
typedef struct __DFB_CoreFontManager         CoreFontManager;
typedef struct __DFB_CoreGlyphData           CoreGlyphData;
typedef struct __DFB_CoreInputDevice         CoreInputDevice;
typedef struct __DFB_CoreLayer               CoreLayer;
typedef struct __DFB_CoreLayerContext        CoreLayerContext;
typedef struct __DFB_CoreLayerRegion         CoreLayerRegion;
typedef struct __DFB_CoreMemoryPermission    CoreMemoryPermission;
typedef struct __DFB_CorePalette             CorePalette;
typedef struct __DFB_CoreScreen              CoreScreen;
typedef struct __DFB_CoreSurface             CoreSurface;
typedef struct __DFB_CoreSurfaceAllocation   CoreSurfaceAllocation;
typedef struct __DFB_CoreSurfaceBuffer       CoreSurfaceBuffer;
typedef struct __DFB_CoreSurfaceBufferLock   CoreSurfaceBufferLock;
typedef struct __DFB_CoreSurfaceClient       CoreSurfaceClient;
typedef struct __DFB_CoreSurfaceConfig       CoreSurfaceConfig;
typedef struct __DFB_CoreSurfacePool         CoreSurfacePool;
typedef struct __DFB_CoreSurfacePoolBridge   CoreSurfacePoolBridge;
typedef struct __DFB_CoreWindow              CoreWindow;
typedef struct __DFB_CoreWindowConfig        CoreWindowConfig;
typedef struct __DFB_CoreWindowStack         CoreWindowStack;
typedef struct __DFB_GraphicsDeviceInfo      GraphicsDeviceInfo;
typedef struct __DFB_GraphicsDriverInfo      GraphicsDriverInfo;
typedef struct __DFB_GenefxState             GenefxState;

/**********************************************************************************************************************/

typedef struct {
     unsigned int serial;
     unsigned int generation;
} CoreGraphicsSerial;

/**********************************************************************************************************************/

typedef enum {
     CWMGT_KEYBOARD        = 0x00000000,
     CWMGT_POINTER         = 0x00000001,
     CWMGT_KEY             = 0x00000002,
     CWMGT_UNSELECTED_KEYS = 0x00000003,
} CoreWMGrabTarget;

/**********************************************************************************************************************/

typedef enum {
     CSAID_NONE    = 0x00000000, /* none or unknown accessor */

     CSAID_CPU     = 0x00000001, /* local processor */

     CSAID_GPU     = 0x00000002, /* primary accelerator, as in traditional 'gfxcard' core (ACCEL0) */

     CSAID_ACCEL0  = 0x00000002, /* accelerators, decoders (CSAID_ACCEL0 + accel_id<0-5>) */
     CSAID_ACCEL1  = 0x00000003,
     CSAID_ACCEL2  = 0x00000004,
     CSAID_ACCEL3  = 0x00000005,
     CSAID_ACCEL4  = 0x00000006,
     CSAID_ACCEL5  = 0x00000007,

     CSAID_LAYER0  = 0x00000008, /* display layers (CSAID_LAYER0 + layer_id<0-MAX_LAYERS>) */
     CSAID_LAYER1  = 0x00000009,
     CSAID_LAYER2  = 0x0000000a,
     CSAID_LAYER3  = 0x0000000b,
     CSAID_LAYER4  = 0x0000000c,
     CSAID_LAYER5  = 0x0000000d,
     CSAID_LAYER6  = 0x0000000e,
     CSAID_LAYER7  = 0x0000000f,
     CSAID_LAYER8  = 0x00000010,
     CSAID_LAYER9  = 0x00000011,
     CSAID_LAYER10 = 0x00000012,
     CSAID_LAYER11 = 0x00000013,
     CSAID_LAYER12 = 0x00000014,
     CSAID_LAYER13 = 0x00000015,
     CSAID_LAYER14 = 0x00000016,
     CSAID_LAYER15 = 0x00000017,

     _CSAID_NUM    = 0x00000018, /* number of statically assigned IDs for usage in static arrays */

     CSAID_ANY     = 0x00000100  /* any other accessor needs to be registered using IDs starting from here */
} CoreSurfaceAccessorID;

typedef unsigned int CoreSurfacePoolID;

typedef unsigned int CoreSurfacePoolBridgeID;

#endif

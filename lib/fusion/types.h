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

#ifndef __FUSION__TYPES_H__
#define __FUSION__TYPES_H__

#include <direct/messages.h>
#include <fusion/build.h>

/**********************************************************************************************************************/

#define FUSION_API

#define FCEF_NODIRECT 0x80

#define FUSION_SHM_TMPFS_PATH_NAME_LEN 64

/**********************************************************************************************************************/

typedef struct __Fusion_FusionArena         FusionArena;
typedef struct __Fusion_FusionCall          FusionCall;
typedef struct __Fusion_FusionHash          FusionHash;
typedef struct __Fusion_FusionObject        FusionObject;
typedef struct __Fusion_FusionObjectPool    FusionObjectPool;
typedef struct __Fusion_FusionReactor       FusionReactor;
typedef struct __Fusion_FusionRef           FusionRef;
typedef struct __Fusion_FusionSHMPool       FusionSHMPool;
typedef struct __Fusion_FusionSHMPoolShared FusionSHMPoolShared;
typedef struct __Fusion_FusionSHM           FusionSHM;
typedef struct __Fusion_FusionSHMShared     FusionSHMShared;
typedef struct __Fusion_FusionWorld         FusionWorld;
typedef struct __Fusion_FusionWorldShared   FusionWorldShared;

/**********************************************************************************************************************/

#if FUSION_BUILD_MULTI && FUSION_BUILD_KERNEL

#include <linux/fusion.h>

#define FUSION_CALL_MAX_LENGTH (FUSION_MESSAGE_SIZE - sizeof(FusionReadMessage))

#else /* FUSION_BUILD_MULTI && FUSION_BUILD_KERNEL */

typedef unsigned long FusionID;

typedef enum {
     FCEF_NONE   = 0x00000000,
     FCEF_ONEWAY = 0x00000001,
     FCEF_QUEUE  = 0x00000002,
     FCEF_ALL    = 0x00000003
} FusionCallExecFlags;

#if FUSION_BUILD_MULTI

#include <fusion/protocol.h>

#endif /* FUSION_BUILD_MULTI */

#define FUSION_ID_MASTER 1L

#define FUSION_CALL_MAX_LENGTH (64 * 1024)

#endif /* FUSION_BUILD_MULTI && FUSION_BUILD_KERNEL */

#if FUSION_BUILD_MULTI
#define D_OOSHM() direct_messages_ooshm( __FUNCTION__, __FILE__, __LINE__ )
#else /* FUSION_BUILD_MULTI */
#define D_OOSHM() D_OOM()
#endif /* FUSION_BUILD_MULTI */

#endif

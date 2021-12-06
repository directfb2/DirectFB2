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

#ifndef __FUSION_PROTOCOL_H__
#define __FUSION_PROTOCOL_H__

#include <fusion/call.h>

/**********************************************************************************************************************/

typedef enum {
     FMT_SEND    = 0x00000000,
     FMT_ENTER   = 0x00000001,
     FMT_LEAVE   = 0x00000002,
     FMT_CALL    = 0x00000003,
     FMT_CALLRET = 0x00000004,
     FMT_REACTOR = 0x00000005
} FusionMessageType;

/*
 * Enter world (slave).
 */
typedef struct {
     FusionMessageType type;

     FusionID          fusion_id;
} FusionEnter;

/*
 * Leave the world (slave).
 */
typedef struct {
     FusionMessageType type;

     FusionID          fusion_id;
} FusionLeave;

/*
 * Execute a call.
 */
typedef struct {
     FusionMessageType    type;

     unsigned int         serial;

     FusionID             caller;
     int                  call_id;
     int                  call_arg;
     unsigned int         call_length; /* Length of data. */
     unsigned int         ret_length;  /* Maximum length of return data. */

     void                *handler;
     void                *handler3;
     void                *ctx;

     FusionCallExecFlags  flags;
} FusionCallMessage;

/*
 * Send call return.
 */
typedef struct {
     FusionMessageType type;

     unsigned int      length;
} FusionCallReturn;

/*
 * Send reactor message.
 */
typedef struct {
     FusionMessageType  type;

     int                id;
     int                channel;

     FusionRef         *ref;
} FusionReactorMessage;

typedef union {
     FusionMessageType    type;

     FusionEnter          enter;
     FusionLeave          leave;
     FusionCallMessage    call;
     FusionCallReturn     callret;
     FusionReactorMessage reactor;
} FusionMessage;

#endif

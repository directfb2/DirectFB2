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

#ifndef __FUSION__CALL_H__
#define __FUSION__CALL_H__

#include <fusion/types.h>

/**********************************************************************************************************************/

typedef enum {
     FCHR_RETURN = 0x00000000,
     FCHR_RETAIN = 0x00000001
} FusionCallHandlerResult;

typedef FusionCallHandlerResult (*FusionCallHandler) ( int           caller,    /* Fusion id of the caller. */
                                                       int           call_arg,  /* Optional call parameter. */
                                                       void         *call_ptr,  /* Optional call parameter. */
                                                       void         *ctx,       /* Optional handler context. */
                                                       unsigned int  serial,
                                                       int          *ret_val );

typedef FusionCallHandlerResult (*FusionCallHandler3)( int           caller,    /* Fusion id of the caller. */
                                                       int           call_arg,  /* Optional call parameter. */
                                                       void         *ptr,       /* Optional call parameter. */
                                                       unsigned int  length,    /* Length. */
                                                       void         *ctx,       /* Optional handler context. */
                                                       unsigned int  serial,
                                                       void         *ret_ptr,
                                                       unsigned int  ret_size,
                                                       unsigned int *ret_length );

struct __Fusion_FusionCall {
     FusionWorldShared  *shared;
     int                 call_id;
     FusionID            fusion_id;
     FusionCallHandler   handler;
     FusionCallHandler3  handler3;
     void               *ctx;
};

/**********************************************************************************************************************/

typedef enum {
     FUSION_CALL_PERMIT_NONE    = 0x00000000,

     FUSION_CALL_PERMIT_EXECUTE = 0x00000001,

     FUSION_CALL_PERMIT_ALL     = 0x00000001
} FusionCallPermissions;

/**********************************************************************************************************************/

DirectResult FUSION_API fusion_call_init           ( FusionCall            *call,
                                                     FusionCallHandler      handler,
                                                     void                  *ctx,
                                                     const FusionWorld     *world );

DirectResult FUSION_API fusion_call_init3          ( FusionCall            *call,
                                                     FusionCallHandler3     handler3,
                                                     void                  *ctx,
                                                     const FusionWorld     *world );

DirectResult FUSION_API fusion_call_init_from      ( FusionCall            *call,
                                                     int                    call_id,
                                                     const FusionWorld     *world );

DirectResult FUSION_API fusion_call_set_name       ( FusionCall            *call,
                                                     const char            *name );

DirectResult FUSION_API fusion_call_execute        ( FusionCall            *call,
                                                     FusionCallExecFlags    flags,
                                                     int                    call_arg,
                                                     void                  *call_ptr,
                                                     int                   *ret_val );

DirectResult FUSION_API fusion_call_execute2       ( FusionCall            *call,
                                                     FusionCallExecFlags    flags,
                                                     int                    call_arg,
                                                     void                  *ptr,
                                                     unsigned int           length,
                                                     int                   *ret_val );

DirectResult FUSION_API fusion_call_execute3       ( FusionCall            *call,
                                                     FusionCallExecFlags    flags,
                                                     int                    call_arg,
                                                     void                  *ptr,
                                                     unsigned int           length,
                                                     void                  *ret_ptr,
                                                     unsigned int           ret_size,
                                                     unsigned int          *ret_length );

DirectResult FUSION_API fusion_world_flush_calls   ( FusionWorld           *world,
                                                     int                    lock );

DirectResult FUSION_API fusion_call_return         ( FusionCall            *call,
                                                     unsigned int           serial,
                                                     int                    val );

DirectResult FUSION_API fusion_call_return3        ( FusionCall            *call,
                                                     unsigned int           serial,
                                                     void                  *ptr,
                                                     unsigned int           length );

DirectResult FUSION_API fusion_call_get_owner      ( FusionCall            *call,
                                                     FusionID              *ret_fusion_id );

DirectResult FUSION_API fusion_call_set_quota      ( FusionCall            *call,
                                                     FusionID               fusion_id,
                                                     unsigned int           limit );

DirectResult FUSION_API fusion_call_destroy        ( FusionCall            *call );

/*
 * Give permissions to another fusionee to use the call.
 */
DirectResult FUSION_API fusion_call_add_permissions( FusionCall            *call,
                                                     FusionID               fusion_id,
                                                     FusionCallPermissions  permissions );

/**********************************************************************************************************************/

void __Fusion_call_init  ( void );
void __Fusion_call_deinit( void );

#endif

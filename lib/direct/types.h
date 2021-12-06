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

#ifndef __DIRECT__TYPES_H__
#define __DIRECT__TYPES_H__

#ifndef __DIRECT__OS__TYPES_H__
#include <direct/os/types.h>
#endif

/**********************************************************************************************************************/

#define DIRECT_API

#define D_RESULT_TYPE_CHAR_MASK  ((unsigned int) 0x2F)
#define D_RESULT_TYPE_CHAR_MIN   ((unsigned int) 0x30)
#define D_RESULT_TYPE_CHAR_MAX   (D_RESULT_TYPE_CHAR_MIN + D_RESULT_TYPE_CHAR_MASK)
#define D_RESULT_TYPE_CHAR_MUL_0 ((unsigned int) 1)
#define D_RESULT_TYPE_CHAR_MUL_1 ((unsigned int) (D_RESULT_TYPE_CHAR_MASK + 1))
#define D_RESULT_TYPE_CHAR_MUL_2 (D_RESULT_TYPE_CHAR_MUL_1 * D_RESULT_TYPE_CHAR_MUL_1)
#define D_RESULT_TYPE_CHAR_MUL_3 (D_RESULT_TYPE_CHAR_MUL_1 * D_RESULT_TYPE_CHAR_MUL_2)
#define D_RESULT_TYPE_CHAR(C)    (((unsigned int) (C) - D_RESULT_TYPE_CHAR_MIN) & D_RESULT_TYPE_CHAR_MASK)
#define D_RESULT_TYPE_SPACE      ((unsigned int) (0xFFFFFFFF / (D_RESULT_TYPE_CHAR_MASK * D_RESULT_TYPE_CHAR_MUL_3 + \
                                                                D_RESULT_TYPE_CHAR_MASK * D_RESULT_TYPE_CHAR_MUL_2 + \
                                                                D_RESULT_TYPE_CHAR_MASK * D_RESULT_TYPE_CHAR_MUL_1 + \
                                                                D_RESULT_TYPE_CHAR_MASK * D_RESULT_TYPE_CHAR_MUL_0)  \
                                                  - 1))

/**********************************************************************************************************************/

/*
 * Generate result code base for API
 *
 * Allowed are ASCII values between (inclusive) D_RESULT_TYPE_CHAR_MIN (0x30) and D_RESULT_TYPE_CHAR_MAX (0x5F)
 *
 *  0  1  2  3  4  5  6  7  8  9  :  ;  <  =  >  ?
 *
 *  @  A  B  C  D  E  F  G  H  I  J  K  L  M  N  O
 *
 *  P  Q  R  S  T  U  V  W  X  Y  Z  [  \  ]  ^  _
 *
 */
#define D_RESULT_TYPE_CODE_BASE(a,b,c,d) ((D_RESULT_TYPE_CHAR( a ) * (D_RESULT_TYPE_CHAR_MUL_3) +  \
                                           D_RESULT_TYPE_CHAR( b ) * (D_RESULT_TYPE_CHAR_MUL_2) +  \
                                           D_RESULT_TYPE_CHAR( c ) * (D_RESULT_TYPE_CHAR_MUL_1) +  \
                                           D_RESULT_TYPE_CHAR( d ) * (D_RESULT_TYPE_CHAR_MUL_0)) * D_RESULT_TYPE_SPACE)

#define D_RESULT_TYPE(code)              ((code) - ((code) % D_RESULT_TYPE_SPACE))
#define D_RESULT_INDEX(code)             ((code) % D_RESULT_TYPE_SPACE)

/**********************************************************************************************************************/

typedef enum {
     DR_OK,              /* No error occurred */

     DR__RESULT_BASE = D_RESULT_TYPE_CODE_BASE( 'D','R','_', '1' ),

     DR_FAILURE,         /* A general or unknown error occurred */
     DR_INIT,            /* A general initialization error occurred */
     DR_BUG,             /* Internal bug or inconsistency has been detected */
     DR_DEAD,            /* Interface has a zero reference counter (available in debug mode) */
     DR_UNSUPPORTED,     /* The requested operation or an argument is (currently) not supported */
     DR_UNIMPLEMENTED,   /* The requested operation is not implemented, yet */
     DR_ACCESSDENIED,    /* Access to the resource is denied */
     DR_INVAREA,         /* An invalid area has been specified or detected */
     DR_INVARG,          /* An invalid argument has been specified */
     DR_NOLOCALMEMORY,   /* There's not enough local system memory */
     DR_NOSHAREDMEMORY,  /* There's not enough shared system memory */
     DR_LOCKED,          /* The resource is (already) locked */
     DR_BUFFEREMPTY,     /* The buffer is empty */
     DR_FILENOTFOUND,    /* The specified file has not been found */
     DR_IO,              /* A general I/O error occurred */
     DR_BUSY,            /* The resource or device is busy */
     DR_NOIMPL,          /* No implementation for this interface or content type has been found */
     DR_TIMEOUT,         /* The operation timed out */
     DR_THIZNULL,        /* 'thiz' pointer is NULL */
     DR_IDNOTFOUND,      /* No resource has been found by the specified id */
     DR_DESTROYED,       /* The requested object has been destroyed */
     DR_FUSION,          /* Internal fusion error detected, most likely related to IPC resources */
     DR_BUFFERTOOLARGE,  /* Buffer is too large */
     DR_INTERRUPTED,     /* The operation has been interrupted */
     DR_NOCONTEXT,       /* No context available */
     DR_TEMPUNAVAIL,     /* Temporarily unavailable */
     DR_LIMITEXCEEDED,   /* Attempted to exceed limit, i.e. any kind of maximum size, count etc */
     DR_NOSUCHMETHOD,    /* Requested method is not known */
     DR_NOSUCHINSTANCE,  /* Requested instance is not known */
     DR_ITEMNOTFOUND,    /* No such item found */
     DR_VERSIONMISMATCH, /* Some versions didn't match */
     DR_EOF,             /* Reached end of file */
     DR_SUSPENDED,       /* The requested object is suspended */
     DR_INCOMPLETE,      /* The operation has been executed, but not completely */
     DR_NOCORE,          /* Core part not available */
     DR_SIGNALLED,       /* Received a signal, e.g. while waiting */
     DR_TASK_NOT_FOUND,  /* The corresponding task has not been found */

     DR__RESULT_END
} DirectResult;

typedef enum {
     DENUM_OK     = 0,   /* Proceed with enumeration */
     DENUM_CANCEL = 1,   /* Cancel enumeration */
     DENUM_REMOVE = 2    /* Remove item */
} DirectEnumerationResult;

/**********************************************************************************************************************/

typedef struct __D_DirectCleanupHandler    DirectCleanupHandler;
typedef struct __D_DirectHash              DirectHash;
typedef struct __D_DirectLink              DirectLink;
typedef struct __D_DirectLog               DirectLog;
typedef struct __D_DirectMap               DirectMap;
typedef struct __D_DirectModuleDir         DirectModuleDir;
typedef struct __D_DirectModuleEntry       DirectModuleEntry;
typedef struct __D_DirectSignalHandler     DirectSignalHandler;
typedef struct __D_DirectStream            DirectStream;
typedef struct __D_DirectTraceBuffer       DirectTraceBuffer;
typedef struct __D_DirectThread            DirectThread;
typedef struct __D_DirectThreadInitHandler DirectThreadInitHandler;

#endif

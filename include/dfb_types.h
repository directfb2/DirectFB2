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

#ifndef __DFB_TYPES_H__
#define __DFB_TYPES_H__

#include <direct/types.h>

#define DIRECTFB_API

/*
 * A boolean.
 */
typedef enum {
     DFB_FALSE           = 0,                  /* false */
     DFB_TRUE            = !DFB_FALSE          /* true */
} DFBBoolean;

/*
 * Return code of all interface methods and most functions.
 *
 * Whenever a method has to return any information, it is done via output parameters.
 */
typedef enum {
     /*
      * Aliases for backward compatibility and uniform look in DirectFB code
      */
     DFB_OK              = DR_OK,              /* No error occurred. */
     DFB_FAILURE         = DR_FAILURE,         /* A general or unknown error occurred. */
     DFB_INIT            = DR_INIT,            /* A general initialization error occurred. */
     DFB_BUG             = DR_BUG,             /* Internal bug or inconsistency has been detected. */
     DFB_DEAD            = DR_DEAD,            /* Interface has a zero reference counter (available in debug mode). */
     DFB_UNSUPPORTED     = DR_UNSUPPORTED,     /* The requested operation or an argument is (currently) not supported. */
     DFB_UNIMPLEMENTED   = DR_UNIMPLEMENTED,   /* The requested operation is not implemented, yet. */
     DFB_ACCESSDENIED    = DR_ACCESSDENIED,    /* Access to the resource is denied. */
     DFB_INVAREA         = DR_INVAREA,         /* An invalid area has been specified or detected. */
     DFB_INVARG          = DR_INVARG,          /* An invalid argument has been specified. */
     DFB_NOSYSTEMMEMORY  = DR_NOLOCALMEMORY,   /* There's not enough system memory. */
     DFB_NOSHAREDMEMORY  = DR_NOSHAREDMEMORY,  /* There's not enough shared memory. */
     DFB_LOCKED          = DR_LOCKED,          /* The resource is (already) locked. */
     DFB_BUFFEREMPTY     = DR_BUFFEREMPTY,     /* The buffer is empty. */
     DFB_FILENOTFOUND    = DR_FILENOTFOUND,    /* The specified file has not been found. */
     DFB_IO              = DR_IO,              /* A general I/O error occurred. */
     DFB_BUSY            = DR_BUSY,            /* The resource or device is busy. */
     DFB_NOIMPL          = DR_NOIMPL,          /* No implementation for this interface or content type has been found. */
     DFB_TIMEOUT         = DR_TIMEOUT,         /* The operation timed out. */
     DFB_THIZNULL        = DR_THIZNULL,        /* 'thiz' pointer is NULL. */
     DFB_IDNOTFOUND      = DR_IDNOTFOUND,      /* No resource has been found by the specified id. */
     DFB_DESTROYED       = DR_DESTROYED,       /* The requested object has been destroyed. */
     DFB_FUSION          = DR_FUSION,          /* Internal fusion error detected, most likely related to IPC resources. */
     DFB_BUFFERTOOLARGE  = DR_BUFFERTOOLARGE,  /* Buffer is too large. */
     DFB_INTERRUPTED     = DR_INTERRUPTED,     /* The operation has been interrupted. */
     DFB_NOCONTEXT       = DR_NOCONTEXT,       /* No context available. */
     DFB_TEMPUNAVAIL     = DR_TEMPUNAVAIL,     /* Temporarily unavailable. */
     DFB_LIMITEXCEEDED   = DR_LIMITEXCEEDED,   /* Attempted to exceed limit, i.e. any kind of maximum size, count etc. */
     DFB_NOSUCHMETHOD    = DR_NOSUCHMETHOD,    /* Requested method is not known. */
     DFB_NOSUCHINSTANCE  = DR_NOSUCHINSTANCE,  /* Requested instance is not known. */
     DFB_ITEMNOTFOUND    = DR_ITEMNOTFOUND,    /* No such item found. */
     DFB_VERSIONMISMATCH = DR_VERSIONMISMATCH, /* Some versions didn't match. */
     DFB_EOF             = DR_EOF,             /* Reached end of file. */
     DFB_SUSPENDED       = DR_SUSPENDED,       /* The requested object is suspended. */
     DFB_INCOMPLETE      = DR_INCOMPLETE,      /* The operation has been executed, but not completely. */
     DFB_NOCORE          = DR_NOCORE,          /* Core part not available. */

     /*
      * DirectFB specific result codes starting at this offset.
      */
     DFB__RESULT_BASE    = D_RESULT_TYPE_CODE_BASE( 'D','F','B','1' ),

     DFB_NOVIDEOMEMORY,                        /* There's not enough video memory. */
     DFB_MISSINGFONT,                          /* No font has been set. */
     DFB_MISSINGIMAGE,                         /* No image has been set. */
     DFB_NOALLOCATION,                         /* No allocation. */
     DFB_NOBUFFER,                             /* No buffer. */

     DFB__RESULT_END
} DFBResult;

/*
 * Return value of callback function of enumerations.
 */
typedef enum {
     DFENUM_OK           = 0,                  /* Proceed with enumeration. */
     DFENUM_CANCEL       = 1                   /* Cancel enumeration. */
} DFBEnumerationResult;

#endif

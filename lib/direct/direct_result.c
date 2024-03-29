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

#include <direct/direct_result.h>
#include <direct/result.h>
#include <direct/util.h>

/**********************************************************************************************************************/

static const char       *DirectResult__strings[DR__RESULT_END - DR__RESULT_BASE];

static DirectResultType  DirectResult__type = {
     0, 0, DR__RESULT_BASE, DirectResult__strings, D_ARRAY_SIZE(DirectResult__strings)
};

/**********************************************************************************************************************/

void
__D_direct_result_init()
{
     DirectResult__strings[0] = "DirectResult";

     DirectResult__strings[D_RESULT_INDEX(DR_FAILURE)] = "A general or unknown error occurred";
     DirectResult__strings[D_RESULT_INDEX(DR_INIT)] = "A general initialization error occurred";
     DirectResult__strings[D_RESULT_INDEX(DR_BUG)] = "Internal bug or inconsistency has been detected";
     DirectResult__strings[D_RESULT_INDEX(DR_DEAD)] = "Interface has a zero reference counter (available in debug mode)";
     DirectResult__strings[D_RESULT_INDEX(DR_UNSUPPORTED)] = "The requested operation or an argument is (currently) not supported";
     DirectResult__strings[D_RESULT_INDEX(DR_UNIMPLEMENTED)] = "The requested operation is not implemented, yet";
     DirectResult__strings[D_RESULT_INDEX(DR_ACCESSDENIED)] = "Access to the resource is denied";
     DirectResult__strings[D_RESULT_INDEX(DR_INVAREA)] = "An invalid area has been specified or detected";
     DirectResult__strings[D_RESULT_INDEX(DR_INVARG)] = "An invalid argument has been specified";
     DirectResult__strings[D_RESULT_INDEX(DR_NOLOCALMEMORY)] = "There's not enough local system memory";
     DirectResult__strings[D_RESULT_INDEX(DR_NOSHAREDMEMORY)] = "There's not enough shared system memory";
     DirectResult__strings[D_RESULT_INDEX(DR_LOCKED)] = "The resource is (already) locked";
     DirectResult__strings[D_RESULT_INDEX(DR_BUFFEREMPTY)] = "The buffer is empty";
     DirectResult__strings[D_RESULT_INDEX(DR_FILENOTFOUND)] = "The specified file has not been found";
     DirectResult__strings[D_RESULT_INDEX(DR_IO)] = "A general I/O error occurred";
     DirectResult__strings[D_RESULT_INDEX(DR_BUSY)] = "The resource or device is busy";
     DirectResult__strings[D_RESULT_INDEX(DR_NOIMPL)] = "No implementation for this interface or content type has been found";
     DirectResult__strings[D_RESULT_INDEX(DR_TIMEOUT)] = "The operation timed out";
     DirectResult__strings[D_RESULT_INDEX(DR_THIZNULL)] = "'thiz' pointer is NULL";
     DirectResult__strings[D_RESULT_INDEX(DR_IDNOTFOUND)] = "No resource has been found by the specified id";
     DirectResult__strings[D_RESULT_INDEX(DR_DESTROYED)] = "The requested object has been destroyed";
     DirectResult__strings[D_RESULT_INDEX(DR_FUSION)] = "Internal fusion error detected, most likely related to IPC resources";
     DirectResult__strings[D_RESULT_INDEX(DR_BUFFERTOOLARGE)] = "Buffer is too large";
     DirectResult__strings[D_RESULT_INDEX(DR_INTERRUPTED)] = "The operation has been interrupted";
     DirectResult__strings[D_RESULT_INDEX(DR_NOCONTEXT)] = "No context available";
     DirectResult__strings[D_RESULT_INDEX(DR_TEMPUNAVAIL)] = "Temporarily unavailable";
     DirectResult__strings[D_RESULT_INDEX(DR_LIMITEXCEEDED)] = "Attempted to exceed limit, i.e. any kind of maximum size, count etc";
     DirectResult__strings[D_RESULT_INDEX(DR_NOSUCHMETHOD)] = "Requested method is not known";
     DirectResult__strings[D_RESULT_INDEX(DR_NOSUCHINSTANCE)] = "Requested instance is not known";
     DirectResult__strings[D_RESULT_INDEX(DR_ITEMNOTFOUND)] = "No such item found";
     DirectResult__strings[D_RESULT_INDEX(DR_VERSIONMISMATCH)] = "Some versions didn't match";
     DirectResult__strings[D_RESULT_INDEX(DR_EOF)] = "Reached end of file";
     DirectResult__strings[D_RESULT_INDEX(DR_SUSPENDED)] = "The requested object is suspended";
     DirectResult__strings[D_RESULT_INDEX(DR_INCOMPLETE)] = "The operation has been executed, but not completely";
     DirectResult__strings[D_RESULT_INDEX(DR_NOCORE)] = "Core part not available";
     DirectResult__strings[D_RESULT_INDEX(DR_SIGNALLED)] = "Received a signal, e.g. while waiting";
     DirectResult__strings[D_RESULT_INDEX(DR_TASK_NOT_FOUND)] = "The corresponding task has not been found";

     DirectResultTypeRegister( &DirectResult__type );
}

void
__D_direct_result_deinit()
{
     DirectResultTypeUnregister( &DirectResult__type );
}

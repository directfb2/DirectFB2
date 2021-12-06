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

#ifndef __IDIRECTFB_H__
#define __IDIRECTFB_H__

#include <core/coredefs.h>
#include <core/coretypes.h>
#include <fusion/reactor.h>

/**********************************************************************************************************************/

typedef struct {
     int                         ref;     /* reference counter */
     CoreDFB                    *core;

     DFBCooperativeLevel         level;   /* current cooperative level */

     CoreLayer                  *layer;   /* primary display layer */
     CoreLayerContext           *context; /* shared context of primary layer */
     CoreWindowStack            *stack;   /* window stack of primary layer */

     struct {
          int                    width;
          int                    height;
          DFBSurfacePixelFormat  format;
          DFBSurfaceColorSpace   colorspace;

          CoreWindow            *window;
          Reaction               reaction;
          bool                   focused;

          CoreLayerContext      *context;
          DFBWindowOptions       window_options;
     } primary;

     bool                        app_focus;

     struct {
          CoreLayer             *layer;
          CoreLayerContext      *context;
          CoreLayerRegion       *region;
          CoreSurface           *surface;
          CorePalette           *palette;
     } layers[MAX_LAYERS];

     bool                        init_done;
     DirectMutex                 init_lock;
     DirectWaitQueue             init_wq;
} IDirectFB_data;

/**********************************************************************************************************************/

extern IDirectFB *idirectfb_singleton;

void              eventbuffer_containers_remove( IDirectFBEventBuffer *thiz );

/*
 * initializes interface struct and private data
 */
DFBResult         IDirectFB_Construct          ( IDirectFB            *thiz );

DFBResult         IDirectFB_WaitInitialised    ( IDirectFB            *thiz );

DFBResult         IDirectFB_SetAppFocus        ( IDirectFB            *thiz,
                                                 DFBBoolean            focused );

#endif

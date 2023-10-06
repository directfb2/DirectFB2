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

#ifndef __DIRECTFBGL_H__
#define __DIRECTFBGL_H__

#include <directfb.h>

#ifdef __cplusplus
extern "C" {
#endif

/***************
 * IDirectFBGL *
 ***************/

/*
 * The DirectFBGL interface version.
 */
#ifndef DIRECTFBGL_INTERFACE_VERSION
#define DIRECTFBGL_INTERFACE_VERSION 2
#endif

/*
 * Attributes of an OpenGL context.
 */
typedef struct {
     int                                     buffer_size;        /* depth of the color buffer */
     int                                     depth_size;         /* number of bits in the depth buffer */
     int                                     stencil_size;       /* number of bits in the stencil buffer */
     int                                     aux_buffers;        /* number of auxiliary color buffers */
     int                                     red_size;           /* number of bits of red in the framebuffer */
     int                                     green_size;         /* number of bits of green in the framebuffer */
     int                                     blue_size;          /* number of bits of blue in the framebuffer */
     int                                     alpha_size;         /* number of bits of alpha in the framebuffer */
     int                                     accum_red_size;     /* number of bits of red in the accumulation
                                                                    buffer */
     int                                     accum_green_size;   /* number of bits of green in the accumulation
                                                                    buffer */
     int                                     accum_blue_size;    /* number of bits of blue in the accumulation
                                                                    buffer */
     int                                     accum_alpha_size;   /* number of bits of alpha in the accumulation
                                                                    buffer */
     DFBBoolean                              double_buffer;      /* true if color buffers have front/back buffer
                                                                    pairs */
     DFBBoolean                              stereo;             /* true if color buffers have left/right pairs */
} DFBGLAttributes;

/*
 * IDirectFBGL is the OpenGL interface.
 */
D_DEFINE_INTERFACE( IDirectFBGL,

   /** Context handling **/

     /*
      * Acquire the hardware lock.
      */
     DFBResult (*Lock) (
          IDirectFBGL                       *thiz
     );

     /*
      * Release the lock.
      */
     DFBResult (*Unlock) (
          IDirectFBGL                       *thiz
     );

     /*
      * Query the OpenGL attributes.
      */
     DFBResult (*GetAttributes) (
          IDirectFBGL                       *thiz,
          DFBGLAttributes                   *ret_attributes
     );

     /*
      * Get the address of an OpenGL function.
      */
     DFBResult (*GetProcAddress) (
          IDirectFBGL                       *thiz,
          const char                        *name,
          void                             **ret_address
     );

#if DIRECTFBGL_INTERFACE_VERSION > 1
     /*
      * Swap buffers.
      */
     DFBResult (*SwapBuffers) (
          IDirectFBGL                       *thiz
     );
#endif
)

#ifdef __cplusplus
}
#endif

#endif

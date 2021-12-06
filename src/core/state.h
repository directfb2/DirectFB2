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

#ifndef __CORE__STATE_H__
#define __CORE__STATE_H__

#include <core/gfxcard.h>
#include <core/surface_buffer.h>
#include <directfb_util.h>
#include <misc/conf.h>

/**********************************************************************************************************************/

/*
 * Maximum number of color ids.
 */
#define DFB_COLOR_IDS_MAX 8

typedef enum {
     CSF_NONE               = 0x00000000, /* none of these */

     CSF_DESTINATION        = 0x00000001, /* destination is set using dfb_state_set_destination() */
     CSF_SOURCE             = 0x00000002, /* source is set using dfb_state_set_source() */
     CSF_SOURCE_MASK        = 0x00000008, /* source mask is set using dfb_state_set_source_mask() */

     CSF_SOURCE_LOCKED      = 0x00000010, /* source surface is locked */
     CSF_SOURCE_MASK_LOCKED = 0x00000020, /* source mask surface is locked */

     CSF_SOURCE2            = 0x00000100, /* source2 is set using dfb_state_set_source2() */
     CSF_SOURCE2_LOCKED     = 0x00000200, /* source2 surface is locked */

     CSF_DRAWING            = 0x00010000, /* something has been rendered with this state, this is cleared by flushing
                                             the state, e.g. upon flip */

     CSF_ALL                = 0x0001033B  /* all of these */
} CardStateFlags;

typedef enum {
     SMF_NONE                  = 0x00000000,

     SMF_DRAWING_FLAGS         = 0x00000001,
     SMF_BLITTING_FLAGS        = 0x00000002,
     SMF_CLIP                  = 0x00000004,
     SMF_COLOR                 = 0x00000008,
     SMF_SRC_BLEND             = 0x00000010,
     SMF_DST_BLEND             = 0x00000020,
     SMF_SRC_COLORKEY          = 0x00000040,
     SMF_DST_COLORKEY          = 0x00000080,
     SMF_DESTINATION           = 0x00000100,
     SMF_SOURCE                = 0x00000200,
     SMF_SOURCE_MASK           = 0x00000400,
     SMF_SOURCE_MASK_VALS      = 0x00000800,
     SMF_INDEX_TRANSLATION     = 0x00001000,
     SMF_COLORKEY              = 0x00002000,

     SMF_SRC_COLORMATRIX       = 0x00008000,
     SMF_RENDER_OPTIONS        = 0x00010000,
     SMF_MATRIX                = 0x00020000,
     SMF_SRC_COLORKEY_EXTENDED = 0x00040000,
     SMF_DST_COLORKEY_EXTENDED = 0x00080000,

     SMF_SOURCE2               = 0x00100000,
     SMF_SRC_CONVOLUTION       = 0x00200000,

     SMF_FROM                  = 0x10000000,
     SMF_TO                    = 0x20000000,

     SMF_ALL                   = 0x303FBFFF
} StateModificationFlags;

struct __DFB_CardState {
     /* graphics card state */

     int                      magic;

     CoreDFB                 *core;                             /* core instance */
     FusionID                 fusion_id;                        /* fusion id */

     DirectMutex              lock;                             /* lock for state handling */

     CardStateFlags           flags;                            /* flags defining the state */

     StateModificationFlags   modified;                         /* indicate which fields have been modified, these flags
                                                                   will be cleared by the gfx drivers */
     StateModificationFlags   mod_hw;                           /* modification flags for drivers. */

     /* values forming the state for graphics operations */

     DFBSurfaceDrawingFlags   drawingflags;                     /* drawing flags */
     DFBSurfaceBlittingFlags  blittingflags;                    /* blitting flags */

     DFBRegion                clip;                             /* clipping rectangle */
     DFBColor                 color;                            /* color for drawing or modulation */
     unsigned int             color_index;                      /* index to color in palette */
     DFBSurfaceBlendFunction  src_blend;                        /* blend function for source */
     DFBSurfaceBlendFunction  dst_blend;                        /* blend function for destination */
     u32                      src_colorkey;                     /* colorkey for source */
     u32                      dst_colorkey;                     /* colorkey for destination */

     CoreSurface             *destination;                      /* destination surface */
     CoreSurface             *source;                           /* source surface */
     CoreSurfaceBuffer       *source_buffer;                    /* source surface */

     DirectSerial             dst_serial;                       /* last destination surface serial */
     DirectSerial             src_serial;                       /* last source surface serial */

     int                     *index_translation;                /* translation table used for fast indexed to indexed
                                                                   pixel format conversion */
     int                      num_translation;                  /* translation table size */

     /* hardware abstraction and state handling helpers */

     DFBAccelerationMask      accel;                            /* remember checked commands if they are accelerated */
     DFBAccelerationMask      checked;                          /* commands for which a state has been checked */
     DFBAccelerationMask      set;                              /* commands for which a state is valid */
     DFBAccelerationMask      disabled;                         /* commands which are disabled temporarily */
     CoreGraphicsSerial       serial;                           /* hardware serial of the last operation */

     /* from/to buffers */

     DFBSurfaceBufferRole     from;                             /* usually DSBR_FRONT */
     DFBSurfaceStereoEye      from_eye;                         /* usually DSSE_LEFT */
     DFBSurfaceBufferRole     to;                               /* usually DSBR_BACK */
     DFBSurfaceStereoEye      to_eye;                           /* usually DSSE_LEFT */

     /* read/write locks during operation */

     CoreSurfaceBufferLock    dst;                              /* destination read/write lock */
     CoreSurfaceBufferLock    src;                              /* source read/write lock */

     /* software driver */

     GenefxState             *gfxs;                             /* state of the virtual graphics processing unit */

     /* extended state */

     DFBSurfaceRenderOptions  render_options;                   /* options for drawing and blitting operations */

     DFBColorKey              colorkey;                         /* key for color key protection */

     s32                      matrix[9];                        /* transformation matrix for DSRO_MATRIX */
     DFBBoolean               affine_matrix;                    /* true if affine transformation */

     CoreSurface             *source_mask;                      /* source mask surface */
     CoreSurfaceBufferLock    src_mask;                         /* source mask surface lock */
     DirectSerial             src_mask_serial;                  /* last source mask surface serial */
     DFBPoint                 src_mask_offset;                  /* relative or absolute coordinates */
     DFBSurfaceMaskFlags      src_mask_flags;                   /* controls coordinate mode and more */

     CoreSurface             *source2;                          /* source2 surface */
     DirectSerial             src2_serial;                      /* last source2 surface serial */
     CoreSurfaceBufferLock    src2;                             /* source2 surface lock */

     DFBColor                 colors[DFB_COLOR_IDS_MAX];        /* colors for drawing or modulation */
     unsigned int             color_indices[DFB_COLOR_IDS_MAX]; /* indices to colors in palette */

     DFBColorKeyExtended      src_colorkey_extended;            /* extended colorkey for source */
     DFBColorKeyExtended      dst_colorkey_extended;            /* extended colorkey for destination */

     s32                      src_colormatrix[12];              /* transformation matrix for DSBLIT_SRC_COLORMATRIX */

     DFBConvolutionFilter     src_convolution;                  /* 3x3 kernel, scale and bias */

     void                    *gfxcard_data;                     /* gfx driver specific state data */

     u32                      source_flip_count;                /* source flip count */
     bool                     source_flip_count_used;           /* source flip count used */

     void                    *client;                           /* state client */

     u32                      destination_flip_count;           /* destination flip count */
     bool                     destination_flip_count_used;      /* destination flip count used */
};

/**********************************************************************************************************************/

int       dfb_state_init                 ( CardState                  *state,
                                           CoreDFB                    *core );

void      dfb_state_destroy              ( CardState                  *state );

DFBResult dfb_state_set_destination      ( CardState                  *state,
                                           CoreSurface                *destination );

DFBResult dfb_state_set_destination_2    ( CardState                  *state,
                                           CoreSurface                *destination,
                                           u32                         flip_count );

DFBResult dfb_state_set_source           ( CardState                  *state,
                                           CoreSurface                *source );

DFBResult dfb_state_set_source_2         ( CardState                  *state,
                                           CoreSurface                *source,
                                           u32                         flip_count );

DFBResult dfb_state_set_source2          ( CardState                  *state,
                                           CoreSurface                *source2 );

DFBResult dfb_state_set_source_mask      ( CardState                  *state,
                                           CoreSurface                *source_mask );

void      dfb_state_update               ( CardState                  *state,
                                           bool                        update_source );

void      dfb_state_update_destination   ( CardState                  *state );

void      dfb_state_update_sources       ( CardState                  *state,
                                           CardStateFlags              flags );

DFBResult dfb_state_set_index_translation( CardState                  *state,
                                           const int                  *indices,
                                           int                         num_indices );

void      dfb_state_set_matrix           ( CardState                  *state,
                                           const s32                  *matrix );

void      dfb_state_set_src_colormatrix  ( CardState                  *state,
                                           const s32                  *matrix );

void      dfb_state_set_src_convolution  ( CardState                  *state,
                                           const DFBConvolutionFilter *filter );

/*
 * Multifunctional color configuration function. Always tries to set both color and index.
 *
 * If color index is -1, color is used and searched in palette of destination surface if present.
 * If color index is valid the color is looked up in palette if present.
 */
void      dfb_state_set_color_or_index   ( CardState                  *state,
                                           const DFBColor             *color,
                                           int                         index );

/*
 * Return the mask of accelerated functions.
 */
DFBResult dfb_state_get_acceleration_mask( CardState                  *state,
                                           DFBAccelerationMask        *ret_accel );

/**********************************************************************************************************************/

static __inline__ void
dfb_state_lock( CardState *state )
{
     D_MAGIC_ASSERT( state, CardState );

     DFB_REGION_ASSERT( &state->clip );

     direct_mutex_lock( &state->lock );
}

static __inline__ void
dfb_state_start_drawing( CardState *state )
{
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( state->destination != NULL );

     if (dfb_config->startstop) {
          if (!(state->flags & CSF_DRAWING)) {
               dfb_gfxcard_start_drawing( state );

               state->flags = state->flags | CSF_DRAWING;
          }
     }
}

static __inline__ void
dfb_state_stop_drawing( CardState *state )
{
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( state->destination != NULL );

     if (dfb_config->startstop) {
          if (state->flags & CSF_DRAWING) {
               dfb_gfxcard_stop_drawing( state );

               state->flags = state->flags & ~CSF_DRAWING;
          }
     }
}

static __inline__ void
dfb_state_unlock( CardState *state )
{
     D_MAGIC_ASSERT( state, CardState );

     DFB_REGION_ASSERT( &state->clip );

     direct_mutex_unlock( &state->lock );
}

static __inline__ void
dfb_state_set_from( CardState            *state,
                    DFBSurfaceBufferRole  role,
                    DFBSurfaceStereoEye   eye )
{
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( role == DSBR_FRONT || role == DSBR_BACK || role == DSBR_IDLE );
     D_ASSERT( eye == DSSE_LEFT || eye == DSSE_RIGHT );

     if (state->from != role || state->from_eye != eye) {
          state->from     = role;
          state->from_eye = eye;
          state->modified = state->modified | SMF_SOURCE | SMF_SOURCE2 | SMF_SOURCE_MASK | SMF_FROM;
     }
}

static __inline__ void
dfb_state_set_to( CardState            *state,
                  DFBSurfaceBufferRole  role,
                  DFBSurfaceStereoEye   eye )
{
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( role == DSBR_FRONT || role == DSBR_BACK || role == DSBR_IDLE );
     D_ASSERT( eye == DSSE_LEFT || eye == DSSE_RIGHT );

     if (state->to != role || state->to_eye != eye) {
          state->to       = role;
          state->to_eye   = eye;
          state->modified = state->modified | SMF_DESTINATION | SMF_TO;
     }
}

static __inline__ void
dfb_state_set_clip( CardState       *state,
                    const DFBRegion *clip )
{
     D_MAGIC_ASSERT( state, CardState );
     DFB_REGION_ASSERT( clip );

     if (!DFB_REGION_EQUAL( state->clip, *clip )) {
          state->clip     = *clip;
          state->modified = state->modified | SMF_CLIP;
     }
}

static __inline__ void
dfb_state_set_color( CardState      *state,
                     const DFBColor *color )
{
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( color != NULL );

     if (!DFB_COLOR_EQUAL( state->color, *color )) {
          state->color    = *color;
          state->modified = state->modified | SMF_COLOR;
     }
}

static __inline__ void
dfb_state_set_colorkey( CardState         *state,
                        const DFBColorKey *key )
{
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( key != NULL );

     if (!DFB_COLORKEY_EQUAL( state->colorkey, *key )) {
          state->colorkey = *key;
          state->modified = state->modified | SMF_COLORKEY;
     }
}

static __inline__ void
dfb_state_set_source_mask_vals( CardState           *state,
                                const DFBPoint      *offset,
                                DFBSurfaceMaskFlags  flags )
{
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( offset != NULL );
     D_FLAGS_ASSERT( flags, DSMF_ALL );

     if (!DFB_POINT_EQUAL( state->src_mask_offset, *offset ) || state->src_mask_flags != flags) {
          state->src_mask_offset = *offset;
          state->src_mask_flags  = flags;

          state->modified = state->modified | SMF_SOURCE_MASK_VALS;
     }
}

static __inline__ void
dfb_state_set_src_colorkey_extended( CardState                 *state,
                                     const DFBColorKeyExtended *key )
{
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( key != NULL );

     if (state->src_colorkey_extended.polarity != key->polarity             ||
         !DFB_COLOR_EQUAL( state->src_colorkey_extended.lower, key->lower ) ||
         !DFB_COLOR_EQUAL( state->src_colorkey_extended.upper, key->upper )) {
          state->src_colorkey_extended = *key;
          state->modified              = state->modified | SMF_SRC_COLORKEY_EXTENDED;
     }
}

static __inline__ void
dfb_state_set_dst_colorkey_extended( CardState                 *state,
                                     const DFBColorKeyExtended *key )
{
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( key != NULL );

     if (state->dst_colorkey_extended.polarity != key->polarity             ||
         !DFB_COLOR_EQUAL( state->dst_colorkey_extended.lower, key->lower ) ||
         !DFB_COLOR_EQUAL( state->dst_colorkey_extended.upper, key->upper )) {
          state->dst_colorkey_extended = *key;
          state->modified              = state->modified | SMF_DST_COLORKEY_EXTENDED;
     }
}

#define _dfb_state_set_checked(member,flag,state,value)       \
do {                                                          \
     D_MAGIC_ASSERT( state, CardState );                      \
                                                              \
     if ((value) != (state)->member) {                        \
          (state)->member   = (value);                        \
          (state)->modified = (state)->modified | SMF_##flag; \
     }                                                        \
} while (0)

#define dfb_state_set_drawing_flags(state,flags)  _dfb_state_set_checked( drawingflags,   DRAWING_FLAGS,  state, flags )

#define dfb_state_set_blitting_flags(state,flags) _dfb_state_set_checked( blittingflags,  BLITTING_FLAGS, state, flags )

#define dfb_state_set_color_index(state,index)    _dfb_state_set_checked( color_index,    COLOR,          state, index )

#define dfb_state_set_src_blend(state,blend)      _dfb_state_set_checked( src_blend,      SRC_BLEND,      state, blend )

#define dfb_state_set_dst_blend(state,blend)      _dfb_state_set_checked( dst_blend,      DST_BLEND,      state, blend )

#define dfb_state_set_src_colorkey(state,key)     _dfb_state_set_checked( src_colorkey,   SRC_COLORKEY,   state, key )

#define dfb_state_set_dst_colorkey(state,key)     _dfb_state_set_checked( dst_colorkey,   DST_COLORKEY,   state, key )

#define dfb_state_set_render_options(state,opts)  _dfb_state_set_checked( render_options, RENDER_OPTIONS, state, opts )

#endif

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

#include <core/core.h>
#include <core/palette.h>
#include <core/state.h>
#include <direct/memcpy.h>

D_DEBUG_DOMAIN( Core_GfxState, "Core/GfxState", "DirectFB Core Gfx State" );

/**********************************************************************************************************************/

static __inline__ void
validate_clip( CardState *state,
               int        xmax,
               int        ymax,
               bool       warn )
{
     D_DEBUG_AT( Core_GfxState, "%s( %p, %d, %d, %d )\n", __FUNCTION__, state, xmax, ymax, warn );

     D_MAGIC_ASSERT( state, CardState );
     DFB_REGION_ASSERT( &state->clip );
     D_ASSERT( xmax >= 0 );
     D_ASSERT( ymax >= 0 );
     D_ASSERT( state->clip.x1 <= state->clip.x2 );
     D_ASSERT( state->clip.y1 <= state->clip.y2 );

     if (state->clip.x1 <= xmax &&
         state->clip.y1 <= ymax &&
         state->clip.x2 <= xmax &&
         state->clip.y2 <= ymax)
          return;

     if (warn)
          D_WARN( "clip %4d,%4d-%4dx%4d invalid, adjusting to fit %dx%d",
                  DFB_RECTANGLE_VALS_FROM_REGION( &state->clip ), xmax + 1, ymax + 1 );

     if (state->clip.x1 > xmax)
          state->clip.x1 = xmax;

     if (state->clip.y1 > ymax)
          state->clip.y1 = ymax;

     if (state->clip.x2 > xmax)
          state->clip.x2 = xmax;

     if (state->clip.y2 > ymax)
          state->clip.y2 = ymax;

     state->modified |= SMF_CLIP;
}

/**********************************************************************************************************************/

int
dfb_state_init( CardState *state,
                CoreDFB   *core )
{
     D_ASSERT( state != NULL );

     memset( state, 0, sizeof(CardState) );

     state->core           = core;
     state->fusion_id      = fusion_id( dfb_core_world( core ) );
     state->modified       = SMF_ALL;
     state->src_blend      = DSBF_SRCALPHA;
     state->dst_blend      = DSBF_INVSRCALPHA;
     state->render_options = dfb_config->render_options;

     state->matrix[0] = 0x10000;
     state->matrix[1] = 0x00000;
     state->matrix[2] = 0x00000;
     state->matrix[3] = 0x00000;
     state->matrix[4] = 0x10000;
     state->matrix[5] = 0x00000;
     state->matrix[6] = 0x00000;
     state->matrix[7] = 0x00000;
     state->matrix[8] = 0x10000;
     state->affine_matrix = DFB_TRUE;

     state->from     = DSBR_FRONT;
     state->from_eye = DSSE_LEFT;
     state->to       = DSBR_BACK;
     state->to_eye   = DSSE_LEFT;

     state->src_colormatrix[0]  = 0x10000;
     state->src_colormatrix[1]  = 0x00000;
     state->src_colormatrix[2]  = 0x00000;
     state->src_colormatrix[3]  = 0x00000;

     state->src_colormatrix[4]  = 0x00000;
     state->src_colormatrix[5]  = 0x10000;
     state->src_colormatrix[6]  = 0x00000;
     state->src_colormatrix[7]  = 0x00000;

     state->src_colormatrix[8]  = 0x00000;
     state->src_colormatrix[9]  = 0x00000;
     state->src_colormatrix[10] = 0x10000;
     state->src_colormatrix[11] = 0x00000;

     state->src_convolution.kernel[4] = 0x10000;
     state->src_convolution.scale     = 0x10000;

     direct_recursive_mutex_init( &state->lock );

     direct_serial_init( &state->dst_serial );
     direct_serial_init( &state->src_serial );
     direct_serial_init( &state->src_mask_serial );
     direct_serial_init( &state->src2_serial );

     D_MAGIC_SET( state, CardState );

     state->gfxcard_data = NULL;
     dfb_gfxcard_state_init( state );

     return 0;
}

void
dfb_state_destroy( CardState *state )
{
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( state->destination == NULL );
     D_ASSERT( state->source == NULL );
     D_ASSERT( state->source2 == NULL );
     D_ASSERT( state->source_mask == NULL );

     if (!dfb_config->startstop)
          D_ASSUME( !(state->flags & CSF_DRAWING) );

     dfb_gfxcard_state_destroy( state );

     state->gfxcard_data = NULL;

     D_MAGIC_CLEAR( state );

     direct_serial_deinit( &state->dst_serial );
     direct_serial_deinit( &state->src_serial );
     direct_serial_deinit( &state->src_mask_serial );
     direct_serial_deinit( &state->src2_serial );

     if (state->num_translation) {
          D_ASSERT( state->index_translation != NULL );

          D_FREE( state->index_translation );
     }
     else
          D_ASSERT( state->index_translation == NULL );

     direct_mutex_deinit( &state->lock );
}

DFBResult
dfb_state_set_destination( CardState   *state,
                           CoreSurface *destination )
{
     D_MAGIC_ASSERT( state, CardState );

     dfb_state_lock( state );

     if (!dfb_config->startstop)
          D_ASSUME( !(state->flags & CSF_DRAWING) );

     if (state->destination != destination) {
          if (destination) {
               if (dfb_surface_ref( destination )) {
                    D_WARN( "could not ref() destination" );
                    dfb_state_unlock( state );
                    return DFB_DEAD;
               }

               validate_clip( state, destination->config.size.w - 1, destination->config.size.h - 1, false );
          }

          if (state->destination) {
               D_ASSERT( D_FLAGS_IS_SET( state->flags, CSF_DESTINATION ) );

               dfb_surface_unref( state->destination );
          }

          state->destination  = destination;
          state->modified    |= SMF_DESTINATION;

          if (destination) {
               direct_serial_copy( &state->dst_serial, &destination->serial );

               D_FLAGS_SET( state->flags, CSF_DESTINATION );
          }
          else
               D_FLAGS_CLEAR( state->flags, CSF_DESTINATION );
     }

     dfb_state_unlock( state );

     return DFB_OK;
}

DFBResult
dfb_state_set_destination_2( CardState   *state,
                             CoreSurface *destination,
                             u32          flip_count )
{
     D_MAGIC_ASSERT( state, CardState );

     dfb_state_lock( state );

     if (!dfb_config->startstop)
          D_ASSUME( !(state->flags & CSF_DRAWING) );

     if (state->destination != destination) {
          if (destination) {
               if (dfb_surface_ref( destination )) {
                    D_WARN( "could not ref() destination" );
                    dfb_state_unlock( state );
                    return DFB_DEAD;
               }

               validate_clip( state, destination->config.size.w - 1, destination->config.size.h - 1, false );
          }

          if (state->destination) {
               D_ASSERT( D_FLAGS_IS_SET( state->flags, CSF_DESTINATION ) );

               dfb_surface_unref( state->destination );
          }

          if (destination) {
               direct_serial_copy( &state->dst_serial, &destination->serial );

               D_FLAGS_SET( state->flags, CSF_DESTINATION );
          }
          else
               D_FLAGS_CLEAR( state->flags, CSF_DESTINATION );

          state->destination  = destination;
          state->modified    |= SMF_DESTINATION;
     }

     if (state->destination_flip_count != flip_count || !state->destination_flip_count_used) {
          state->destination_flip_count      = flip_count;
          state->destination_flip_count_used = true;

          state->destination  = destination;
          state->modified    |= SMF_DESTINATION;
     }

     dfb_state_unlock( state );

     return DFB_OK;
}

DFBResult
dfb_state_set_source( CardState   *state,
                      CoreSurface *source )
{
     D_MAGIC_ASSERT( state, CardState );

     dfb_state_lock( state );

     if (state->source != source) {
          if (source && dfb_surface_ref( source )) {
               D_WARN( "could not ref() source" );
               dfb_state_unlock( state );
               return DFB_DEAD;
          }

          if (state->source) {
               D_ASSERT( D_FLAGS_IS_SET( state->flags, CSF_SOURCE ) );

               dfb_surface_unref( state->source );
          }

          state->source    = source;
          state->modified |= SMF_SOURCE;

          if (source) {
               direct_serial_copy( &state->src_serial, &source->serial );

               D_FLAGS_SET( state->flags, CSF_SOURCE );
          }
          else
               D_FLAGS_CLEAR( state->flags, CSF_SOURCE );
     }

     dfb_state_unlock( state );

     return DFB_OK;
}

DFBResult
dfb_state_set_source_2( CardState   *state,
                        CoreSurface *source,
                        u32          flip_count )
{
     D_MAGIC_ASSERT( state, CardState );

     dfb_state_lock( state );

     if (state->source != source) {
          if (source && dfb_surface_ref( source )) {
               D_WARN( "could not ref() source" );
               dfb_state_unlock( state );
               return DFB_DEAD;
          }

          if (state->source) {
               D_ASSERT( D_FLAGS_IS_SET( state->flags, CSF_SOURCE ) );

               dfb_surface_unref( state->source );
          }

          state->source    = source;
          state->modified |= SMF_SOURCE;

          if (source) {
               direct_serial_copy( &state->src_serial, &source->serial );

               D_FLAGS_SET( state->flags, CSF_SOURCE );
          }
          else
               D_FLAGS_CLEAR( state->flags, CSF_SOURCE );
     }

     if (state->source_flip_count != flip_count || !state->source_flip_count_used) {
          state->source_flip_count      = flip_count;
          state->source_flip_count_used = true;

          state->source    = source;
          state->modified |= SMF_SOURCE;
     }

     dfb_state_unlock( state );

     return DFB_OK;
}

DFBResult
dfb_state_set_source2( CardState   *state,
                       CoreSurface *source2 )
{
     D_MAGIC_ASSERT( state, CardState );

     dfb_state_lock( state );

     if (state->source2 != source2) {
          if (source2 && dfb_surface_ref( source2 )) {
               D_WARN( "could not ref() source2" );
               dfb_state_unlock( state );
               return DFB_DEAD;
          }

          if (state->source2) {
               D_ASSERT( D_FLAGS_IS_SET( state->flags, CSF_SOURCE2 ) );

               dfb_surface_unref( state->source2 );
          }

          state->source2   = source2;
          state->modified |= SMF_SOURCE2;

          if (source2) {
               direct_serial_copy( &state->src2_serial, &source2->serial );

               D_FLAGS_SET( state->flags, CSF_SOURCE2 );
          }
          else
               D_FLAGS_CLEAR( state->flags, CSF_SOURCE2 );
     }

     dfb_state_unlock( state );

     return DFB_OK;
}

DFBResult
dfb_state_set_source_mask( CardState   *state,
                           CoreSurface *source_mask )
{
     D_MAGIC_ASSERT( state, CardState );

     dfb_state_lock( state );

     if (state->source_mask != source_mask) {
          if (source_mask && dfb_surface_ref( source_mask )) {
               D_WARN( "could not ref() source mask" );
               dfb_state_unlock( state );
               return DFB_DEAD;
          }

          if (state->source_mask) {
               D_ASSERT( D_FLAGS_IS_SET( state->flags, CSF_SOURCE_MASK ) );

               dfb_surface_unref( state->source_mask );
          }

          state->source_mask  = source_mask;
          state->modified    |= SMF_SOURCE_MASK;

          if (source_mask) {
               direct_serial_copy( &state->src_mask_serial, &source_mask->serial );

               D_FLAGS_SET( state->flags, CSF_SOURCE_MASK );
          }
          else
               D_FLAGS_CLEAR( state->flags, CSF_SOURCE_MASK );
     }

     dfb_state_unlock( state );

     return DFB_OK;
}

void
dfb_state_update( CardState *state,
                  bool       update_sources )
{
     CoreSurface *destination;

     D_MAGIC_ASSERT( state, CardState );
     DFB_REGION_ASSERT( &state->clip );

     destination = state->destination;

     if (D_FLAGS_IS_SET( state->flags, CSF_DESTINATION )) {
          D_ASSERT( destination != NULL );

          if (direct_serial_update( &state->dst_serial, &destination->serial )) {
               validate_clip( state, destination->config.size.w - 1, destination->config.size.h - 1, true );

               state->modified |= SMF_DESTINATION;
          }
     }
     else if (destination)
          validate_clip( state, destination->config.size.w - 1, destination->config.size.h - 1, true );

     if (update_sources && D_FLAGS_IS_SET( state->flags, CSF_SOURCE )) {
          CoreSurface *source = state->source;

          D_ASSERT( source != NULL );

          if (direct_serial_update( &state->src_serial, &source->serial ))
               state->modified |= SMF_SOURCE;
     }

     if (update_sources && D_FLAGS_IS_SET( state->flags, CSF_SOURCE_MASK )) {
          CoreSurface *source_mask = state->source_mask;

          D_ASSERT( source_mask != NULL );

          if (direct_serial_update( &state->src_mask_serial, &source_mask->serial ))
               state->modified |= SMF_SOURCE_MASK;
     }

     if (update_sources && D_FLAGS_IS_SET( state->flags, CSF_SOURCE2 )) {
          CoreSurface *source2 = state->source2;

          D_ASSERT( source2 != NULL );

          if (direct_serial_update( &state->src2_serial, &source2->serial ))
               state->modified |= SMF_SOURCE2;
     }
}

void
dfb_state_update_destination( CardState *state )
{
     CoreSurface *destination;

     D_DEBUG_AT( Core_GfxState, "%s( %p )\n", __FUNCTION__, state );

     D_MAGIC_ASSERT( state, CardState );
     DFB_REGION_ASSERT( &state->clip );

     destination = state->destination;

     if (D_FLAGS_IS_SET( state->flags, CSF_DESTINATION )) {
          D_DEBUG_AT( Core_GfxState, "  -> CSF_DESTINATION is set\n" );

          D_ASSERT( destination != NULL );

          if (direct_serial_update( &state->dst_serial, &destination->serial )) {
               D_DEBUG_AT( Core_GfxState, "  -> serial is updated\n" );

               validate_clip( state, destination->config.size.w - 1, destination->config.size.h - 1, true );

               state->modified |= SMF_DESTINATION;
          }
     }
     else if (destination)
          validate_clip( state, destination->config.size.w - 1, destination->config.size.h - 1, true );
}

void
dfb_state_update_sources( CardState      *state,
                          CardStateFlags  flags )
{
     D_DEBUG_AT( Core_GfxState, "%s( %p )\n", __FUNCTION__, state );

     D_MAGIC_ASSERT( state, CardState );
     DFB_REGION_ASSERT( &state->clip );

     if (D_FLAGS_IS_SET( state->flags & flags, CSF_SOURCE )) {
          CoreSurface *source = state->source;

          D_ASSERT( source != NULL );

          if (direct_serial_update( &state->src_serial, &source->serial ))
               state->modified |= SMF_SOURCE;
     }

     if (D_FLAGS_IS_SET( state->flags & flags, CSF_SOURCE_MASK )) {
          CoreSurface *source_mask = state->source_mask;

          D_ASSERT( source_mask != NULL );

          if (direct_serial_update( &state->src_mask_serial, &source_mask->serial ))
               state->modified |= SMF_SOURCE_MASK;
     }

     if (D_FLAGS_IS_SET( state->flags & flags, CSF_SOURCE2 )) {
          CoreSurface *source2 = state->source2;

          D_ASSERT( source2 != NULL );

          if (direct_serial_update( &state->src2_serial, &source2->serial ))
               state->modified |= SMF_SOURCE2;
     }
}

DFBResult
dfb_state_set_index_translation( CardState *state,
                                 const int *indices,
                                 int        num_indices )
{
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( indices != NULL || num_indices == 0 );

     dfb_state_lock( state );

     if (state->num_translation != num_indices) {
          int *new_trans = D_REALLOC( state->index_translation, num_indices * sizeof(int) );

          D_ASSERT( num_indices || new_trans == NULL );

          if (num_indices && !new_trans) {
               dfb_state_unlock( state );
               return D_OOM();
          }

          state->index_translation = new_trans;
          state->num_translation   = num_indices;
     }

     if (num_indices)
          direct_memcpy( state->index_translation, indices, num_indices * sizeof(int) );

     state->modified |= SMF_INDEX_TRANSLATION;

     dfb_state_unlock( state );

     return DFB_OK;
}

void
dfb_state_set_matrix( CardState *state,
                      const s32 *matrix )
{
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( matrix != NULL );

     if (memcmp( state->matrix, matrix, sizeof(state->matrix) )) {
          direct_memcpy( state->matrix, matrix, sizeof(state->matrix) );

          state->affine_matrix = (matrix[6] == 0x00000 && matrix[7] == 0x00000 && matrix[8] == 0x10000);

          state->modified |= SMF_MATRIX;
     }
}

void
dfb_state_set_src_colormatrix( CardState *state,
                               const s32 *matrix )
{
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( matrix != NULL );

     if (memcmp( state->src_colormatrix, matrix, sizeof(state->src_colormatrix) )) {
          direct_memcpy( state->src_colormatrix, matrix, sizeof(state->src_colormatrix) );

          state->modified |= SMF_SRC_COLORMATRIX;
     }
}

void
dfb_state_set_src_convolution( CardState                  *state,
                               const DFBConvolutionFilter *filter )
{
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( filter != NULL );

     if (memcmp( &state->src_convolution, filter, sizeof(state->src_convolution) )) {
          direct_memcpy( &state->src_convolution, filter, sizeof(state->src_convolution) );

          state->modified |= SMF_SRC_CONVOLUTION;
     }
}

void
dfb_state_set_color_or_index( CardState      *state,
                              const DFBColor *color,
                              int             index )
{
     CoreSurface *destination;
     CorePalette *palette = NULL;

     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( color != NULL );

     destination = state->destination;
     if (destination)
          palette = destination->palette;

     if (index < 0) {
          D_ASSERT( color != NULL );

          if (palette)
               dfb_state_set_color_index( state,
                                          dfb_palette_search( palette, color->r, color->g, color->b, color->a ) );

          dfb_state_set_color( state, color );
     }
     else {
          dfb_state_set_color_index( state, index );

          if (palette) {
               D_ASSERT( palette->num_entries > 0 );
               D_ASSUME( palette->num_entries > index );

               dfb_state_set_color( state, &palette->entries[index % palette->num_entries] );
          }
     }
}

DFBResult
dfb_state_get_acceleration_mask( CardState           *state,
                                 DFBAccelerationMask *ret_accel )
{
    DFBAccelerationMask mask = DFXL_NONE;

    D_MAGIC_ASSERT( state, CardState );
    D_ASSERT( ret_accel != NULL );

    dfb_state_lock( state );

    /* Check drawing functions. */

    if (dfb_gfxcard_state_check( state, DFXL_FILLRECTANGLE ))
         D_FLAGS_SET( mask, DFXL_FILLRECTANGLE );

    if (dfb_gfxcard_state_check( state, DFXL_DRAWRECTANGLE ))
         D_FLAGS_SET( mask, DFXL_DRAWRECTANGLE );

    if (dfb_gfxcard_state_check( state, DFXL_DRAWLINE ))
         D_FLAGS_SET( mask, DFXL_DRAWLINE );

    if (dfb_gfxcard_state_check( state, DFXL_FILLTRIANGLE ))
         D_FLAGS_SET( mask, DFXL_FILLTRIANGLE );

    if (dfb_gfxcard_state_check( state, DFXL_FILLTRAPEZOID ))
         D_FLAGS_SET( mask, DFXL_FILLTRAPEZOID );

    /* Check blitting functions. */

    if (state->source) {
         if (dfb_gfxcard_state_check( state, DFXL_BLIT ))
              D_FLAGS_SET( mask, DFXL_BLIT );

         if (dfb_gfxcard_state_check( state, DFXL_STRETCHBLIT ))
              D_FLAGS_SET( mask, DFXL_STRETCHBLIT );

         if (dfb_gfxcard_state_check( state, DFXL_TEXTRIANGLES ))
              D_FLAGS_SET( mask, DFXL_TEXTRIANGLES );
    }

    if (state->source2) {
         if (dfb_gfxcard_state_check( state, DFXL_BLIT2 ))
              D_FLAGS_SET( mask, DFXL_BLIT2 );
    }

    dfb_state_unlock( state );

    *ret_accel = mask;

    return DFB_OK;
}

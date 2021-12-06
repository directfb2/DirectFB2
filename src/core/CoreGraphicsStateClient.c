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

#include <core/CoreDFB.h>
#include <core/CoreGraphicsState.h>
#include <core/CoreGraphicsStateClient.h>
#include <core/core.h>
#include <core/graphics_state.h>
#include <fusion/conf.h>

D_DEBUG_DOMAIN(
     Core_GraphicsStateClient,       "Core/GfxState/Client",       "DirectFB Core Graphics State Client" );
D_DEBUG_DOMAIN(
     Core_GraphicsStateClient_Flush, "Core/GfxState/Client/Flush", "DirectFB Core Graphics State Client Flush" );

/**********************************************************************************************************************/

typedef struct clients_s {
     CoreGraphicsStateClient *client;
     struct clients_s *next;
} clients_t;

static clients_t  *clients_begin = NULL, *clients_end;
static int         lock_init = 0;
static DirectMutex lock;

void
AddClient( CoreGraphicsStateClient *client )
{
     if (!lock_init) {
          direct_mutex_init( &lock );
          lock_init = 1;
     }

     direct_mutex_lock( &lock );

     clients_t *clients = D_CALLOC (1, sizeof(clients_t));
     clients->client = client;
     clients->next = NULL;
     if (!clients_begin)
          clients_begin = clients_end = clients;
     else {
          clients_end->next = clients;
          clients_end = clients;
     }

     direct_mutex_unlock( &lock );
}

void
RemoveClient( CoreGraphicsStateClient *client )
{
     direct_mutex_lock( &lock );

     clients_t *clients = clients_begin;

     if (clients->client == client) {
          if (clients->next) {
               clients_begin = clients->next;
               D_FREE( clients );
          }
          else {
               D_FREE( clients );
               clients_begin = NULL;
               direct_mutex_unlock( &lock );
               direct_mutex_deinit( &lock );
               lock_init = 0;
               return;
          }
     }
     else {
          while (clients->next) {
               if (clients->next->client == client) {
                    clients_t *tmp_clients = clients;
                    tmp_clients = clients->next;
                    clients->next = tmp_clients->next;
                    if (!tmp_clients->next)
                         clients_end = clients;
                    D_FREE( tmp_clients );
                    break;
               }
               clients = clients->next;
          }
     }

     direct_mutex_unlock( &lock );
}

/**********************************************************************************************************************/

DFBResult
CoreGraphicsStateClient_Init( CoreGraphicsStateClient *client,
                              CardState               *state )
{
     DFBResult ret;

     D_DEBUG_AT( Core_GraphicsStateClient, "%s( %p, %p )\n", __FUNCTION__, client, state );

     D_ASSERT( client != NULL );
     D_MAGIC_ASSERT( state, CardState );
     D_MAGIC_ASSERT( state->core, CoreDFB );

     client->magic     = 0;
     client->core      = state->core;
     client->state     = state;
     client->gfx_state = NULL;

     ret = CoreDFB_CreateState( state->core, &client->gfx_state );
     if (ret)
          return ret;

     D_DEBUG_AT( Core_GraphicsStateClient, "  -> gfxstate id 0x%x\n",
                 (unsigned int) client->gfx_state->object.ref.multi.id );

     D_MAGIC_SET( client, CoreGraphicsStateClient );

     AddClient( client );

     /* Make legacy functions use state client. */
     state->client = client;

     return DFB_OK;
}

void
CoreGraphicsStateClient_Deinit( CoreGraphicsStateClient *client )
{
     D_DEBUG_AT( Core_GraphicsStateClient, "%s( %p )\n", __FUNCTION__, client );

     D_DEBUG_AT( Core_GraphicsStateClient, "  -> gfxstate id 0x%x\n",
                 (unsigned int) client->gfx_state->object.ref.multi.id );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );

     CoreGraphicsStateClient_Flush( client );

     dfb_graphics_state_unref( client->gfx_state );

     RemoveClient( client );

     D_MAGIC_CLEAR( client );
}

void
CoreGraphicsStateClient_Flush( CoreGraphicsStateClient *client )
{
     D_DEBUG_AT( Core_GraphicsStateClient_Flush, "%s( %p )\n", __FUNCTION__, client );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );

      if (!dfb_config->call_nodirect && (dfb_core_is_master( client->core ) || !fusion_config->secure_fusion)) {
           dfb_gfxcard_flush();
      }
      else {
           CoreGraphicsState_Flush( client->gfx_state );
      }
}

DFBResult
CoreGraphicsStateClient_ReleaseSource( CoreGraphicsStateClient *client )
{
     D_DEBUG_AT( Core_GraphicsStateClient, "%s( %p )\n", __FUNCTION__, client );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );

     CoreGraphicsState_ReleaseSource( client->gfx_state );

     return DFB_OK;
}

DFBResult
CoreGraphicsStateClient_SetColorAndIndex( CoreGraphicsStateClient *client,
                                          const DFBColor          *color,
                                          u32                      index )
{
     D_DEBUG_AT( Core_GraphicsStateClient, "%s( %p )\n", __FUNCTION__, client );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );

     CoreGraphicsState_SetColorAndIndex( client->gfx_state, color, index );

     return DFB_OK;
}

static DFBResult
CoreGraphicsStateClient_SetState( CoreGraphicsStateClient *client,
                                  CardState               *state,
                                  StateModificationFlags   flags )
{
     DFBResult ret;

     D_DEBUG_AT( Core_GraphicsStateClient, "%s( %p, %p, flags 0x%08x )\n", __FUNCTION__, client, state, flags );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_MAGIC_ASSERT( state, CardState );

     if (flags & SMF_DRAWING_FLAGS) {
          ret = CoreGraphicsState_SetDrawingFlags( client->gfx_state, state->drawingflags );
          if (ret)
               return ret;
     }

     if (flags & SMF_BLITTING_FLAGS) {
          ret = CoreGraphicsState_SetBlittingFlags( client->gfx_state, state->blittingflags );
          if (ret)
               return ret;
     }

     if (flags & SMF_CLIP) {
          ret = CoreGraphicsState_SetClip( client->gfx_state, &state->clip );
          if (ret)
               return ret;
     }

     if (flags & SMF_COLOR) {
          ret = CoreGraphicsState_SetColor( client->gfx_state, &state->color );
          if (ret)
               return ret;
     }

     if (flags & SMF_SRC_BLEND) {
          ret = CoreGraphicsState_SetSrcBlend( client->gfx_state, state->src_blend );
          if (ret)
               return ret;
     }

     if (flags & SMF_DST_BLEND) {
          ret = CoreGraphicsState_SetDstBlend( client->gfx_state, state->dst_blend );
          if (ret)
               return ret;
     }

     if (flags & SMF_SRC_COLORKEY) {
          ret = CoreGraphicsState_SetSrcColorKey( client->gfx_state, state->src_colorkey );
          if (ret)
               return ret;
     }

     if (flags & SMF_DST_COLORKEY) {
          ret = CoreGraphicsState_SetDstColorKey( client->gfx_state, state->dst_colorkey );
          if (ret)
               return ret;
     }

     if (flags & SMF_DESTINATION) {
          D_DEBUG_AT( Core_GraphicsStateClient, "  -> destination %p [%u]\n",
                      state->destination, state->destination->object.id );

          ret = CoreGraphicsState_SetDestination( client->gfx_state, state->destination );
          if (ret)
               return ret;
     }

     if (flags & SMF_SOURCE) {
          ret = CoreGraphicsState_SetSource( client->gfx_state, state->source );
          if (ret)
               return ret;
     }

     if (flags & SMF_SOURCE_MASK) {
          ret = CoreGraphicsState_SetSourceMask( client->gfx_state, state->source_mask );
          if (ret)
               return ret;
     }

     if (flags & SMF_SOURCE_MASK_VALS) {
          ret = CoreGraphicsState_SetSourceMaskVals( client->gfx_state, &state->src_mask_offset,
                                                     state->src_mask_flags );
          if (ret)
               return ret;
     }

     if (flags & SMF_INDEX_TRANSLATION) {
          ret = CoreGraphicsState_SetIndexTranslation( client->gfx_state, state->index_translation,
                                                       state->num_translation );
          if (ret)
               return ret;
     }

     if (flags & SMF_COLORKEY) {
          ret = CoreGraphicsState_SetColorKey( client->gfx_state, &state->colorkey );
          if (ret)
               return ret;
     }

     if (flags & SMF_RENDER_OPTIONS) {
          ret = CoreGraphicsState_SetRenderOptions( client->gfx_state, state->render_options );
          if (ret)
               return ret;
     }

     if (flags & SMF_MATRIX) {
          ret = CoreGraphicsState_SetMatrix( client->gfx_state, state->matrix );
          if (ret)
               return ret;
     }

     if (flags & SMF_SOURCE2) {
          ret = CoreGraphicsState_SetSource2( client->gfx_state, state->source2 );
          if (ret)
               return ret;
     }

     if (flags & SMF_FROM) {
          ret = CoreGraphicsState_SetFrom( client->gfx_state, state->from, state->from_eye );
          if (ret)
               return ret;
     }

     if (flags & SMF_TO) {
          ret = CoreGraphicsState_SetTo( client->gfx_state, state->to, state->to_eye );
          if (ret)
               return ret;
     }

     if (flags & SMF_SRC_CONVOLUTION) {
          ret = CoreGraphicsState_SetSrcConvolution( client->gfx_state, &state->src_convolution );
          if (ret)
               return ret;
     }

     return DFB_OK;
}

DFBResult
CoreGraphicsStateClient_Update( CoreGraphicsStateClient *client,
                                DFBAccelerationMask      accel,
                                CardState               *state )
{
     DFBResult              ret;
     StateModificationFlags flags = SMF_TO | SMF_DESTINATION | SMF_CLIP | SMF_RENDER_OPTIONS;

     D_DEBUG_AT( Core_GraphicsStateClient, "%s( %p )\n", __FUNCTION__, client );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( state->mod_hw == SMF_NONE );

     if (state->render_options & DSRO_MATRIX)
          flags |= SMF_MATRIX;

     if (DFB_DRAWING_FUNCTION( accel )) {
          flags |= SMF_DRAWING_FLAGS | SMF_COLOR;

          if (state->drawingflags & DSDRAW_BLEND)
               flags |= SMF_SRC_BLEND | SMF_DST_BLEND;

          if (state->drawingflags & DSDRAW_DST_COLORKEY)
               flags |= SMF_DST_COLORKEY;
     }
     else {
          flags |= SMF_BLITTING_FLAGS | SMF_FROM | SMF_SOURCE;

          if (accel == DFXL_BLIT2)
               flags |= SMF_FROM | SMF_SOURCE2;

          if (state->blittingflags & (DSBLIT_BLEND_COLORALPHA | DSBLIT_COLORIZE | DSBLIT_SRC_PREMULTCOLOR))
               flags |= SMF_COLOR;

          if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA))
               flags |= SMF_SRC_BLEND | SMF_DST_BLEND;

          if (state->blittingflags & DSBLIT_SRC_COLORKEY)
               flags |= SMF_SRC_COLORKEY;

          if (state->blittingflags & DSBLIT_DST_COLORKEY)
               flags |= SMF_DST_COLORKEY;

          if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR))
               flags |= SMF_FROM | SMF_SOURCE_MASK | SMF_SOURCE_MASK_VALS;

          if (state->blittingflags & DSBLIT_INDEX_TRANSLATION)
               flags |= SMF_INDEX_TRANSLATION;

          if (state->blittingflags & DSBLIT_COLORKEY_PROTECT)
               flags |= SMF_COLORKEY;

          if (state->blittingflags & DSBLIT_SRC_CONVOLUTION)
               flags |= SMF_SRC_CONVOLUTION;
     }

     ret = CoreGraphicsStateClient_SetState( client, state, state->modified & flags );
     if (ret)
          return ret;

     state->modified = state->modified & ~flags;

     return DFB_OK;
}

DFBResult
CoreGraphicsStateClient_GetAccelerationMask( CoreGraphicsStateClient *client,
                                             DFBAccelerationMask     *ret_accel )
{
     D_DEBUG_AT( Core_GraphicsStateClient, "%s( %p )\n", __FUNCTION__, client );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_ASSERT( ret_accel != NULL );

     if (!dfb_config->call_nodirect && (dfb_core_is_master( client->core ) || !fusion_config->secure_fusion)) {
          return dfb_state_get_acceleration_mask( client->state, ret_accel );
     }
     else {
          DFBResult ret;

          CoreGraphicsStateClient_Update( client, client->state->source ?
                                          (client->state->source2 ? DFXL_BLIT2 : DFXL_BLIT) : DFXL_FILLRECTANGLE,
                                          client->state );

          ret = CoreGraphicsState_GetAccelerationMask( client->gfx_state, ret_accel );
          if (ret)
               return ret;
     }

     return DFB_OK;
}

DFBResult
CoreGraphicsStateClient_FillRectangles( CoreGraphicsStateClient *client,
                                        const DFBRectangle      *rects,
                                        unsigned int             num )
{
     D_DEBUG_AT( Core_GraphicsStateClient, "%s( %p )\n", __FUNCTION__, client );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_ASSERT( rects != NULL );

     if (!dfb_config->call_nodirect && (dfb_core_is_master( client->core ) || !fusion_config->secure_fusion)) {
          dfb_gfxcard_fillrectangles( (DFBRectangle*) rects, num, client->state );
     }
     else {
          DFBResult ret;

          CoreGraphicsStateClient_Update( client, DFXL_FILLRECTANGLE, client->state );

          ret = CoreGraphicsState_FillRectangles( client->gfx_state, rects, num );
          if (ret)
               return ret;
     }

     return DFB_OK;
}

DFBResult
CoreGraphicsStateClient_DrawRectangles( CoreGraphicsStateClient *client,
                                        const DFBRectangle      *rects,
                                        unsigned int             num )
{
     D_DEBUG_AT( Core_GraphicsStateClient, "%s( %p )\n", __FUNCTION__, client );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_ASSERT( rects != NULL );

     if (!dfb_config->call_nodirect && (dfb_core_is_master( client->core ) || !fusion_config->secure_fusion)) {
          unsigned int i;

          for (i = 0; i < num; i++)
               dfb_gfxcard_drawrectangle( (DFBRectangle*) &rects[i], client->state );
     }
     else {
          DFBResult ret;

          CoreGraphicsStateClient_Update( client, DFXL_DRAWRECTANGLE, client->state );

          ret = CoreGraphicsState_DrawRectangles( client->gfx_state, rects, num );
          if (ret)
               return ret;
     }

     return DFB_OK;
}

DFBResult
CoreGraphicsStateClient_DrawLines( CoreGraphicsStateClient *client,
                                   const DFBRegion         *lines,
                                   unsigned int             num )
{
     D_DEBUG_AT( Core_GraphicsStateClient, "%s( %p )\n", __FUNCTION__, client );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_ASSERT( lines != NULL );

     if (!dfb_config->call_nodirect && (dfb_core_is_master( client->core ) || !fusion_config->secure_fusion)) {
          dfb_gfxcard_drawlines( (DFBRegion*) lines, num, client->state );
     }
     else {
          DFBResult ret;

          CoreGraphicsStateClient_Update( client, DFXL_DRAWLINE, client->state );

          ret = CoreGraphicsState_DrawLines( client->gfx_state, lines, num );
          if (ret)
               return ret;
     }

     return DFB_OK;
}

DFBResult
CoreGraphicsStateClient_FillTriangles( CoreGraphicsStateClient *client,
                                       const DFBTriangle       *triangles,
                                       unsigned int             num )
{
     D_DEBUG_AT( Core_GraphicsStateClient, "%s( %p )\n", __FUNCTION__, client );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_ASSERT( triangles != NULL );

     if (!dfb_config->call_nodirect && (dfb_core_is_master( client->core ) || !fusion_config->secure_fusion)) {
          dfb_gfxcard_filltriangles( (DFBTriangle*) triangles, num, client->state );
     }
     else {
          DFBResult ret;

          CoreGraphicsStateClient_Update( client, DFXL_FILLTRIANGLE, client->state );

          ret = CoreGraphicsState_FillTriangles( client->gfx_state, triangles, num );
          if (ret)
               return ret;
     }

     return DFB_OK;
}

DFBResult
CoreGraphicsStateClient_FillTrapezoids( CoreGraphicsStateClient *client,
                                        const DFBTrapezoid      *trapezoids,
                                        unsigned int             num )
{
     D_DEBUG_AT( Core_GraphicsStateClient, "%s( %p )\n", __FUNCTION__, client );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_ASSERT( trapezoids != NULL );

     if (!dfb_config->call_nodirect && (dfb_core_is_master( client->core ) || !fusion_config->secure_fusion)) {
          dfb_gfxcard_filltrapezoids( (DFBTrapezoid*) trapezoids, num, client->state );
     }
     else {
          DFBResult ret;

          CoreGraphicsStateClient_Update( client, DFXL_FILLTRAPEZOID, client->state );

          ret = CoreGraphicsState_FillTrapezoids( client->gfx_state, trapezoids, num );
          if (ret)
               return ret;
     }

     return DFB_OK;
}

DFBResult
CoreGraphicsStateClient_FillQuadrangles( CoreGraphicsStateClient *client,
                                         const DFBPoint          *points,
                                         unsigned int             num )
{
     D_DEBUG_AT( Core_GraphicsStateClient, "%s( %p )\n", __FUNCTION__, client );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_ASSERT( points != NULL );

     if (!dfb_config->call_nodirect && (dfb_core_is_master( client->core ) || !fusion_config->secure_fusion)) {
          dfb_gfxcard_fillquadrangles( (DFBPoint*) points, num, client->state );
     }
     else {
          DFBResult ret;

          CoreGraphicsStateClient_Update( client, DFXL_FILLQUADRANGLE, client->state );

          ret = CoreGraphicsState_FillQuadrangles( client->gfx_state, points, num );
          if (ret)
               return ret;
     }

     return DFB_OK;
}

DFBResult
CoreGraphicsStateClient_FillSpans( CoreGraphicsStateClient *client,
                                   int                      y,
                                   const DFBSpan           *spans,
                                   unsigned int             num )
{
     D_DEBUG_AT( Core_GraphicsStateClient, "%s( %p )\n", __FUNCTION__, client );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_ASSERT( spans != NULL );

     if (!dfb_config->call_nodirect && (dfb_core_is_master( client->core ) || !fusion_config->secure_fusion)) {
          dfb_gfxcard_fillspans( y, (DFBSpan*) spans, num, client->state );
     }
     else {
          DFBResult ret;

          CoreGraphicsStateClient_Update( client, DFXL_FILLRECTANGLE, client->state );

          ret = CoreGraphicsState_FillSpans( client->gfx_state, y, spans, num );
          if (ret)
               return ret;
     }

     return DFB_OK;
}

DFBResult
CoreGraphicsStateClient_Blit( CoreGraphicsStateClient *client,
                              const DFBRectangle      *rects,
                              const DFBPoint          *points,
                              unsigned int             num )
{
     D_DEBUG_AT( Core_GraphicsStateClient, "%s( %p )\n", __FUNCTION__, client );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_ASSERT( rects != NULL );
     D_ASSERT( points != NULL );

     if (!dfb_config->call_nodirect && (dfb_core_is_master( client->core ) || !fusion_config->secure_fusion)) {
          dfb_gfxcard_batchblit( (DFBRectangle*) rects, (DFBPoint*) points, num, client->state );
     }
     else {
          DFBResult    ret;
          unsigned int i;

          CoreGraphicsStateClient_Update( client, DFXL_BLIT, client->state );

          for (i = 0; i < num; i += 200) {
               ret = CoreGraphicsState_Blit( client->gfx_state, &rects[i], &points[i], MIN( 200, num - i ) );
               if (ret)
                    return ret;
          }
     }

     return DFB_OK;
}

DFBResult
CoreGraphicsStateClient_Blit2( CoreGraphicsStateClient *client,
                               const DFBRectangle      *rects,
                               const DFBPoint          *points1,
                               const DFBPoint          *points2,
                               unsigned int             num )
{
     D_DEBUG_AT( Core_GraphicsStateClient, "%s( %p )\n", __FUNCTION__, client );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_ASSERT( rects != NULL );
     D_ASSERT( points1 != NULL );
     D_ASSERT( points2 != NULL );

     if (!dfb_config->call_nodirect && (dfb_core_is_master( client->core ) || !fusion_config->secure_fusion)) {
          dfb_gfxcard_batchblit2( (DFBRectangle*) rects, (DFBPoint*) points1, (DFBPoint*) points2, num, client->state );
     }
     else {
          DFBResult ret;

          CoreGraphicsStateClient_Update( client, DFXL_BLIT2, client->state );

          ret = CoreGraphicsState_Blit2( client->gfx_state, rects, points1, points2, num );
          if (ret)
               return ret;
     }

     return DFB_OK;
}

DFBResult
CoreGraphicsStateClient_StretchBlit( CoreGraphicsStateClient *client,
                                     const DFBRectangle      *srects,
                                     const DFBRectangle      *drects,
                                     unsigned int             num )
{
     D_DEBUG_AT( Core_GraphicsStateClient, "%s( %p ) <- source buffer %p\n", __FUNCTION__,
                 client, client->state->source_buffer );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_ASSERT( srects != NULL );
     D_ASSERT( drects != NULL );

     if (num == 0)
          return DFB_OK;

     if (!dfb_config->call_nodirect && (dfb_core_is_master( client->core ) || !fusion_config->secure_fusion)) {
          if (num == 1 && srects[0].w == drects[0].w && srects[0].h == drects[0].h) {
               DFBPoint point = { drects[0].x, drects[0].y };

               D_DEBUG_AT( Core_GraphicsStateClient, "  -> %4d,%4d => %4d,%4d-%4dx%4d\n",
                           srects[0].x, srects[0].y, drects[0].x, drects[0].x, drects[0].w, drects[0].h );

               dfb_gfxcard_batchblit( (DFBRectangle*) srects, &point, 1, client->state );
          }
          else {
               dfb_gfxcard_batchstretchblit( (DFBRectangle*) srects, (DFBRectangle*) drects, num, client->state );
          }
     }
     else {
          DFBResult ret;

          if (num == 1 && srects[0].w == drects[0].w && srects[0].h == drects[0].h) {
               CoreGraphicsStateClient_Update( client, DFXL_BLIT, client->state );

               DFBPoint point = { drects[0].x, drects[0].y };
               ret = CoreGraphicsState_Blit( client->gfx_state, srects, &point, 1 );
               if (ret)
                    return ret;
          }
          else {
               CoreGraphicsStateClient_Update( client, DFXL_STRETCHBLIT, client->state );

               ret = CoreGraphicsState_StretchBlit( client->gfx_state, srects, drects, num );
               if (ret)
                    return ret;
          }
     }

     return DFB_OK;
}

DFBResult
CoreGraphicsStateClient_TileBlit( CoreGraphicsStateClient *client,
                                  const DFBRectangle      *rects,
                                  const DFBPoint          *points1,
                                  const DFBPoint          *points2,
                                  unsigned int             num )
{
     D_DEBUG_AT( Core_GraphicsStateClient, "%s( %p )\n", __FUNCTION__, client );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_ASSERT( rects != NULL );
     D_ASSERT( points1 != NULL );
     D_ASSERT( points2 != NULL );

     if (!dfb_config->call_nodirect && (dfb_core_is_master( client->core ) || !fusion_config->secure_fusion)) {
          u32 i;

          for (i = 0; i < num; i++)
               dfb_gfxcard_tileblit( (DFBRectangle*) &rects[i], points1[i].x, points1[i].y, points2[i].x, points2[i].y,
                                     client->state );
     }
     else {
          DFBResult ret;

          CoreGraphicsStateClient_Update( client, DFXL_BLIT, client->state );

          ret = CoreGraphicsState_TileBlit( client->gfx_state, rects, points1, points2, num );
          if (ret)
               return ret;
     }

     return DFB_OK;
}

DFBResult
CoreGraphicsStateClient_TextureTriangles( CoreGraphicsStateClient *client,
                                          const DFBVertex         *vertices,
                                          int                      num,
                                          DFBTriangleFormation     formation )
{
     D_DEBUG_AT( Core_GraphicsStateClient, "%s( %p )\n", __FUNCTION__, client );

     D_MAGIC_ASSERT( client, CoreGraphicsStateClient );
     D_ASSERT( vertices != NULL );

     if (!dfb_config->call_nodirect && (dfb_core_is_master( client->core ) || !fusion_config->secure_fusion)) {
          dfb_gfxcard_texture_triangles( (DFBVertex*) vertices, num, formation, client->state );
     }
     else {
          DFBResult ret;

          CoreGraphicsStateClient_Update( client, DFXL_TEXTRIANGLES, client->state );

          ret = CoreGraphicsState_TextureTriangles( client->gfx_state, vertices, num, formation );
          if (ret)
               return ret;
     }

     return DFB_OK;
}

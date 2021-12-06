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

#include <core/CoreGraphicsStateClient.h>
#include <core/state.h>
#include <gfx/util.h>

/**********************************************************************************************************************/

static bool      copy_state_inited;
static CardState copy_state;

static bool      btf_state_inited;
static CardState btf_state;

static DirectMutex copy_lock = DIRECT_MUTEX_INITIALIZER();
static DirectMutex btf_lock  = DIRECT_MUTEX_INITIALIZER();

void
dfb_gfx_copy_stereo( CoreSurface         *source,
                     DFBSurfaceStereoEye  source_eye,
                     CoreSurface         *destination,
                     DFBSurfaceStereoEye  destination_eye,
                     const DFBRectangle  *rect,
                     int                  x,
                     int                  y,
                     bool                 from_back )
{
     DFBRectangle sourcerect = { 0, 0, source->config.size.w, source->config.size.h };

     direct_mutex_lock( &copy_lock );

     if (!copy_state_inited) {
          dfb_state_init( &copy_state, NULL );
          copy_state_inited = true;
     }

     copy_state.modified   |= SMF_CLIP | SMF_SOURCE | SMF_DESTINATION | SMF_FROM | SMF_TO;
     copy_state.clip.x2     = destination->config.size.w - 1;
     copy_state.clip.y2     = destination->config.size.h - 1;
     copy_state.source      = source;
     copy_state.destination = destination;
     copy_state.from        = from_back ? DSBR_BACK : DSBR_FRONT;
     copy_state.from_eye    = source_eye;
     copy_state.to          = DSBR_BACK;
     copy_state.to_eye      = destination_eye;

     if (rect) {
          if (dfb_rectangle_intersect( &sourcerect, rect ))
               dfb_gfxcard_blit( &sourcerect, x + sourcerect.x - rect->x, y + sourcerect.y - rect->y, &copy_state );
     }
     else
          dfb_gfxcard_blit( &sourcerect, x, y, &copy_state );

     dfb_gfxcard_flush();

     /* Signal end of sequence. */
     dfb_state_stop_drawing( &copy_state );

     copy_state.destination = NULL;
     copy_state.source      = NULL;

     direct_mutex_unlock( &copy_lock );
}

void
dfb_gfx_clear( CoreSurface          *surface,
               DFBSurfaceBufferRole  role )
{
     DFBRectangle rect = { 0, 0, surface->config.size.w, surface->config.size.h };

     direct_mutex_lock( &copy_lock );

     if (!copy_state_inited) {
          dfb_state_init( &copy_state, NULL );
          copy_state_inited = true;
     }

     copy_state.modified   |= SMF_CLIP | SMF_COLOR | SMF_DESTINATION | SMF_TO;
     copy_state.clip.x2     = surface->config.size.w - 1;
     copy_state.clip.y2     = surface->config.size.h - 1;
     copy_state.destination = surface;
     copy_state.to          = role;
     copy_state.to_eye      = DSSE_LEFT;
     copy_state.color.a     = 0;
     copy_state.color.r     = 0;
     copy_state.color.g     = 0;
     copy_state.color.b     = 0;
     copy_state.color_index = 0;

     dfb_gfxcard_fillrectangles( &rect, 1, &copy_state );

     dfb_gfxcard_flush();

     /* Signal end of sequence. */
     dfb_state_stop_drawing( &copy_state );

     copy_state.destination = NULL;

     direct_mutex_unlock( &copy_lock );
}

void
dfb_gfx_stretch_stereo( CoreSurface         *source,
                        DFBSurfaceStereoEye  source_eye,
                        CoreSurface         *destination,
                        DFBSurfaceStereoEye  destination_eye,
                        const DFBRectangle  *srect,
                        const DFBRectangle  *drect,
                        bool                 from_back )
{
     DFBRectangle sourcerect = { 0, 0, source->config.size.w, source->config.size.h };
     DFBRectangle destrect   = { 0, 0, destination->config.size.w, destination->config.size.h };

     if (srect) {
          if (!dfb_rectangle_intersect( &sourcerect, srect ))
               return;
     }

     if (drect) {
          if (!dfb_rectangle_intersect( &destrect, drect ))
               return;
     }

     direct_mutex_lock( &copy_lock );

     if (!copy_state_inited) {
          dfb_state_init( &copy_state, NULL );
          copy_state_inited = true;
     }

     copy_state.modified   |= SMF_CLIP | SMF_SOURCE | SMF_DESTINATION | SMF_FROM | SMF_TO;
     copy_state.clip.x2     = destination->config.size.w - 1;
     copy_state.clip.y2     = destination->config.size.h - 1;
     copy_state.source      = source;
     copy_state.destination = destination;
     copy_state.from        = from_back ? DSBR_BACK : DSBR_FRONT;
     copy_state.from_eye    = source_eye;
     copy_state.to          = DSBR_BACK;
     copy_state.to_eye      = destination_eye;

     dfb_gfxcard_stretchblit( &sourcerect, &destrect, &copy_state );

     dfb_gfxcard_flush();

     /* Signal end of sequence. */
     dfb_state_stop_drawing( &copy_state );

     copy_state.destination = NULL;
     copy_state.source      = NULL;

     direct_mutex_unlock( &copy_lock );
}

void
dfb_gfx_copy_regions_client( CoreSurface             *source,
                             DFBSurfaceBufferRole     from,
                             DFBSurfaceStereoEye      source_eye,
                             CoreSurface             *destination,
                             DFBSurfaceBufferRole     to,
                             DFBSurfaceStereoEye      destination_eye,
                             const DFBRegion         *regions,
                             unsigned int             num,
                             int                      x,
                             int                      y,
                             CoreGraphicsStateClient *client )
{
     unsigned int  i, n = 0;
     DFBRectangle  rect = { 0, 0, source->config.size.w, source->config.size.h };
     DFBRectangle  rects[num];
     DFBPoint      points[num];
     CardState    *state;
     CardState     backup;

     for (i = 0; i < num; i++) {
          DFB_REGION_ASSERT( &regions[i] );

          rects[n] = DFB_RECTANGLE_INIT_FROM_REGION( &regions[i] );

          if (dfb_rectangle_intersect( &rects[n], &rect )) {
               points[n].x = x + rects[n].x;
               points[n].y = y + rects[n].y;

               n++;
          }
     }

     if (n > 0) {
          if (!client) {
               direct_mutex_lock( &copy_lock );

               if (!copy_state_inited) {
                    dfb_state_init( &copy_state, NULL );
                    copy_state_inited = true;
               }

               state = &copy_state;
          }
          else
               state = client->state;

          backup.clip          = state->clip;
          backup.source        = state->source;
          backup.destination   = state->destination;
          backup.from          = state->from;
          backup.from_eye      = state->from_eye;
          backup.to            = state->to;
          backup.to_eye        = state->to_eye;
          backup.blittingflags = state->blittingflags;

          state->modified     |= SMF_CLIP | SMF_SOURCE | SMF_DESTINATION | SMF_FROM | SMF_TO | SMF_BLITTING_FLAGS;
          state->clip.x1       = 0;
          state->clip.y1       = 0;
          state->clip.x2       = destination->config.size.w - 1;
          state->clip.y2       = destination->config.size.h - 1;
          state->source        = source;
          state->destination   = destination;
          state->from          = from;
          state->from_eye      = source_eye;
          state->to            = to;
          state->to_eye        = destination_eye;
          state->blittingflags = DSBLIT_NOFX;

          if (!client) {
               dfb_gfxcard_batchblit( rects, points, n, &copy_state );

               dfb_gfxcard_flush();
          }
          else {
               CoreGraphicsStateClient_Blit( client, rects, points, n );

               CoreGraphicsStateClient_Flush( client );
          }

          state->modified     |= SMF_CLIP | SMF_SOURCE | SMF_DESTINATION | SMF_FROM | SMF_TO | SMF_BLITTING_FLAGS;
          state->clip          = backup.clip;
          state->source        = backup.source;
          state->destination   = backup.destination;
          state->from          = backup.from;
          state->from_eye      = backup.from_eye;
          state->to            = backup.to;
          state->to_eye        = backup.to_eye;
          state->blittingflags = backup.blittingflags;

          if (!client)
               direct_mutex_unlock( &copy_lock );
     }
}

static void
back_to_front_copy( CoreSurface             *surface,
                    DFBSurfaceStereoEye      eye,
                    const DFBRegion         *region,
                    DFBSurfaceBlittingFlags  flags,
                    int                      rotation )
{
     DFBRectangle  rect;
     int           dx, dy;
     CardState    *state = &btf_state;

     if (region) {
          rect.x = region->x1;
          rect.y = region->y1;
          rect.w = region->x2 - region->x1 + 1;
          rect.h = region->y2 - region->y1 + 1;
     }
     else {
          rect.x = 0;
          rect.y = 0;
          rect.w = surface->config.size.w;
          rect.h = surface->config.size.h;
     }

     dx = rect.x;
     dy = rect.y;

     direct_mutex_lock( &btf_lock );

     if (!btf_state_inited) {
          dfb_state_init( state, NULL );

          state->from = DSBR_BACK;
          state->to   = DSBR_FRONT;

          btf_state_inited = true;
     }

     state->modified   |= SMF_CLIP | SMF_SOURCE | SMF_DESTINATION | SMF_FROM | SMF_TO;
     state->clip.x2     = surface->config.size.w - 1;
     state->clip.y2     = surface->config.size.h - 1;
     state->source      = surface;
     state->destination = surface;
     state->from_eye    = eye;
     state->to_eye      = eye;

     if (rotation == 90) {
          dx = rect.y;
          dy = surface->config.size.w - rect.w - rect.x;

          flags |= DSBLIT_ROTATE90;
     }
     else if (rotation == 180) {
          dx = surface->config.size.w - rect.w - rect.x;
          dy = surface->config.size.h - rect.h - rect.y;

          flags |= DSBLIT_ROTATE180;
     }
     else if (rotation == 270) {
          dx = surface->config.size.h - rect.h - rect.y;
          dy = rect.x;

          flags |= DSBLIT_ROTATE270;
     }

     dfb_state_set_blitting_flags( state, flags );

     dfb_gfxcard_blit( &rect, dx, dy, state );

     dfb_gfxcard_flush();

     /* Signal end of sequence. */
     dfb_state_stop_drawing( state );

     direct_mutex_unlock( &btf_lock );
}

void
dfb_back_to_front_copy_stereo( CoreSurface         *surface,
                               DFBSurfaceStereoEye  eyes,
                               const DFBRegion     *left_region,
                               const DFBRegion     *right_region,
                               int                  rotation )
{
     if (eyes & DSSE_LEFT)
          back_to_front_copy( surface, DSSE_LEFT, left_region, DSBLIT_NOFX, rotation );

     if (eyes & DSSE_RIGHT)
          back_to_front_copy( surface, DSSE_RIGHT, right_region, DSBLIT_NOFX, rotation );
}

void
dfb_sort_triangle( DFBTriangle *tri )
{
     int temp;

     if (tri->y1 > tri->y2) {
          temp = tri->x1;
          tri->x1 = tri->x2;
          tri->x2 = temp;

          temp = tri->y1;
          tri->y1 = tri->y2;
          tri->y2 = temp;
     }

     if (tri->y2 > tri->y3) {
          temp = tri->x2;
          tri->x2 = tri->x3;
          tri->x3 = temp;

          temp = tri->y2;
          tri->y2 = tri->y3;
          tri->y3 = temp;
     }

     if (tri->y1 > tri->y2) {
          temp = tri->x1;
          tri->x1 = tri->x2;
          tri->x2 = temp;

          temp = tri->y1;
          tri->y1 = tri->y2;
          tri->y2 = temp;
     }
}

void
dfb_sort_trapezoid( DFBTrapezoid *trap )
{
     int temp;

     if (trap->y1 > trap->y2) {
          temp = trap->x1;
          trap->x1 = trap->x2;
          trap->x2 = temp;

          temp = trap->y1;
          trap->y1 = trap->y2;
          trap->y2 = temp;

          temp = trap->w1;
          trap->w1 = trap->w2;
          trap->w2 = temp;
     }
}

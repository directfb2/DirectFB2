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

#include <core/CoreWindow.h>
#include <core/windows.h>
#include <direct/thread.h>
#include <display/idirectfbsurface.h>
#include <display/idirectfbsurface_window.h>

D_DEBUG_DOMAIN( Surface, "IDirectFBSurfaceW", "IDirectFBSurface_Window Interface" );

/**********************************************************************************************************************/

/*
 * private data struct of IDirectFBSurface_Window
 */
typedef struct {
     IDirectFBSurface_data  base;        /* base surface implementation */

     CoreWindow            *window;      /* the window object */

     DirectThread          *flip_thread; /* thread for non-flipping primary surfaces, to make changes visible */
} IDirectFBSurface_Window_data;

/**********************************************************************************************************************/

static void
IDirectFBSurface_Window_Destruct( IDirectFBSurface *thiz )
{
     IDirectFBSurface_Window_data *data = thiz->priv;

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (data->flip_thread) {
          direct_thread_cancel( data->flip_thread );
          direct_thread_join( data->flip_thread );
          direct_thread_destroy( data->flip_thread );
     }

     dfb_window_unref( data->window );

     IDirectFBSurface_Destruct( thiz );
}

static DirectResult
IDirectFBSurface_Window_Release( IDirectFBSurface *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface_Window )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (--data->base.ref == 0)
          IDirectFBSurface_Window_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_Window_Flip( IDirectFBSurface    *thiz,
                              const DFBRegion     *region,
                              DFBSurfaceFlipFlags  flags )
{
     DFBResult ret;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface_Window )

     D_DEBUG_AT( Surface, "%s( %p, %p, 0x%08x )\n", __FUNCTION__, thiz, region, flags );

     ret = IDirectFBSurface_Flip( thiz, region, flags );
     if (ret)
          return ret;

     if (!data->window->config.opacity && data->base.caps & DSCAPS_PRIMARY) {
          CoreWindowConfig config = { .opacity = 0xff };

          return CoreWindow_SetConfig( data->window, &config, NULL, 0, DWCONF_OPACITY );
     }

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_Window_FlipStereo( IDirectFBSurface    *thiz,
                                    const DFBRegion     *left_region,
                                    const DFBRegion     *right_region,
                                    DFBSurfaceFlipFlags  flags )
{
     DFBResult ret;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface_Window )

     D_DEBUG_AT( Surface, "%s( %p, %p, %p, 0x%08x )\n", __FUNCTION__, thiz, left_region, right_region, flags );

     ret = IDirectFBSurface_FlipStereo( thiz, left_region, right_region, flags );
     if (ret)
          return ret;

     if (!data->window->config.opacity && data->base.caps & DSCAPS_PRIMARY) {
          CoreWindowConfig config = { .opacity = 0xff };

          return CoreWindow_SetConfig( data->window, &config, NULL, 0, DWCONF_OPACITY );
     }

     return DFB_OK;
}

static DFBResult
IDirectFBSurface_Window_GetSubSurface( IDirectFBSurface    *thiz,
                                       const DFBRectangle  *rect,
                                       IDirectFBSurface   **ret_interface )
{
     DFBResult ret;

     DIRECT_INTERFACE_GET_DATA( IDirectFBSurface_Window )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->base.surface || !data->window || !data->window->surface)
          return DFB_DESTROYED;

     if (!ret_interface)
          return DFB_INVARG;

     direct_mutex_lock( &data->base.children_lock );

     if (data->base.children_free) {
          IDirectFBSurface_data *child_data;

          child_data = (IDirectFBSurface_data*) data->base.children_free;

          direct_list_remove( &data->base.children_free, &child_data->link );
          direct_list_append( &data->base.children_data, &child_data->link );

          direct_mutex_unlock( &data->base.children_lock );

          *ret_interface = child_data->thiz;

          ret = (*ret_interface)->MakeSubSurface( *ret_interface, thiz, rect );
          if (ret) {
               direct_mutex_unlock( &data->base.children_lock );
               return ret;
          }

          return DFB_OK;
     }

     direct_mutex_unlock( &data->base.children_lock );

     DIRECT_ALLOCATE_INTERFACE( *ret_interface, IDirectFBSurface );

     if (rect || data->base.limit_set) {
          DFBRectangle wanted, granted;

          /* Compute wanted rectangle. */
          if (rect) {
               wanted = *rect;

               wanted.x += data->base.area.wanted.x;
               wanted.y += data->base.area.wanted.y;

               if (wanted.w <= 0 || wanted.h <= 0) {
                    wanted.w = 0;
                    wanted.h = 0;
               }
          }
          else {
               wanted = data->base.area.wanted;
          }

          /* Compute granted rectangle. */
          granted = wanted;

          dfb_rectangle_intersect( &granted, &data->base.area.granted );

          ret = IDirectFBSurface_Window_Construct( *ret_interface, thiz, &wanted, &granted, data->window,
                                                   data->base.caps | DSCAPS_SUBSURFACE, data->base.core,
                                                   data->base.idirectfb );
     }
     else {
          ret = IDirectFBSurface_Window_Construct( *ret_interface, thiz, NULL, NULL, data->window,
                                                   data->base.caps | DSCAPS_SUBSURFACE, data->base.core,
                                                   data->base.idirectfb );
     }

     return ret;
}

static void *
IDirectFBSurface_Window_Flipping( DirectThread *thread,
                                  void         *arg )
{
     IDirectFBSurface             *thiz = arg;
     IDirectFBSurface_Window_data *data;

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     D_ASSERT( thiz != NULL );

     data = thiz->priv;

     D_ASSERT( data != NULL );

     while (data->base.surface && data->window->surface) {
          direct_thread_testcancel( thread );

          thiz->Flip( thiz, NULL, DSFLIP_NONE );

          direct_thread_sleep( 40000 );
     }

     return NULL;
}

DFBResult
IDirectFBSurface_Window_Construct( IDirectFBSurface       *thiz,
                                   IDirectFBSurface       *parent,
                                   DFBRectangle           *wanted,
                                   DFBRectangle           *granted,
                                   CoreWindow             *window,
                                   DFBSurfaceCapabilities  caps,
                                   CoreDFB                *core,
                                   IDirectFB              *dfb )
{
     DFBResult    ret;
     DFBInsets    insets;
     CoreSurface *surface;

     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBSurface_Window )

     D_DEBUG_AT( Surface, "%s( %p )\n", __FUNCTION__, thiz );

     D_MAGIC_ASSERT( window, CoreWindow );

     ret = CoreWindow_GetInsets( window, &insets );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     ret = CoreWindow_GetSurface( window, &surface );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     ret = IDirectFBSurface_Construct( thiz, parent, wanted, granted, &insets, surface, caps, core, dfb );

     dfb_surface_unref( surface );

     if (ret)
          return ret;

     ret = dfb_window_ref( window );
     if (ret) {
          IDirectFBSurface_Destruct( thiz );
          return ret;
     }

     data->window = window;

     /* Create an auto flipping thread if the application requested a (primary) surface that doesn't need to be flipped.
        Window surfaces even need to be flipped when they are single buffered. */
     if (!(caps & DSCAPS_FLIPPING) && !(caps & DSCAPS_SUBSURFACE)) {
          if (dfb_config->autoflip_window)
               data->flip_thread = direct_thread_create( DTT_DEFAULT, IDirectFBSurface_Window_Flipping, thiz,
                                                         "SurfWin Flipping" );
          else
               D_WARN( "non-flipping window surface and no 'autoflip-window' option used" );
     }

     thiz->Release       = IDirectFBSurface_Window_Release;
     thiz->Flip          = IDirectFBSurface_Window_Flip;
     thiz->FlipStereo    = IDirectFBSurface_Window_FlipStereo;
     thiz->GetSubSurface = IDirectFBSurface_Window_GetSubSurface;

     return DFB_OK;
}

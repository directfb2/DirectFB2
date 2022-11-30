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
#include <core/CorePalette.h>
#include <core/palette.h>
#include <direct/memcpy.h>
#include <display/idirectfbpalette.h>
#include <gfx/convert.h>

D_DEBUG_DOMAIN( Palette, "IDirectFBPalette", "IDirectFBPalette Interface" );

/**********************************************************************************************************************/

static void
IDirectFBPalette_Destruct( IDirectFBPalette *thiz )
{
     IDirectFBPalette_data *data = thiz->priv;

     D_DEBUG_AT( Palette, "%s( %p )\n", __FUNCTION__, thiz );

     if (data->palette)
          dfb_palette_unref( data->palette );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DirectResult
IDirectFBPalette_AddRef( IDirectFBPalette *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBPalette )

     D_DEBUG_AT( Palette, "%s( %p )\n", __FUNCTION__, thiz );

     data->ref++;

     return DFB_OK;
}

static DirectResult
IDirectFBPalette_Release( IDirectFBPalette *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBPalette )

     D_DEBUG_AT( Palette, "%s( %p )\n", __FUNCTION__, thiz );

     if (--data->ref == 0)
          IDirectFBPalette_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBPalette_GetCapabilities( IDirectFBPalette       *thiz,
                                  DFBPaletteCapabilities *ret_caps )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBPalette )

     D_DEBUG_AT( Palette, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->palette)
          return DFB_DESTROYED;

     if (!ret_caps)
          return DFB_INVARG;

     *ret_caps = DPCAPS_NONE;

     return DFB_OK;
}

static DFBResult
IDirectFBPalette_GetSize( IDirectFBPalette *thiz,
                          unsigned int     *ret_size )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBPalette )

     D_DEBUG_AT( Palette, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->palette)
          return DFB_DESTROYED;

     if (!ret_size)
          return DFB_INVARG;

     *ret_size = data->palette->num_entries;

     return DFB_OK;
}

static DFBResult
IDirectFBPalette_SetEntries( IDirectFBPalette *thiz,
                             const DFBColor   *entries,
                             unsigned int      num_entries,
                             unsigned int      offset )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBPalette )

     D_DEBUG_AT( Palette, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->palette)
          return DFB_DESTROYED;

     if (!entries || offset + num_entries > data->palette->num_entries)
          return DFB_INVARG;

     return CorePalette_SetEntries( data->palette, entries, num_entries, offset );
}

static DFBResult
IDirectFBPalette_GetEntries( IDirectFBPalette *thiz,
                             DFBColor         *ret_entries,
                             unsigned int      num_entries,
                             unsigned int      offset )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBPalette )

     D_DEBUG_AT( Palette, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->palette)
          return DFB_DESTROYED;

     if (!ret_entries || offset + num_entries > data->palette->num_entries)
          return DFB_INVARG;

     direct_memcpy( ret_entries, data->palette->entries + offset, num_entries * sizeof(DFBColor) );

     return DFB_OK;
}

static DFBResult
IDirectFBPalette_FindBestMatch( IDirectFBPalette *thiz,
                                u8                r,
                                u8                g,
                                u8                b,
                                u8                a,
                                unsigned int     *ret_index )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBPalette )

     D_DEBUG_AT( Palette, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_index)
          return DFB_INVARG;

     if (!data->palette)
          return DFB_DESTROYED;

     *ret_index = dfb_palette_search( data->palette, r, g, b, a );

     return DFB_OK;
}

static DFBResult
IDirectFBPalette_CreateCopy( IDirectFBPalette  *thiz,
                             IDirectFBPalette **ret_interface )
{
     DFBResult         ret;
     IDirectFBPalette *iface;
     CorePalette      *palette = NULL;

     DIRECT_INTERFACE_GET_DATA( IDirectFBPalette )

     D_DEBUG_AT( Palette, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->palette)
          return DFB_DESTROYED;

     if (!ret_interface)
          return DFB_INVARG;

     ret = CoreDFB_CreatePalette( data->core, data->palette->num_entries, data->palette->colorspace, &palette );
     if (ret)
          return ret;

     CorePalette_SetEntries( palette, data->palette->entries, palette->num_entries, 0 );

     DIRECT_ALLOCATE_INTERFACE( iface, IDirectFBPalette );

     ret = IDirectFBPalette_Construct( iface, palette, data->core );

     dfb_palette_unref( palette );

     if (ret == DFB_OK)
          *ret_interface = iface;

     return ret;
}

static DFBResult
IDirectFBPalette_SetEntriesYUV( IDirectFBPalette  *thiz,
                                const DFBColorYUV *entries,
                                unsigned int       num_entries,
                                unsigned int       offset )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBPalette )

     D_DEBUG_AT( Palette, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->palette)
          return DFB_DESTROYED;

     if (!entries || offset + num_entries > data->palette->num_entries)
          return DFB_INVARG;

     return CorePalette_SetEntriesYUV( data->palette, entries, num_entries, offset );
}

static DFBResult
IDirectFBPalette_GetEntriesYUV( IDirectFBPalette *thiz,
                                DFBColorYUV      *ret_entries,
                                unsigned int      num_entries,
                                unsigned int      offset )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBPalette )

     D_DEBUG_AT( Palette, "%s( %p )\n", __FUNCTION__, thiz );

     if (!data->palette)
          return DFB_DESTROYED;

     if (!ret_entries || offset + num_entries > data->palette->num_entries)
          return DFB_INVARG;

     direct_memcpy( ret_entries, data->palette->entries_yuv + offset, num_entries * sizeof(DFBColorYUV) );

     return DFB_OK;
}

static DFBResult
IDirectFBPalette_FindBestMatchYUV( IDirectFBPalette *thiz,
                                   u8                y,
                                   u8                u,
                                   u8                v,
                                   u8                a,
                                   unsigned int     *ret_index )
{
     int r, g, b;

     DIRECT_INTERFACE_GET_DATA( IDirectFBPalette )

     D_DEBUG_AT( Palette, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_index)
          return DFB_INVARG;

     if (!data->palette)
          return DFB_DESTROYED;

     if (data->palette->colorspace == DSCS_BT601)
          YCBCR_TO_RGB_BT601( y, u, v, r, g, b );
     else if (data->palette->colorspace == DSCS_RGB || data->palette->colorspace == DSCS_BT709)
          YCBCR_TO_RGB_BT709( y, u, v, r, g, b );
     else if (data->palette->colorspace == DSCS_BT2020)
          YCBCR_TO_RGB_BT2020( y, u, v, r, g, b );
     else
          r = g = b = 0;

     *ret_index = dfb_palette_search( data->palette, r, g, b, a );

     return DFB_OK;
}

DFBResult
IDirectFBPalette_Construct( IDirectFBPalette *thiz,
                            CorePalette      *palette,
                            CoreDFB          *core )
{
     DFBResult ret;

     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBPalette )

     D_DEBUG_AT( Palette, "%s( %p )\n", __FUNCTION__, thiz );

     ret = dfb_palette_ref( palette );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     data->ref     = 1;
     data->palette = palette;
     data->core    = core;

     thiz->AddRef           = IDirectFBPalette_AddRef;
     thiz->Release          = IDirectFBPalette_Release;
     thiz->GetCapabilities  = IDirectFBPalette_GetCapabilities;
     thiz->GetSize          = IDirectFBPalette_GetSize;
     thiz->SetEntries       = IDirectFBPalette_SetEntries;
     thiz->GetEntries       = IDirectFBPalette_GetEntries;
     thiz->FindBestMatch    = IDirectFBPalette_FindBestMatch;
     thiz->CreateCopy       = IDirectFBPalette_CreateCopy;
     thiz->SetEntriesYUV    = IDirectFBPalette_SetEntriesYUV;
     thiz->GetEntriesYUV    = IDirectFBPalette_GetEntriesYUV;
     thiz->FindBestMatchYUV = IDirectFBPalette_FindBestMatchYUV;

     return DFB_OK;
}

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

#include <direct/util.h>

#include "drmkms_mode.h"

D_DEBUG_DOMAIN( DRMKMS_Mode, "DRMKMS/Mode", "DRM/KMS Mode" );

/**********************************************************************************************************************/

static int xres_table[] = {
      640,  720,  720,  800, 1024, 1152, 1280, 1280, 1280, 1280, 1400, 1600, 1920, 960, 1440, 800, 1024, 1366, 1920,
     2560, 2560, 3840, 4096
};

static int yres_table[] = {
      480,  480,  576,  600,  768,  864,  720,  768,  960, 1024, 1050, 1200, 1080, 540,  540, 480,  600,  768, 1200,
     1440, 1600, 2160, 2160
};

static int freq_table[] = { 25, 30, 50, 59, 60, 75, 30, 24, 23 };

DFBScreenOutputResolution
drmkms_modes_to_dsor_bitmask( DRMKMSData *drmkms,
                              int         connector )
{
     int                        i, j;
     drmModeModeInfo           *mode = drmkms->connector[connector]->modes;
     DFBScreenOutputResolution  dsor = DSOR_UNKNOWN;

     D_DEBUG_AT( DRMKMS_Mode, "%s()\n", __FUNCTION__ );

     for (i = 0; i < drmkms->connector[connector]->count_modes; i++) {
          for (j = 0; j < D_ARRAY_SIZE(xres_table); j++) {
               if (mode[i].hdisplay == xres_table[j] && mode[i].vdisplay == yres_table[j]) {
                    dsor |= (1 << j);
                    break;
               }
          }
     }

     return dsor;
}

drmModeModeInfo *
drmkms_find_mode( DRMKMSData   *drmkms,
                  int           connector,
                  int           width,
                  int           height,
                  unsigned int  freq )
{
     int              i;
     drmModeModeInfo *mode = drmkms->connector[connector]->modes;
     drmModeModeInfo *found_mode = NULL;

     D_DEBUG_AT( DRMKMS_Mode, "%s()\n", __FUNCTION__ );

     for (i = 0; i < drmkms->connector[connector]->count_modes; i++) {
          if (mode[i].hdisplay == width && mode[i].vdisplay == height && ((mode[i].vrefresh == freq) || (freq == 0))) {
               found_mode = &mode[i];
               D_DEBUG_AT( DRMKMS_Mode, "  -> found mode %dx%d@%uHz\n", width, height, mode[i].vrefresh );
               break;
          }
          else
               D_DEBUG_AT( DRMKMS_Mode, "  -> mode %dx%d@%uHz does not match requested %dx%d@%uHz\n",
                           mode[i].hdisplay, mode[i].vdisplay, mode[i].vrefresh, width, height, freq );

     }

     if (!found_mode)
          D_ONCE( "no mode found for %dx%d at %u Hz\n", width, height, freq );

     return found_mode;
}

drmModeModeInfo *
drmkms_dsor_dsef_to_mode( DRMKMSData                *drmkms,
                          int                        connector,
                          DFBScreenOutputResolution  dsor,
                          DFBScreenEncoderFrequency  dsef )
{
     unsigned int res  = D_BITn32( dsor );
     unsigned int freq = D_BITn32( dsef );

     D_DEBUG_AT( DRMKMS_Mode, "%s( dsor %x, dsef %x)\n", __FUNCTION__, dsor, dsef );

     if (res == -1 || res >= D_ARRAY_SIZE(xres_table) || freq >= D_ARRAY_SIZE(freq_table))
          return NULL;

     if (freq == -1)
          freq = 0;

     return drmkms_find_mode( drmkms, connector, xres_table[res], yres_table[res], freq_table[freq] );
}

DFBResult
drmkms_mode_to_dsor_dsef( const drmModeModeInfo     *mode,
                          DFBScreenOutputResolution *dsor,
                          DFBScreenEncoderFrequency *dsef )
{
     int j;

     D_DEBUG_AT( DRMKMS_Mode, "%s()\n", __FUNCTION__ );

     if (dsor)
          *dsor = DSOR_UNKNOWN;

     if (dsef)
          *dsef = DSEF_UNKNOWN;

     if (dsor) {
          for (j = 0; j < D_ARRAY_SIZE(xres_table); j++) {
               if (mode->hdisplay == xres_table[j] && mode->vdisplay == yres_table[j]) {
                    *dsor = 1 << j;
                    break;
               }
          }
     }

     if (dsef) {
          for (j = 0; j < D_ARRAY_SIZE(freq_table); j++) {
               if (mode->vrefresh == freq_table[j]) {
                    *dsef = 1 << j;
                    break;
               }
          }
     }

     return DFB_OK;
}

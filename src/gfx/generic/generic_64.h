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

static void
Cop_to_Aop_32_64( GenefxState *gfxs )
{
     int  l;
     int  w    = gfxs->length;
     u32 *D    = gfxs->Aop[0];
     u32  Cop  = gfxs->Cop;
     u64  DCop = ((u64) Cop << 32) | Cop;

     if ((long) D & 4) {
          *D++ = Cop;
          w--;
     }

     for (l = w >> 1; l; l--) {
          *((u64*) D) = DCop;
          D += 2;
     }

     if (w & 1)
          *D = Cop;
}

static void
Bop_rgb32_toK_Aop_64( GenefxState *gfxs )
{
     int  l;
     int  w     = gfxs->length;
     u32 *S     = gfxs->Bop[0];
     u32 *D     = gfxs->Aop[0];
     u32  Dkey  = gfxs->Dkey;
     u64  DDkey = ((u64) Dkey << 32) | Dkey;

     if ((long) D & 4) {
          if ((*D & 0x00ffffff) == Dkey)
               *D = *S;

          S++;
          D++;
          w--;
     }

     for (l = w >> 1; l; l--) {
          u64 d = *((u64*) D);

          if ((d & 0x00ffffff00000000ull) == (DDkey & 0x00ffffff00000000ull)) {
               if ((d & 0x0000000000ffffffull) == (DDkey & 0x0000000000ffffffull)) {
                    *((u64*) D) = *((u64*) S);
               }
               else {
#ifdef WORDS_BIGENDIAN
                    D[0] = S[0];
#else
                    D[1] = S[1];
#endif
               }
          }
          else if ((d & 0x0000000000ffffffull) == (DDkey & 0x0000000000ffffffull)) {
#ifdef WORDS_BIGENDIAN
               D[1] = S[1];
#else
               D[0] = S[0];
#endif
          }

          S += 2;
          D += 2;
     }

     if (w & 1) {
          if ((*D & 0x00ffffff) == Dkey)
               *D = *S;
     }
}

static void
Bop_rgb32_Kto_Aop_64( GenefxState *gfxs )
{
     int  l;
     int  w     = gfxs->length;
     u32 *S     = gfxs->Bop[0];
     u32 *D     = gfxs->Aop[0];
     u32  Skey  = gfxs->Skey;
     u64  DSkey = ((u64) Skey << 32) | Skey;

     if ((long) D & 4) {
          if ((*S & 0x00ffffff) != Skey)
               *D = *S;

          S++;
          D++;
          w--;
     }

     for (l = w >> 1; l; l--) {
          u64 s = *((u64*) S);

          if ((s & 0x00ffffff00ffffffull) != DSkey) {
               if ((s & 0x00ffffff00000000ull) != (DSkey & 0x00ffffff00000000ull)) {
                    if ((s & 0x0000000000ffffffull) != (DSkey & 0x0000000000ffffffull)) {
                         *((u64*) D) = s;
                    }
                    else {
#ifdef WORDS_BIGENDIAN
                         D[0] = (u32) (s >> 32);
#else
                         D[1] = (u32) (s >> 32);
#endif
                    }
               }
               else {
#ifdef WORDS_BIGENDIAN
                    D[1] = (u32) s;
#else
                    D[0] = (u32) s;
#endif
               }
          }

          S += 2;
          D += 2;
     }

     if (w & 1) {
          if ((*S & 0x00ffffff) != Skey)
               *D = *S;
     }
}

static void
Bop_32_Sto_Aop_64( GenefxState *gfxs )
{
     int  l;
     int  w      = gfxs->length;
     int  i      = 0;
     u32 *D      = gfxs->Aop[0];
     u32 *S      = gfxs->Bop[0];
     int  SperD  = gfxs->SperD;
     int  SperD2 = SperD << 1;

     if ((long) D & 4) {
          *D++ = *S;
          i += SperD;
          w--;
     }

     for (l = w >> 1; l; l--) {
#ifdef WORDS_BIGENDIAN
          *((u64*) D) = ((u64) S[i>>16] << 32) | S[(i+SperD)>>16];
#else
          *((u64*) D) = ((u64) S[(i+SperD)>>16] << 32) | S[i>>16];
#endif
          D += 2;
          i += SperD2;
     }

     if (w & 1)
          *D = S[i>>16];
}

static void
Dacc_xor_64( GenefxState *gfxs )
{
     int  w = gfxs->length;
     u64 *D = (u64*) gfxs->Dacc;
     u64  color;

#ifdef WORDS_BIGENDIAN
     color = ((u64) gfxs->color.b << 48) |
             ((u64) gfxs->color.g << 32) |
             ((u64) gfxs->color.r << 16) |
             ((u64) gfxs->color.a);
#else
     color = ((u64) gfxs->color.a << 48) |
             ((u64) gfxs->color.r << 32) |
             ((u64) gfxs->color.g << 16) |
             ((u64) gfxs->color.b);
#endif

     for (; w; w--) {
          *D ^= color;
          D++;
     }
}

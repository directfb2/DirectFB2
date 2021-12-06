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

#ifdef WORD_BIGENDIAN
#define READ_PIXEL(S)                     \
     (                                    \
          ((S)[0] << 16) |                \
          ((S)[1] <<  8) |                \
           (S)[2]                         \
     )
#define WRITE_PIXEL(D,pix)                \
     do {                                 \
           (D)[0] = ((pix) >> 16) & 0xff; \
           (D)[1] = ((pix) >>  8) & 0xff; \
           (D)[2] =  (pix)        & 0xff; \
     } while (0)
#else
#define READ_PIXEL(S)                     \
     (                                    \
          ((S)[2] << 16) |                \
          ((S)[1] <<  8) |                \
           (S)[0]                         \
     )
#define WRITE_PIXEL(d,pix)                \
     do {                                 \
           (D)[0] =  (pix)        & 0xff; \
           (D)[1] = ((pix) >>  8) & 0xff; \
           (D)[2] = ((pix) >> 16) & 0xff; \
     } while (0)
#endif

/**********************************************************************************************************************
 ********************************* Cop_toK_Aop_PFI ********************************************************************
 **********************************************************************************************************************/

static void
Cop_OP_Aop_PFI(toK)( GenefxState *gfxs )
{
     int  w    = gfxs->length + 1;
     u8  *D    = gfxs->Aop[0];
     u32  Cop  = gfxs->Cop;
     u32  Dkey = gfxs->Dkey;

     while (--w) {
          u32 d = READ_PIXEL( D );

          if ((d & RGB_MASK) == Dkey)
               WRITE_PIXEL( D, Cop );

          D += 3;
     }
}

/**********************************************************************************************************************
 ********************************* Bop_PFI_toK_Aop_PFI ****************************************************************
 **********************************************************************************************************************/

static void
Bop_PFI_OP_Aop_PFI(toK)( GenefxState *gfxs )
{
     int  w     = gfxs->length + 1;
     u8  *S     = gfxs->Bop[0];
     u8  *D     = gfxs->Aop[0];
     u32  Dkey  = gfxs->Dkey;
     int  Ostep = gfxs->Ostep * 3;

     if (Ostep < 0) {
          S += (gfxs->length - 1) * 3;
          D += (gfxs->length - 1) * 3;
     }

     while (--w) {
          u32 d = READ_PIXEL( D );

          if ((d & RGB_MASK) == Dkey) {
               D[0] = S[0];
               D[1] = S[1];
               D[2] = S[2];
          }

          S += Ostep;
          D += Ostep;
     }
}

/**********************************************************************************************************************
 ********************************* Bop_PFI_Kto_Aop_PFI ****************************************************************
 **********************************************************************************************************************/

static void
Bop_PFI_OP_Aop_PFI(Kto)( GenefxState *gfxs )
{
     int  w     = gfxs->length + 1;
     u8  *S     = gfxs->Bop[0];
     u8  *D     = gfxs->Aop[0];
     u32  Skey  = gfxs->Skey;
     int  Ostep = gfxs->Ostep * 3;

     if (Ostep < 0) {
          S += (gfxs->length - 1) * 3;
          D += (gfxs->length - 1) * 3;
     }

     while (--w) {
          u32 s = READ_PIXEL( S );

          if ((s & RGB_MASK) != Skey) {
               D[0] = S[0];
               D[1] = S[1];
               D[2] = S[2];
          }

          S += Ostep;
          D += Ostep;
     }
}

/**********************************************************************************************************************
 ********************************* Bop_PFI_KtoK_Aop_PFI ***************************************************************
 **********************************************************************************************************************/

static void
Bop_PFI_OP_Aop_PFI(KtoK)( GenefxState *gfxs )
{
     int  w     = gfxs->length + 1;
     u8  *S     = gfxs->Bop[0];
     u8  *D     = gfxs->Aop[0];
     u32  Skey  = gfxs->Skey;
     u32  Dkey  = gfxs->Dkey;
     int  Ostep = gfxs->Ostep * 3;

     if (Ostep < 0) {
          S += (gfxs->length - 1) * 3;
          D += (gfxs->length - 1) * 3;
     }

     while (--w) {
          u32 s = READ_PIXEL( S );

          if ((s & RGB_MASK) != Skey) {
               u32 d = READ_PIXEL( D );

               if ((d & RGB_MASK) == Dkey) {
                    D[0] = S[0];
                    D[1] = S[1];
                    D[2] = S[2];
               }
          }

          S += Ostep;
          D += Ostep;
     }
}

/**********************************************************************************************************************
 ********************************* Bop_PFI_SKto_Aop_PFI ***************************************************************
 **********************************************************************************************************************/

static void
Bop_PFI_OP_Aop_PFI(SKto)( GenefxState *gfxs )
{
     int  i     = gfxs->Xphase;
     int  w     = gfxs->length + 1;
     u8  *S     = gfxs->Bop[0];
     u8  *D     = gfxs->Aop[0];
     u32  Skey  = gfxs->Skey;
     int  Dstep = gfxs->Astep * 3;
     int  SperD = gfxs->SperD;

     while (--w) {
          int pixelstart = (i >> 16) * 3;
          u32 s = READ_PIXEL( &S[pixelstart] );

          if ((s & RGB_MASK) != Skey) {
               D[0] = S[pixelstart++];
               D[1] = S[pixelstart++];
               D[2] = S[pixelstart];
          }

          D += Dstep;
          i += SperD;
     }
}

/**********************************************************************************************************************
 ********************************* Bop_PFI_StoK_Aop_PFI ***************************************************************
 **********************************************************************************************************************/

static void
Bop_PFI_OP_Aop_PFI(StoK)( GenefxState *gfxs )
{
     int  i     = gfxs->Xphase;
     int  w     = gfxs->length + 1;
     u8  *S     = gfxs->Bop[0];
     u8  *D     = gfxs->Aop[0];
     u32  Dkey  = gfxs->Dkey;
     int  Dstep = gfxs->Astep * 3;
     int  SperD = gfxs->SperD;

     while (--w) {
          u32 d = READ_PIXEL( D );

          if ((d & RGB_MASK) == Dkey) {
               int pixelstart = (i >> 16) * 3;
               D[0] = S[pixelstart++];
               D[1] = S[pixelstart++];
               D[2] = S[pixelstart];
          }

          D += Dstep;
          i += SperD;
     }
}

/**********************************************************************************************************************
 ********************************* Bop_PFI_SKtoK_Aop_PFI **************************************************************
 **********************************************************************************************************************/

static void
Bop_PFI_OP_Aop_PFI(SKtoK)( GenefxState *gfxs )
{
     int  i     = gfxs->Xphase;
     int  w     = gfxs->length + 1;
     u8  *S     = gfxs->Bop[0];
     u8  *D     = gfxs->Aop[0];
     u32  Skey  = gfxs->Skey;
     u32  Dkey  = gfxs->Dkey;
     int  SperD = gfxs->SperD;

     while (--w) {
          int pixelstart = (i >> 16) * 3;
          u32 s = READ_PIXEL( &S[pixelstart] );

          if ((s & RGB_MASK) != Skey) {
               u32 d = READ_PIXEL( D );

               if ((d & RGB_MASK) == Dkey) {
                    D[0] = S[pixelstart++];
                    D[1] = S[pixelstart++];
                    D[2] = S[pixelstart];
               }
          }

          D += 3;
          i += SperD;
     }
}

/**********************************************************************************************************************/

#undef READ_PIXEL
#undef WRITE_PIXEL

#undef RGB_MASK
#undef Cop_OP_Aop_PFI
#undef Bop_PFI_OP_Aop_PFI

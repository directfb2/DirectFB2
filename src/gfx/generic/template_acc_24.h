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

#define RGB_MASK (R_MASK | G_MASK | B_MASK)

#if RGB_MASK == 0xffffff
#define MASK_RGB(p) (p)
#else
#define MASK_RGB(p) ((p) & RGB_MASK)
#endif

#define PIXEL(x) PIXEL_OUT( ((x).RGB.a & 0xff00) ? 0xff : (x).RGB.a, \
                            ((x).RGB.r & 0xff00) ? 0xff : (x).RGB.r, \
                            ((x).RGB.g & 0xff00) ? 0xff : (x).RGB.g, \
                            ((x).RGB.b & 0xff00) ? 0xff : (x).RGB.b )

#define EXPAND(d,s) do {                                 \
     (d).RGB.a = EXPAND_Ato8( (s & A_MASK) >> A_SHIFT ); \
     (d).RGB.r = EXPAND_Rto8( (s & R_MASK) >> R_SHIFT ); \
     (d).RGB.g = EXPAND_Gto8( (s & G_MASK) >> G_SHIFT ); \
     (d).RGB.b = EXPAND_Bto8( (s & B_MASK) >> B_SHIFT ); \
} while (0)

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
#define WRITE_PIXEL(D,pix)                \
     do {                                 \
           (D)[0] =  (pix)        & 0xff; \
           (D)[1] = ((pix) >>  8) & 0xff; \
           (D)[2] = ((pix) >> 16) & 0xff; \
     } while (0)
#endif

/**********************************************************************************************************************
 ********************************* Sop_PFI_to_Dacc ********************************************************************
 **********************************************************************************************************************/

static void
Sop_PFI_OP_Dacc(to)( GenefxState *gfxs )
{
     int                w     = gfxs->length + 1;
     u8                *S     = gfxs->Sop[0];
     GenefxAccumulator *D     = gfxs->Dacc;
     int                Ostep = gfxs->Ostep * 3;

     while (--w) {
          u32 s = READ_PIXEL( S );

          EXPAND( *D, s );

          S += Ostep;
          ++D;
     }
}

/**********************************************************************************************************************
 ********************************* Sop_PFI_Kto_Dacc *******************************************************************
 **********************************************************************************************************************/

static void
Sop_PFI_OP_Dacc(Kto)( GenefxState *gfxs )
{
     int                w     = gfxs->length + 1;
     u8                *S     = gfxs->Sop[0];
     GenefxAccumulator *D     = gfxs->Dacc;
     u32                Skey  = gfxs->Skey;
     int                Ostep = gfxs->Ostep * 3;

     while (--w) {
          u32 s = READ_PIXEL( S );

          if (MASK_RGB( s ) != Skey)
               EXPAND( *D, s );
          else
               D->RGB.a = 0xf000;

          S += Ostep;
          ++D;
     }
}

/**********************************************************************************************************************
 ********************************* Sop_PFI_Sto_Dacc *******************************************************************
 **********************************************************************************************************************/

static void
Sop_PFI_OP_Dacc(Sto)( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     u8                *S     = gfxs->Sop[0];
     GenefxAccumulator *D     = gfxs->Dacc;
     int                SperD = gfxs->SperD;
     int                Ostep = gfxs->Ostep;

     if (Ostep != 1)
          D_UNIMPLEMENTED();

     while (--w) {
          int pixelstart = (i >> 16) * 3;
          u32 s = READ_PIXEL( &S[pixelstart] );

          EXPAND( *D, s );

          ++D;
          i += SperD;
     }
}

/**********************************************************************************************************************
 ********************************* Sop_PFI_SKto_Dacc ******************************************************************
 **********************************************************************************************************************/

static void
Sop_PFI_OP_Dacc(SKto)( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     u8                *S     = gfxs->Sop[0];
     GenefxAccumulator *D     = gfxs->Dacc;
     u32                Skey  = gfxs->Skey;
     int                SperD = gfxs->SperD;
     int                Ostep = gfxs->Ostep;

     if (Ostep != 1)
          D_UNIMPLEMENTED();

     while (--w) {
          int pixelstart = (i >> 16) * 3;
          u32 s = READ_PIXEL( &S[pixelstart] );

          if (MASK_RGB( s ) != Skey)
               EXPAND( *D, s );
          else
               D->RGB.a = 0xf000;

          ++D;
          i += SperD;
     }
}

/**********************************************************************************************************************
 ********************************* Sop_PFI_TEX_to_Dacc ****************************************************************
 **********************************************************************************************************************/

static void
Sop_PFI_OP_Dacc(TEX_to)( GenefxState *gfxs )
{
     int                s     = gfxs->s;
     int                t     = gfxs->t;
     int                w     = gfxs->length + 1;
     u8                *S     = gfxs->Sop[0];
     GenefxAccumulator *D     = gfxs->Dacc;
     int                sp3   = gfxs->src_pitch / 3;
     int                SperD = gfxs->SperD;
     int                TperD = gfxs->TperD;
     int                Ostep = gfxs->Ostep * 3;

     if (Ostep != 1)
          D_UNIMPLEMENTED();

     while (--w) {
          int pixelstart = ((s >> 16) + (t >> 16) * sp3) * 3;
          u32 p = READ_PIXEL( &S[pixelstart] );

          EXPAND( *D, p );

          ++D;
          s += SperD;
          t += TperD;
     }
}

/**********************************************************************************************************************
 ********************************* Sop_PFI_TEX_Kto_Dacc ***************************************************************
 **********************************************************************************************************************/

static void
Sop_PFI_OP_Dacc(TEX_Kto)( GenefxState *gfxs )
{
     int                s     = gfxs->s;
     int                t     = gfxs->t;
     int                w     = gfxs->length + 1;
     u8                *S     = gfxs->Sop[0];
     GenefxAccumulator *D     = gfxs->Dacc;
     int                sp3   = gfxs->src_pitch / 3;
     u32                Skey  = gfxs->Skey;
     int                SperD = gfxs->SperD;
     int                TperD = gfxs->TperD;
     int                Ostep = gfxs->Ostep * 3;

     if (Ostep != 1)
          D_UNIMPLEMENTED();

     while (--w) {
          int pixelstart = ((s >> 16) + (t >> 16) * sp3) * 3;
          u32 p = READ_PIXEL( &S[pixelstart] );

          if (MASK_RGB( p ) != Skey)
               EXPAND( *D, p );
          else
               D->RGB.a = 0xf000;

          ++D;
          s += SperD;
          t += TperD;
     }
}

/**********************************************************************************************************************
 ********************************* Sacc_to_Aop_PFI ********************************************************************
 **********************************************************************************************************************/

static void
Sacc_OP_Aop_PFI(to)( GenefxState *gfxs )
{
     int                w     = gfxs->length + 1;
     GenefxAccumulator *S     = gfxs->Sacc;
     u8                *D     = gfxs->Aop[0];
     int                Dstep = gfxs->Astep * 3;

     while (--w) {
          if (!(S->RGB.a & 0xf000)) {
               u32 pix = PIXEL( *S );
               WRITE_PIXEL( D, pix );
          }

          ++S;
          D += Dstep;
     }
}

/**********************************************************************************************************************
 ********************************* Sacc_toK_Aop_PFI *******************************************************************
 **********************************************************************************************************************/

static void
Sacc_OP_Aop_PFI(toK)( GenefxState *gfxs )
{
     int                w     = gfxs->length + 1;
     GenefxAccumulator *S     = gfxs->Sacc;
     u8                *D     = gfxs->Aop[0];
     u32                Dkey  = gfxs->Dkey;
     int                Dstep = gfxs->Astep * 3;

     while (--w) {
          if (!(S->RGB.a & 0xf000)) {
               u32 pix = READ_PIXEL( D );

               if (MASK_RGB( pix ) == Dkey) {
                    pix = PIXEL( *S );
                    WRITE_PIXEL( D, pix );
               }
          }

          ++S;
          D += Dstep;
     }
}

/**********************************************************************************************************************
 ********************************* Sacc_Sto_Aop_PFI *******************************************************************
 **********************************************************************************************************************/

static void
Sacc_OP_Aop_PFI(Sto)( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     GenefxAccumulator *S     = gfxs->Sacc;
     u8                *D     = gfxs->Aop[0];
     int                Dstep = gfxs->Astep * 3;
     int                SperD = gfxs->SperD;

     while (--w) {
          GenefxAccumulator *S0 = &S[i>>16];

          if (!(S0->RGB.a & 0xf000)) {
               u32 pix = PIXEL( *S0 );
               WRITE_PIXEL( D, pix );
          }

          D += Dstep;
          i += SperD;
     }
}

/**********************************************************************************************************************
 ********************************* Sacc_StoK_Aop_PFI ******************************************************************
 **********************************************************************************************************************/

static void
Sacc_OP_Aop_PFI(StoK)( GenefxState *gfxs )
{
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length + 1;
     GenefxAccumulator *S     = gfxs->Sacc;
     u8                *D     = gfxs->Aop[0];
     u32                Dkey  = gfxs->Dkey;
     int                Dstep = gfxs->Astep * 3;
     int                SperD = gfxs->SperD;

     while (--w) {
          GenefxAccumulator *S0 = &S[i>>16];

          if (!(S0->RGB.a & 0xf000)) {
               u32 pix = READ_PIXEL( D );

               if (MASK_RGB( pix ) == Dkey) {
                    pix = PIXEL( *S0 );
                    WRITE_PIXEL( D, pix );
               }
          }

          D += Dstep;
          i += SperD;
     }
}

/**********************************************************************************************************************/

#undef RGB_MASK
#undef MASK_RGB
#undef PIXEL
#undef EXPAND
#undef READ_PIXEL
#undef WRITE_PIXEL

#undef A_SHIFT
#undef R_SHIFT
#undef G_SHIFT
#undef B_SHIFT
#undef A_MASK
#undef R_MASK
#undef G_MASK
#undef B_MASK
#undef PIXEL_OUT
#undef EXPAND_Ato8
#undef EXPAND_Rto8
#undef EXPAND_Gto8
#undef EXPAND_Bto8
#undef Sop_PFI_OP_Dacc
#undef Sacc_OP_Aop_PFI

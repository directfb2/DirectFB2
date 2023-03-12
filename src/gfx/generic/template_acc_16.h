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

#if RGB_MASK == 0xffff
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

/**********************************************************************************************************************
 ********************************* Sop_PFI_to_Dacc ********************************************************************
 **********************************************************************************************************************/

static void
Sop_PFI_OP_Dacc(to)( GenefxState *gfxs )
{
     int                l;
     int                w     = gfxs->length;
     u16               *S     = gfxs->Sop[0];
     GenefxAccumulator *D     = gfxs->Dacc;
     int                Ostep = gfxs->Ostep;

     if (Ostep != 1) {
          w++;
          while (--w) {
               u16 s = *S;

               EXPAND( *D, s );

               S += Ostep;
               ++D;
          }

          return;
     }

     if ((long) S & 2) {
          u16 s = *S++;

          EXPAND( *D, s );

          ++D;
          --w;
     }

     l = (w >> 1) + 1;
     while (--l) {
          u32 s = *(u32*) S;

#ifdef WORDS_BIGENDIAN
          EXPAND( D[0], s >> 16 );
          EXPAND( D[1], s );
#else
          EXPAND( D[0], s );
          EXPAND( D[1], s >> 16 );
#endif

          S += 2;
          D += 2;
     }

     if (w & 1) {
          u16 s = *S;

          EXPAND( *D, s );
     }
}

/**********************************************************************************************************************
 ********************************* Sop_PFI_Kto_Dacc *******************************************************************
 **********************************************************************************************************************/

static void
Sop_PFI_OP_Dacc(Kto)( GenefxState *gfxs )
{
     int                w     = gfxs->length + 1;
     u16               *S     = gfxs->Sop[0];
     GenefxAccumulator *D     = gfxs->Dacc;
     u16                Skey  = gfxs->Skey;
     int                Ostep = gfxs->Ostep;

     while (--w) {
          u16 s = *S;

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
     u16               *S     = gfxs->Sop[0];
     GenefxAccumulator *D     = gfxs->Dacc;
     int                SperD = gfxs->SperD;
     int                Ostep = gfxs->Ostep;

     if (Ostep != 1)
          D_UNIMPLEMENTED();

     while (--w) {
          u16 s = S[i>>16];

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
     u16               *S     = gfxs->Sop[0];
     GenefxAccumulator *D     = gfxs->Dacc;
     u16                Skey  = gfxs->Skey;
     int                SperD = gfxs->SperD;
     int                Ostep = gfxs->Ostep;

     if (Ostep != 1)
          D_UNIMPLEMENTED();

     while (--w) {
          u16 s = S[i>>16];

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
     u16               *S     = gfxs->Sop[0];
     GenefxAccumulator *D     = gfxs->Dacc;
     int                sp2   = gfxs->src_pitch / 2;
     int                SperD = gfxs->SperD;
     int                TperD = gfxs->TperD;
     int                Ostep = gfxs->Ostep;

     if (Ostep != 1)
          D_UNIMPLEMENTED();

     while (--w) {
          u16 p = S[(s>>16)+(t>>16)*sp2];

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
     u16               *S     = gfxs->Sop[0];
     GenefxAccumulator *D     = gfxs->Dacc;
     int                sp2   = gfxs->src_pitch / 2;
     u16                Skey  = gfxs->Skey;
     int                SperD = gfxs->SperD;
     int                TperD = gfxs->TperD;
     int                Ostep = gfxs->Ostep;

     if (Ostep != 1)
          D_UNIMPLEMENTED();

     while (--w) {
          u16 p = S[(s>>16)+(t>>16)*sp2];

          if ((p & RGB_MASK) != Skey)
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
     int                l;
     int                w     = gfxs->length;
     GenefxAccumulator *S     = gfxs->Sacc;
     u16               *D     = gfxs->Aop[0];
     int                Dstep = gfxs->Astep;

     if (Dstep != 1) {
          w++;
          while (--w) {
               if (!(S->RGB.a & 0xf000))
                    *D = PIXEL( *S );

               ++S;
               D += Dstep;
          }

          return;
     }

     if ((long) D & 2) {
          if (!(S->RGB.a & 0xf000))
               *D = PIXEL( *S );

          ++S;
          ++D;
          --w;
     }

     l = (w >> 1) + 1;
     while (--l) {
          u32 *D2 = (u32*) D;

          if (!(S[0].RGB.a & 0xf000) && !(S[1].RGB.a & 0xf000)) {
#ifdef WORDS_BIGENDIAN
               *D2 = PIXEL( S[1] ) | PIXEL( S[0] ) << 16;
#else
               *D2 = PIXEL( S[0] ) | PIXEL( S[1] ) << 16;
#endif
          }
          else {
               if (!(S[0].RGB.a & 0xf000))
                    D[0] = PIXEL( S[0] );
               else if (!(S[1].RGB.a & 0xf000))
                    D[1] = PIXEL( S[1] );
          }

          S += 2;
          D += 2;
     }

     if (w & 1) {
          if (!(S->RGB.a & 0xf000))
               *D = PIXEL( *S );
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
     u16               *D     = gfxs->Aop[0];
     u16                Dkey  = gfxs->Dkey;
     int                Dstep = gfxs->Astep;

     while (--w) {
          if (!(S->RGB.a & 0xf000) && MASK_RGB( *D ) == Dkey)
               *D = PIXEL( *S );

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
     int                l;
     int                i     = gfxs->Xphase;
     int                w     = gfxs->length;
     GenefxAccumulator *S     = gfxs->Sacc;
     u16               *D     = gfxs->Aop[0];
     int                Dstep = gfxs->Astep;
     int                SperD = gfxs->SperD;

     if (Dstep != 1) {
         w++;
         while (--w) {
               GenefxAccumulator *S0 = &S[i>>16];

               if (!(S0->RGB.a & 0xf000))
                    *D = PIXEL( *S0 );

               D += Dstep;
               i += SperD;
         }

         return;
     }

     if ((long) D & 2) {
          if (!(S->RGB.a & 0xf000))
               *D = PIXEL( *S );

          ++D;
          --w;
          i += SperD;
     }

     l = (w >> 1) + 1;
     while (--l) {
          GenefxAccumulator *S0 = &S[i>>16];
          GenefxAccumulator *S1 = &S[(i+SperD)>>16];
          u32               *D2 = (u32*) D;

          if (!(S0->RGB.a & 0xf000) && !(S1->RGB.a & 0xf000)) {
#ifdef WORDS_BIGENDIAN
               *D2 = PIXEL( *S1 ) | PIXEL( *S0 ) << 16;
#else
               *D2 = PIXEL( *S0 ) | PIXEL( *S1 ) << 16;
#endif
          }
          else {
               if (!(S0->RGB.a & 0xf000))
                    D[0] = PIXEL( *S0 );
               else if (!(S1->RGB.a & 0xf000))
                    D[1] = PIXEL( *S1 );
          }

          D += 2;
          i += SperD << 1;
     }

     if (w & 1) {
          GenefxAccumulator *S0 = &S[i>>16];

          if (!(S0->RGB.a & 0xf000))
               *D = PIXEL( *S0 );
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
     u16               *D     = gfxs->Aop[0];
     u16                Dkey  = gfxs->Dkey;
     int                Dstep = gfxs->Astep;
     int                SperD = gfxs->SperD;

     while (--w) {
          GenefxAccumulator *S0 = &S[i>>16];

          if (!(S0->RGB.a & 0xf000) && MASK_RGB( *D ) == Dkey)
               *D = PIXEL( *S0 );

          D += Dstep;
          i += SperD;
     }
}

/**********************************************************************************************************************/

#undef RGB_MASK
#undef MASK_RGB
#undef PIXEL
#undef EXPAND

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

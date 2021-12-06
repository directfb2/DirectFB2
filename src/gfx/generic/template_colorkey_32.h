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

/**********************************************************************************************************************
 ********************************* Cop_toK_Aop_PFI ********************************************************************
 **********************************************************************************************************************/

static void
Cop_OP_Aop_PFI(toK)( GenefxState *gfxs )
{
     int  w    = gfxs->length + 1;
     u32 *D    = gfxs->Aop[0];
     u32  Cop  = gfxs->Cop;
     u32  Dkey = gfxs->Dkey;

     while (--w) {
          if ((*D & RGB_MASK) == Dkey)
               *D = Cop;

          ++D;
     }
}

/**********************************************************************************************************************
 ********************************* Bop_PFI_toK_Aop_PFI ****************************************************************
 **********************************************************************************************************************/

static void
Bop_PFI_OP_Aop_PFI(toK)( GenefxState *gfxs )
{
     int  w     = gfxs->length + 1;
     u32 *S     = gfxs->Bop[0];
     u32 *D     = gfxs->Aop[0];
     u32  Dkey  = gfxs->Dkey;
     int  Sstep = gfxs->Bstep;
     int  Dstep = gfxs->Astep;

     if (Sstep < 0) {
          S +=  gfxs->length - 1;
          D += (gfxs->length - 1) * Dstep;
     }

     while (--w) {
          if ((*D & RGB_MASK) == Dkey)
               *D = *S;

          S += Sstep;
          D += Dstep;
     }
}


/**********************************************************************************************************************
 ********************************* Bop_PFI_Kto_Aop_PFI ****************************************************************
 **********************************************************************************************************************/

static void
Bop_PFI_OP_Aop_PFI(Kto)( GenefxState *gfxs )
{
     int  w     = gfxs->length + 1;
     u32 *S     = gfxs->Bop[0];
     u32 *D     = gfxs->Aop[0];
     u32  Skey  = gfxs->Skey;
     int  Sstep = gfxs->Bstep;
     int  Dstep = gfxs->Astep;

     if (Sstep < 0) {
          S +=  gfxs->length - 1;
          D += (gfxs->length - 1) * Dstep;
     }

     while (--w) {
          u32 s = *S;

          if ((s & RGB_MASK) != Skey)
               *D = s;

          S += Sstep;
          D += Dstep;
     }
}

/**********************************************************************************************************************
 ********************************* Bop_PFI_KtoK_Aop_PFI ***************************************************************
 **********************************************************************************************************************/

static void
Bop_PFI_OP_Aop_PFI(KtoK)( GenefxState *gfxs )
{
     int  w     = gfxs->length + 1;
     u32 *S     = gfxs->Bop[0];
     u32 *D     = gfxs->Aop[0];
     u32  Skey  = gfxs->Skey;
     u32  Dkey  = gfxs->Dkey;
     int  Sstep = gfxs->Bstep;
     int  Dstep = gfxs->Astep;

     if (Sstep < 0) {
          S +=  gfxs->length - 1;
          D += (gfxs->length - 1) * Dstep;
     }

     while (--w) {
          u32 s = *S;

          if ((s & RGB_MASK) != Skey && (*D & RGB_MASK) == Dkey)
               *D = s;

          S += Sstep;
          D += Dstep;
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
     u32 *S     = gfxs->Bop[0];
     u32 *D     = gfxs->Aop[0];
     u32  Skey  = gfxs->Skey;
     int  Dstep = gfxs->Astep;
     int  SperD = gfxs->SperD;

     while (--w) {
          u32 s = S[i>>16];

          if ((s & RGB_MASK) != Skey)
               *D = s;

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
     u32 *S     = gfxs->Bop[0];
     u32 *D     = gfxs->Aop[0];
     u32  Dkey  = gfxs->Dkey;
     int  Dstep = gfxs->Astep;
     int  SperD = gfxs->SperD;

     while (--w) {
          if ((*D & RGB_MASK) == Dkey)
               *D = S[i>>16];

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
     u32 *S     = gfxs->Bop[0];
     u32 *D     = gfxs->Aop[0];
     u32  Skey  = gfxs->Skey;
     u32  Dkey  = gfxs->Dkey;
     int  Dstep = gfxs->Astep;
     int  SperD = gfxs->SperD;

     while (--w) {
          u32 s = S[i>>16];

          if ((s & RGB_MASK) != Skey && (*D & RGB_MASK) == Dkey)
               *D = s;

          D += Dstep;
          i += SperD;
     }
}

/**********************************************************************************************************************/

#undef RGB_MASK
#undef Cop_OP_Aop_PFI
#undef Bop_PFI_OP_Aop_PFI

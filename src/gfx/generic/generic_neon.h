/*
   Copyright (c) 2011 ARM Ltd. All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
   
   Author: Kui Zheng <kui.zheng@arm.com>

*/

#include <gfx/convert.h>

#define RGB32_TO_RGB16(pixel)  ( (((pixel) & 0xF80000) >> 8) | \
                                 (((pixel) & 0x00FC00) >> 5) | \
                                 (((pixel) & 0x0000F8) >> 3) )

#define RGB16_EXPAND_5to8(v)   (((v) << 3) | ((v) >> 2))
#define RGB16_EXPAND_6to8(v)   (((v) << 2) | ((v) >> 4))

#define RGB16_EXPAND_Ato8( a ) 0xFF
#define RGB16_EXPAND_Rto8( r ) RGB16_EXPAND_5to8( r )
#define RGB16_EXPAND_Gto8( g ) RGB16_EXPAND_6to8( g )
#define RGB16_EXPAND_Bto8( b ) RGB16_EXPAND_5to8( b )
#define RGB16_A_SHIFT 0
#define RGB16_R_SHIFT 11
#define RGB16_G_SHIFT 5
#define RGB16_B_SHIFT 0
#define RGB16_A_MASK 0
#define RGB16_R_MASK 0xf800
#define RGB16_G_MASK 0x07e0
#define RGB16_B_MASK 0x001f

#define RGB16_EXPAND( d, s ) do { \
     (d).RGB.a = RGB16_EXPAND_Ato8( (s & RGB16_A_MASK) >> RGB16_A_SHIFT ); \
     (d).RGB.r = RGB16_EXPAND_Rto8( (s & RGB16_R_MASK) >> RGB16_R_SHIFT ); \
     (d).RGB.g = RGB16_EXPAND_Gto8( (s & RGB16_G_MASK) >> RGB16_G_SHIFT ); \
     (d).RGB.b = RGB16_EXPAND_Bto8( (s & RGB16_B_MASK) >> RGB16_B_SHIFT ); \
} while (0)

/*
 * NEON verion of Sop_rgb16_to_Dacc. 
 */
static void Sop_rgb16_to_Dacc_NEON( GenefxState *gfxs )
{
     int	       l = gfxs->length;
     u16               *S = gfxs->Sop[0];
     GenefxAccumulator *D = gfxs->Dacc;
     int               Ostep = gfxs->Ostep;
     unsigned int      loop = l >> 3;
     unsigned int      single = l & 0x7;
     u16               s;

     if (Ostep != 1) {
          while (l--) {
               s = *S;
               RGB16_EXPAND( *D, s );
               S += Ostep;
               D++;
          }
          return;
     }
     while (single) {
          s = *S;
          RGB16_EXPAND( *D, s );
          S++;
          D++;
	  single--;	       
     }
     if (loop) {
          __asm__ __volatile__ (
               "1:                              \n\t"
               "pld          [%[S], #0xC0]      \n\t"
               "vld1.16      {q0}, [%[S]]!      \n\t"
               "vmov.i16     q4, #0x00FF        \n\t"
               "vshr.u16     q3, q0, #8         \n\t"
               "vsri.u8      q3, q3, #5         \n\t"

               "vshl.u16     q2, q0, #5         \n\t"
               "vshl.u16     q1, q0, #11 	\n\t"
               "vshr.u16     q2, q2, #8         \n\t"
               "vshr.u16     q1, q1, #8         \n\t"
               "vsri.u8      q2, q2, #6         \n\t"
               "vsri.u8      q1, q1, #5         \n\t"

               "vst4.16      {d2, d4, d6, d8}, [%[D]]!     \n\t"
               "vst4.16      {d3, d5, d7, d9}, [%[D]]!     \n\t"
               "subs         %[loop], %[loop], #1    	   \n\t"
               "bne          1b           	    "
               :
               : [S] "r" (S), [D] "r" (D), [loop] "r" (loop)
               : "memory", "d0", "d1", "d2", "d3", "d4", 
	  	 "d5", "d6", "d7", "d8", "d9"
          );
     }

}

/*
 * NEON verion of Sop_argb_to_Dacc. 
 */
static void Sop_argb_to_Dacc_NEON ( GenefxState *gfxs )
{
     __asm__ __volatile__ (
          "mov          r5, %[len]      \n\t"
          "cmp          %[Ostep], #1    \n\t"
          /* Ostep != 1 */
          "bne          1f              \n\t"

          "lsrs         r4, %[len], #3  \n\t"
          /* Ostep = 1 && len < 8 */
          "beq          1f              \n\t"

          /* Ostep = 1 && len >= 8 */
          "2:                           \n\t"
          "pld          [%[Sop], #0xC0] \n\t"
          "vld4.8       {d0, d1, d2, d3}, [%[Sop]]!     \n\t"
          "vmovl.u8     q2, d0          \n\t"
          "vmovl.u8     q3, d1          \n\t"
          "vmovl.u8     q4, d2          \n\t"
          "vmovl.u8     q5, d3          \n\t"
          "vst4.16      {d4, d6, d8, d10}, [%[Dacc]]!   \n\t"
          "vst4.16      {d5, d7, d9, d11}, [%[Dacc]]!   \n\t"
          "subs         r4, r4, #1      \n\t"
          "bne          2b              \n\t"

          "ands         r5, %[len], #7  \n\t"
          /* Ostep = 1 && len = 8 */
          "beq          5f              \n\t"

          "1:                           \n\t"
          "lsl          r6, %[Ostep], #2        \n\t"
          "vmov.i32     d0, #0          \n\t"
          "vmov         d1, d0          \n\t"
          "vmov         d2, d0          \n\t"
          "vmov         d3, d0          \n\t"
          "lsrs         r4, r5, #2      \n\t"
          /* len < 4 */
          "beq          4f              \n\t"

          /* len >= 4 */
          "3:                           \n\t"
          "pld          [%[Sop], #0xC0] \n\t"
          "vld4.8       {d0[0], d1[0], d2[0], d3[0]}, [%[Sop]],r6     \n\t"
          "vld4.8       {d0[2], d1[2], d2[2], d3[2]}, [%[Sop]],r6     \n\t"
          "vld4.8       {d0[4], d1[4], d2[4], d3[4]}, [%[Sop]],r6     \n\t"
          "vld4.8       {d0[6], d1[6], d2[6], d3[6]}, [%[Sop]],r6     \n\t"
          "vst4.16      {d0, d1, d2, d3}, [%[Dacc]]!   \n\t"
          "subs         r4, r4, #1      \n\t"
          "bne          3b              \n\t"

          "ands         r5, %[len], #3  \n\t"
          /* len = 4 */
          "beq          5f              \n\t"

          "4:                           \n\t"
          "vld4.8       {d0[0], d1[0], d2[0], d3[0]}, [%[Sop]],r6     \n\t"
          "vst4.16      {d0[0], d1[0], d2[0], d3[0]}, [%[Dacc]]!      \n\t"
          "subs         r5, r5, #1      \n\t"
          "bne          4b              \n\t"

          "5:                               "
          :
          : [Sop] "r" (gfxs->Sop[0]), [Dacc] "r" (gfxs->Dacc),
            [len] "r" (gfxs->length), [Ostep] "r" (gfxs->Ostep)
          : "memory", "r4", "r5", "r6", "d0", "d1", "d2", "d3",
            "d4", "d5", "d6", "d7", "d8", "d9", "d10", "d11"
          );
}

/*
 * NEON verion of Bop_rgb32_to_Aop_rgb16_LE. 
 */
static void Bop_rgb32_to_Aop_rgb16_LE_NEON( GenefxState *gfxs )
{
     int  w = gfxs->length;
     u32  *S = gfxs->Bop[0];
     u32  *D = gfxs->Aop[0];

     if ((unsigned long)D & 2) {
          u16 *d = (u16*)D;
          d[0] = RGB32_TO_RGB16( S[0] );
          w--;
          S++;
          D = (u32*)(d+1);
     }

     __asm__ __volatile__ (
          "lsrs         r4, %[len], #3  \n\t"
          "beq          2f              \n\t"

          "1:                           \n\t"
          "subs         r4, r4, #1      \n\t"
          "pld          [%[S], #0xC0]   \n\t"
          "vld4.8       {d0, d1, d2, d3}, [%[S]]! \n\t"
          "vshll.u8     q2, d2, #8      \n\t"
          "vshll.u8     q3, d1, #8      \n\t"
          "vsri.16      q2, q3, #5      \n\t"
          "vshll.u8     q3, d0, #8      \n\t"
          "vsri.16      q2, q3, #11     \n\t"
          "vst1.16      {q2}, [%[D]]!   \n\t"
          "bne          1b              \n\t"

          "2:                           \n\t"
          "ands         r5, %[len], #7  \n\t"
          "beq          5f              \n\t"
          "cmp          r5, #4          \n\t"
          "blt          4f              \n\t"

          "3:                           \n\t"
          "vld1.32      {d0, d1}, [%[S]]!       \n\t"
          "vshr.u32     q1, q0, #19     \n\t"
          "vshr.u16     q2, q0, #10     \n\t"
          "vshl.u16     q3, q0, #8      \n\t"
          "vshl.u16     q1, q1, #11     \n\t"
          "vshl.u16     q2, q2, #5      \n\t"
          "vshr.u16     q3, q3, #11     \n\t"
          "vorr         q1, q1, q2      \n\t"
          "vorr         q1, q1, q3      \n\t"
          "vmovn.i32    d4, q1          \n\t"
          "vst1.16      {d4}, [%[D]]!   \n\t"

          "ands         r5, %[len], #3  \n\t"
          "beq          5f              \n\t"

          "4:                           \n\t"
          "vld1.32      {d0[0]}, [%[S]]!\n\t"
          "vshr.u32     d1, d0, #19     \n\t"
          "vshr.u16     d2, d0, #10     \n\t"
          "vshl.u16     d3, d0, #8      \n\t"
          "vshl.u16     d1, d1, #11     \n\t"
          "vshl.u16     d2, d2, #5      \n\t"
          "vshr.u16     d3, d3, #11     \n\t"
          "vorr         d1, d1, d2      \n\t"
          "vorr         d1, d1, d3      \n\t"
          "vst1.16      {d1[0]}, [%[D]]!\n\t"
          "subs         r5, r5, #1      \n\t"
          "bne          4b              \n\t"
          "5:                               "
          :
          : [S] "r" (S), [len] "r" (w), [D] "r" (D)
          : "memory", "r4", "r5", "d0", "d1", "d2",
            "d3", "d4", "d5", "d6", "d7"
          );
}

/*
 * NEON verion of SCacc_add_to_Dacc. 
 */
static void SCacc_add_to_Dacc_NEON ( GenefxState *gfxs )
{
     int               w = gfxs->length;
     GenefxAccumulator *D = gfxs->Dacc;
     GenefxAccumulator SCacc = gfxs->SCacc;
     unsigned int      loop = w >> 3;
     unsigned int      single = w & 0x7;
     u16               maska = 0xf000;

     while (single) {
          if (!(D->RGB.a & 0xF000)) {
               D->RGB.a += SCacc.RGB.a;
               D->RGB.r += SCacc.RGB.r;
               D->RGB.g += SCacc.RGB.g;
               D->RGB.b += SCacc.RGB.b;
          }
          D++;
          single--;
     }
     if (loop) {
          __asm__ __volatile__ (
               "mov          r4, %[D]           \n\t"
               "mov          r5, %[D]           \n\t"
               "vdup.16      q3, %[SCacc_a]     \n\t"
               "vdup.16      q2, %[SCacc_r]     \n\t"
               "vdup.16      q1, %[SCacc_g]     \n\t"
               "vdup.16      q0, %[SCacc_b]     \n\t"
               "vdup.16      q8, %[maska]       \n\t"
               "1:                              \n\t"
               "pld          [r4, #0xC0]        \n\t"
               "pld          [r4, #0x100]       \n\t"
               "vld4.16      {d8, d10, d12, d14}, [r4]! \n\t"
               "vld4.16      {d9, d11, d13, d15}, [r4]! \n\t"
               "vand         q9, q7, q8         \n\t"
               "vceq.i16     q9, q9, #0         \n\t"
               "vand         q10, q9, q0        \n\t"
               "vand         q11, q9, q1        \n\t"
               "vand         q12, q9, q2        \n\t"
               "vand         q13, q9, q3        \n\t"
               "vadd.i16     q4, q4, q10        \n\t"
               "vadd.i16     q5, q5, q11        \n\t"
               "vadd.i16     q6, q6, q12        \n\t"
               "vadd.i16     q7, q7, q13        \n\t"
               "vst4.16      {d8, d10, d12, d14}, [r5]! \n\t"
               "vst4.16      {d9, d11, d13, d15}, [r5]! \n\t"
               "subs         %[loop], %[loop], #1       \n\t"
               "bne          1b                 "
               :
               : [SCacc_a] "r" (gfxs->SCacc.RGB.a), [SCacc_r] "r" (gfxs->SCacc.RGB.r),
                 [SCacc_g] "r" (gfxs->SCacc.RGB.g), [SCacc_b] "r" (gfxs->SCacc.RGB.b),
                 [D] "r" (D), [maska] "r" (maska), [loop] "r" (loop)
               : "memory", "r4", "r5", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7",
                 "d8", "d9", "d10", "d11", "d12", "d13", "d14", "d15", "d16", "d17",
                 "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d25", "d26", "d27"
          );
     }
}

/*
 * NEON verion of Sacc_add_to_Dacc. 
 */
static void Sacc_add_to_Dacc_NEON ( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator  *D = gfxs->Dacc;
     GenefxAccumulator  *S = gfxs->Sacc;
     unsigned int       loop = w >> 3;
     unsigned int       single = w & 0x7;
     u16                maska = 0xf000;

     while (single) {
          if (!(D->RGB.a & 0xF000)) {
               D->RGB.a += S->RGB.a;
               D->RGB.r += S->RGB.r;
               D->RGB.g += S->RGB.g;
               D->RGB.b += S->RGB.b;
          }
          D++;
          S++;
          single--;
     }
     if (loop) {
          __asm__ __volatile__ (
               "mov          r4, %[D]           \n\t"
               "mov          r5, %[S]           \n\t"
               "mov          r6, %[D]           \n\t"
               "vdup.16      q8, %[maska]       \n\t"
               "1:                              \n\t"
               "pld          [r4, #0xC0]        \n\t"
               "pld          [r4, #0x100]       \n\t"
               "vld4.16      {d8, d10, d12, d14}, [r4]! \n\t"
               "vld4.16      {d9, d11, d13, d15}, [r4]! \n\t"
               "pld          [r5, #0xC0]        \n\t"
               "pld          [r5, #0x100]       \n\t"
               "vld4.16      {d0, d2, d4, d6}, [r5]! \n\t"
               "vld4.16      {d1, d3, d5, d7}, [r5]! \n\t"

               "vand         q9, q7, q8         \n\t"
               "vceq.i16     q9, q9, #0         \n\t"

               "vand         q0, q9, q0         \n\t"
               "vand         q1, q9, q1         \n\t"
               "vand         q2, q9, q2         \n\t"
               "vand         q3, q9, q3         \n\t"
               "vadd.i16     q4, q4, q0         \n\t"
               "vadd.i16     q5, q5, q1         \n\t"
               "vadd.i16     q6, q6, q2         \n\t"
               "vadd.i16     q7, q7, q3         \n\t"
               "vst4.16      {d8, d10, d12, d14}, [r6]! \n\t"
               "vst4.16      {d9, d11, d13, d15}, [r6]! \n\t"
               "subs         %[loop], %[loop], #1       \n\t"
               "bne          1b                     "
               :
               : [S] "r" (S), [D] "r" (D), [maska] "r" (maska), [loop] "r" (loop)
               : "memory", "r4", "r5", "r6", "d0", "d1", "d2", "d3", "d4", "d5",
                 "d6", "d7", "d8", "d9", "d10", "d11", "d12", "d13", "d14", "d15",
                 "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24",
                 "d25", "d26", "d27"
          );
     }
}

/*
 * NEON version of Sacc_to_Aop_rgb16
 */
static void Sacc_to_Aop_rgb16_NEON ( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator  *S = gfxs->Sacc;
     u16                *D = gfxs->Aop[0];
     int                Dstep = gfxs->Astep;
     unsigned int       loop;
     unsigned int       single;
     u32                mask = 0xFF00FF00;
     u32                color = 0x00FF00FF;
     int                i, j;

#define PIXEL_OUT( a, r, g, b ) PIXEL_RGB16( r, g, b )

#define PIXEL( x ) PIXEL_OUT( ((x).RGB.a & 0xFF00) ? 0xFF : (x).RGB.a, \
                              ((x).RGB.r & 0xFF00) ? 0xFF : (x).RGB.r, \
                              ((x).RGB.g & 0xFF00) ? 0xFF : (x).RGB.g, \
                              ((x).RGB.b & 0xFF00) ? 0xFF : (x).RGB.b )

     if (Dstep != 1) {
          while (w--) {
              if (!(S->RGB.a & 0xF000))
                   *D = PIXEL( *S );

              S++;
              D += Dstep;
          }
          return;
     }

     if ((long)D & 2) {
          if (!(S->RGB.a & 0xF000))
               *D = PIXEL( *S );
          S++;
          D++;
          w--;
     }

     loop = w >> 3;
     single = w & 0x7;
     while(loop){
          for (i=0; i<8; i++) {
               if ((S[i].RGB.a & 0xF000)) {
                    break;
               }
          }
          if (i < 8) {
               for (j=0; j<i; j++)
                    D[j] = PIXEL( S[j] );
               for (j=i+1; j<8; j++) {
                    if (!(S[j].RGB.a & 0xF000))
                         D[j] = PIXEL( S[j] );
               }
          } else {
               __asm__ __volatile__ (
                    "mov        r4, %[S]        \n\t"
                    "pld        [r4, #0xC0]     \n\t"
                    "pld        [r4, #0x100]    \n\t"
                    "vld4.16    {d0, d2, d4, d6}, [r4]! \n\t"
                    "vld4.16    {d1, d3, d5, d7}, [r4]  \n\t"
                    "vdup.32    q3, %[mask]     \n\t"
                    "vdup.32    q4, %[color]    \n\t"
                    /* b */
                    "vtst.16    q5, q0, q3      \n\t"
                    "vceq.u16   q6, q5, #0      \n\t"
                    "vand.16    q5, q4, q5      \n\t"
                    "vand.16    q6, q0, q6      \n\t"
                    "vorr.16    q0, q5, q6      \n\t"
                    "vshr.u16   q0, q0, #3      \n\t"
                    /* g */
                    "vtst.16    q5, q1, q3      \n\t"
                    "vceq.u16   q6, q5, #0      \n\t"
                    "vand.16    q5, q4, q5      \n\t"
                    "vand.16    q6, q1, q6      \n\t"
                    "vorr.16    q1, q5, q6      \n\t"
                    "vshr.u16   q1, q1, #2      \n\t"
                    "vshl.u16   q1, q1, #5      \n\t"
                    /* r */
                    "vtst.16    q5, q2, q3      \n\t"
                    "vceq.u16   q6, q5, #0      \n\t"
                    "vand.16    q5, q4, q5      \n\t"
                    "vand.16    q6, q2, q6      \n\t"
                    "vorr.16    q2, q5, q6      \n\t"
                    "vshr.u16   q2, q2, #3      \n\t"
                    "vshl.u16   q2, q2, #11     \n\t"
                    /* rgb */
                    "vorr.16    q1, q1, q2      \n\t"
                    "vorr.16    q0, q0, q1      \n\t"
                    "vst1.16    {d0, d1}, [%[D]]     "
                    :
                    : [D] "r" (D), [S] "r" (S), [mask] "r" (mask), [color] "r" (color)
                    : "memory", "r4", "d0", "d1", "d2", "d3", "d4","d5", "d6",
                      "d7", "d8", "d9", "d10", "d11", "d12", "d13"
               );
               }//else
                    S += 8;
                    D += 8;
                    loop--;
          }// end of loop

     while (single){
          if (!(S->RGB.a & 0xF000)) {
               *D = PIXEL( *S );
          }
          S++;
          D++;
          single--;
     }
}


static void Xacc_blend_invsrcalpha_Sacc1_NEON ( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator  *X = gfxs->Xacc;
     GenefxAccumulator  *Y = gfxs->Yacc;
     GenefxAccumulator  *S = gfxs->Sacc;
     unsigned int       loop = w >> 3;
     unsigned int       single = w & 0x7;
     u16                maska = 0xF000;
     u16                inv = 0x100;
          
     if (loop){
          __asm__ __volatile__ (
               "vdup.16         q8, %[maska]       \n\t"
               "vdup.16         q9, %[inv]         \n\t"
               "1:                                 \n\t"
               "pld             [%[Y], #0xC0]      \n\t"
               "pld             [%[Y], #0x100]     \n\t"
               "pld             [%[S], #0xC0]      \n\t"
               "pld             [%[S], #0x100]     \n\t"
               /* vload Y, q0:b, q1:g, q2:r, q3:a */
               "vld4.16         {d0, d2, d4, d6}, [%[Y]]!  \n\t"
               "vld4.16         {d1, d3, d5, d7}, [%[Y]]!  \n\t"
               /* vload S, q4:b, q5:g, q6:r, q7:a */
               "vld4.16         {d8, d10, d12, d14}, [%[S]]! \n\t"
               "vld4.16         {d9, d11, d13, d15}, [%[S]]! \n\t"
               /* q4: q3, maska */
               "vand            q4, q3, q8         \n\t"
               "vceq.i16        q4, q4, #0         \n\t"
               /* Sa:q5, inv, q7 */
               "vsub.i16        q5, q9, q7         \n\t"
               /* b:q10, q5, q0 */
               "vmull.u16       q6, d10, d0        \n\t"
               "vshrn.i32       d20, q6, #8        \n\t"
               "vmull.u16       q6, d11, d1        \n\t"
               "vshrn.i32       d21, q6, #8        \n\t"
               /* g:q11, q5, q1 */
               "vmull.u16       q6, d10, d2        \n\t"
               "vshrn.i32       d22, q6, #8        \n\t"
               "vmull.u16       q6, d11, d3        \n\t"
               "vshrn.i32       d23, q6, #8        \n\t"
               /* r:q12, q5, q2 */
               "vmull.u16       q6, d10, d4        \n\t"
               "vshrn.i32       d24, q6, #8        \n\t"
               "vmull.u16       q6, d11, d5        \n\t"
               "vshrn.i32       d25, q6, #8        \n\t"
               /* a:q13, q5, q3 */
               "vmull.u16       q6, d10, d6        \n\t"
               "vshrn.i32       d26, q6, #8        \n\t"
               "vmull.u16       q6, d11, d7        \n\t"
               "vshrn.i32       d27, q6, #8        \n\t"
               /* if (!(Y->RGB.a & 0xF000)) */
               "vand            q10, q4, q10       \n\t"
               "vand            q11, q4, q11       \n\t"
               "vand            q12, q4, q12       \n\t"
               "vand            q13, q4, q13       \n\t"
               /* if ((Y->RGB.a & 0xF000)) */
               "vceq.i16        q4, q4, #0         \n\t"
               "vand            q0, q4, q0         \n\t"
               "vand            q1, q4, q1         \n\t"
               "vand            q2, q4, q2         \n\t"
               "vand            q3, q4, q3         \n\t"
               /* X: q0(b), q1(g), q2(r), q3(a) */
               "vorr            q0, q0, q10        \n\t"
               "vorr            q1, q1, q11        \n\t"
               "vorr            q2, q2, q12        \n\t"
               "vorr            q3, q3, q13        \n\t"
               "vst4.16         {d0, d2, d4, d6}, [%[X]]!  \n\t"
               "vst4.16         {d1, d3, d5, d7}, [%[X]]!  \n\t"
               "subs            %[loop], %[loop], #1       \n\t"
               "bne             1b                     "
               :
               : [X] "r" (gfxs->Xacc), [Y] "r" (gfxs->Yacc), [S] "r" (gfxs->Sacc),
                 [maska] "r" (maska), [inv] "r" (inv), [loop] "r" (loop)
               : "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "d8", "d9",
                 "d10", "d11", "d12", "d13", "d14", "d15", "d16", "d17", "d18", "d19",
                 "d20", "d21", "d22", "d23", "d24", "d25", "d26", "d27"
               );
     }
     while (single){
          if (!(Y->RGB.a & 0xF000)) {
               register u16 Sa = 0x100 - S->RGB.a;

               X->RGB.r = (Sa * Y->RGB.r) >> 8;
               X->RGB.g = (Sa * Y->RGB.g) >> 8;
               X->RGB.b = (Sa * Y->RGB.b) >> 8;
               X->RGB.a = (Sa * Y->RGB.a) >> 8;
          } else
               *X = *Y;

          X++;
          Y++;
          S++;
          single--;
     }
}

static void Xacc_blend_invsrcalpha_Sacc0_NEON ( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator  *X = gfxs->Xacc;
     GenefxAccumulator  *Y = gfxs->Yacc;
     unsigned int       loop = w >> 3;
     unsigned int       single = w & 0x7;
     u16                maska = 0xF000;
     u16                inv = 0x100;
     u16 		Sa = 0x100 - gfxs->color.a;
          
     if (loop){
          __asm__ __volatile__ (
               "vdup.16         q7, %[maska]       \n\t"
               "vdup.16         q5, %[Sa]          \n\t"
               "1:                                 \n\t"
               "pld             [%[Y], #0xC0]      \n\t"
               "pld             [%[Y], #0x100]     \n\t"
               /* vload Y, q0:b, q1:g, q2:r, q3:a */
               "vld4.16         {d0, d2, d4, d6}, [%[Y]]!  \n\t"
               "vld4.16         {d1, d3, d5, d7}, [%[Y]]!  \n\t"
               /* q4: q3, maska */
               "vand            q4, q3, q7         \n\t"
               "vceq.i16        q4, q4, #0         \n\t"
               /* b:q10, q5, q0 */
               "vmull.u16       q6, d10, d0        \n\t"
               "vshrn.i32       d20, q6, #8        \n\t"
               "vmull.u16       q6, d11, d1        \n\t"
               "vshrn.i32       d21, q6, #8        \n\t"
               /* g:q11, q5, q1 */
               "vmull.u16       q6, d10, d2        \n\t"
               "vshrn.i32       d22, q6, #8        \n\t"
               "vmull.u16       q6, d11, d3        \n\t"
               "vshrn.i32       d23, q6, #8        \n\t"
               /* r:q12, q5, q2 */
               "vmull.u16       q6, d10, d4        \n\t"
               "vshrn.i32       d24, q6, #8        \n\t"
               "vmull.u16       q6, d11, d5        \n\t"
               "vshrn.i32       d25, q6, #8        \n\t"
               /* a:q13, q5, q3 */
               "vmull.u16       q6, d10, d6        \n\t"
               "vshrn.i32       d26, q6, #8        \n\t"
               "vmull.u16       q6, d11, d7        \n\t"
               "vshrn.i32       d27, q6, #8        \n\t"
               /* if (!(Y->RGB.a & 0xF000)) */
               "vand            q10, q4, q10       \n\t"
               "vand            q11, q4, q11       \n\t"
               "vand            q12, q4, q12       \n\t"
               "vand            q13, q4, q13       \n\t"
               /* if ((Y->RGB.a & 0xF000)) */
               "vceq.i16        q4, q4, #0         \n\t"
               "vand            q0, q4, q0         \n\t"
               "vand            q1, q4, q1         \n\t"
               "vand            q2, q4, q2         \n\t"
               "vand            q3, q4, q3         \n\t"
               /* X: q0(b), q1(g), q2(r), q3(a) */
               "vorr            q0, q0, q10        \n\t"
               "vorr            q1, q1, q11        \n\t"
               "vorr            q2, q2, q12        \n\t"
               "vorr            q3, q3, q13        \n\t"
               "vst4.16         {d0, d2, d4, d6}, [%[X]]!  \n\t"
               "vst4.16         {d1, d3, d5, d7}, [%[X]]!  \n\t"
               "subs            %[loop], %[loop], #1       \n\t"
               "bne             1b                 "
               :
               : [X] "r" (gfxs->Xacc), [Y] "r" (gfxs->Yacc), [Sa] "r" (Sa),
                 [maska] "r" (maska), [inv] "r" (inv), [loop] "r" (loop)
               : "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "d8", "d9",
                 "d10", "d11", "d12", "d13", "d14", "d15", "d20", "d21", "d22", "d23",
                 "d24", "d25", "d26", "d27"
               );
     }
     while (single){
          if (!(Y->RGB.a & 0xF000)) {
               X->RGB.r = (Sa * Y->RGB.r) >> 8;
               X->RGB.g = (Sa * Y->RGB.g) >> 8;
               X->RGB.b = (Sa * Y->RGB.b) >> 8;
               X->RGB.a = (Sa * Y->RGB.a) >> 8;
          } else
               *X = *Y;

          X++;
          Y++;
          single--;
     }
}

/*
 * NEON verion of Xacc_blend_invsrcalpha. 
 */
static void Xacc_blend_invsrcalpha_NEON ( GenefxState *gfxs )
{
     if (gfxs->Sacc) 
          Xacc_blend_invsrcalpha_Sacc1_NEON ( gfxs );
     else
          Xacc_blend_invsrcalpha_Sacc0_NEON ( gfxs );
}


static void Xacc_blend_srcalpha_Sacc1_NEON( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator  *X = gfxs->Xacc;
     GenefxAccumulator  *Y = gfxs->Yacc;
     GenefxAccumulator  *S = gfxs->Sacc;
     unsigned int       loop = w >> 3;
     unsigned int       single = w & 0x7;
     u16                maska = 0xF000;
     u16                sadd = 0x1;
          
     if (loop){
          __asm__ __volatile__ (
               "vdup.16         q8, %[maska]	   \n\t"
               "vdup.16         q9, %[sadd]        \n\t"
               "1:                                 \n\t"
               "pld             [%[Y], #0xC0]      \n\t"
               "pld             [%[Y], #0x100]     \n\t"
               "pld             [%[S], #0xC0]      \n\t"
               "pld             [%[S], #0x100]     \n\t"
               /* vload Y, q0:b, q1:g, q2:r, q3:a */
               "vld4.16         {d0, d2, d4, d6}, [%[Y]]!  \n\t"
               "vld4.16         {d1, d3, d5, d7}, [%[Y]]!  \n\t"
               /* vload S, q4:b, q5:g, q6:r, q7:a */
               "vld4.16         {d8, d10, d12, d14}, [%[S]]! \n\t"
               "vld4.16         {d9, d11, d13, d15}, [%[S]]! \n\t"
               /* q4: q3, maska */
               "vand            q4, q3, q8         \n\t"
               "vceq.i16        q4, q4, #0         \n\t"
               /* Sa:q5, sadd, q7 */
               "vadd.i16        q5, q9, q7         \n\t"
               /* b:q10, q5, q0 */
               "vmull.u16       q6, d10, d0        \n\t"
               "vshrn.i32       d20, q6, #8        \n\t"
               "vmull.u16       q6, d11, d1        \n\t"
               "vshrn.i32       d21, q6, #8        \n\t"
               /* g:q11, q5, q1 */
               "vmull.u16       q6, d10, d2        \n\t"
               "vshrn.i32       d22, q6, #8        \n\t"
               "vmull.u16       q6, d11, d3        \n\t"
               "vshrn.i32       d23, q6, #8        \n\t"
               /* r:q12, q5, q2 */
               "vmull.u16       q6, d10, d4        \n\t"
               "vshrn.i32       d24, q6, #8        \n\t"
               "vmull.u16       q6, d11, d5        \n\t"
               "vshrn.i32       d25, q6, #8        \n\t"
               /* a:q13, q5, q3 */
               "vmull.u16       q6, d10, d6        \n\t"
               "vshrn.i32       d26, q6, #8        \n\t"
               "vmull.u16       q6, d11, d7        \n\t"
               "vshrn.i32       d27, q6, #8        \n\t"
               /* if (!(Y->RGB.a & 0xF000)) */
               "vand            q10, q4, q10       \n\t"
               "vand            q11, q4, q11       \n\t"
               "vand            q12, q4, q12       \n\t"
               "vand            q13, q4, q13       \n\t"
               /* if ((Y->RGB.a & 0xF000)) */
               "vceq.i16        q4, q4, #0         \n\t"
               "vand            q0, q4, q0         \n\t"
               "vand            q1, q4, q1         \n\t"
               "vand            q2, q4, q2         \n\t"
               "vand            q3, q4, q3         \n\t"
               /* X: q0(b), q1(g), q2(r), q3(a) */
               "vorr            q0, q0, q10        \n\t"
               "vorr            q1, q1, q11        \n\t"
               "vorr            q2, q2, q12        \n\t"
               "vorr            q3, q3, q13        \n\t"
               "vst4.16         {d0, d2, d4, d6}, [%[X]]!  \n\t"
               "vst4.16         {d1, d3, d5, d7}, [%[X]]!  \n\t"
               "subs            %[loop], %[loop], #1       \n\t"
               "bne             1b                     "
               :
               : [X] "r" (gfxs->Xacc), [Y] "r" (gfxs->Yacc), [S] "r" (gfxs->Sacc),
                 [maska] "r" (maska), [sadd] "r" (sadd), [loop] "r" (loop)
               : "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "d8", "d9",
                 "d10", "d11", "d12", "d13", "d14", "d15", "d16", "d17", "d18", "d19",
                 "d20", "d21", "d22", "d23", "d24", "d25", "d26", "d27"
               );
     }
     while (single){
          if (!(Y->RGB.a & 0xF000)) {
               register u16 Sa = S->RGB.a + 1;

               X->RGB.r = (Sa * Y->RGB.r) >> 8;
               X->RGB.g = (Sa * Y->RGB.g) >> 8;
               X->RGB.b = (Sa * Y->RGB.b) >> 8;
               X->RGB.a = (Sa * Y->RGB.a) >> 8;
          } else
               *X = *Y;

          X++;
          Y++;
          S++;
          single--;
     }
}

static void Xacc_blend_srcalpha_Sacc0_NEON( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator  *X = gfxs->Xacc;
     GenefxAccumulator  *Y = gfxs->Yacc;
     unsigned int       loop = w >> 3;
     unsigned int       single = w & 0x7;
     u16                maska = 0xF000;
     u16                sadd = 0x1;
     u16 		Sa = gfxs->color.a + 1;
          
     if (loop){
          __asm__ __volatile__ (
               "vdup.16         q7, %[maska]       \n\t"
               "vdup.16         q5, %[Sa]          \n\t"
               "1:                                 \n\t"
               "pld             [%[Y], #0xC0]      \n\t"
               "pld             [%[Y], #0x100]     \n\t"
               /* vload Y, q0:b, q1:g, q2:r, q3:a */
               "vld4.16         {d0, d2, d4, d6}, [%[Y]]!  \n\t"
               "vld4.16         {d1, d3, d5, d7}, [%[Y]]!  \n\t"
               /* q4: q3, maska */
               "vand            q4, q3, q7         \n\t"
               "vceq.i16        q4, q4, #0         \n\t"
               /* b:q10, q5, q0 */
               "vmull.u16       q6, d10, d0        \n\t"
               "vshrn.i32       d20, q6, #8        \n\t"
               "vmull.u16       q6, d11, d1        \n\t"
               "vshrn.i32       d21, q6, #8        \n\t"
               /* g:q11, q5, q1 */
               "vmull.u16       q6, d10, d2        \n\t"
               "vshrn.i32       d22, q6, #8        \n\t"
               "vmull.u16       q6, d11, d3        \n\t"
               "vshrn.i32       d23, q6, #8        \n\t"
               /* r:q12, q5, q2 */
               "vmull.u16       q6, d10, d4        \n\t"
               "vshrn.i32       d24, q6, #8        \n\t"
               "vmull.u16       q6, d11, d5        \n\t"
               "vshrn.i32       d25, q6, #8        \n\t"
               /* a:q13, q5, q3 */
               "vmull.u16       q6, d10, d6        \n\t"
               "vshrn.i32       d26, q6, #8        \n\t"
               "vmull.u16       q6, d11, d7        \n\t"
               "vshrn.i32       d27, q6, #8        \n\t"
               /* if (!(Y->RGB.a & 0xF000)) */
               "vand            q10, q4, q10       \n\t"
               "vand            q11, q4, q11       \n\t"
               "vand            q12, q4, q12       \n\t"
               "vand            q13, q4, q13       \n\t"
               /* if ((Y->RGB.a & 0xF000)) */
               "vceq.i16        q4, q4, #0         \n\t"
               "vand            q0, q4, q0         \n\t"
               "vand            q1, q4, q1         \n\t"
               "vand            q2, q4, q2         \n\t"
               "vand            q3, q4, q3         \n\t"
               /* X: q0(b), q1(g), q2(r), q3(a) */
               "vorr            q0, q0, q10        \n\t"
               "vorr            q1, q1, q11        \n\t"
               "vorr            q2, q2, q12        \n\t"
               "vorr            q3, q3, q13        \n\t"
               "vst4.16         {d0, d2, d4, d6}, [%[X]]!  \n\t"
               "vst4.16         {d1, d3, d5, d7}, [%[X]]!  \n\t"
               "subs            %[loop], %[loop], #1       \n\t"
               "bne             1b                     "
               :
               : [X] "r" (gfxs->Xacc), [Y] "r" (gfxs->Yacc), [Sa] "r" (Sa),
                 [maska] "r" (maska), [sadd] "r" (sadd), [loop] "r" (loop)
               : "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "d8", "d9",
                 "d10", "d11", "d12", "d13", "d14", "d15", "d20", "d21", "d22", "d23",
                 "d24", "d25", "d26", "d27"
               );
     }
     while (single){
          if (!(Y->RGB.a & 0xF000)) {
               X->RGB.r = (Sa * Y->RGB.r) >> 8;
               X->RGB.g = (Sa * Y->RGB.g) >> 8;
               X->RGB.b = (Sa * Y->RGB.b) >> 8;
               X->RGB.a = (Sa * Y->RGB.a) >> 8;
          } else
               *X = *Y;

          X++;
          Y++;
          single--;
     }
}

/*
 * NEON version of Xacc_blend_srcalpha
 */
static void Xacc_blend_srcalpha_NEON( GenefxState *gfxs )
{
     if (gfxs->Sacc) 
          Xacc_blend_srcalpha_Sacc1_NEON ( gfxs );
     else
          Xacc_blend_srcalpha_Sacc0_NEON ( gfxs );  
}

/* 
 * NEON version of Dacc_modulate_argb_NEON.
 */
static void Dacc_modulate_argb_NEON( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *D = gfxs->Dacc;
     GenefxAccumulator  Cacc = gfxs->Cacc;
     unsigned int       loop = w >> 3;
     unsigned int       single = w & 0x7;
     u16                maska = 0xF000;

     while (single){
          if (!(D->RGB.a & 0xF000)) {
               D->RGB.a = (Cacc.RGB.a * D->RGB.a) >> 8;
               D->RGB.r = (Cacc.RGB.r * D->RGB.r) >> 8;
               D->RGB.g = (Cacc.RGB.g * D->RGB.g) >> 8;
               D->RGB.b = (Cacc.RGB.b * D->RGB.b) >> 8;
          }
          D++;
          single--;
     }

     if (loop) {
          __asm__ __volatile__ (
               "mov             r4, %[D]        \n\t"
               "mov             r5, %[D]        \n\t"
               "vdup.16         q3, %[Cacc_a]   \n\t"
               "vdup.16         q2, %[Cacc_r]   \n\t"
               "vdup.16         q1, %[Cacc_g]   \n\t"
               "vdup.16         q0, %[Cacc_b]   \n\t"
               "vdup.16         q8, %[maska]    \n\t"
               "1:                              \n\t"
               "pld             [r4, #0xC0]     \n\t"
               "pld             [r4, #0x100]     \n\t"
               /* vload q4:b, q5:g, q6:r, q7:a */
               "vld4.16         {d8, d10, d12, d14}, [r4]! \n\t"
               "vld4.16         {d9, d11, d13, d15}, [r4]! \n\t"

               "vand            q9, q7, q8      \n\t"
               "vceq.i16        q9, q9, #0      \n\t"
               /* b:q10  q0, q4 */
               "vmull.u16       q14, d8, d0     \n\t"
               "vshrn.i32       d20, q14, #8    \n\t"
               "vmull.u16       q14, d9, d1     \n\t"
               "vshrn.i32       d21, q14, #8    \n\t"
               /* g:q11, q1, q5 */
               "vmull.u16       q14, d10, d2    \n\t"
               "vshrn.i32       d22, q14, #8    \n\t"
               "vmull.u16       q14, d11, d3    \n\t"
               "vshrn.i32       d23, q14, #8    \n\t"
               /* r:q12, q2, q6 */
               "vmull.u16       q14, d12, d4    \n\t"
               "vshrn.i32       d24, q14, #8    \n\t"
               "vmull.u16       q14, d13, d5    \n\t"
               "vshrn.i32       d25, q14, #8    \n\t"
               /* a:q13, q3, q7 */
               "vmull.u16       q14, d14, d6    \n\t"
               "vshrn.i32       d26, q14, #8    \n\t"
               "vmull.u16       q14, d15, d7    \n\t"
               "vshrn.i32       d27, q14, #8    \n\t"
               /* if (!(D->RGB.a & 0xF000)) */
               "vand            q10, q9, q10    \n\t"
               "vand            q11, q9, q11    \n\t"
               "vand            q12, q9, q12    \n\t"
               "vand            q13, q9, q13    \n\t"
               /* if ((D->RGB.a & 0xF000)) */
               "vceq.i16        q9, q9, #0      \n\t"
               "vand            q4, q9, q4      \n\t"
               "vand            q5, q9, q5      \n\t"
               "vand            q6, q9, q6      \n\t"
               "vand            q7, q9, q7      \n\t"
               /* Dacc: q4(b), q5(g), q6(r), q7(a) */
               "vorr            q4, q4, q10     \n\t"
               "vorr            q5, q5, q11     \n\t"
               "vorr            q6, q6, q12     \n\t"
               "vorr            q7, q7, q13     \n\t"
               "vst4.16         {d8, d10, d12, d14}, [r5]! \n\t"
               "vst4.16         {d9, d11, d13, d15}, [r5]! \n\t"
               "subs            %[loop], %[loop], #1       \n\t"
               "bne             1b                 "
               :
               : [Cacc_a] "r" (Cacc.RGB.a), [Cacc_r] "r" (Cacc.RGB.r),
                 [Cacc_g] "r" (Cacc.RGB.g), [Cacc_b] "r" (Cacc.RGB.b),
                 [D] "r" (D), [maska] "r" (maska), [loop] "r" (loop)
               : "memory", "r4", "r5", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7",
                 "d8", "d9", "d10", "d11", "d12", "d13", "d14", "d15", "d16", "d17", "d18",
                 "d19", "d20", "d21", "d22", "d23", "d24", "d25", "d26", "d27", "d28", "d29"
          );
     }

}

/* 
 * NEON version of Dacc_modulate_rgb_NEON.
 */
static void Dacc_modulate_rgb_NEON( GenefxState *gfxs )
{
     int                w = gfxs->length;
     GenefxAccumulator *D = gfxs->Dacc;
     GenefxAccumulator  Cacc = gfxs->Cacc;
     unsigned int       loop = w >> 3;
     unsigned int       single = w & 0x7;
     u16                maska = 0xF000;

     while (single){
          if (!(D->RGB.a & 0xF000)) {
               D->RGB.r = (Cacc.RGB.r * D->RGB.r) >> 8;
               D->RGB.g = (Cacc.RGB.g * D->RGB.g) >> 8;
               D->RGB.b = (Cacc.RGB.b * D->RGB.b) >> 8;
          }
          D++;
          single--;
     }

     if (loop) {
          __asm__ __volatile__ (
               "mov             r4, %[D]        \n\t"
               "mov             r5, %[D]        \n\t"
               "vdup.16         q2, %[Cacc_r]   \n\t"
               "vdup.16         q1, %[Cacc_g]   \n\t"
               "vdup.16         q0, %[Cacc_b]   \n\t"
               "vdup.16         q8, %[maska]    \n\t"
               "1:                              \n\t"
               "pld             [r4, #0xC0]     \n\t"
               "pld             [r4, #0x100]     \n\t"
               /* vload q4:b, q5:g, q6:r, q7:a */
               "vld4.16         {d8, d10, d12, d14}, [r4]! \n\t"
               "vld4.16         {d9, d11, d13, d15}, [r4]! \n\t"
               "vand            q9, q7, q8      \n\t"
               "vceq.i16        q9, q9, #0      \n\t"
               /* b:q10  q0, q4 */
               "vmull.u16       q3, d8, d0      \n\t"
               "vshrn.i32       d20, q3, #8    	\n\t"
               "vmull.u16       q3, d9, d1      \n\t"
               "vshrn.i32       d21, q3, #8    	\n\t"
               /* g:q11, q1, q5 */
               "vmull.u16       q3, d10, d2	\n\t"
               "vshrn.i32       d22, q3, #8    	\n\t"
               "vmull.u16       q3, d11, d3    	\n\t"
               "vshrn.i32       d23, q3, #8    	\n\t"
               /* r:q12, q2, q6 */
               "vmull.u16       q3, d12, d4    	\n\t"
               "vshrn.i32       d24, q3, #8    	\n\t"
               "vmull.u16       q3, d13, d5    	\n\t"
               "vshrn.i32       d25, q3, #8    	\n\t"
               /* if (!(D->RGB.a & 0xF000)) */
               "vand            q10, q9, q10    \n\t"
               "vand            q11, q9, q11    \n\t"
               "vand            q12, q9, q12    \n\t"
               /* if ((D->RGB.a & 0xF000)) */
               "vceq.i16        q9, q9, #0      \n\t"
               "vand            q4, q9, q4      \n\t"
               "vand            q5, q9, q5      \n\t"
               "vand            q6, q9, q6      \n\t"
               /* Dacc: q4(b), q5(g), q6(r), q7(a) */
               "vorr            q4, q4, q10     \n\t"
               "vorr            q5, q5, q11     \n\t"
               "vorr            q6, q6, q12     \n\t"
               "vst4.16         {d8, d10, d12, d14}, [r5]! \n\t"
               "vst4.16         {d9, d11, d13, d15}, [r5]! \n\t"
               "subs            %[loop], %[loop], #1       \n\t"
               "bne             1b                  "
               :
               : [Cacc_r] "r" (Cacc.RGB.r), [Cacc_g] "r" (Cacc.RGB.g), 
		 [Cacc_b] "r" (Cacc.RGB.b), [D] "r" (D), [maska] "r" (maska), 
		 [loop] "r" (loop)
               : "memory", "r4", "r5", "d0", "d1", "d2", "d3", "d4", "d5", "d6", 
		 "d7", "d8", "d9", "d10", "d11", "d12", "d13", "d14", "d15", "d16", 
		 "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d25"
          );
     }
}

/*
 * NEON version of Bop_argb_blend_alphachannel_src_invsrc_Aop_rgb16.
 */
static void Bop_argb_blend_alphachannel_src_invsrc_Aop_rgb16_NEON( GenefxState *gfxs)
{
     int  w = gfxs->length;
     u32  *S = gfxs->Bop[0];
     u16  *D = gfxs->Aop[0];
     int  single = w & 3;
     int  loop = w >> 2;
     u32  factor[4] = { 0xf800, 0x001f, 0x003e07c0, 0x0001f800 };
     u16  f2 = 0x0000f81f;
     u16  f3 = 0x000007e0;
     int  flag = 0;
     u32  alpha, valpha;
     int  i;

#define SET_PIXEL( D, S )               \
     switch (S >> 26) {                 \
          case 0:                       \
               break;                   \
          case 0x3f:                    \
               D = RGB32_TO_RGB16( S ); \
               break;                   \
          default:                      \
               D = (((( (((S>>8) & 0xf800) | ((S>>3) & 0x001f))                                     \
                        - (D & 0xf81f)) * ((S>>26)+1) + ((D & 0xf81f)<<6)) & 0x003e07c0)            \
                      +                                                                             \
                    ((( ((S>>5) & 0x07e0)                                                           \
                        - (D & 0x07e0)) * ((S>>26)+1) + ((D & 0x07e0)<<6)) & 0x0001f800)) >> 6;     \
     } while (0)


     while (single) {
          SET_PIXEL( *D, *S );
          S++;
          D++;
          single--;
     }

     __asm__ __volatile__ (
          "vld1.32      {d0, d1}, [%[vf1]] \n\t"
          "vdup.32      q1, d0[0]    	\n\t"   
          "vdup.32      q2, d0[1]    	\n\t"   
          "vdup.32      q3, %[f2]    	\n\t"   
          "vdup.32      q4, d1[0]       \n\t"   
          "vdup.32      q5, %[f3]       \n\t"   
          "vdup.32      q6, d1[1]       \n\t"   
          "vmov.i32     q11, #0x1       \n\t"
          :
          : [vf1] "r" (factor), [f2] "r" (f2), [f3] "r" (f3)
          : "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", 
	    "d8", "d9", "d10", "d11", "d12", "d13", "d22", "d23"
     );

     while (loop) {
          valpha = 0;
          flag = 0;
          for (i=0; i<4; i++) {
               alpha = S[i] >> 26;
               valpha |= alpha << (i << 3);
               flag = flag || ((alpha == 0x0) || (alpha == 0x3f));
          }

          switch (valpha) {
               case 0:       
                    break;
               case 0x3f3f3f3f:  
                    __asm__ __volatile__ (		         
               		 "pld          [%[S], #0xC0]	\n\t"
                         "vld1.32      {d0, d1}, [%[S]]	\n\t"
                         "vshr.u32     q7, q0, #19	\n\t"
                         "vshr.u16     q8, q0, #10     	\n\t"
                         "vshl.u16     q9, q0, #8      	\n\t"
                         "vshl.u16     q7, q7, #11     	\n\t"
                         "vshl.u16     q8, q8, #5     	\n\t"
                         "vshr.u16     q9, q9, #11     	\n\t"
                     	 "vorr         q7, q7, q8   	\n\t"
                         "vorr         q7, q7, q9   	\n\t"
                         "vmovn.i32    d14, q7      	\n\t"
                         "vst1.16      {d14}, [%[D]]       "
                         :
                         : [S] "r" (S), [D] "r" (D)
                         : "memory", "d0", "d1", "d14", "d15", "d16"
                    );
                    break;
               default:
                    if (flag) {     
                         for (i=0; i<4; i++)
                              SET_PIXEL( D[i], S[i] );
                    } else {
                         __asm__ __volatile__ (
               		      "pld          [%[D], #0xC0]	\n\t"
               		      "pld          [%[S], #0xC0]      	\n\t"
          		      "vld1.16      {d14}, [%[D]]	\n\t"          
                              "vld1.32      {d0, d1}, [%[S]]	\n\t"   
          		      "vmovl.u16    q7, d14         	\n\t"   
          		     
		              "vshr.u32     q8, q0, #8    	\n\t"  
		              "vand         q8, q8, q1		\n\t"   
		              "vshr.u32     q9, q0, #3      	\n\t"   
		              "vand         q9, q9, q2      	\n\t"   
		              "vorr         q8, q8, q9      	\n\t"   
		              
		              "vand         q9, q7, q3      	\n\t"
		              "vsub.i32     q8, q8, q9      	\n\t"
	
		              "vshr.u32     q10, q0, #26     	\n\t"
		              "vadd.i32     q10, q10, q11   	\n\t"
		              "vmul.i32     q8, q8, q10     	\n\t"
		              "vshl.u32     q9, q9, #6      	\n\t"
		              "vadd.i32     q8, q8, q9      	\n\t"
		              "vand         q8, q8, q4      	\n\t"

		              "vshr.u32     q12, q0, #5     	\n\t"
		              "vand         q12, q12, q5    	\n\t"
		              "vand         q13, q7, q5     	\n\t"
		              "vsub.i32     q12, q12, q13   	\n\t"
	   	              "vmul.i32     q12, q12, q10   	\n\t"
		              "vshl.u32     q13, q13, #6    	\n\t"
		              "vadd.i32     q12, q12, q13   	\n\t"
		              "vand         q12, q12, q6    	\n\t"
		              "vadd.i32     q8, q8, q12     	\n\t"
		              "vshrn.u32    d16, q8, #6     	\n\t"
		              "vst1.16      {d16}, [%[D]]           "
          		      :
		              : [S] "r" (S), [D] "r" (D)
			      : "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7",
				"d8", "d9", "d10", "d11", "d12", "d13", "d14", "d15",
				"d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23",
				"d24", "d25", "d26", "d27"
     			      );
     			 } //end of default-else
               break;
          } // end of switch
          S += 4;
          D += 4;
          loop--;
     } //end of loop
}

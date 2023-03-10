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
Sop_rgb16_to_Dacc_NEON( GenefxState *gfxs )
{
     int                l;
     int                w     = gfxs->length;
     u16               *S     = gfxs->Sop[0];
     GenefxAccumulator *D     = gfxs->Dacc;
     int                Ostep = gfxs->Ostep;

#define EXPAND_Rto8(r) (((r) << 3) | ((r) >> 2))
#define EXPAND_Gto8(g) (((g) << 2) | ((g) >> 4))
#define EXPAND_Bto8(b) (((b) << 3) | ((b) >> 2))
#define EXPAND(d,s) do {                            \
     (d).RGB.a = 0xff;                              \
     (d).RGB.r = EXPAND_Rto8( (s & 0xf800) >> 11 ); \
     (d).RGB.g = EXPAND_Gto8( (s & 0x07e0) >> 5 );  \
     (d).RGB.b = EXPAND_Bto8(  s & 0x001f );        \
} while (0)

     if (Ostep != 1) {
          while (w--) {
               u16 s = *S;

               EXPAND( *D, s );

               S += Ostep;
               ++D;
          }

          return;
     }

     l = w & 0x7;
     while (l) {
          u16 s = *S;

          EXPAND( *D, s );

          ++S;
          ++D;
          --l;
     }

     l = w >> 3;
     if (l) {
          __asm__ __volatile__ (
               "1:\n\t"
               "pld       [%[S], #0xC0]\n\t"
               "vld1.16   {q0}, [%[S]]!\n\t"
               "vmov.i16  q4, #0x00FF\n\t"
               "vshr.u16  q3, q0, #8\n\t"
               "vsri.u8   q3, q3, #5\n\t"
               "vshl.u16  q2, q0, #5\n\t"
               "vshl.u16  q1, q0, #11\n\t"
               "vshr.u16  q2, q2, #8\n\t"
               "vshr.u16  q1, q1, #8\n\t"
               "vsri.u8   q2, q2, #6\n\t"
               "vsri.u8   q1, q1, #5\n\t"
               "vst4.16   {d2, d4, d6, d8}, [%[D]]!\n\t"
               "vst4.16   {d3, d5, d7, d9}, [%[D]]!\n\t"
               "subs      %[l], %[l], #1\n\t"
               "bne       1b"
               : /* no outputs */
               : [S] "r" (S), [D] "r" (D), [l] "r" (l)
               : "memory", "d2", "d3", "d4", "d5", "d6", "d7", "d8", "d9"
          );
     }

#undef EXPAND
#undef EXPAND_Rto8
#undef EXPAND_Gto8
#undef EXPAND_Bto8
}

static void
Sop_argb_to_Dacc_NEON( GenefxState *gfxs )
{
     int                w     = gfxs->length;
     u16               *S     = gfxs->Sop[0];
     GenefxAccumulator *D     = gfxs->Dacc;
     int                Ostep = gfxs->Ostep;

     __asm__ __volatile__ (
          "mov       r5, %[w]\n\t"
          "cmp       %[Ostep], #1\n\t"
          /* Ostep != 1 */
          "bne       1f\n\t"
          "lsrs      r4, %[w], #3\n\t"
          /* Ostep = 1 && w < 8 */
          "beq       1f\n\t"
          /* Ostep = 1 && w >= 8 */
          "2:\n\t"
          "pld       [%[S], #0xC0] \n\t"
          "vld4.8    {d0, d1, d2, d3}, [%[S]]!\n\t"
          "vmovl.u8  q2, d0\n\t"
          "vmovl.u8  q3, d1\n\t"
          "vmovl.u8  q4, d2\n\t"
          "vmovl.u8  q5, d3\n\t"
          "vst4.16   {d4, d6, d8, d10}, [%[D]]!\n\t"
          "vst4.16   {d5, d7, d9, d11}, [%[D]]!\n\t"
          "subs      r4, r4, #1\n\t"
          "bne       2b\n\t"
          "ands      r5, %[w], #7\n\t"
          /* Ostep = 1 && w = 8 */
          "beq       5f\n\t"
          "1:\n\t"
          "lsl       r6, %[Ostep], #2\n\t"
          "vmov.i32  d0, #0\n\t"
          "vmov      d1, d0\n\t"
          "vmov      d2, d0\n\t"
          "vmov      d3, d0\n\t"
          "lsrs      r4, r5, #2\n\t"
          /* w < 4 */
          "beq       4f\n\t"
          /* w >= 4 */
          "3:\n\t"
          "pld       [%[S], #0xC0]\n\t"
          "vld4.8    {d0[0], d1[0], d2[0], d3[0]}, [%[S]],r6\n\t"
          "vld4.8    {d0[2], d1[2], d2[2], d3[2]}, [%[S]],r6\n\t"
          "vld4.8    {d0[4], d1[4], d2[4], d3[4]}, [%[S]],r6\n\t"
          "vld4.8    {d0[6], d1[6], d2[6], d3[6]}, [%[S]],r6\n\t"
          "vst4.16   {d0, d1, d2, d3}, [%[D]]!\n\t"
          "subs      r4, r4, #1\n\t"
          "bne       3b\n\t"
          "ands      r5, %[w], #3\n\t"
          /* w = 4 */
          "beq       5f\n\t"
          "4:\n\t"
          "vld4.8    {d0[0], d1[0], d2[0], d3[0]}, [%[S]],r6\n\t"
          "vst4.16   {d0[0], d1[0], d2[0], d3[0]}, [%[D]]!\n\t"
          "subs      r5, r5, #1\n\t"
          "bne       4b\n\t"
          "5:"
          : /* no outputs */
          : [S] "r" (S), [D] "r" (D), [w] "r" (w), [Ostep] "r" (Ostep)
          : "memory", "r4", "r5", "r6", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "d8", "d9", "d10", "d11"
     );
}

static void
Sacc_to_Aop_rgb16_NEON( GenefxState *gfxs )
{
     int                l;
     int                w     = gfxs->length;
     GenefxAccumulator *S     = gfxs->Sacc;
     u16               *D     = gfxs->Aop[0];
     int                Dstep = gfxs->Astep;
     u32                mask  = 0xff00ff00;
     u32                color = 0x00ff00ff;

#define PIXEL_OUT(r,g,b) ((((r) & 0xf8) << 8) | (((g) & 0xfc) << 3) | (((b) & 0xf8) >> 3))
#define PIXEL(x) PIXEL_OUT( ((x).RGB.r & 0xff00) ? 0xff : (x).RGB.r, \
                            ((x).RGB.g & 0xff00) ? 0xff : (x).RGB.g, \
                            ((x).RGB.b & 0xff00) ? 0xff : (x).RGB.b )

     if (Dstep != 1) {
          while (w--) {
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

     l = w >> 3;
     while (l) {
          int i;

          for (i = 0; i < 8; i++) {
               if (S[i].RGB.a & 0xf000)
                    break;
          }

          if (i < 8) {
               int j;

               for (j = 0; j < i; j++)
                    D[j] = PIXEL( S[j] );

               for (j = i + 1; j < 8; j++) {
                    if (!(S[j].RGB.a & 0xf000))
                         D[j] = PIXEL( S[j] );
               }
          }
          else {
               __asm__ __volatile__ (
                    "mov       r4, %[S]\n\t"
                    "pld       [r4, #0xC0]\n\t"
                    "pld       [r4, #0x100]\n\t"
                    "vld4.16   {d0, d2, d4, d6}, [r4]!\n\t"
                    "vld4.16   {d1, d3, d5, d7}, [r4]\n\t"
                    "vdup.32   q3, %[mask]\n\t"
                    "vdup.32   q4, %[color]\n\t"
                    /* b */
                    "vtst.16   q5, q0, q3\n\t"
                    "vceq.u16  q6, q5, #0\n\t"
                    "vand.16   q5, q4, q5\n\t"
                    "vand.16   q6, q0, q6\n\t"
                    "vorr.16   q0, q5, q6\n\t"
                    "vshr.u16  q0, q0, #3\n\t"
                    /* g */
                    "vtst.16   q5, q1, q3\n\t"
                    "vceq.u16  q6, q5, #0\n\t"
                    "vand.16   q5, q4, q5\n\t"
                    "vand.16   q6, q1, q6\n\t"
                    "vorr.16   q1, q5, q6\n\t"
                    "vshr.u16  q1, q1, #2\n\t"
                    "vshl.u16  q1, q1, #5\n\t"
                    /* r */
                    "vtst.16   q5, q2, q3\n\t"
                    "vceq.u16  q6, q5, #0\n\t"
                    "vand.16   q5, q4, q5\n\t"
                    "vand.16   q6, q2, q6\n\t"
                    "vorr.16   q2, q5, q6\n\t"
                    "vshr.u16  q2, q2, #3\n\t"
                    "vshl.u16  q2, q2, #11\n\t"
                    /* rgb */
                    "vorr.16   q1, q1, q2\n\t"
                    "vorr.16   q0, q0, q1\n\t"
                    "vst1.16   {d0, d1}, [%[D]]"
                    : /* no outputs */
                    : [D] "r" (D), [S] "r" (S),
                      [mask] "r" (mask), [color] "r" (color)
                    : "memory", "r4", "d0", "d1", "d2", "d3", "d4","d5", "d6", "d7"
               );
          }

          S += 8;
          D += 8;
          --l;
     }

     l = w & 0x7;
     while (l) {
          if (!(S->RGB.a & 0xf000)) {
               *D = PIXEL( *S );
          }

          ++S;
          ++D;
          --l;
     }

#undef PIXEL
#undef PIXEL_OUT
}

static void
Bop_argb_blend_alphachannel_src_invsrc_Aop_rgb16_NEON( GenefxState *gfxs )
{
     int  l;
     int  w     = gfxs->length;
     u32 *S     = gfxs->Bop[0];
     u16 *D     = gfxs->Aop[0];
     u16  f0    = 0xf81f;
     u16  f1    = 0x07e0;
     u32  vf[4] = { 0x0000f800, 0x0000001f, 0x003e07c0, 0x0001f800 };

#define SET_PIXEL(d,s)                                                                                                \
     switch (s >> 26) {                                                                                               \
          case 0:                                                                                                     \
               break;                                                                                                 \
          case 0x3f:                                                                                                  \
               d = ((s & 0x00f80000) >> 8) | ((s & 0x0000fc00) >> 5) | ((s & 0x000000f8) >> 3);                       \
               break;                                                                                                 \
          default:                                                                                                    \
               d = (((((((s >> 8) & 0xf800) | ((s >> 3) & 0x001f)) - (d & 0xf81f)) * ((s >> 26) + 1) +                \
                      ((d & 0xf81f) << 6)                                                           ) & 0x003e07c0) + \
                    ((((( s >> 5) & 0x07e0)                        - (d & 0x07e0)) * ((s >> 26) + 1) +                \
                      ((d & 0x07e0) << 6)                                                           ) & 0x0001f800)   \
                   ) >> 6;                                                                                            \
     }

     l = w & 3;
     while (l) {
          SET_PIXEL( *D, *S );

          ++S;
          ++D;
          --l;
     }

     __asm__ __volatile__ (
          "vld1.32   {d0, d1}, [%[vf]]\n\t"
          "vdup.32   q1, d0[0]\n\t"
          "vdup.32   q2, d0[1]\n\t"
          "vdup.32   q3, %[f0]\n\t"
          "vdup.32   q4, d1[0]\n\t"
          "vdup.32   q5, %[f1]\n\t"
          "vdup.32   q6, d1[1]\n\t"
          "vmov.i32  q11, #0x1\n\t"
          : /* no outputs */
          : [vf] "r" (vf), [f0] "r" (f0), [f1] "r" (f1)
          : "memory", "d0", "d1"
     );

     l = w >> 2;
     while (l) {
          int  i;
          u32  valpha = 0;
          bool a_flag = false;

          for (i = 0; i < 4; i++) {
               u32 alpha;

               alpha   = S[i] >> 26;
               valpha |= alpha << (i << 3);
               a_flag  = a_flag || (alpha == 0) || (alpha == 0x3f);
          }

          switch (valpha) {
               case 0:
                    break;
               case 0x3f3f3f3f:
                    __asm__ __volatile__ (
                         "pld       [%[S], #0xC0]\n\t"
                         "vld1.32   {d0, d1}, [%[S]]\n\t"
                         "vshr.u32  q7, q0, #19\n\t"
                         "vshr.u16  q8, q0, #10\n\t"
                         "vshl.u16  q9, q0, #8\n\t"
                         "vshl.u16  q7, q7, #11\n\t"
                         "vshl.u16  q8, q8, #5\n\t"
                         "vshr.u16  q9, q9, #11\n\t"
                         "vorr      q7, q7, q8\n\t"
                         "vorr      q7, q7, q9\n\t"
                         "vmovn.i32 d14, q7\n\t"
                         "vst1.16   {d14}, [%[D]]"
                         : /* no outputs */
                         : [S] "r" (S), [D] "r" (D)
                         : "memory", "d0", "d1", "d14"
                    );
                    break;
               default:
                    if (a_flag) {
                         for (i = 0; i < 4; i++)
                              SET_PIXEL( D[i], S[i] );
                    }
                    else {
                         __asm__ __volatile__ (
                              "pld       [%[D], #0xC0]\n\t"
                              "pld       [%[S], #0xC0]\n\t"
                              "vld1.16   {d14}, [%[D]]\n\t"
                              "vld1.32   {d0, d1}, [%[S]]\n\t"
                              "vmovl.u16 q7, d14\n\t"
                              "vshr.u32  q8, q0, #8\n\t"
                              "vand      q8, q8, q1\n\t"
                              "vshr.u32  q9, q0, #3\n\t"
                              "vand      q9, q9, q2\n\t"
                              "vorr      q8, q8, q9\n\t"
                              "vand      q9, q7, q3\n\t"
                              "vsub.i32  q8, q8, q9\n\t"
                              "vshr.u32  q10, q0, #26\n\t"
                              "vadd.i32  q10, q10, q11\n\t"
                              "vmul.i32  q8, q8, q10\n\t"
                              "vshl.u32  q9, q9, #6\n\t"
                              "vadd.i32  q8, q8, q9\n\t"
                              "vand      q8, q8, q4\n\t"
                              "vshr.u32  q12, q0, #5\n\t"
                              "vand      q12, q12, q5\n\t"
                              "vand      q13, q7, q5\n\t"
                              "vsub.i32  q12, q12, q13\n\t"
                              "vmul.i32  q12, q12, q10\n\t"
                              "vshl.u32  q13, q13, #6\n\t"
                              "vadd.i32  q12, q12, q13\n\t"
                              "vand      q12, q12, q6\n\t"
                              "vadd.i32  q8, q8, q12\n\t"
                              "vshrn.u32 d16, q8, #6\n\t"
                              "vst1.16   {d16}, [%[D]]"
                              : /* no outputs */
                              : [S] "r" (S), [D] "r" (D)
                              : "memory", "d0", "d1", "d14", "d16"
                         );
                    }
                    break;
          }

          S += 4;
          D += 4;
          --l;
     }
}

static void
Xacc_blend_srcalpha_NEON( GenefxState *gfxs )
{
     int                l;
     int                w    = gfxs->length;
     GenefxAccumulator *X    = gfxs->Xacc;
     GenefxAccumulator *Y    = gfxs->Yacc;
     u16                mask = 0xf000;

     if (gfxs->Sacc) {
          GenefxAccumulator *S    = gfxs->Sacc;
          u16                sadd = 1;

          l = w >> 3;
          if (l) {
               __asm__ __volatile__ (
                    "vdup.16   q8, %[mask]\n\t"
                    "vdup.16   q9, %[sadd]\n\t"
                    "1:\n\t"
                    "pld       [%[Y], #0xC0]\n\t"
                    "pld       [%[Y], #0x100]\n\t"
                    "pld       [%[S], #0xC0]\n\t"
                    "pld       [%[S], #0x100]\n\t"
                    "vld4.16   {d0, d2, d4, d6}, [%[Y]]!\n\t"
                    "vld4.16   {d1, d3, d5, d7}, [%[Y]]!\n\t"
                    "vld4.16   {d8, d10, d12, d14}, [%[S]]!\n\t"
                    "vld4.16   {d9, d11, d13, d15}, [%[S]]!\n\t"
                    "vand      q4, q3, q8\n\t"
                    "vceq.i16  q4, q4, #0\n\t"
                    "vadd.i16  q5, q9, q7\n\t"
                    /* b */
                    "vmull.u16 q6, d10, d0\n\t"
                    "vshrn.i32 d20, q6, #8\n\t"
                    "vmull.u16 q6, d11, d1\n\t"
                    "vshrn.i32 d21, q6, #8\n\t"
                    /* g */
                    "vmull.u16 q6, d10, d2\n\t"
                    "vshrn.i32 d22, q6, #8\n\t"
                    "vmull.u16 q6, d11, d3\n\t"
                    "vshrn.i32 d23, q6, #8\n\t"
                    /* r */
                    "vmull.u16 q6, d10, d4\n\t"
                    "vshrn.i32 d24, q6, #8\n\t"
                    "vmull.u16 q6, d11, d5\n\t"
                    "vshrn.i32 d25, q6, #8\n\t"
                    /* a */
                    "vmull.u16 q6, d10, d6\n\t"
                    "vshrn.i32 d26, q6, #8\n\t"
                    "vmull.u16 q6, d11, d7\n\t"
                    "vshrn.i32 d27, q6, #8\n\t"
                    /* if (!(Y->RGB.a & 0xf000)) */
                    "vand      q10, q4, q10\n\t"
                    "vand      q11, q4, q11\n\t"
                    "vand      q12, q4, q12\n\t"
                    "vand      q13, q4, q13\n\t"
                    /* if ((Y->RGB.a & 0xf000)) */
                    "vceq.i16  q4, q4, #0\n\t"
                    "vand      q0, q4, q0\n\t"
                    "vand      q1, q4, q1\n\t"
                    "vand      q2, q4, q2\n\t"
                    "vand      q3, q4, q3\n\t"
                    /* X: q0(b), q1(g), q2(r), q3(a) */
                    "vorr      q0, q0, q10\n\t"
                    "vorr      q1, q1, q11\n\t"
                    "vorr      q2, q2, q12\n\t"
                    "vorr      q3, q3, q13\n\t"
                    "vst4.16   {d0, d2, d4, d6}, [%[X]]!\n\t"
                    "vst4.16   {d1, d3, d5, d7}, [%[X]]!\n\t"
                    "subs      %[l], %[l], #1\n\t"
                    "bne       1b"
                    : /* no outputs */
                    : [X] "r" (X), [Y] "r" (Y), [S] "r" (S), [mask] "r" (mask), [sadd] "r" (sadd), [l] "r" (l)
                    : "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "d8", "d9", "d10", "d11",
                      "d12", "d13", "d14", "d15", "d20", "d21", "d22", "d23", "d24", "d25", "d26", "d27"
               );
          }

          l = w & 0x7;
          while (l) {
               if (!(Y->RGB.a & 0xf000)) {
                    u16 Sa = S->RGB.a + 1;

                    X->RGB.r = (Sa * Y->RGB.r) >> 8;
                    X->RGB.g = (Sa * Y->RGB.g) >> 8;
                    X->RGB.b = (Sa * Y->RGB.b) >> 8;
                    X->RGB.a = (Sa * Y->RGB.a) >> 8;
               }
               else
                    *X = *Y;

               ++S;
               ++X; ++Y;
               --l;
          }
     }
     else {
          u16 Sa = gfxs->color.a + 1;

          l = w >> 3;
          if (l) {
               __asm__ __volatile__ (
                    "vdup.16   q7, %[mask]\n\t"
                    "vdup.16   q5, %[Sa]\n\t"
                    "1:\n\t"
                    "pld       [%[Y], #0xC0]\n\t"
                    "pld       [%[Y], #0x100]\n\t"
                    "vld4.16   {d0, d2, d4, d6}, [%[Y]]!\n\t"
                    "vld4.16   {d1, d3, d5, d7}, [%[Y]]!\n\t"
                    "vand      q4, q3, q7\n\t"
                    "vceq.i16  q4, q4, #0\n\t"
                    /* b */
                    "vmull.u16 q6, d10, d0\n\t"
                    "vshrn.i32 d20, q6, #8\n\t"
                    "vmull.u16 q6, d11, d1\n\t"
                    "vshrn.i32 d21, q6, #8\n\t"
                    /* g */
                    "vmull.u16 q6, d10, d2\n\t"
                    "vshrn.i32 d22, q6, #8\n\t"
                    "vmull.u16 q6, d11, d3\n\t"
                    "vshrn.i32 d23, q6, #8\n\t"
                    /* r */
                    "vmull.u16 q6, d10, d4\n\t"
                    "vshrn.i32 d24, q6, #8\n\t"
                    "vmull.u16 q6, d11, d5\n\t"
                    "vshrn.i32 d25, q6, #8\n\t"
                    /* a */
                    "vmull.u16 q6, d10, d6\n\t"
                    "vshrn.i32 d26, q6, #8\n\t"
                    "vmull.u16 q6, d11, d7\n\t"
                    "vshrn.i32 d27, q6, #8\n\t"
                    /* if (!(Y->RGB.a & 0xf000)) */
                    "vand      q10, q4, q10\n\t"
                    "vand      q11, q4, q11\n\t"
                    "vand      q12, q4, q12\n\t"
                    "vand      q13, q4, q13\n\t"
                    /* if ((Y->RGB.a & 0xf000)) */
                    "vceq.i16  q4, q4, #0\n\t"
                    "vand      q0, q4, q0\n\t"
                    "vand      q1, q4, q1\n\t"
                    "vand      q2, q4, q2\n\t"
                    "vand      q3, q4, q3\n\t"
                    /* X: q0(b), q1(g), q2(r), q3(a) */
                    "vorr      q0, q0, q10\n\t"
                    "vorr      q1, q1, q11\n\t"
                    "vorr      q2, q2, q12\n\t"
                    "vorr      q3, q3, q13\n\t"
                    "vst4.16   {d0, d2, d4, d6}, [%[X]]!\n\t"
                    "vst4.16   {d1, d3, d5, d7}, [%[X]]!\n\t"
                    "subs      %[l], %[l], #1\n\t"
                    "bne       1b"
                    : /* no outputs */
                    : [X] "r" (X), [Y] "r" (Y), [Sa] "r" (Sa), [mask] "r" (mask), [l] "r" (l)
                    : "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "d10", "d11",
                      "d20", "d21", "d22", "d23", "d24", "d25", "d26", "d27"
               );
          }

          l = w & 0x7;
          while (l) {
               if (!(Y->RGB.a & 0xf000)) {
                    X->RGB.r = (Sa * Y->RGB.r) >> 8;
                    X->RGB.g = (Sa * Y->RGB.g) >> 8;
                    X->RGB.b = (Sa * Y->RGB.b) >> 8;
                    X->RGB.a = (Sa * Y->RGB.a) >> 8;
               }
               else
                    *X = *Y;

               ++X; ++Y;
               --l;
          }
     }
}

static void
Xacc_blend_invsrcalpha_NEON( GenefxState *gfxs )
{
     int                l;
     int                w = gfxs->length;
     GenefxAccumulator *X = gfxs->Xacc;
     GenefxAccumulator *Y = gfxs->Yacc;
     u16                mask = 0xf000;

     if (gfxs->Sacc) {
          GenefxAccumulator *S   = gfxs->Sacc;
          u16                inv = 0x100;

          l = w >> 3;
          if (l) {
               __asm__ __volatile__ (
                    "vdup.16   q8, %[mask]\n\t"
                    "vdup.16   q9, %[inv]\n\t"
                    "1:\n\t"
                    "pld       [%[Y], #0xC0]\n\t"
                    "pld       [%[Y], #0x100]\n\t"
                    "pld       [%[S], #0xC0]\n\t"
                    "pld       [%[S], #0x100]\n\t"
                    "vld4.16   {d0, d2, d4, d6}, [%[Y]]!\n\t"
                    "vld4.16   {d1, d3, d5, d7}, [%[Y]]!\n\t"
                    "vld4.16   {d8, d10, d12, d14}, [%[S]]!\n\t"
                    "vld4.16   {d9, d11, d13, d15}, [%[S]]!\n\t"
                    "vand      q4, q3, q8\n\t"
                    "vceq.i16  q4, q4, #0\n\t"
                    "vsub.i16  q5, q9, q7\n\t"
                    /* b */
                    "vmull.u16 q6, d10, d0\n\t"
                    "vshrn.i32 d20, q6, #8\n\t"
                    "vmull.u16 q6, d11, d1\n\t"
                    "vshrn.i32 d21, q6, #8\n\t"
                    /* g */
                    "vmull.u16 q6, d10, d2\n\t"
                    "vshrn.i32 d22, q6, #8\n\t"
                    "vmull.u16 q6, d11, d3\n\t"
                    "vshrn.i32 d23, q6, #8\n\t"
                    /* r */
                    "vmull.u16 q6, d10, d4\n\t"
                    "vshrn.i32 d24, q6, #8\n\t"
                    "vmull.u16 q6, d11, d5\n\t"
                    "vshrn.i32 d25, q6, #8\n\t"
                    /* a */
                    "vmull.u16 q6, d10, d6\n\t"
                    "vshrn.i32 d26, q6, #8\n\t"
                    "vmull.u16 q6, d11, d7\n\t"
                    "vshrn.i32 d27, q6, #8\n\t"
                    /* if (!(Y->RGB.a & 0xf000)) */
                    "vand      q10, q4, q10\n\t"
                    "vand      q11, q4, q11\n\t"
                    "vand      q12, q4, q12\n\t"
                    "vand      q13, q4, q13\n\t"
                    /* if ((Y->RGB.a & 0xf000)) */
                    "vceq.i16  q4, q4, #0\n\t"
                    "vand      q0, q4, q0\n\t"
                    "vand      q1, q4, q1\n\t"
                    "vand      q2, q4, q2\n\t"
                    "vand      q3, q4, q3\n\t"
                    /* X: q0(b), q1(g), q2(r), q3(a) */
                    "vorr      q0, q0, q10\n\t"
                    "vorr      q1, q1, q11\n\t"
                    "vorr      q2, q2, q12\n\t"
                    "vorr      q3, q3, q13\n\t"
                    "vst4.16   {d0, d2, d4, d6}, [%[X]]!\n\t"
                    "vst4.16   {d1, d3, d5, d7}, [%[X]]!\n\t"
                    "subs      %[l], %[l], #1\n\t"
                    "bne       1b"
                    : /* no outputs */
                    : [X] "r" (X), [Y] "r" (Y), [S] "r" (S), [mask] "r" (mask), [inv] "r" (inv), [l] "r" (l)
                    : "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "d8", "d9", "d10", "d11",
                      "d12", "d13", "d14", "d15", "d20", "d21", "d22", "d23", "d24", "d25", "d26", "d27"
               );
          }

          l = w & 0x7;
          while (l) {
               if (!(Y->RGB.a & 0xf000)) {
                    u16 Sa = 0x100 - S->RGB.a;

                    X->RGB.r = (Sa * Y->RGB.r) >> 8;
                    X->RGB.g = (Sa * Y->RGB.g) >> 8;
                    X->RGB.b = (Sa * Y->RGB.b) >> 8;
                    X->RGB.a = (Sa * Y->RGB.a) >> 8;
               }
               else
                    *X = *Y;

               ++S;
               ++X; ++Y;
               --l;
          }
     }
     else {
          u16 Sa = 0x100 - gfxs->color.a;

          l = w >> 3;
          if (l) {
               __asm__ __volatile__ (
                    "vdup.16   q7, %[mask]\n\t"
                    "vdup.16   q5, %[Sa]\n\t"
                    "1:\n\t"
                    "pld       [%[Y], #0xC0]\n\t"
                    "pld       [%[Y], #0x100]\n\t"
                    "vld4.16   {d0, d2, d4, d6}, [%[Y]]!\n\t"
                    "vld4.16   {d1, d3, d5, d7}, [%[Y]]!\n\t"
                    "vand      q4, q3, q7\n\t"
                    "vceq.i16  q4, q4, #0\n\t"
                    /* b */
                    "vmull.u16 q6, d10, d0\n\t"
                    "vshrn.i32 d20, q6, #8\n\t"
                    "vmull.u16 q6, d11, d1\n\t"
                    "vshrn.i32 d21, q6, #8\n\t"
                    /* g */
                    "vmull.u16 q6, d10, d2\n\t"
                    "vshrn.i32 d22, q6, #8\n\t"
                    "vmull.u16 q6, d11, d3\n\t"
                    "vshrn.i32 d23, q6, #8\n\t"
                    /* r */
                    "vmull.u16 q6, d10, d4\n\t"
                    "vshrn.i32 d24, q6, #8\n\t"
                    "vmull.u16 q6, d11, d5\n\t"
                    "vshrn.i32 d25, q6, #8\n\t"
                    /* a */
                    "vmull.u16 q6, d10, d6\n\t"
                    "vshrn.i32 d26, q6, #8\n\t"
                    "vmull.u16 q6, d11, d7\n\t"
                    "vshrn.i32 d27, q6, #8\n\t"
                    /* if (!(Y->RGB.a & 0xf000)) */
                    "vand      q10, q4, q10\n\t"
                    "vand      q11, q4, q11\n\t"
                    "vand      q12, q4, q12\n\t"
                    "vand      q13, q4, q13\n\t"
                    /* if ((Y->RGB.a & 0xf000)) */
                    "vceq.i16  q4, q4, #0\n\t"
                    "vand      q0, q4, q0\n\t"
                    "vand      q1, q4, q1\n\t"
                    "vand      q2, q4, q2\n\t"
                    "vand      q3, q4, q3\n\t"
                    /* X: q0(b), q1(g), q2(r), q3(a) */
                    "vorr      q0, q0, q10\n\t"
                    "vorr      q1, q1, q11\n\t"
                    "vorr      q2, q2, q12\n\t"
                    "vorr      q3, q3, q13\n\t"
                    "vst4.16   {d0, d2, d4, d6}, [%[X]]!\n\t"
                    "vst4.16   {d1, d3, d5, d7}, [%[X]]!\n\t"
                    "subs      %[l], %[l], #1\n\t"
                    "bne       1b"
                    : /* no outputs */
                    : [X] "r" (X), [Y] "r" (Y), [Sa] "r" (Sa), [mask] "r" (mask), [l] "r" (l)
                    : "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "d10", "d11",
                      "d20", "d21", "d22", "d23", "d24", "d25", "d26", "d27"
               );
          }

          l = w & 0x7;
          while (l) {
               if (!(Y->RGB.a & 0xf000)) {
                    X->RGB.r = (Sa * Y->RGB.r) >> 8;
                    X->RGB.g = (Sa * Y->RGB.g) >> 8;
                    X->RGB.b = (Sa * Y->RGB.b) >> 8;
                    X->RGB.a = (Sa * Y->RGB.a) >> 8;
               }
               else
                    *X = *Y;

               ++X; ++Y;
               --l;
          }
     }
}

static void
Dacc_modulate_rgb_NEON( GenefxState *gfxs )
{
     int                l;
     int                w    = gfxs->length;
     GenefxAccumulator *D    = gfxs->Dacc;
     GenefxAccumulator  Cacc = gfxs->Cacc;
     u16                mask = 0xf000;

     l = w & 0x7;
     while (l) {
          if (!(D->RGB.a & 0xf000)) {
               D->RGB.r = (Cacc.RGB.r * D->RGB.r) >> 8;
               D->RGB.g = (Cacc.RGB.g * D->RGB.g) >> 8;
               D->RGB.b = (Cacc.RGB.b * D->RGB.b) >> 8;
          }

          ++D;
          --l;
     }

     l = w >> 3;
     if (l) {
          __asm__ __volatile__ (
               "mov       r4, %[D]\n\t"
               "mov       r5, %[D]\n\t"
               "vdup.16   q2, %[Cacc_r]\n\t"
               "vdup.16   q1, %[Cacc_g]\n\t"
               "vdup.16   q0, %[Cacc_b]\n\t"
               "vdup.16   q8, %[mask]\n\t"
               "1:\n\t"
               "pld       [r4, #0xC0]\n\t"
               "pld       [r4, #0x100]\n\t"
               "vld4.16   {d8, d10, d12, d14}, [r4]!\n\t"
               "vld4.16   {d9, d11, d13, d15}, [r4]!\n\t"
               "vand      q9, q7, q8\n\t"
               "vceq.i16  q9, q9, #0\n\t"
               /* b */
               "vmull.u16 q3, d8, d0\n\t"
               "vshrn.i32 d20, q3, #8\n\t"
               "vmull.u16 q3, d9, d1\n\t"
               "vshrn.i32 d21, q3, #8\n\t"
               /* g */
               "vmull.u16 q3, d10, d2\n\t"
               "vshrn.i32 d22, q3, #8\n\t"
               "vmull.u16 q3, d11, d3\n\t"
               "vshrn.i32 d23, q3, #8\n\t"
               /* r */
               "vmull.u16 q3, d12, d4\n\t"
               "vshrn.i32 d24, q3, #8\n\t"
               "vmull.u16 q3, d13, d5\n\t"
               "vshrn.i32 d25, q3, #8\n\t"
               /* if (!(D->RGB.a & 0xf000)) */
               "vand      q10, q9, q10\n\t"
               "vand      q11, q9, q11\n\t"
               "vand      q12, q9, q12\n\t"
               /* if ((D->RGB.a & 0xf000)) */
               "vceq.i16  q9, q9, #0\n\t"
               "vand      q4, q9, q4\n\t"
               "vand      q5, q9, q5\n\t"
               "vand      q6, q9, q6\n\t"
               /* D: q4(b), q5(g), q6(r), q7(a) */
               "vorr      q4, q4, q10\n\t"
               "vorr      q5, q5, q11\n\t"
               "vorr      q6, q6, q12\n\t"
               "vst4.16   {d8, d10, d12, d14}, [r5]!\n\t"
               "vst4.16   {d9, d11, d13, d15}, [r5]!\n\t"
               "subs      %[l], %[l], #1\n\t"
               "bne       1b"
               : /* no outputs */
               : [Cacc_r] "r" (Cacc.RGB.r), [Cacc_g] "r" (Cacc.RGB.g), [Cacc_b] "r" (Cacc.RGB.b),
                 [D] "r" (D), [mask] "r" (mask), [l] "r" (l)
               : "memory", "r4", "r5", "d0", "d1", "d2", "d3", "d4", "d5", "d8", "d9", "d10", "d11",
                 "d12", "d13", "d14", "d15", "d20", "d21", "d22", "d23", "d24", "d25"
          );
     }
}

static void
Dacc_modulate_argb_NEON( GenefxState *gfxs )
{
     int                l;
     int                w    = gfxs->length;
     GenefxAccumulator *D    = gfxs->Dacc;
     GenefxAccumulator  Cacc = gfxs->Cacc;
     u16                mask = 0xf000;

     l = w & 0x7;
     while (l) {
          if (!(D->RGB.a & 0xf000)) {
               D->RGB.a = (Cacc.RGB.a * D->RGB.a) >> 8;
               D->RGB.r = (Cacc.RGB.r * D->RGB.r) >> 8;
               D->RGB.g = (Cacc.RGB.g * D->RGB.g) >> 8;
               D->RGB.b = (Cacc.RGB.b * D->RGB.b) >> 8;
          }

          ++D;
          --l;
     }

     l = w >> 3;
     if (l) {
          __asm__ __volatile__ (
               "mov       r4, %[D]\n\t"
               "mov       r5, %[D]\n\t"
               "vdup.16   q3, %[Cacc_a]\n\t"
               "vdup.16   q2, %[Cacc_r]\n\t"
               "vdup.16   q1, %[Cacc_g]\n\t"
               "vdup.16   q0, %[Cacc_b]\n\t"
               "vdup.16   q8, %[mask]\n\t"
               "1:\n\t"
               "pld       [r4, #0xC0]\n\t"
               "pld       [r4, #0x100]\n\t"
               "vld4.16   {d8, d10, d12, d14}, [r4]!\n\t"
               "vld4.16   {d9, d11, d13, d15}, [r4]!\n\t"
               "vand      q9, q7, q8\n\t"
               "vceq.i16  q9, q9, #0\n\t"
               /* b */
               "vmull.u16 q14, d8, d0\n\t"
               "vshrn.i32 d20, q14, #8\n\t"
               "vmull.u16 q14, d9, d1\n\t"
               "vshrn.i32 d21, q14, #8\n\t"
               /* g */
               "vmull.u16 q14, d10, d2\n\t"
               "vshrn.i32 d22, q14, #8\n\t"
               "vmull.u16 q14, d11, d3\n\t"
               "vshrn.i32 d23, q14, #8\n\t"
               /* r */
               "vmull.u16 q14, d12, d4\n\t"
               "vshrn.i32 d24, q14, #8\n\t"
               "vmull.u16 q14, d13, d5\n\t"
               "vshrn.i32 d25, q14, #8\n\t"
               /* a */
               "vmull.u16 q14, d14, d6\n\t"
               "vshrn.i32 d26, q14, #8\n\t"
               "vmull.u16 q14, d15, d7\n\t"
               "vshrn.i32 d27, q14, #8\n\t"
               /* if (!(D->RGB.a & 0xf000)) */
               "vand      q10, q9, q10\n\t"
               "vand      q11, q9, q11\n\t"
               "vand      q12, q9, q12\n\t"
               "vand      q13, q9, q13\n\t"
               /* if ((D->RGB.a & 0xf000)) */
               "vceq.i16  q9, q9, #0\n\t"
               "vand      q4, q9, q4\n\t"
               "vand      q5, q9, q5\n\t"
               "vand      q6, q9, q6\n\t"
               "vand      q7, q9, q7\n\t"
               /* D: q4(b), q5(g), q6(r), q7(a) */
               "vorr      q4, q4, q10\n\t"
               "vorr      q5, q5, q11\n\t"
               "vorr      q6, q6, q12\n\t"
               "vorr      q7, q7, q13\n\t"
               "vst4.16   {d8, d10, d12, d14}, [r5]!\n\t"
               "vst4.16   {d9, d11, d13, d15}, [r5]!\n\t"
               "subs      %[l], %[l], #1\n\t"
               "bne       1b"
               : /* no outputs */
               : [Cacc_a] "r" (Cacc.RGB.a),
                 [Cacc_r] "r" (Cacc.RGB.r), [Cacc_g] "r" (Cacc.RGB.g), [Cacc_b] "r" (Cacc.RGB.b),
                 [D] "r" (D), [mask] "r" (mask), [l] "r" (l)
               : "memory", "r4", "r5", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "d8", "d9", "d10", "d11",
                 "d12", "d13", "d14", "d15", "d20", "d21", "d22", "d23", "d24", "d25", "d26", "d27"
          );
     }
}

static void
SCacc_add_to_Dacc_NEON( GenefxState *gfxs )
{
     int                l;
     int                w     = gfxs->length;
     GenefxAccumulator *D     = gfxs->Dacc;
     GenefxAccumulator  SCacc = gfxs->SCacc;
     u16                mask  = 0xf000;

     l = w & 0x7;
     while (l) {
          if (!(D->RGB.a & 0xf000)) {
               D->RGB.a += SCacc.RGB.a;
               D->RGB.r += SCacc.RGB.r;
               D->RGB.g += SCacc.RGB.g;
               D->RGB.b += SCacc.RGB.b;
          }

          ++D;
          --l;
     }

     l = w >> 3;
     if (l) {
          __asm__ __volatile__ (
               "mov       r4, %[D]\n\t"
               "mov       r5, %[D]\n\t"
               "vdup.16   q3, %[SCacc_a]\n\t"
               "vdup.16   q2, %[SCacc_r]\n\t"
               "vdup.16   q1, %[SCacc_g]\n\t"
               "vdup.16   q0, %[SCacc_b]\n\t"
               "vdup.16   q8, %[mask]\n\t"
               "1:\n\t"
               "pld       [r4, #0xC0]\n\t"
               "pld       [r4, #0x100]\n\t"
               "vld4.16   {d8, d10, d12, d14}, [r4]!\n\t"
               "vld4.16   {d9, d11, d13, d15}, [r4]!\n\t"
               "vand      q9, q7, q8\n\t"
               "vceq.i16  q9, q9, #0\n\t"
               "vand      q10, q9, q0\n\t"
               "vand      q11, q9, q1\n\t"
               "vand      q12, q9, q2\n\t"
               "vand      q13, q9, q3\n\t"
               "vadd.i16  q4, q4, q10\n\t"
               "vadd.i16  q5, q5, q11\n\t"
               "vadd.i16  q6, q6, q12\n\t"
               "vadd.i16  q7, q7, q13\n\t"
               "vst4.16   {d8, d10, d12, d14}, [r5]!\n\t"
               "vst4.16   {d9, d11, d13, d15}, [r5]!\n\t"
               "subs      %[l], %[l], #1\n\t"
               "bne       1b"
               : /* no outputs */
               : [SCacc_a] "r" (gfxs->SCacc.RGB.a),
                 [SCacc_r] "r" (gfxs->SCacc.RGB.r), [SCacc_g] "r" (gfxs->SCacc.RGB.g), [SCacc_b] "r" (gfxs->SCacc.RGB.b),
                 [D] "r" (D), [mask] "r" (mask), [l] "r" (l)
               : "memory", "r4", "r5", "d8", "d9", "d10", "d11", "d12", "d13", "d14", "d15"
          );
     }
}

static void
Sacc_add_to_Dacc_NEON( GenefxState *gfxs )
{
     int                l;
     int                w    = gfxs->length;
     GenefxAccumulator *D    = gfxs->Dacc;
     GenefxAccumulator *S    = gfxs->Sacc;
     u16                mask = 0xf000;

     l = w & 0x7;
     while (l) {
          if (!(D->RGB.a & 0xf000)) {
               D->RGB.a += S->RGB.a;
               D->RGB.r += S->RGB.r;
               D->RGB.g += S->RGB.g;
               D->RGB.b += S->RGB.b;
          }

          ++S;
          ++D;
          --l;
     }

     l = w >> 3;
     if (l) {
          __asm__ __volatile__ (
               "mov       r4, %[D]\n\t"
               "mov       r5, %[S]\n\t"
               "mov       r6, %[D]\n\t"
               "vdup.16   q8, %[mask]\n\t"
               "1:\n\t"
               "pld       [r4, #0xC0]\n\t"
               "pld       [r4, #0x100]\n\t"
               "vld4.16   {d8, d10, d12, d14}, [r4]!\n\t"
               "vld4.16   {d9, d11, d13, d15}, [r4]!\n\t"
               "pld       [r5, #0xC0]\n\t"
               "pld       [r5, #0x100]\n\t"
               "vld4.16   {d0, d2, d4, d6}, [r5]!\n\t"
               "vld4.16   {d1, d3, d5, d7}, [r5]!\n\t"
               "vand      q9, q7, q8\n\t"
               "vceq.i16  q9, q9, #0\n\t"
               "vand      q0, q9, q0\n\t"
               "vand      q1, q9, q1\n\t"
               "vand      q2, q9, q2\n\t"
               "vand      q3, q9, q3\n\t"
               "vadd.i16  q4, q4, q0\n\t"
               "vadd.i16  q5, q5, q1\n\t"
               "vadd.i16  q6, q6, q2\n\t"
               "vadd.i16  q7, q7, q3\n\t"
               "vst4.16   {d8, d10, d12, d14}, [r6]!\n\t"
               "vst4.16   {d9, d11, d13, d15}, [r6]!\n\t"
               "subs      %[l], %[l], #1\n\t"
               "bne       1b"
               : /* no outputs */
               : [S] "r" (S), [D] "r" (D), [mask] "r" (mask), [l] "r" (l)
               : "memory", "r4", "r5", "r6", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "d8", "d9", "d10", "d11",
                 "d12", "d13", "d14", "d15"
          );
     }
}

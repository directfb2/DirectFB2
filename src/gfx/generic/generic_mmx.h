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
Xacc_blend_srcalpha_MMX( GenefxState *gfxs )
{
     static const u32 ones[]  = { 0x00010001, 0x00010001 };
     static const u32 zeros[] = { 0, 0 };

     __asm__ __volatile__ (
          "movq      %3, %%mm7\n\t"
          "cmp       $0, %2\n\t"
          "jne       3f\n\t"
          "movq      %4, %%mm6\n\t"
          "movd      %5, %%mm0\n\t"
          "punpcklbw %%mm6, %%mm0\n\t"   /* mm0 = 00aa 00rr 00gg 00bb */
          "punpcklwd %%mm0, %%mm0\n\t"   /* mm0 = 00aa 00aa xxxx xxxx */
          "punpckldq %%mm0, %%mm0\n\t"   /* mm0 = 00aa 00aa 00aa 00aa */
          "paddw     %%mm7, %%mm0\n"
          ".align    16\n"
          "4:\n\t"                       /* blend from color */
          "testw     $0xf000, 6(%0)\n\t"
          "jnz       6f\n\t"
          "movq      (%0), %%mm1\n\t"
          "pmullw    %%mm0, %%mm1\n\t"
          "psrlw     $8, %%mm1\n\t"
          "movq      %%mm1, (%6)\n\t"
          "jmp       1f\n"
          "6:\n\t"
          "movq      (%0), %%mm1\n\t"
          "movq      %%mm1, (%6)\n"
          "1:\n\t"
          "add       $8, %0\n\t"
          "add       $8, %6\n\t"
          "dec       %1\n\t"
          "jnz       4b\n\t"
          "jmp       2f\n"
          ".align    16\n"
          "3:\n\t"                       /* blend from Sacc */
          "testw     $0xf000, 6(%0)\n\t"
          "jnz       5f\n\t"
          "movq      (%2), %%mm0\n\t"
          "movq      (%0), %%mm1\n\t"
          "punpckhwd %%mm0, %%mm0\n\t"   /* mm2 = 00aa 00aa xxxx xxxx */
          "punpckhdq %%mm0, %%mm0\n\t"   /* mm2 = 00aa 00aa 00aa 00aa */
          "paddw     %%mm7, %%mm0\n\t"
          "pmullw    %%mm0, %%mm1\n\t"
          "psrlw     $8, %%mm1\n\t"
          "movq      %%mm1, (%6)\n\t"
          "jmp       7f\n"
          "5:\n\t"
          "movq      (%0), %%mm1\n\t"
          "movq      %%mm1, (%6)\n"
          "7:\n\t"
          "add       $8, %2\n\t"
          "add       $8, %0\n\t"
          "add       $8, %6\n\t"
          "dec       %1\n\t"
          "jnz       3b\n"
          "2:\n\t"
          "emms"
          : /* no outputs */
          : "D" (gfxs->Yacc), "c" (gfxs->length), "S" (gfxs->Sacc),
            "m" (*ones), "m" (*zeros), "m" (gfxs->color), "r" (gfxs->Xacc)
          : "%st", "memory");
}

static void
Xacc_blend_invsrcalpha_MMX( GenefxState *gfxs )
{
     static const u32 ones[]  = { 0x01000100, 0x01000100 };
     static const u32 zeros[] = { 0, 0 };

     __asm__ __volatile__ (
          "movq      %3, %%mm7\n\t"
          "cmp       $0, %2\n\t"
          "jne       1f\n\t"
          "movq      %4, %%mm6\n\t"
          "movd      %5, %%mm0\n\t"
          "punpcklbw %%mm6, %%mm0\n\t"   /* mm0 = 00aa 00rr 00gg 00bb */
          "punpcklwd %%mm0, %%mm0\n\t"   /* mm0 = 00aa 00aa xxxx xxxx */
          "movq      %%mm7, %%mm1\n\t"
          "punpckldq %%mm0, %%mm0\n\t"   /* mm0 = 00aa 00aa 00aa 00aa */
          "psubw     %%mm0, %%mm1\n"
          ".align    16\n"
          "2:\n\t"                       /* blend from color */
          "testw     $0xf000, 6(%0)\n\t"
          "jnz       3f\n\t"
          "movq      (%0), %%mm0\n\t"
          "pmullw    %%mm1, %%mm0\n\t"
          "psrlw     $8, %%mm0\n\t"
          "movq      %%mm0, (%6)\n\t"
          "jmp       4f\n"
          "3:\n\t"
          "movq      (%0), %%mm0\n\t"
          "movq      %%mm0, (%6)\n"
          "4:\n\t"
          "add       $8, %0\n\t"
          "add       $8, %6\n\t"
          "dec       %1\n\t"
          "jnz       2b\n\t"
          "jmp       9f\n"
          ".align    16\n"
          "1:\n\t"                       /* blend from Sacc */
          "testw     $0xf000, 6(%0)\n\t"
          "jnz       5f\n\t"
          "movq      (%2), %%mm2\n\t"
          "movq      (%0), %%mm0\n\t"
          "punpckhwd %%mm2, %%mm2\n\t"   /* mm2 = 00aa 00aa xxxx xxxx */
          "movq      %%mm7, %%mm1\n\t"
          "punpckhdq %%mm2, %%mm2\n\t"   /* mm2 = 00aa 00aa 00aa 00aa */
          "psubw     %%mm2, %%mm1\n\t"
          "pmullw    %%mm1, %%mm0\n\t"
          "psrlw     $8, %%mm0\n\t"
          "movq      %%mm0, (%6)\n\t"
          "jmp       6f\n"
          "5:\n\t"
          "movq      (%0), %%mm0\n\t"
          "movq      %%mm0, (%6)\n"
          "6:\n\t"
          "add       $8, %2\n\t"
          "add       $8, %0\n\t"
          "add       $8, %6\n\t"
          "dec       %1\n\t"
          "jnz       1b\n"
          "9:\n\t"
          "emms"
          : /* no outputs */
          : "D" (gfxs->Yacc), "c" (gfxs->length), "S" (gfxs->Sacc),
            "m" (*ones), "m" (*zeros), "m" (gfxs->color), "r" (gfxs->Xacc)
          : "%st", "memory");
}

static void
Dacc_modulate_argb_MMX( GenefxState *gfxs )
{
     __asm__ __volatile__ (
          "movq      %2, %%mm0\n"
          ".align    16\n"
          "1:\n\t"
          "testw     $0xf000, 6(%0)\n\t"
          "jnz       2f\n\t"
          "movq      (%0), %%mm1\n\t"
          "pmullw    %%mm0, %%mm1\n\t"
          "psrlw     $8, %%mm1\n\t"
          "movq      %%mm1, (%0)\n"
          ".align    16\n"
          "2:\n\t"
          "add       $8, %0\n\t"
          "dec       %1\n\t"
          "jnz       1b\n\t"
          "emms"
          : /* no outputs */
          : "D" (gfxs->Dacc), "c" (gfxs->length), "m" (gfxs->Cacc)
          : "%st", "memory");
}

static void
SCacc_add_to_Dacc_MMX( GenefxState *gfxs )
{
     __asm__ __volatile__ (
          "movq      %2, %%mm0\n"
          ".align    16\n"
          "1:\n\t"
          "movq      (%0), %%mm1\n\t"
          "paddw     %%mm0, %%mm1\n\t"
          "movq      %%mm1, (%0)\n\t"
          "add       $8, %0\n\t"
          "dec       %1\n\t"
          "jnz       1b\n\t"
          "emms"
          : /* no outputs */
          : "D" (gfxs->Dacc), "c" (gfxs->length), "m" (gfxs->SCacc)
          : "%st", "memory");
}

static void
Sacc_add_to_Dacc_MMX( GenefxState *gfxs )
{
     __asm__ __volatile__ (
          ".align    16\n"
          "1:\n\t"
          "movq      (%2), %%mm0\n\t"
          "movq      (%0), %%mm1\n\t"
          "paddw     %%mm1, %%mm0\n\t"
          "movq      %%mm0, (%0)\n\t"
          "add       $8, %0\n\t"
          "add       $8, %2\n\t"
          "dec       %1\n\t"
          "jnz       1b\n\t"
          "emms"
          : /* no outputs */
          : "D" (gfxs->Dacc), "c" (gfxs->length), "S" (gfxs->Sacc)
          : "%st", "memory");
}

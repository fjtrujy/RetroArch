/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */


#include <retro_miscellaneous.h>
#include <kernel.h>
#include <gsKit.h>
#include <gsInline.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../gfx_display.h"

#include "../../retroarch.h"
#include "../font_driver.h"

static void *gfx_display_ps2_get_default_mvp(void *data)
{
   ps2_video_t *ps2 = (ps2_video_t*)data;

   if (!ps2)
      return NULL;

   return &ps2->vp;
}

static void gfx_display_ps2_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_width, unsigned video_height)
{
   int colorR, colorG, colorB, colorA;
   unsigned i;
   GSTEXTURE *texture   = NULL;
   const float *vertex              = NULL;
   const float *tex_coord           = NULL;
   const float *color               = NULL;
   ps2_video_t             *ps2 = (ps2_video_t*)data;

   //  printf("%s:\n", __FUNCTION__);
   //  printf("- dest:    %dx%d @ x=%.1f, y=%.1f\n", draw->width, draw->height, draw->x, draw->y);

   if (!ps2 || !draw || draw->x < 0 || draw->y < 0)
      return;

   if (draw->width > ps2->gsGlobal->Width)
    draw->width > ps2->gsGlobal->Width;

   if (draw->height > ps2->gsGlobal->Height)
    draw->height = ps2->gsGlobal->Height;

   texture            = (GSTEXTURE*)draw->texture;
   vertex             = draw->coords->vertex;
   tex_coord          = draw->coords->tex_coord;
   color              = draw->coords->color;

   if (!texture)
      return;

   //  printf("- texture: %dx%d\n", texture->Width, texture->Height);

   colorR = (int)((color[0])*128.f);
   colorG = (int)((color[1])*128.f);
   colorB = (int)((color[2])*128.f);
   // 255 == 2.0
   // 128 == 1.0
   //  64 == 0.5
   // The texture uses alpha 255, so by multiplying with 64 in the "texture function"
   // the result becomes 128.
   colorA = (int)((color[3])* 64.f);

   //  printf("- colorf:  %.2f-%.2f-%.2f-%.2f\n", color[0], color[1], color[2], color[3]);
   //  printf("- colori:  0x%02x-0x%02x-0x%02x-0x%02x\n", colorR, colorG, colorB, colorA);

/*
 * Color calculation:
 *
 * 1. The texture function:
 * - Texture function = MODULATE, this is fixed in gsKit (other functions not supported!!!)
 * - TCC flag (Texture Color Component) must be set to use the alpha value of the texture:
 *   - gsGlobal->PrimAlphaEnable = GS_SETTING_ON
 * - Texture  colors: (Rt, Gt, Bt, At), taken from texture
 * - Fragment colors: (Rf, Gf, Bf, Af), taken from draw->coords->color
 * - Output   colors: (Rv, Gv, Bv, Av)
 * - MODULATE function:
 *   - Rv = Rt * Rf
 *   - Gv = Gt * Gf
 *   - Bv = Bt * Bf
 *   - Av = At * Af
 *   - When the fragment colors are 0x80 (128), the output colors dont change
 *
 * 2. Alpha blending:
 * - gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);
 *   - A = 0 = Cs (RGB value of the source is used)
 *   - B = 1 = Cd (RGB value in the frame buffer is used.)
 *   - C = 0 = As (Alpha of the source is used.)
 *   - D = 1 = Cd (RGB value in the frame buffer is used.)
 *   - FIX = not used
 * - (A - B) * C + D            NOTE: X * Y = (X x Y) >> 7
 * - (Cs - Cd) * As + Cd
 */

    //if (texture->Width > 1 || texture->Height > 1) {
        // This is a texture
        gsKit_TexManager_bind(ps2->gsGlobal, texture);
        gsKit_prim_sprite_texture(ps2->gsGlobal, texture,
            draw->x,                /* X1 */
            ps2->gsGlobal->Height - draw->y,                /* Y1 */
            0,                      /* U1 */
            texture->Height,                      /* V1 */
            draw->x + draw->width,  /* X2 */
            ps2->gsGlobal->Height - (draw->y + draw->height), /* Y2 */
            texture->Width,         /* U2 */
            0,        /* V2 */
            4,                      /* Z  */
            GS_SETREG_RGBAQ(colorR,colorG,colorB,colorA,0x00));
    //}
    //else {
        // This is not a texture, its a colored rectangle
        // Draw faster using a quad
        // NOTE: do we need to multiply the color by the 1 pixel texture color?
    //    gsKit_prim_sprite(ps2->gsGlobal,
    //        draw->x,                /* X1 */
    //        draw->y,                /* Y1 */
    //        draw->x + draw->width,  /* X3 */
    //        draw->y + draw->height, /* Y3 */
    //        4,                      /* Z  */
    //        GS_SETREG_RGBAQ(colorR,colorG,colorB,colorA,0x00));
    //}
}

static bool gfx_display_ps2_font_init_first(
      void **font_handle, void *video_data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   font_data_t **handle = (font_data_t**)font_handle;

   //  printf("%s\n", __FUNCTION__);

   *handle              = font_driver_init_first(video_data,
         font_path, font_size, true,
         is_threaded,
         FONT_DRIVER_RENDER_PS2);
   return *handle;
}

gfx_display_ctx_driver_t gfx_display_ctx_ps2 = {
   gfx_display_ps2_draw,
   NULL,                                        /* draw_pipeline */
   NULL,                                        /* blend_begin   */
   NULL,                                        /* blend_end     */
   NULL,
   NULL,
   NULL,
   gfx_display_ps2_font_init_first,
   GFX_VIDEO_DRIVER_PS2,
   "ps2",
   true,
   NULL,
   NULL
};

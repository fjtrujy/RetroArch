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

// static void set_texture(GSTEXTURE *texture, const void *frame,
//       int width, int height, int PSM, int filter)
// {
//    texture->Width  = width;
//    texture->Height = height;
//    texture->PSM    = PSM;
//    texture->Filter = filter;
//    texture->Mem    = (void *)frame;
// }










static const float ps2_vertexes[] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const float ps2_tex_coords[] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0
};

static const float ps2_colors[] = {
   1.0f, 1.0f, 1.0f, 1.0f,
   1.0f, 1.0f, 1.0f, 1.0f,
   1.0f, 1.0f, 1.0f, 1.0f,
   1.0f, 1.0f, 1.0f, 1.0f,
};

static const float *gfx_display_ps2_get_default_vertices(void)
{
   printf("FJTRUJY: %s:%i\n", __FILE__, __LINE__);
   return &ps2_vertexes[0];
}

static const float *gfx_display_ps2_get_default_color(void)
{
   printf("FJTRUJY: %s:%i\n", __FILE__, __LINE__);
   return &ps2_colors[0];
}

static const float *gfx_display_ps2_get_default_tex_coords(void)
{
   printf("FJTRUJY: %s:%i\n", __FILE__, __LINE__);
   return &ps2_tex_coords[0];
}

static void *gfx_display_ps2_get_default_mvp(void *data)
{
   printf("FJTRUJY: %s:%i\n", __FILE__, __LINE__);
   ps2_video_t *ps2 = (ps2_video_t*)data;

   if (!ps2)
      return NULL;

   return NULL;
   // return &ps2->mvp_no_rot;
}

static void gfx_display_ps2_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_width, unsigned video_height)
{
   printf("FJTRUJY: %s:%i\n", __FILE__, __LINE__);

   unsigned i;
   // struct vita2d_texture *texture   = NULL;
   const float *vertex              = NULL;
   const float *tex_coord           = NULL;
   const float *color               = NULL;
   ps2_video_t             *ps2 = (ps2_video_t*)data;

   if (!ps2 || !draw)
      return;

   // texture            = (struct vita2d_texture*)draw->texture;
   vertex             = draw->coords->vertex;
   tex_coord          = draw->coords->tex_coord;
   color              = draw->coords->color;

   set_texture(ps2->displayTexture, draw->texture, draw->width, draw->height, GS_PSM_CT16, 0);
   gsKit_TexManager_invalidate(ps2->gsGlobal, ps2->displayTexture);
   gsKit_TexManager_bind(ps2->gsGlobal, ps2->displayTexture);
   
   gsKit_prim_sprite_texture(ps2->gsGlobal, draw->texture,
         0,            /* X1 */
         0,            /* Y1 */
         0,  /* U1 */
         0,   /* V1 */
         ps2->gsGlobal->Width,            /* X2 */
         ps2->gsGlobal->Height,            /* Y2 */
         draw->width,   /* U2 */
         draw->height, /* V2 */
         4,
         GS_TEXT);

}

static bool gfx_display_ps2_font_init_first(
      void **font_handle, void *video_data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   printf("FJTRUJY: %s:%i\n", __FILE__, __LINE__);
   font_data_t **handle = (font_data_t**)font_handle;
   *handle              = font_driver_init_first(video_data,
         font_path, font_size, true,
         is_threaded,
         FONT_DRIVER_RENDER_PS2);
   return *handle;
}

static void gfx_display_ps2_scissor_begin(void *data,
      unsigned video_width,
      unsigned video_height,
      int x, int y,
      unsigned width, unsigned height)
{
    printf("FJTRUJY: %s:%i\n", __FILE__, __LINE__);
   // ps2_set_clip_rectangle(x, y, x + width, y + height);  
   // ps2_set_region_clip(SCE_GXM_REGION_CLIP_OUTSIDE, x, y, x + width, y + height);
}

static void gfx_display_ps2_scissor_end(
      void *data,
      unsigned video_width,
      unsigned video_height)
{
    printf("FJTRUJY: %s:%i\n", __FILE__, __LINE__);
   // ps2_set_region_clip(SCE_GXM_REGION_CLIP_NONE, 0, 0,
   //       video_width, video_height);
   // ps2_disable_clipping();
}

gfx_display_ctx_driver_t gfx_display_ctx_ps2 = {
   gfx_display_ps2_draw,
   NULL,                                        /* draw_pipeline */
   NULL,                                        /* blend_begin   */
   NULL,                                        /* blend_end     */
   gfx_display_ps2_get_default_mvp,
   gfx_display_ps2_get_default_vertices,
   gfx_display_ps2_get_default_tex_coords,
   gfx_display_ps2_font_init_first,
   GFX_VIDEO_DRIVER_PS2,
   "ps2",
   true,
   gfx_display_ps2_scissor_begin,
   gfx_display_ps2_scissor_end
};

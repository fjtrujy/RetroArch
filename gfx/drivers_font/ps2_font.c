/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2019 - Francisco Javier Trujillo Mata
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

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <gsToolkit.h>

#include "../font_driver.h"

#define FONTM_TEXTURE_COLOR         GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0x00)
#define FONTM_TEXTURE_SCALED        64.0f
#define FONTM_TEXTURE_LEFT_MARGIN   0
#define FONTM_TEXTURE_BOTTOM_MARGIN 15
#define FONTM_TEXTURE_ZPOSITION     3

typedef struct ps2_font_info
{
   ps2_video_t *ps2_video;
   GSFONTM *gsFontM;
} ps2_font_info_t;

static void *ps2_font_init_font(void *gl_data, const char *font_path,
      float font_size, bool is_threaded)
{
   ps2_font_info_t *ps2 = (ps2_font_info_t*)calloc(1, sizeof(ps2_font_info_t));
   ps2->ps2_video = (ps2_video_t *)gl_data;
   ps2->gsFontM = gsKit_init_fontm();

   gsKit_fontm_upload(ps2->ps2_video->gsGlobal, ps2->gsFontM);

   return ps2;
}

static void ps2_font_free_font(void *data, bool is_threaded)
{
   ps2_font_info_t *ps2 = (ps2_font_info_t *)data;
   gsKit_free_fontm(ps2->ps2_video->gsGlobal, ps2->gsFontM);
   ps2->ps2_video = NULL;
   
   free(ps2);
   ps2 = NULL;
}

// static void vita2d_font_render_line(
//       vita_font_t *font, const char *msg, unsigned msg_len,
//       float scale, const unsigned int color, float pos_x,
//       float pos_y,
//       unsigned width, unsigned height, unsigned text_align)
// {
//    unsigned i;
//    int x           = roundf(pos_x * width);
//    int y           = roundf((1.0f - pos_y) * height);
//    int delta_x     = 0;
//    int delta_y     = 0;

//    switch (text_align)
//    {
//       case TEXT_ALIGN_RIGHT:
//          x -= vita2d_font_get_message_width(font, msg, msg_len, scale);
//          break;
//       case TEXT_ALIGN_CENTER:
//          x -= vita2d_font_get_message_width(font, msg, msg_len, scale) / 2;
//          break;
//    }

//    for (i = 0; i < msg_len; i++)
//    {
//       int off_x, off_y, tex_x, tex_y, width, height;
//       unsigned int stride, pitch, j, k;
//       const struct font_glyph *glyph = NULL;
//       const uint8_t         *frame32 = NULL;
//       uint8_t                 *tex32 = NULL;
//       const char *msg_tmp            = &msg[i];
//       unsigned code                  = utf8_walk(&msg_tmp);
//       unsigned skip                  = msg_tmp - &msg[i];

//       if (skip > 1)
//          i += skip - 1;

//       glyph = font->font_driver->get_glyph(font->font_data, code);

//       if (!glyph) /* Do something smarter here ... */
//          glyph = font->font_driver->get_glyph(font->font_data, '?');

//       if (!glyph)
//          continue;

//       off_x  = glyph->draw_offset_x;
//       off_y  = glyph->draw_offset_y;
//       tex_x  = glyph->atlas_offset_x;
//       tex_y  = glyph->atlas_offset_y;
//       width  = glyph->width;
//       height = glyph->height;

//       if (font->atlas->dirty)
//       {
//         stride  = vita2d_texture_get_stride(font->texture);
//         tex32   = vita2d_texture_get_datap(font->texture);
//         frame32 = font->atlas->buffer;
//         pitch   = font->atlas->width;

//         for (j = 0; j < font->atlas->height; j++)
//            for (k = 0; k < font->atlas->width; k++)
//               tex32[k + j*stride] = frame32[k + j*pitch];

//          font->atlas->dirty = false;
//       }

//       vita2d_draw_texture_tint_part_scale(font->texture,
//             x + (off_x + delta_x) * scale,
//             y + (off_y + delta_y) * scale,
//             tex_x, tex_y, width, height,
//             scale,
//             scale,
//             color);

//       delta_x += glyph->advance_x;
//       delta_y += glyph->advance_y;
//    }
// }

// static void ps2_font_render_message(
//       ps2_font_t *font, const char *msg, float scale,
//       const unsigned int color, float pos_x, float pos_y,
//       unsigned width, unsigned height, unsigned text_align)
// {
//    struct font_line_metrics *line_metrics = NULL;
//    int lines                              = 0;
//    float line_height;

//    if (!msg || !*msg)
//       return;

//    /* If font line metrics are not supported just draw as usual */
//    if (!font->font_driver->get_line_metrics ||
//        !font->font_driver->get_line_metrics(font->font_data, &line_metrics))
//    {
//       vita2d_font_render_line(font, msg, strlen(msg),
//             scale, color, pos_x, pos_y, width, height, text_align);
//       return;
//    }

//    line_height = line_metrics->height * scale / font->vita->vp.height;

//    for (;;)
//    {
//       const char *delim = strchr(msg, '\n');
//       unsigned msg_len  = (delim) ? 
//          (unsigned)(delim - msg) : strlen(msg);

//       /* Draw the line */
//       vita2d_font_render_line(font, msg, msg_len,
//             scale, color, pos_x, pos_y - (float)lines * line_height,
//             width, height, text_align);

//       if (!delim)
//          break;

//       msg += msg_len + 1;
//       lines++;
//    }
// }

static void ps2_font_render_msg(
      void *userdata,
      void *data, const char *msg,
      const struct font_params *params)
{
   float x, y, font_size, scale, drop_mod, drop_alpha;
   int drop_x, drop_y;
   unsigned max_glyphs;
   enum text_alignment text_align;
   bool full_screen                 = false ;
   unsigned color, color_dark, r, g, b,
            alpha, r_dark, g_dark, b_dark, alpha_dark;
   
   settings_t *settings             = config_get_ptr();
   float video_msg_pos_x            = settings->floats.video_msg_pos_x;
   float video_msg_pos_y            = settings->floats.video_msg_pos_y;
   float video_msg_color_r          = settings->floats.video_msg_color_r;
   float video_msg_color_g          = settings->floats.video_msg_color_g;
   float video_msg_color_b          = settings->floats.video_msg_color_b;
   float video_msg_font_size        = settings->floats.video_font_size;

   ps2_video_t *ps2                 = (ps2_video_t *)userdata;
   ps2_font_info_t *font            = (ps2_font_info_t *)data;
   unsigned width                   = ps2->gsGlobal->Width;
   unsigned height                  = ps2->gsGlobal->Height;

   if (!font || !msg || !*msg)
      return;
   
   if (params)
   {
      x              = params->x;
      y              = params->y;
      scale          = params->scale;
      full_screen    = params->full_screen;
      text_align     = params->text_align;
      drop_x         = params->drop_x;
      drop_y         = params->drop_y;
      drop_mod       = params->drop_mod;
      drop_alpha     = params->drop_alpha;
      r    				= FONT_COLOR_GET_RED(params->color);
      g    				= FONT_COLOR_GET_GREEN(params->color);
      b    				= FONT_COLOR_GET_BLUE(params->color);
      alpha    		= FONT_COLOR_GET_ALPHA(params->color);
      color    		= GS_SETREG_RGBAQ(r,g,b,alpha,0x00);
   }
   else
   {
      x              = video_msg_pos_x;
      y              = video_msg_pos_y;
      scale          = 1.0f;
      full_screen    = true;
      text_align     = TEXT_ALIGN_LEFT;

      r              = (video_msg_color_r * 128);
      g              = (video_msg_color_g * 128);
      b              = (video_msg_color_b * 128);
      alpha			   = 128;
      color 		   = GS_SETREG_RGBAQ(r,g,b,alpha,0x00);

      drop_x         = -2;
      drop_y         = -2;
      drop_mod       = 0.3f;
      drop_alpha     = 1.0f;
   }

   video_driver_set_viewport(width, height, full_screen, false);

   max_glyphs        = strlen(msg);

   scale = scale * video_msg_font_size / FONTM_TEXTURE_SCALED;
   x = font->ps2_video->gsGlobal->Width * x;
   y = font->ps2_video->gsGlobal->Height - (font->ps2_video->gsGlobal->Height * y);

   if (drop_x || drop_y)
      max_glyphs    *= 2;

   if (drop_x || drop_y)
   {
      r_dark         = r * drop_mod;
      g_dark         = g * drop_mod;
      b_dark         = b * drop_mod;
      alpha_dark		= alpha * drop_alpha;
      color_dark     = GS_SETREG_RGBAQ(r_dark,g_dark,b_dark,alpha_dark,0x00);

      gsKit_fontm_print_scaled(
         font->ps2_video->gsGlobal, 
         font->gsFontM, x, y, FONTM_TEXTURE_ZPOSITION,
         scale, color, msg);
   }

   gsKit_fontm_print_scaled(
         font->ps2_video->gsGlobal, 
         font->gsFontM, x, y, FONTM_TEXTURE_ZPOSITION,
         scale, color, msg);   
}

font_renderer_t ps2_font = {
   ps2_font_init_font,
   ps2_font_free_font,
   ps2_font_render_msg,
   "PS2 font",
   NULL,                      /* get_glyph */
   NULL,                      /* bind_block */
   NULL,                      /* flush */
   NULL,                      /* get_message_width */
   NULL                       /* get_line_metrics */
};

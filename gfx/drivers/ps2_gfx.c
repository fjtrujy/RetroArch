/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018 - Francisco Javier Trujillo Mata - fjtrujy
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

#include <kernel.h>
#include <gsKit.h>
#include <gsInline.h>

#include "../../driver.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

#include "../../libretro-common/include/libretro_gskit_ps2.h"

/* turn white GS Screen */
#define GS_TEXT GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0x00)
/* turn black GS Screen */
#define GS_BLACK GS_SETREG_RGBAQ(0x00,0x00,0x00,0x00,0x00)

#define NTSC_WIDTH  640
#define NTSC_HEIGHT 448
#define HIRES_MODE

typedef struct ps2_video
{
   /* I need to create this additional field
    * to be used in the font driver*/
   bool clearVRAM_font;
   bool menuVisible;
   bool fullscreen;
   bool vsync;
   int vsync_callback_id;
   bool force_aspect;
   bool msg_rendering_enabled;

   int PSM;
   int menu_filter;
   int core_filter;

   video_viewport_t vp;

   /* Palette in the cores */
   struct retro_hw_render_interface_gskit_ps2 iface;

   GSGLOBAL *gsGlobal;
   GSTEXTURE *menuTexture;
   GSTEXTURE *coreTexture;
   GSTEXTURE *displayTexture;
} ps2_video_t;

#ifndef HIRES_MODE
static int vsync_sema_id;

/* PRIVATE METHODS */
static int vsync_handler()
{
   iSignalSema(vsync_sema_id);

   ExitHandler();
   return 0;
}

// Copy of gsKit_sync_flip, but without the 'flip'
static void gsKit_sync(GSGLOBAL *gsGlobal)
{
   if(!gsGlobal->FirstFrame)
      WaitSema(vsync_sema_id);

      while (PollSema(vsync_sema_id) >= 0)
         ;
}

// Copy of gsKit_sync_flip, but without the 'sync'
static void gsKit_flip(GSGLOBAL *gsGlobal)
{
   if(!gsGlobal->FirstFrame)
   {
      if(gsGlobal->DoubleBuffering == GS_SETTING_ON)
      {
         GS_SET_DISPFB2( gsGlobal->ScreenBuffer[gsGlobal->ActiveBuffer & 1] / 8192,
            gsGlobal->Width / 64, gsGlobal->PSM, 0, 0 );

         gsGlobal->ActiveBuffer ^= 1;
      }

   }

   gsKit_setactive(gsGlobal);
}
#endif

static GSGLOBAL *init_GSGlobal(void)
{
#ifdef HIRES_MODE
   int iPassCount;
#else
   ee_sema_t sema;
   sema.init_count = 0;
   sema.max_count = 1;
   sema.option = 0;
   vsync_sema_id = CreateSema(&sema);
#endif

   //  printf("%s\n", __FUNCTION__);

	dmaKit_init(D_CTRL_RELE_OFF,D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
		    D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);

   /* Initialize the DMAC */
	dmaKit_chan_init(DMA_CHANNEL_GIF);

#ifdef HIRES_MODE
   GSGLOBAL *gsGlobal        = gsKit_hires_init_global();

   #if 0
   // 480p
   gsGlobal->Mode            = GS_MODE_DTV_480P;
   gsGlobal->Interlace       = GS_NONINTERLACED;
   gsGlobal->Field           = GS_FRAME;
   gsGlobal->Width           = NTSC_WIDTH;
   gsGlobal->Height          = NTSC_HEIGHT;
   gsGlobal->PSM             = GS_PSM_CT32;
   iPassCount                = 2;
   #endif

   #if 1
   // 720p
   gsGlobal->Mode            = GS_MODE_DTV_720P;
   gsGlobal->Interlace       = GS_NONINTERLACED;
   gsGlobal->Field           = GS_FRAME;
   gsGlobal->Width           = 1280;
   gsGlobal->Height          = 720;
   gsGlobal->PSM             = GS_PSM_CT16S;
   iPassCount                = 3;
   #endif

#else
   GSGLOBAL *gsGlobal        = gsKit_init_global();

   gsGlobal->Mode            = GS_MODE_NTSC;
   gsGlobal->Interlace       = GS_INTERLACED;
   gsGlobal->Field           = GS_FIELD;
   gsGlobal->Width           = NTSC_WIDTH;
   gsGlobal->Height          = NTSC_HEIGHT;
   gsGlobal->PSM             = GS_PSM_CT32;
#endif

   gsGlobal->PSMZ            = GS_PSMZ_16S;
   gsGlobal->DoubleBuffering = GS_SETTING_ON;
   gsGlobal->ZBuffering      = GS_SETTING_OFF;
   gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
   gsGlobal->Dithering       = GS_SETTING_ON;

   gsGlobal->Test->ATST = 7; // NOTEQUAL to AREF passes
   gsGlobal->Test->AREF = 0x00;
   gsGlobal->Test->AFAIL = 0; // KEEP

   // Coordinate space ranges from 0 to 4096 pixels
   // Center the buffer in the coordinate space
   gsGlobal->OffsetX = ((4096 - gsGlobal->Width) / 2) * 16;
   gsGlobal->OffsetY = ((4096 - gsGlobal->Height) / 2) * 16;

#ifdef HIRES_MODE
   gsKit_hires_init_screen(gsGlobal, iPassCount);
#else
   gsKit_init_screen(gsGlobal);
   gsKit_mode_switch(gsGlobal, GS_ONESHOT);
#endif

   gsKit_set_test(gsGlobal, GS_ZTEST_OFF);
   gsKit_set_test(gsGlobal, GS_ATEST_OFF);
   gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);

#ifdef HIRES_MODE
   gsKit_hires_flip(gsGlobal);
#else
   gsKit_clear(gsGlobal, GS_BLACK);
   gsKit_flip(gsGlobal);
#endif

   return gsGlobal;
}

static void deinit_GSGlobal(GSGLOBAL *gsGlobal)
{
   gsKit_clear(gsGlobal, GS_BLACK);
   gsKit_vram_clear(gsGlobal);
#ifdef HIRES_MODE
   gsKit_hires_deinit_global(gsGlobal);
#else
   gsKit_deinit_global(gsGlobal);
#endif
}

static GSTEXTURE *prepare_new_texture(void)
{
   GSTEXTURE *texture = (GSTEXTURE*)calloc(1, sizeof(GSTEXTURE));
   return texture;
}

static void init_ps2_video(ps2_video_t *ps2)
{
   //  printf("%s\n", __FUNCTION__);

   ps2->gsGlobal    = init_GSGlobal();
   gsKit_TexManager_init(ps2->gsGlobal);

   ps2->vp.x                = 0;
   ps2->vp.y                = 0;
   ps2->vp.width            = ps2->gsGlobal->Width;
   ps2->vp.height           = ps2->gsGlobal->Height;
   ps2->vp.full_width       = ps2->gsGlobal->Width;
   ps2->vp.full_height      = ps2->gsGlobal->Height;

#ifndef HIRES_MODE
   ps2->vsync_callback_id = gsKit_add_vsync_handler(vsync_handler);
#endif
   ps2->menuTexture = prepare_new_texture();
   ps2->coreTexture = prepare_new_texture();
   ps2->displayTexture = prepare_new_texture();

   /* Used for cores that supports palette */
   ps2->iface.interface_type    = RETRO_HW_RENDER_INTERFACE_GSKIT_PS2;
   ps2->iface.interface_version = RETRO_HW_RENDER_INTERFACE_GSKIT_PS2_VERSION;
   ps2->iface.coreTexture       = ps2->coreTexture;

   video_driver_set_size(ps2->gsGlobal->Width, ps2->gsGlobal->Height);
}

static void ps2_gfx_deinit_texture(GSTEXTURE *texture)
{
   texture->Mem  = NULL;
   texture->Clut = NULL;
}

static void set_texture(GSTEXTURE *texture, const void *frame,
      int width, int height, int PSM, int filter)
{
   texture->Width  = width;
   texture->Height = height;
   texture->PSM    = PSM;
   texture->Filter = filter;
   texture->Mem    = (void *)frame;
}

static void prim_texture(GSGLOBAL *gsGlobal, GSTEXTURE *texture, int zPosition, bool force_aspect, struct retro_hw_ps2_insets padding)
{
   float x1, y1, x2, y2;
   float visible_width  =  texture->Width - padding.left - padding.right;
   float visible_height =  texture->Height - padding.top - padding.bottom;

   if (force_aspect)
   {
      float width_proportion  = (float)gsGlobal->Width / (float)visible_width;
      float height_proportion = (float)gsGlobal->Height / (float)visible_height;
      float delta             = MIN(width_proportion, height_proportion);
      float newWidth          = visible_width * delta;
      float newHeight         = visible_height * delta;

      x1 = (gsGlobal->Width - newWidth) / 2.0f;
      y1 = (gsGlobal->Height - newHeight) / 2.0f;
      x2 = newWidth + x1;
      y2 = newHeight + y1;
   }
   else
   {
      x1 = 0.0f;
      y1 = 0.0f;
      x2 = gsGlobal->Width;
      y2 = gsGlobal->Height;
   }

   //  printf("%s %dx%d -> %dx%d @ x=%.1f, y=%.1f\n", __FUNCTION__, texture->Width, texture->Height, x2-x1, y2-y1, x1, y1);

   gsKit_prim_sprite_texture( gsGlobal, texture,
         x1,            /* X1 */
         y1,            /* Y1 */
         padding.left,  /* U1 */
         padding.top,   /* V1 */
         x2,            /* X2 */
         y2,            /* Y2 */
         texture->Width - padding.right,   /* U2 */
         texture->Height - padding.bottom, /* V2 */
         zPosition,
         GS_TEXT);
}

static void refreshScreen(ps2_video_t *ps2)
{
   // printf("==========================================\n");
   // printf("%s\n", __FUNCTION__);
   // printf("==========================================\n");

   // Sync and flip
#ifdef HIRES_MODE
   if (ps2->vsync)
      gsKit_hires_sync(ps2->gsGlobal);
   gsKit_hires_flip(ps2->gsGlobal);
#else
   // Draw everything
   gsKit_set_finish(ps2->gsGlobal);
   gsKit_queue_exec(ps2->gsGlobal);
   gsKit_finish();

   if (ps2->vsync)
      gsKit_sync(ps2->gsGlobal);
   gsKit_flip(ps2->gsGlobal);
#endif

   // Let texture manager know we're moving to the next frame
   gsKit_TexManager_nextFrame(ps2->gsGlobal);
}

static void *ps2_gfx_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   void *ps2input   = NULL;
   ps2_video_t *ps2 = (ps2_video_t*)calloc(1, sizeof(ps2_video_t));

   //  printf("%s\n", __FUNCTION__);

   *input_data      = NULL;

   if (!ps2)
      return NULL;

   init_ps2_video(ps2);
   if (video->font_enable)
      font_driver_init_osd(ps2,
            video,
            false,
            video->is_threaded,
            FONT_DRIVER_RENDER_PS2);

   ps2->PSM          = (video->rgb32 ? GS_PSM_CT32 : GS_PSM_CT16);
   ps2->fullscreen   = video->fullscreen;
   ps2->core_filter  = video->smooth ? GS_FILTER_LINEAR : GS_FILTER_NEAREST;
   ps2->force_aspect = video->force_aspect;
   ps2->vsync        = video->vsync;
   ps2->msg_rendering_enabled     = false;

   if (input && input_data)
   {
      settings_t *settings = config_get_ptr();
      ps2input             = input_driver_init_wrap(&input_ps2,
            settings->arrays.input_joypad_driver);
      *input               = ps2input ? &input_ps2 : NULL;
      *input_data          = ps2input;
   }

   return ps2;
}

static bool ps2_gfx_frame(void *data, const void *frame,
      unsigned width, unsigned height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   ps2_video_t               *ps2 = (ps2_video_t*)data;
   struct font_params *osd_params = (struct font_params*)
      &video_info->osd_stat_params;
   bool statistics_show           = video_info->statistics_show;

   if (!width || !height)
      return false;

#if defined(DEBUG)
   if (frame_count % 180 == 0)
      printf("ps2_gfx_frame %llu\n", frame_count);
#endif

   gsKit_clear(ps2->gsGlobal, GS_BLACK);

   if (frame)
   {
      struct retro_hw_ps2_insets padding = empty_ps2_insets;
      /* Checking if the transfer is done in the core */
      if (frame != RETRO_HW_FRAME_BUFFER_VALID)
      {
         /* calculate proper width based in the pitch */
         int shifh_per_bytes = (ps2->PSM == GS_PSM_CT32) ? 2 : 1;
         int real_width      = pitch >> shifh_per_bytes;
         set_texture(ps2->coreTexture, frame, real_width, height, ps2->PSM, ps2->core_filter);

         padding.right       = real_width - width;
      }
      else
      {
         padding = ps2->iface.padding;
      }

      gsKit_TexManager_invalidate(ps2->gsGlobal, ps2->coreTexture);
      gsKit_TexManager_bind(ps2->gsGlobal, ps2->coreTexture);
      prim_texture(ps2->gsGlobal, ps2->coreTexture, 1, ps2->force_aspect, padding);
   }

   if (ps2->menuVisible)
   {
#ifdef HAVE_MENU
      ps2->msg_rendering_enabled = true;
      menu_driver_frame(ps2->menuVisible, video_info);
      ps2->msg_rendering_enabled = false;
#endif
      bool texture_empty = !ps2->menuTexture->Width || !ps2->menuTexture->Height;
      if (!texture_empty)
      {
         prim_texture(ps2->gsGlobal, ps2->menuTexture, 2, ps2->fullscreen, empty_ps2_insets);
      }
   }
   else if (statistics_show)
   {
      if (osd_params)
         font_driver_render_msg(ps2, video_info->stat_text,
               osd_params, NULL);
   }

   if(!string_is_empty(msg))
      font_driver_render_msg(ps2, msg, NULL, NULL);

   refreshScreen(ps2);

   return true;
}

static void ps2_gfx_set_nonblock_state(void *data, bool toggle,
      bool adaptive_vsync_enabled, unsigned swap_interval)
{
   ps2_video_t *ps2 = (ps2_video_t*)data;

   if (ps2)
      ps2->vsync = !toggle;
}

static bool ps2_gfx_alive(void *data) { return true; }
static bool ps2_gfx_focus(void *data) { return true; }
static bool ps2_gfx_suppress_screensaver(void *data, bool enable) { return false; }
static bool ps2_gfx_has_windowed(void *data) { return false; }

static void ps2_gfx_free(void *data)
{
   ps2_video_t *ps2 = (ps2_video_t*)data;

   gsKit_clear(ps2->gsGlobal, GS_BLACK);
   gsKit_vram_clear(ps2->gsGlobal);

   font_driver_free_osd();

   ps2_gfx_deinit_texture(ps2->menuTexture);
   ps2_gfx_deinit_texture(ps2->coreTexture);

   free(ps2->menuTexture);
   free(ps2->coreTexture);

#ifndef HIRES_MODE
   gsKit_remove_vsync_handler(ps2->vsync_callback_id);
#endif
   deinit_GSGlobal(ps2->gsGlobal);

#ifndef HIRES_MODE
   if (vsync_sema_id >= 0)
      DeleteSema(vsync_sema_id);
#endif

   free(data);
}

static bool ps2_gfx_set_shader(void *data,
      enum rarch_shader_type type, const char *path) { return false; }

static uintptr_t ps2_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   unsigned int stride, pitch, j, filter;

   const uint32_t *frame32        = NULL;
   struct texture_image *image    = (struct texture_image*)data;

   // printf("%s\n", __FUNCTION__);
   // printf("- texture: %dx%d\n", image->width, image->height);

   filter = ((filter_type == TEXTURE_FILTER_MIPMAP_LINEAR) ||
      (filter_type == TEXTURE_FILTER_LINEAR)) ? GS_FILTER_LINEAR : GS_FILTER_NEAREST;

   int textSize = image->width * image->height * 4;
   GSTEXTURE *texture = prepare_new_texture();
   uint32_t *tex32 = malloc(textSize);

   for (j = 0; j <  image->width * image->height; j++ ) {
      uint32_t currentColor = image->pixels[j];
      tex32[j] = ((currentColor >> 16) & 0x000000FF) | (currentColor & 0xFF00FF00) | ((currentColor << 16) & 0x00FF0000);
   }

   set_texture(texture, tex32, image->width, image->height, GS_PSM_CT32, filter);

   return (uintptr_t)texture;
}

static void ps2_unload_texture(void *data, bool threaded,
      uintptr_t handle)
{
   ps2_video_t *ps2 = (ps2_video_t*)data;
   GSTEXTURE *gsTexture = (GSTEXTURE *)handle;

   if (!gsTexture)
      return;
   gsKit_TexManager_invalidate(ps2->gsGlobal, gsTexture);
   free(gsTexture->Mem);
   free(gsTexture);
}

static void ps2_gfx_viewport_info(void *data,
      struct video_viewport *vp)
{
    ps2_video_t *ps2 = (ps2_video_t*)data;

    if (ps2)
       *vp = ps2->vp;
}

static void ps2_set_filtering(void *data, unsigned index, bool smooth, bool ctx_scaling)
{
   ps2_video_t *ps2 = (ps2_video_t*)data;

   ps2->menu_filter = smooth ? GS_FILTER_LINEAR : GS_FILTER_NEAREST;
}

static void ps2_set_texture_frame(void *data, const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
   ps2_video_t *ps2      = (ps2_video_t*)data;

   int PSM               = (rgb32 ? GS_PSM_CT32 : GS_PSM_CT16);

   set_texture(ps2->menuTexture, frame, width, height, PSM, ps2->menu_filter);
   gsKit_TexManager_invalidate(ps2->gsGlobal, ps2->menuTexture);
   gsKit_TexManager_bind(ps2->gsGlobal, ps2->menuTexture);
}

static void ps2_set_texture_enable(void *data, bool enable, bool fullscreen)
{
   ps2_video_t *ps2 = (ps2_video_t*)data;

   if (ps2->menuVisible != enable)
   {
      /* If Menu change status, CLEAR SCREEN */
      gsKit_clear(ps2->gsGlobal, GS_BLACK);
   }
   ps2->menuVisible = enable;
   ps2->fullscreen  = fullscreen;
}

static void ps2_set_osd_msg(void *data,
      const char *msg,
      const void *params, void *font)
{
   ps2_video_t *ps2 = (ps2_video_t*)data;

   if (ps2 && ps2->msg_rendering_enabled)
      font_driver_render_msg(data, msg, params, font);
}

static bool ps2_get_hw_render_interface(void* data,
      const struct retro_hw_render_interface** iface)
{
   ps2_video_t          *ps2 = (ps2_video_t*)data;
   ps2->iface.padding        = empty_ps2_insets;
   *iface                    =
      (const struct retro_hw_render_interface*)&ps2->iface;
   return true;
}

static uint32_t ps2_get_flags(void *data)
{
   uint32_t             flags   = 0;

   return flags;
}

static const video_poke_interface_t ps2_poke_interface = {
   ps2_get_flags,          /* get_flags  */
   ps2_load_texture,
   ps2_unload_texture,
   NULL,
   NULL, /* get_refresh_rate */
   ps2_set_filtering,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   NULL, /* set_aspect_ratio */
   NULL, /* apply_state_changes */
   ps2_set_texture_frame,
   ps2_set_texture_enable,
   ps2_set_osd_msg,             /* set_osd_msg */
   NULL,                        /* show_mouse  */
   NULL,                        /* grab_mouse_toggle */
   NULL,                        /* get_current_shader */
   NULL,                        /* get_current_software_framebuffer */
   ps2_get_hw_render_interface  /* get_hw_render_interface */
};

static void ps2_gfx_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &ps2_poke_interface;
}

video_driver_t video_ps2 = {
   ps2_gfx_init,
   ps2_gfx_frame,
   ps2_gfx_set_nonblock_state,
   ps2_gfx_alive,
   ps2_gfx_focus,
   ps2_gfx_suppress_screensaver,
   ps2_gfx_has_windowed,
   ps2_gfx_set_shader,
   ps2_gfx_free,
   "ps2",
   NULL,
   NULL, /* set_rotation */
   ps2_gfx_viewport_info,
   NULL, /* read_viewport  */
   NULL, /* read_frame_raw */

#ifdef HAVE_OVERLAY
  NULL, /* overlay_interface */
#endif
#ifdef HAVE_VIDEO_LAYOUT
  NULL,
#endif
  ps2_gfx_get_poke_interface,
};

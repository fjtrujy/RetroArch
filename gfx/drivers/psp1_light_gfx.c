/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Ali Bouhlel
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

#include <pspkernel.h>
#include <pspdisplay.h>
#include <malloc.h>
#include <pspgu.h>
#include <pspgum.h>
#include <psprtc.h>

#include <retro_assert.h>
#include <retro_inline.h>
#include <retro_math.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../font_driver.h"

#include "../../defines/psp_defines.h"

#ifndef SCEGU_SCR_WIDTH
#define SCEGU_SCR_WIDTH 480
#endif

#ifndef SCEGU_SCR_HEIGHT
#define SCEGU_SCR_HEIGHT 272
#endif

#ifndef SCEGU_VRAM_WIDTH
#define SCEGU_VRAM_WIDTH 512
#endif

/* Frame buffer */
#define SCEGU_VRAM_TOP        (0x44000000)
/* 16bit mode */
#define SCEGU_VRAM_BUFSIZE    (SCEGU_VRAM_WIDTH*SCEGU_SCR_HEIGHT*2)
#define SCEGU_VRAM_BP_0       ((void *)(SCEGU_VRAM_TOP))
#define SCEGU_VRAM_BP_1       ((void *)(SCEGU_VRAM_TOP+SCEGU_VRAM_BUFSIZE))
#define SCEGU_VRAM_BP_2       ((void *)(SCEGU_VRAM_TOP+(SCEGU_VRAM_BUFSIZE*2)))
/* 32bit mode */
#define SCEGU_VRAM_BUFSIZE32  (SCEGU_VRAM_WIDTH*SCEGU_SCR_HEIGHT*4)
#define SCEGU_VRAM_BP32_0     ((void *)(SCEGU_VRAM_TOP))
#define SCEGU_VRAM_BP32_1     ((void *)(SCEGU_VRAM_TOP+SCEGU_VRAM_BUFSIZE32))
#define SCEGU_VRAM_BP32_2     ((void *)(SCEGU_VRAM_TOP+(SCEGU_VRAM_BUFSIZE32*2)))

#define TO_UNCACHED_PTR(ptr)  ((void *)((uint32_t)(ptr)|0x40000000))
#define TO_CACHED_PTR(ptr)    ((void *)((uint32_t)(ptr)&~0x40000000))

#define FROM_GU_POINTER(ptr)  ((void *)((uint32_t)(ptr)|0x44000000))
#define TO_GU_POINTER(ptr)    ((void *)((uint32_t)(ptr)&~0x44000000))

typedef struct __attribute__((packed)) psp1_light_vertex
{
   float u,v;
   float x,y,z;

} psp1_light_vertex_t;

typedef struct __attribute__((packed)) psp1_light_sprite
{
   psp1_light_vertex_t v0;
   psp1_light_vertex_t v1;
} psp1_light_sprite_t;

typedef struct psp1_light_menu_frame
{
   void* dList;
   void* frame;
   psp1_light_sprite_t* frame_coords;

   bool active;

   PspGeContext context_storage;
} psp1_light_menu_frame_t;

typedef struct psp1_light_video
{
   void* main_dList;
   void* frame_dList;
   void* draw_buffer;
   void* texture;
   psp1_light_sprite_t* frame_coords;
   int tex_filter;

   bool vsync;
   bool rgb32;
   int bpp_log2;

   psp1_light_menu_frame_t menu;

   video_viewport_t vp;

   unsigned rotation;
   bool vblank_not_reached;
   bool keep_aspect;
   bool should_resize;
   bool hw_render;
} psp1_light_video_t;


static void init_psp_video(psp1_light_video_t *psp) {

}

static void *psp_light_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   void *pspinput = NULL;
   *input_data = NULL;
   psp1_light_video_t *psp        = (psp1_light_video_t*)calloc(1, sizeof(psp1_light_video_t));

   if (!psp)
      return NULL;

   init_psp_video(psp);

   if (input && input_data)
   {
      settings_t *settings = config_get_ptr();
      pspinput             = input_psp.init(settings->arrays.input_joypad_driver);
      *input               = pspinput ? &input_psp : NULL;
      *input_data          = pspinput;
   }

   return psp;
}

static bool psp_light_frame(void *data, const void *frame,
      unsigned width, unsigned height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   psp1_light_video_t *psp                       = (psp1_light_video_t*)data;

   if (!width || !height)
      return false;

   pspDebugScreenSetXY(0, 0);
	pspDebugScreenSetTextColor(0xFFFFFFFF);
	pspDebugScreenPuts("psp_light_frame\n");

   return true;
}

static void psp_light_set_nonblock_state(void *data, bool toggle)
{
   psp1_light_video_t *psp = (psp1_light_video_t*)data;

   if (psp)
      psp->vsync = !toggle;
}

static bool psp_light_alive(void *data)
{
   (void)data;
   return true;
}

static bool psp_light_focus(void *data)
{
   (void)data;
   return true;
}

static bool psp_light_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static void psp_light_free(void *data)
{
   psp1_light_video_t *psp = (psp1_light_video_t*)data;

   free(data);
}

static void psp_light_set_texture_frame(void *data, const void *frame, bool rgb32,
                               unsigned width, unsigned height, float alpha)
{
   psp1_light_video_t *psp = (psp1_light_video_t*)data;

   (void) rgb32;
   (void) alpha;
}

static void psp_light_set_texture_enable(void *data, bool state, bool full_screen)
{
   (void) full_screen;

   psp1_light_video_t *psp = (psp1_light_video_t*)data;

   if (psp)
      psp->menu.active = state;
}

static void psp_light_set_filtering(void *data, unsigned index, bool smooth)
{
   psp1_light_video_t *psp = (psp1_light_video_t*)data;

   if (psp)
      psp->tex_filter = smooth? GU_LINEAR : GU_NEAREST;
}

static const video_poke_interface_t psp_light_poke_interface = {
   NULL,          /* get_flags  */
   NULL,
   NULL,
   NULL,
   NULL, /* get_refresh_rate */
   psp_light_set_filtering,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   NULL, /* set_aspect_ratio */
   NULL, /* apply_state_changes */
   psp_light_set_texture_frame,
   psp_light_set_texture_enable,
   NULL,                        /* set_osd_msg */
   NULL,                        /* show_mouse  */
   NULL,                        /* grab_mouse_toggle */
   NULL,                        /* get_current_shader */
   NULL,                        /* get_current_software_framebuffer */
   NULL                         /* get_hw_render_interface */
};

static void psp_light_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &psp_light_poke_interface;
}

static bool psp_light_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   (void)data;
   (void)type;
   (void)path;

   return false;
}

video_driver_t video_psp1_light = {
   psp_light_init,
   psp_light_frame,
   psp_light_set_nonblock_state,
   psp_light_alive,
   psp_light_focus,
   psp_light_suppress_screensaver,
   NULL, /* has_windowed */
   psp_light_set_shader,
   psp_light_free,
   "psp1_light",
   NULL, /* set_viewport */
   NULL, /* set_rotation */
   NULL, /* viewport_info */
   NULL, /* read_viewport  */
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   NULL,
#endif
#ifdef HAVE_VIDEO_LAYOUT
  NULL,
#endif
   psp_light_get_poke_interface
};

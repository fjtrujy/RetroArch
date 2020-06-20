/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include <stdint.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <pad.h>

#include <boolean.h>
#include <libretro.h>
#include <retro_miscellaneous.h>

#include "../input_driver.h"

static void ps4_input_free_input(void *data) { }
static void* ps4_input_initialize(const char *a) { return (void*)-1; }

#ifdef HAVE_MOUSE
#if defined(HAVE_OOSDK)
#include <orbis/Mouse.h>
#else
#include <mouse.h>
#endif
#define MOUSE_MAX_HISTORY 1
#endif

#ifdef HAVE_KEYBOARD
#if defined(HAVE_OOSDK)
#include <orbis/DbgKeyboard.h>
#else
#include <dbg_keyboard.h>
#endif
#include "../input_keymaps.h"
#define PS4_MAX_SCANCODE 0xE7
#define PS4_NUM_MODIFIERS 11 /* number of modifiers reported */
#define KEYBOARD_MAX_HISTORY 6
uint8_t modifier_lut[PS4_NUM_MODIFIERS][2] =
{
   { 0xE0, 0x01 }, /* LCTRL */
   { 0xE4, 0x10 }, /* RCTRL */
   { 0xE1, 0x02 }, /* LSHIFT */
   { 0xE5, 0x20 }, /* RSHIFT */
   { 0xE2, 0x04 }, /* LALT */
   { 0xE6, 0x40 }, /* RALT */
   { 0xE3, 0x08 }, /* LGUI */
   { 0xE7, 0x80 }, /* RGUI */
   { 0x53, 0x01 }, /* NUMLOCK */
   { 0x39, 0x02 }, /* CAPSLOCK */
   { 0x47, 0x04 }  /* SCROLLOCK */
};
#endif

#include "../../defines/ps4_defines.h"

#ifdef HAVE_MOUSE
   bool mice_connected;
   int32_t mouse_handle;
   bool mouse_button_left;
   bool mouse_button_right;
   bool mouse_button_middle;
   int32_t mouse_x;
   int32_t mouse_y;
#endif
#ifdef HAVE_KEYBOARD
   bool keyboard_connected;
   int32_t keyboard_handle;
   bool keyboard_state[PS4_MAX_SCANCODE + 1];
   uint8_t prev_keys[KEYBOARD_MAX_HISTORY];
#endif

#ifdef HAVE_MOUSE
   int ret;
   SceMouseData mouse_state;
   ret = sceMouseRead(ps4->mouse_handle, &mouse_state, MOUSE_MAX_HISTORY);
   ps4->mice_connected = mouse_state.connected;
   ps4->mouse_x = 0;
   ps4->mouse_y = 0;
   if (ret > 0 && ps4->mice_connected && !(mouse_state.buttons & SCE_MOUSE_BUTTON_INTERCEPTED))
   {
     ps4->mouse_button_left = mouse_state.buttons & SCE_MOUSE_BUTTON_PRIMARY;
     ps4->mouse_button_right = mouse_state.buttons & SCE_MOUSE_BUTTON_SECONDARY;
     ps4->mouse_button_middle = mouse_state.buttons & SCE_MOUSE_BUTTON_OPTIONAL;
     ps4->mouse_x = mouse_state.xAxis;
     ps4->mouse_y = mouse_state.yAxis;
   }
#endif
#ifdef HAVE_KEYBOARD
   unsigned int i = 0;
   int key_sym = 0;
   unsigned key_code = 0;
   uint8_t mod_code = 0;
   uint16_t mod = 0;
   uint32_t modifiers;
   bool key_held = false;
   SceDbgKeyboardData keyboard_state;
   ret = sceDbgKeyboardReadState(ps4->keyboard_handle, &keyboard_state);
   ps4->keyboard_connected = keyboard_state.connected;
   if (ps4->keyboard_connected)
   {
     modifiers = keyboard_state.modifierKey;
     mod = (uint16_t)keyboard_state.modifierKey;

     for (i = 0; i < PS4_NUM_MODIFIERS; i++)
     {
        key_sym = (int) modifier_lut[i][0];
        mod_code = modifier_lut[i][1];
        key_code = input_keymaps_translate_keysym_to_rk(key_sym);
        key_held = (modifiers & mod_code);
        if (key_held && !(ps4->keyboard_state[key_sym]))
        {
           ps4->keyboard_state[key_sym] = true;
           input_keyboard_event(true, key_code, 0, mod, RETRO_DEVICE_KEYBOARD);
        }
        else if (!key_held && (ps4->keyboard_state[key_sym]))
        {
           ps4->keyboard_state[key_sym] = false;
           input_keyboard_event(false, key_code, 0, mod, RETRO_DEVICE_KEYBOARD);
        }
    for (i = 0; i < KEYBOARD_MAX_HISTORY; i++)
    {
      key_sym = keyboard_state.keyCode[i];

      if (key_sym != ps4->prev_keys[i])
      {
         if (ps4->prev_keys[i])
         {
            ps4->keyboard_state[ps4->prev_keys[i]] = false;
            key_code = input_keymaps_translate_keysym_to_rk(ps4->prev_keys[i]);
            input_keyboard_event(false, key_code, 0, mod, RETRO_DEVICE_KEYBOARD);
         }
         if (key_sym)
         {
            ps4->keyboard_state[key_sym] = true;
            key_code = input_keymaps_translate_keysym_to_rk(key_sym);
            input_keyboard_event(true, key_code, 0, mod, RETRO_DEVICE_KEYBOARD);
         }
         ps4->prev_keys[i] = key_sym;
      }
    }
   }
#endif
}

#ifdef HAVE_MOUSE
static int16_t ps4_input_mouse_state(ps4_input_t *ps4, unsigned id, bool screen)
{
   int16_t val = 0;

   if (!ps4->mice_connected)
      return 0;

   switch (id)
   {
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         val = ps4->mouse_button_left;
         break;
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         val = ps4->mouse_button_right;
         break;
      case RETRO_DEVICE_ID_MOUSE_MIDDLE:
         val = ps4->mouse_button_middle;
         break;
      case RETRO_DEVICE_ID_MOUSE_X:
         val = ps4->mouse_x;
         break;
      case RETRO_DEVICE_ID_MOUSE_Y:
         val = ps4->mouse_y;
         break;
   }

   return val;
}
#endif

#ifdef HAVE_KEYBOARD
      case RETRO_DEVICE_KEYBOARD:
         return ((id < RETROK_LAST) && ps4->keyboard_state[rarch_keysym_lut[(enum retro_key)id]]);
         break;
#endif
#ifdef HAVE_MOUSE
      case RETRO_DEVICE_MOUSE:
         return ps4_input_mouse_state(ps4, id, false);
         break;
      case RARCH_DEVICE_MOUSE_SCREEN:
         return ps4_input_mouse_state(ps4, id, true);
         break;
#endif
#if defined(HAVE_MOUSE)
   sceMouseClose(ps4->mouse_handle);
#endif
#if defined(HAVE_KEYBOARD)
   sceDbgKeyboardClose(ps4->keyboard_handle);
#endif
#if defined(HAVE_MOUSE) || defined(HAVE_KEYBOARD)
  SceUserServiceUserId user_id;
  sceUserServiceGetInitialUser(&user_id);
#ifdef HAVE_MOUSE
  sceMouseInit();
  SceMouseOpenParam param;
  param.behaviorFlag = 0;
  param.behaviorFlag |= SCE_MOUSE_OPEN_PARAM_MERGED;
  ps4->mouse_handle = sceMouseOpen(user_id, SCE_MOUSE_PORT_TYPE_STANDARD, 0, &param);
  ps4->mouse_x = 0;
  ps4->mouse_y = 0;
#endif
#ifdef HAVE_KEYBOARD
  sceDbgKeyboardInit();
  ps4->keyboard_handle = sceDbgKeyboardOpen(user_id, SCE_DBG_KEYBOARD_PORT_TYPE_STANDARD, 0, NULL);
  input_keymaps_init_keyboard_lut(rarch_key_map_ps4);
  unsigned int i;
  for (i = 0; i <= PS4_MAX_SCANCODE; i++)
  {
    ps4->keyboard_state[i] = false;
  }
  for (i = 0; i < KEYBOARD_MAX_HISTORY; i++)
  {
    ps4->prev_keys[i] = 0;
  }
#endif
#endif

static uint64_t ps4_input_get_capabilities(void *data)
{
   return
#ifdef HAVE_KEYBOARD
      (1 << RETRO_DEVICE_KEYBOARD) |
#endif
#ifdef HAVE_MOUSE
      (1 << RETRO_DEVICE_MOUSE)    |
#endif
      (1 << RETRO_DEVICE_JOYPAD)   |
      (1 << RETRO_DEVICE_ANALOG);
}

input_driver_t input_ps4 = {
   ps4_input_initialize,
   NULL,                         /* poll */
   NULL,                         /* input_state */
   ps4_input_free_input,
   NULL,
   NULL,
   ps4_input_get_capabilities,
   "ps4",
   NULL,                         /* grab_mouse */
   NULL
};

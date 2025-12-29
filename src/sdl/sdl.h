#ifndef KTVLV_SDL_SDL_H
#define KTVLV_SDL_SDL_H

#include <lvgl.h>
#include <SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

bool sdl_init(void);
void sdl_display_flush(lv_disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p);
void sdl_mouse_read(lv_indev_drv_t* indev_drv, lv_indev_data_t* data);
void sdl_keyboard_read(lv_indev_drv_t* indev_drv, lv_indev_data_t* data);
void sdl_update_mouse_state(SDL_Event* e);
void sdl_update_keyboard_state(SDL_Event* e);

#ifdef __cplusplus
}
#endif

#endif  // KTVLV_SDL_SDL_H


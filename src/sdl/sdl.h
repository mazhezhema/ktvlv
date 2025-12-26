#ifndef KTVLV_SDL_SDL_H
#define KTVLV_SDL_SDL_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

void sdl_init(void);
bool sdl_handle_events(void);  // 主循环调用，处理 SDL 事件，返回 false 表示应该退出
void sdl_display_flush(lv_disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p);
void sdl_mouse_read(lv_indev_drv_t* indev_drv, lv_indev_data_t* data);
void sdl_keyboard_read(lv_indev_drv_t* indev_drv, lv_indev_data_t* data);

#ifdef __cplusplus
}
#endif

#endif  // KTVLV_SDL_SDL_H


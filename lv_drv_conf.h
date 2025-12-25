/**
 * Minimal lv_drivers configuration for SDL on PC.
 */
#ifndef LV_DRV_CONF_H
#define LV_DRV_CONF_H

#define USE_SDL 1
#define USE_SDL_GPU 0

/* SDL 头文件路径 */
#define SDL_INCLUDE_PATH <SDL.h>

/* 仿真分辨率与刷新周期 */
#define SDL_HOR_RES 1280
#define SDL_VER_RES 720
#define SDL_ZOOM 1
#define SDL_REFR_PERIOD 5

#endif // LV_DRV_CONF_H


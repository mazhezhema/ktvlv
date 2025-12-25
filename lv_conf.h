/**
 * Minimal LVGL configuration for PC (SDL) simulation.
 * Uses 32-bit color depth, no GPU, no file system binding here.
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 32
#define LV_MEM_CUSTOM 0
#define LV_USE_LOG 1

#define LV_USE_GPU 0
#define LV_USE_PERF_MONITOR 0

/* Enable necessary widgets */
#define LV_USE_BTN        1
#define LV_USE_LABEL      1
#define LV_USE_LIST       1
#define LV_USE_SLIDER     1
#define LV_USE_TEXTAREA   1
#define LV_USE_KEYBOARD   1
#define LV_USE_MSGBOX     1
#define LV_USE_TILEVIEW   1
#define LV_USE_TABVIEW    1

/* Display resolution for SDL simulation */
#define LV_HOR_RES_MAX 1280
#define LV_VER_RES_MAX 720

#endif // LV_CONF_H


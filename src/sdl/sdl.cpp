#include "sdl.h"
#include <SDL.h>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#elif defined(__unix__) || defined(__linux__)
#include <unistd.h>
#endif

static SDL_Window* window = nullptr;
static SDL_Renderer* renderer = nullptr;
static SDL_Texture* texture = nullptr;

// 全局事件状态，由主循环更新
static int32_t mouse_x = 0;
static int32_t mouse_y = 0;
static bool mouse_pressed = false;
static uint32_t keyboard_key = 0;
static bool keyboard_pressed = false;

void sdl_init(void) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL初始化失败: %s\n", SDL_GetError());
        return;
    }
    
    window = SDL_CreateWindow(
        "KTV LVGL",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        LV_HOR_RES_MAX,
        LV_VER_RES_MAX,
        SDL_WINDOW_SHOWN
    );
    if (!window) {
        fprintf(stderr, "SDL窗口创建失败: %s\n", SDL_GetError());
        return;
    }
    
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "SDL渲染器创建失败: %s\n", SDL_GetError());
        return;
    }
    
    texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STATIC,
        LV_HOR_RES_MAX,
        LV_VER_RES_MAX
    );
    if (!texture) {
        fprintf(stderr, "SDL纹理创建失败: %s\n", SDL_GetError());
        return;
    }
    
    // 清空渲染器为黑色背景
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
}

void sdl_display_flush(lv_disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p) {
    if (!renderer || !texture) {
        lv_disp_flush_ready(disp_drv);
        return;
    }
    
    static bool first_flush = true;
    if (first_flush) {
        first_flush = false;
        fprintf(stderr, "首次刷新屏幕: area=(%d,%d)-(%d,%d)\n", 
                area->x1, area->y1, area->x2, area->y2);
    }

    int32_t x1 = area->x1;
    int32_t y1 = area->y1;
    int32_t x2 = area->x2;
    int32_t y2 = area->y2;

    int32_t w = (x2 - x1 + 1);
    int32_t h = (y2 - y1 + 1);

    SDL_Rect rect;
    rect.x = x1;
    rect.y = y1;
    rect.w = w;
    rect.h = h;

    // Convert LVGL color format to SDL format
    // Use static buffer to avoid dynamic allocation
    static uint32_t pixel_buf[LV_HOR_RES_MAX * LV_VER_RES_MAX];
    uint32_t* pixels = pixel_buf;
    
    // LVGL 32位颜色格式：lv_color32_t 结构为 {blue, green, red, alpha}
    // SDL需要ARGB8888格式，直接使用full值或手动转换
    for (int32_t y = 0; y < h; y++) {
        for (int32_t x = 0; x < w; x++) {
            lv_color_t color = color_p[y * w + x];
            // LVGL 32位: full值已经是ARGB格式，但字节序可能不同
            // 手动构建ARGB8888: A R G B (大端序)
            uint32_t argb = ((uint32_t)color.ch.alpha << 24) | 
                           ((uint32_t)color.ch.red << 16) | 
                           ((uint32_t)color.ch.green << 8) | 
                           (uint32_t)color.ch.blue;
            pixels[y * w + x] = argb;
        }
    }

    SDL_UpdateTexture(texture, &rect, pixels, w * sizeof(uint32_t));
    
    // 将纹理复制到渲染器（使用相同的rect作为目标）
    SDL_RenderCopy(renderer, texture, &rect, &rect);
    
    // 注意：不要每次都调用 RenderPresent，应该在主循环中统一调用
    // 但为了确保显示，这里先保留
    SDL_RenderPresent(renderer);
    lv_disp_flush_ready(disp_drv);
}

// 更新鼠标状态（由主循环调用）
void sdl_update_mouse_state(SDL_Event* e) {
    if (e->type == SDL_MOUSEMOTION) {
        mouse_x = e->motion.x;
        mouse_y = e->motion.y;
    } else if (e->type == SDL_MOUSEBUTTONDOWN) {
        mouse_x = e->button.x;
        mouse_y = e->button.y;
        mouse_pressed = true;
    } else if (e->type == SDL_MOUSEBUTTONUP) {
        mouse_x = e->button.x;
        mouse_y = e->button.y;
        mouse_pressed = false;
    }
}

void sdl_mouse_read(lv_indev_drv_t* indev_drv, lv_indev_data_t* data) {
    (void)indev_drv;  // 未使用
    data->point.x = mouse_x;
    data->point.y = mouse_y;
    data->state = mouse_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

// 更新键盘状态（由主循环调用）
void sdl_update_keyboard_state(SDL_Event* e) {
    if (e->type == SDL_KEYDOWN) {
        keyboard_key = e->key.keysym.sym;
        keyboard_pressed = true;
    } else if (e->type == SDL_KEYUP) {
        if (e->key.keysym.sym == keyboard_key) {
            keyboard_pressed = false;
        }
    }
}

void sdl_keyboard_read(lv_indev_drv_t* indev_drv, lv_indev_data_t* data) {
    (void)indev_drv;  // 未使用
    data->key = keyboard_pressed ? keyboard_key : 0;
    data->state = keyboard_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}


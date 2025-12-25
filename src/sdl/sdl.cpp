#include "sdl.h"
#include <SDL.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

static SDL_Window* window = nullptr;
static SDL_Renderer* renderer = nullptr;
static SDL_Texture* texture = nullptr;
static SDL_PixelFormat* pixel_format = nullptr;  // 缓存格式，避免重复分配和内存泄漏

// 鼠标和键盘状态（静态变量，由主循环更新）
static int32_t mouse_x = 0, mouse_y = 0;
static bool mouse_pressed = false;
static uint32_t keyboard_key = 0;
static bool keyboard_pressed = false;

void sdl_init(void) {
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow(
        "KTV LVGL",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        LV_HOR_RES_MAX,
        LV_VER_RES_MAX,
        SDL_WINDOW_SHOWN
    );
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STATIC,
        LV_HOR_RES_MAX,
        LV_VER_RES_MAX
    );
    // 缓存像素格式，避免重复分配
    pixel_format = SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888);
}

void sdl_display_flush(lv_disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p) {
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

    // 使用静态缓冲区，避免动态分配
    static uint32_t pixel_buf[LV_HOR_RES_MAX * LV_VER_RES_MAX];
    uint32_t* pixels = pixel_buf;
    
    // LVGL 使用 32-bit ARGB8888，直接转换
    for (int32_t y = 0; y < h; y++) {
        for (int32_t x = 0; x < w; x++) {
            lv_color_t color = color_p[y * w + x];
            // 使用缓存的像素格式，避免重复分配
            pixels[y * w + x] = SDL_MapRGBA(
                pixel_format,
                color.ch.red,
                color.ch.green,
                color.ch.blue,
                255  // 不透明
            );
        }
    }

    SDL_UpdateTexture(texture, &rect, pixels, w * sizeof(uint32_t));
    SDL_RenderCopy(renderer, texture, &rect, &rect);
    SDL_RenderPresent(renderer);
    lv_disp_flush_ready(disp_drv);
}

// 主循环调用此函数处理 SDL 事件
void sdl_handle_events() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            // 可以设置退出标志，这里先忽略
        } else if (e.type == SDL_MOUSEMOTION) {
            mouse_x = e.motion.x;
            mouse_y = e.motion.y;
        } else if (e.type == SDL_MOUSEBUTTONDOWN) {
            mouse_x = e.button.x;
            mouse_y = e.button.y;
            mouse_pressed = true;
        } else if (e.type == SDL_MOUSEBUTTONUP) {
            mouse_x = e.button.x;
            mouse_y = e.button.y;
            mouse_pressed = false;
        } else if (e.type == SDL_KEYDOWN) {
            keyboard_key = e.key.keysym.sym;
            keyboard_pressed = true;
        } else if (e.type == SDL_KEYUP) {
            keyboard_key = e.key.keysym.sym;
            keyboard_pressed = false;
        }
    }
}

void sdl_mouse_read(lv_indev_drv_t* indev_drv, lv_indev_data_t* data) {
    // 返回静态变量保存的状态，由主循环通过 sdl_handle_events 更新
    data->point.x = mouse_x;
    data->point.y = mouse_y;
    data->state = mouse_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

void sdl_keyboard_read(lv_indev_drv_t* indev_drv, lv_indev_data_t* data) {
    // 返回静态变量保存的状态，由主循环通过 sdl_handle_events 更新
    data->key = keyboard_key;
    data->state = keyboard_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}


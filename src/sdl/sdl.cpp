#include "sdl.h"
#include <SDL.h>
#include <cstdio>

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
    printf("Initializing SDL...\n");
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return;
    }
    printf("SDL_Init OK\n");
    
    printf("Creating SDL window %dx%d...\n", LV_HOR_RES_MAX, LV_VER_RES_MAX);
    window = SDL_CreateWindow(
        "KTV LVGL",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        LV_HOR_RES_MAX,
        LV_VER_RES_MAX,
        SDL_WINDOW_SHOWN
    );
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return;
    }
    printf("SDL window created\n");
    
    // 尝试软件渲染器作为备选
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        printf("Hardware renderer failed, trying software renderer...\n");
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return;
    }
    printf("SDL renderer created\n");
    
    texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STATIC,
        LV_HOR_RES_MAX,
        LV_VER_RES_MAX
    );
    if (!texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        return;
    }
    printf("SDL texture created\n");
    
    // 缓存像素格式，避免重复分配
    pixel_format = SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888);
    if (!pixel_format) {
        fprintf(stderr, "SDL_AllocFormat failed: %s\n", SDL_GetError());
        return;
    }
    printf("SDL pixel format allocated\n");
    
    // 清空屏幕为测试颜色（浅灰色），确保窗口可见
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    printf("SDL screen cleared and presented\n");
}

void sdl_display_flush(lv_disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p) {
    static int flush_count = 0;
    flush_count++;
    
    // 前10次 flush 都打印详细信息，之后只打印前3次
    if (flush_count <= 10 || flush_count <= 3) {
        printf("SDL flush #%d CALLED: area (%d,%d) to (%d,%d), size=%dx%d\n", 
               flush_count, area->x1, area->y1, area->x2, area->y2,
               (area->x2 - area->x1 + 1), (area->y2 - area->y1 + 1));
        fflush(stdout);
    }
    
    if (!texture || !renderer || !pixel_format) {
        printf("SDL flush: texture/renderer/pixel_format is NULL!\n");
        lv_disp_flush_ready(disp_drv);
        return;
    }
    
    int32_t x1 = area->x1;
    int32_t y1 = area->y1;
    int32_t x2 = area->x2;
    int32_t y2 = area->y2;

    int32_t w = (x2 - x1 + 1);
    int32_t h = (y2 - y1 + 1);
    
    if (w <= 0 || h <= 0 || w > LV_HOR_RES_MAX || h > LV_VER_RES_MAX) {
        if (flush_count <= 3) {
            printf("SDL flush: invalid area size %dx%d\n", w, h);
        }
        lv_disp_flush_ready(disp_drv);
        return;
    }

    SDL_Rect rect;
    rect.x = x1;
    rect.y = y1;
    rect.w = w;
    rect.h = h;

    // 使用静态缓冲区，避免动态分配
    static uint32_t pixel_buf[LV_HOR_RES_MAX * LV_VER_RES_MAX];
    uint32_t* pixels = pixel_buf;
    
    // LVGL 32-bit 颜色格式转换
    // LV_COLOR_DEPTH 32 时，lv_color_t 结构体包含 ch.red, ch.green, ch.blue, ch.alpha
    for (int32_t y = 0; y < h; y++) {
        for (int32_t x = 0; x < w; x++) {
            lv_color_t color = color_p[y * w + x];
            
#if LV_COLOR_DEPTH == 32
            // 32-bit 模式：直接使用 ch 结构体成员
            uint8_t r = color.ch.red;
            uint8_t g = color.ch.green;
            uint8_t b = color.ch.blue;
#else
            // 其他模式：需要转换
            uint8_t r = color.ch.red;
            uint8_t g = color.ch.green;
            uint8_t b = color.ch.blue;
#endif
            
            // 使用缓存的像素格式，避免重复分配
            pixels[y * w + x] = SDL_MapRGBA(
                pixel_format,
                r, g, b,
                255  // 不透明
            );
        }
    }

    SDL_UpdateTexture(texture, &rect, pixels, w * sizeof(uint32_t));
    SDL_RenderCopy(renderer, texture, &rect, &rect);
    SDL_RenderPresent(renderer);
    
    if (flush_count <= 3) {
        printf("SDL flush #%d: rendered %dx%d area, presented to screen\n", flush_count, w, h);
    }
    
    lv_disp_flush_ready(disp_drv);
}

// 主循环调用此函数处理 SDL 事件
// 返回 false 表示应该退出程序
bool sdl_handle_events() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            return false;  // 窗口关闭，退出程序
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
    return true;  // 继续运行
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


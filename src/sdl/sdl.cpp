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

// Global event state, updated by main loop
static int32_t mouse_x = 0;
static int32_t mouse_y = 0;
static bool mouse_pressed = false;
static uint32_t keyboard_key = 0;
static bool keyboard_pressed = false;

bool sdl_init(void) {
    fprintf(stderr, "Starting SDL initialization...\n");
    // 初始化SDL视频和定时器子系统
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return false;
    }
    fprintf(stderr, "SDL initialized successfully\n");
    
    // 设置渲染缩放质量提示（线性过滤）
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    
    window = SDL_CreateWindow(
        "KTV LVGL",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        LV_HOR_RES_MAX,
        LV_VER_RES_MAX,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        fprintf(stderr, "SDL window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }
    fprintf(stderr, "SDL window created: %dx%d\n", LV_HOR_RES_MAX, LV_VER_RES_MAX);
    
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "SDL renderer creation failed, trying software renderer: %s\n", SDL_GetError());
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        if (!renderer) {
            fprintf(stderr, "SDL software renderer creation also failed: %s\n", SDL_GetError());
            SDL_DestroyWindow(window);
            SDL_Quit();
            return false;
        }
    }
    fprintf(stderr, "SDL renderer created successfully\n");
    
    texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STATIC,
        LV_HOR_RES_MAX,
        LV_VER_RES_MAX
    );
    if (!texture) {
        fprintf(stderr, "SDL texture creation failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }
    fprintf(stderr, "SDL texture created successfully\n");
    
    // Clear renderer with black background
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    fprintf(stderr, "SDL initialization complete\n");
    return true;
}

void sdl_display_flush(lv_disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p) {
    // Always log that flush is being called (for first 20 times)
    static int flush_count = 0;
    flush_count++;
    
    if (flush_count <= 20) {
        fprintf(stderr, "[FLUSH] sdl_display_flush called (#%d): area=(%d,%d)-(%d,%d), size=%dx%d\n", 
                flush_count, area->x1, area->y1, area->x2, area->y2,
                (area->x2 - area->x1 + 1), (area->y2 - area->y1 + 1));
    }
    
    if (!renderer || !texture) {
        fprintf(stderr, "ERROR: sdl_display_flush called but renderer or texture is NULL! (flush #%d)\n", flush_count);
        lv_disp_flush_ready(disp_drv);
        return;
    }

    int32_t x1 = area->x1;
    int32_t y1 = area->y1;
    int32_t x2 = area->x2;
    int32_t y2 = area->y2;

    // Boundary check
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= LV_HOR_RES_MAX) x2 = LV_HOR_RES_MAX - 1;
    if (y2 >= LV_VER_RES_MAX) y2 = LV_VER_RES_MAX - 1;
    
    if (x1 > x2 || y1 > y2) {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    int32_t w = (x2 - x1 + 1);
    int32_t h = (y2 - y1 + 1);
    
    if (w <= 0 || h <= 0 || w > LV_HOR_RES_MAX || h > LV_VER_RES_MAX) {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    SDL_Rect rect;
    rect.x = x1;
    rect.y = y1;
    rect.w = w;
    rect.h = h;

    // 检查 color_p 指针有效性
    if (!color_p) {
        fprintf(stderr, "ERROR: sdl_display_flush called with NULL color_p! (flush #%d)\n", flush_count);
        lv_disp_flush_ready(disp_drv);
        return;
    }
    
    // Convert LVGL color format to SDL format
    // Use static buffer to avoid dynamic allocation
    static uint32_t pixel_buf[LV_HOR_RES_MAX * LV_VER_RES_MAX];
    uint32_t* pixels = pixel_buf;
    
    // 计算缓冲区大小（color_p 的大小应该是 w * h）
    size_t color_buf_size = (size_t)w * h;
    size_t pixel_buf_size = (size_t)w * h;
    
    // 确保不会溢出静态缓冲区
    if (pixel_buf_size > (size_t)(LV_HOR_RES_MAX * LV_VER_RES_MAX)) {
        fprintf(stderr, "ERROR: Region too large! w=%d, h=%d (flush #%d)\n", w, h, flush_count);
        lv_disp_flush_ready(disp_drv);
        return;
    }
    
    // LVGL 32-bit color format: lv_color32_t structure is {blue, green, red, alpha}
    // SDL needs ARGB8888 format, manually build ARGB8888: A R G B
    // color_p 是行优先数组，大小为 w * h
    for (int32_t y = 0; y < h; y++) {
        for (int32_t x = 0; x < w; x++) {
            size_t idx = (size_t)(y * w + x);
            
            // 边界检查：确保不超出缓冲区
            if (idx >= color_buf_size) {
                fprintf(stderr, "ERROR: Index out of bounds! idx=%zu, size=%zu (flush #%d)\n", 
                        idx, color_buf_size, flush_count);
                break;  // 跳出内层循环
            }
            
            // 安全访问 color_p
            lv_color_t color = color_p[idx];
            // Manually build ARGB8888: A R G B (big-endian)
            uint32_t argb = ((uint32_t)color.ch.alpha << 24) | 
                           ((uint32_t)color.ch.red << 16) | 
                           ((uint32_t)color.ch.green << 8) | 
                           (uint32_t)color.ch.blue;
            pixels[idx] = argb;
        }
    }

    // Update texture
    if (SDL_UpdateTexture(texture, &rect, pixels, w * sizeof(uint32_t)) != 0) {
        fprintf(stderr, "ERROR: SDL_UpdateTexture failed: %s (flush #%d)\n", SDL_GetError(), flush_count);
        lv_disp_flush_ready(disp_drv);
        return;
    }
    
    // Copy texture to renderer (using same rect as destination)
    if (SDL_RenderCopy(renderer, texture, &rect, &rect) != 0) {
        fprintf(stderr, "ERROR: SDL_RenderCopy failed: %s (flush #%d)\n", SDL_GetError(), flush_count);
        lv_disp_flush_ready(disp_drv);
        return;
    }
    
    // 关键：刷新权交给LVGL，SDL只做承载
    // RenderPresent必须在flush_cb中调用，不要在主循环中调用
    // 这样可以避免SDL和LVGL同时刷屏导致的冲突和黑屏
    SDL_RenderPresent(renderer);
    
    if (flush_count <= 10) {
        fprintf(stderr, "Screen flush #%d completed successfully\n", flush_count);
    }
    
    // 必须调用！通知LVGL刷新完成，否则会阻塞和死锁
    lv_disp_flush_ready(disp_drv);
}

// Update mouse state (called by main loop)
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
    (void)indev_drv;  // Unused
    data->point.x = mouse_x;
    data->point.y = mouse_y;
    data->state = mouse_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

// Update keyboard state (called by main loop)
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
    (void)indev_drv;  // Unused
    data->key = keyboard_pressed ? keyboard_key : 0;
    data->state = keyboard_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}


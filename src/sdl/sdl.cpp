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
    
    // ✅ 关键修复：优先使用软件渲染，避免 GPU/SDL 内部渲染器劫持 flush_cb
    // 在 flush_cb 正常工作前，必须使用 SOFTWARE 模式确保控制权
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) {
        fprintf(stderr, "[SDL] Software renderer failed: %s\n", SDL_GetError());
        // 如果软件渲染失败，尝试硬件加速（但不推荐，可能绕过 flush_cb）
        fprintf(stderr, "[SDL] Falling back to hardware renderer...\n");
        renderer = SDL_CreateRenderer(window, -1, 
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer) {
            fprintf(stderr, "❌ [SDL] All renderers failed: %s\n", SDL_GetError());
            SDL_DestroyWindow(window);
            SDL_Quit();
            return false;
        }
        fprintf(stderr, "⚠️ [SDL] Using ACCELERATED renderer (fallback, may bypass flush_cb)\n");
    } else {
        fprintf(stderr, "✅ [SDL] Using SOFTWARE renderer (flush_cb will work)\n");
    }
    
    // ✅ 诊断：检查 renderer 信息
    SDL_RendererInfo info;
    if (SDL_GetRendererInfo(renderer, &info) == 0) {
        fprintf(stderr, "[SDL] Renderer: %s, flags=0x%x\n", info.name, info.flags);
    }
    
    // ✅ Step2修复：明确使用 32bit ARGB8888 格式，使用 STREAMING 模式以便频繁更新
    texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,  // 明确指定 32bit ARGB
        SDL_TEXTUREACCESS_STREAMING,  // 改为 STREAMING，适合频繁更新
        LV_HOR_RES_MAX,
        LV_VER_RES_MAX
    );
    if (!texture) {
        fprintf(stderr, "❌ [SDL] Texture creation failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }
    
    // ✅ 诊断：检查 texture 格式
    Uint32 format;
    int access, w, h;
    if (SDL_QueryTexture(texture, &format, &access, &w, &h) == 0) {
        fprintf(stderr, "[SDL] Texture created: %dx%d, format=0x%x (ARGB8888=0x%x), access=%d\n",
                w, h, format, SDL_PIXELFORMAT_ARGB8888, access);
        if (format != SDL_PIXELFORMAT_ARGB8888) {
            fprintf(stderr, "⚠️ [SDL] WARNING: Texture format mismatch! Expected ARGB8888\n");
        }
    }
    fprintf(stderr, "[SDL] Texture created successfully\n");
    
    // Clear renderer with black background
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    fprintf(stderr, "SDL initialization complete\n");
    return true;
}

// ✅ 完整最小可运行驱动 - 确保 flush_cb 拥有控制权
// ⭐ 必须在 extern "C" 块内，确保函数符号不被 C++ 命名修饰
extern "C" {
void sdl_display_flush(lv_disp_drv_t* disp_drv, const lv_area_t* area, lv_color_t* color_p) {
    static int flush_count = 0;
    flush_count++;
    
    int w = area->x2 - area->x1 + 1;
    int h = area->y2 - area->y1 + 1;
    fprintf(stderr, "🔥 FLUSH %d x %d\n", w, h);
    
    // 基本NULL检查
    if (!renderer || !texture || !color_p) {
        fprintf(stderr, "❌ [FLUSH] ERROR: renderer/texture/color_p NULL (flush #%d)\n", flush_count);
        lv_disp_flush_ready(disp_drv);
        return;
    }

    // ✅ 极简版：直接使用 color_p，全屏更新
    // full_refresh 模式下，area 总是全屏，直接全屏更新
    const int pitch = LV_HOR_RES_MAX * 4;  // 32bit = 4字节
    
    // ✅ 像素格式转换：LVGL 32bit (BGRA) -> SDL ARGB8888
    static uint32_t pixel_buf[LV_HOR_RES_MAX * LV_VER_RES_MAX];
    const size_t total_pixels = LV_HOR_RES_MAX * LV_VER_RES_MAX;
    
    for (size_t i = 0; i < total_pixels; i++) {
        lv_color_t color = color_p[i];
        // 构建 ARGB8888：A(alpha) R(red) G(green) B(blue)
        pixel_buf[i] = ((uint32_t)color.ch.alpha << 24) | 
                      ((uint32_t)color.ch.red << 16) | 
                      ((uint32_t)color.ch.green << 8) | 
                      (uint32_t)color.ch.blue;
    }

    // ✅ 全屏更新 texture
    if (SDL_UpdateTexture(texture, NULL, pixel_buf, pitch) != 0) {
        fprintf(stderr, "❌ [FLUSH] SDL_UpdateTexture failed: %s\n", SDL_GetError());
        lv_disp_flush_ready(disp_drv);
        return;
    }

    // ✅ 全屏渲染
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    
    // 必须调用！通知LVGL刷新完成
    lv_disp_flush_ready(disp_drv);
}

void sdl_mouse_read(lv_indev_drv_t* indev_drv, lv_indev_data_t* data) {
    (void)indev_drv;  // Unused
    data->point.x = mouse_x;
    data->point.y = mouse_y;
    data->state = mouse_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

void sdl_keyboard_read(lv_indev_drv_t* indev_drv, lv_indev_data_t* data) {
    (void)indev_drv;  // Unused
    data->key = keyboard_pressed ? keyboard_key : 0;
    data->state = keyboard_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

} // extern "C" - 结束所有 LVGL 回调函数的 C 链接规范

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


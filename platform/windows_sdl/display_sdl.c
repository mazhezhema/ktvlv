/**
 * @file display_sdl.c
 * @brief Windows SDL 平台显示驱动实现
 * 
 * 从 src/sdl/sdl.cpp 迁移并适配抽象接口
 */

#include "drivers/display_driver.h"
#include <lvgl.h>
#include <SDL.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_Texture* texture = NULL;
static uint32_t* pixel_buf_static = NULL;  // 全局像素缓冲区
static size_t pixel_buf_size_static = 0;

static int display_sdl_init(void) {
    fprintf(stderr, "[SDL] Initializing display...\n");
    
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        fprintf(stderr, "[SDL] SDL_Init failed: %s\n", SDL_GetError());
        return 0;
    }
    
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
        fprintf(stderr, "[SDL] Window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 0;
    }
    
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "[SDL] Renderer creation failed, trying software: %s\n", SDL_GetError());
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        if (!renderer) {
            fprintf(stderr, "[SDL] Software renderer also failed: %s\n", SDL_GetError());
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 0;
        }
    }
    
    texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STATIC,
        LV_HOR_RES_MAX,
        LV_VER_RES_MAX
    );
    if (!texture) {
        fprintf(stderr, "[SDL] Texture creation failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 0;
    }
    
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    
    fprintf(stderr, "[SDL] Display initialized: %dx%d\n", LV_HOR_RES_MAX, LV_VER_RES_MAX);
    return 1;
}

static void display_sdl_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
    if (!renderer || !texture) {
        lv_disp_flush_ready(drv);
        return;
    }
    
    int32_t x1 = area->x1;
    int32_t y1 = area->y1;
    int32_t x2 = area->x2;
    int32_t y2 = area->y2;
    
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= LV_HOR_RES_MAX) x2 = LV_HOR_RES_MAX - 1;
    if (y2 >= LV_VER_RES_MAX) y2 = LV_VER_RES_MAX - 1;
    
    if (x1 > x2 || y1 > y2) {
        lv_disp_flush_ready(drv);
        return;
    }
    
    int32_t w = (x2 - x1 + 1);
    int32_t h = (y2 - y1 + 1);
    
    if (w <= 0 || h <= 0 || w > LV_HOR_RES_MAX || h > LV_VER_RES_MAX) {
        lv_disp_flush_ready(drv);
        return;
    }
    
    SDL_Rect rect;
    rect.x = x1;
    rect.y = y1;
    rect.w = w;
    rect.h = h;
    
    // 颜色格式转换：LVGL -> SDL ARGB8888
    // 使用动态分配避免栈溢出（大分辨率时）
    size_t needed_size = (size_t)w * h * sizeof(uint32_t);
    
    if (needed_size > pixel_buf_size_static) {
        if (pixel_buf_static) free(pixel_buf_static);
        pixel_buf_static = (uint32_t*)malloc(needed_size);
        if (!pixel_buf_static) {
            fprintf(stderr, "[SDL] Failed to allocate pixel buffer\n");
            lv_disp_flush_ready(drv);
            return;
        }
        pixel_buf_size_static = needed_size;
    }
    uint32_t* pixels = pixel_buf_static;
    
    for (int32_t y = 0; y < h; y++) {
        for (int32_t x = 0; x < w; x++) {
            size_t idx = y * w + x;
            if (idx >= (size_t)(w * h)) break;
            
            lv_color_t color = color_p[idx];
            uint32_t argb = ((uint32_t)color.ch.alpha << 24) | 
                           ((uint32_t)color.ch.red << 16) | 
                           ((uint32_t)color.ch.green << 8) | 
                           (uint32_t)color.ch.blue;
            pixels[idx] = argb;
        }
    }
    
    if (SDL_UpdateTexture(texture, &rect, pixels, w * sizeof(uint32_t)) != 0) {
        fprintf(stderr, "[SDL] UpdateTexture failed: %s\n", SDL_GetError());
        lv_disp_flush_ready(drv);
        return;
    }
    
    if (SDL_RenderCopy(renderer, texture, &rect, &rect) != 0) {
        fprintf(stderr, "[SDL] RenderCopy failed: %s\n", SDL_GetError());
        lv_disp_flush_ready(drv);
        return;
    }
    
    SDL_RenderPresent(renderer);
    lv_disp_flush_ready(drv);
}

static void display_sdl_deinit(void) {
    // 释放像素缓冲区
    if (pixel_buf_static) {
        free(pixel_buf_static);
        pixel_buf_static = NULL;
        pixel_buf_size_static = 0;
    }
    
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = NULL;
    }
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = NULL;
    }
    SDL_Quit();
    fprintf(stderr, "[SDL] Display deinitialized\n");
}

static bool display_sdl_get_resolution(int32_t *width, int32_t *height) {
    if (!window) return false;
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    if (width) *width = w;
    if (height) *height = h;
    return true;
}

// 导出接口实例
display_iface_t DISPLAY = {
    .init = display_sdl_init,
    .flush = display_sdl_flush,
    .deinit = display_sdl_deinit,
    .get_resolution = display_sdl_get_resolution
};


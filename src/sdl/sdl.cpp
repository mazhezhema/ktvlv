#include "sdl.h"
#include <SDL.h>

#ifdef _WIN32
#include <windows.h>
#elif defined(__unix__) || defined(__linux__)
#include <unistd.h>
#endif

static SDL_Window* window = nullptr;
static SDL_Renderer* renderer = nullptr;
static SDL_Texture* texture = nullptr;

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

    // Convert LVGL color format to SDL format
    // Use static buffer to avoid dynamic allocation
    static uint32_t pixel_buf[LV_HOR_RES_MAX * LV_VER_RES_MAX];
    uint32_t* pixels = pixel_buf;
    
    for (int32_t y = 0; y < h; y++) {
        for (int32_t x = 0; x < w; x++) {
            lv_color_t color = color_p[y * w + x];
            pixels[y * w + x] = SDL_MapRGBA(
                SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888),
                color.ch.red,
                color.ch.green,
                color.ch.blue,
                255
            );
        }
    }

    SDL_UpdateTexture(texture, &rect, pixels, w * sizeof(uint32_t));
    SDL_RenderCopy(renderer, texture, &rect, &rect);
    SDL_RenderPresent(renderer);
    lv_disp_flush_ready(disp_drv);
}

void sdl_mouse_read(lv_indev_drv_t* indev_drv, lv_indev_data_t* data) {
    int32_t x = 0, y = 0;
    bool pressed = false;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_MOUSEMOTION) {
            x = e.motion.x;
            y = e.motion.y;
        } else if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) {
            x = e.button.x;
            y = e.button.y;
            pressed = (e.type == SDL_MOUSEBUTTONDOWN);
        }
    }

    data->point.x = x;
    data->point.y = y;
    data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

void sdl_keyboard_read(lv_indev_drv_t* indev_drv, lv_indev_data_t* data) {
    static uint32_t last_key = 0;
    data->key = 0;
    data->state = LV_INDEV_STATE_RELEASED;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_KEYDOWN) {
            last_key = e.key.keysym.sym;
            data->key = last_key;
            data->state = LV_INDEV_STATE_PRESSED;
        } else if (e.type == SDL_KEYUP) {
            if (e.key.keysym.sym == last_key) {
                data->key = last_key;
                data->state = LV_INDEV_STATE_RELEASED;
            }
        }
    }
}


/**
 * @file input_sdl.c
 * @brief Windows SDL 平台输入驱动实现
 */

#include "drivers/input_driver.h"
#include <lvgl.h>
#include <SDL.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

static lv_indev_t* pointer_indev = NULL;
static lv_indev_t* keypad_indev = NULL;

// 全局输入状态（由主循环更新）
static int32_t mouse_x = 0;
static int32_t mouse_y = 0;
static bool mouse_pressed = false;
static uint32_t keyboard_key = 0;
static bool keyboard_pressed = false;

// LVGL 输入读取回调
static void sdl_mouse_read(lv_indev_drv_t* indev_drv, lv_indev_data_t* data) {
    (void)indev_drv;
    data->point.x = mouse_x;
    data->point.y = mouse_y;
    data->state = mouse_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void sdl_keyboard_read(lv_indev_drv_t* indev_drv, lv_indev_data_t* data) {
    (void)indev_drv;
    data->key = keyboard_pressed ? keyboard_key : 0;
    data->state = keyboard_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static int input_sdl_init(void) {
    fprintf(stderr, "[SDL] Input driver initialized\n");
    return 1;
}

static lv_indev_t* input_sdl_register_device(input_device_type_t type) {
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    
    if (type == INPUT_TYPE_POINTER) {
        indev_drv.type = LV_INDEV_TYPE_POINTER;
        indev_drv.read_cb = sdl_mouse_read;
        pointer_indev = lv_indev_drv_register(&indev_drv);
        fprintf(stderr, "[SDL] Pointer device registered\n");
        return pointer_indev;
    } else if (type == INPUT_TYPE_KEYPAD) {
        indev_drv.type = LV_INDEV_TYPE_KEYPAD;
        indev_drv.read_cb = sdl_keyboard_read;
        keypad_indev = lv_indev_drv_register(&indev_drv);
        fprintf(stderr, "[SDL] Keypad device registered\n");
        return keypad_indev;
    }
    
    return NULL;
}

static bool input_sdl_process_event(void *event_data) {
    SDL_Event* e = (SDL_Event*)event_data;
    
    if (e->type == SDL_MOUSEMOTION) {
        mouse_x = e->motion.x;
        mouse_y = e->motion.y;
        return true;
    } else if (e->type == SDL_MOUSEBUTTONDOWN) {
        mouse_x = e->button.x;
        mouse_y = e->button.y;
        mouse_pressed = true;
        return true;
    } else if (e->type == SDL_MOUSEBUTTONUP) {
        mouse_x = e->button.x;
        mouse_y = e->button.y;
        mouse_pressed = false;
        return true;
    } else if (e->type == SDL_KEYDOWN) {
        keyboard_key = e->key.keysym.sym;
        keyboard_pressed = true;
        return true;
    } else if (e->type == SDL_KEYUP) {
        if (e->key.keysym.sym == keyboard_key) {
            keyboard_pressed = false;
        }
        return true;
    }
    
    return false;
}

static void input_sdl_deinit(void) {
    pointer_indev = NULL;
    keypad_indev = NULL;
    fprintf(stderr, "[SDL] Input driver deinitialized\n");
}

// 导出接口实例
input_iface_t INPUT = {
    .init = input_sdl_init,
    .register_device = input_sdl_register_device,
    .process_event = input_sdl_process_event,
    .deinit = input_sdl_deinit
};


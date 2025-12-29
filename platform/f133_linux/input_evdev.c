/**
 * @file input_evdev.c
 * @brief F133 Linux 平台输入驱动实现（evdev）
 * 
 * 支持：
 * - 触摸屏：/dev/input/eventX（触摸事件）
 * - 遥控器：/dev/input/eventX（按键事件）
 * 
 * 注意：此文件为框架模板，需要根据实际 F133 输入设备配置调整
 */

#include "drivers/input_driver.h"
#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <string.h>

// 输入设备路径（根据实际 F133 配置调整）
#define TOUCH_DEVICE "/dev/input/event0"  // 触摸屏
#define KEYPAD_DEVICE "/dev/input/event1"  // 遥控器/键盘

static int touch_fd = -1;
static int keypad_fd = -1;
static lv_indev_t* pointer_indev = NULL;
static lv_indev_t* keypad_indev = NULL;

// 输入状态
static int32_t touch_x = 0;
static int32_t touch_y = 0;
static bool touch_pressed = false;
static uint32_t keypad_key = 0;
static bool keypad_pressed = false;

// LVGL 输入读取回调
static void evdev_touch_read(lv_indev_drv_t* indev_drv, lv_indev_data_t* data) {
    (void)indev_drv;
    data->point.x = touch_x;
    data->point.y = touch_y;
    data->state = touch_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void evdev_keypad_read(lv_indev_drv_t* indev_drv, lv_indev_data_t* data) {
    (void)indev_drv;
    data->key = keypad_pressed ? keypad_key : 0;
    data->state = keypad_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

// 按键码映射（Linux input event -> LVGL key）
static uint32_t map_linux_key_to_lvgl(uint16_t linux_key) {
    switch (linux_key) {
        case KEY_UP: return LV_KEY_UP;
        case KEY_DOWN: return LV_KEY_DOWN;
        case KEY_LEFT: return LV_KEY_LEFT;
        case KEY_RIGHT: return LV_KEY_RIGHT;
        case KEY_ENTER: return LV_KEY_ENTER;
        case KEY_ESC: return LV_KEY_ESC;
        case KEY_BACKSPACE: return LV_KEY_BACKSPACE;
        default: return 0;
    }
}

static int input_evdev_init(void) {
    fprintf(stderr, "[EVDEV] Initializing input devices...\n");
    
    // 打开触摸屏设备（非阻塞）
    touch_fd = open(TOUCH_DEVICE, O_RDONLY | O_NONBLOCK);
    if (touch_fd < 0) {
        fprintf(stderr, "[EVDEV] Warning: Failed to open touch device %s\n", TOUCH_DEVICE);
    } else {
        fprintf(stderr, "[EVDEV] Touch device opened: %s\n", TOUCH_DEVICE);
    }
    
    // 打开遥控器/键盘设备（非阻塞）
    keypad_fd = open(KEYPAD_DEVICE, O_RDONLY | O_NONBLOCK);
    if (keypad_fd < 0) {
        fprintf(stderr, "[EVDEV] Warning: Failed to open keypad device %s\n", KEYPAD_DEVICE);
    } else {
        fprintf(stderr, "[EVDEV] Keypad device opened: %s\n", KEYPAD_DEVICE);
    }
    
    return 1;
}

static lv_indev_t* input_evdev_register_device(input_device_type_t type) {
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    
    if (type == INPUT_TYPE_POINTER) {
        indev_drv.type = LV_INDEV_TYPE_POINTER;
        indev_drv.read_cb = evdev_touch_read;
        pointer_indev = lv_indev_drv_register(&indev_drv);
        fprintf(stderr, "[EVDEV] Pointer device registered\n");
        return pointer_indev;
    } else if (type == INPUT_TYPE_KEYPAD) {
        indev_drv.type = LV_INDEV_TYPE_KEYPAD;
        indev_drv.read_cb = evdev_keypad_read;
        keypad_indev = lv_indev_drv_register(&indev_drv);
        fprintf(stderr, "[EVDEV] Keypad device registered\n");
        return keypad_indev;
    }
    
    return NULL;
}

static bool input_evdev_process_event(void *event_data) {
    // 注意：evdev 事件在主循环中通过 read() 读取
    // 此函数可以用于处理其他类型的事件
    (void)event_data;
    return false;
}

// 从 evdev 设备读取事件（在主循环中调用）
static void evdev_read_events(void) {
    struct input_event ev;
    ssize_t n;
    
    // 读取触摸屏事件
    if (touch_fd >= 0) {
        while ((n = read(touch_fd, &ev, sizeof(ev))) == sizeof(ev)) {
            if (ev.type == EV_ABS) {
                if (ev.code == ABS_X) {
                    touch_x = ev.value;
                } else if (ev.code == ABS_Y) {
                    touch_y = ev.value;
                }
            } else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
                touch_pressed = (ev.value != 0);
            }
        }
    }
    
    // 读取遥控器/键盘事件
    if (keypad_fd >= 0) {
        while ((n = read(keypad_fd, &ev, sizeof(ev))) == sizeof(ev)) {
            if (ev.type == EV_KEY) {
                if (ev.value == 1) {  // 按下
                    keypad_key = map_linux_key_to_lvgl(ev.code);
                    keypad_pressed = true;
                } else if (ev.value == 0) {  // 释放
                    if (map_linux_key_to_lvgl(ev.code) == keypad_key) {
                        keypad_pressed = false;
                    }
                }
            }
        }
    }
}

static void input_evdev_deinit(void) {
    if (touch_fd >= 0) {
        close(touch_fd);
        touch_fd = -1;
    }
    if (keypad_fd >= 0) {
        close(keypad_fd);
        keypad_fd = -1;
    }
    pointer_indev = NULL;
    keypad_indev = NULL;
    fprintf(stderr, "[EVDEV] Input driver deinitialized\n");
}

// 导出接口实例
input_iface_t INPUT = {
    .init = input_evdev_init,
    .register_device = input_evdev_register_device,
    .process_event = input_evdev_process_event,
    .deinit = input_evdev_deinit
};

// 导出 evdev 事件读取函数（供主循环调用）
// 注意：此函数需要在头文件中声明
void evdev_read_events_exported(void) {
    evdev_read_events();
}


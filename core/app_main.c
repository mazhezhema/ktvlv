/**
 * @file app_main.c
 * @brief 应用主入口（跨平台统一入口）
 * 
 * 职责：
 * - 初始化平台驱动（通过抽象接口）
 * - 初始化 LVGL
 * - 初始化 UI 系统
 * - 进入主循环
 */

#include "drivers/display_driver.h"
#include "drivers/input_driver.h"
#include "drivers/audio_driver.h"
#include <lvgl.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include "platform/f133_linux/input_evdev.h"

// LVGL 显示缓冲区（partial refresh，约 1/7 屏幕高度）
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[LV_HOR_RES_MAX * 100];
static lv_color_t buf2[LV_HOR_RES_MAX * 100];

/**
 * @brief 初始化显示系统
 */
static bool init_display_system(void) {
    fprintf(stderr, "[APP] Initializing display system...\n");
    
    // 初始化平台显示驱动
    if (!DISPLAY.init()) {
        fprintf(stderr, "[APP] Display driver init failed\n");
        return false;
    }
    
    // 初始化 LVGL 显示缓冲区（双缓冲 + partial refresh）
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, LV_HOR_RES_MAX * 100);
    
    // 注册 LVGL 显示驱动
    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = DISPLAY.flush;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.hor_res = LV_HOR_RES_MAX;
    disp_drv.ver_res = LV_VER_RES_MAX;
    disp_drv.full_refresh = 0;  // 启用 partial refresh（F133 关键优化）
    
    lv_disp_t* disp = lv_disp_drv_register(&disp_drv);
    if (!disp) {
        fprintf(stderr, "[APP] LVGL display registration failed\n");
        DISPLAY.deinit();
        return false;
    }
    
    fprintf(stderr, "[APP] Display system initialized\n");
    return true;
}

/**
 * @brief 初始化输入系统
 */
static bool init_input_system(void) {
    fprintf(stderr, "[APP] Initializing input system...\n");
    
    // 初始化平台输入驱动
    if (!INPUT.init()) {
        fprintf(stderr, "[APP] Input driver init failed\n");
        return false;
    }
    
    // 注册触摸屏/鼠标
    if (!INPUT.register_device(INPUT_TYPE_POINTER)) {
        fprintf(stderr, "[APP] Warning: Pointer device registration failed\n");
    }
    
    // 注册遥控器/键盘
    if (!INPUT.register_device(INPUT_TYPE_KEYPAD)) {
        fprintf(stderr, "[APP] Warning: Keypad device registration failed\n");
    }
    
    fprintf(stderr, "[APP] Input system initialized\n");
    return true;
}

/**
 * @brief 初始化音频系统（可选）
 */
static bool init_audio_system(void) {
    fprintf(stderr, "[APP] Initializing audio system...\n");
    
    if (!AUDIO.init()) {
        fprintf(stderr, "[APP] Warning: Audio driver init failed (continuing)\n");
        return false;
    }
    
    fprintf(stderr, "[APP] Audio system initialized\n");
    return true;
}

/**
 * @brief 应用主循环
 */
static void app_main_loop(void) {
    fprintf(stderr, "[APP] Entering main loop...\n");
    
    // F133 Linux 主循环
    while (1) {
        // 读取 evdev 事件
        evdev_read_events_exported();
        
        // LVGL 定时器处理
        lv_timer_handler();
        
        // 短暂休眠
        usleep(5000);  // 5ms
    }
}

/**
 * @brief 应用初始化（跨平台统一入口）
 */
int app_main_init(void) {
    fprintf(stderr, "=== KTV LVGL Application Start ===\n");
    
    // 1. 初始化 LVGL（必须在显示之前）
    fprintf(stderr, "[APP] Initializing LVGL...\n");
    lv_init();
    
    // 2. 初始化显示系统
    if (!init_display_system()) {
        fprintf(stderr, "[APP] Display initialization failed\n");
        return -1;
    }
    
    // 3. 初始化输入系统
    if (!init_input_system()) {
        fprintf(stderr, "[APP] Input initialization failed\n");
        DISPLAY.deinit();
        return -1;
    }
    
    // 4. 初始化音频系统（可选）
    init_audio_system();
    
    fprintf(stderr, "[APP] Application initialized successfully\n");
    return 0;
}

/**
 * @brief 应用清理（跨平台统一清理）
 */
void app_main_cleanup(void) {
    fprintf(stderr, "[APP] Cleaning up...\n");
    
    AUDIO.deinit();
    INPUT.deinit();
    DISPLAY.deinit();
    
    fprintf(stderr, "[APP] Cleanup complete\n");
}

/**
 * @brief 应用主入口（平台特定 main 函数调用此函数）
 */
int app_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    if (app_main_init() != 0) {
        return -1;
    }
    
    // TODO: 初始化 UI 系统（调用 UI 层初始化）
    // ktv::ui::init_ui_system(LV_HOR_RES_MAX, LV_VER_RES_MAX);
    
    // TODO: 创建并加载主屏幕
    // lv_obj_t* scr = ktv::ui::create_main_screen();
    // lv_scr_load(scr);
    
    // 进入主循环
    app_main_loop();
    
    // 清理
    app_main_cleanup();
    
    return 0;
}


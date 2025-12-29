#include <lvgl.h>
#include <SDL.h>
#include "ui/layouts.h"
#include "ui/page_manager.h"
#include "ui/ui_scale.h"
#include "sdl/sdl.h"
#include "logging/logger.h"
#include "config/config.h"
#include "services/http_service.h"
#include "services/song_service.h"
#include "services/licence_service.h"
#include "services/history_service.h"
#include "services/m3u8_download_service.h"
#include "services/player_service.h"
#include "events/event_bus.h"

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[LV_HOR_RES_MAX * 120];

static void init_display() {
    sdl_init();

    lv_disp_draw_buf_init(&draw_buf, buf1, nullptr, LV_HOR_RES_MAX * 120);

    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = sdl_display_flush;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.hor_res = LV_HOR_RES_MAX;
    disp_drv.ver_res = LV_VER_RES_MAX;
    lv_disp_drv_register(&disp_drv);
}

static void init_input() {
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = sdl_mouse_read;
    lv_indev_drv_register(&indev_drv);

    lv_indev_drv_t kb_drv;
    lv_indev_drv_init(&kb_drv);
    kb_drv.type = LV_INDEV_TYPE_KEYPAD;
    kb_drv.read_cb = sdl_keyboard_read;
    lv_indev_drv_register(&kb_drv);
}

#ifdef __cplusplus
extern "C"
#endif
int SDL_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    lv_init();
    ktv::logging::init();  // 控制台日志
    ktv::config::NetworkConfig net_cfg;
    bool cfg_ok = ktv::config::loadFromFile("config.ini", net_cfg);
    if (!cfg_ok) {
        PLOGW << "config.ini not found or parse fail, using defaults.";
    }
    
    init_display();
    init_input();
    
    // 初始化UI系统（缩放、焦点、主题）
    ktv::ui::init_ui_system(LV_HOR_RES_MAX, LV_VER_RES_MAX);

    // 初始化服务（占位/可选参数）
    ktv::services::HttpService::getInstance().initialize(net_cfg.base_url, net_cfg.timeout);
    ktv::services::LicenceService::getInstance().initialize();
    ktv::services::HistoryService::getInstance().setCapacity(50);
    ktv::services::M3u8DownloadService::getInstance().initialize();

    PLOGI << "创建主屏幕...";
    lv_obj_t* scr = ktv::ui::create_main_screen();
    if (!scr) {
        PLOGE << "创建主屏幕失败！";
        return -1;
    }
    
    PLOGI << "加载屏幕...";
    lv_scr_load(scr);
    
    // 确保屏幕有内容，触发一次刷新
    PLOGI << "触发屏幕刷新...";
    lv_obj_invalidate(scr);
    lv_refr_now(nullptr);
    
    // 再刷新几次确保内容显示
    for (int i = 0; i < 3; i++) {
        lv_timer_handler();
        SDL_Delay(10);
    }
    
    PLOGI << "初始化完成，进入主循环";

    bool quit = false;
    SDL_Event e;
    while (!quit) {
        // 处理SDL事件
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                quit = true;
            } else {
                // 更新输入设备状态
                sdl_update_mouse_state(&e);
                sdl_update_keyboard_state(&e);
            }
        }
        
        // 处理LVGL定时器（这会触发输入设备读取和屏幕刷新）
        uint32_t task_delay = lv_timer_handler();
        
        // 使用LVGL建议的延迟时间，但最小5ms
        SDL_Delay(task_delay > 5 ? task_delay : 5);
    }
    return 0;
}


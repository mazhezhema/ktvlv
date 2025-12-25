#include <lvgl.h>
#include <SDL.h>
#include "ui/layouts.h"
#include "ui/page_manager.h"
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

int main() {
    lv_init();
    ktv::logging::init();  // 控制台日志
    ktv::config::NetworkConfig net_cfg;
    bool cfg_ok = ktv::config::loadFromFile("config.ini", net_cfg);
    if (!cfg_ok) {
        PLOGW << "config.ini not found or parse fail, using defaults.";
    }
    ktv::ui::init_ui_theme();
    init_display();
    init_input();

    // 初始化服务（占位/可选参数）
    ktv::services::HttpService::getInstance().initialize(net_cfg.base_url, net_cfg.timeout);
    ktv::services::LicenceService::getInstance().initialize();
    ktv::services::HistoryService::getInstance().setCapacity(50);
    ktv::services::M3u8DownloadService::getInstance().initialize();

    lv_obj_t* scr = ktv::ui::create_main_screen();
    lv_scr_load(scr);

    bool running = true;
    while (running) {
        sdl_handle_events();  // 处理 SDL 事件（窗口关闭、鼠标、键盘等）
        lv_timer_handler();
        SDL_Delay(5);
    }
    return 0;
}


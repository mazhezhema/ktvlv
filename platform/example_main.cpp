/**
 * @file example_main.cpp
 * @brief 使用新驱动抽象层的示例主文件
 * 
 * 此文件展示如何将现有的 main.cpp 迁移到新架构
 * 
 * 迁移步骤：
 * 1. 包含新的驱动接口头文件
 * 2. 使用 DISPLAY/INPUT/AUDIO 接口替代直接调用 SDL
 * 3. 调用 app_main() 或自行实现初始化流程
 */

#include "core/app_main.h"
#include "drivers/display_driver.h"
#include "drivers/input_driver.h"
#include "drivers/audio_driver.h"

// 现有 UI 和服务层头文件
#include "ui/layouts.h"
#include "ui/page_manager.h"
#include "ui/ui_scale.h"
#include "logging/logger.h"
#include "config/config.h"
#include "services/http_service.h"
#include "services/song_service.h"
#include "services/licence_service.h"
#include "services/history_service.h"
#include "services/m3u8_download_service.h"
#include "services/player_service.h"
#include "events/event_bus.h"

#include <lvgl.h>
#include <cstdio>
#include <exception>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#endif

#ifdef KTV_PLATFORM_WINDOWS_SDL
#include <SDL.h>
#endif

/**
 * @brief 使用新架构的主函数
 * 
 * 方式1：直接调用 app_main()（最简单）
 * 方式2：自行实现初始化流程（更灵活）
 */
int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif

    fprintf(stderr, "=== KTV LVGL Program Start (New Architecture) ===\n");
    ktv::logging::init();

    try {
        // 方式1：使用 app_main()（推荐用于快速迁移）
        // return app_main(argc, argv);

        // 方式2：自行实现初始化流程（更灵活，可以集成现有服务）
        // ============================================================
        
        // 1. 初始化 LVGL
        PLOGI << "Initializing LVGL...";
        lv_init();
        
        // 2. 加载配置
        PLOGI << "Loading config file...";
        ktv::config::NetworkConfig net_cfg;
        bool cfg_ok = ktv::config::loadFromFile("config.ini", net_cfg);
        if (!cfg_ok) {
            PLOGW << "config.ini not found or parse fail, using defaults.";
        }
        
        // 3. 初始化显示系统（使用抽象接口）
        PLOGI << "Initializing display system...";
        if (!DISPLAY.init()) {
            PLOGE << "Display initialization failed!";
            return -1;
        }
        
        // 注册 LVGL 显示驱动
        static lv_disp_draw_buf_t draw_buf;
        static lv_color_t buf1[LV_HOR_RES_MAX * 100];
        static lv_color_t buf2[LV_HOR_RES_MAX * 100];
        
        lv_disp_draw_buf_init(&draw_buf, buf1, buf2, LV_HOR_RES_MAX * 100);
        
        lv_disp_drv_t disp_drv;
        lv_disp_drv_init(&disp_drv);
        disp_drv.flush_cb = DISPLAY.flush;  // 使用抽象接口
        disp_drv.draw_buf = &draw_buf;
        disp_drv.hor_res = LV_HOR_RES_MAX;
        disp_drv.ver_res = LV_VER_RES_MAX;
        disp_drv.full_refresh = 0;
        
        lv_disp_t* disp = lv_disp_drv_register(&disp_drv);
        if (!disp) {
            PLOGE << "LVGL display registration failed!";
            DISPLAY.deinit();
            return -1;
        }
        
        // 4. 初始化输入系统（使用抽象接口）
        PLOGI << "Initializing input system...";
        if (!INPUT.init()) {
            PLOGE << "Input initialization failed!";
            DISPLAY.deinit();
            return -1;
        }
        
        INPUT.register_device(INPUT_TYPE_POINTER);
        INPUT.register_device(INPUT_TYPE_KEYPAD);
        
        // 5. 初始化音频系统（可选）
        AUDIO.init();
        
        // 6. 初始化 UI 系统
        PLOGI << "Initializing UI system...";
        ktv::ui::init_ui_system(LV_HOR_RES_MAX, LV_VER_RES_MAX);
        
        // 7. 初始化服务
        PLOGI << "Initializing services...";
        ktv::services::HttpService::getInstance().initialize(net_cfg.base_url, net_cfg.timeout);
        ktv::services::LicenceService::getInstance().initialize();
        ktv::services::HistoryService::getInstance().setCapacity(50);
        ktv::services::M3u8DownloadService::getInstance().initialize();
        
        // 8. 创建主屏幕
        PLOGI << "Creating main screen...";
        // TODO: 创建并加载主屏幕
        // lv_obj_t* scr = ktv::ui::create_main_screen();
        // lv_scr_load(scr);
        
        // 9. 进入主循环
        PLOGI << "Entering main loop...";
        
#ifdef KTV_PLATFORM_WINDOWS_SDL
        SDL_Event e;
        bool quit = false;
        
        while (!quit) {
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) {
                    quit = true;
                    break;
                }
                INPUT.process_event(&e);  // 使用抽象接口处理事件
            }
            
            lv_timer_handler();
            SDL_Delay(5);
        }
#elif defined(KTV_PLATFORM_F133)
        #include "platform/f133_linux/input_evdev.h"
        while (1) {
            evdev_read_events_exported();
            lv_timer_handler();
            usleep(5000);
        }
#endif
        
        // 10. 清理
        PLOGI << "Cleaning up...";
        AUDIO.deinit();
        INPUT.deinit();
        DISPLAY.deinit();
        
        return 0;
        
    } catch (const std::exception& e) {
        PLOGE << "Exception: " << e.what();
        return -1;
    }
}


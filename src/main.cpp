#include <lvgl.h>
#include <SDL.h>
#include "ui/layouts.h"
#include "ui/page_manager.h"
#include "sdl/sdl.h"
#include "logging/logger.h"
#include "config/config.h"
#include "services/http_service.h"
#include "services/song_service.h"
#include "services/cache_service.h"
#include "services/task_service.h"
#include "services/licence_service.h"
#include <exception>
#include <stdexcept>
#include "services/history_service.h"
#include "services/m3u8_download_service.h"
#include "services/player_service.h"
#include "events/event_bus.h"

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[LV_HOR_RES_MAX * 120];
static lv_disp_t* g_display = nullptr;  // 保存显示驱动指针，避免重复查询
static lv_disp_drv_t disp_drv;  // 必须声明为 static，因为 LVGL 会保存指向它的指针
static lv_indev_drv_t indev_drv;  // 必须声明为 static，因为 LVGL 会保存指向它的指针
static lv_indev_drv_t kb_drv;  // 必须声明为 static，因为 LVGL 会保存指向它的指针

static void init_display() {
    // ⚡ 步骤 1: 验证宏定义是否正确
    const lv_coord_t EXPECTED_HOR_RES = 1280;
    const lv_coord_t EXPECTED_VER_RES = 720;
    
    printf("=== Display Initialization ===\n");
    printf("LV_HOR_RES_MAX = %d (expected %d)\n", (int)LV_HOR_RES_MAX, (int)EXPECTED_HOR_RES);
    printf("LV_VER_RES_MAX = %d (expected %d)\n", (int)LV_VER_RES_MAX, (int)EXPECTED_VER_RES);
    fflush(stdout);
    
    // 硬防御断言：确保宏定义正确
    if (LV_HOR_RES_MAX != EXPECTED_HOR_RES || LV_VER_RES_MAX != EXPECTED_VER_RES) {
        printf("ERROR: LVGL resolution macros mismatch! Check lv_conf.h\n");
        fflush(stdout);
        return;
    }
    
    // ⚡ 步骤 2: 初始化 SDL（必须在 LVGL 之前）
    printf("Step 1: Initializing SDL...\n");
    fflush(stdout);
    sdl_init();
    printf("Step 1: SDL initialized\n");
    fflush(stdout);
    
    // ⚡ 步骤 3: 初始化 LVGL 显示缓冲区
    printf("Step 2: Initializing LVGL draw buffer...\n");
    fflush(stdout);
    lv_disp_draw_buf_init(&draw_buf, buf1, nullptr, LV_HOR_RES_MAX * 120);
    printf("Step 2: Draw buffer initialized (size: %zu bytes)\n", sizeof(buf1) / sizeof(buf1[0]));
    fflush(stdout);
    
    // ⚡ 步骤 4: 初始化显示驱动结构体（会清零所有字段）
    printf("Step 3: Initializing display driver structure...\n");
    fflush(stdout);
    lv_disp_drv_init(&disp_drv);
    
    // ⚡ 步骤 5: 提前设置 flush_cb（优化：在设置其他参数之前注册回调）
    // 这样可以确保 flush_cb 在整个驱动生命周期内都可用
    printf("Step 4: Registering flush callback (early registration)...\n");
    fflush(stdout);
    disp_drv.flush_cb = sdl_display_flush;
    
    // ⚡ 步骤 6: 设置显示驱动参数（必须在 init 之后，register 之前）
    printf("Step 5: Setting display driver parameters...\n");
    fflush(stdout);
    disp_drv.draw_buf = &draw_buf;
    
    // ⚡ 关键：设置分辨率（必须在注册前设置，使用常量值确保正确）
    disp_drv.hor_res = EXPECTED_HOR_RES;
    disp_drv.ver_res = EXPECTED_VER_RES;
    
    // 硬防御断言：验证分辨率已正确设置
    if (disp_drv.hor_res != EXPECTED_HOR_RES || disp_drv.ver_res != EXPECTED_VER_RES) {
        printf("ERROR: Display resolution not set correctly before registration!\n");
        printf("Expected: %dx%d, Got: %dx%d\n", 
               (int)EXPECTED_HOR_RES, (int)EXPECTED_VER_RES,
               (int)disp_drv.hor_res, (int)disp_drv.ver_res);
        fflush(stdout);
        return;
    }
    
    // 验证 draw_buf 是否正确初始化
    if (!disp_drv.draw_buf || !disp_drv.draw_buf->buf1) {
        printf("ERROR: draw_buf not properly initialized!\n");
        fflush(stdout);
        return;
    }
    
    printf("Step 5: Display driver parameters set: %dx%d\n", 
           (int)disp_drv.hor_res, (int)disp_drv.ver_res);
    fflush(stdout);
    
    // ⚡ 步骤 7: 注册显示驱动（flush_cb 已提前设置）
    printf("Step 6: Registering display driver...\n");
    fflush(stdout);
    lv_disp_t* disp = lv_disp_drv_register(&disp_drv);
    if (!disp) {
        PLOGE << "Failed to register display driver";
        printf("ERROR: Failed to register display driver\n");
        fflush(stdout);
        return;
    }
    
    // 保存显示驱动指针
    g_display = disp;
    printf("Step 6: Display driver registered successfully (flush_cb ready)\n");
    fflush(stdout);
    
    // ⚡ 步骤 8: 验证注册后的分辨率（硬防御检查）
    printf("Step 7: Verifying display resolution after registration...\n");
    fflush(stdout);
    
    // 使用注册返回的指针验证
    lv_coord_t reg_w = lv_disp_get_hor_res(disp);
    lv_coord_t reg_h = lv_disp_get_ver_res(disp);
    printf("Registered display resolution: %dx%d\n", (int)reg_w, (int)reg_h);
    fflush(stdout);
    
    // 硬防御断言：分辨率必须匹配
    if (reg_w != EXPECTED_HOR_RES || reg_h != EXPECTED_VER_RES) {
        printf("ERROR: Display resolution corrupted after registration!\n");
        printf("Expected: %dx%d, Got: %dx%d\n", 
               (int)EXPECTED_HOR_RES, (int)EXPECTED_VER_RES,
               (int)reg_w, (int)reg_h);
        printf("This indicates memory corruption or initialization order issue!\n");
        fflush(stdout);
        // 不返回，继续运行以便调试
    } else {
        printf("Step 7: Display resolution verified: OK\n");
        fflush(stdout);
    }
    
    // 也检查默认显示（应该指向同一个显示）
    lv_coord_t default_w = lv_disp_get_hor_res(NULL);
    lv_coord_t default_h = lv_disp_get_ver_res(NULL);
    printf("Default display resolution: %dx%d\n", (int)default_w, (int)default_h);
    fflush(stdout);
    
    if (default_w != reg_w || default_h != reg_h) {
        printf("WARNING: Default display resolution mismatch with registered display!\n");
        fflush(stdout);
    }
    
    printf("=== Display Initialization Complete ===\n");
    fflush(stdout);
    
    PLOGI << "Display driver registered, resolution: " << EXPECTED_HOR_RES << "x" << EXPECTED_VER_RES;
}

static void init_input() {
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = sdl_mouse_read;
    lv_indev_drv_register(&indev_drv);

    lv_indev_drv_init(&kb_drv);
    kb_drv.type = LV_INDEV_TYPE_KEYPAD;
    kb_drv.read_cb = sdl_keyboard_read;
    lv_indev_drv_register(&kb_drv);
}

// SDL2 在 Windows 上需要 SDL_main 作为入口点
#ifdef _WIN32
#include <SDL_main.h>
#endif

int main(int argc, char* argv[]) {
    printf("\n=== KTVLV Application Starting ===\n");
    fflush(stdout);
    
    // ⚡ 步骤 1: 初始化日志（必须在最前面）
    ktv::logging::init();
    PLOGI << "Starting KTVLV application...";
    
    // ⚡ 步骤 2: 初始化 LVGL（必须在显示驱动之前）
    printf("Initializing LVGL...\n");
    fflush(stdout);
    lv_init();
    PLOGI << "LVGL initialized";
    printf("LVGL initialized\n");
    fflush(stdout);
    
    // ⚡ 步骤 3: 加载配置（不阻塞初始化）
    ktv::config::NetworkConfig net_cfg;
    bool cfg_ok = ktv::config::loadFromFile("config.ini", net_cfg);
    if (!cfg_ok) {
        PLOGW << "config.ini not found or parse fail, using defaults.";
    }
    
    // ⚡ 步骤 4: 初始化显示驱动（必须在 UI 主题之前，因为主题可能依赖显示）
    init_display();
    PLOGI << "Display initialized";
    
    // 硬防御检查：验证显示分辨率在显示初始化后仍然正确
    if (g_display) {
        lv_coord_t w = lv_disp_get_hor_res(g_display);
        lv_coord_t h = lv_disp_get_ver_res(g_display);
        if (w != 1280 || h != 720) {
            printf("CRITICAL ERROR: Display resolution corrupted immediately after init_display()!\n");
            printf("Expected: 1280x720, Got: %dx%d\n", (int)w, (int)h);
            fflush(stdout);
            // 继续运行以便调试
        }
    }
    
    // ⚡ 步骤 5: 初始化 UI 主题（在显示驱动之后）
    printf("Initializing UI theme...\n");
    fflush(stdout);
    ktv::ui::init_ui_theme();
    PLOGI << "UI theme initialized";
    printf("UI theme initialized\n");
    fflush(stdout);
    
    // ⚡ 步骤 6: 初始化输入设备
    printf("Initializing input devices...\n");
    fflush(stdout);
    init_input();
    PLOGI << "Input initialized";
    printf("Input initialized\n");
    fflush(stdout);

    // ⚡ 步骤 7: 验证显示分辨率（在服务初始化前）
    printf("Verifying display resolution before service initialization...\n");
    fflush(stdout);
    if (g_display) {
        lv_coord_t w1 = lv_disp_get_hor_res(g_display);
        lv_coord_t h1 = lv_disp_get_ver_res(g_display);
        printf("Display resolution BEFORE services init: %dx%d\n", (int)w1, (int)h1);
        if (w1 != 1280 || h1 != 720) {
            printf("ERROR: Display resolution already corrupted before services init!\n");
            fflush(stdout);
        }
    }

    // ⚡ 步骤 8: 初始化基础服务
    printf("Initializing services...\n");
    fflush(stdout);
    ktv::services::HttpService::getInstance().initialize(net_cfg.base_url, net_cfg.timeout);
    ktv::services::LicenceService::getInstance().initialize();
    ktv::services::HistoryService::getInstance().setCapacity(50);
    ktv::services::M3u8DownloadService::getInstance().initialize();
    
    // ⚡ 步骤 8.1: 网络服务初始化将在后台线程异步执行（不阻塞主线程）
    // 所有耗时操作（网络请求、IO）都移到主循环启动后执行
    printf("Network services will be initialized asynchronously after UI loads...\n");
    fflush(stdout);
    
    // 验证显示分辨率（在TaskService初始化前）
    if (g_display) {
        lv_coord_t w2 = lv_disp_get_hor_res(g_display);
        lv_coord_t h2 = lv_disp_get_ver_res(g_display);
        printf("Display resolution BEFORE TaskService init: %dx%d\n", (int)w2, (int)h2);
        if (w2 != 1280 || h2 != 720) {
            printf("ERROR: Display resolution corrupted before TaskService init!\n");
            fflush(stdout);
        }
    }
    
    // ⚡ 步骤 9: 初始化任务服务（异步任务队列，必须在LVGL初始化后）
    printf("Initializing TaskService...\n");
    fflush(stdout);
    if (!ktv::services::TaskService::getInstance().initialize()) {
        PLOGE << "Failed to initialize task service";
        return 1;
    }
    PLOGI << "Task service initialized";
    printf("TaskService initialized\n");
    fflush(stdout);
    
    // 验证显示分辨率（在TaskService初始化后）
    if (g_display) {
        lv_coord_t w3 = lv_disp_get_hor_res(g_display);
        lv_coord_t h3 = lv_disp_get_ver_res(g_display);
        printf("Display resolution AFTER TaskService init: %dx%d\n", (int)w3, (int)h3);
        if (w3 != 1280 || h3 != 720) {
            printf("ERROR: Display resolution corrupted after TaskService init!\n");
            printf("This indicates TaskService initialization may be corrupting memory!\n");
            fflush(stdout);
        }
    }
    
    // ⚡ 步骤 10: 设置SongService的网络配置（快速操作，不阻塞）
    ktv::services::SongService::getInstance().setNetworkConfig(net_cfg);
    
    // ⚡ 注意：CacheService 初始化移到后台线程，避免文件系统操作阻塞主线程
    // std::filesystem::create_directories() 在某些情况下可能阻塞（网络驱动器、权限问题等）

    // 验证显示分辨率（在创建屏幕前）
    if (g_display) {
        lv_coord_t w4 = lv_disp_get_hor_res(g_display);
        lv_coord_t h4 = lv_disp_get_ver_res(g_display);
        printf("Display resolution BEFORE create_main_screen: %dx%d\n", (int)w4, (int)h4);
        if (w4 != 1280 || h4 != 720) {
            printf("ERROR: Display resolution corrupted before create_main_screen!\n");
            fflush(stdout);
        }
    }

    PLOGI << "Creating main screen...";
    lv_obj_t* scr = ktv::ui::create_main_screen();
    if (!scr) {
        PLOGE << "Failed to create main screen";
        return 1;
    }
    PLOGI << "Main screen created";
    
    // 在加载屏幕前检查显示分辨率
    lv_disp_t* disp_before = lv_disp_get_default();
    if (disp_before) {
        lv_coord_t disp_w_before = lv_disp_get_hor_res(disp_before);
        lv_coord_t disp_h_before = lv_disp_get_ver_res(disp_before);
        printf("Display resolution BEFORE lv_scr_load: %dx%d\n", (int)disp_w_before, (int)disp_h_before);
        fflush(stdout);
    }
    
    // ⚡ 关键修复：在加载屏幕之前，确保屏幕对象的大小和位置正确
    lv_obj_set_pos(scr, 0, 0);
    lv_obj_set_size(scr, LV_HOR_RES_MAX, LV_VER_RES_MAX);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(scr, LV_OPA_COVER, 0);
    
    printf("Before lv_scr_load: Screen size set to %dx%d\n", (int)LV_HOR_RES_MAX, (int)LV_VER_RES_MAX);
    fflush(stdout);
    
    lv_scr_load(scr);
    PLOGI << "Main screen loaded";
    printf("Main screen loaded, forcing refresh...\n");
    fflush(stdout);
    
    // ⚡ 关键修复：加载后再次确保屏幕大小正确（LVGL 可能会重置）
    lv_obj_set_pos(scr, 0, 0);
    lv_obj_set_size(scr, LV_HOR_RES_MAX, LV_VER_RES_MAX);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(scr, LV_OPA_COVER, 0);
    
    // 验证屏幕对象的大小
    lv_area_t scr_coords;
    lv_obj_get_coords(scr, &scr_coords);
    lv_coord_t scr_w = lv_area_get_width(&scr_coords);
    lv_coord_t scr_h = lv_area_get_height(&scr_coords);
    printf("Screen object after load: size=%dx%d, pos=(%d,%d)\n",
           (int)scr_w, (int)scr_h, (int)scr_coords.x1, (int)scr_coords.y1);
    fflush(stdout);
    
    // 在加载后立即检查显示分辨率
    lv_disp_t* disp_after = g_display ? g_display : lv_disp_get_default();
    if (disp_after) {
        lv_coord_t disp_w_after = lv_disp_get_hor_res(disp_after);
        lv_coord_t disp_h_after = lv_disp_get_ver_res(disp_after);
        printf("Display resolution AFTER lv_scr_load: %dx%d\n", 
               (int)disp_w_after, (int)disp_h_after);
        fflush(stdout);
        
        // 标记整个屏幕需要刷新（让 LVGL 自动调度刷新，避免过度调用 lv_refr_now）
        lv_obj_invalidate(scr);
        printf("Screen invalidated, LVGL will schedule refresh automatically\n");
        fflush(stdout);
        
        // 只调用一次 lv_refr_now 进行初始刷新（后续由 lv_timer_handler 自动处理）
        lv_refr_now(disp_after);
        printf("Initial refresh completed, flush callback should have been called\n");
        fflush(stdout);
        PLOGI << "Screen refreshed, size: " << LV_HOR_RES_MAX << "x" << LV_VER_RES_MAX;
    } else {
        PLOGE << "No display found!";
        printf("ERROR: No display found!\n");
        fflush(stdout);
        return 1;
    }

    // ⚡ 步骤 11: 立即启动主循环（不等待任何初始化）
    // 关键：主循环必须在所有耗时操作之前启动，确保 UI 立即响应
    printf("========================================\n");
    printf("CRITICAL: Starting main loop IMMEDIATELY...\n");
    printf("All blocking operations will run in background thread\n");
    printf("========================================\n");
    fflush(stdout);
    
    // ⚡ 步骤 12: 异步初始化所有耗时服务（不阻塞主线程）
    // 所有耗时操作（文件系统、网络请求、IO）都在后台线程执行
    // 注意：这个调用是异步的，不会阻塞主循环
    ktv::services::TaskService::getInstance().runAsync([net_cfg]() {
        // ⚡ 首先初始化 CacheService（文件系统操作，可能阻塞）
        printf("[Background] Initializing CacheService...\n");
        fflush(stdout);
        if (!ktv::services::CacheService::getInstance().initialize("cache")) {
            PLOGW << "Failed to initialize cache service, continuing without cache";
            printf("[Background] CacheService initialization failed, continuing without cache\n");
        } else {
            PLOGI << "Cache service initialized";
            printf("[Background] CacheService initialized successfully\n");
        }
        fflush(stdout);
        
        // ⚡ 然后执行网络请求
        printf("[Background] Starting token authentication...\n");
        fflush(stdout);
        
        std::string token;
        if (!net_cfg.license.empty()) {
            printf("[Background] Step 1: Calling /karaoke_sdk/vod_token_by_macid API...\n");
            printf("[Background]   License: %s\n", net_cfg.license.c_str());
            printf("[Background]   Company: %s\n", net_cfg.company.c_str());
            printf("[Background]   App Name: %s\n", net_cfg.app_name.c_str());
            fflush(stdout);
            
            // 获取 MAC 地址作为 macid
            std::string macid = ktv::services::LicenceService::getMacAddress();
            printf("[Background]   MAC ID: %s\n", macid.c_str());
            fflush(stdout);
            
            // 网络请求在后台线程执行，不阻塞主线程
            token = ktv::services::LicenceService::getInstance().getTokenFromLicense(
                net_cfg.license, net_cfg.company, net_cfg.app_name, macid);
            
            if (!token.empty()) {
                printf("[Background] Token obtained successfully (length: %zu)\n", token.length());
                fflush(stdout);
                
                // 设置 token 到 SongService（需要在UI线程执行，因为可能触发UI更新）
                ktv::services::TaskService::getInstance().runOnUIThread([token]() {
                    ktv::services::SongService::getInstance().setToken(token);
                    printf("[UI Thread] Token set to SongService\n");
                    fflush(stdout);
                    PLOGI << "Token set to SongService, length: " << token.length();
                });
                
                // 继续在后台线程获取配置和检查更新
                printf("[Background] Fetching runtime config...\n");
                fflush(stdout);
                bool config_ok = ktv::services::LicenceService::getInstance().getRuntimeConfig(
                    token, net_cfg.platform, net_cfg.company, net_cfg.app_name, net_cfg.vn);
                if (config_ok) {
                    printf("[Background] Runtime config loaded successfully\n");
                    fflush(stdout);
                    PLOGI << "Runtime configuration loaded";
                } else {
                    // 网络失败是正常现象（离线、网络不通、服务器异常等）
                    printf("[Background] WARNING: Failed to load runtime config (network unavailable or offline)\n");
                    printf("[Background]   This is normal - application will continue in offline mode\n");
                    fflush(stdout);
                    PLOGW << "Runtime configuration failed (network unavailable), continuing in offline mode";
                }
                
                // 检查更新（可选，失败不影响程序运行）
                printf("[Background] Checking for updates...\n");
                fflush(stdout);
                std::string update_url = ktv::services::LicenceService::getInstance().checkUpdate(
                    token, net_cfg.platform, net_cfg.vn, net_cfg.license, 
                    net_cfg.company, net_cfg.app_name);
                if (!update_url.empty()) {
                    printf("[Background] Update available: %s\n", update_url.c_str());
                    fflush(stdout);
                    PLOGI << "Update available: " << update_url;
                } else {
                    // 更新检查失败是正常现象（离线、网络不通、服务器异常等）
                    printf("[Background] No update available (or network unavailable)\n");
                    printf("[Background]   This is normal - application continues to work\n");
                    fflush(stdout);
                }
            } else {
                // Token获取失败是正常现象（离线、网络不通、服务器异常等）
                // 程序会继续运行，使用离线模式
                printf("[Background] WARNING: Token acquisition failed (network unavailable or offline)\n");
                printf("[Background]   This is normal - application will continue in offline mode\n");
                printf("[Background]   Check debug_token_response.json for details\n");
                fflush(stdout);
                PLOGW << "Token acquisition failed (network unavailable), continuing in offline mode";
            }
        } else {
            printf("[Background] WARNING: No license configured, skipping token acquisition\n");
            fflush(stdout);
            PLOGW << "No license configured, skipping token acquisition";
        }
        
        printf("[Background] Service initialization complete\n");
        fflush(stdout);
        
        // ⚡ 优化：CacheService 初始化完成后，更新 UI 显示首页内容
        // ⚡ 注意：首页内容已经在 create_main_screen 中立即显示了
        // 这里不再需要重复调用 show_home_tab，因为 UI 应该已经可见
        // 如果需要在后台初始化完成后刷新数据，可以在这里调用数据更新逻辑
        printf("[Background] CacheService initialized, UI should already be visible\n");
        fflush(stdout);
    });
    
    // ⚡ 主循环立即启动（不等待任何后台初始化）
    PLOGI << "Application started, entering main loop...";
    printf("========================================\n");
    printf("[MAIN] Entering main event loop NOW...\n");
    printf("[MAIN] UI should be visible, all blocking ops in background\n");
    printf("========================================\n");
    fflush(stdout);
    
    bool running = true;
    int loop_count = 0;
    uint32_t last_print_time = SDL_GetTicks();
    uint32_t loop_start_time = SDL_GetTicks();
    
    while (running) {
        try {
            // ⚡ 关键：每次循环都检查，确保快速响应
            // 处理 SDL 事件（必须快速执行，不阻塞）
            running = sdl_handle_events();
            if (!running) {
                printf("[MAIN] SDL event handler requested exit (SDL_QUIT event received)\n");
                printf("[MAIN] This usually means the window was closed by user\n");
                fflush(stdout);
                break;
            }
        } catch (const std::exception& e) {
            printf("[MAIN] Exception in main loop: %s\n", e.what());
            fflush(stdout);
            PLOGE << "Exception in main loop: " << e.what();
            // 继续运行，不退出
        } catch (...) {
            printf("[MAIN] Unknown exception in main loop\n");
            fflush(stdout);
            PLOGE << "Unknown exception in main loop";
            // 继续运行，不退出
        }
        
        try {
            // 处理 LVGL 定时器和刷新（必须快速执行，不阻塞）
            // lv_timer_handler() 返回建议的延迟时间（毫秒），0 表示需要立即处理
            uint32_t task_delay = lv_timer_handler();
            
            // 第一次循环：强制刷新并输出确认信息
            if (loop_count == 0) {
                uint32_t elapsed = SDL_GetTicks() - loop_start_time;
                lv_disp_t* disp = g_display ? g_display : lv_disp_get_default();
                if (disp) {
                    lv_refr_now(disp);  // 仅第一次强制刷新
                }
                printf("[MAIN] Loop #%d STARTED (elapsed: %ums), initial refresh completed\n", 
                       loop_count, elapsed);
                printf("[MAIN] Main loop is RUNNING, UI should be responsive now\n");
                fflush(stdout);
            }
            
            // 每50次循环打印一次状态（更频繁，便于诊断）
            if (loop_count > 0 && loop_count % 50 == 0) {
                uint32_t current_time = SDL_GetTicks();
                uint32_t elapsed = current_time - last_print_time;
                printf("[MAIN] Loop #%d running (elapsed: %ums, task_delay: %u, total: %ums, running=%d)\n", 
                       loop_count, elapsed, task_delay, current_time - loop_start_time, running);
                fflush(stdout);
                last_print_time = current_time;
            }
            
            loop_count++;
            
            // 优化主循环调度策略：
            // - task_delay == 0: 需要立即处理，延迟 1ms 避免 CPU 占用过高
            // - task_delay > 0: 使用 LVGL 建议的延迟时间
            // - 最小延迟 1ms，最大延迟 50ms（避免响应过慢）
            uint32_t delay_ms = 1;
            if (task_delay > 0) {
                delay_ms = (task_delay < 50) ? task_delay : 50;
            }
            
            // ⚡ 关键：必须调用 SDL_Delay，否则主循环会占用 100% CPU
            // 但延迟时间要短，确保响应性
            SDL_Delay(delay_ms);
        } catch (const std::exception& e) {
            printf("[MAIN] Exception in main loop (after delay): %s\n", e.what());
            fflush(stdout);
            PLOGE << "Exception in main loop (after delay): " << e.what();
            // 继续运行，不退出
        } catch (...) {
            printf("[MAIN] Unknown exception in main loop (after delay)\n");
            fflush(stdout);
            PLOGE << "Unknown exception in main loop (after delay)";
            // 继续运行，不退出
        }
    }
    
    printf("[MAIN] Main loop exiting (running=%d, loop_count=%d)\n", running, loop_count);
    fflush(stdout);
    
    // 清理任务服务
    ktv::services::TaskService::getInstance().cleanup();
    
    PLOGI << "Application exiting...";
    return 0;
}


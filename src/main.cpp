// ✅ 关键修复：LVGL 是 C 库，必须用 extern "C" 包裹，避免 C++ 命名修饰
extern "C" {
#include <lvgl.h>
}
#include <SDL.h>
#include <cstdio>
#include <exception>
#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif
#include "ui/layouts.h"
#include "ui/page_manager.h"
#include "ui/ui_scale.h"
#include "sdl/sdl.h"
#include <syslog.h>
#include "config/config.h"
#include "services/http_service.h"
#include "services/song_service.h"
#include "services/licence_service.h"
#include "services/history_service.h"
#include "services/m3u8_download_service.h"
#include "services/player_service.h"
#include "events/event_bus.h"

static lv_disp_draw_buf_t draw_buf;
// ✅ 第一步修复：改为全屏单buffer模式，避免partial buffer带来的复杂刷新逻辑
// 全屏buffer：1280*720*4=3.6MB（可接受的内存开销）
static lv_color_t buf[LV_HOR_RES_MAX * LV_VER_RES_MAX];

static bool init_display() {
    const lv_coord_t width = LV_HOR_RES_MAX;
    const lv_coord_t height = LV_VER_RES_MAX;

    // ⚠️ 防御性检查：分辨率必须有效
    if (width <= 0 || height <= 0) {
        syslog(LOG_ERR, "[ktv][sys][init_fail] component=display reason=invalid_resolution width=%d height=%d", (int)width, (int)height);
        fprintf(stderr, "[INIT] ERROR: Invalid display resolution: %dx%d\n", (int)width, (int)height);
        return false;
    }

    syslog(LOG_INFO, "[ktv][sys][init] component=sdl");
    fprintf(stderr, "[INIT] SDL display initialization (%dx%d)...\n", (int)width, (int)height);
    if (!sdl_init()) {
        syslog(LOG_ERR, "[ktv][sys][init_fail] component=sdl");
        return false;
    }

    syslog(LOG_INFO, "[ktv][sys][init] component=lvgl_buffer mode=full_screen");
    fprintf(stderr, "[INIT] LVGL display buffer: %dx%d (full screen buffer)\n",
            (int)width, (int)height);
    // ✅ 第一步修复：使用全屏单buffer，第二个buffer设为nullptr
    lv_disp_draw_buf_init(&draw_buf, buf, nullptr, width * height);
    
    // ✅ 诊断：检查 draw_buf 配置
    fprintf(stderr, "[DIAG] draw_buf size: %d pixels (expected: %d)\n", 
            (int)draw_buf.size, (int)(width * height));
    fprintf(stderr, "[DIAG] draw_buf buf1: %p, buf2: %p\n", 
            (void*)draw_buf.buf1, (void*)draw_buf.buf2);

    // ⚠️ 关键修复：使用静态变量确保 disp_drv 在整个程序生命周期内有效
    // LVGL 内部会保存驱动指针，如果使用局部变量可能导致悬空指针
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    
    // ✅ 关键修复：所有设置必须在 register 之前完成！
    // 顺序：分辨率 → flush_cb → draw_buf → full_refresh → register
    // ⚠️ 必须在注册前设置分辨率，否则 LVGL 会使用默认值 0x0，导致驱动无法激活
    disp_drv.hor_res = width;
    disp_drv.ver_res = height;
    
    // ⚠️ 必须设置 flush 回调，否则 LVGL 无法刷新屏幕
    disp_drv.flush_cb = sdl_display_flush;
    disp_drv.draw_buf = &draw_buf;
    
    // ✅ 第一步修复：开启full_refresh，让LVGL每次都刷全屏，简化flush逻辑
    // ⚠️ 必须在 register 之前设置，否则无效！
    disp_drv.full_refresh = 1;
    
    // ✅ 验证：确保所有关键参数在 register 前已设置
    fprintf(stderr, "[DIAG] Before register: res=%dx%d, flush_cb=%p, full_refresh=%d\n",
            (int)disp_drv.hor_res, (int)disp_drv.ver_res, 
            (void*)disp_drv.flush_cb, disp_drv.full_refresh);

    // ✅ Step1诊断：确认 flush_cb 被注册
    if (disp_drv.flush_cb == NULL) {
        fprintf(stderr, "❌ [DIAG] flush_cb NOT SET - CRITICAL ERROR!\n");
        syslog(LOG_ERR, "[ktv][sys][init_fail] component=display reason=flush_cb_null");
        return false;
    } else {
        fprintf(stderr, "✅ [DIAG] flush_cb is SET: %p\n", (void*)disp_drv.flush_cb);
        syslog(LOG_INFO, "[ktv][sys][init] component=display_flush_cb status=registered");
    }

    // ⚠️ 防御性检查：确保分辨率已正确设置
    if (disp_drv.hor_res <= 0 || disp_drv.ver_res <= 0) {
        syslog(LOG_ERR, "[ktv][sys][init_fail] component=display reason=invalid_resolution width=%d height=%d", (int)disp_drv.hor_res, (int)disp_drv.ver_res);
        fprintf(stderr, "[INIT] ERROR: Display driver resolution is invalid: %dx%d\n",
                (int)disp_drv.hor_res, (int)disp_drv.ver_res);
        return false;
    }
    
    // ✅ Step2诊断：确认 full_refresh 在注册前设置
    fprintf(stderr, "[DIAG] full_refresh = %d (must be 1 before register)\n", disp_drv.full_refresh);

    fprintf(stderr, "[INIT] Registering LVGL display driver: %dx%d\n",
            (int)disp_drv.hor_res, (int)disp_drv.ver_res);

    lv_disp_t* disp = lv_disp_drv_register(&disp_drv);
    if (!disp) {
        syslog(LOG_ERR, "[ktv][sys][init_fail] component=display reason=registration_failed");
        fprintf(stderr, "❌ [INIT] Failed to register display driver\n");
        return false;
    }

    // ✅ 关键诊断：注册后立即验证 flush_cb 是否仍然存在
    // 注意：LVGL 的 disp 结构体可能不直接暴露 driver，我们通过 disp_drv 验证
    fprintf(stderr, "[DIAG] After register: verifying flush_cb in original disp_drv...\n");
    if (disp_drv.flush_cb == NULL) {
        fprintf(stderr, "❌ [DIAG] CRITICAL: flush_cb is NULL in disp_drv after registration!\n");
        return false;
    } else {
        fprintf(stderr, "✅ [DIAG] flush_cb still valid in disp_drv: %p\n", 
                (void*)disp_drv.flush_cb);
        // ✅ 验证函数指针是否指向我们的函数
        if (disp_drv.flush_cb == sdl_display_flush) {
            fprintf(stderr, "✅ [DIAG] flush_cb matches sdl_display_flush function\n");
        } else {
            fprintf(stderr, "⚠️ [DIAG] flush_cb pointer mismatch! Expected: %p, Got: %p\n",
                    (void*)sdl_display_flush, (void*)disp_drv.flush_cb);
        }
    }

    // ✅ 关键修复：必须设为默认显示器，否则 LVGL 不知道要把画面刷到哪里
    // 这是解决 flush_cb 不被调用的根本原因
    lv_disp_set_default(disp);
    fprintf(stderr, "🎯 [INIT] LVGL default display set to %p\n", (void*)disp);
    syslog(LOG_INFO, "[ktv][sys][init] component=display status=default_set");
    
    // ✅ 决定性验证：检查当前分辨率是否被正确激活
    lv_coord_t current_hor = lv_disp_get_hor_res(NULL);
    lv_coord_t current_ver = lv_disp_get_ver_res(NULL);
    fprintf(stderr, "🚩 [DIAG] Current display res: %d x %d (expected: %d x %d)\n",
            (int)current_hor, (int)current_ver, (int)width, (int)height);
    
    if (current_hor != width || current_ver != height) {
        fprintf(stderr, "❌ [DIAG] CRITICAL: Display resolution mismatch! Driver not activated!\n");
        fprintf(stderr, "   Expected: %dx%d, Got: %dx%d\n", 
                (int)width, (int)height, (int)current_hor, (int)current_ver);
        syslog(LOG_ERR, "[ktv][sys][init_fail] component=display reason=resolution_mismatch");
        return false;
    } else {
        fprintf(stderr, "✅ [DIAG] Display resolution verified - driver activated\n");
    }

    // ⚠️ 关键修复：注册后立即验证分辨率是否正确传递
    lv_coord_t disp_w = lv_disp_get_hor_res(disp);
    lv_coord_t disp_h = lv_disp_get_ver_res(disp);
    
    if (disp_w <= 0 || disp_h <= 0) {
        syslog(LOG_ERR, "[ktv][sys][init_fail] component=display reason=resolution_zero_after_registration");
        fprintf(stderr, "[INIT] CRITICAL ERROR: Display driver resolution is 0x0 after registration!\n");
        fprintf(stderr, "[INIT] This will cause memory access violations in lv_timer_handler()\n");
        return false;
    }
    
    fprintf(stderr, "[INIT] Display driver registered successfully: %dx%d\n", (int)disp_w, (int)disp_h);
    syslog(LOG_INFO, "[ktv][sys][init] component=display status=registered width=%d height=%d", (int)disp_w, (int)disp_h);
    return true;
}

// Windows SEH异常处理包装函数（必须是纯C函数，不能有C++对象）
#ifdef _WIN32
static bool g_seh_exception_occurred = false;
static DWORD g_seh_exception_code = 0;

extern "C" {
static uint32_t safe_lv_timer_handler_impl() {
    uint32_t delay = 5;
    g_seh_exception_occurred = false;
    __try {
        delay = lv_timer_handler();
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        g_seh_exception_code = GetExceptionCode();
        g_seh_exception_occurred = true;
        fprintf(stderr, "CRITICAL: Windows SEH exception (0x%08X) in lv_timer_handler()\n", g_seh_exception_code);
        delay = 5;  // 返回默认延迟
    }
    return delay;
}
}  // extern "C"

static uint32_t safe_lv_timer_handler() {
    uint32_t delay = safe_lv_timer_handler_impl();
    // 在C++代码中记录日志（仅在真正发生异常时）
    if (g_seh_exception_occurred) {
        syslog(LOG_ERR, "[ktv][sys][error] component=lv_timer_handler exception=seh code=0x%x", g_seh_exception_code);
    }
    return delay;
}
#else
static uint32_t safe_lv_timer_handler() {
    return lv_timer_handler();
}
#endif

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
    
    // Set Windows console to UTF-8 encoding to fix character encoding issues
#ifdef _WIN32
    SetConsoleOutputCP(65001);  // UTF-8
    SetConsoleCP(65001);        // UTF-8
#endif
    
    // Initialize syslog
    fprintf(stderr, "=== KTV LVGL Program Start ===\n");
    openlog("ktv", LOG_PID | LOG_NDELAY, LOG_USER);
    
    try {
        syslog(LOG_INFO, "[ktv][sys][init] component=lvgl");
        lv_init();
        
        syslog(LOG_INFO, "[ktv][sys][init] component=config");
        ktv::config::NetworkConfig net_cfg;
        bool cfg_ok = ktv::config::loadFromFile("config.ini", net_cfg);
        if (!cfg_ok) {
            syslog(LOG_WARNING, "[ktv][sys][config] file=config.ini status=not_found action=using_defaults");
        }
        
        syslog(LOG_INFO, "[ktv][sys][init] component=display");
        if (!init_display()) {
            syslog(LOG_ERR, "[ktv][sys][init_fail] component=display");
            fprintf(stderr, "Press any key to exit...\n");
#ifdef _WIN32
            _getch();
#else
            getchar();
#endif
            return -1;
        }
        
        syslog(LOG_INFO, "[ktv][sys][init] component=input");
        init_input();
    
        // ✅ 关键修复：UIScale 必须从实际显示驱动分辨率初始化
        // 不能使用宏，必须从 lv_disp_get_*_res 获取实际分辨率
        // 这是适配不同平台（SDL/F133）的关键
        lv_disp_t* default_disp = lv_disp_get_default();
        lv_coord_t actual_width = LV_HOR_RES_MAX;
        lv_coord_t actual_height = LV_VER_RES_MAX;
        
        if (!default_disp) {
            syslog(LOG_ERR, "[ktv][sys][init_fail] component=display reason=no_driver");
            fprintf(stderr, "[INIT] CRITICAL ERROR: No display driver found!\n");
            fprintf(stderr, "[INIT] This will cause memory access violations in lv_timer_handler()\n");
            fprintf(stderr, "Press any key to exit...\n");
#ifdef _WIN32
            _getch();
#else
            getchar();
#endif
            return -1;
        }
        
        lv_coord_t disp_w = lv_disp_get_hor_res(default_disp);
        lv_coord_t disp_h = lv_disp_get_ver_res(default_disp);
        
        // ⚠️ 关键修复：如果分辨率是 0x0，立即报错并退出
        // 继续运行会导致 lv_timer_handler() 访问非法内存
        if (disp_w <= 0 || disp_h <= 0) {
            syslog(LOG_ERR, "[ktv][sys][init_fail] component=display reason=resolution_zero width=%d height=%d", (int)disp_w, (int)disp_h);
            fprintf(stderr, "[INIT] CRITICAL ERROR: Display driver resolution is 0x0!\n");
            fprintf(stderr, "[INIT] Expected: %dx%d, Got: %dx%d\n",
                    (int)LV_HOR_RES_MAX, (int)LV_VER_RES_MAX, (int)disp_w, (int)disp_h);
            fprintf(stderr, "[INIT] This will cause memory access violations in lv_timer_handler()\n");
            fprintf(stderr, "[INIT] Possible causes:\n");
            fprintf(stderr, "[INIT]   1. disp_drv.hor_res/ver_res not set before lv_disp_drv_register()\n");
            fprintf(stderr, "[INIT]   2. LVGL internal error during driver registration\n");
            fprintf(stderr, "[INIT]   3. Display driver structure was destroyed before registration\n");
            fprintf(stderr, "Press any key to exit...\n");
#ifdef _WIN32
            _getch();
#else
            getchar();
#endif
            return -1;
        }
        
        actual_width = disp_w;
        actual_height = disp_h;
        fprintf(stderr, "[INIT] Using display driver resolution: %dx%d\n", 
                (int)actual_width, (int)actual_height);
        syslog(LOG_INFO, "[ktv][sys][init] component=display_resolution width=%d height=%d", (int)actual_width, (int)actual_height);
    
        syslog(LOG_INFO, "[ktv][sys][init] component=ui");
        // ✅ 使用实际分辨率初始化 UIScale，设计稿标准为 1920x1080
        ktv::ui::init_ui_system(actual_width, actual_height);

        syslog(LOG_INFO, "[ktv][sys][init] component=services");
        // Initialize services (placeholder/optional parameters)
        ktv::services::HttpService::getInstance().initialize(net_cfg.base_url, net_cfg.timeout);
        ktv::services::LicenceService::getInstance().initialize();
        ktv::services::HistoryService::getInstance().setCapacity(50);
        ktv::services::M3u8DownloadService::getInstance().initialize();

        syslog(LOG_INFO, "[ktv][sys][init] component=main_screen");
        fprintf(stderr, "Creating main screen...\n");
        lv_obj_t* scr = nullptr;
        try {
            scr = ktv::ui::create_main_screen();
        } catch (const std::exception& e) {
            fprintf(stderr, "Exception while creating main screen: %s\n", e.what());
            syslog(LOG_ERR, "[ktv][sys][init_fail] component=main_screen exception=%s", e.what());
            throw;
        } catch (...) {
            fprintf(stderr, "Unknown exception while creating main screen\n");
            syslog(LOG_ERR, "[ktv][sys][init_fail] component=main_screen exception=unknown");
            throw;
        }

        if (!scr || !lv_obj_is_valid(scr)) {
            syslog(LOG_ERR, "[ktv][sys][init_fail] component=main_screen reason=create_failed");
            fprintf(stderr, "create_main_screen returned NULL or invalid\n");
            fprintf(stderr, "Press any key to exit...\n");
#ifdef _WIN32
            _getch();
#else
            getchar();
#endif
            return -1;
        }
        fprintf(stderr, "Main screen created successfully\n");

        // ✅ 关键修复：确保屏幕被正确加载
        lv_scr_load(scr);
        fprintf(stderr, "[INIT] Screen loaded\n");
        
        // ✅ 关键修复：创建屏幕后立即更新布局，确保UI对象正确布局
        fprintf(stderr, "[INIT] Updating screen layout...\n");
        lv_obj_update_layout(scr);
        
        // ✅ 关键修复：不要在初始化阶段立即触发布局刷新
        // 将首次刷新延迟到主循环，让LVGL自然处理，避免卡死
        fprintf(stderr, "[INIT] Screen created, deferring first refresh to main loop...\n");
        
        // ✅ 关键修复：只设置屏幕尺寸（这是安全的，不会触发布局计算）
        lv_obj_set_size(scr, LV_HOR_RES_MAX, LV_VER_RES_MAX);
        
        // ✅ 关键修复：初始化 tick 系统（必须在主循环前）
        // LVGL 需要 tick 才能正确工作
        lv_tick_inc(1);  // 初始化 tick
        
        // ✅ 关键修复：短暂延迟，让对象创建完成（但不触发刷新）
        SDL_Delay(20);
        
        // ✅ 关键修复：标记屏幕无效，让主循环自然处理刷新
        lv_obj_invalidate(scr);
        fprintf(stderr, "[INIT] Screen invalidated, first refresh will happen in main loop\n");
        
        // ✅ 强制测试：创建一个简单的测试对象，确保有内容需要渲染
        // 这可以强制 LVGL 触发 flush_cb
        lv_obj_t* test_obj = lv_obj_create(scr);
        if (test_obj) {
            lv_obj_set_size(test_obj, 200, 100);
            lv_obj_set_pos(test_obj, 50, 50);
            lv_obj_set_style_bg_color(test_obj, lv_color_hex(0xFF0000), 0); // 红色背景
            lv_obj_set_style_bg_opa(test_obj, LV_OPA_COVER, 0);
            lv_obj_invalidate(test_obj);
            fprintf(stderr, "[INIT] Test object created (red rectangle) to force refresh\n");
        }

        syslog(LOG_INFO, "[ktv][sys][ready] status=initialization_complete");
        fprintf(stderr, "Program ready. Close window or press ESC to exit.\n");

        // 主循环：按照最佳实践，刷新权完全交给LVGL
        // 顺序：先更新 tick，再 lv_timer_handler（触发渲染），再处理SDL事件
        bool quit = false;
        SDL_Event e;
        int loop_count = 0;
        
        // ✅ 关键修复：初始化 SDL tick 跟踪
        uint32_t last_tick = SDL_GetTicks();
        bool first_loop = true;
        int loop_count_before_flush = 0;
        
        fprintf(stderr, "[MAIN] Starting main loop, last_tick=%u\n", last_tick);
        
        while (!quit) {
            // ✅ 核心修复：LVGL tick 更新（必须在 lv_timer_handler 之前）
            // 这是 LVGL 的心跳，没有 tick → 没有刷新 → flush 不会触发
            uint32_t now = SDL_GetTicks();
            uint32_t elapsed = now - last_tick;
            
            // ⚠️ 关键：即使 elapsed = 0，也要确保 tick 系统已初始化
            // 第一次循环时可能 elapsed = 0，但后续必须更新
            if (elapsed > 0 || first_loop) {
                lv_tick_inc(elapsed > 0 ? elapsed : 1);  // 首次至少给 1ms
                if (first_loop || loop_count < 5) {
                    fprintf(stderr, "[MAIN] Tick updated: elapsed=%ums (loop #%d)\n", 
                            elapsed > 0 ? elapsed : 1, loop_count);
                }
                last_tick = now;
            }
            
            // ✅ 调试：首次循环时强制触发刷新
            if (first_loop) {
                fprintf(stderr, "[MAIN] Entering main loop, forcing first refresh...\n");
                first_loop = false;
                
                // ⚠️ 关键：先运行一次 timer handler，让 LVGL 初始化内部状态
                fprintf(stderr, "[MAIN] Running first lv_timer_handler() to initialize LVGL...\n");
                lv_timer_handler();
                
                // 强制标脏整个屏幕
                lv_obj_t* scr = lv_scr_act();
                if (scr) {
                    lv_obj_invalidate(scr);
                    fprintf(stderr, "[MAIN] Screen invalidated\n");
                }
                
                // 再次运行 timer handler，这次应该触发刷新
                fprintf(stderr, "[MAIN] Running second lv_timer_handler() to trigger refresh...\n");
                lv_timer_handler();
                
                // 如果还没刷新，强制刷新
                lv_disp_t* disp = lv_disp_get_default();
                if (disp) {
                    fprintf(stderr, "[MAIN] Calling lv_refr_now() as fallback...\n");
                    lv_refr_now(disp);
                    fprintf(stderr, "[MAIN] lv_refr_now() called, check for 🔥 FLUSH CALLED logs\n");
                }
            }

            // ✅ 核心修复：调用 LVGL timer handler（触发渲染）
            // 这是 LVGL 的刷新引擎，必须每帧调用
            uint32_t task_delay = 5;
            try {
                // ✅ 关键：每次循环都强制标脏一次（仅前几次，用于诊断）
                if (loop_count < 5) {
                    lv_obj_t* scr = lv_scr_act();
                    if (scr) {
                        lv_obj_invalidate(scr);
                        if (loop_count == 0) {
                            fprintf(stderr, "[MAIN] Screen invalidated for first timer handler call\n");
                        }
                    }
                }
                
                // ⚠️ 关键：调用 timer handler，这会触发 flush_cb
                task_delay = safe_lv_timer_handler();
                
                // ✅ 调试：前几次循环输出信息
                loop_count_before_flush++;
                if (loop_count_before_flush <= 10) {
                    fprintf(stderr, "[MAIN] Loop #%d: lv_timer_handler returned delay=%dms\n", 
                            loop_count_before_flush, task_delay);
                }
            } catch (const std::exception& e) {
                fprintf(stderr, "ERROR in lv_timer_handler: %s\n", e.what());
                syslog(LOG_ERR, "[ktv][sys][error] component=lv_timer_handler exception=%s", e.what());
            } catch (...) {
                fprintf(stderr, "ERROR in lv_timer_handler: unknown exception\n");
                syslog(LOG_ERR, "[ktv][sys][error] component=lv_timer_handler exception=unknown");
            }

            // ✅ 关键修复：在主线程中分发 EventBus 事件，确保所有 UI 更新都在主线程执行
            // 这是避免多线程访问 LVGL 导致 0xC0000005 崩溃的关键步骤
            // 所有后台线程（下载、播放器等）只能通过 EventBus 发布事件，不能直接操作 UI
            try {
                ktv::events::EventBus::getInstance().dispatchOnUiThread();
            } catch (const std::exception& e) {
                fprintf(stderr, "ERROR in EventBus dispatch: %s\n", e.what());
                syslog(LOG_ERR, "[ktv][sys][error] component=eventbus exception=%s", e.what());
            } catch (...) {
                fprintf(stderr, "ERROR in EventBus dispatch: unknown exception\n");
                syslog(LOG_ERR, "[ktv][sys][error] component=eventbus exception=unknown");
            }
            
            while (SDL_PollEvent(&e)) {
                try {
                    if (e.type == SDL_QUIT) {
                        syslog(LOG_INFO, "[ktv][sys][event] type=quit");
                        quit = true;
                    } else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
                        syslog(LOG_INFO, "[ktv][sys][event] type=key_escape");
                        quit = true;
                    } else {
                        // 更新输入设备状态（鼠标、键盘）
                        sdl_update_mouse_state(&e);
                        sdl_update_keyboard_state(&e);
                    }
                } catch (const std::exception& ex) {
                    fprintf(stderr, "ERROR processing SDL event: %s\n", ex.what());
                    // 继续处理下一个事件
                } catch (...) {
                    fprintf(stderr, "ERROR processing SDL event: unknown exception\n");
                    // 继续处理下一个事件
                }
            }
            
            // ✅ 关键修复：避免 CPU 打满，让 LVGL 有机会触发刷新
            // 这是 LVGL 刷新循环的关键，没有 delay → 刷新可能被跳过
            SDL_Delay(task_delay > 5 ? task_delay : 5);

            loop_count++;
            if (loop_count % 1000 == 0) {
                syslog(LOG_INFO, "[ktv][sys][heartbeat] loop_count=%d", loop_count);
            }
        }
        
        syslog(LOG_INFO, "[ktv][sys][exit] reason=normal");
        return 0;
    } catch (const std::exception& e) {
        fprintf(stderr, "\n=== Program Exception Exit ===\n");
        fprintf(stderr, "Exception type: std::exception\n");
        fprintf(stderr, "Exception message: %s\n", e.what());
        try {
            syslog(LOG_ERR, "[ktv][sys][exit] reason=exception exception=%s", e.what());
        } catch (...) {
            // Logger system may also have problems, ignore
        }
        fprintf(stderr, "\nPress any key to exit...\n");
#ifdef _WIN32
        _getch();
#else
        getchar();
#endif
        return -1;
    } catch (...) {
        fprintf(stderr, "\n=== Program Exception Exit ===\n");
        fprintf(stderr, "Exception type: Unknown exception\n");
        try {
            syslog(LOG_ERR, "[ktv][sys][exit] reason=unknown_exception");
        } catch (...) {
            // Logger system may also have problems, ignore
        }
        fprintf(stderr, "\nPress any key to exit...\n");
#ifdef _WIN32
        _getch();
#else
        getchar();
#endif
        return -1;
    }
}


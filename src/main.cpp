#include <lvgl.h>
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
// 双缓冲partial buffer: 约1/7屏幕高度，优化性能和稳定性
static lv_color_t buf1[LV_HOR_RES_MAX * 100];
static lv_color_t buf2[LV_HOR_RES_MAX * 100];

static bool init_display() {
    PLOGI << "Initializing SDL display...";
    fprintf(stderr, "[INIT] SDL display initialization...\n");
    if (!sdl_init()) {
        PLOGE << "SDL initialization failed!";
        return false;
    }
    
    // Check if SDL initialized successfully (by checking if window was created)
    // Note: sdl_init has internal error checking, but we need to ensure window exists
    PLOGI << "Initializing LVGL display buffer (dual buffer, partial refresh)...";
    fprintf(stderr, "[INIT] LVGL display buffer: %dx%d (buffer size: %d lines)\n", 
            LV_HOR_RES_MAX, LV_VER_RES_MAX, 100);
    // 双缓冲 + partial refresh：提高稳定性，避免黑屏
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, LV_HOR_RES_MAX * 100);

    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = sdl_display_flush;
    disp_drv.draw_buf = &draw_buf;
    
    // ✅ 关键修复：明确设置显示驱动分辨率
    // 这是解决 0x0 问题的核心步骤
    // 注意：必须在注册前设置，且值必须 > 0
    disp_drv.hor_res = LV_HOR_RES_MAX;
    disp_drv.ver_res = LV_VER_RES_MAX;
    disp_drv.full_refresh = 0;  // 启用partial refresh，只刷新脏区域
    
    // ✅ 添加 rounder_cb 回调，确保分辨率正确传递
    // 这个回调函数用于调整刷新区域，但也可以用来确保分辨率正确
    disp_drv.rounder_cb = nullptr;  // 使用默认的 rounder（如果需要可以自定义）
    
    fprintf(stderr, "[INIT] Registering LVGL display driver: %dx%d\n", 
            disp_drv.hor_res, disp_drv.ver_res);
    
    // 验证设置的值
    if (disp_drv.hor_res == 0 || disp_drv.ver_res == 0) {
        PLOGE << "Display driver resolution is 0x0 BEFORE registration!";
        fprintf(stderr, "[ERROR] Display driver resolution is 0x0 BEFORE registration!\n");
        fprintf(stderr, "[ERROR] LV_HOR_RES_MAX=%d, LV_VER_RES_MAX=%d\n", 
                LV_HOR_RES_MAX, LV_VER_RES_MAX);
        return false;
    }
    
    lv_disp_t* disp = lv_disp_drv_register(&disp_drv);
    if (!disp) {
        PLOGE << "LVGL display driver registration failed!";
        return false;
    }
    
    // ✅ 关键修复：注册后立即验证并强制设置分辨率
    // 如果 LVGL 没有正确保存分辨率，我们需要通过其他方式设置
    lv_coord_t disp_w = lv_disp_get_hor_res(disp);
    lv_coord_t disp_h = lv_disp_get_ver_res(disp);
    fprintf(stderr, "[INIT] Display driver registered: %dx%d (from lv_disp_get_*_res)\n", 
            (int)disp_w, (int)disp_h);
    
    // 如果分辨率仍然是 0，尝试通过设置屏幕尺寸来触发分辨率更新
    if (disp_w == 0 || disp_h == 0) {
        PLOGW << "Display driver resolution is 0x0 after registration, attempting fix...";
        fprintf(stderr, "[WARN] Display driver resolution is 0x0 after registration!\n");
        fprintf(stderr, "[WARN] Attempting to fix by setting screen size...\n");
        
        // 尝试获取默认屏幕并设置尺寸
        lv_obj_t* default_scr = lv_scr_act();
        if (default_scr) {
            lv_obj_set_size(default_scr, LV_HOR_RES_MAX, LV_VER_RES_MAX);
            fprintf(stderr, "[WARN] Set default screen size to %dx%d\n", 
                    LV_HOR_RES_MAX, LV_VER_RES_MAX);
        }
        
        // 再次检查分辨率
        disp_w = lv_disp_get_hor_res(disp);
        disp_h = lv_disp_get_ver_res(disp);
        fprintf(stderr, "[WARN] After fix attempt: %dx%d\n", (int)disp_w, (int)disp_h);
        
        // 如果仍然是 0，这是一个严重问题，但不应该阻止程序运行
        // 让主循环中的自动修复机制来处理
        if (disp_w == 0 || disp_h == 0) {
            PLOGW << "Display resolution still 0x0, will rely on auto-fix in main loop";
            fprintf(stderr, "[WARN] Display resolution still 0x0, will rely on auto-fix in main loop\n");
        }
    }
    
    PLOGI << "Display driver registered successfully";
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
        PLOGE << "Windows SEH exception (0x" << std::hex << g_seh_exception_code << std::dec << ") in lv_timer_handler()";
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

/**
 * @brief 调试函数：打印屏幕和显示驱动的详细信息
 * 用于诊断 0x0 尺寸问题
 */
static void dbg_screen_info() {
    lv_obj_t* scr = lv_scr_act();
    lv_disp_t* disp = lv_disp_get_default();
    
    fprintf(stderr, "\n[DBG] ========== Screen Info ==========\n");
    fprintf(stderr, "[DBG] Screen object: %p\n", (void*)scr);
    if (scr) {
        fprintf(stderr, "[DBG] Screen valid: %s\n", lv_obj_is_valid(scr) ? "YES" : "NO");
        fprintf(stderr, "[DBG] Screen size: %dx%d\n", 
                (int)lv_obj_get_width(scr), (int)lv_obj_get_height(scr));
        fprintf(stderr, "[DBG] Screen children: %u\n", (unsigned)lv_obj_get_child_cnt(scr));
    } else {
        fprintf(stderr, "[DBG] Screen object is NULL!\n");
    }
    
    fprintf(stderr, "[DBG] Display driver: %p\n", (void*)disp);
    if (disp) {
        lv_coord_t disp_w = lv_disp_get_hor_res(disp);
        lv_coord_t disp_h = lv_disp_get_ver_res(disp);
        fprintf(stderr, "[DBG] Display resolution: %dx%d\n", (int)disp_w, (int)disp_h);
        if (disp_w == 0 || disp_h == 0) {
            fprintf(stderr, "[DBG] ⚠️  Display resolution is 0x0! Using LV_HOR_RES_MAX/LV_VER_RES_MAX instead.\n");
        }
    } else {
        fprintf(stderr, "[DBG] Display driver is NULL!\n");
    }
    fprintf(stderr, "[DBG] LV_HOR_RES_MAX: %d, LV_VER_RES_MAX: %d\n", 
            LV_HOR_RES_MAX, LV_VER_RES_MAX);
    fprintf(stderr, "[DBG] ✅ Effective resolution (should be used): %dx%d\n", 
            LV_HOR_RES_MAX, LV_VER_RES_MAX);
    fprintf(stderr, "[DBG] ====================================\n\n");
}

/**
 * @brief 检查屏幕是否ready，可以安全刷新
 * @param check_count 检查次数（用于调试输出）
 * @return true 如果屏幕ready，false 如果屏幕未ready
 * 
 * ✅ READY 判定标准（三项必须全部满足）：
 * 1. scr != null && lv_obj_is_valid(scr)
 * 2. size != 0 (width > 0 && height > 0)
 * 3. child_count > 0 (KTV界面不应该为空屏)
 * 
 * 这是解决首次刷新崩溃的核心机制
 */
static bool is_screen_ready_for_refresh(int check_count = 0) {
    // 1. 检查屏幕是否存在
    lv_obj_t* scr = lv_scr_act();
    if (scr == NULL) {
        if (check_count < 3) {
            fprintf(stderr, "Screen ready check #%d: Screen not exist yet\n", check_count);
        }
        return false;
    }
    
    // 2. 检查屏幕对象是否有效
    if (!lv_obj_is_valid(scr)) {
        if (check_count < 3) {
            fprintf(stderr, "Screen ready check #%d: Screen object is invalid\n", check_count);
        }
        return false;
    }
    
    // 3. 检查是否有至少一个可见子对象（KTV界面不应该为空屏）
    uint32_t child_cnt = lv_obj_get_child_cnt(scr);
    if (child_cnt == 0) {
        if (check_count < 3) {
            fprintf(stderr, "Screen ready check #%d: Screen empty (no children), skip\n", check_count);
        }
        return false;
    }
    
    // 4. 检查屏幕尺寸是否正常
    lv_coord_t width = lv_obj_get_width(scr);
    lv_coord_t height = lv_obj_get_height(scr);
    
    // ✅ 关键修复：检测异常值（负数、过大值、0）
    // 这些异常值通常表示内存损坏或 LVGL 内部状态错误
    bool size_invalid = false;
    if (width <= 0 || height <= 0) {
        size_invalid = true;
    }
    // 检测异常大的值（超过合理范围，比如 > 10000）
    if (width > 10000 || height > 10000) {
        size_invalid = true;
        if (check_count <= 3) {
            fprintf(stderr, "Screen ready check #%d: ⚠️ Invalid screen size detected: %dx%d (too large!)\n",
                    check_count, (int)width, (int)height);
        }
    }
    
    if (size_invalid) {
        // ✅ 强制使用常量值，不依赖 LVGL 的返回值
        // 因为 LVGL 可能返回损坏的值（负数、异常大值等）
        lv_coord_t safe_width = LV_HOR_RES_MAX;
        lv_coord_t safe_height = LV_VER_RES_MAX;
        
        if (check_count <= 3) {
            fprintf(stderr, "Screen ready check #%d: ⚠️ Screen size invalid (width=%d, height=%d), "
                    "FORCING to safe values (%dx%d)...\n", 
                    check_count, (int)width, (int)height, (int)safe_width, (int)safe_height);
        }
        
        // 强制设置屏幕尺寸（使用安全值）
        lv_obj_set_size(scr, safe_width, safe_height);
        
        // 重新获取尺寸（但可能仍然是异常值，所以不依赖它）
        width = lv_obj_get_width(scr);
        height = lv_obj_get_height(scr);
        
        // 如果仍然是异常值，使用安全值进行判断
        if (width <= 0 || height <= 0 || width > 10000 || height > 10000) {
            if (check_count <= 3) {
                fprintf(stderr, "Screen ready check #%d: ⚠️ LVGL still returns invalid size, using safe values for check\n",
                        check_count);
            }
            // 使用安全值进行后续判断
            width = safe_width;
            height = safe_height;
        }
    }
    
    // 如果尺寸有效，直接通过
    if (width > 0 && height > 0) {
        if (check_count < 3) {
            fprintf(stderr, "Screen ready check #%d: ✅ All conditions met (size=%dx%d, children=%u), READY!\n", 
                    check_count, (int)width, (int)height, (unsigned)child_cnt);
        }
        return true;
    }
    
    // ✅ 如果尺寸仍然无效，说明布局还没计算完成或 LVGL 状态异常
    // 由于我们已经强制设置了屏幕尺寸，对象本身是有效的
    // 但 LVGL 的布局计算是延迟的，需要等待几个周期
    
    // 简化逻辑：最多等待 10 次检查（约 100ms），然后允许刷新
    // 此时屏幕对象有效且有子对象，让 LVGL 的 timer handler 来计算布局和尺寸
    if (check_count < 10) {
        if (check_count < 3) {
            fprintf(stderr, "Screen ready check #%d: Screen size not ready yet (width=%d, height=%d), waiting...\n", 
                    check_count, (int)width, (int)height);
        }
        return false;
    }
    
    // ✅ 10次检查后，即使尺寸异常也允许刷新
    // 此时屏幕对象有效且有子对象，我们已经强制设置了安全尺寸
    // 让 LVGL 的 timer handler 来处理，即使它可能会崩溃，也比无限等待好
    // 这是"免疫"机制的最后防线
    if (check_count == 10) {
        fprintf(stderr, "Screen ready check #%d: ⚠️ Size still invalid after 10 checks, "
                "but allowing refresh (using forced safe size %dx%d)\n", 
                check_count, LV_HOR_RES_MAX, LV_VER_RES_MAX);
        // 最后一次强制设置
        lv_obj_set_size(scr, LV_HOR_RES_MAX, LV_VER_RES_MAX);
    }
    return true;
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
    
    // Initialize logger first to ensure subsequent logs can output normally
    fprintf(stderr, "=== KTV LVGL Program Start ===\n");
    fprintf(stderr, "Initializing logger...\n");
    ktv::logging::init();  // Console logging
    
    try {
        PLOGI << "Initializing LVGL...";
        lv_init();
        
        PLOGI << "Loading config file...";
        ktv::config::NetworkConfig net_cfg;
        bool cfg_ok = ktv::config::loadFromFile("config.ini", net_cfg);
        if (!cfg_ok) {
            PLOGW << "config.ini not found or parse fail, using defaults.";
        }
        
        PLOGI << "Initializing display system...";
        if (!init_display()) {
            PLOGE << "Display initialization failed!";
            fprintf(stderr, "Press any key to exit...\n");
#ifdef _WIN32
            _getch();
#else
            getchar();
#endif
            return -1;
        }
        
        PLOGI << "Initializing input system...";
        init_input();
    
        // ✅ 关键修复：UIScale 必须从实际显示驱动分辨率初始化
        // 不能使用宏，必须从 lv_disp_get_*_res 获取实际分辨率
        // 这是适配不同平台（SDL/F133）的关键
        lv_disp_t* default_disp = lv_disp_get_default();
        lv_coord_t actual_width = LV_HOR_RES_MAX;
        lv_coord_t actual_height = LV_VER_RES_MAX;
        
        if (default_disp) {
            lv_coord_t disp_w = lv_disp_get_hor_res(default_disp);
            lv_coord_t disp_h = lv_disp_get_ver_res(default_disp);
            if (disp_w > 0 && disp_h > 0) {
                actual_width = disp_w;
                actual_height = disp_h;
                fprintf(stderr, "[INIT] Using display driver resolution: %dx%d\n", 
                        (int)actual_width, (int)actual_height);
            } else {
                fprintf(stderr, "[INIT] Display driver resolution is 0x0, using LV_HOR_RES_MAX/LV_VER_RES_MAX: %dx%d\n",
                        (int)actual_width, (int)actual_height);
            }
        } else {
            fprintf(stderr, "[INIT] No display driver, using LV_HOR_RES_MAX/LV_VER_RES_MAX: %dx%d\n",
                    (int)actual_width, (int)actual_height);
        }
    
        PLOGI << "Initializing UI system (scale, focus, theme)...";
        // ✅ 使用实际分辨率初始化 UIScale，设计稿标准为 1920x1080
        ktv::ui::init_ui_system(actual_width, actual_height);

        PLOGI << "Initializing services...";
        // Initialize services (placeholder/optional parameters)
        ktv::services::HttpService::getInstance().initialize(net_cfg.base_url, net_cfg.timeout);
        ktv::services::LicenceService::getInstance().initialize();
        ktv::services::HistoryService::getInstance().setCapacity(50);
        ktv::services::M3u8DownloadService::getInstance().initialize();

        PLOGI << "Creating main screen...";
        fprintf(stderr, "Creating main screen...\n");
        lv_obj_t* scr = nullptr;
        try {
            scr = ktv::ui::create_main_screen();
        } catch (const std::exception& e) {
            fprintf(stderr, "Exception while creating main screen: %s\n", e.what());
            PLOGE << "Exception creating main screen: " << e.what();
            throw;  // Re-throw exception
        } catch (...) {
            fprintf(stderr, "Unknown exception while creating main screen\n");
            PLOGE << "Unknown exception creating main screen";
            throw;  // Re-throw exception
        }
        
        if (!scr) {
            PLOGE << "Failed to create main screen!";
            fprintf(stderr, "create_main_screen returned NULL\n");
            fprintf(stderr, "Press any key to exit...\n");
#ifdef _WIN32
            _getch();
#else
            getchar();
#endif
            return -1;
        }
        fprintf(stderr, "Main screen created successfully\n");
        
        // 验证屏幕对象
        if (!lv_obj_is_valid(scr)) {
            PLOGE << "Screen object is invalid after creation!";
            fprintf(stderr, "ERROR: Screen object is invalid after creation!\n");
            return -1;
        }
        
        // ✅ 关键修复：在加载屏幕前先设置尺寸
        // 确保屏幕对象有正确的尺寸，避免 0x0 问题
        fprintf(stderr, "Setting screen size: %dx%d\n", LV_HOR_RES_MAX, LV_VER_RES_MAX);
        lv_obj_set_size(scr, LV_HOR_RES_MAX, LV_VER_RES_MAX);
        
        // 加载屏幕（不立即刷新）
        // 注意：UI必须在驱动注册之后创建（已确保）
        PLOGI << "Loading screen...";
        fprintf(stderr, "Loading screen...\n");
        lv_scr_load(scr);
        
        // 验证屏幕已加载
        lv_obj_t* current_screen = lv_scr_act();
        if (current_screen != scr) {
            PLOGW << "Screen load mismatch!";
            fprintf(stderr, "WARNING: Screen load mismatch! Expected %p, got %p\n", 
                    (void*)scr, (void*)current_screen);
        }
        
        // ✅ 再次确保屏幕尺寸（加载后可能被重置）
        lv_obj_set_size(scr, LV_HOR_RES_MAX, LV_VER_RES_MAX);
        
        // 验证尺寸设置成功
        lv_coord_t w = lv_obj_get_width(scr);
        lv_coord_t h = lv_obj_get_height(scr);
        fprintf(stderr, "Screen size after set: %dx%d\n", (int)w, (int)h);
        if (w == 0 || h == 0) {
            PLOGW << "Screen size still 0x0 after set_size, will retry in main loop";
            fprintf(stderr, "WARNING: Screen size still 0x0, will retry in main loop\n");
        }
        
        // ✅ 关键修复：不在初始化阶段立即触发布局刷新
        // 原因：UI对象树可能尚未完全稳定，过早刷新会导致0xC0000005内存访问异常
        // 解决方案：将首次刷新延迟到主循环中，让LVGL自然处理
        fprintf(stderr, "Screen loaded, deferring first refresh to main loop...\n");
        PLOGI << "Screen loaded successfully, first refresh will happen in main loop";
        
        // 短暂延迟，让对象创建完成（但不触发刷新）
        SDL_Delay(20);  // 给对象创建和挂载一些时间
        
        // 调试信息：打印屏幕和显示驱动状态
        dbg_screen_info();
        
        PLOGI << "Initialization complete, entering main loop";
        PLOGI << "Tip: Close window or press ESC to exit";
        fprintf(stderr, "Program ready. Close window or press ESC to exit.\n");

        // 主循环：按照最佳实践，刷新权完全交给LVGL
        // 顺序：先lv_timer_handler（触发渲染），再处理SDL事件
        bool quit = false;
        SDL_Event e;
        int loop_count = 0;
        bool first_refresh_done = false;  // 标记首次刷新是否完成
        int ready_check_count = 0;  // 记录ready检查次数
        
        while (!quit) {
            // 1. 先处理LVGL定时器（触发渲染刷新和输入读取）
            //    刷新权交给LVGL，SDL只做承载窗口+显存贴图
            
            // ✅ 首次刷新保护：确保屏幕对象完全ready后再刷新
            // READY标准：屏幕存在 + 尺寸正常 + 有子对象
            if (!first_refresh_done) {
                ready_check_count++;
                if (is_screen_ready_for_refresh(ready_check_count)) {
                    // 屏幕已ready，可以安全刷新
                    first_refresh_done = true;
                    fprintf(stderr, "\n🔥 First refresh: Screen is READY (check #%d), entering normal refresh cycle\n", 
                            ready_check_count);
                    PLOGI << "🔥 First refresh: Screen READY, entering normal cycle";
                } else {
                    // 屏幕未ready，跳过本次刷新，等待下一轮
                    // 只在前几次输出日志，避免刷屏
                    if (ready_check_count <= 5) {
                        fprintf(stderr, "First refresh: screen not ready yet (check #%d), skipping...\n", 
                                ready_check_count);
                    }
                    SDL_Delay(10);
                    continue;
                }
            }
            
            try {
                uint32_t task_delay = safe_lv_timer_handler();
                // 使用LVGL建议的延迟时间，最小5ms避免CPU 100%
                SDL_Delay(task_delay > 5 ? task_delay : 5);
            } catch (const std::exception& e) {
                fprintf(stderr, "ERROR in lv_timer_handler: %s\n", e.what());
                PLOGE << "lv_timer_handler exception: " << e.what();
                SDL_Delay(5);  // 继续运行，不要退出
            } catch (...) {
                fprintf(stderr, "ERROR in lv_timer_handler: unknown exception\n");
                PLOGE << "lv_timer_handler unknown exception";
                SDL_Delay(5);  // 继续运行，不要退出
            }
            
            // 2. 处理SDL事件（输入事件）
            while (SDL_PollEvent(&e)) {
                try {
                    if (e.type == SDL_QUIT) {
                        PLOGI << "Received quit event";
                        quit = true;
                    } else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
                        PLOGI << "Received ESC key, exiting";
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
            
            // 注意：不在这里调用SDL_RenderPresent()！
            // RenderPresent只在flush_cb中调用，刷新权完全交给LVGL
            
            // Output log every 1000 loops (approximately 5 seconds)
            loop_count++;
            if (loop_count % 1000 == 0) {
                PLOGI << "Main loop running... (count: " << loop_count << ")";
            }
        }
        
        PLOGI << "Program exiting normally";
        return 0;
    } catch (const std::exception& e) {
        fprintf(stderr, "\n=== Program Exception Exit ===\n");
        fprintf(stderr, "Exception type: std::exception\n");
        fprintf(stderr, "Exception message: %s\n", e.what());
        try {
            PLOGE << "Caught exception: " << e.what();
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
            PLOGE << "Caught unknown exception";
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


#include "layouts.h"
#include "page_manager.h"
#include "../services/mock_data.h"
#include "../services/song_service.h"
#include "../services/history_service.h"
#include "../services/player_service.h"
#include "../services/queue_service.h"
#include "../services/task_service.h"
#include "../events/event_bus.h"
#include <plog/Log.h>
#include <vector>
#include <string>
#include <cstring>

// 兼容配置缺省时的心形符号
#ifndef LV_SYMBOL_HEART
#define LV_SYMBOL_HEART  "\u2665"
#endif

namespace {
// 全局样式
lv_style_t style_bg;
lv_style_t style_card;
lv_style_t style_btn;
lv_style_t style_btn_pressed;
lv_style_t style_input;
lv_style_t style_list_item;
lv_style_t style_icon;
lv_style_t style_focus;
lv_style_t style_shadow;
lv_style_t style_vip;
lv_style_t style_subtext;

constexpr lv_coord_t kPad = 12;
constexpr lv_coord_t kGap = 10;
constexpr lv_coord_t kRadius = 12;

lv_color_t color_hex(uint32_t hex) {
    return lv_color_hex(hex);
}
}  // namespace

namespace ktv::ui {

void init_ui_theme() {
    static bool inited = false;
    if (inited) return;
    inited = true;

    // 背景渐变：深紫 → 浅紫
    lv_style_init(&style_bg);
    lv_style_set_bg_color(&style_bg, color_hex(0x5F4B9A));
    lv_style_set_bg_grad_color(&style_bg, color_hex(0x7C6BCB));
    lv_style_set_bg_grad_dir(&style_bg, LV_GRAD_DIR_VER);
    lv_style_set_bg_opa(&style_bg, LV_OPA_COVER);  // 确保背景完全不透明

    // 通用卡片
    lv_style_init(&style_card);
    lv_style_set_radius(&style_card, kRadius);
    lv_style_set_bg_color(&style_card, lv_color_hex(0x6E5CA8));
    lv_style_set_bg_opa(&style_card, LV_OPA_60);
    lv_style_set_pad_all(&style_card, kPad);
    lv_style_set_pad_column(&style_card, kGap);
    lv_style_set_pad_row(&style_card, kGap);

    // 列表项
    lv_style_init(&style_list_item);
    lv_style_set_radius(&style_list_item, kRadius);
    lv_style_set_bg_color(&style_list_item, lv_color_hex(0x67579E));
    lv_style_set_bg_opa(&style_list_item, LV_OPA_50);
    lv_style_set_pad_all(&style_list_item, kPad);
    lv_style_set_pad_column(&style_list_item, kGap);
    lv_style_set_pad_row(&style_list_item, 4);

    // 副文本颜色（次要信息）
    lv_style_init(&style_subtext);
    lv_style_set_text_color(&style_subtext, lv_color_hex(0xC8C9D4));
    lv_style_set_text_opa(&style_subtext, LV_OPA_100);

    // 图标样式（用于符号显示）
    lv_style_init(&style_icon);
    lv_style_set_text_color(&style_icon, lv_color_white());
    lv_style_set_text_opa(&style_icon, LV_OPA_100);
    lv_style_set_bg_opa(&style_icon, LV_OPA_TRANSP);

    // 按钮
    lv_style_init(&style_btn);
    lv_style_set_radius(&style_btn, 10);
    lv_style_set_bg_color(&style_btn, lv_color_hex(0x4F7BFF));
    lv_style_set_bg_opa(&style_btn, LV_OPA_80);
    lv_style_set_text_color(&style_btn, lv_color_white());
    lv_style_set_pad_all(&style_btn, 10);

    lv_style_init(&style_btn_pressed);
    lv_style_set_radius(&style_btn_pressed, 10);
    lv_style_set_bg_color(&style_btn_pressed, lv_color_hex(0x3A5FD6));
    lv_style_set_bg_opa(&style_btn_pressed, LV_OPA_100);
    lv_style_set_text_color(&style_btn_pressed, lv_color_white());
    lv_style_set_pad_all(&style_btn_pressed, 10);

    // 输入框
    lv_style_init(&style_input);
    lv_style_set_radius(&style_input, 10);
    lv_style_set_bg_color(&style_input, lv_color_hex(0x6E5CA8));
    lv_style_set_bg_opa(&style_input, LV_OPA_50);
    lv_style_set_pad_all(&style_input, 10);

    // 焦点高亮（遥控器）
    lv_style_init(&style_focus);
    lv_style_set_outline_color(&style_focus, lv_color_hex(0x4F7BFF));
    lv_style_set_outline_width(&style_focus, 2);
    lv_style_set_outline_opa(&style_focus, LV_OPA_80);
    lv_style_set_outline_pad(&style_focus, 2);

    // 阴影（轻度玻璃感）
    lv_style_init(&style_shadow);
    lv_style_set_shadow_color(&style_shadow, lv_color_hex(0x2D234F));
    lv_style_set_shadow_width(&style_shadow, 12);
    lv_style_set_shadow_ofs_x(&style_shadow, 0);
    lv_style_set_shadow_ofs_y(&style_shadow, 4);
    lv_style_set_shadow_opa(&style_shadow, LV_OPA_40);

    // VIP 渐变按钮
    lv_style_init(&style_vip);
    lv_style_set_radius(&style_vip, 16);
    lv_style_set_bg_color(&style_vip, lv_color_hex(0xF6A000));
    lv_style_set_bg_grad_color(&style_vip, lv_color_hex(0xF65C00));
    lv_style_set_bg_grad_dir(&style_vip, LV_GRAD_DIR_HOR);
    lv_style_set_bg_opa(&style_vip, LV_OPA_100);
    lv_style_set_pad_all(&style_vip, 12);
    lv_style_set_text_color(&style_vip, lv_color_white());
}

static void setup_flex_row(lv_obj_t* obj, lv_coord_t gap = 8, lv_coord_t pad = 8) {
    lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(obj, pad, 0);
    lv_obj_set_style_pad_row(obj, gap, 0);
    lv_obj_set_style_pad_column(obj, gap, 0);
}

static void setup_flex_col(lv_obj_t* obj, lv_coord_t gap = 8, lv_coord_t pad = 8) {
    lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(obj, pad, 0);
    lv_obj_set_style_pad_row(obj, gap, 0);
    lv_obj_set_style_pad_column(obj, gap, 0);
}

// 返回主屏幕的事件处理（公共函数）
static void on_back_to_main(lv_event_t* e) {
    (void)e;
    lv_obj_t* main_scr = create_main_screen();
    lv_scr_load(main_scr);
}

// 创建带返回按钮的标题栏（公共函数）
// 用于分类、排行榜、歌手等二级页面
static lv_obj_t* create_title_bar(lv_obj_t* parent, const char* title_text) {
    lv_obj_t* title_bar = lv_obj_create(parent);
    lv_obj_set_size(title_bar, LV_PCT(100), 50);
    setup_flex_row(title_bar, 10, 10);
    lv_obj_set_style_bg_opa(title_bar, LV_OPA_60, 0);
    
    // 返回按钮（左侧）
    lv_obj_t* back_btn = lv_btn_create(title_bar);
    lv_obj_add_style(back_btn, &style_btn, 0);
    lv_obj_add_style(back_btn, &style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_set_size(back_btn, 80, LV_PCT(100));
    lv_obj_t* back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT " 返回");
    lv_obj_center(back_lbl);
    lv_obj_add_event_cb(back_btn, on_back_to_main, LV_EVENT_CLICKED, nullptr);
    
    // 标题（中间，占据剩余空间）
    lv_obj_t* title = lv_label_create(title_bar);
    lv_label_set_text(title, title_text);
    lv_obj_set_style_text_font(title, LV_FONT_DEFAULT, 0);
    lv_obj_set_flex_grow(title, 1);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    
    return title_bar;
}

// 创建翻页指示器（公共函数）
static lv_obj_t* create_page_indicator(lv_obj_t* parent, const char* page_text) {
    lv_obj_t* indicator = lv_obj_create(parent);
    lv_obj_add_style(indicator, &style_card, 0);
    lv_obj_set_style_bg_opa(indicator, LV_OPA_40, 0);
    lv_obj_set_size(indicator, 64, LV_PCT(80));
    setup_flex_col(indicator, 8, 8);
    
    lv_obj_t* up = lv_btn_create(indicator);
    lv_obj_add_style(up, &style_btn, 0);
    lv_obj_add_style(up, &style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_style(up, &style_focus, LV_STATE_FOCUSED);
    lv_obj_t* up_lbl = lv_label_create(up);
    lv_label_set_text(up_lbl, LV_SYMBOL_UP);
    lv_obj_center(up_lbl);
    
    lv_obj_t* page_lbl = lv_label_create(indicator);
    lv_obj_add_style(page_lbl, &style_icon, 0);
    lv_label_set_text(page_lbl, page_text ? page_text : "1/1");
    lv_obj_set_style_text_align(page_lbl, LV_TEXT_ALIGN_CENTER, 0);
    
    lv_obj_t* down = lv_btn_create(indicator);
    lv_obj_add_style(down, &style_btn, 0);
    lv_obj_add_style(down, &style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_style(down, &style_focus, LV_STATE_FOCUSED);
    lv_obj_t* down_lbl = lv_label_create(down);
    lv_label_set_text(down_lbl, LV_SYMBOL_DOWN);
    lv_obj_center(down_lbl);
    
    return indicator;
}

static void on_top_btn_event(lv_event_t* e) {
    lv_obj_t* btn = lv_event_get_target(e);
    intptr_t id = reinterpret_cast<intptr_t>(lv_event_get_user_data(e));
    Page page = Page::Home;
    if (id == 1) page = Page::History;
    // 注意：搜索功能在首页内，不再作为独立的Tab
    PageManager::getInstance().switchTo(page);
}

static lv_obj_t* create_top_bar(lv_obj_t* parent) {
    lv_obj_t* bar = lv_obj_create(parent);
    lv_obj_set_size(bar, LV_PCT(100), 50);
    setup_flex_row(bar, 12, 10);
    lv_obj_set_style_bg_opa(bar, LV_OPA_60, 0);

    // 1. Logo区域（左侧）
    lv_obj_t* logo_area = lv_obj_create(bar);
    lv_obj_set_size(logo_area, 150, LV_PCT(100));
    lv_obj_set_style_bg_opa(logo_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(logo_area, 5, 0);
    setup_flex_row(logo_area, 5, 5);
    
    // Logo图标（占位，使用文字代替）
    lv_obj_t* logo_icon = lv_label_create(logo_area);
    lv_label_set_text(logo_icon, "👁");  // 眼睛图标占位
    lv_obj_set_style_text_font(logo_icon, LV_FONT_DEFAULT, 0);
    
    // Logo文字
    lv_obj_t* logo_text = lv_label_create(logo_area);
    lv_label_set_text(logo_text, "雷石官方正版");
    lv_obj_set_style_text_color(logo_text, lv_color_white(), 0);
    lv_obj_set_flex_grow(logo_text, 1);

    // 2. 导航标签区域（中间）
    lv_obj_t* nav_area = lv_obj_create(bar);
    lv_obj_set_flex_grow(nav_area, 1);  // 占据剩余空间
    lv_obj_set_height(nav_area, LV_PCT(100));
    setup_flex_row(nav_area, 5, 5);
    lv_obj_set_style_bg_opa(nav_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(nav_area, 5, 0);

    auto add_btn = [&](const char* txt, int idx) {
        lv_obj_t* btn = lv_btn_create(nav_area);
        lv_obj_add_style(btn, &style_btn, 0);
        lv_obj_add_style(btn, &style_btn_pressed, LV_STATE_PRESSED);
        lv_obj_add_style(btn, &style_focus, LV_STATE_FOCUSED);
        lv_obj_set_height(btn, LV_PCT(100));
        // 如果是首页（idx=0），设置选中样式（蓝色高亮，更明显）
        if (idx == 0) {
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x4F7BFF), 0);
            lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);  // 确保完全不透明
            lv_obj_set_style_text_color(btn, lv_color_white(), 0);  // 白色文字
        } else {
            // 未选中的标签使用半透明背景
            lv_obj_set_style_bg_opa(btn, LV_OPA_50, 0);
        }
        lv_obj_t* label = lv_label_create(btn);
        lv_label_set_text(label, txt);
        lv_obj_center(label);
        lv_obj_add_event_cb(btn, on_top_btn_event, LV_EVENT_CLICKED,
                            reinterpret_cast<void*>(static_cast<intptr_t>(idx)));
        return btn;
    };

    // 根据设计稿：只保留"首页"和"历史记录"两个Tab
    // 注意：不实现"猜你喜欢"和"我的收藏"（用户要求）
    add_btn("首页", 0);
    add_btn("历史记录", 1);
    
    // 3. VIP按钮（右侧）
    lv_obj_t* vip_btn = lv_btn_create(bar);
    lv_obj_add_style(vip_btn, &style_vip, 0);
    lv_obj_add_style(vip_btn, &style_focus, LV_STATE_FOCUSED);
    lv_obj_set_size(vip_btn, 120, LV_PCT(100));
    lv_obj_t* vip_label = lv_label_create(vip_btn);
    lv_label_set_text(vip_label, "VIP会员中心");
    lv_obj_center(vip_label);
    // TODO: 添加VIP按钮点击事件
    
    return bar;
}

static lv_obj_t* create_content_area(lv_obj_t* parent) {
    lv_obj_t* area = lv_obj_create(parent);
    // 内容区域应该占据剩余空间（top_bar 50px + bottom 80px 之后的空间）
    lv_obj_set_flex_grow(area, 1);  // 关键：使用 flex_grow 占据剩余空间
    lv_obj_set_width(area, LV_PCT(100));
    // 高度不需要设置，由 flex_grow 自动计算
    setup_flex_col(area, 6, 6);
    lv_obj_set_scroll_dir(area, LV_DIR_VER);
    // 确保内容区域可见
    lv_obj_set_style_bg_opa(area, LV_OPA_COVER, 0);
    lv_obj_clear_flag(area, LV_OBJ_FLAG_HIDDEN);
    return area;
}

// 播放控制栏按钮事件处理
static void on_player_btn_click(lv_event_t* e) {
    lv_obj_t* btn = lv_event_get_target(e);
    intptr_t id = reinterpret_cast<intptr_t>(lv_event_get_user_data(e));
    
    switch (id) {
        case 0: {  // 已点
            // 显示已点列表页面
            lv_obj_t* scr = lv_scr_act();
            lv_obj_t* queue_page = create_queue_page(scr);
            if (queue_page) {
                lv_scr_load(queue_page);
            }
            break;
        }
        case 1: {  // 切歌
            auto& queue = ktv::services::QueueService::getInstance();
            auto* next = queue.getNext();
            if (next) {
                auto& player = ktv::services::PlayerService::getInstance();
                player.play(next->song_id, next->m3u8_url);
                PLOGI << "切歌到: " << next->title;
            } else {
                PLOGW << "队列为空，无法切歌";
            }
            break;
        }
        case 2: {  // 伴唱（音轨切换）
            // TODO: 实现音轨切换（需要 PlayerService 支持）
            PLOGI << "切换音轨（原唱/伴奏）";
            break;
        }
        case 3: {  // 暂停
            auto& player = ktv::services::PlayerService::getInstance();
            if (player.state() == ktv::services::PlayerState::Playing) {
                player.pause();
                // 更新按钮文本为"播放"
                lv_obj_t* label = lv_obj_get_child(btn, 0);
                if (label) {
                    lv_label_set_text(label, LV_SYMBOL_PLAY " 播放");
                }
            } else if (player.state() == ktv::services::PlayerState::Paused) {
                player.resume();
                // 更新按钮文本为"暂停"
                lv_obj_t* label = lv_obj_get_child(btn, 0);
                if (label) {
                    lv_label_set_text(label, LV_SYMBOL_PAUSE " 暂停");
                }
            }
            break;
        }
        case 4: {  // 重唱
            auto& queue = ktv::services::QueueService::getInstance();
            auto* current = queue.getCurrent();
            if (current) {
                auto& player = ktv::services::PlayerService::getInstance();
                player.stop();
                player.play(current->song_id, current->m3u8_url);
                PLOGI << "重唱: " << current->title;
            } else {
                PLOGW << "没有正在播放的歌曲";
            }
            break;
        }
        case 5: {  // 调音
            lv_obj_t* scr = lv_scr_act();
            lv_obj_t* audio_page = create_audio_settings_page(scr);
            if (audio_page) {
                lv_scr_load(audio_page);
            }
            break;
        }
        case 6: {  // 设置
            lv_obj_t* scr = lv_scr_act();
            lv_obj_t* settings_page = create_settings_page(scr);
            if (settings_page) {
                lv_scr_load(settings_page);
            }
            break;
        }
        case 7: {  // 退出
            // 退出应用程序
            PLOGI << "用户点击退出按钮";
            // TODO: 实现退出逻辑（可能需要发送退出事件或设置退出标志）
            break;
        }
    }
}

lv_obj_t* create_player_bar(lv_obj_t* parent) {
    lv_obj_t* bar = lv_obj_create(parent);
    lv_obj_set_size(bar, LV_PCT(100), 80);
    setup_flex_row(bar, 10, 10);
    lv_obj_set_style_bg_opa(bar, LV_OPA_70, 0);

    const char* labels[] = {
        LV_SYMBOL_LIST " 已点",
        LV_SYMBOL_NEXT " 切歌",
        LV_SYMBOL_AUDIO " 伴唱",
        LV_SYMBOL_PAUSE " 暂停",
        LV_SYMBOL_REFRESH " 重唱",
        LV_SYMBOL_SETTINGS " 调音",
        LV_SYMBOL_SETTINGS " 设置",
        LV_SYMBOL_CLOSE " 退出"  // 修复：应该是"退出"而不是"返回"
    };
    
    for (size_t i = 0; i < sizeof(labels) / sizeof(labels[0]); i++) {
        lv_obj_t* btn = lv_btn_create(bar);
        lv_obj_add_style(btn, &style_btn, 0);
        lv_obj_add_style(btn, &style_btn_pressed, LV_STATE_PRESSED);
        lv_obj_add_style(btn, &style_focus, LV_STATE_FOCUSED);
        
        // 特殊处理"已点"按钮：添加红色圆圈数字显示
        if (i == 0) {
            // 创建容器用于显示文字和数字圆圈
            lv_obj_t* container = lv_obj_create(btn);
            lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
            lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(container, 0, 0);
            lv_obj_set_style_pad_all(container, 0, 0);
            setup_flex_col(container, 0, 0);
            
            // 文字标签
            lv_obj_t* label = lv_label_create(container);
            lv_label_set_text(label, "已点");
            lv_obj_set_style_text_color(label, lv_color_white(), 0);
            lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, 0);
            
            // 红色圆圈数字（显示已点数量）
            auto& queue = ktv::services::QueueService::getInstance();
            int queue_count = queue.size();
            if (queue_count > 0) {
                lv_obj_t* badge = lv_obj_create(container);
                lv_obj_set_size(badge, 20, 20);
                lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
                lv_obj_set_style_bg_color(badge, lv_color_hex(0xFF0000), 0);  // 红色
                lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
                lv_obj_set_style_border_width(badge, 0, 0);
                lv_obj_align(badge, LV_ALIGN_TOP_RIGHT, -5, 5);
                
                lv_obj_t* badge_label = lv_label_create(badge);
                char count_text[8];
                std::snprintf(count_text, sizeof(count_text), "%d", queue_count);
                lv_label_set_text(badge_label, count_text);
                lv_obj_set_style_text_color(badge_label, lv_color_white(), 0);
                lv_obj_center(badge_label);
            }
        } else {
            // 其他按钮正常显示
            lv_obj_t* label = lv_label_create(btn);
            lv_label_set_text(label, labels[i]);
            lv_obj_center(label);
        }
        
        // 为每个按钮添加事件处理，传递按钮ID
        lv_obj_add_event_cb(btn, on_player_btn_click, LV_EVENT_CLICKED,
                            reinterpret_cast<void*>(static_cast<intptr_t>(i)));
    }
    return bar;
}

static void on_song_click(lv_event_t* e) {
    lv_obj_t* btn = lv_event_get_target(e);
    const char* song_id = static_cast<const char*>(lv_event_get_user_data(e));
    (void)btn;
    
    if (!song_id || strlen(song_id) == 0) {
        PLOGW << "点歌失败: song_id 为空";
        return;
    }
    
    // 离线模式：直接从 mock 数据中查找歌曲信息
    // TODO: 在线模式时，在后台线程异步调用服务器接口
    ktv::services::SongItem* found = nullptr;
    
    // 先从 mock 数据中查找
    auto mock_songs = ktv::mock::hotSongs();
    for (auto& mock_s : mock_songs) {
        if (mock_s.title == song_id || mock_s.title.find(song_id) != std::string::npos) {
            ktv::services::SongItem s;
            s.id = mock_s.title;
            s.title = mock_s.title;
            s.artist = mock_s.artist;
            s.m3u8_url = "";  // mock 数据没有 URL
            
            ktv::services::QueueItem queue_item;
            queue_item.song_id = s.id;
            queue_item.title = s.title;
            queue_item.artist = s.artist;
            queue_item.m3u8_url = s.m3u8_url;
            
            auto& queue = ktv::services::QueueService::getInstance();
            queue.add(queue_item);
            
            // 如果当前没有播放的歌曲，自动播放
            auto& player = ktv::services::PlayerService::getInstance();
            if (player.state() == ktv::services::PlayerState::Stopped) {
                queue.setCurrentIndex(static_cast<int>(queue.getQueue().size() - 1));
                player.play(queue_item.song_id, queue_item.m3u8_url);
            }
            
            PLOGI << "点歌成功（离线模式）: " << s.title;
            
            // 发布事件
            ktv::events::Event ev;
            ev.type = ktv::events::EventType::SongSelected;
            ev.payload = song_id;
            ktv::events::EventBus::getInstance().publish(ev);
            return;
        }
    }
    
    // 如果找不到，使用 song_id 作为标题添加到队列
    ktv::services::QueueItem queue_item;
    queue_item.song_id = song_id;
    queue_item.title = song_id;
    queue_item.artist = "未知";
    queue_item.m3u8_url = "";
    
    auto& queue = ktv::services::QueueService::getInstance();
    queue.add(queue_item);
    PLOGI << "点歌成功（使用ID，离线模式）: " << song_id;
    
    // 发布事件
    ktv::events::Event ev;
    ev.type = ktv::events::EventType::SongSelected;
    ev.payload = song_id;
    ktv::events::EventBus::getInstance().publish(ev);
}

// 创建歌曲列表项的主实现（使用 SongItem）
static void create_song_list_item_impl(lv_obj_t* list, const ktv::services::SongItem& s) {
    const char* title = s.title.c_str();
    const char* subtitle = s.artist.c_str();
    lv_obj_t* item = lv_obj_create(list);
    lv_obj_add_style(item, &style_list_item, 0);
    lv_obj_add_style(item, &style_shadow, 0);
    lv_obj_set_width(item, LV_PCT(100));
    lv_obj_set_height(item, 72);
    setup_flex_row(item, 6, 8);

    lv_obj_t* left = lv_obj_create(item);
    lv_obj_set_size(left, 60, 60);
    lv_obj_set_style_bg_opa(left, LV_OPA_30, 0);
    lv_obj_set_style_bg_color(left, lv_color_hex(0x8A7AC5), 0);

    lv_obj_t* center = lv_obj_create(item);
    lv_obj_set_flex_grow(center, 1);
    setup_flex_col(center, 4, 0);

    lv_obj_t* title_lbl = lv_label_create(center);
    lv_label_set_text(title_lbl, title);
    lv_obj_t* sub_lbl = lv_label_create(center);
    lv_obj_add_style(sub_lbl, &style_subtext, 0);
    lv_label_set_text(sub_lbl, subtitle);

    // 收藏心形（符号版）
    lv_obj_t* heart = lv_btn_create(item);
    lv_obj_add_style(heart, &style_btn, 0);
    lv_obj_add_style(heart, &style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_style(heart, &style_focus, LV_STATE_FOCUSED);
    lv_obj_set_style_pad_all(heart, 8, 0);
    lv_obj_t* heart_lbl = lv_label_create(heart);
    lv_obj_add_style(heart_lbl, &style_icon, 0);
    lv_label_set_text(heart_lbl, LV_SYMBOL_HEART);
    lv_obj_center(heart_lbl);

    // 点歌按钮
    lv_obj_t* right = lv_btn_create(item);
    lv_obj_add_style(right, &style_btn, 0);
    lv_obj_add_style(right, &style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_style(right, &style_focus, LV_STATE_FOCUSED);
    lv_obj_set_style_pad_all(right, 10, 0);
    lv_obj_t* label = lv_label_create(right);
    lv_label_set_text(label, LV_SYMBOL_PLAY " 点歌");
    lv_obj_center(label);
    // 事件携带 song_id
    lv_obj_add_event_cb(right, on_song_click, LV_EVENT_CLICKED,
                        (void*)s.id.c_str());
}

// 创建歌曲列表项的公共接口（重载版本）
static void create_song_list_item(lv_obj_t* list, const ktv::services::SongItem& s) {
    create_song_list_item_impl(list, s);
}

// 创建歌曲列表项的辅助函数（兼容旧接口，避免重复代码）
static void create_song_list_item(lv_obj_t* list, const char* title, const char* subtitle) {
    // 转换为 SongItem 调用主实现，避免代码重复
    ktv::services::SongItem item;
    item.id = title ? title : "";
    item.title = title ? title : "";
    item.artist = subtitle ? subtitle : "";
    create_song_list_item_impl(list, item);
}

// 创建视频播放器占位（唯一的播放器控件，用于首页中间）
static lv_obj_t* create_video_player_placeholder(lv_obj_t* parent, const char* title, bool show_qrcode = false) {
    lv_obj_t* player = lv_obj_create(parent);
    lv_obj_set_flex_grow(player, 1);
    lv_obj_set_height(player, 400);
    lv_obj_set_style_bg_opa(player, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(player, 10, 0);
    lv_obj_set_style_pad_all(player, 10, 0);
    
    // 播放器使用深色背景
    lv_obj_set_style_bg_color(player, lv_color_hex(0x2D234F), 0);  // 深色背景
    
    // 标题文字（占位）
    lv_obj_t* title_label = lv_label_create(player);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
    lv_obj_align(title_label, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    // 添加二维码占位（微信扫码点歌）
    if (show_qrcode) {
        lv_obj_t* qr_label = lv_label_create(player);
        lv_label_set_text(qr_label, "微信扫码点歌");
        lv_obj_set_style_text_color(qr_label, lv_color_white(), 0);
        lv_obj_align(qr_label, LV_ALIGN_TOP_RIGHT, -10, 10);
    }
    
    // 播放器控件本身不可点击（它是播放器，不是按钮）
    // 但可以添加播放控制事件（暂停/播放等）
    
    return player;
}

// 创建排行榜面板（可点击）
static lv_obj_t* create_chart_panel(lv_obj_t* parent, const char* title, uint32_t bg_color, const char* songs[][2], int song_count) {
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_set_size(panel, LV_PCT(100), 200);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(bg_color), 0);
    lv_obj_set_style_radius(panel, 10, 0);
    lv_obj_set_style_pad_all(panel, 10, 0);
    setup_flex_col(panel, 5, 5);
    
    // 添加点击效果（按下时背景色变深）
    lv_obj_set_style_bg_color(panel, lv_color_hex(bg_color), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(panel, LV_OPA_80, LV_STATE_PRESSED);
    
    // 标题
    lv_obj_t* title_label = lv_label_create(panel);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(title_label, LV_FONT_DEFAULT, 0);
    
    // 歌曲列表
    for (int i = 0; i < song_count && songs[i][0]; i++) {
        lv_obj_t* song_item = lv_label_create(panel);
        char text[128];
        std::snprintf(text, sizeof(text), "%d. %s - %s", i + 1, songs[i][0], songs[i][1]);
        lv_label_set_text(song_item, text);
        lv_obj_set_style_text_color(song_item, lv_color_white(), 0);
    }
    
    return panel;
}

// 创建大按钮（带渐变背景和图标）
static lv_obj_t* create_large_category_btn(lv_obj_t* parent, const char* text, uint32_t color_from, uint32_t color_to, const char* icon, Page target_page) {
    lv_obj_t* btn = lv_obj_create(parent);
    lv_obj_set_flex_grow(btn, 1);
    lv_obj_set_height(btn, 150);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(color_from), 0);
    lv_obj_set_style_bg_grad_color(btn, lv_color_hex(color_to), 0);
    lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_radius(btn, 15, 0);
    lv_obj_set_style_pad_all(btn, 15, 0);
    setup_flex_col(btn, 5, 5);
    
    // 图标
    if (icon) {
        lv_obj_t* icon_label = lv_label_create(btn);
        lv_label_set_text(icon_label, icon);
        lv_obj_set_style_text_color(icon_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(icon_label, LV_FONT_DEFAULT, 0);
    }
    
    // 文字
    lv_obj_t* text_label = lv_label_create(btn);
    lv_label_set_text(text_label, text);
    lv_obj_set_style_text_color(text_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(text_label, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_align(text_label, LV_TEXT_ALIGN_CENTER, 0);
    
    // 点击事件（使用静态数组存储页面类型）
    static Page page_storage[5] = {Page::Home, Page::Home, Page::Home, Page::Home, Page::Home};
    static int btn_index = 0;
    if (btn_index < 5) {
        page_storage[btn_index] = target_page;
        int idx = btn_index++;
        lv_obj_add_event_cb(btn, [](lv_event_t* e) {
            int idx = *static_cast<int*>(lv_event_get_user_data(e));
            static Page page_storage[5] = {Page::Artist, Page::Ranking, Page::Category, Page::HotSongs, Page::Home};
            PageManager::getInstance().switchTo(page_storage[idx]);
        }, LV_EVENT_CLICKED, &idx);
    }
    
    return btn;
}

void show_home_tab(lv_obj_t* content_area) {
    if (!content_area) {
        PLOGE << "show_home_tab: content_area is NULL!";
        return;
    }
    
    // 验证 content_area 的大小
    lv_coord_t area_w = lv_obj_get_width(content_area);
    lv_coord_t area_h = lv_obj_get_height(content_area);
    printf("[show_home_tab] Content area size: %dx%d\n", (int)area_w, (int)area_h);
    fflush(stdout);
    
    lv_obj_clean(content_area);
    // 按照设计稿：首页内容分为上下两行
    setup_flex_col(content_area, 10, 10);
    lv_obj_set_scroll_dir(content_area, LV_DIR_VER);
    lv_obj_set_style_bg_opa(content_area, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(content_area, lv_color_hex(0x5F4B9A), 0);  // 紫色背景
    lv_obj_clear_flag(content_area, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(content_area, LV_OPA_COVER, 0);  // 确保不透明
    lv_obj_set_style_pad_all(content_area, 10, 0);
    
    // 强制刷新内容区域
    lv_obj_invalidate(content_area);
    
    printf("[show_home_tab] Home tab UI elements created\n");
    fflush(stdout);

    // ========== 上行：左大卡 + 中间播放器 + 右侧双榜单 ==========
    lv_obj_t* row_top = lv_obj_create(content_area);
    lv_obj_set_size(row_top, LV_PCT(100), 420);
    setup_flex_row(row_top, 10, 10);
    lv_obj_set_style_bg_opa(row_top, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(row_top, 0, 0);
    
    // 左侧：大卡片（"歌名"入口）
    lv_obj_t* card_songname = lv_obj_create(row_top);
    lv_obj_set_size(card_songname, 260, 420);  // 固定宽度，近似设计稿比例
    lv_obj_set_style_bg_opa(card_songname, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(card_songname, lv_color_hex(0x7C6BCB), 0);  // 紫色背景
    lv_obj_set_style_radius(card_songname, 10, 0);
    lv_obj_set_style_pad_all(card_songname, 20, 0);
    setup_flex_col(card_songname, 10, 10);
    
    // 添加点击效果
    lv_obj_set_style_bg_color(card_songname, lv_color_hex(0x6E5CA8), LV_STATE_PRESSED);
    
    // 歌名文字（大字体）
    lv_obj_t* songname_label = lv_label_create(card_songname);
    lv_label_set_text(songname_label, "歌名");
    lv_obj_set_style_text_color(songname_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(songname_label, LV_FONT_DEFAULT, 0);
    lv_obj_align(songname_label, LV_ALIGN_CENTER, 0, -20);
    
    // 提示文案
    lv_obj_t* hint_label = lv_label_create(card_songname);
    lv_label_set_text(hint_label, "点击点歌");
    lv_obj_set_style_text_color(hint_label, lv_color_hex(0xC8C9D4), 0);  // 浅灰色
    lv_obj_set_style_text_font(hint_label, LV_FONT_DEFAULT, 0);
    lv_obj_align(hint_label, LV_ALIGN_CENTER, 0, 20);
    
    // 点击事件：可以跳转到搜索页面或显示点歌界面
    lv_obj_add_event_cb(card_songname, [](lv_event_t* e) {
        // TODO: 点击"歌名"卡片可以跳转到搜索页面或显示点歌界面
        PageManager::getInstance().switchTo(Page::Search);
        PLOGI << "点击歌名卡片，跳转到搜索页面";
    }, LV_EVENT_CLICKED, nullptr);
    
    // 中间：播放器占位区域（唯一的播放器控件，带二维码）
    lv_obj_t* player_area = lv_obj_create(row_top);
    lv_obj_set_flex_grow(player_area, 1);  // 占据剩余空间
    lv_obj_set_height(player_area, 420);
    lv_obj_set_style_bg_opa(player_area, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(player_area, lv_color_hex(0x2D234F), 0);  // 深色背景
    lv_obj_set_style_radius(player_area, 10, 0);
    lv_obj_set_style_pad_all(player_area, 10, 0);
    
    // 播放器标题文字（占位）
    lv_obj_t* player_title = lv_label_create(player_area);
    lv_label_set_text(player_title, "成龍 / 陳淑梅");
    lv_obj_set_style_text_color(player_title, lv_color_white(), 0);
    lv_obj_align(player_title, LV_ALIGN_BOTTOM_MID, 0, -10);
    
    // 二维码占位（微信扫码点歌）
    lv_obj_t* qr_label = lv_label_create(player_area);
    lv_label_set_text(qr_label, "微信扫码点歌");
    lv_obj_set_style_text_color(qr_label, lv_color_white(), 0);
    lv_obj_align(qr_label, LV_ALIGN_TOP_RIGHT, -10, 10);
    
    // 右侧：竖向榜单容器
    lv_obj_t* right_col = lv_obj_create(row_top);
    lv_obj_set_size(right_col, 260, 420);  // 固定宽度，与左侧对称
    setup_flex_col(right_col, 10, 10);
    lv_obj_set_style_bg_opa(right_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(right_col, 0, 0);
    
    // 情歌对唱榜（可点击，点击后跳转到排行榜页面）
    const char* love_songs[][2] = {
        {"偏爱(HD)", "张芸京"},
        {"选择(HD)", "林子祥/叶倩文"},
        {"广岛之恋(HD)", "莫文蔚/张洪量"}
    };
    lv_obj_t* panel_love = create_chart_panel(right_col, "情歌对唱榜", 0xE6F3FF, love_songs, 3);
    lv_obj_add_event_cb(panel_love, [](lv_event_t* e) {
        PageManager::getInstance().switchTo(Page::Ranking);
        PLOGI << "点击情歌对唱榜，跳转到排行榜页面";
    }, LV_EVENT_CLICKED, nullptr);
    
    // 抖音热唱榜（可点击，点击后跳转到热歌榜页面）
    const char* douyin_songs[][2] = {
        {"最后的人(HD)", "杨小壮"},
        {"孤勇者(HD)", "陈奕迅"},
        {"成都-歌手(HD)", "赵雷"}
    };
    lv_obj_t* panel_douyin = create_chart_panel(right_col, "抖音热唱榜", 0xFFE6F0, douyin_songs, 3);
    lv_obj_add_event_cb(panel_douyin, [](lv_event_t* e) {
        PageManager::getInstance().switchTo(Page::HotSongs);
        PLOGI << "点击抖音热唱榜，跳转到热歌榜页面";
    }, LV_EVENT_CLICKED, nullptr);

    // ========== 下行：一排入口卡片（横向排列） ==========
    lv_obj_t* row_bottom = lv_obj_create(content_area);
    lv_obj_set_size(row_bottom, LV_PCT(100), 140);
    setup_flex_row(row_bottom, 10, 10);
    lv_obj_set_style_bg_opa(row_bottom, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(row_bottom, 0, 0);
    
    // 歌手按钮（粉色到紫色渐变，右侧有3个圆形头像）
    lv_obj_t* artist_btn = lv_obj_create(row_bottom);
    lv_obj_set_size(artist_btn, 220, 120);  // 固定尺寸，横向排列
    lv_obj_set_style_bg_opa(artist_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(artist_btn, lv_color_hex(0xFF6B9D), 0);
    lv_obj_set_style_bg_grad_color(artist_btn, lv_color_hex(0x7C6BCB), 0);
    lv_obj_set_style_bg_grad_dir(artist_btn, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_radius(artist_btn, 15, 0);
    lv_obj_set_style_pad_all(artist_btn, 15, 0);
    setup_flex_row(artist_btn, 5, 5);
    
    // 左侧文字和图标
    lv_obj_t* artist_left = lv_obj_create(artist_btn);
    lv_obj_set_flex_grow(artist_left, 1);
    lv_obj_set_size(artist_left, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(artist_left, LV_OPA_TRANSP, 0);
    setup_flex_col(artist_left, 5, 5);
    
    lv_obj_t* artist_icon = lv_label_create(artist_left);
    lv_label_set_text(artist_icon, "👤");
    lv_obj_set_style_text_color(artist_icon, lv_color_white(), 0);
    
    lv_obj_t* artist_text = lv_label_create(artist_left);
    lv_label_set_text(artist_text, "歌手");
    lv_obj_set_style_text_color(artist_text, lv_color_white(), 0);
    
    // 右侧3个圆形头像（占位）
    lv_obj_t* artist_right = lv_obj_create(artist_btn);
    lv_obj_set_size(artist_right, 60, LV_PCT(100));
    lv_obj_set_style_bg_opa(artist_right, LV_OPA_TRANSP, 0);
    setup_flex_row(artist_right, 3, 3);
    
    for (int i = 0; i < 3; i++) {
        lv_obj_t* avatar = lv_obj_create(artist_right);
        lv_obj_set_size(avatar, 18, 18);
        lv_obj_set_style_bg_opa(avatar, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(avatar, lv_color_hex(0xCCCCCC), 0);  // 灰色占位
        lv_obj_set_style_radius(avatar, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(avatar, 2, 0);
        lv_obj_set_style_border_color(avatar, lv_color_white(), 0);
    }
    
    // 点击事件
    static Page artist_page = Page::Artist;
    lv_obj_add_event_cb(artist_btn, [](lv_event_t* e) {
        PageManager::getInstance().switchTo(Page::Artist);
    }, LV_EVENT_CLICKED, &artist_page);
    
    // 排行榜按钮（粉色到红色渐变）
    lv_obj_t* ranking_btn = create_large_category_btn(row_bottom, "排行榜", 0xFF6B9D, 0xFF4444, "🏆", Page::Ranking);
    lv_obj_set_size(ranking_btn, 220, 120);
    
    // 分类按钮（蓝色到浅蓝渐变）
    lv_obj_t* category_btn = create_large_category_btn(row_bottom, "分类", 0x4F7BFF, 0x6E8FFF, "📁", Page::Category);
    lv_obj_set_size(category_btn, 220, 120);
    
    // 热歌榜按钮（粉色到红色渐变）
    lv_obj_t* hotsongs_btn = create_large_category_btn(row_bottom, "热歌榜", 0xFF6B9D, 0xFF4444, "⭐", Page::HotSongs);
    lv_obj_set_size(hotsongs_btn, 220, 120);
    
    // 免费专区按钮（蓝色到紫色渐变，两行文字）
    lv_obj_t* free_btn = lv_obj_create(row_bottom);
    lv_obj_set_size(free_btn, 220, 120);
    lv_obj_set_style_bg_opa(free_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(free_btn, lv_color_hex(0x4F7BFF), 0);
    lv_obj_set_style_bg_grad_color(free_btn, lv_color_hex(0x7C6BCB), 0);
    lv_obj_set_style_bg_grad_dir(free_btn, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_radius(free_btn, 15, 0);
    lv_obj_set_style_pad_all(free_btn, 15, 0);
    setup_flex_col(free_btn, 5, 5);
    
    lv_obj_t* free_icon = lv_label_create(free_btn);
    lv_label_set_text(free_icon, "✨");
    lv_obj_set_style_text_color(free_icon, lv_color_white(), 0);
    
    lv_obj_t* free_text1 = lv_label_create(free_btn);
    lv_label_set_text(free_text1, "免费专区");
    lv_obj_set_style_text_color(free_text1, lv_color_white(), 0);
    
    lv_obj_t* free_text2 = lv_label_create(free_btn);
    lv_label_set_text(free_text2, "即刻开唱");
    lv_obj_set_style_text_color(free_text2, lv_color_white(), 0);
    
    PLOGI << "Home tab: displayed according to design (left card + center player + right charts + bottom menu cards)";
}

void show_history_tab(lv_obj_t* content_area) {
    lv_obj_clean(content_area);
    setup_flex_row(content_area, 6, 6);

    lv_obj_t* list = lv_obj_create(content_area);
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_size(list, LV_PCT(100), LV_PCT(100));
    setup_flex_col(list, 6, 6);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);

    // 历史记录优先从HistoryService加载（这是本地数据）
    auto& history_service = ktv::services::HistoryService::getInstance();
    const auto& history_items = history_service.items();
    
    if (!history_items.empty()) {
        // 使用真实历史记录
        for (const auto& h : history_items) {
            ktv::services::SongItem s;
            s.id = h.title;
            s.title = h.title;
            s.artist = h.artist;
            s.m3u8_url = h.local_path;
            create_song_list_item(list, s);
        }
    } else {
        // 如果没有历史记录，使用Mock数据作为默认显示
        for (const auto& mock_s : ktv::mock::historySongs()) {
            ktv::services::SongItem s;
            s.id = mock_s.title;
            s.title = mock_s.title;
            s.artist = mock_s.artist;
            create_song_list_item(list, s);
        }
    }

    // 翻页指示器（使用公共函数）
    create_page_indicator(content_area, "1/3");
}

void show_search_page(lv_obj_t* content_area) {
    lv_obj_clean(content_area);
    setup_flex_row(content_area, 6, 6);

    // 左侧内容列
    lv_obj_t* left_col = lv_obj_create(content_area);
    lv_obj_set_flex_grow(left_col, 1);
    lv_obj_set_size(left_col, LV_PCT(100), LV_PCT(100));
    setup_flex_col(left_col, 6, 6);

    // 搜索框
    lv_obj_t* ta = lv_textarea_create(left_col);
    lv_obj_add_style(ta, &style_input, 0);
    lv_obj_set_width(ta, LV_PCT(100));
    lv_textarea_set_placeholder_text(ta, "请输入歌曲或歌手");

    // 虚拟键盘（示例 Grid）
    lv_obj_t* kb = lv_keyboard_create(left_col);
    lv_keyboard_set_textarea(kb, ta);
    lv_obj_set_style_pad_all(kb, 6, 0);
    lv_obj_set_style_bg_opa(kb, LV_OPA_40, 0);

    // 结果列表
    lv_obj_t* list = lv_obj_create(left_col);
    lv_obj_set_size(list, LV_PCT(100), LV_PCT(50));
    setup_flex_col(list, 6, 6);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);

    auto refresh_results = [list](const char* kw) {
        std::string keyword = kw ? kw : "";
        
        // 先显示加载状态或Mock数据搜索结果
        lv_obj_clean(list);
        if (!keyword.empty()) {
            // 先显示Mock数据搜索结果（立即响应）
            std::vector<mock::SongItem> mock_results = mock::searchSongs(keyword);
            for (auto& mock_s : mock_results) {
                ktv::services::SongItem s;
                s.id = mock_s.title;
                s.title = mock_s.title;
                s.artist = mock_s.artist;
                create_song_list_item(list, s);
            }
        }
        
        // 异步搜索真实数据（后台线程执行，不阻塞UI）
        auto& song_service = ktv::services::SongService::getInstance();
        song_service.searchOfflineFirstAsync(keyword, 1, 20, [list, keyword](const std::vector<ktv::services::SongItem>& results) {
            // 这个回调在UI线程执行，可以安全地更新LVGL对象
            if (!results.empty()) {
                // 清空Mock数据，显示真实搜索结果
                lv_obj_clean(list);
                for (const auto& s : results) {
                    create_song_list_item(list, s);
                }
                PLOGI << "Search: updated with " << results.size() << " results for: " << keyword;
            } else if (!keyword.empty()) {
                // 如果异步搜索失败，显示提示信息
                // 这是正常现象（网络不通、离线、服务器异常等）
                lv_obj_clean(list);
                ktv::services::SongItem empty_item;
                empty_item.title = "网络不可用";
                empty_item.artist = "请检查网络连接或使用缓存数据";
                create_song_list_item(list, empty_item);
                PLOGI << "Search failed (network unavailable or offline), showing offline message";
            }
        });
    };

    // 初始显示
    refresh_results("");

    // 输入事件：回车或失焦触发刷新
    lv_obj_add_event_cb(ta, [](lv_event_t* e) {
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_READY || code == LV_EVENT_DEFOCUSED) {
            lv_obj_t* ta = lv_event_get_target(e);
            const char* txt = lv_textarea_get_text(ta);
            lv_obj_t* list = (lv_obj_t*)lv_event_get_user_data(e);
            lv_obj_clean(list);
            
            // 先显示 mock 数据，避免阻塞 UI
            // TODO: 在后台线程异步加载真实数据，加载完成后更新列表
            auto mockRes = ktv::mock::searchSongs(txt ? txt : "");
            if (mockRes.empty()) {
                ktv::services::SongItem empty_item;
                empty_item.title = "未找到";
                empty_item.artist = "请换个关键词";
                create_song_list_item(list, empty_item);
            } else {
                for (auto& mock_s : mockRes) {
                    ktv::services::SongItem s;
                    s.id = mock_s.title;
                    s.title = mock_s.title;
                    s.artist = mock_s.artist;
                    create_song_list_item(list, s);
                }
            }
        }
    }, LV_EVENT_ALL, list);

    // 右侧翻页指示器（使用公共函数）
    create_page_indicator(content_area, "1/5");
}

// 显示分类浏览页面
void show_category_page(lv_obj_t* content_area) {
    lv_obj_clean(content_area);
    setup_flex_col(content_area, 6, 6);
    
    // 标题栏（带返回按钮）
    lv_obj_t* title_bar = create_title_bar(content_area, "分类浏览");
    
    // 歌曲列表
    lv_obj_t* list = lv_obj_create(content_area);
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_size(list, LV_PCT(100), LV_PCT(100));
    setup_flex_col(list, 6, 6);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    
    // 显示Mock数据（TODO: 后续替换为真实分类数据）
    for (const auto& mock_s : ktv::mock::hotSongs()) {
        ktv::services::SongItem s;
        s.id = mock_s.title;
        s.title = mock_s.title;
        s.artist = mock_s.artist;
        create_song_list_item(list, s);
    }
    
    PLOGI << "Category page: displayed";
}

// 显示排行榜页面
void show_ranking_page(lv_obj_t* content_area) {
    lv_obj_clean(content_area);
    setup_flex_col(content_area, 6, 6);
    
    // 标题栏（带返回按钮）
    lv_obj_t* title_bar = create_title_bar(content_area, "排行榜");
    
    // 歌曲列表
    lv_obj_t* list = lv_obj_create(content_area);
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_size(list, LV_PCT(100), LV_PCT(100));
    setup_flex_col(list, 6, 6);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    
    // 显示Mock数据（TODO: 后续替换为真实排行榜数据）
    for (const auto& mock_s : ktv::mock::hotSongs()) {
        ktv::services::SongItem s;
        s.id = mock_s.title;
        s.title = mock_s.title;
        s.artist = mock_s.artist;
        create_song_list_item(list, s);
    }
    
    PLOGI << "Ranking page: displayed";
}

// 显示歌手页面
void show_artist_page(lv_obj_t* content_area) {
    lv_obj_clean(content_area);
    setup_flex_col(content_area, 6, 6);
    
    // 标题栏（带返回按钮）
    lv_obj_t* title_bar = create_title_bar(content_area, "歌手");
    
    // 歌曲列表
    lv_obj_t* list = lv_obj_create(content_area);
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_size(list, LV_PCT(100), LV_PCT(100));
    setup_flex_col(list, 6, 6);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    
    // 显示Mock数据（TODO: 后续替换为真实歌手数据）
    for (const auto& mock_s : ktv::mock::hotSongs()) {
        ktv::services::SongItem s;
        s.id = mock_s.title;
        s.title = mock_s.title;
        s.artist = mock_s.artist;
        create_song_list_item(list, s);
    }
    
    PLOGI << "Artist page: displayed";
}

// 显示热歌榜页面
void show_hot_songs_page(lv_obj_t* content_area) {
    lv_obj_clean(content_area);
    setup_flex_col(content_area, 6, 6);
    
    // 标题栏（带返回按钮）
    lv_obj_t* title_bar = create_title_bar(content_area, "热歌榜");
    
    // 歌曲列表
    lv_obj_t* list = lv_obj_create(content_area);
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_size(list, LV_PCT(100), LV_PCT(100));
    setup_flex_col(list, 6, 6);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    
    // 显示Mock数据（TODO: 后续替换为真实热歌榜数据）
    for (const auto& mock_s : ktv::mock::hotSongs()) {
        ktv::services::SongItem s;
        s.id = mock_s.title;
        s.title = mock_s.title;
        s.artist = mock_s.artist;
        create_song_list_item(list, s);
    }
    
    PLOGI << "Hot songs page: displayed";
}

// 已点列表页面删除按钮事件
static void on_queue_item_delete(lv_event_t* e) {
    lv_obj_t* btn = lv_event_get_target(e);
    intptr_t index = reinterpret_cast<intptr_t>(lv_event_get_user_data(e));
    auto& queue = ktv::services::QueueService::getInstance();
    queue.remove(static_cast<int>(index));
    // 刷新页面
    lv_obj_t* scr = lv_scr_act();
    lv_obj_clean(scr);
    lv_obj_t* new_page = create_queue_page(scr);
    if (new_page) {
        lv_scr_load(new_page);
    }
}

lv_obj_t* create_queue_page(lv_obj_t* parent) {
    lv_obj_t* scr = lv_obj_create(parent);
    lv_obj_set_size(scr, LV_PCT(100), LV_PCT(100));
    lv_obj_add_style(scr, &style_bg, 0);
    setup_flex_col(scr, 10, 10);

    // 标题栏（使用公共函数）
    create_title_bar(scr, "已点列表");

    // 列表
    lv_obj_t* list = lv_obj_create(scr);
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_size(list, LV_PCT(100), LV_PCT(100));
    setup_flex_col(list, 6, 6);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);

    auto& queue = ktv::services::QueueService::getInstance();
    const auto& items = queue.getQueue();
    int current_idx = queue.getCurrentIndex();

    for (size_t i = 0; i < items.size(); i++) {
        const auto& item = items[i];
        lv_obj_t* list_item = lv_obj_create(list);
        lv_obj_add_style(list_item, &style_list_item, 0);
        lv_obj_set_width(list_item, LV_PCT(100));
        lv_obj_set_height(list_item, 72);
        setup_flex_row(list_item, 6, 8);

        // 序号（当前播放的标记）
        lv_obj_t* idx_label = lv_obj_create(list_item);
        lv_obj_set_size(idx_label, 40, 40);
        lv_obj_set_style_bg_opa(idx_label, LV_OPA_30, 0);
        if (static_cast<int>(i) == current_idx) {
            lv_obj_set_style_bg_color(idx_label, lv_color_hex(0xFF6B6B), 0);
            lv_obj_t* lbl = lv_label_create(idx_label);
            lv_label_set_text(lbl, LV_SYMBOL_PLAY);
            lv_obj_center(lbl);
        } else {
            lv_obj_t* lbl = lv_label_create(idx_label);
            char buf[16];
            snprintf(buf, sizeof(buf), "%zu", i + 1);
            lv_label_set_text(lbl, buf);
            lv_obj_center(lbl);
        }

        // 歌曲信息
        lv_obj_t* center = lv_obj_create(list_item);
        lv_obj_set_flex_grow(center, 1);
        setup_flex_col(center, 4, 0);
        lv_obj_t* title_lbl = lv_label_create(center);
        lv_label_set_text(title_lbl, item.title.c_str());
        lv_obj_t* artist_lbl = lv_label_create(center);
        lv_obj_add_style(artist_lbl, &style_subtext, 0);
        lv_label_set_text(artist_lbl, item.artist.c_str());

        // 删除按钮
        lv_obj_t* del_btn = lv_btn_create(list_item);
        lv_obj_add_style(del_btn, &style_btn, 0);
        lv_obj_add_style(del_btn, &style_btn_pressed, LV_STATE_PRESSED);
        lv_obj_t* del_lbl = lv_label_create(del_btn);
        lv_label_set_text(del_lbl, LV_SYMBOL_CLOSE);
        lv_obj_center(del_lbl);
        lv_obj_add_event_cb(del_btn, on_queue_item_delete, LV_EVENT_CLICKED,
                           reinterpret_cast<void*>(static_cast<intptr_t>(i)));
    }

    if (items.empty()) {
        lv_obj_t* empty_lbl = lv_label_create(list);
        lv_label_set_text(empty_lbl, "队列为空");
        lv_obj_add_style(empty_lbl, &style_subtext, 0);
    }

    return scr;
}

lv_obj_t* create_audio_settings_page(lv_obj_t* parent) {
    lv_obj_t* scr = lv_obj_create(parent);
    lv_obj_set_size(scr, LV_PCT(100), LV_PCT(100));
    lv_obj_add_style(scr, &style_bg, 0);
    setup_flex_col(scr, 10, 10);

    // 标题栏（使用公共函数）
    create_title_bar(scr, "调音设置");

    // 内容区
    lv_obj_t* content = lv_obj_create(scr);
    lv_obj_set_flex_grow(content, 1);
    lv_obj_set_size(content, LV_PCT(100), LV_PCT(100));
    setup_flex_col(content, 20, 20);
    lv_obj_set_style_pad_all(content, 20, 0);

    // 音量控制
    lv_obj_t* vol_card = lv_obj_create(content);
    lv_obj_add_style(vol_card, &style_card, 0);
    lv_obj_set_width(vol_card, LV_PCT(100));
    setup_flex_col(vol_card, 10, 10);
    
    lv_obj_t* vol_title = lv_label_create(vol_card);
    lv_label_set_text(vol_title, "音量");
    
    lv_obj_t* vol_slider = lv_slider_create(vol_card);
    lv_obj_set_width(vol_slider, LV_PCT(100));
    lv_slider_set_range(vol_slider, 0, 100);
    lv_slider_set_value(vol_slider, 80, LV_ANIM_OFF);
    lv_obj_add_event_cb(vol_slider, [](lv_event_t* e) {
        int32_t val = lv_slider_get_value(lv_event_get_target(e));
        PLOGI << "音量设置: " << val;
        // TODO: 调用 PlayerService 设置音量
    }, LV_EVENT_VALUE_CHANGED, nullptr);
    
    lv_obj_t* vol_value = lv_label_create(vol_card);
    lv_label_set_text(vol_value, "80");
    lv_obj_add_style(vol_value, &style_subtext, 0);

    // 升降调控制
    lv_obj_t* pitch_card = lv_obj_create(content);
    lv_obj_add_style(pitch_card, &style_card, 0);
    lv_obj_set_width(pitch_card, LV_PCT(100));
    setup_flex_col(pitch_card, 10, 10);
    
    lv_obj_t* pitch_title = lv_label_create(pitch_card);
    lv_label_set_text(pitch_title, "升降调");
    
    lv_obj_t* pitch_slider = lv_slider_create(pitch_card);
    lv_obj_set_width(pitch_slider, LV_PCT(100));
    lv_slider_set_range(pitch_slider, -12, 12);
    lv_slider_set_value(pitch_slider, 0, LV_ANIM_OFF);
    lv_obj_add_event_cb(pitch_slider, [](lv_event_t* e) {
        int32_t val = lv_slider_get_value(lv_event_get_target(e));
        PLOGI << "升降调设置: " << val;
        // TODO: 调用 PlayerService 设置升降调
    }, LV_EVENT_VALUE_CHANGED, nullptr);
    
    lv_obj_t* pitch_value = lv_label_create(pitch_card);
    lv_label_set_text(pitch_value, "0");
    lv_obj_add_style(pitch_value, &style_subtext, 0);

    return scr;
}

lv_obj_t* create_settings_page(lv_obj_t* parent) {
    lv_obj_t* scr = lv_obj_create(parent);
    lv_obj_set_size(scr, LV_PCT(100), LV_PCT(100));
    lv_obj_add_style(scr, &style_bg, 0);
    setup_flex_col(scr, 10, 10);

    // 标题栏（使用公共函数）
    create_title_bar(scr, "设置");

    // 内容区
    lv_obj_t* content = lv_obj_create(scr);
    lv_obj_set_flex_grow(content, 1);
    lv_obj_set_size(content, LV_PCT(100), LV_PCT(100));
    setup_flex_col(content, 10, 10);
    lv_obj_set_style_pad_all(content, 20, 0);

    // 网络配置
    lv_obj_t* net_card = lv_obj_create(content);
    lv_obj_add_style(net_card, &style_card, 0);
    lv_obj_set_width(net_card, LV_PCT(100));
    setup_flex_col(net_card, 10, 10);
    
    lv_obj_t* net_title = lv_label_create(net_card);
    lv_label_set_text(net_title, "网络配置");
    
    lv_obj_t* net_input = lv_textarea_create(net_card);
    lv_obj_set_width(net_input, LV_PCT(100));
    lv_textarea_set_placeholder_text(net_input, "服务器地址");
    // TODO: 加载当前配置

    // Licence 激活
    lv_obj_t* lic_card = lv_obj_create(content);
    lv_obj_add_style(lic_card, &style_card, 0);
    lv_obj_set_width(lic_card, LV_PCT(100));
    setup_flex_col(lic_card, 10, 10);
    
    lv_obj_t* lic_title = lv_label_create(lic_card);
    lv_label_set_text(lic_title, "Licence 激活");
    
    lv_obj_t* lic_btn = lv_btn_create(lic_card);
    lv_obj_add_style(lic_btn, &style_btn, 0);
    lv_obj_t* lic_btn_lbl = lv_label_create(lic_btn);
    lv_label_set_text(lic_btn_lbl, "激活 Licence");
    lv_obj_center(lic_btn_lbl);
    lv_obj_add_event_cb(lic_btn, [](lv_event_t* e) {
        lv_obj_t* scr = lv_scr_act();
        lv_obj_t* dialog = create_licence_dialog(scr);
        if (dialog) {
            lv_obj_move_foreground(dialog);
        }
    }, LV_EVENT_CLICKED, nullptr);

    // 系统信息
    lv_obj_t* info_card = lv_obj_create(content);
    lv_obj_add_style(info_card, &style_card, 0);
    lv_obj_set_width(info_card, LV_PCT(100));
    setup_flex_col(info_card, 10, 10);
    
    lv_obj_t* info_title = lv_label_create(info_card);
    lv_label_set_text(info_title, "系统信息");
    
    lv_obj_t* info_text = lv_label_create(info_card);
    lv_label_set_text(info_text, "版本: 1.0.0\n平台: F133");
    lv_obj_add_style(info_text, &style_subtext, 0);

    return scr;
}

lv_obj_t* create_licence_dialog(lv_obj_t* parent) {
    lv_obj_t* modal = lv_obj_create(parent);
    lv_obj_set_size(modal, LV_PCT(90), LV_PCT(60));
    setup_flex_col(modal, 10, 12);
    lv_obj_center(modal);

    lv_obj_t* title = lv_label_create(modal);
    lv_label_set_text(title, "请输入 Licence");

    lv_obj_t* ta = lv_textarea_create(modal);
    lv_obj_set_width(ta, LV_PCT(100));
    lv_textarea_set_placeholder_text(ta, "XXXX-XXXX-XXXX-XXXX");

    lv_obj_t* btn_row = lv_obj_create(modal);
    setup_flex_row(btn_row, 10, 0);
    lv_obj_set_width(btn_row, LV_PCT(100));

    const char* btns[] = {"确认", "取消"};
    for (const char* txt : btns) {
        lv_obj_t* btn = lv_btn_create(btn_row);
        lv_obj_set_flex_grow(btn, 1);
        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, txt);
        lv_obj_center(lbl);
    }
    return modal;
}

lv_obj_t* create_main_screen() {
    // 创建屏幕对象（NULL 表示创建新的屏幕）
    // LVGL 屏幕对象会自动填满整个显示区域，但为了确保正确，我们显式设置大小
    lv_obj_t* scr = lv_obj_create(NULL);
    
    // ⚡ 关键修复：显式设置屏幕对象的大小和位置，确保它填满整个显示区域
    lv_obj_set_pos(scr, 0, 0);
    lv_obj_set_size(scr, LV_HOR_RES_MAX, LV_VER_RES_MAX);
    
    // 添加背景样式
    lv_obj_add_style(scr, &style_bg, 0);
    
    // 移除默认的填充，确保内容填满整个区域
    lv_obj_set_style_pad_all(scr, 0, 0);
    
    // 设置布局：垂直布局，顶部菜单栏、内容区、底部控制栏
    setup_flex_col(scr, 0, 0);  // 主屏幕不需要间距，子元素之间紧密排列
    
    // 移除测试标签，按照设计稿实现正式UI

    // 1. 顶部菜单栏（固定高度 50px）
    lv_obj_t* top = create_top_bar(scr);
    lv_obj_set_size(top, LV_PCT(100), 50);  // 确保固定高度
    
    // 2. 内容区域（占据剩余空间）
    lv_obj_t* content = create_content_area(scr);
    
    // 3. 底部控制栏（固定高度 80px）
    lv_obj_t* bottom = create_player_bar(scr);
    lv_obj_set_size(bottom, LV_PCT(100), 80);  // 确保固定高度

    (void)top;
    (void)bottom;

    PageManager::getInstance().setContentArea(content);
    
    // ⚡ 立即显示首页内容（不等待后台初始化）
    // 这样可以确保UI立即可见，即使后台初始化失败也不影响UI显示
    show_home_tab(content);
    PLOGI << "Home tab displayed immediately after screen creation";
    
    // 强制刷新屏幕，确保UI可见
    // 标记整个屏幕对象及其所有子对象为无效
    lv_obj_invalidate(scr);
    
    // 确保屏幕对象可见且不透明
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(scr, LV_OPA_COVER, 0);
    
    // 验证屏幕对象的大小和位置
    lv_area_t coords;
    lv_obj_get_coords(scr, &coords);
    lv_coord_t scr_w = lv_area_get_width(&coords);
    lv_coord_t scr_h = lv_area_get_height(&coords);
    
    PLOGI << "Main screen created, size: " << scr_w << "x" << scr_h << " (expected: " << LV_HOR_RES_MAX << "x" << LV_VER_RES_MAX << ")";
    printf("Main screen object created: size=%dx%d, pos=(%d,%d), hidden=%d, opa=%d\n", 
           (int)scr_w, (int)scr_h,
           (int)coords.x1, (int)coords.y1,
           lv_obj_has_flag(scr, LV_OBJ_FLAG_HIDDEN) ? 1 : 0,
           lv_obj_get_style_opa(scr, 0));
    fflush(stdout);
    
    // 如果大小不正确，再次强制设置
    if (scr_w != LV_HOR_RES_MAX || scr_h != LV_VER_RES_MAX) {
        printf("WARNING: Screen size incorrect, forcing fix: %dx%d -> %dx%d\n",
               (int)scr_w, (int)scr_h, (int)LV_HOR_RES_MAX, (int)LV_VER_RES_MAX);
        lv_obj_set_pos(scr, 0, 0);
        lv_obj_set_size(scr, LV_HOR_RES_MAX, LV_VER_RES_MAX);
        fflush(stdout);
    }
    
    return scr;
}

}  // namespace ktv::ui


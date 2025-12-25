#include "layouts.h"
#include "page_manager.h"
#include "../services/mock_data.h"
#include "../services/song_service.h"
#include "../events/event_bus.h"
#include <plog/Log.h>
#include <vector>
#include <string>

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

static void on_top_btn_event(lv_event_t* e) {
    lv_obj_t* btn = lv_event_get_target(e);
    intptr_t id = reinterpret_cast<intptr_t>(lv_event_get_user_data(e));
    Page page = Page::Home;
    if (id == 1) page = Page::History;
    else if (id == 2) page = Page::Search;
    PageManager::getInstance().switchTo(page);
}

static lv_obj_t* create_top_bar(lv_obj_t* parent) {
    lv_obj_t* bar = lv_obj_create(parent);
    lv_obj_set_size(bar, LV_PCT(100), 50);
    setup_flex_row(bar, 12, 10);
    lv_obj_set_style_bg_opa(bar, LV_OPA_60, 0);

    auto add_btn = [&](const char* txt, int idx) {
        lv_obj_t* btn = lv_btn_create(bar);
        lv_obj_add_style(btn, &style_btn, 0);
        lv_obj_add_style(btn, &style_btn_pressed, LV_STATE_PRESSED);
        lv_obj_add_style(btn, &style_focus, LV_STATE_FOCUSED);
        lv_obj_set_height(btn, LV_PCT(100));
        lv_obj_t* label = lv_label_create(btn);
        lv_label_set_text(label, txt);
        lv_obj_center(label);
        lv_obj_add_event_cb(btn, on_top_btn_event, LV_EVENT_CLICKED,
                            reinterpret_cast<void*>(static_cast<intptr_t>(idx)));
        return btn;
    };

    add_btn("首页", 0);
    add_btn("历史记录", 1);
    add_btn("搜索", 2);
    // 占位弹性伸展，将 VIP 推到右侧
    lv_obj_t* spacer = lv_obj_create(bar);
    lv_obj_set_size(spacer, 1, 1);
    lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_grow(spacer, 1);

    // VIP 按钮
    lv_obj_t* vip = lv_btn_create(bar);
    lv_obj_add_style(vip, &style_vip, 0);
    lv_obj_add_style(vip, &style_focus, LV_STATE_FOCUSED);
    lv_obj_set_height(vip, LV_PCT(100));
    lv_obj_t* vip_lbl = lv_label_create(vip);
    lv_label_set_text(vip_lbl, "VIP会员中心");
    lv_obj_center(vip_lbl);
    return bar;
}

static lv_obj_t* create_content_area(lv_obj_t* parent) {
    lv_obj_t* area = lv_obj_create(parent);
    lv_obj_set_size(area, LV_PCT(100), LV_PCT(100));
    setup_flex_col(area, 6, 6);
    lv_obj_set_scroll_dir(area, LV_DIR_VER);
    return area;
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
        LV_SYMBOL_CLOSE " 返回"
    };
    for (const char* txt : labels) {
        lv_obj_t* btn = lv_btn_create(bar);
        lv_obj_add_style(btn, &style_btn, 0);
        lv_obj_add_style(btn, &style_btn_pressed, LV_STATE_PRESSED);
        lv_obj_add_style(btn, &style_focus, LV_STATE_FOCUSED);
        lv_obj_t* label = lv_label_create(btn);
        lv_label_set_text(label, txt);
        lv_obj_center(label);
    }
    return bar;
}

static void on_song_click(lv_event_t* e) {
    lv_obj_t* btn = lv_event_get_target(e);
    const char* title = static_cast<const char*>(lv_event_get_user_data(e));
    (void)btn;
    // 直接调用点歌接口；若失败则记录日志
    std::string song_id = title ? title : "";
    bool ok = ktv::services::SongService::getInstance().addToQueue(song_id);
    if (!ok) {
        PLOGW << "点歌失败: " << song_id;
    } else {
        PLOGI << "点歌成功: " << song_id;
        ktv::events::Event ev;
        ev.type = ktv::events::EventType::SongSelected;
        ev.payload = song_id;
        ktv::events::EventBus::getInstance().publish(ev);
    }
}

static void create_song_list_item(lv_obj_t* list, const char* title, const char* subtitle) {
    // Deprecated helper kept for fallback; prefer overload with SongItem
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
    lv_label_set_text(heart_lbl, LV_SYMBOL_HEART);  // 默认空心/实心可后续切换
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
    lv_obj_add_event_cb(right, on_song_click, LV_EVENT_CLICKED, (void*)title);
}

static void create_song_list_item(lv_obj_t* list, const ktv::services::SongItem& s) {
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

void show_home_tab(lv_obj_t* content_area) {
    lv_obj_clean(content_area);
    setup_flex_row(content_area, 6, 6);

    lv_obj_t* list = lv_obj_create(content_area);
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_size(list, LV_PCT(100), LV_PCT(100));
    setup_flex_col(list, 6, 6);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);

    auto songs = ktv::services::SongService::getInstance().listSongs(1, 20);
    if (songs.empty()) {
        // fallback to mock
        for (const auto& s : ktv::mock::hotSongs()) {
            create_song_list_item(list, s.title.c_str(), s.artist.c_str());
        }
    } else {
        for (const auto& s : songs) {
            create_song_list_item(list, s);
        }
    }

    // 翻页指示器（符号版）
    lv_obj_t* indicator = lv_obj_create(content_area);
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
    lv_label_set_text(page_lbl, "1/10");
    lv_obj_set_style_text_align(page_lbl, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t* down = lv_btn_create(indicator);
    lv_obj_add_style(down, &style_btn, 0);
    lv_obj_add_style(down, &style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_style(down, &style_focus, LV_STATE_FOCUSED);
    lv_obj_t* down_lbl = lv_label_create(down);
    lv_label_set_text(down_lbl, LV_SYMBOL_DOWN);
    lv_obj_center(down_lbl);
}

void show_history_tab(lv_obj_t* content_area) {
    lv_obj_clean(content_area);
    setup_flex_row(content_area, 6, 6);

    lv_obj_t* list = lv_obj_create(content_area);
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_size(list, LV_PCT(100), LV_PCT(100));
    setup_flex_col(list, 6, 6);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);

    auto songs = ktv::services::SongService::getInstance().listSongs(1, 20);
    if (songs.empty()) {
        for (const auto& s : ktv::mock::historySongs()) {
            create_song_list_item(list, s.title.c_str(), s.artist.c_str());
        }
    } else {
        for (const auto& s : songs) {
            create_song_list_item(list, s);
        }
    }

    lv_obj_t* indicator = lv_obj_create(content_area);
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
    lv_label_set_text(page_lbl, "1/3");
    lv_obj_set_style_text_align(page_lbl, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t* down = lv_btn_create(indicator);
    lv_obj_add_style(down, &style_btn, 0);
    lv_obj_add_style(down, &style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_style(down, &style_focus, LV_STATE_FOCUSED);
    lv_obj_t* down_lbl = lv_label_create(down);
    lv_label_set_text(down_lbl, LV_SYMBOL_DOWN);
    lv_obj_center(down_lbl);
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
        lv_obj_clean(list);
        std::vector<mock::SongItem> results = mock::searchSongs(kw ? kw : "");
        if (results.empty()) {
            create_song_list_item(list, "未找到", "请换个关键词");
            return;
        }
        for (auto& s : results) {
            create_song_list_item(list, s.title.c_str(), s.artist.c_str());
        }
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
            auto results = ktv::services::SongService::getInstance().search(txt ? txt : "", 1, 20);
            if (results.empty()) {
                auto mockRes = ktv::mock::searchSongs(txt ? txt : "");
                if (mockRes.empty()) {
                    create_song_list_item(list, "未找到", "请换个关键词");
                } else {
                    for (auto& s : mockRes) {
                        create_song_list_item(list, s.title.c_str(), s.artist.c_str());
                    }
                }
                return;
            }
            for (auto& s : results) {
                create_song_list_item(list, s);
            }
        }
    }, LV_EVENT_ALL, list);

    // 右侧翻页指示器
    lv_obj_t* indicator = lv_obj_create(content_area);
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
    lv_label_set_text(page_lbl, "1/5");
    lv_obj_set_style_text_align(page_lbl, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t* down = lv_btn_create(indicator);
    lv_obj_add_style(down, &style_btn, 0);
    lv_obj_add_style(down, &style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_style(down, &style_focus, LV_STATE_FOCUSED);
    lv_obj_t* down_lbl = lv_label_create(down);
    lv_label_set_text(down_lbl, LV_SYMBOL_DOWN);
    lv_obj_center(down_lbl);
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
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_add_style(scr, &style_bg, 0);
    setup_flex_col(scr, 6, 6);

    lv_obj_t* top = create_top_bar(scr);
    lv_obj_t* content = create_content_area(scr);
    lv_obj_t* bottom = create_player_bar(scr);

    (void)top;
    (void)bottom;

    PageManager::getInstance().setContentArea(content);
    show_home_tab(content);
    return scr;
}

}  // namespace ktv::ui


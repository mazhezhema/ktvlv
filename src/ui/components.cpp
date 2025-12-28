#include "components.h"
#include "ui_scale.h"
#include "../services/song_service.h"
#include "../events/event_bus.h"
#include <plog/Log.h>

namespace ktv::ui::components {

static lv_style_t style_gradient_card;
static lv_style_t style_action_btn;
static lv_style_t style_action_btn_pressed;
static lv_style_t style_action_btn_disabled;
static bool styles_initialized = false;

static void initStyles() {
    if (styles_initialized) return;
    styles_initialized = true;

    // 渐变卡片样式
    lv_style_init(&style_gradient_card);
    lv_style_set_radius(&style_gradient_card, UIScale::s(48));
    lv_style_set_border_color(&style_gradient_card, lv_color_hex(0xFFFFFF));
    lv_style_set_border_opa(&style_gradient_card, 89);  // 35% opacity (35 * 255 / 100 ≈ 89)
    lv_style_set_border_width(&style_gradient_card, 2);
    lv_style_set_shadow_color(&style_gradient_card, lv_color_hex(0x000000));
    lv_style_set_shadow_width(&style_gradient_card, UIScale::s(25));
    lv_style_set_shadow_ofs_y(&style_gradient_card, UIScale::s(10));
    lv_style_set_shadow_opa(&style_gradient_card, LV_OPA_40);
    lv_style_set_pad_all(&style_gradient_card, 0);

    // 操作按钮样式
    lv_style_init(&style_action_btn);
    lv_style_set_radius(&style_action_btn, UIScale::s(20));
    lv_style_set_bg_color(&style_action_btn, lv_color_hex(0x1e1b4b));
    lv_style_set_bg_opa(&style_action_btn, 217);  // 85% opacity (85 * 255 / 100 ≈ 217)
    lv_style_set_border_color(&style_action_btn, lv_color_hex(0xFFFFFF));
    lv_style_set_border_opa(&style_action_btn, 38);  // 15% opacity (15 * 255 / 100 ≈ 38)
    lv_style_set_border_width(&style_action_btn, 1);
    lv_style_set_text_color(&style_action_btn, lv_color_hex(0xE6E6E6));
    lv_style_set_text_opa(&style_action_btn, LV_OPA_90);
    lv_style_set_pad_all(&style_action_btn, UIScale::s(10));

    lv_style_init(&style_action_btn_pressed);
    lv_style_set_bg_color(&style_action_btn_pressed, lv_color_hex(0x312e81));
    lv_style_set_bg_opa(&style_action_btn_pressed, LV_OPA_100);

    lv_style_init(&style_action_btn_disabled);
    lv_style_set_text_opa(&style_action_btn_disabled, LV_OPA_50);
}

lv_obj_t* createGradientCard(lv_obj_t* parent, uint32_t color_start, uint32_t color_end,
                              lv_coord_t radius) {
    initStyles();
    
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_add_style(card, &style_gradient_card, 0);
    // 直接设置对象样式，而不是修改全局样式
    lv_obj_set_style_radius(card, radius, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(color_start), 0);
    lv_obj_set_style_bg_grad_color(card, lv_color_hex(color_end), 0);
    lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_100, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    
    return card;
}

static void onSongClick(lv_event_t* e) {
    const char* song_id = static_cast<const char*>(lv_event_get_user_data(e));
    if (!song_id) return;
    
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

lv_obj_t* createActionButton(lv_obj_t* parent, const char* text, bool enabled) {
    initStyles();
    
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_flex_grow(btn, 1);
    lv_obj_add_style(btn, &style_action_btn, 0);
    lv_obj_add_style(btn, &style_action_btn_pressed, LV_STATE_PRESSED);
    
    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    
    if (!enabled) {
        lv_obj_add_style(label, &style_action_btn_disabled, 0);
    }
    
    return btn;
}

lv_obj_t* createSongListItem(lv_obj_t* parent, const char* title, const char* subtitle,
                             const char* song_id) {
    lv_obj_t* item = lv_obj_create(parent);
    lv_obj_set_width(item, LV_PCT(100));
    lv_obj_set_height(item, UIScale::s(72));
    lv_obj_set_style_radius(item, UIScale::s(12), 0);
    lv_obj_set_style_bg_color(item, lv_color_hex(0x67579E), 0);
    lv_obj_set_style_bg_opa(item, LV_OPA_50, 0);
    lv_obj_set_style_pad_all(item, UIScale::s(8), 0);
    lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
    
    // 使用 flex 布局
    lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(item, UIScale::s(6), 0);
    
    // 标题和副标题
    lv_obj_t* text_container = lv_obj_create(item);
    lv_obj_set_flex_grow(text_container, 1);
    lv_obj_set_style_bg_opa(text_container, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(text_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(text_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(text_container, UIScale::s(4), 0);
    
    lv_obj_t* title_lbl = lv_label_create(text_container);
    lv_label_set_text(title_lbl, title);
    lv_obj_set_style_text_color(title_lbl, lv_color_white(), 0);
    
    if (subtitle) {
        lv_obj_t* sub_lbl = lv_label_create(text_container);
        lv_label_set_text(sub_lbl, subtitle);
        lv_obj_set_style_text_color(sub_lbl, lv_color_hex(0xC8C9D4), 0);
    }
    
    // 点歌按钮
    if (song_id) {
        lv_obj_t* play_btn = lv_btn_create(item);
        lv_obj_set_style_pad_all(play_btn, UIScale::s(10), 0);
        lv_obj_t* play_lbl = lv_label_create(play_btn);
        lv_label_set_text(play_lbl, LV_SYMBOL_PLAY " 点歌");
        lv_obj_center(play_lbl);
        lv_obj_add_event_cb(play_btn, onSongClick, LV_EVENT_CLICKED, (void*)song_id);
    }
    
    return item;
}

}  // namespace ktv::ui::components


#ifndef KTVLV_UI_LAYOUTS_H
#define KTVLV_UI_LAYOUTS_H

#include <lvgl.h>

namespace ktv::ui {

// 初始化主题样式（颜色、圆角、阴影等）
void init_ui_theme();

// 创建主屏（含顶部菜单、内容区、底部播放条）
lv_obj_t* create_main_screen();

// 切换到首页内容
void show_home_tab(lv_obj_t* content_area);

// 切换到历史记录内容
void show_history_tab(lv_obj_t* content_area);

// 显示搜索页
void show_search_page(lv_obj_t* content_area);

// 显示播放浮层/控制条
lv_obj_t* create_player_bar(lv_obj_t* parent);

// 显示 Licence 弹窗（占位）
lv_obj_t* create_licence_dialog(lv_obj_t* parent);

// 显示已点列表页面
lv_obj_t* create_queue_page(lv_obj_t* parent);

// 显示调音页面
lv_obj_t* create_audio_settings_page(lv_obj_t* parent);

// 显示设置页面
lv_obj_t* create_settings_page(lv_obj_t* parent);

}  // namespace ktv::ui

#endif  // KTVLV_UI_LAYOUTS_H


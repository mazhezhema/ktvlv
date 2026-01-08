#ifndef KTVLV_UI_COMPONENTS_H
#define KTVLV_UI_COMPONENTS_H

#include <lvgl.h>

namespace ktv::ui::components {

/**
 * 创建渐变卡片
 * @param parent 父容器
 * @param color_start 起始颜色（hex）
 * @param color_end 结束颜色（hex）
 * @param radius 圆角半径（已缩放）
 * @return 卡片对象
 */
lv_obj_t* createGradientCard(lv_obj_t* parent, uint32_t color_start, uint32_t color_end,
                              lv_coord_t radius = 48);

/**
 * 创建操作按钮（底部控制栏用）
 * @param parent 父容器
 * @param text 按钮文本
 * @param enabled 是否启用（影响透明度）
 * @return 按钮对象
 */
lv_obj_t* createActionButton(lv_obj_t* parent, const char* text, bool enabled = true);

/**
 * 创建列表项（歌曲列表用）
 * @param parent 父容器
 * @param title 标题
 * @param subtitle 副标题
 * @param song_id 歌曲ID（用于点击事件）
 * @return 列表项对象
 */
lv_obj_t* createSongListItem(lv_obj_t* parent, const char* title, const char* subtitle,
                             const char* song_id = nullptr);

}  // namespace ktv::ui::components

#endif  // KTVLV_UI_COMPONENTS_H











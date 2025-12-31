#include "ui_scale.h"
#include <algorithm>
#include <cmath>

namespace ktv::ui {

void UIScale::initialize(lv_coord_t screen_width, lv_coord_t screen_height,
                         lv_coord_t design_width, lv_coord_t design_height) {
    screen_width_ = screen_width;
    screen_height_ = screen_height;
    design_width_ = design_width;
    design_height_ = design_height;

    // 计算缩放比例：取宽高比例的最小值，保证内容不溢出
    float scale_w = static_cast<float>(screen_width) / design_width;
    float scale_h = static_cast<float>(screen_height) / design_height;
    scale_ = std::min(scale_w, scale_h);

    // 限制缩放范围：0.5x ~ 2.0x，避免极端情况
    scale_ = std::max(0.5f, std::min(2.0f, scale_));
}

lv_coord_t UIScale::scale(lv_coord_t value) const {
    return static_cast<lv_coord_t>(std::round(value * scale_));
}

const lv_font_t* UIScale::scaleFont(const lv_font_t* base_font) const {
    // LVGL 字体系统较复杂，这里简化处理
    // 实际项目中可以维护字体映射表
    // 暂时返回原字体，后续可扩展
    (void)scale_;  // 避免未使用警告
    return base_font;
}

}  // namespace ktv::ui








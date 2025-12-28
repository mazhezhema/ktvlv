#ifndef KTVLV_UI_UI_SCALE_H
#define KTVLV_UI_UI_SCALE_H

#include <lvgl.h>

namespace ktv::ui {

/**
 * 全局UI缩放系统
 * 基于设计稿 1920x1080，自动适配不同分辨率
 */
class UIScale {
public:
    static UIScale& getInstance() {
        static UIScale instance;
        return instance;
    }

    UIScale(const UIScale&) = delete;
    UIScale& operator=(const UIScale&) = delete;

    /**
     * 初始化缩放系统
     * @param screen_width 实际屏幕宽度
     * @param screen_height 实际屏幕高度
     * @param design_width 设计稿宽度（默认1920）
     * @param design_height 设计稿高度（默认1080）
     */
    void initialize(lv_coord_t screen_width, lv_coord_t screen_height,
                    lv_coord_t design_width = 1920, lv_coord_t design_height = 1080);

    /**
     * 获取缩放比例
     */
    float getScale() const { return scale_; }

    /**
     * 缩放坐标值（用于尺寸、边距等）
     */
    lv_coord_t scale(lv_coord_t value) const;

    /**
     * 缩放字体大小（返回最接近的可用字体）
     */
    const lv_font_t* scaleFont(const lv_font_t* base_font) const;

    /**
     * 快速缩放函数（内联优化）
     * 注意：必须在 initialize() 之后调用
     */
    static inline lv_coord_t s(lv_coord_t value) {
        return getInstance().scale(value);
    }

private:
    UIScale() = default;
    ~UIScale() = default;

    float scale_ = 1.0f;
    lv_coord_t screen_width_ = 1920;
    lv_coord_t screen_height_ = 1080;
    lv_coord_t design_width_ = 1920;
    lv_coord_t design_height_ = 1080;
};

}  // namespace ktv::ui

#endif  // KTVLV_UI_UI_SCALE_H


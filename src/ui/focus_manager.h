#ifndef KTVLV_UI_FOCUS_MANAGER_H
#define KTVLV_UI_FOCUS_MANAGER_H

#include <lvgl.h>
#include <vector>

namespace ktv::ui {

/**
 * 焦点管理器（遥控器导航）
 * 管理焦点组和焦点路由
 */
class FocusManager {
public:
    static FocusManager& getInstance() {
        static FocusManager instance;
        return instance;
    }

    FocusManager(const FocusManager&) = delete;
    FocusManager& operator=(const FocusManager&) = delete;

    /**
     * 初始化焦点系统
     */
    void initialize();

    /**
     * 创建新的焦点组
     */
    lv_group_t* createGroup();

    /**
     * 添加对象到当前焦点组
     */
    void addToGroup(lv_obj_t* obj);

    /**
     * 设置当前活动的焦点组
     */
    void setActiveGroup(lv_group_t* group);

    /**
     * 获取当前活动的焦点组
     */
    lv_group_t* getActiveGroup() const { return active_group_; }

    /**
     * 手动设置焦点对象（用于方向键导航）
     */
    void setFocus(lv_obj_t* obj);

private:
    FocusManager() = default;
    ~FocusManager() = default;

    std::vector<lv_group_t*> groups_;
    lv_group_t* active_group_ = nullptr;
};

}  // namespace ktv::ui

#endif  // KTVLV_UI_FOCUS_MANAGER_H







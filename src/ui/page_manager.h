#ifndef KTVLV_UI_PAGE_MANAGER_H
#define KTVLV_UI_PAGE_MANAGER_H

#include <lvgl.h>

namespace ktv::ui {

enum class Page {
    Home = 0,
    History,
    Search,
};

// 轻量页面管理（单例，记录内容区域并切换展示）
class PageManager {
public:
    static PageManager& getInstance() {
        static PageManager instance;
        return instance;
    }

    PageManager(const PageManager&) = delete;
    PageManager& operator=(const PageManager&) = delete;

    // 设置内容区域容器
    void setContentArea(lv_obj_t* content_area);

    // 切换页面
    void switchTo(Page page);

private:
    PageManager() = default;
    ~PageManager() = default;

    lv_obj_t* content_area_ = nullptr;
    Page current_ = Page::Home;
};

}  // namespace ktv::ui

#endif  // KTVLV_UI_PAGE_MANAGER_H


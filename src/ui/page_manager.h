#ifndef KTVLV_UI_PAGE_MANAGER_H
#define KTVLV_UI_PAGE_MANAGER_H

#include <lvgl.h>
#include <functional>
#include <unordered_map>

namespace ktv::ui {

enum class Page {
    Home = 0,
    History,
    Search,
};

// 轻量页面管理（单例，记录内容区域并切换展示）
class PageManager {
public:
    using UnmountCallback = std::function<void()>;

    static PageManager& getInstance() {
        static PageManager instance;
        return instance;
    }

    PageManager(const PageManager&) = delete;
    PageManager& operator=(const PageManager&) = delete;

    // 设置内容区域容器
    void setContentArea(lv_obj_t* content_area);

    // 注册页面的 unmount 回调（用于释放资源、清理定时器等）
    void registerUnmountCallback(Page page, UnmountCallback cb);

    // 切换页面
    void switchTo(Page page);

    // 获取当前页面
    Page getCurrentPage() const { return current_; }

private:
    PageManager() = default;
    ~PageManager() = default;

    lv_obj_t* content_area_ = nullptr;
    Page current_ = Page::Home;
    std::unordered_map<Page, UnmountCallback> unmount_callbacks_;
};

}  // namespace ktv::ui

#endif  // KTVLV_UI_PAGE_MANAGER_H


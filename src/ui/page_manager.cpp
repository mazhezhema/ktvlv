#include "page_manager.h"
#include "layouts.h"
#include "focus_manager.h"

namespace ktv::ui {

void PageManager::setContentArea(lv_obj_t* content_area) {
    content_area_ = content_area;
}

void PageManager::registerUnmountCallback(Page page, UnmountCallback cb) {
    unmount_callbacks_[page] = cb;
}

void PageManager::switchTo(Page page) {
    if (!content_area_) return;
    if (page == current_) return;

    // ✅ P0修复：调用旧页面的 unmount 回调，释放资源（定时器、事件监听等）
    // 这是解决资源泄漏和 Ghost UI 的关键步骤
    if (auto it = unmount_callbacks_.find(current_); it != unmount_callbacks_.end()) {
        if (it->second) {
            it->second();
        }
    }

    // ✅ 关键修复：先清空焦点组，避免 group 里还挂着旧对象
    // 这是解决 lv_timer_handler 内部焦点处理冲突的关键步骤
    FocusManager::getInstance().resetActiveGroup();

    // ✅ 核心修复：清理旧子树，避免 UI 叠加和对象悬空
    // 这是解决刷新期崩溃的关键步骤
    lv_obj_clean(content_area_);

    current_ = page;
    switch (page) {
        case Page::Home:
            show_home_tab(content_area_);
            break;
        case Page::History:
            show_history_tab(content_area_);
            break;
        case Page::Search:
            show_search_page(content_area_);
            break;
        default:
            break;
    }

    // ✅ 更新布局（但不立即刷新，让主循环处理）
    // 这样可以避免在切换页面时触发过早的刷新
    lv_obj_update_layout(content_area_);
}

}  // namespace ktv::ui


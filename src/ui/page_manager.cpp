#include "page_manager.h"
#include "layouts.h"

namespace ktv::ui {

void PageManager::setContentArea(lv_obj_t* content_area) {
    content_area_ = content_area;
}

void PageManager::switchTo(Page page) {
    if (!content_area_) return;
    if (page == current_) return;

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


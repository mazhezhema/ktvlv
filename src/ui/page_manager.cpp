#include "page_manager.h"
#include "layouts.h"

namespace ktv::ui {

void PageManager::setContentArea(lv_obj_t* content_area) {
    content_area_ = content_area;
}

void PageManager::switchTo(Page page) {
    if (!content_area_) return;
    if (page == current_) return;

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
}

}  // namespace ktv::ui


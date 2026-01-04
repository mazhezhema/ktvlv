#include "page_lifecycle.h"
#include <syslog.h>

namespace ktv::ui {

PageLifecycle::PageLifecycle(lv_obj_t* parent_container)
    : parent_container_(parent_container)
    , page_(nullptr)
{
}

PageLifecycle::~PageLifecycle() {
    destroy();
}

void PageLifecycle::show() {
    if (!created_) {
        // 创建页面
        if (onCreate_) {
            page_ = onCreate_(parent_container_);
            if (!page_) {
                syslog(LOG_WARNING, "[ktv][ui][error] component=page_lifecycle reason=creation_failed");
                return;
            }
            created_ = true;
        } else {
            // 默认创建空容器
            page_ = lv_obj_create(parent_container_);
            lv_obj_set_size(page_, LV_PCT(100), LV_PCT(100));
            created_ = true;
        }
    }

    if (page_ && !visible_) {
        lv_obj_clear_flag(page_, LV_OBJ_FLAG_HIDDEN);
        visible_ = true;
        
        if (onShow_) {
            onShow_(page_);
        }
    }
}

void PageLifecycle::hide() {
    if (page_ && visible_) {
        lv_obj_add_flag(page_, LV_OBJ_FLAG_HIDDEN);
        visible_ = false;
        
        if (onHide_) {
            onHide_(page_);
        }
    }
}

void PageLifecycle::destroy() {
    if (page_) {
        if (onDestroy_) {
            onDestroy_(page_);
        }
        
        lv_obj_del(page_);
        page_ = nullptr;
        created_ = false;
        visible_ = false;
    }
}

bool PageLifecycle::isVisible() const {
    return visible_ && page_ && !lv_obj_has_flag(page_, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace ktv::ui









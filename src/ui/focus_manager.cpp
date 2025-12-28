#include "focus_manager.h"

namespace ktv::ui {

void FocusManager::initialize() {
    // 创建默认焦点组
    active_group_ = createGroup();
}

lv_group_t* FocusManager::createGroup() {
    lv_group_t* group = lv_group_create();
    groups_.push_back(group);
    return group;
}

void FocusManager::addToGroup(lv_obj_t* obj) {
    if (active_group_) {
        lv_group_add_obj(active_group_, obj);
    }
}

void FocusManager::setActiveGroup(lv_group_t* group) {
    active_group_ = group;
    if (group) {
        lv_indev_set_group(lv_indev_get_next(nullptr), group);
    }
}

void FocusManager::setFocus(lv_obj_t* obj) {
    if (active_group_) {
        lv_group_focus_obj(obj);
    }
}

}  // namespace ktv::ui







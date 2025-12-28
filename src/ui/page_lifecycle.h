#ifndef KTVLV_UI_PAGE_LIFECYCLE_H
#define KTVLV_UI_PAGE_LIFECYCLE_H

#include <lvgl.h>
#include <functional>
#include <vector>

namespace ktv::ui {

/**
 * 页面生命周期管理
 * 统一管理页面的创建、显示、隐藏、销毁
 */
class PageLifecycle {
public:
    using CreateCallback = std::function<lv_obj_t*(lv_obj_t* parent)>;
    using ShowCallback = std::function<void(lv_obj_t* page)>;
    using HideCallback = std::function<void(lv_obj_t* page)>;
    using DestroyCallback = std::function<void(lv_obj_t* page)>;

    PageLifecycle(lv_obj_t* parent_container);
    ~PageLifecycle();

    // 禁止拷贝
    PageLifecycle(const PageLifecycle&) = delete;
    PageLifecycle& operator=(const PageLifecycle&) = delete;

    /**
     * 设置生命周期回调
     */
    void setOnCreate(CreateCallback cb) { onCreate_ = cb; }
    void setOnShow(ShowCallback cb) { onShow_ = cb; }
    void setOnHide(HideCallback cb) { onHide_ = cb; }
    void setOnDestroy(DestroyCallback cb) { onDestroy_ = cb; }

    /**
     * 显示页面
     */
    void show();

    /**
     * 隐藏页面
     */
    void hide();

    /**
     * 销毁页面
     */
    void destroy();

    /**
     * 获取页面对象
     */
    lv_obj_t* getPage() const { return page_; }

    /**
     * 是否可见
     */
    bool isVisible() const;

private:
    lv_obj_t* parent_container_;
    lv_obj_t* page_;
    
    CreateCallback onCreate_;
    ShowCallback onShow_;
    HideCallback onHide_;
    DestroyCallback onDestroy_;
    
    bool created_ = false;
    bool visible_ = false;
};

}  // namespace ktv::ui

#endif  // KTVLV_UI_PAGE_LIFECYCLE_H







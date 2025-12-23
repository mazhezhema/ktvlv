# KTV应用C++架构设计 - 页面管理系统

> **文档版本**：v1.0（智能指针版本）  
> **相关文档**：详见 [项目架构设计总览.md](./项目架构设计总览.md)  
> **编码规范**：详见 [C++编码规范与避坑指南.md](./C++编码规范与避坑指南.md)  
> **⚠️ 重要**：本项目实际采用**预分配内存版本**，详见 [C++架构设计-预分配内存版本.md](./C++架构设计-预分配内存版本.md)

## ⚠️ 版本说明

本文档为**智能指针版本**（参考实现），实际项目应使用**预分配内存版本**：
- **预分配内存版本**：所有页面使用静态存储，零动态分配，详见 `C++架构设计-预分配内存版本.md`
- **智能指针版本**：使用智能指针管理资源（本文档，仅供参考）

## 一、C++设计原则

### 1.1 核心原则

- **RAII（Resource Acquisition Is Initialization）**：资源获取即初始化
- **智能指针**：使用`std::unique_ptr`和`std::shared_ptr`管理资源
- **现代C++特性**：C++11/14/17标准，避免C风格代码
- **异常安全**：保证异常情况下的资源正确释放
- **const正确性**：正确使用const修饰符
- **移动语义**：使用移动构造和移动赋值优化性能
- **命名空间**：合理组织代码命名空间

### 1.2 避坑指南

- ❌ 避免使用裸指针（raw pointer）管理资源
- ❌ 避免手动new/delete，使用智能指针
- ❌ 避免C风格数组，使用std::vector/std::array
- ❌ 避免C风格字符串，使用std::string
- ❌ 避免宏定义，使用constexpr/const
- ❌ 避免全局变量，使用单例模式或依赖注入
- ✅ 使用RAII管理资源
- ✅ 使用const引用传递大对象
- ✅ 使用移动语义避免不必要的拷贝

## 二、核心类设计

### 2.1 页面基类（Page）

```cpp
#ifndef PAGE_H
#define PAGE_H

#include <lvgl.h>
#include <memory>
#include <functional>
#include <string>

namespace ktv {

// 页面类型枚举
enum class PageType {
    HomeTab = 0,
    HistoryTab,
    SongList,
    Search,
    Category,
    Ranking,
    Artist,
    QueueList,
    Tune,
    Settings,
    Count
};

// 页面状态
enum class PageState {
    Uninitialized,
    Hidden,
    Visible,
    Destroyed
};

// 页面参数基类
struct PageParams {
    virtual ~PageParams() = default;
};

/**
 * 页面基类
 * 使用RAII管理LVGL对象生命周期
 */
class Page {
public:
    using ShowCallback = std::function<void(PageParams*)>;
    using HideCallback = std::function<void()>;
    using RefreshCallback = std::function<void()>;
    
    Page(PageType type, bool is_dialog = false);
    virtual ~Page();
    
    // 禁止拷贝，允许移动
    Page(const Page&) = delete;
    Page& operator=(const Page&) = delete;
    Page(Page&&) noexcept = default;
    Page& operator=(Page&&) noexcept = default;
    
    // 生命周期方法
    virtual void onCreate(PageParams* params = nullptr);
    virtual void onShow(PageParams* params = nullptr);
    virtual void onHide();
    virtual void onRefresh();
    virtual void onDestroy();
    
    // 属性访问
    PageType getType() const { return type_; }
    PageState getState() const { return state_; }
    bool isDialog() const { return is_dialog_; }
    lv_obj_t* getContainer() const { return container_; }
    
    // 显示/隐藏
    void show(PageParams* params = nullptr);
    void hide();
    
protected:
    // 子类可重写的创建方法
    virtual lv_obj_t* createContainer(lv_obj_t* parent);
    virtual void setupUI() {}
    virtual void loadData() {}
    
    PageType type_;
    PageState state_;
    bool is_dialog_;
    lv_obj_t* container_;  // LVGL对象，由LVGL管理生命周期
    
    // 回调函数
    ShowCallback on_show_callback_;
    HideCallback on_hide_callback_;
    RefreshCallback on_refresh_callback_;
};

} // namespace ktv

#endif // PAGE_H
```

### 2.2 页面基类实现

```cpp
#include "page.h"
#include <cassert>

namespace ktv {

Page::Page(PageType type, bool is_dialog)
    : type_(type)
    , state_(PageState::Uninitialized)
    , is_dialog_(is_dialog)
    , container_(nullptr)
{
}

Page::~Page() {
    onDestroy();
}

void Page::onCreate(PageParams* params) {
    if (state_ != PageState::Uninitialized) {
        return;
    }
    
    // 获取父容器
    lv_obj_t* parent = is_dialog_ ? lv_scr_act() : 
                      PageManager::getInstance().getContentArea();
    
    // 创建容器
    container_ = createContainer(parent);
    assert(container_ != nullptr);
    
    // 初始隐藏
    lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN);
    
    // 调用子类设置UI
    setupUI();
    
    // 加载数据
    loadData();
    
    state_ = PageState::Hidden;
}

void Page::onShow(PageParams* params) {
    if (state_ == PageState::Destroyed) {
        return;
    }
    
    // 如果未创建，先创建
    if (state_ == PageState::Uninitialized) {
        onCreate(params);
    }
    
    // 显示容器
    lv_obj_clear_flag(container_, LV_OBJ_FLAG_HIDDEN);
    
    // 刷新数据
    loadData();
    
    // 调用回调
    if (on_show_callback_) {
        on_show_callback_(params);
    }
    
    state_ = PageState::Visible;
}

void Page::onHide() {
    if (state_ != PageState::Visible) {
        return;
    }
    
    // 调用回调
    if (on_hide_callback_) {
        on_hide_callback_();
    }
    
    // 隐藏容器
    lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN);
    
    state_ = PageState::Hidden;
}

void Page::onRefresh() {
    if (state_ == PageState::Destroyed || 
        state_ == PageState::Uninitialized) {
        return;
    }
    
    // 刷新数据
    loadData();
    
    // 调用回调
    if (on_refresh_callback_) {
        on_refresh_callback_();
    }
}

void Page::onDestroy() {
    if (state_ == PageState::Destroyed) {
        return;
    }
    
    // 先隐藏
    if (state_ == PageState::Visible) {
        onHide();
    }
    
    // 销毁LVGL对象（LVGL会自动管理）
    if (container_) {
        lv_obj_del(container_);
        container_ = nullptr;
    }
    
    state_ = PageState::Destroyed;
}

void Page::show(PageParams* params) {
    onShow(params);
}

void Page::hide() {
    onHide();
}

lv_obj_t* Page::createContainer(lv_obj_t* parent) {
    lv_obj_t* container = lv_obj_create(parent);
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);
    return container;
}

} // namespace ktv
```

### 2.3 页面管理器（PageManager）单例

```cpp
#ifndef PAGE_MANAGER_H
#define PAGE_MANAGER_H

#include "page.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace ktv {

class BaseLayout;

/**
 * 页面管理器（单例模式）
 * 使用智能指针管理页面生命周期
 * 线程安全（可选）
 */
class PageManager {
public:
    // 获取单例实例
    static PageManager& getInstance() {
        static PageManager instance;
        return instance;
    }
    
    // 禁止拷贝和移动
    PageManager(const PageManager&) = delete;
    PageManager& operator=(const PageManager&) = delete;
    PageManager(PageManager&&) = delete;
    PageManager& operator=(PageManager&&) = delete;
    
    // 初始化
    void initialize();
    
    // 页面切换
    void switchTo(PageType type, std::unique_ptr<PageParams> params = nullptr);
    void back();
    
    // 页面刷新
    void refreshCurrent();
    void refreshPage(PageType type);
    
    // 获取页面（懒加载）
    Page* getPage(PageType type);
    
    // 获取当前页面
    Page* getCurrentPage() const {
        return current_page_;
    }
    
    // 获取内容区容器
    lv_obj_t* getContentArea() const;
    
    // 清理所有页面
    void cleanup();
    
private:
    PageManager() = default;
    ~PageManager() = default;
    
    // 创建页面实例（工厂方法）
    std::unique_ptr<Page> createPage(PageType type);
    
    // 页面实例映射（使用智能指针管理）
    std::unordered_map<PageType, std::unique_ptr<Page>> pages_;
    
    // 当前页面
    Page* current_page_;
    PageType current_page_type_;
    
    // 页面历史栈（用于返回）
    std::vector<PageType> page_history_;
    
    // 主框架
    std::unique_ptr<BaseLayout> base_layout_;
    
    // 线程安全（如果需要）
    mutable std::mutex mutex_;
};

} // namespace ktv

#endif // PAGE_MANAGER_H
```

### 2.4 页面管理器实现

```cpp
#include "page_manager.h"
#include "base_layout.h"
#include "pages/home_tab.h"
#include "pages/history_tab.h"
#include "pages/song_list_page.h"
#include "pages/search_page.h"
// ... 其他页面头文件

namespace ktv {

void PageManager::initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 创建主框架
    base_layout_ = std::make_unique<BaseLayout>();
    
    // 初始化状态
    current_page_ = nullptr;
    current_page_type_ = PageType::Count;
    page_history_.clear();
    
    // 默认显示首页
    switchTo(PageType::HomeTab);
}

void PageManager::switchTo(PageType type, std::unique_ptr<PageParams> params) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 如果切换到当前页面，直接返回
    if (current_page_type_ == type && current_page_ != nullptr) {
        return;
    }
    
    // 隐藏当前页面
    if (current_page_ != nullptr) {
        current_page_->hide();
        
        // 保存到历史栈（非弹窗页面）
        if (!current_page_->isDialog()) {
            page_history_.push_back(current_page_type_);
        }
    }
    
    // 获取目标页面（懒加载）
    Page* target = getPage(type);
    if (target == nullptr) {
        return;
    }
    
    // 显示目标页面
    target->show(params.get());
    
    // 更新当前页面
    current_page_ = target;
    current_page_type_ = type;
}

void PageManager::back() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (page_history_.empty()) {
        return;
    }
    
    // 获取上一个页面
    PageType prev_type = page_history_.back();
    page_history_.pop_back();
    
    // 切换到上一个页面
    switchTo(prev_type);
}

void PageManager::refreshCurrent() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (current_page_ != nullptr) {
        current_page_->onRefresh();
    }
}

void PageManager::refreshPage(PageType type) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = pages_.find(type);
    if (it != pages_.end() && it->second != nullptr) {
        it->second->onRefresh();
    }
}

Page* PageManager::getPage(PageType type) {
    // 如果页面已创建，直接返回
    auto it = pages_.find(type);
    if (it != pages_.end() && it->second != nullptr) {
        return it->second.get();
    }
    
    // 创建页面实例（懒加载）
    auto page = createPage(type);
    if (page == nullptr) {
        return nullptr;
    }
    
    Page* page_ptr = page.get();
    pages_[type] = std::move(page);
    
    return page_ptr;
}

std::unique_ptr<Page> PageManager::createPage(PageType type) {
    switch (type) {
        case PageType::HomeTab:
            return std::make_unique<HomeTab>();
            
        case PageType::HistoryTab:
            return std::make_unique<HistoryTab>();
            
        case PageType::SongList:
            return std::make_unique<SongListPage>();
            
        case PageType::Search:
            return std::make_unique<SearchPage>();
            
        case PageType::QueueList:
            return std::make_unique<QueueListPage>(true);  // 弹窗
            
        case PageType::Tune:
            return std::make_unique<TunePage>(true);  // 弹窗
            
        // ... 其他页面类型
        
        default:
            return nullptr;
    }
}

lv_obj_t* PageManager::getContentArea() const {
    if (base_layout_ != nullptr) {
        return base_layout_->getContentArea();
    }
    return nullptr;
}

void PageManager::cleanup() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 隐藏当前页面
    if (current_page_ != nullptr) {
        current_page_->hide();
        current_page_ = nullptr;
    }
    
    // 清理所有页面
    pages_.clear();
    
    // 清理主框架
    base_layout_.reset();
    
    // 清空历史栈
    page_history_.clear();
}

} // namespace ktv
```

## 三、具体页面实现示例

### 3.1 首页Tab（HomeTab）

```cpp
#ifndef HOME_TAB_H
#define HOME_TAB_H

#include "page.h"
#include "models/song_item.h"
#include <vector>
#include <memory>

namespace ktv {

// 首页Tab参数
struct HomeTabParams : public PageParams {
    int category_id = 0;
    std::string category_name;
};

/**
 * 首页Tab页面
 */
class HomeTab : public Page {
public:
    HomeTab();
    ~HomeTab() override = default;
    
protected:
    void setupUI() override;
    void loadData() override;
    
private:
    // UI组件
    lv_obj_t* category_bar_;
    lv_obj_t* search_box_;
    lv_obj_t* song_list_;
    
    // 数据
    std::vector<SongItem> songs_;
    
    // 辅助方法
    void createCategoryBar();
    void createSearchBox();
    void createSongList();
    void refreshSongList();
    
    // 事件回调
    static void onCategoryClicked(lv_event_t* e);
    static void onSearchClicked(lv_event_t* e);
    static void onSongItemClicked(lv_event_t* e);
};

} // namespace ktv

#endif // HOME_TAB_H
```

```cpp
#include "home_tab.h"
#include "page_manager.h"
#include "ui_components/song_list_item.h"
#include "ui_components/category_bar.h"
#include <algorithm>

namespace ktv {

HomeTab::HomeTab()
    : Page(PageType::HomeTab, false)
    , category_bar_(nullptr)
    , search_box_(nullptr)
    , song_list_(nullptr)
{
}

void HomeTab::setupUI() {
    createCategoryBar();
    createSearchBox();
    createSongList();
}

void HomeTab::createCategoryBar() {
    category_bar_ = CategoryBar::create(container_);
    lv_obj_align(category_bar_, LV_ALIGN_TOP_MID, 0, 0);
    
    // 添加事件回调
    CategoryBar::setOnCategoryClicked(category_bar_, [](int category_id) {
        HomeTabParams params;
        params.category_id = category_id;
        PageManager::getInstance().switchTo(
            PageType::Category, 
            std::make_unique<HomeTabParams>(params)
        );
    });
}

void HomeTab::createSearchBox() {
    search_box_ = lv_textarea_create(container_);
    lv_obj_set_size(search_box_, LV_PCT(90), 40);
    lv_obj_align(search_box_, LV_ALIGN_TOP_MID, 0, 60);
    lv_textarea_set_placeholder_text(search_box_, "Q 输入首字母搜索");
    
    // 添加事件回调
    lv_obj_add_event_cb(search_box_, [](lv_event_t* e) {
        if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
            PageManager::getInstance().switchTo(PageType::Search);
        }
    }, LV_EVENT_CLICKED, nullptr);
}

void HomeTab::createSongList() {
    song_list_ = lv_list_create(container_);
    lv_obj_set_size(song_list_, LV_PCT(100), LV_PCT(100) - 150);
    lv_obj_align(song_list_, LV_ALIGN_BOTTOM_MID, 0, -80);
}

void HomeTab::loadData() {
    // 从数据源加载歌曲列表
    // songs_ = dataService->getSongs();
    
    refreshSongList();
}

void HomeTab::refreshSongList() {
    // 清空列表
    lv_obj_clean(song_list_);
    
    // 创建列表项
    for (const auto& song : songs_) {
        lv_obj_t* item = SongListItem::create(song_list_, song);
        lv_obj_add_event_cb(item, onSongItemClicked, 
                           LV_EVENT_CLICKED, nullptr);
    }
}

void HomeTab::onSongItemClicked(lv_event_t* e) {
    lv_obj_t* item = lv_event_get_target(e);
    SongItem* song = static_cast<SongItem*>(
        lv_obj_get_user_data(item)
    );
    
    if (song != nullptr) {
        // 添加到播放队列
        // playerService->addToQueue(*song);
        
        // 开始播放
        // playerService->play(*song);
    }
}

} // namespace ktv
```

### 3.2 历史记录Tab（HistoryTab）

```cpp
#ifndef HISTORY_TAB_H
#define HISTORY_TAB_H

#include "page.h"
#include "models/history_item.h"
#include <vector>

namespace ktv {

/**
 * 历史记录Tab页面
 */
class HistoryTab : public Page {
public:
    HistoryTab();
    ~HistoryTab() override = default;
    
protected:
    void setupUI() override;
    void loadData() override;
    
private:
    lv_obj_t* history_list_;
    std::vector<HistoryItem> history_items_;
    
    void refreshHistoryList();
    static void onHistoryItemClicked(lv_event_t* e);
};

} // namespace ktv

#endif // HISTORY_TAB_H
```

```cpp
#include "history_tab.h"
#include "history_service.h"
#include "ui_components/history_list_item.h"
#include <ctime>
#include <iomanip>
#include <sstream>

namespace ktv {

HistoryTab::HistoryTab()
    : Page(PageType::HistoryTab, false)
    , history_list_(nullptr)
{
}

void HistoryTab::setupUI() {
    history_list_ = lv_list_create(container_);
    lv_obj_set_size(history_list_, LV_PCT(100), LV_PCT(100));
}

void HistoryTab::loadData() {
    // 从历史记录服务加载数据
    history_items_ = HistoryService::getInstance().getHistoryList(50);
    refreshHistoryList();
}

void HistoryTab::refreshHistoryList() {
    // 清空列表
    lv_obj_clean(history_list_);
    
    // 按时间倒序排序
    std::sort(history_items_.begin(), history_items_.end(),
        [](const HistoryItem& a, const HistoryItem& b) {
            return a.play_time > b.play_time;
        }
    );
    
    // 创建列表项
    for (const auto& item : history_items_) {
        lv_obj_t* list_item = HistoryListItem::create(history_list_, item);
        lv_obj_add_event_cb(list_item, onHistoryItemClicked, 
                           LV_EVENT_CLICKED, nullptr);
    }
}

void HistoryTab::onHistoryItemClicked(lv_event_t* e) {
    lv_obj_t* item = lv_event_get_target(e);
    HistoryItem* history = static_cast<HistoryItem*>(
        lv_obj_get_user_data(item)
    );
    
    if (history != nullptr) {
        // 播放历史记录中的歌曲
        // playerService->play(history->m3u8_url);
    }
}

} // namespace ktv
```

## 四、数据模型设计

### 4.1 歌曲项（SongItem）

```cpp
#ifndef SONG_ITEM_H
#define SONG_ITEM_H

#include <string>
#include <cstdint>

namespace ktv {

/**
 * 歌曲数据模型
 */
struct SongItem {
    std::string song_id;
    std::string song_name;
    std::string artist;
    std::string m3u8_url;
    bool is_hd = false;
    int duration = 0;  // 秒
    
    // 默认构造函数
    SongItem() = default;
    
    // 移动构造函数
    SongItem(SongItem&&) noexcept = default;
    SongItem& operator=(SongItem&&) noexcept = default;
    
    // 拷贝构造函数（如果需要）
    SongItem(const SongItem&) = default;
    SongItem& operator=(const SongItem&) = default;
};

} // namespace ktv

#endif // SONG_ITEM_H
```

### 4.2 历史记录项（HistoryItem）

```cpp
#ifndef HISTORY_ITEM_H
#define HISTORY_ITEM_H

#include <string>
#include <ctime>

namespace ktv {

/**
 * 历史记录数据模型
 */
struct HistoryItem {
    std::string song_id;
    std::string song_name;
    std::string artist;
    std::string m3u8_url;
    std::time_t play_time;
    int play_count = 1;
    
    HistoryItem() = default;
    HistoryItem(HistoryItem&&) noexcept = default;
    HistoryItem& operator=(HistoryItem&&) noexcept = default;
};

} // namespace ktv

#endif // HISTORY_ITEM_H
```

## 五、服务层设计

### 5.1 历史记录服务（HistoryService）

```cpp
#ifndef HISTORY_SERVICE_H
#define HISTORY_SERVICE_H

#include "models/history_item.h"
#include <vector>
#include <memory>
#include <mutex>
#include <deque>

namespace ktv {

/**
 * 历史记录服务（单例）
 * 使用FIFO队列管理历史记录（最多50条）
 */
class HistoryService {
public:
    static HistoryService& getInstance() {
        static HistoryService instance;
        return instance;
    }
    
    HistoryService(const HistoryService&) = delete;
    HistoryService& operator=(const HistoryService&) = delete;
    
    // 添加播放记录
    void addRecord(const std::string& song_id,
                   const std::string& song_name,
                   const std::string& artist,
                   const std::string& m3u8_url);
    
    // 获取历史记录列表
    std::vector<HistoryItem> getHistoryList(int max_count = 50) const;
    
    // 清空历史记录
    void clear();
    
    // 持久化
    void saveToFile(const std::string& filepath);
    void loadFromFile(const std::string& filepath);
    
private:
    HistoryService() = default;
    ~HistoryService() = default;
    
    static constexpr int MAX_HISTORY_COUNT = 50;
    
    // 使用deque实现FIFO队列
    std::deque<HistoryItem> history_queue_;
    mutable std::mutex mutex_;
};

} // namespace ktv

#endif // HISTORY_SERVICE_H
```

```cpp
#include "history_service.h"
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>  // 使用nlohmann/json库

namespace ktv {

void HistoryService::addRecord(const std::string& song_id,
                                const std::string& song_name,
                                const std::string& artist,
                                const std::string& m3u8_url) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 检查是否已存在（去重）
    auto it = std::find_if(history_queue_.begin(), history_queue_.end(),
        [&song_id](const HistoryItem& item) {
            return item.song_id == song_id;
        }
    );
    
    if (it != history_queue_.end()) {
        // 已存在，更新时间和次数，移动到头部
        it->play_time = std::time(nullptr);
        it->play_count++;
        
        // 移动到队列头部
        HistoryItem item = std::move(*it);
        history_queue_.erase(it);
        history_queue_.push_front(std::move(item));
    } else {
        // 新记录
        HistoryItem item;
        item.song_id = song_id;
        item.song_name = song_name;
        item.artist = artist;
        item.m3u8_url = m3u8_url;
        item.play_time = std::time(nullptr);
        item.play_count = 1;
        
        // FIFO：如果队列已满，删除最旧的
        if (history_queue_.size() >= MAX_HISTORY_COUNT) {
            history_queue_.pop_back();
        }
        
        history_queue_.push_front(std::move(item));
    }
    
    // 自动保存
    saveToFile("/data/ktv_history.json");
}

std::vector<HistoryItem> HistoryService::getHistoryList(int max_count) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<HistoryItem> result;
    result.reserve(std::min(static_cast<size_t>(max_count), 
                           history_queue_.size()));
    
    auto end_it = history_queue_.begin() + 
                  std::min(static_cast<size_t>(max_count), 
                          history_queue_.size());
    
    std::copy(history_queue_.begin(), end_it, std::back_inserter(result));
    
    return result;
}

void HistoryService::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    history_queue_.clear();
}

void HistoryService::saveToFile(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json j;
    j["count"] = history_queue_.size();
    j["items"] = nlohmann::json::array();
    
    for (const auto& item : history_queue_) {
        nlohmann::json item_json;
        item_json["song_id"] = item.song_id;
        item_json["song_name"] = item.song_name;
        item_json["artist"] = item.artist;
        item_json["m3u8_url"] = item.m3u8_url;
        item_json["play_time"] = item.play_time;
        item_json["play_count"] = item.play_count;
        j["items"].push_back(item_json);
    }
    
    std::ofstream file(filepath);
    if (file.is_open()) {
        file << j.dump(2);
        file.close();
    }
}

void HistoryService::loadFromFile(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return;
    }
    
    try {
        nlohmann::json j;
        file >> j;
        
        history_queue_.clear();
        
        if (j.contains("items") && j["items"].is_array()) {
            for (const auto& item_json : j["items"]) {
                HistoryItem item;
                item.song_id = item_json["song_id"].get<std::string>();
                item.song_name = item_json["song_name"].get<std::string>();
                item.artist = item_json["artist"].get<std::string>();
                item.m3u8_url = item_json["m3u8_url"].get<std::string>();
                item.play_time = item_json["play_time"].get<std::time_t>();
                item.play_count = item_json.value("play_count", 1);
                
                history_queue_.push_back(std::move(item));
            }
        }
    } catch (const std::exception& e) {
        // 日志记录错误
    }
}

} // namespace ktv
```

## 六、使用示例

### 6.1 应用初始化

```cpp
#include "page_manager.h"
#include "history_service.h"
#include <lvgl.h>

int main() {
    // 初始化LVGL
    lv_init();
    
    // 加载历史记录
    HistoryService::getInstance().loadFromFile("/data/ktv_history.json");
    
    // 初始化页面管理器
    PageManager::getInstance().initialize();
    
    // 主循环
    while (1) {
        lv_timer_handler();
        usleep(5000);
    }
    
    // 清理
    PageManager::getInstance().cleanup();
    HistoryService::getInstance().saveToFile("/data/ktv_history.json");
    
    return 0;
}
```

### 6.2 页面切换

```cpp
// 切换到搜索页面
void onSearchClicked() {
    PageManager::getInstance().switchTo(PageType::Search);
}

// 切换到分类页面（带参数）
void onCategoryClicked(int category_id) {
    auto params = std::make_unique<HomeTabParams>();
    params->category_id = category_id;
    params->category_name = "流行";
    PageManager::getInstance().switchTo(PageType::Category, 
                                       std::move(params));
}

// 返回上一页
void onBackClicked() {
    PageManager::getInstance().back();
}

// 播放歌曲后刷新历史记录
void onSongPlayed(const SongItem& song) {
    HistoryService::getInstance().addRecord(
        song.song_id,
        song.song_name,
        song.artist,
        song.m3u8_url
    );
    
    // 刷新历史记录Tab
    PageManager::getInstance().refreshPage(PageType::HistoryTab);
}
```

## 七、C++最佳实践总结

### 7.1 资源管理
- ✅ 使用`std::unique_ptr`管理独占资源
- ✅ 使用`std::shared_ptr`管理共享资源
- ✅ 使用RAII原则，构造函数获取资源，析构函数释放资源
- ✅ 避免手动new/delete

### 7.2 内存安全
- ✅ 使用`std::vector`、`std::string`等STL容器
- ✅ 使用移动语义避免不必要的拷贝
- ✅ 使用const引用传递大对象
- ✅ 避免悬空指针和内存泄漏

### 7.3 代码组织
- ✅ 使用命名空间组织代码
- ✅ 使用类封装相关功能
- ✅ 使用const正确性
- ✅ 使用现代C++特性（auto、lambda、范围for等）

### 7.4 异常安全
- ✅ 使用RAII保证异常安全
- ✅ 使用智能指针自动管理资源
- ✅ 避免在析构函数中抛出异常

---

**总结**：采用现代C++设计，使用智能指针、RAII、移动语义等特性，确保代码安全、高效、易维护。


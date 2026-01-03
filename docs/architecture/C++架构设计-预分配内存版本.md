# KTV应用C++架构设计 - 预分配内存版本

> **文档版本**：v2.1（预分配内存版本 + 开源库优先）  
> **相关文档**：详见 [项目架构设计总览.md](./项目架构设计总览.md)  
> **编码规范**：详见 [C++编码规范与避坑指南.md](./C++编码规范与避坑指南.md)  
> **开源库选型**：详见 [开源库选型指南.md](./开源库选型指南.md)

## ⚠️ 重要说明

**本项目采用预分配内存模式 + 简化设计 + 开源库优先**，所有内存在编译期或启动时预分配，**禁止动态内存分配**。

### 核心设计理念

- ✅ **避免造轮子**：严格遵循避免造轮子原则，优先使用成熟开源库
- ✅ **低代码实现**：通过库简化实现，减少代码量
- ✅ **适合嵌入式**：选择轻量级、资源占用小的库
- ✅ **预分配内存友好**：库支持固定大小缓冲区
- ✅ **单线程友好**：库支持单线程模式或无锁设计

### 核心约束

- ✅ **零动态分配**：禁止使用`new`/`delete`、`malloc`/`free`
- ✅ **预分配内存**：所有内存在编译期或启动时预分配
- ✅ **固定大小数组**：使用`std::array`或C风格数组
- ✅ **固定大小字符串**：使用`char[]`或`std::array<char, N>`
- ✅ **单例页面**：所有页面实例预分配，使用静态存储
- ✅ **栈内存优先**：优先使用栈内存，避免堆分配

### 简化设计原则（降低复杂度，减少资源泄露）

- ✅ **尽量少线程**：主线程 + 最小化后台线程（如播放器回调线程）
- ✅ **尽量少状态机**：避免复杂状态机，使用简单状态枚举
- ✅ **尽量无锁**：单线程设计，避免锁竞争
- ✅ **尽量无状态结构**：函数式风格，最小化状态
- ✅ **单例化**：页面和控件尽量单例化
- ✅ **简化设计**：降低整体复杂度，减少资源泄露风险

---

## 一、C++设计原则（预分配版本 + 简化设计）

### 1.1 核心原则

- **零动态分配**：禁止使用`new`/`delete`、`malloc`/`free`
- **预分配内存**：所有内存在编译期或启动时预分配
- **固定大小数组**：使用`std::array`或C风格数组替代`std::vector`
- **固定大小字符串**：使用`char[]`或`std::array<char, N>`替代`std::string`
- **单例页面**：所有页面实例预分配，使用静态存储
- **栈内存优先**：优先使用栈内存，避免堆分配

### 1.2 简化设计原则

- **尽量少线程**：主线程处理UI和业务逻辑，最小化后台线程
- **尽量少状态机**：避免复杂状态机，使用简单状态枚举
- **尽量无锁**：单线程设计，避免锁竞争和死锁风险
- **尽量无状态结构**：函数式风格，最小化状态，减少状态管理复杂度
- **单例化**：页面和控件尽量单例化，避免重复创建
- **简化设计**：降低整体复杂度，减少资源泄露风险
- **避免造轮子**：严格遵循避免造轮子原则，优先使用开源库（libcurl、cJSON、inih等）
- **消息队列**：使用标准库 `std::queue + std::mutex + condition_variable`，不使用无锁队列（moodycamel 系列）
- **低代码**：通过库简化实现，减少代码量

### 1.3 避坑指南

- ❌ **禁止**：`new` / `delete`
- ❌ **禁止**：`malloc` / `free`
- ❌ **禁止**：`std::make_unique` / `std::make_shared`
- ❌ **禁止**：`std::vector`（动态扩容）
- ❌ **禁止**：`std::string`（动态分配）
- ❌ **禁止**：`std::deque`（动态分配）
- ❌ **禁止**：`std::unordered_map`（动态分配）
- ❌ **禁止**：`std::mutex` / `std::lock_guard`（无锁设计）
- ❌ **禁止**：复杂状态机（使用简单状态枚举）
- ❌ **禁止**：多线程竞争（单线程设计）
- ✅ **推荐**：`std::array<T, N>` - 固定大小数组
- ✅ **推荐**：`char buffer[SIZE]` - 固定大小字符数组
- ✅ **推荐**：静态实例 - 页面对象使用静态存储
- ✅ **推荐**：栈内存 - 局部变量使用栈
- ✅ **推荐**：单线程 - 主线程处理所有逻辑
- ✅ **推荐**：简单状态 - 使用枚举而非状态机
- ✅ **推荐**：无状态函数 - 函数式风格

---

## 二、核心类设计（预分配版本）

### 2.1 页面基类（Page）

```cpp
#ifndef PAGE_H
#define PAGE_H

#include <lvgl.h>
#include <array>
#include <cstdint>

namespace ktv {

// 页面类型枚举
enum class PageType : uint8_t {
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
enum class PageState : uint8_t {
    Uninitialized,
    Hidden,
    Visible,
    Destroyed
};

// 页面参数（使用固定大小缓冲区）
struct PageParams {
    int32_t param1 = 0;
    int32_t param2 = 0;
    char buffer[256] = {0};  // 固定大小字符串缓冲区
};

/**
 * 页面基类（预分配版本）
 * 所有页面实例使用静态存储，零动态分配
 */
class Page {
public:
    Page(PageType type, bool is_dialog = false);
    virtual ~Page() = default;  // 禁止动态分配，析构函数不释放内存
    
    // 禁止拷贝和移动（单例模式）
    Page(const Page&) = delete;
    Page& operator=(const Page&) = delete;
    Page(Page&&) = delete;
    Page& operator=(Page&&) = delete;
    
    // 生命周期方法
    virtual void onCreate(const PageParams* params = nullptr);
    virtual void onShow(const PageParams* params = nullptr);
    virtual void onHide();
    virtual void onRefresh();
    virtual void onDestroy();
    
    // 属性访问
    PageType getType() const { return type_; }
    PageState getState() const { return state_; }
    bool isDialog() const { return is_dialog_; }
    lv_obj_t* getContainer() const { return container_; }
    
    // 显示/隐藏
    void show(const PageParams* params = nullptr);
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
    
private:
    // 禁止动态分配
    void* operator new(size_t) = delete;
    void* operator new[](size_t) = delete;
    void operator delete(void*) = delete;
    void operator delete[](void*) = delete;
};

} // namespace ktv

#endif // PAGE_H
```

### 2.2 页面管理器（PageManager）单例（预分配版本）

```cpp
#ifndef PAGE_MANAGER_H
#define PAGE_MANAGER_H

#include "page.h"
#include <array>
#include <cstdint>

namespace ktv {

class BaseLayout;

// 前向声明所有页面类型
class HomeTab;
class HistoryTab;
class SongListPage;
class SearchPage;
class CategoryPage;
class RankingPage;
class ArtistPage;
class QueueListPage;
class TunePage;
class SettingsPage;

/**
 * 页面管理器（单例模式，预分配版本，无锁设计）
 * 所有页面实例预分配为静态存储
 * 单线程设计，无需锁保护
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
    
    // 初始化（预分配所有页面）
    void initialize();
    
    // 页面切换（无锁，单线程调用）
    void switchTo(PageType type, const PageParams* params = nullptr);
    void back();
    
    // 页面刷新（无锁，单线程调用）
    void refreshCurrent();
    void refreshPage(PageType type);
    
    // 获取页面（返回预分配的实例）
    Page* getPage(PageType type);
    
    // 获取当前页面
    Page* getCurrentPage() const {
        return current_page_;
    }
    
    // 获取内容区容器
    lv_obj_t* getContentArea() const;
    
    // 清理（只清理状态，不释放内存）
    void cleanup();
    
private:
    PageManager() = default;
    ~PageManager() = default;
    
    // 页面实例数组（预分配，静态存储）
    std::array<Page*, static_cast<size_t>(PageType::Count)> pages_;
    
    // 当前页面
    Page* current_page_;
    PageType current_page_type_;
    
    // 页面历史栈（用于返回，固定大小）
    static constexpr size_t MAX_HISTORY_DEPTH = 10;
    std::array<PageType, MAX_HISTORY_DEPTH> page_history_;
    size_t history_top_;
    
    // 主框架（静态实例）
    BaseLayout* base_layout_;
    
    // 注意：无mutex，单线程设计，无需锁保护
    
    // 禁止动态分配
    void* operator new(size_t) = delete;
    void* operator new[](size_t) = delete;
    void operator delete(void*) = delete;
    void operator delete[](void*) = delete;
};

} // namespace ktv

#endif // PAGE_MANAGER_H
```

### 2.3 页面管理器实现（预分配版本）

```cpp
#include "page_manager.h"
#include "base_layout.h"
#include "pages/home_tab.h"
#include "pages/history_tab.h"
#include "pages/song_list_page.h"
#include "pages/search_page.h"
#include "pages/category_page.h"
#include "pages/ranking_page.h"
#include "pages/artist_page.h"
#include "pages/queue_list_page.h"
#include "pages/tune_page.h"
#include "pages/settings_page.h"

namespace ktv {

// 所有页面实例（静态存储，预分配）
namespace {
    // 静态页面实例（预分配）
    HomeTab g_home_tab;
    HistoryTab g_history_tab;
    SongListPage g_song_list_page;
    SearchPage g_search_page;
    CategoryPage g_category_page;
    RankingPage g_ranking_page;
    ArtistPage g_artist_page;
    QueueListPage g_queue_list_page;
    TunePage g_tune_page;
    SettingsPage g_settings_page;
}

void PageManager::initialize() {
    // 初始化页面数组（指向预分配的静态实例）
    pages_[static_cast<size_t>(PageType::HomeTab)] = &g_home_tab;
    pages_[static_cast<size_t>(PageType::HistoryTab)] = &g_history_tab;
    pages_[static_cast<size_t>(PageType::SongList)] = &g_song_list_page;
    pages_[static_cast<size_t>(PageType::Search)] = &g_search_page;
    pages_[static_cast<size_t>(PageType::Category)] = &g_category_page;
    pages_[static_cast<size_t>(PageType::Ranking)] = &g_ranking_page;
    pages_[static_cast<size_t>(PageType::Artist)] = &g_artist_page;
    pages_[static_cast<size_t>(PageType::QueueList)] = &g_queue_list_page;
    pages_[static_cast<size_t>(PageType::Tune)] = &g_tune_page;
    pages_[static_cast<size_t>(PageType::Settings)] = &g_settings_page;
    
    // 初始化主框架（静态实例）
    static BaseLayout g_base_layout;
    base_layout_ = &g_base_layout;
    base_layout_->initialize();
    
    // 初始化状态
    current_page_ = nullptr;
    current_page_type_ = PageType::Count;
    history_top_ = 0;
    
    // 默认显示首页
    switchTo(PageType::HomeTab);
}

void PageManager::switchTo(PageType type, const PageParams* params) {
    // 单线程设计，无需锁保护
    
    // 如果切换到当前页面，直接返回
    if (current_page_type_ == type && current_page_ != nullptr) {
        return;
    }
    
    // 隐藏当前页面
    if (current_page_ != nullptr) {
        current_page_->hide();
        
        // 保存到历史栈（非弹窗页面，且栈未满）
        if (!current_page_->isDialog() && history_top_ < MAX_HISTORY_DEPTH) {
            page_history_[history_top_] = current_page_type_;
            history_top_++;
        }
    }
    
    // 获取目标页面（预分配的静态实例）
    Page* target = getPage(type);
    if (target == nullptr) {
        return;
    }
    
    // 显示目标页面
    target->show(params);
    
    // 更新当前页面（简单状态更新，非状态机）
    current_page_ = target;
    current_page_type_ = type;
}

void PageManager::back() {
    if (history_top_ == 0) {
        return;
    }
    
    // 获取上一个页面
    history_top_--;
    PageType prev_type = page_history_[history_top_];
    
    // 切换到上一个页面
    switchTo(prev_type);
}

void PageManager::refreshCurrent() {
    if (current_page_ != nullptr) {
        current_page_->onRefresh();
    }
}

void PageManager::refreshPage(PageType type) {
    Page* page = getPage(type);
    if (page != nullptr) {
        page->onRefresh();
    }
}

Page* PageManager::getPage(PageType type) {
    size_t index = static_cast<size_t>(type);
    if (index < pages_.size()) {
        return pages_[index];
    }
    return nullptr;
}

lv_obj_t* PageManager::getContentArea() const {
    if (base_layout_ != nullptr) {
        return base_layout_->getContentArea();
    }
    return nullptr;
}

void PageManager::cleanup() {
    // 隐藏当前页面
    if (current_page_ != nullptr) {
        current_page_->hide();
        current_page_ = nullptr;
    }
    
    // 清理所有页面状态（不释放内存）
    for (Page* page : pages_) {
        if (page != nullptr) {
            page->onDestroy();
        }
    }
    
    // 清空历史栈
    history_top_ = 0;
}

} // namespace ktv
```

---

## 三、具体页面实现示例（预分配版本）

### 3.1 首页Tab（HomeTab）

```cpp
#ifndef HOME_TAB_H
#define HOME_TAB_H

#include "page.h"
#include "models/song_item.h"
#include <array>

namespace ktv {

/**
 * 首页Tab页面（预分配版本）
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
    
    // 数据（固定大小数组）
    static constexpr size_t MAX_SONGS = 1000;
    std::array<SongItem, MAX_SONGS> songs_;
    size_t song_count_;
    
    // 辅助方法
    void createCategoryBar();
    void createSearchBox();
    void createSongList();
    void refreshSongList();
    
    // 事件回调
    static void onCategoryClicked(lv_event_t* e);
    static void onSearchClicked(lv_event_t* e);
    static void onSongItemClicked(lv_event_t* e);
    
    // 禁止动态分配
    void* operator new(size_t) = delete;
    void* operator new[](size_t) = delete;
    void operator delete(void*) = delete;
    void operator delete[](void*) = delete;
};

} // namespace ktv

#endif // HOME_TAB_H
```

```cpp
#include "home_tab.h"
#include "song_service.h"  // 歌曲数据服务（通过HTTP REST API获取）
#include "page_manager.h"
#include "ui_components/song_list_item.h"
#include "ui_components/category_bar.h"

namespace ktv {

HomeTab::HomeTab()
    : Page(PageType::HomeTab, false)
    , category_bar_(nullptr)
    , search_box_(nullptr)
    , song_list_(nullptr)
    , song_count_(0)
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
        PageParams params;
        params.param1 = category_id;
        PageManager::getInstance().switchTo(PageType::Category, &params);
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
    // 从服务器获取歌曲列表（使用HTTP REST API）
    song_count_ = SongService::getInstance().getSongList(
        songs_.data(), songs_.size(), 1, 20, 0  // 第1页，每页20条
    );
    
    refreshSongList();
}

void HomeTab::refreshSongList() {
    // 清空列表
    lv_obj_clean(song_list_);
    
    // 创建列表项
    for (size_t i = 0; i < song_count_; ++i) {
        lv_obj_t* item = SongListItem::create(song_list_, songs_[i]);
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
        // 点歌（通过REST API）
        if (SongService::getInstance().addToQueue(song->getId())) {
            // 成功，开始播放
            // playerService->play(*song);
            
            // 添加到历史记录
            HistoryService::getInstance().addRecord(
                song->getId(),
                song->getName(),
                song->getArtist(),
                song->getUrl()
            );
        }
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
#include <array>

namespace ktv {

/**
 * 历史记录Tab页面（预分配版本）
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
    
    // 历史记录数据（固定大小数组，最多50条）
    static constexpr size_t MAX_HISTORY = 50;
    std::array<HistoryItem, MAX_HISTORY> history_items_;
    size_t history_count_;
    
    void refreshHistoryList();
    static void onHistoryItemClicked(lv_event_t* e);
    
    // 禁止动态分配
    void* operator new(size_t) = delete;
    void* operator new[](size_t) = delete;
    void operator delete(void*) = delete;
    void operator delete[](void*) = delete;
};

} // namespace ktv

#endif // HISTORY_TAB_H
```

---

## 四、数据模型设计（预分配版本）

### 4.1 歌曲项（SongItem）

```cpp
#ifndef SONG_ITEM_H
#define SONG_ITEM_H

#include <array>
#include <cstdint>

namespace ktv {

/**
 * 歌曲数据模型（预分配版本）
 * 使用固定大小字符数组
 */
struct SongItem {
    // 固定大小字符串缓冲区
    static constexpr size_t MAX_ID_LEN = 64;
    static constexpr size_t MAX_NAME_LEN = 128;
    static constexpr size_t MAX_ARTIST_LEN = 64;
    static constexpr size_t MAX_URL_LEN = 512;
    
    std::array<char, MAX_ID_LEN> song_id = {0};
    std::array<char, MAX_NAME_LEN> song_name = {0};
    std::array<char, MAX_ARTIST_LEN> artist = {0};
    std::array<char, MAX_URL_LEN> m3u8_url = {0};
    
    bool is_hd = false;
    int32_t duration = 0;  // 秒
    
    // 辅助方法
    const char* getId() const { return song_id.data(); }
    const char* getName() const { return song_name.data(); }
    const char* getArtist() const { return artist.data(); }
    const char* getUrl() const { return m3u8_url.data(); }
    
    void setId(const char* id) {
        strncpy(song_id.data(), id, MAX_ID_LEN - 1);
        song_id[MAX_ID_LEN - 1] = '\0';
    }
    
    void setName(const char* name) {
        strncpy(song_name.data(), name, MAX_NAME_LEN - 1);
        song_name[MAX_NAME_LEN - 1] = '\0';
    }
    
    void setArtist(const char* artist_name) {
        strncpy(artist.data(), artist_name, MAX_ARTIST_LEN - 1);
        artist[MAX_ARTIST_LEN - 1] = '\0';
    }
    
    void setUrl(const char* url) {
        strncpy(m3u8_url.data(), url, MAX_URL_LEN - 1);
        m3u8_url[MAX_URL_LEN - 1] = '\0';
    }
    
    void setLocalPath(const char* path) {
        strncpy(local_path.data(), path, MAX_LOCAL_PATH_LEN - 1);
        local_path[MAX_LOCAL_PATH_LEN - 1] = '\0';
    }
};

} // namespace ktv

#endif // SONG_ITEM_H
```

### 4.2 历史记录项（HistoryItem）

```cpp
#ifndef HISTORY_ITEM_H
#define HISTORY_ITEM_H

#include <array>
#include <ctime>
#include <cstdint>

namespace ktv {

/**
 * 历史记录数据模型（预分配版本）
 */
struct HistoryItem {
    // 固定大小字符串缓冲区
    static constexpr size_t MAX_ID_LEN = 64;
    static constexpr size_t MAX_NAME_LEN = 128;
    static constexpr size_t MAX_ARTIST_LEN = 64;
    static constexpr size_t MAX_URL_LEN = 512;
    
    std::array<char, MAX_ID_LEN> song_id = {0};
    std::array<char, MAX_NAME_LEN> song_name = {0};
    std::array<char, MAX_ARTIST_LEN> artist = {0};
    std::array<char, MAX_URL_LEN> m3u8_url = {0};
    
    // 本地文件路径（m3u8下载后存储的hash目录）
    static constexpr size_t MAX_LOCAL_PATH_LEN = 256;
    std::array<char, MAX_LOCAL_PATH_LEN> local_path = {0};
    
    std::time_t play_time = 0;
    int32_t play_count = 1;
    
    // 辅助方法（同SongItem）
    const char* getId() const { return song_id.data(); }
    const char* getName() const { return song_name.data(); }
    const char* getArtist() const { return artist.data(); }
    const char* getUrl() const { return m3u8_url.data(); }
    
    void setId(const char* id) {
        strncpy(song_id.data(), id, MAX_ID_LEN - 1);
        song_id[MAX_ID_LEN - 1] = '\0';
    }
    
    // ... 其他setter方法类似
};

} // namespace ktv

#endif // HISTORY_ITEM_H
```

---

## 五、服务层设计（预分配版本）

### 5.1 历史记录服务（HistoryService）

```cpp
#ifndef HISTORY_SERVICE_H
#define HISTORY_SERVICE_H

#include "models/history_item.h"
#include "cJSON.h"  // 开源库：JSON解析（详见：开源库选型指南.md）
#include <array>
#include <cstdint>

namespace ktv {

/**
 * 历史记录服务（单例，预分配版本，无锁设计）
 * 使用固定大小数组实现FIFO队列（最多50条）
 * 单线程设计，无需锁保护
 */
class HistoryService {
public:
    static HistoryService& getInstance() {
        static HistoryService instance;
        return instance;
    }
    
    HistoryService(const HistoryService&) = delete;
    HistoryService& operator=(const HistoryService&) = delete;
    
    // 添加播放记录（无锁，单线程调用）
    void addRecord(const char* song_id,
                   const char* song_name,
                   const char* artist,
                   const char* m3u8_url,
                   const char* local_path = nullptr);
    
    // 更新本地文件路径
    void updateLocalPath(const char* song_id, const char* local_path);
    
    // 删除最早的历史记录（包括本地文件）
    void removeOldestRecord();
    
    // 获取历史记录列表（返回固定大小数组，无锁，只读）
    size_t getHistoryList(HistoryItem* out_items, size_t max_count) const;
    
    // 清空历史记录（无锁，单线程调用）
    void clear();
    
    // 持久化（使用固定大小缓冲区，无锁）
    void saveToFile(const char* filepath);
    void loadFromFile(const char* filepath);
    
private:
    HistoryService() = default;
    ~HistoryService() = default;
    
    static constexpr size_t MAX_HISTORY_COUNT = 50;
    
    // 使用固定大小数组实现FIFO队列
    std::array<HistoryItem, MAX_HISTORY_COUNT> history_queue_;
    size_t queue_size_;
    size_t queue_head_;  // 队列头部索引
    
    // 注意：无mutex，单线程设计，无需锁保护
    
    // 禁止动态分配
    void* operator new(size_t) = delete;
    void* operator new[](size_t) = delete;
    void operator delete(void*) = delete;
    void operator delete[](void*) = delete;
};

} // namespace ktv

#endif // HISTORY_SERVICE_H
```

```cpp
#include "history_service.h"
#include <cstring>
#include <fstream>
#include <cstdio>

namespace ktv {

void HistoryService::addRecord(const char* song_id,
                                const char* song_name,
                                const char* artist,
                                const char* m3u8_url,
                                const char* local_path) {
    // 单线程设计，无需锁保护
    
    // 检查是否已存在（去重）
    size_t found_index = MAX_HISTORY_COUNT;
    for (size_t i = 0; i < queue_size_; ++i) {
        size_t idx = (queue_head_ + i) % MAX_HISTORY_COUNT;
        if (strcmp(history_queue_[idx].song_id.data(), song_id) == 0) {
            found_index = idx;
            break;
        }
    }
    
    if (found_index < MAX_HISTORY_COUNT) {
        // 已存在，更新时间和次数（简单状态更新）
        history_queue_[found_index].play_time = std::time(nullptr);
        history_queue_[found_index].play_count++;
        if (local_path) {
            history_queue_[found_index].setLocalPath(local_path);
        }
        
        // 移动到队列头部（简化：不移动，只更新时间）
    } else {
        // 新记录：如果队列已满，删除最早的记录（包括文件）
        if (queue_size_ >= MAX_HISTORY_COUNT) {
            removeOldestRecord();
        }
        
        // 添加到队列
        size_t new_idx = (queue_head_ + queue_size_) % MAX_HISTORY_COUNT;
        HistoryItem& item = history_queue_[new_idx];
        item.setId(song_id);
        item.setName(song_name);
        item.setArtist(artist);
        item.setUrl(m3u8_url);
        if (local_path) {
            item.setLocalPath(local_path);
        }
        item.play_time = std::time(nullptr);
        item.play_count = 1;
        
        queue_size_++;
    }
    
    // 自动保存（无锁，单线程）
    saveToFile("/data/ktv_history.json");
}

void HistoryService::updateLocalPath(const char* song_id, const char* local_path) {
    // 查找记录并更新本地路径
    for (size_t i = 0; i < queue_size_; ++i) {
        size_t idx = (queue_head_ + i) % MAX_HISTORY_COUNT;
        if (strcmp(history_queue_[idx].song_id.data(), song_id) == 0) {
            history_queue_[idx].setLocalPath(local_path);
            saveToFile("/data/ktv_history.json");
            return;
        }
    }
}

void HistoryService::removeOldestRecord() {
    if (queue_size_ == 0) {
        return;
    }
    
    // 获取最早的记录
    HistoryItem& oldest = history_queue_[queue_head_];
    
    // 删除本地文件（如果存在）
    if (oldest.getLocalPath()[0] != '\0') {
        // 删除整个hash目录
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "rm -rf %s", oldest.getLocalPath());
        system(cmd);
    }
    
    // 从队列中移除（FIFO）
    queue_head_ = (queue_head_ + 1) % MAX_HISTORY_COUNT;
    queue_size_--;
}

size_t HistoryService::getHistoryList(HistoryItem* out_items, size_t max_count) const {
    size_t count = (queue_size_ < max_count) ? queue_size_ : max_count;
    
    for (size_t i = 0; i < count; ++i) {
        size_t idx = (queue_head_ + i) % MAX_HISTORY_COUNT;
        out_items[i] = history_queue_[idx];
    }
    
    return count;
}

void HistoryService::updateLocalPath(const char* song_id, const char* local_path) {
    // 查找记录并更新本地路径
    for (size_t i = 0; i < queue_size_; ++i) {
        size_t idx = (queue_head_ + i) % MAX_HISTORY_COUNT;
        if (strcmp(history_queue_[idx].song_id.data(), song_id) == 0) {
            history_queue_[idx].setLocalPath(local_path);
            saveToFile("/data/ktv_history.json");
            return;
        }
    }
}

void HistoryService::removeOldestRecord() {
    if (queue_size_ == 0) {
        return;
    }
    
    // 获取最早的记录
    HistoryItem& oldest = history_queue_[queue_head_];
    
    // 删除本地文件（如果存在）
    if (oldest.getLocalPath()[0] != '\0') {
        // 删除整个hash目录
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "rm -rf %s", oldest.getLocalPath());
        system(cmd);
    }
    
    // 从队列中移除（FIFO）
    queue_head_ = (queue_head_ + 1) % MAX_HISTORY_COUNT;
    queue_size_--;
}

void HistoryService::clear() {
    // 删除所有本地文件
    for (size_t i = 0; i < queue_size_; ++i) {
        size_t idx = (queue_head_ + i) % MAX_HISTORY_COUNT;
        if (history_queue_[idx].getLocalPath()[0] != '\0') {
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "rm -rf %s", history_queue_[idx].getLocalPath());
            system(cmd);
        }
    }
    
    queue_size_ = 0;
    queue_head_ = 0;
}

void HistoryService::saveToFile(const char* filepath) {
    // 使用cJSON库（开源库，低代码实现）
    // 详见：开源库选型指南.md
    // 注意：需要在文件顶部包含 #include "cJSON.h"
    
    cJSON* json = cJSON_CreateObject();
    cJSON* items = cJSON_CreateArray();
    
    for (size_t i = 0; i < queue_size_; ++i) {
        size_t idx = (queue_head_ + i) % MAX_HISTORY_COUNT;
        const HistoryItem& item = history_queue_[idx];
        
        cJSON* json_item = cJSON_CreateObject();
        cJSON_AddStringToObject(json_item, "song_id", item.getId());
        cJSON_AddStringToObject(json_item, "song_name", item.getName());
        cJSON_AddStringToObject(json_item, "artist", item.getArtist());
        cJSON_AddStringToObject(json_item, "m3u8_url", item.getUrl());
        cJSON_AddNumberToObject(json_item, "play_time", item.play_time);
        cJSON_AddNumberToObject(json_item, "play_count", item.play_count);
        
        cJSON_AddItemToArray(items, json_item);
    }
    
    cJSON_AddItemToObject(json, "items", items);
    cJSON_AddNumberToObject(json, "count", queue_size_);
    
    char* json_string = cJSON_Print(json);
    FILE* file = fopen(filepath, "w");
    if (file) {
        fwrite(json_string, 1, strlen(json_string), file);
        fclose(file);
    }
    
    free(json_string);  // cJSON_Print分配的内存（库内部，可接受）
    cJSON_Delete(json);
}

void HistoryService::loadFromFile(const char* filepath) {
    // 使用cJSON库（开源库，低代码实现）
    // 详见：开源库选型指南.md
    // 注意：需要在文件顶部包含 #include "cJSON.h"
    
    char buffer[8192] = {0};  // 预分配缓冲区
    FILE* file = fopen(filepath, "r");
    if (!file) return;
    
    size_t len = fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);
    
    cJSON* json = cJSON_ParseWithLength(buffer, len);
    if (!json) return;
    
    cJSON* items = cJSON_GetObjectItem(json, "items");
    if (cJSON_IsArray(items)) {
        queue_size_ = 0;
        int array_size = cJSON_GetArraySize(items);
        
        for (int i = 0; i < array_size && i < MAX_HISTORY_COUNT; ++i) {
            cJSON* item = cJSON_GetArrayItem(items, i);
            HistoryItem& history_item = history_queue_[i];
            
            cJSON* song_id = cJSON_GetObjectItem(item, "song_id");
            cJSON* song_name = cJSON_GetObjectItem(item, "song_name");
            cJSON* artist = cJSON_GetObjectItem(item, "artist");
            cJSON* m3u8_url = cJSON_GetObjectItem(item, "m3u8_url");
            cJSON* local_path = cJSON_GetObjectItem(item, "local_path");
            cJSON* play_time = cJSON_GetObjectItem(item, "play_time");
            cJSON* play_count = cJSON_GetObjectItem(item, "play_count");
            
            if (song_id) history_item.setId(cJSON_GetStringValue(song_id));
            if (song_name) history_item.setName(cJSON_GetStringValue(song_name));
            if (artist) history_item.setArtist(cJSON_GetStringValue(artist));
            if (m3u8_url) history_item.setUrl(cJSON_GetStringValue(m3u8_url));
            if (local_path) history_item.setLocalPath(cJSON_GetStringValue(local_path));
            if (play_time) history_item.play_time = cJSON_GetNumberValue(play_time);
            if (play_count) history_item.play_count = cJSON_GetNumberValue(play_count);
            
            queue_size_++;
        }
    }
    
    cJSON_Delete(json);
}

} // namespace ktv
```

---

## 六、使用示例（预分配版本）

### 6.1 应用初始化

```cpp
#include "page_manager.h"
#include "history_service.h"
#include "http_service.h"  // HTTP REST API客户端
#include "song_service.h"  // 歌曲数据服务
#include "m3u8_download_service.h"  // M3u8下载服务
#include <lvgl.h>

int main() {
    // 初始化LVGL
    lv_init();
    
    // 初始化HTTP服务（libcurl）
    HttpService::getInstance().initialize();
    HttpService::getInstance().setBaseUrl("http://api.ktv.example.com");
    HttpService::getInstance().setTimeout(10);
    
    // 初始化M3u8下载服务
    M3u8DownloadService::getInstance().initialize();
    
    // 加载历史记录（使用固定大小缓冲区）
    HistoryService::getInstance().loadFromFile("/data/ktv_history.json");
    
    // 初始化页面管理器（预分配所有页面）
    PageManager::getInstance().initialize();
    
    // 主循环
    while (1) {
        lv_timer_handler();
        
        // 处理M3u8下载（后台任务）
        M3u8DownloadService::getInstance().processDownload();
        
        usleep(5000);
    }
    
    // 清理（只清理状态，不释放内存）
    PageManager::getInstance().cleanup();
    HistoryService::getInstance().saveToFile("/data/ktv_history.json");
    M3u8DownloadService::getInstance().cleanup();
    HttpService::getInstance().cleanup();
    
    return 0;
}
```

### 6.2 页面切换（零动态分配）

```cpp
// 切换到搜索页面（使用预分配的页面实例）
void onSearchClicked() {
    PageManager::getInstance().switchTo(PageType::Search);
}

// 切换到分类页面（带参数，使用栈内存）
void onCategoryClicked(int category_id) {
    PageParams params;  // 栈分配
    params.param1 = category_id;
    PageManager::getInstance().switchTo(PageType::Category, &params);
}

// 返回上一页
void onBackClicked() {
    PageManager::getInstance().back();
}

// 播放歌曲后刷新历史记录
void onSongPlayed(const SongItem& song) {
    // 1. 直接播放m3u8（不等待下载）
    PlayerService::getInstance().play(song);
    
    // 2. 后台开始下载m3u8和ts文件（异步）
    M3u8DownloadService::getInstance().startDownload(
        song.getId(),
        song.getUrl()
    );
    
    // 3. 添加到历史记录（先不包含本地路径，下载完成后更新）
    HistoryService::getInstance().addRecord(
        song.getId(),
        song.getName(),
        song.getArtist(),
        song.getUrl()
    );
    
    // 刷新历史记录Tab
    PageManager::getInstance().refreshPage(PageType::HistoryTab);
}

// M3u8下载完成后的处理
void onM3u8DownloadCompleted(const char* song_id) {
    char local_path[256];
    if (M3u8DownloadService::getInstance().getLocalPath(song_id, local_path, sizeof(local_path))) {
        // 更新历史记录，添加本地文件路径
        HistoryService::getInstance().updateLocalPath(song_id, local_path);
    }
}

// 从服务器获取歌曲列表（使用HTTP REST API）
void loadSongsFromServer() {
    std::array<SongItem, 100> songs;
    size_t count = SongService::getInstance().getSongList(
        songs.data(), songs.size(), 1, 20, 0  // 第1页，每页20条
    );
    
    // 更新UI显示
    // ...
}

// 搜索歌曲（使用HTTP REST API）
void searchSongs(const char* keyword) {
    std::array<SongItem, 100> songs;
    size_t count = SongService::getInstance().searchSongs(
        songs.data(), songs.size(), keyword, "song"
    );
    
    // 显示搜索结果
    // ...
}

// 从服务器获取歌曲列表（使用HTTP REST API）
void loadSongsFromServer() {
    std::array<SongItem, 100> songs;
    size_t count = SongService::getInstance().getSongList(
        songs.data(), songs.size(), 1, 20, 0  // 第1页，每页20条
    );
    
    // 更新UI显示
    // ...
}

// 搜索歌曲（使用HTTP REST API）
void searchSongs(const char* keyword) {
    std::array<SongItem, 100> songs;
    size_t count = SongService::getInstance().searchSongs(
        songs.data(), songs.size(), keyword, "song"
    );
    
    // 显示搜索结果
    // ...
}
```

---

## 七、内存布局说明

### 7.1 静态存储布局

```
静态数据段（.data / .bss）
├── g_home_tab (HomeTab实例)
├── g_history_tab (HistoryTab实例)
├── g_song_list_page (SongListPage实例)
├── ... (其他页面实例)
├── g_base_layout (BaseLayout实例)
└── PageManager::instance (PageManager单例)

所有页面实例在程序启动时预分配，生命周期与程序相同
```

### 7.2 栈内存使用

```
函数调用栈
├── 局部变量（PageParams等小对象）
├── 函数参数
└── 返回地址

优先使用栈内存，避免堆分配
```

---

## 八、网络层设计（HTTP REST API）

### 9.1 服务器协议

**服务器端已实现**：HTTP REST API + JSON
- 协议：HTTP REST API
- 数据格式：JSON
- 客户端实现：使用libcurl + cJSON

### 9.2 HTTP客户端服务

**HttpService（单例，libcurl）**
- 发送HTTP GET/POST请求
- 使用固定大小缓冲区接收响应
- 单线程设计，无锁

### 9.3 歌曲数据服务

**SongService（单例）**
- 通过HTTP REST API获取歌曲数据
- 使用cJSON解析JSON响应
- 提供统一的API接口

**详见**：[HTTP_REST_API客户端设计.md](./HTTP_REST_API客户端设计.md)

---

## 十、总结

### 10.1 预分配内存优势

- ✅ **零内存泄漏**：所有内存预分配，无需释放
- ✅ **确定性内存使用**：内存使用量在编译期确定
- ✅ **快速启动**：无需动态分配，启动速度快
- ✅ **适合嵌入式**：适合资源受限的嵌入式系统

### 8.2 注意事项

- ⚠️ **内存限制**：需要预先估算最大内存需求
- ⚠️ **灵活性降低**：无法动态调整内存大小
- ⚠️ **字符串长度**：需要设置合理的最大长度限制

---

**总结**：采用预分配内存模式 + 简化设计（单线程、无锁、简单状态、无状态结构、单例化）+ **严格遵循避免造轮子原则**（优先使用开源库：libcurl、cJSON、inih等，低代码实现）+ **消息队列使用标准库**（`std::queue + std::mutex + condition_variable`，不使用无锁队列），所有页面使用静态存储，零动态内存分配，降低整体复杂度，减少资源泄露风险，适合嵌入式系统。

**相关文档**：
- [HTTP_REST_API客户端设计.md](./HTTP_REST_API客户端设计.md) ⭐ **网络层设计**（libcurl + cJSON）
- [输入设备与交互设计.md](./输入设备与交互设计.md) ⭐ **输入设备设计**（触摸屏+遥控器）
- [Licence许可证管理系统设计.md](./Licence许可证管理系统设计.md) ⭐ **Licence管理设计**
- [M3u8下载与本地存储设计.md](./M3u8下载与本地存储设计.md) ⭐ **M3u8下载与存储设计**
- [线程与事件系统设计.md](./线程与事件系统设计.md) ⚠️ **参考文档（旧版）** - 线程与事件设计（内容已过时）
- [开源库选型指南.md](./开源库选型指南.md) - 推荐使用的开源库清单和集成方案
- [C++编码规范与避坑指南.md](./C++编码规范与避坑指南.md) - 编码规范和最佳实践


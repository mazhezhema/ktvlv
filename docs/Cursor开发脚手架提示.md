# Cursor 开发脚手架提示

> **文档版本**：v1.0  
> **最后更新**：2025-12-30  
> **状态**：✅ 核心文档  
> **用途**：复制给 Cursor，生成符合规范的代码

---

## 🎯 核心架构约束（必须遵守）

```
We are implementing a KTVLV project (F133/Tina Linux) with the following architecture:

**Resource Management Principles:**
- ALL resources are Singleton (no new/delete/free)
- UI controls created once, never deleted (use show()/hide())
- Lifecycle = App lifecycle (no manual release)
- No malloc/free/new/delete in business code

**Architecture:**
- 4 threads: UI (main), PlayerThread (std::thread), Network (std::async), SDK internal thread
- 2 message queues: PlayerCmdQueue (UI/Network -> PlayerThread), UiEventQueue (PlayerThread/Network -> UI)
- Service layer: PlayerService, HttpService, WebSocketService, CacheService, etc. (all Singleton)
- Business layer: features/ (Search, Charts, Playlist, etc.) - Java/Web style development

**Rules:**
- Command Down / Event Up
- UI never calls tplayer directly (use PlayerService)
- tplayer callbacks push events to UiEventQueue, then UiDispatcher::post() to UI thread
- std::queue + mutex inside services, no lock exposed to business layer
- No moodycamel, no boost, no raw pthread for business
- No direct cross-thread widget updates
- No new/delete/free/lv_obj_del in business code
- All Pages are Singleton (created once, use show()/hide())
- All Services are Singleton (created once, lifecycle = App lifecycle)

**Forbidden:**
- ❌ new/delete/free/lv_obj_del in business code
- ❌ Creating controls in loops (use control pool)
- ❌ Creating pages on each navigation (use Singleton + show()/hide())
- ❌ Direct tplayer_* calls (use PlayerService)
- ❌ curl_easy_* in business code (use HttpService)
- ❌ pthread_create in business code (use Service threads)

**Patterns:**
- Singleton pattern for all Services and Pages
- Control pool for lists (pre-create fixed number of items)
- show()/hide() for page navigation
- update() for data refresh (don't recreate controls)
```

---

## 📝 代码生成模板

### 1. Service 模板

```cpp
// ServiceName.h
#pragma once

class ServiceName {
public:
    static ServiceName& instance();
    
    // 初始化（只调用一次）
    void init();
    
    // 业务接口
    void doSomething();
    
private:
    ServiceName() = default;
    ~ServiceName() = default;
    ServiceName(const ServiceName&) = delete;
    ServiceName& operator=(const ServiceName&) = delete;
    
    bool m_initialized = false;
};

// ServiceName.cpp
ServiceName& ServiceName::instance() {
    static ServiceName inst;
    return inst;
}

void ServiceName::init() {
    if (m_initialized) return;
    // 初始化逻辑（只执行一次）
    m_initialized = true;
}
```

### 2. Page 模板

```cpp
// PageName.h
#pragma once
#include "ui/BasePage.h"

class PageName : public BasePage {
public:
    static PageName& instance();
    
    void show() override;
    void hide() override;
    void update(const DataType& data);
    
private:
    PageName();  // 私有构造函数
    
    void buildUI();  // 只调用一次
    
    lv_obj_t* m_label = nullptr;
    // ... 其他控件
};

// PageName.cpp
PageName::PageName() {
    root = lv_obj_create(lv_scr_act());
    buildUI();
    lv_obj_add_flag(root, LV_OBJ_FLAG_HIDDEN);
}

PageName& PageName::instance() {
    static PageName inst;
    return inst;
}

void PageName::buildUI() {
    // 创建所有控件（只调用一次）
    m_label = lv_label_create(root);
    // ... 其他控件
}

void PageName::show() {
    lv_obj_clear_flag(root, LV_OBJ_FLAG_HIDDEN);
}

void PageName::hide() {
    lv_obj_add_flag(root, LV_OBJ_FLAG_HIDDEN);
}

void PageName::update(const DataType& data) {
    // 更新已有控件，不创建新控件
    lv_label_set_text(m_label, data.text.c_str());
}
```

### 3. List View 模板（控件池）

```cpp
// ListView.h
class ListView {
private:
    static constexpr int POOL_SIZE = 50;
    lv_obj_t* m_itemPool[POOL_SIZE] = {nullptr};
    lv_obj_t* m_root = nullptr;
    
public:
    ListView(lv_obj_t* parent);
    
    void updateList(const std::vector<ItemType>& items);
    
private:
    void buildItemPool();
    void setItemContent(lv_obj_t* item, const ItemType& data);
};

// ListView.cpp
ListView::ListView(lv_obj_t* parent) {
    m_root = lv_obj_create(parent);
    buildItemPool();
}

void ListView::buildItemPool() {
    // 预创建固定数量项
    for(int i = 0; i < POOL_SIZE; i++) {
        m_itemPool[i] = lv_list_add_btn(m_root, NULL, "");
        lv_obj_add_flag(m_itemPool[i], LV_OBJ_FLAG_HIDDEN);
    }
}

void ListView::updateList(const std::vector<ItemType>& items) {
    for(int i = 0; i < POOL_SIZE; i++) {
        if(i < items.size()) {
            setItemContent(m_itemPool[i], items[i]);
            lv_obj_clear_flag(m_itemPool[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(m_itemPool[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}
```

### 4. Controller 模板

```cpp
// ControllerName.h
#pragma once
#include "services/HttpService.h"
#include "services/PlayerService.h"
#include "services/UiEventBus.h"
#include "models/DataModel.h"

class ControllerName {
public:
    static ControllerName& instance();
    
    void onEvent(const EventType& event);
    void onAction(const ActionType& action);
    
private:
    ControllerName() = default;
    
    void handleData(const DataType& data);
};
```

---

## 🔍 代码审查提示

生成代码后，检查以下项：

- [ ] 是否使用 Singleton 模式？
- [ ] 是否有 `new`/`delete`/`free`/`lv_obj_del`？
- [ ] 控件是否在循环内创建？
- [ ] 页面切换是否使用 `show()/hide()`？
- [ ] 是否使用 Service 层接口？
- [ ] 事件是否通过 `UiEventBus`？

---

## 📚 相关文档

- **资源管理规范**：[资源管理规范v1.md](./资源管理规范v1.md)
- **团队开发规范**：[团队开发规范v1.md](./团队开发规范v1.md)
- **服务层API设计**：[服务层API设计文档.md](./服务层API设计文档.md)

---

**最后更新**: 2025-12-30  
**状态**: ✅ 核心文档，Cursor开发脚手架提示


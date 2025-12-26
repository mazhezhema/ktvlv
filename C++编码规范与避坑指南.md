# KTV项目C++编码规范与避坑指南

> **文档版本**：v2.0（预分配内存版本 + 开源库优先）  
> **相关文档**：详见 [项目架构设计总览.md](./项目架构设计总览.md)  
> **架构设计**：详见 [C++架构设计-预分配内存版本.md](./C++架构设计-预分配内存版本.md)  
> **开源库选型**：详见 [开源库选型指南.md](./开源库选型指南.md)

## ⚠️ 重要说明

**本项目采用预分配内存模式 + 简化设计 + 开源库优先**，所有内存在编译期或启动时预分配，**禁止动态内存分配**。

### 简化设计原则

- ✅ **尽量少线程**：主线程处理UI和业务逻辑，最小化后台线程
- ✅ **尽量少状态机**：避免复杂状态机，使用简单状态枚举
- ✅ **尽量无锁**：单线程设计，避免锁竞争和死锁风险
- ✅ **尽量无状态结构**：函数式风格，最小化状态，减少状态管理复杂度
- ✅ **单例化**：页面和控件尽量单例化，避免重复创建
- ✅ **简化设计**：降低整体复杂度，减少资源泄露风险
- ✅ **避免造轮子**：严格遵循避免造轮子原则，优先使用开源库（libcurl、cJSON、moodycamel::ConcurrentQueue、inih等），低代码实现

### 开源库使用

- ✅ **避免造轮子**：严格遵循避免造轮子原则，优先使用成熟开源库
- ✅ **适合嵌入式平台**：选择轻量级、资源占用小的库
- ✅ **低代码实现**：通过库简化实现，减少代码量
- ✅ **详见**：[开源库选型指南.md](./开源库选型指南.md)

### 代码复用原则

- ✅ **先检查现有模块**：在实现新功能前，先检查是否已有可复用的模块/函数
- ✅ **优先使用已有服务**：如 `PageManager`、`HttpService`、`SongService`、`PlayerService`、`QueueService` 等
- ✅ **复用公共函数**：如 `create_title_bar`、`create_page_indicator`、`on_back_to_main` 等
- ⚠️ **不过度抽象**：不要为了追求复用而增加复杂度，简单直接的复用优于复杂抽象的复用
- ⚠️ **避免过度设计**：**如果复用后反而增加了复杂度，就属于过度设计，必须避免**
- ✅ **详见**：[编码原则.md](./编码原则.md)

## 一、核心原则（预分配内存版本 + 简化设计）

### 1.1 现代C++标准（预分配版本）
- **使用C++11/14/17标准**，避免C风格代码
- **优先使用固定大小数组**：`std::array<T, N>`
- **禁止动态内存分配**：`new`/`delete`、`malloc`/`free`
- **使用静态存储**：页面实例使用静态存储
- **使用栈内存**：优先使用栈内存

### 1.2 禁止事项（避坑）

| ❌ 禁止 | ✅ 推荐 |
|---------|--------|
| `new`/`delete` | 静态实例或栈分配 |
| `malloc`/`free` | 静态实例或栈分配 |
| `std::make_unique`/`std::make_shared` | 静态实例 |
| `std::vector`（动态扩容） | `std::array<T, N>`（固定大小） |
| `std::string`（动态分配） | `char buffer[SIZE]` 或 `std::array<char, N>` |
| `std::deque`（动态分配） | `std::array<T, N>`（固定大小） |
| `std::unordered_map`（动态分配） | `std::array<T, N>`（固定大小） |
| 宏定义 `#define MAX 100` | `constexpr int MAX = 100` |
| 全局变量（可变） | 静态实例（单例模式） |

## 二、资源管理

### 2.1 预分配内存使用（本项目要求）

```cpp
// ❌ 错误：动态分配
void bad_example() {
    Page* page = new Page(PageType::HomeTab);  // 禁止！
    // ... 使用page
    delete page;  // 禁止！
}

// ❌ 错误：智能指针（仍然有动态分配）
void bad_example2() {
    auto page = std::make_unique<Page>(PageType::HomeTab);  // 禁止！
    // 虽然自动释放，但仍然有动态分配
}

// ✅ 正确：静态实例（预分配）
namespace {
    HomeTab g_home_tab;  // 静态存储，预分配
}

void good_example() {
    Page* page = &g_home_tab;  // 使用预分配的实例
    // ... 使用page
    // 无需释放，静态存储
}

// ✅ 正确：栈分配（小对象）
void good_example2() {
    PageParams params;  // 栈分配
    params.param1 = 1;
    // 函数返回时自动释放
}
```

### 2.2 RAII原则

```cpp
// ✅ 资源获取即初始化
class FileHandler {
public:
    FileHandler(const std::string& filename) 
        : file_(std::fopen(filename.c_str(), "r")) {
        if (!file_) {
            throw std::runtime_error("Failed to open file");
        }
    }
    
    ~FileHandler() {
        if (file_) {
            std::fclose(file_);
        }
    }
    
    // 禁止拷贝，允许移动
    FileHandler(const FileHandler&) = delete;
    FileHandler& operator=(const FileHandler&) = delete;
    FileHandler(FileHandler&&) noexcept = default;
    FileHandler& operator=(FileHandler&&) noexcept = default;
    
private:
    FILE* file_;
};
```

## 三、内存管理

### 3.1 避免内存泄漏（预分配版本）

```cpp
// ❌ 错误：动态分配，可能泄漏
void leak_example() {
    Page* page = new Page(PageType::HomeTab);  // 禁止！
    if (some_condition()) {
        return;  // 泄漏！page没有被delete
    }
    delete page;
}

// ✅ 正确：使用预分配的静态实例
namespace {
    HomeTab g_home_tab;  // 静态存储，预分配
}

void safe_example() {
    Page* page = &g_home_tab;  // 使用预分配的实例
    if (some_condition()) {
        return;  // 安全！无需释放
    }
    // 无需释放，静态存储
}
```

### 3.2 避免悬空指针（预分配版本）

```cpp
// ❌ 错误：悬空指针
Page* getPage() {
    Page page(PageType::HomeTab);
    return &page;  // 返回局部变量的地址，危险！
}

// ✅ 正确：返回静态实例的指针
namespace {
    HomeTab g_home_tab;  // 静态存储
}

Page* getPage() {
    return &g_home_tab;  // 返回静态实例的指针，安全！
}

// ✅ 或者返回引用（如果对象生命周期由调用者管理）
Page& getPage(PageManager& mgr) {
    return *mgr.getPage(PageType::HomeTab);  // 返回预分配实例的引用
}
```

## 四、字符串处理

### 4.1 使用固定大小字符串（预分配版本）

```cpp
// ❌ 错误：C风格字符串（动态分配）
void bad_string() {
    char* name = (char*)malloc(100);  // 禁止！
    strcpy(name, "Hello");
    // ... 使用name
    free(name);  // 容易忘记
}

// ❌ 错误：std::string（动态分配）
void bad_string2() {
    std::string name = "Hello";  // 禁止！动态分配
}

// ✅ 正确：固定大小字符数组
void good_string() {
    char name[256] = {0};  // 栈分配，固定大小
    strncpy(name, "Hello", sizeof(name) - 1);
    // ... 使用name
    // 函数返回时自动释放
}

// ✅ 正确：std::array<char, N>
void good_string2() {
    std::array<char, 256> name = {0};  // 栈分配，固定大小
    strncpy(name.data(), "Hello", name.size() - 1);
}

// ✅ 字符串拼接（使用固定大小缓冲区）
void concat_strings(const char* str1, const char* str2) {
    char result[512] = {0};  // 固定大小缓冲区
    snprintf(result, sizeof(result), "%s - %s", str1, str2);
}
```

## 五、容器使用

### 5.1 使用固定大小数组（预分配版本）

```cpp
// ❌ 错误：动态容器
void bad_vector() {
    std::vector<int> arr;  // 禁止！动态分配
    arr.push_back(1);
}

// ❌ 错误：动态map
std::unordered_map<PageType, std::unique_ptr<Page>> pages;  // 禁止！

// ✅ 正确：固定大小数组
void good_array() {
    std::array<int, 100> arr;  // 固定大小，预分配
    arr[0] = 1;
    // 固定大小，安全
}

// ✅ 正确：固定大小数组（历史记录）
std::array<HistoryItem, 50> history_items;  // 预分配50条

// ✅ 正确：固定大小数组（页面实例）
std::array<Page*, 10> pages;  // 预分配10个页面指针

// ✅ 正确：C风格数组（如果大小在编译期确定）
int arr[100];  // 栈分配或静态分配
```

### 5.2 范围for循环

```cpp
// ❌ 错误：传统循环
for (size_t i = 0; i < songs.size(); ++i) {
    process(songs[i]);
}

// ✅ 正确：范围for
for (const auto& song : songs) {
    process(song);
}

// ✅ 需要索引时
for (size_t i = 0; i < songs.size(); ++i) {
    process(songs[i], i);
}

// ✅ 或使用C++20 ranges
#include <ranges>
for (auto [song, index] : songs | std::views::enumerate) {
    process(song, index);
}
```

## 六、移动语义

### 6.1 避免不必要的拷贝

```cpp
// ❌ 错误：不必要的拷贝
void bad_copy(const std::vector<SongItem>& songs) {
    std::vector<SongItem> local_songs = songs;  // 拷贝
    // 使用local_songs
}

// ✅ 正确：使用移动
void good_move(std::vector<SongItem>&& songs) {
    std::vector<SongItem> local_songs = std::move(songs);  // 移动
    // 使用local_songs
}

// ✅ 或者使用const引用（如果不修改）
void good_ref(const std::vector<SongItem>& songs) {
    // 只读访问，不拷贝
    for (const auto& song : songs) {
        process(song);
    }
}
```

### 6.2 移动构造函数

```cpp
class Page {
public:
    // 移动构造函数
    Page(Page&& other) noexcept
        : type_(other.type_)
        , container_(other.container_) {
        other.container_ = nullptr;  // 清空源对象
    }
    
    // 移动赋值运算符
    Page& operator=(Page&& other) noexcept {
        if (this != &other) {
            // 释放当前资源
            cleanup();
            
            // 移动资源
            type_ = other.type_;
            container_ = other.container_;
            
            // 清空源对象
            other.container_ = nullptr;
        }
        return *this;
    }
    
private:
    PageType type_;
    lv_obj_t* container_;
};
```

## 七、const正确性

### 7.1 正确使用const

```cpp
class Page {
public:
    // const成员函数：不修改对象状态
    PageType getType() const { return type_; }
    
    // 非const成员函数：可能修改对象状态
    void show() { /* 修改状态 */ }
    
    // const引用参数：不修改参数
    void process(const SongItem& song) const {
        // 只能调用const成员函数
    }
    
    // 非const引用参数：可能修改参数
    void modify(SongItem& song) {
        song.song_name = "New Name";
    }
};

// ✅ 使用const引用传递大对象
void processSong(const SongItem& song) {
    // 不拷贝，只读访问
}
```

## 八、异常安全

### 8.1 保证异常安全

```cpp
// ✅ 使用RAII保证异常安全
class ResourceManager {
public:
    ResourceManager() {
        resource_ = acquire_resource();
        if (!resource_) {
            throw std::runtime_error("Failed to acquire resource");
        }
    }
    
    ~ResourceManager() {
        if (resource_) {
            release_resource(resource_);
        }
    }
    
    // 即使抛出异常，析构函数也会被调用
private:
    Resource* resource_;
};

// ✅ 使用智能指针
void safe_function() {
    auto resource = std::make_unique<Resource>();
    // 即使抛出异常，也会自动释放
    throw std::exception();
}
```

## 九、命名规范

### 9.1 命名约定

```cpp
// 类名：PascalCase
class PageManager { };
class HomeTab { };

// 函数名：camelCase
void switchToPage() { }
void getCurrentPage() { }

// 变量名：camelCase
int pageCount = 0;
std::string songName;

// 常量：UPPER_SNAKE_CASE
constexpr int MAX_HISTORY_COUNT = 50;
const std::string DEFAULT_PATH = "/data";

// 私有成员：下划线后缀
class Page {
private:
    PageType type_;
    lv_obj_t* container_;
};

// 命名空间：小写
namespace ktv {
    namespace ui {
        // ...
    }
}
```

## 十、代码组织

### 10.1 头文件保护

```cpp
#ifndef PAGE_H
#define PAGE_H

// 头文件内容

#endif // PAGE_H

// ✅ 或使用#pragma once（更简洁）
#pragma once

// 头文件内容
```

### 10.2 前向声明

```cpp
// 头文件中使用前向声明，减少依赖
class PageManager;  // 前向声明

class Page {
    // 只需要指针或引用时，不需要包含完整定义
    PageManager* manager_;
};

// 实现文件中包含完整定义
#include "page_manager.h"
```

### 10.3 包含顺序

```cpp
// 1. 对应的头文件
#include "page.h"

// 2. C标准库
#include <cstdint>
#include <ctime>

// 3. C++标准库
#include <memory>
#include <vector>
#include <string>

// 4. 第三方库
#include <lvgl.h>
#include "cJSON.h"  // 开源库：JSON解析（轻量级，适合嵌入式）

// 5. 项目内其他头文件
#include "page_manager.h"
#include "models/song_item.h"
```

## 十一、常见陷阱

### 11.1 循环引用（智能指针）

```cpp
// ❌ 错误：循环引用导致内存泄漏
class Page {
    std::shared_ptr<PageManager> manager_;  // 持有manager
};

class PageManager {
    std::vector<std::shared_ptr<Page>> pages_;  // 持有pages
    // 循环引用！pages持有manager，manager持有pages
};

// ✅ 正确：使用weak_ptr打破循环
class Page {
    std::weak_ptr<PageManager> manager_;  // 弱引用
};

// ✅ 或者：使用原始指针（如果生命周期由外部管理）
class Page {
    PageManager* manager_;  // 原始指针，不拥有所有权
};
```

### 11.2 单线程设计（无锁）

```cpp
// ❌ 错误：使用mutex（本项目禁止）
class HistoryService {
private:
    mutable std::mutex mutex_;  // 禁止！
    std::deque<HistoryItem> history_queue_;
    
public:
    void addRecord(const HistoryItem& item) {
        std::lock_guard<std::mutex> lock(mutex_);  // 禁止！
        history_queue_.push_back(item);
    }
};

// ✅ 正确：单线程设计，无锁
class HistoryService {
private:
    // 无mutex，单线程设计
    std::array<HistoryItem, 50> history_queue_;  // 固定大小数组
    size_t queue_size_;
    
public:
    void addRecord(const HistoryItem& item) {
        // 单线程调用，无需锁保护
        if (queue_size_ < 50) {
            history_queue_[queue_size_] = item;
            queue_size_++;
        }
    }
    
    size_t getList(HistoryItem* out_items, size_t max_count) const {
        // 只读操作，无需锁保护
        size_t count = (queue_size_ < max_count) ? queue_size_ : max_count;
        for (size_t i = 0; i < count; ++i) {
            out_items[i] = history_queue_[i];
        }
        return count;
    }
};
```

### 11.3 简单状态设计（避免复杂状态机）

```cpp
// ❌ 错误：复杂状态机
class StateMachine {
    enum State { S1, S2, S3, S4, S5, S6, S7, S8 };
    State current_state_;
    void transition(Event event) {
        // 复杂的状态转换逻辑
        if (current_state_ == S1 && event == E1) {
            if (condition1) {
                current_state_ = S2;
            } else if (condition2) {
                current_state_ = S3;
            }
        }
        // ... 更多复杂逻辑
    }
};

// ✅ 正确：简单状态枚举
enum class PageState : uint8_t {
    Uninitialized,
    Hidden,
    Visible,
    Destroyed
};

class Page {
    PageState state_;
    
    void show() {
        // 简单直接的状态转换
        if (state_ == PageState::Hidden) {
            state_ = PageState::Visible;
        }
    }
};
```

### 11.4 避免在析构函数中抛出异常

```cpp
// ❌ 错误：析构函数中抛出异常
~Page() {
    if (some_condition()) {
        throw std::exception();  // 危险！
    }
}

// ✅ 正确：析构函数中捕获异常
~Page() {
    try {
        cleanup();
    } catch (...) {
        // 记录日志，但不抛出
        log_error("Error in destructor");
    }
}
```

## 十二、性能优化

### 12.1 避免不必要的拷贝

```cpp
// ❌ 错误：不必要的拷贝
std::vector<SongItem> getSongs() {
    std::vector<SongItem> songs;
    // ... 填充songs
    return songs;  // 可能拷贝（C++11前）
}

// ✅ 正确：返回值优化（RVO）或移动
std::vector<SongItem> getSongs() {
    std::vector<SongItem> songs;
    // ... 填充songs
    return songs;  // C++11后自动移动
}

// ✅ 或者：使用输出参数（如果调用者已有容器）
void getSongs(std::vector<SongItem>& out_songs) {
    // 直接填充out_songs，避免拷贝
}
```

### 12.2 预分配空间

```cpp
// ✅ 预分配vector空间
std::vector<SongItem> songs;
songs.reserve(1000);  // 预分配，避免多次重分配

// ✅ 使用emplace_back避免临时对象
songs.emplace_back("id", "name", "artist", "url");
// 而不是 songs.push_back(SongItem("id", "name", "artist", "url"));
```

## 十三、总结检查清单

### ✅ 代码审查清单（预分配版本 + 简化设计）

- [ ] 是否禁止使用new/delete？
- [ ] 是否禁止使用malloc/free？
- [ ] 是否禁止使用std::vector/std::string等动态容器？
- [ ] 是否使用std::array替代动态容器？
- [ ] 是否使用固定大小字符数组替代std::string？
- [ ] 是否使用静态实例（单例模式）？
- [ ] 是否优先使用栈内存？
- [ ] 是否禁止使用mutex/lock（无锁设计）？
- [ ] 是否使用单线程设计？
- [ ] 是否避免复杂状态机（使用简单枚举）？
- [ ] 是否使用无状态函数（函数式风格）？
- [ ] 是否使用const正确性？
- [ ] 是否使用命名空间组织代码？
- [ ] 是否遵循命名规范？

---

**记住**：本项目采用预分配内存模式 + 简化设计（单线程、无锁、简单状态、无状态结构、单例化），所有内存在编译期或启动时预分配，禁止动态内存分配，降低整体复杂度，减少资源泄露风险。


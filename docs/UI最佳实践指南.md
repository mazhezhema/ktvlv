# KTV-LVGL UI 最佳实践指南

## 🎯 核心原则

1. **全局缩放，不用硬编码像素**
2. **模块化组件，避免重复代码**
3. **事件驱动，不循环刷新**
4. **焦点管理，支持遥控器导航**

---

## 📐 1. 全局缩放系统

### 使用方式

```cpp
#include "ui/ui_scale.h"

// 在 main() 中初始化（必须在 init_display() 之后）
ktv::ui::init_ui_system(LV_HOR_RES_MAX, LV_VER_RES_MAX);

// 使用缩放
lv_coord_t width = UIScale::s(1920);  // 设计稿1920px → 实际屏幕宽度
lv_coord_t height = UIScale::s(1080);  // 设计稿1080px → 实际屏幕高度
```

### 设计稿基准

- **设计稿尺寸**：1920x1080（1080p）
- **自动适配**：720p / 4K / 其他分辨率
- **缩放范围**：0.5x ~ 2.0x（防止极端情况）

### 注意事项

- ✅ 所有尺寸、边距、字体大小都要用 `UIScale::s()`
- ❌ 不要写死 `300px`、`50px` 这种硬编码
- ✅ 使用 `LV_PCT(%)` 和 `LV_FR()` 做相对布局

---

## 🧩 2. 组件模块化

### 已提供的组件

```cpp
#include "ui/components.h"

// 渐变卡片
lv_obj_t* card = components::createGradientCard(
    parent, 
    0xa855f7,  // 起始颜色
    0xec4899,  // 结束颜色
    UIScale::s(48)  // 圆角
);

// 操作按钮
lv_obj_t* btn = components::createActionButton(
    parent,
    "已点(3)",
    true  // enabled
);

// 歌曲列表项
lv_obj_t* item = components::createSongListItem(
    parent,
    "偏爱",
    "张芸京",
    "song_123"  // song_id
);
```

### 扩展组件

在 `src/ui/components.cpp` 中添加新组件，遵循：
- 使用缩放系统
- 统一样式管理
- 支持焦点导航

---

## 🎮 3. 焦点管理（遥控器）

### 基本使用

```cpp
#include "ui/focus_manager.h"

// 创建焦点组
lv_group_t* group = FocusManager::getInstance().createGroup();

// 添加可聚焦对象
FocusManager::getInstance().addToGroup(btn1);
FocusManager::getInstance().addToGroup(btn2);

// 设置活动组
FocusManager::getInstance().setActiveGroup(group);
```

### 焦点样式

在 `init_ui_theme()` 中已定义 `style_focus`：
- 蓝色边框高亮
- 2px 宽度
- 自动应用到所有按钮

---

## 📄 4. 页面生命周期

### 使用方式

```cpp
#include "ui/page_lifecycle.h"

// 创建页面生命周期管理器
PageLifecycle lifecycle(content_area);

// 设置回调
lifecycle.setOnCreate([](lv_obj_t* parent) -> lv_obj_t* {
    lv_obj_t* page = lv_obj_create(parent);
    // ... 创建UI元素
    return page;
});

lifecycle.setOnShow([](lv_obj_t* page) {
    // 页面显示时刷新数据
    refreshData();
});

lifecycle.setOnHide([](lv_obj_t* page) {
    // 页面隐藏时清理
});

// 显示/隐藏
lifecycle.show();
lifecycle.hide();
lifecycle.destroy();
```

### 生命周期流程

```
创建 → 显示 → [隐藏] → [显示] → ... → 销毁
```

---

## 🎨 5. 布局最佳实践

### ✅ 推荐做法

```cpp
// 使用百分比布局
lv_obj_set_width(container, LV_PCT(100));
lv_obj_set_height(container, LV_PCT(50));

// 使用 Flex 布局
lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW);
lv_obj_set_flex_grow(item, 1);  // 等分

// 使用缩放边距
lv_obj_set_style_pad_all(container, UIScale::s(12), 0);
lv_obj_set_style_pad_column(container, UIScale::s(10), 0);
```

### ❌ 避免做法

```cpp
// ❌ 硬编码像素
lv_obj_set_width(container, 300);
lv_obj_set_height(container, 200);

// ❌ 绝对定位（除非是点缀元素）
lv_obj_align(obj, LV_ALIGN_TOP_LEFT, 120, 233);

// ❌ 嵌套过深（超过4层）
container → row → col → item → label  // 太深了
```

---

## 🚀 6. 性能优化

### 事件驱动

```cpp
// ✅ 正确：事件触发刷新
lv_obj_add_event_cb(btn, [](lv_event_t* e) {
    refreshUI();
}, LV_EVENT_CLICKED, nullptr);

// ❌ 错误：循环刷新
while(1) {
    refreshUI();  // 不要这样做
    lv_timer_handler();
}
```

### 静态UI不刷新

```cpp
// 静态元素创建后不再更新
lv_obj_t* title = lv_label_create(parent);
lv_label_set_text(title, "雷石官方正版");
// 之后不再调用 lv_label_set_text(title, ...)
```

### 批量更新

```cpp
// ✅ 收集数据后统一刷新
std::vector<Song> songs = fetchSongs();
for (const auto& song : songs) {
    createSongItem(list, song);
}

// ❌ 每条数据触发一次刷新
for (const auto& song : songs) {
    createSongItem(list, song);
    lv_obj_invalidate(list);  // 不要这样做
}
```

---

## 📁 7. 文件结构

```
src/ui/
  ├── ui_scale.h/cpp          # 全局缩放系统
  ├── focus_manager.h/cpp     # 焦点管理
  ├── page_lifecycle.h/cpp    # 页面生命周期
  ├── components.h/cpp        # 可复用组件
  ├── layouts.h/cpp           # 页面布局
  └── page_manager.h/cpp      # 页面管理器
```

---

## 🔥 8. 常见问题

### Q: 为什么我的UI在不同分辨率下显示不对？

A: 检查是否所有尺寸都用了 `UIScale::s()`，不要用硬编码像素。

### Q: 遥控器焦点乱跳？

A: 使用 `FocusManager` 手动管理焦点组，不要依赖LVGL自动映射。

### Q: 页面切换时内存泄漏？

A: 使用 `PageLifecycle` 确保页面销毁时调用 `onDestroy()` 清理资源。

### Q: 视频播放卡顿？

A: 视频层不要放在LVGL内，用系统层 overlay/surface，UI只做控制层。

---

## 📝 检查清单

在提交代码前检查：

- [ ] 所有尺寸使用 `UIScale::s()`
- [ ] 使用 `LV_PCT(%)` 做相对布局
- [ ] 组件已模块化，无重复代码
- [ ] 焦点管理已实现
- [ ] 页面生命周期正确
- [ ] 无硬编码像素值
- [ ] 无循环刷新逻辑
- [ ] 事件回调已解绑（页面销毁时）

---

## 🎁 下一步

1. **优化现有布局**：将 `layouts.cpp` 中的硬编码改为使用缩放系统
2. **添加更多组件**：按钮组、列表、卡片等
3. **完善焦点路由**：实现方向键导航映射
4. **性能监控**：添加 FPS 监控和内存统计

---

**记住：LVGL 不是做炫技，是做稳定交付。先把 MVP 跑稳定，再追求美学。**










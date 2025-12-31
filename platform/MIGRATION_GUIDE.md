# 迁移指南：从现有代码迁移到跨平台架构

## 📋 迁移步骤

### 阶段 1：保留现有代码，并行开发

1. **保留 `src/sdl/` 目录**：现有代码继续工作
2. **创建新架构**：已完成的 `drivers/` 和 `platform/` 目录
3. **测试新接口**：在 Windows SDL 上验证新接口工作正常

### 阶段 2：逐步迁移

#### 步骤 1：更新 CMakeLists.txt

在主 `CMakeLists.txt` 中添加平台选择：

```cmake
# 平台选择（默认 Windows SDL）
option(KTV_PLATFORM_F133_LINUX "Build for F133 Linux" OFF)

if(KTV_PLATFORM_F133_LINUX)
    add_definitions(-DKTV_PLATFORM_F133_LINUX)
    # 添加 F133 平台源文件
    target_sources(ktvlv PRIVATE
        platform/f133_linux/display_fbdev.c
        platform/f133_linux/input_evdev.c
        platform/f133_linux/audio_alsa.c
    )
else()
    add_definitions(-DKTV_PLATFORM_WINDOWS_SDL)
    # 添加 Windows SDL 平台源文件
    target_sources(ktvlv PRIVATE
        platform/windows_sdl/display_sdl.c
        platform/windows_sdl/input_sdl.c
        platform/windows_sdl/audio_stub.c
    )
    # SDL2 依赖
    find_package(SDL2 REQUIRED)
    target_link_libraries(ktvlv PRIVATE SDL2::SDL2 SDL2::SDL2main)
endif()

# 添加核心应用入口
target_sources(ktvlv PRIVATE core/app_main.c)

# 添加包含目录
target_include_directories(ktvlv PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers
    ${CMAKE_CURRENT_SOURCE_DIR}/platform
    ${CMAKE_CURRENT_SOURCE_DIR}/core
)
```

#### 步骤 2：修改 main.cpp

**旧代码**（直接调用 SDL）：

```cpp
static bool init_display() {
    if (!sdl_init()) {
        return false;
    }
    // ... LVGL 注册
    disp_drv.flush_cb = sdl_display_flush;
    // ...
}
```

**新代码**（使用抽象接口）：

```cpp
#include "drivers/display_driver.h"

static bool init_display() {
    if (!DISPLAY.init()) {
        return false;
    }
    // ... LVGL 注册
    disp_drv.flush_cb = DISPLAY.flush;  // 使用抽象接口
    // ...
}
```

#### 步骤 3：修改输入处理

**旧代码**：

```cpp
static void init_input() {
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.read_cb = sdl_mouse_read;
    lv_indev_drv_register(&indev_drv);
    // ...
}

// 主循环中
sdl_update_mouse_state(&e);
sdl_update_keyboard_state(&e);
```

**新代码**：

```cpp
#include "drivers/input_driver.h"

static void init_input() {
    if (!INPUT.init()) {
        return;
    }
    INPUT.register_device(INPUT_TYPE_POINTER);
    INPUT.register_device(INPUT_TYPE_KEYPAD);
}

// 主循环中
INPUT.process_event(&e);  // 统一事件处理
```

#### 步骤 4：F133 平台特殊处理

在 F133 主循环中，需要额外调用 evdev 事件读取：

```cpp
#ifdef KTV_PLATFORM_F133
#include "platform/f133_linux/input_evdev.h"

while (1) {
    evdev_read_events_exported();  // 读取 evdev 事件
    lv_timer_handler();
    usleep(5000);
}
#endif
```

### 阶段 3：验证和优化

1. **Windows SDL 验证**：
   - 确保新接口功能与旧代码一致
   - 测试所有输入设备（鼠标、键盘）
   - 测试显示刷新

2. **F133 平台验证**：
   - 根据实际硬件调整设备路径
   - 调整颜色格式转换
   - 测试触摸屏和遥控器

3. **清理旧代码**：
   - 确认新架构稳定后，可以删除 `src/sdl/`（或保留作为参考）

## 🔄 回退方案

如果迁移过程中出现问题，可以：

1. **临时回退**：在 CMakeLists.txt 中注释掉新平台代码，恢复使用 `src/sdl/`
2. **并行运行**：保留两套代码，通过编译选项切换
3. **逐步替换**：先迁移显示，再迁移输入，最后迁移音频

## 📝 检查清单

- [ ] CMakeLists.txt 已更新平台选择
- [ ] main.cpp 已使用新的驱动接口
- [ ] Windows SDL 平台功能正常
- [ ] F133 平台设备路径已配置
- [ ] 输入事件处理正确
- [ ] 显示刷新正常
- [ ] 清理了未使用的旧代码（可选）

## 🆘 常见问题

### Q: 编译错误 "undefined reference to DISPLAY"

**A**: 确保在 CMakeLists.txt 中添加了对应平台的源文件。

### Q: F133 上触摸屏无响应

**A**: 
1. 检查 `/dev/input/eventX` 路径是否正确
2. 确保在主循环中调用了 `evdev_read_events_exported()`
3. 检查设备权限（可能需要 root 或加入 input 组）

### Q: F133 上显示颜色异常

**A**: 检查 `display_fbdev.c` 中的颜色格式转换，根据实际 framebuffer 格式调整。

## 📚 相关文档

- [platform/README.md](./README.md) - 架构说明
- [SDL仿真最佳实践与避坑指南.md](../SDL仿真最佳实践与避坑指南.md)
- [F133_KTV移植方案总结.md](../F133_KTV移植方案总结.md)



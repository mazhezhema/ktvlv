# 跨平台驱动架构说明

## 📁 目录结构

```
/ktvlv
  /drivers              # 驱动抽象接口层（跨平台统一接口）
      display_driver.h  # 显示驱动接口
      input_driver.h   # 输入驱动接口
      audio_driver.h   # 音频驱动接口
      
  /platform
      /windows_sdl/     # Windows SDL 平台实现
          display_sdl.c
          input_sdl.c
          audio_stub.c
          
      /f133_linux/      # F133 Linux 平台实现
          display_fbdev.c
          input_evdev.c
          audio_alsa.c
          
  /core
      app_main.c        # 应用主入口（跨平台统一）
      app_main.h
```

## 🎯 设计原则

1. **接口统一**：所有平台实现相同的抽象接口
2. **平台隔离**：UI 层和服务层不感知具体平台
3. **快速切换**：通过编译选项切换平台，代码无需修改

## 🔧 使用方法

### Windows SDL 平台

在 `CMakeLists.txt` 中：

```cmake
# 定义平台
add_definitions(-DKTV_PLATFORM_WINDOWS_SDL)

# 添加平台实现源文件
target_sources(ktvlv PRIVATE
    platform/windows_sdl/display_sdl.c
    platform/windows_sdl/input_sdl.c
    platform/windows_sdl/audio_stub.c
)
```

### F133 Linux 平台

在 `CMakeLists.txt` 中：

```cmake
# 定义平台
add_definitions(-DKTV_PLATFORM_F133_LINUX)

# 添加平台实现源文件
target_sources(ktvlv PRIVATE
    platform/f133_linux/display_fbdev.c
    platform/f133_linux/input_evdev.c
    platform/f133_linux/audio_alsa.c
)
```

## 📝 接口使用示例

### 显示驱动

```c
#include "drivers/display_driver.h"

// 初始化
if (!DISPLAY.init()) {
    // 错误处理
}

// 在 LVGL 中注册
lv_disp_drv_t disp_drv;
lv_disp_drv_init(&disp_drv);
disp_drv.flush_cb = DISPLAY.flush;  // 使用抽象接口
lv_disp_drv_register(&disp_drv);
```

### 输入驱动

```c
#include "drivers/input_driver.h"

// 初始化
if (!INPUT.init()) {
    // 错误处理
}

// 注册设备
INPUT.register_device(INPUT_TYPE_POINTER);   // 触摸屏
INPUT.register_device(INPUT_TYPE_KEYPAD);    // 遥控器
```

## ⚠️ 注意事项

### F133 平台特殊处理

1. **显示驱动**：
   - 使用 `full_refresh = 0`（partial refresh）
   - 根据实际 framebuffer 格式调整颜色转换

2. **输入驱动**：
   - 在主循环中调用 `evdev_read_events_exported()`
   - 根据实际设备路径调整 `/dev/input/eventX`

3. **音频驱动**：
   - 如果不需要系统音效，可以保持 stub 实现
   - 播放器音频由播放器层直接处理

## 🚀 迁移步骤

1. **保留现有代码**：`src/sdl/` 暂时保留，逐步迁移
2. **测试新接口**：在 Windows SDL 上验证新接口工作正常
3. **实现 F133**：根据实际硬件配置调整 F133 实现
4. **切换平台**：通过 CMake 选项切换，验证功能

## 📚 相关文档

- [SDL仿真最佳实践与避坑指南.md](../SDL仿真最佳实践与避坑指南.md)
- [F133_KTV移植方案总结.md](../F133_KTV移植方案总结.md)




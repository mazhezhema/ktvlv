# 驱动抽象接口层

## 📁 文件说明

- `display_driver.h` - 显示驱动抽象接口
- `input_driver.h` - 输入驱动抽象接口
- `audio_driver.h` - 音频驱动抽象接口

## 🎯 设计目标

1. **统一接口**：所有平台实现相同的接口
2. **平台隔离**：上层代码不感知具体平台
3. **易于扩展**：新增平台只需实现接口

## 📝 接口使用

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
disp_drv.flush_cb = DISPLAY.flush;
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
INPUT.register_device(INPUT_TYPE_POINTER);
INPUT.register_device(INPUT_TYPE_KEYPAD);
```

### 音频驱动

```c
#include "drivers/audio_driver.h"

// 初始化（可选）
AUDIO.init();

// 播放音效（可选）
AUDIO.play_sound(SOUND_ID_CLICK);
```

## ⚠️ 注意事项

1. **接口实例**：每个平台实现必须定义并初始化 `DISPLAY`、`INPUT`、`AUDIO` 全局变量
2. **初始化顺序**：必须先调用 `init()`，再注册到 LVGL
3. **清理**：程序退出前调用 `deinit()`

## 🔗 相关文档

- [platform/README.md](../platform/README.md) - 平台实现说明
- [platform/MIGRATION_GUIDE.md](../platform/MIGRATION_GUIDE.md) - 迁移指南






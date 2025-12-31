# Windows SEH 异常修复说明

## 🎯 问题定位

**核心问题**：`lv_timer_handler()` 在 Windows SDL 仿真环境中访问非法内存，导致 SEH 异常 (0xc0000005)。

**根本原因**：LVGL 显示驱动分辨率被设置为 0x0，导致定时器在处理坐标、刷新、布局时访问无效内存。

## 🔍 问题分析

### 日志证据
```
Display driver resolution is 0x0,0x0  ❌
SDL window created: 1280x720          ✔️
LVGL display buffer: dual buffer      ✔️
Windows SEH exception in lv_timer_handler ❌
```

**结论**：SDL 正确初始化了窗口，但 LVGL 没有获取到分辨率，定时器崩溃。

### 可能原因
1. `lv_disp_drv_t` 使用局部变量，在 `lv_disp_drv_register()` 后可能被销毁
2. 分辨率在注册前未正确设置
3. LVGL 内部在注册时重置了分辨率（虽然不太可能）

## ✅ 修复方案

### 1. 使用静态变量存储 `disp_drv`
**位置**：`src/main.cpp` 的 `init_display()` 函数

**修复前**：
```cpp
lv_disp_drv_t disp_drv;  // 局部变量
lv_disp_drv_init(&disp_drv);
// ...
lv_disp_drv_register(&disp_drv);
```

**修复后**：
```cpp
static lv_disp_drv_t disp_drv;  // 静态变量，确保生命周期
lv_disp_drv_init(&disp_drv);
// ...
lv_disp_drv_register(&disp_drv);
```

**原因**：LVGL 内部会保存驱动指针，如果使用局部变量可能导致悬空指针。

### 2. 添加防御性检查
**位置**：`src/main.cpp` 的 `init_display()` 函数

**添加的检查**：
- ✅ 注册前验证分辨率是否有效（> 0）
- ✅ 注册后立即验证分辨率是否正确传递
- ✅ 如果分辨率是 0x0，立即报错并退出

**代码示例**：
```cpp
// 注册前检查
if (disp_drv.hor_res <= 0 || disp_drv.ver_res <= 0) {
    PLOGE << "Display driver resolution is invalid before registration";
    return false;
}

// 注册后检查
lv_coord_t disp_w = lv_disp_get_hor_res(disp);
lv_coord_t disp_h = lv_disp_get_ver_res(disp);
if (disp_w <= 0 || disp_h <= 0) {
    PLOGE << "CRITICAL: Display driver resolution is 0x0 after registration!";
    return false;
}
```

### 3. 改进主循环中的分辨率获取逻辑
**位置**：`src/main.cpp` 的 `SDL_main()` 函数

**修复前**：如果分辨率是 0x0，会回退到使用宏定义的分辨率，继续运行（导致崩溃）

**修复后**：如果分辨率是 0x0，立即报错并退出，提供详细的错误信息

**代码示例**：
```cpp
if (disp_w <= 0 || disp_h <= 0) {
    PLOGE << "CRITICAL: Display driver resolution is 0x0! This will cause crashes.";
    fprintf(stderr, "[INIT] CRITICAL ERROR: Display driver resolution is 0x0!\n");
    fprintf(stderr, "[INIT] Possible causes:\n");
    fprintf(stderr, "[INIT]   1. disp_drv.hor_res/ver_res not set before lv_disp_drv_register()\n");
    fprintf(stderr, "[INIT]   2. LVGL internal error during driver registration\n");
    fprintf(stderr, "[INIT]   3. Display driver structure was destroyed before registration\n");
    return -1;
}
```

## 📋 修复检查清单

- [x] 使用静态变量存储 `disp_drv`
- [x] 注册前验证分辨率
- [x] 注册后验证分辨率
- [x] 主循环中严格检查分辨率
- [x] 提供详细的错误信息
- [x] 确保 flush 回调正确绑定（已存在）

## 🚀 验证步骤

1. **编译项目**
   ```bash
   cd build_ninja2
   ninja
   ```

2. **运行程序**
   ```bash
   ./ktvlv.exe
   ```

3. **检查日志**
   - ✅ 应该看到：`[INIT] Display driver registered successfully: 1280x720`
   - ❌ 不应该看到：`Display driver resolution is 0x0`
   - ❌ 不应该看到：`Windows SEH exception in lv_timer_handler()`

## 🔧 如果问题仍然存在

如果修复后仍然出现 0x0 分辨率，请检查：

1. **LVGL 版本兼容性**
   - 检查 `lv_disp_drv_register()` 的实现
   - 确认 LVGL 版本是否支持当前用法

2. **初始化顺序**
   - 确保 `lv_init()` 在显示驱动初始化之前调用
   - 确保 SDL 初始化在 LVGL 显示驱动注册之前完成

3. **内存对齐**
   - 检查 `lv_disp_drv_t` 结构体的内存对齐
   - 某些平台可能需要特殊的内存对齐

4. **调试信息**
   - 在 `lv_disp_drv_register()` 前后打印 `disp_drv` 的所有字段
   - 检查是否有其他代码修改了 `disp_drv`

## 📝 相关文件

- `src/main.cpp` - 主程序入口，包含显示驱动初始化
- `src/sdl/sdl.cpp` - SDL 显示驱动实现
- `platform/windows_sdl/display_sdl.c` - 平台抽象显示驱动（备用实现）
- `lv_conf.h` - LVGL 配置文件，定义分辨率宏

## 🎁 最佳实践建议

1. **始终使用静态变量存储 `lv_disp_drv_t`**
   - 避免使用局部变量
   - 确保驱动结构体在整个程序生命周期内有效

2. **注册后立即验证分辨率**
   - 不要假设注册会成功
   - 如果分辨率是 0x0，立即报错并退出

3. **提供详细的错误信息**
   - 帮助快速定位问题
   - 列出可能的原因和解决方案

4. **防御性编程**
   - 在所有关键点添加检查
   - 不要依赖默认行为

---

**修复日期**：2024年
**修复人员**：AI Assistant
**状态**：✅ 已完成



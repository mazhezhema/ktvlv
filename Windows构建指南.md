# Windows 平台构建指南（ktvlv 项目）

> **最后更新**：2025-01-XX  
> **适用平台**：Windows 10/11，MSVC 工具链  
> **构建系统**：CMake + Ninja

---

## 🚀 快速开始

### 一键构建（推荐）

```bat
cd D:\dev\ktvlv\ktvlv
build_fast.bat
```

**脚本功能：**
- ✅ 自动检测构建目录是否存在
- ✅ 自动判断是否需要重新 CMake
- ✅ 自动加载 MSVC 环境
- ✅ 自动并行构建（使用所有 CPU 核心）
- ✅ 一键完成所有构建流程

---

## 📋 构建规则判断

### ✅ 什么时候需要重新运行 CMake？

**必须重新 CMake 的情况：**
- 新增/删除源文件（.cpp/.h）
- 新增库依赖（find_package、target_link_libraries）
- 修改 CMakeLists.txt 构建规则
- 修改编译选项（/O2、/MT、/MD、/EHsc 等）
- 切换工具链（MSVC → MinGW/Clang）
- 修改 include 目录（target_include_directories）
- 新增 target（add_executable / add_library）

### ✅ 什么时候直接运行 Ninja 就行？

**直接 `ninja -C build_ninja2 -j12` 的情况：**
- 只修改代码（.cpp/.h 文件内容）
- 修改 UI 逻辑、事件处理
- 修改业务代码、服务实现
- 修改 LVGL 页面、组件
- 修改宏定义（在已有 target 内）
- 修改资源文件（不影响构建路径）

### 🎯 记忆口诀

> **"动规则跑 CMake，动代码跑 Ninja"**  
> **"动依赖跑 CMake，动逻辑跑 Ninja"**  
> **"动工具链跑 CMake，动界面跑 Ninja"**

**简化版：**
> 只要你觉得"可能影响构建行为" → 跑 CMake  
> 否则先 `ninja`，编不过再跑 CMake

---

## 🔧 手动构建命令

### 完整流程（首次或需要重新配置）

```bat
call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
cmake -S . -B build_ninja2 -G Ninja -DCMAKE_BUILD_TYPE=Release -DKTV_USE_LV_DRIVERS=OFF
ninja -C build_ninja2 -j12
```

### 增量构建（只修改代码）

```bat
call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
cd build_ninja2
ninja -j12
```

### 使用 CMake Preset

```bat
cmake --preset msvc-release
ninja -C build_ninja2 -j12
```

---

## ⚙️ 关键配置说明

### 1. External Include 机制已禁用

**配置位置**：`CMakeLists.txt` 顶层

```cmake
if (MSVC)
  add_compile_options(/external-)
  set_property(GLOBAL PROPERTY CMAKE_MSVC_EXTERNAL_INCLUDE_DIRS "")
  set_property(GLOBAL PROPERTY CMAKE_MSVC_EXTERNAL_WARNING_LEVEL "")
endif()
```

**原因：**
- 避免标准库头文件（`stdint.h`, `string`, `vector` 等）找不到的问题
- 提升构建速度 20%+
- 避免与 vcpkg toolchain 冲突

### 2. 不使用 vcpkg toolchain

**策略：**
- 手动 `find_package` 查找库（SDL2、CURL、ZLIB）
- 从 vcpkg 安装目录手动查找，但不通过 toolchain 文件
- 完全控制编译选项，避免 external include 冲突

**配置示例：**
```cmake
# SDL2 - 手动查找（不使用 vcpkg toolchain）
set(VCPKG_ROOT "D:/vcpkg" CACHE PATH "vcpkg root directory")
if (EXISTS "${VCPKG_ROOT}/installed/x64-windows")
    set(SDL2_DIR "${VCPKG_ROOT}/installed/x64-windows/share/sdl2")
    find_package(SDL2 QUIET)
endif()
```

### 3. 构建目录配置

- **默认目录**：`build_ninja2`
- **应加入 Windows Defender 排除列表**（提升性能）
- **确保在 SSD 上**（不要在 OneDrive 同步目录）

---

## ⚡ 性能优化

### 1. Windows Defender 排除

**操作步骤：**
1. Windows 安全中心 → 病毒和威胁防护 → 排除项
2. 添加文件夹：`D:\dev\ktvlv\ktvlv\build_ninja2`

**效果：** 减少 10% ~ 30% IO 阻滞

### 2. 并行构建

- **脚本自动使用**：`-j %NUMBER_OF_PROCESSORS%`（使用所有 CPU 核心）
- **手动构建使用**：`ninja -j12`（根据 CPU 核心数调整）
- **推荐线程数**：CPU 核心数 / 2

### 3. SSD 位置

- 确保项目在 SSD 上
- 不要在机械硬盘或 OneDrive 同步目录
- 避免网络驱动器

### 4. 其他优化建议

**启用 vcpkg 二进制缓存（可选）：**
```bat
vcpkg integrate install
setx VCPKG_FEATURE_FLAGS "manifests;binarycaching"
```

**使用 Clang-Cl 工具链（可选，速度更快）：**
```bat
cmake -S . -B build_clang -G Ninja ^
  -DCMAKE_C_COMPILER=clang-cl ^
  -DCMAKE_CXX_COMPILER=clang-cl
```

**分析构建瓶颈：**
```bat
ninja -C build_ninja2 -d stats
```

---

## 🐛 常见问题排查

### 权限错误（Permission denied）

**症状：**
```
ninja: error: failed recompaction: Permission denied
```

**解决方案：**
1. 关闭所有可能锁定文件的程序（IDE、文件管理器、杀毒软件等）
2. 完全删除 build_ninja2 目录：
   ```bat
   Remove-Item -Recurse -Force build_ninja2
   ```
3. 重新运行 `build_fast.bat`

### 找不到标准库头文件

**症状：**
```
fatal error C1083: Cannot open include file: 'stdint.h': No such file or directory
```

**解决方案：**
- ✅ 已通过禁用 external 机制解决
- 如果仍有问题，检查 CMakeLists.txt 中的 `/external-` 设置
- 确保没有使用 vcpkg toolchain

### 构建规则缺失

**症状：**
```
ninja: error: loading 'build.ninja': The system cannot find the file specified.
```

**解决方案：**
1. 运行 `ninja -C build_ninja2 -t recompact` 检查
2. 如果失败，需要重新 CMake：
   ```bat
   cmake -S . -B build_ninja2 -G Ninja -DCMAKE_BUILD_TYPE=Release -DKTV_USE_LV_DRIVERS=OFF
   ```

### SDL2 链接错误

**症状：**
```
error LNK2019: unresolved external symbol SDL_main referenced in function main_getcmdline
```

**解决方案：**
- ✅ 已修复：在 `main.cpp` 中添加了 `#include <SDL_main.h>`
- 确保 `main` 函数签名：`int main(int argc, char* argv[])`

---

## 📝 项目特定配置

| 配置项 | 值 |
|--------|-----|
| **工具链** | MSVC 19.50（Visual Studio 2026） |
| **构建系统** | Ninja |
| **构建类型** | Release（默认） |
| **LVGL 驱动** | OFF（Windows 仿真使用 SDL） |
| **vcpkg** | 手动查找，不使用 toolchain |
| **C++ 标准** | C++17 |
| **字符编码** | UTF-8 |

---

## 🎁 快速参考表

| 操作 | 命令 |
|------|------|
| **快速构建** | `build_fast.bat` |
| **增量构建** | `ninja -C build_ninja2 -j12` |
| **重新配置** | `cmake -S . -B build_ninja2 -G Ninja -DCMAKE_BUILD_TYPE=Release -DKTV_USE_LV_DRIVERS=OFF` |
| **清理构建** | `Remove-Item -Recurse -Force build_ninja2` |
| **查看构建统计** | `ninja -C build_ninja2 -d stats` |
| **使用 Preset** | `cmake --preset msvc-release` |

---

## 📚 相关文件

- **构建脚本**：`build_fast.bat`
- **CMake 配置**：`CMakeLists.txt`
- **CMake Presets**：`CMakePresets.json`
- **Git 忽略**：`.gitignore`

---

## 🔄 工作流程建议

### 日常开发流程

```
1. 修改代码
2. 运行 build_fast.bat
3. 测试功能
4. 重复
```

### 添加新功能流程

```
1. 添加源文件
2. 修改 CMakeLists.txt（如需要）
3. 运行 build_fast.bat（会自动重新 CMake）
4. 实现功能
5. 测试
```

### 添加新依赖流程

```
1. 安装库（vcpkg install <package>）
2. 修改 CMakeLists.txt（添加 find_package）
3. 运行 build_fast.bat（会自动重新 CMake）
4. 使用库
```

---

**提示**：大部分情况下，直接运行 `build_fast.bat` 即可，脚本会自动处理所有判断和配置。



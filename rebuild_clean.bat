@echo off
REM ============================================================
REM 完全重新构建脚本 - 清理并重新配置
REM ============================================================

setlocal enabledelayedexpansion

set "PROJECT_ROOT=%~dp0"
set "BUILD_DIR=%PROJECT_ROOT%build_ninja2"

echo ========================================
echo 完全重新构建 - 清理构建目录
echo ========================================
echo.

REM 1. 检测Visual Studio路径
set "VSROOT="
if exist "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" (
    set "VSROOT=D:\Program Files\Microsoft Visual Studio\18\Community"
    echo [检测] 找到 Visual Studio 18
    goto :vs_found
)

echo [错误] 未找到 Visual Studio 安装
exit /b 1

:vs_found

REM 2. 删除旧的构建目录
if exist "%BUILD_DIR%" (
    echo [清理] 删除旧的构建目录: %BUILD_DIR%
    rmdir /s /q "%BUILD_DIR%"
    echo [成功] 构建目录已删除
    echo.
)

REM 3. 初始化MSVC环境
echo [初始化] 设置 MSVC 环境...
call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

if errorlevel 1 (
    echo [错误] MSVC 环境初始化失败
    exit /b 1
)

echo [成功] MSVC 环境已初始化
echo.

REM 4. 重新配置 CMake
echo [配置] 重新配置 CMake...
cmake -S . -B build_ninja2 -G Ninja -DCMAKE_TOOLCHAIN_FILE=D:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows -DCMAKE_BUILD_TYPE=Release

if errorlevel 1 (
    echo [错误] CMake 配置失败
    exit /b 1
)

echo.
echo [成功] CMake 配置完成
echo.

REM 5. 构建
echo [构建] 开始编译 ktvlv.exe...
cd /d "%BUILD_DIR%"
cmake --build . --config Release --target ktvlv --parallel 8

if errorlevel 1 (
    echo.
    echo [错误] 构建失败
    exit /b 1
)

REM 6. 验证构建结果
if exist "ktvlv.exe" (
    echo.
    echo ========================================
    echo [成功] ktvlv.exe 已生成！
    echo ========================================
    for %%F in (ktvlv.exe) do (
        echo 路径: %CD%\ktvlv.exe
        echo 大小: %%~zF 字节
        echo 时间: %%~tF
    )
    echo.
    exit /b 0
) else (
    echo.
    echo [警告] 构建完成但未找到 ktvlv.exe
    exit /b 1
)


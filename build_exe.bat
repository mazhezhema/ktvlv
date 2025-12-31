@echo off
REM ============================================================
REM 构建脚本 - 最佳实践版本
REM 自动检测Visual Studio，初始化环境，构建ktvlv.exe
REM ============================================================

setlocal enabledelayedexpansion

set "PROJECT_ROOT=%~dp0"
set "BUILD_DIR=%PROJECT_ROOT%build_ninja2"
set "CONFIG=Release"
set "PARALLEL=8"

echo ========================================
echo KTVLV 构建脚本
echo ========================================
echo.

REM 1. 检测Visual Studio路径（优先使用vswhere，然后手动检测）
set "VSROOT="

REM 尝试使用vswhere工具
if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
    for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do (
        set "VSROOT=%%i"
    )
    if defined VSROOT (
        if exist "!VSROOT!\VC\Auxiliary\Build\vcvars64.bat" (
            echo [检测] 使用 vswhere 找到 Visual Studio: !VSROOT!
            goto :vs_found
        )
    )
)

REM 手动检测常见路径
if exist "D:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    set "VSROOT=D:\Program Files\Microsoft Visual Studio\2022\Community"
    echo [检测] 找到 Visual Studio 2022
    goto :vs_found
)
if exist "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" (
    set "VSROOT=D:\Program Files\Microsoft Visual Studio\18\Community"
    echo [检测] 找到 Visual Studio 2019 (版本18)
    goto :vs_found
)
if exist "D:\Program Files\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" (
    set "VSROOT=D:\Program Files\Microsoft Visual Studio\2019\Community"
    echo [检测] 找到 Visual Studio 2019
    goto :vs_found
)
if exist "D:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" (
    set "VSROOT=D:\Program Files (x86)\Microsoft Visual Studio\2019\Community"
    echo [检测] 找到 Visual Studio 2019
    goto :vs_found
)

echo [错误] 未找到 Visual Studio 安装
echo 请确保已安装 Visual Studio 2019 或 2022，并包含 C++ 桌面开发组件
exit /b 1

:vs_found

REM 2. 检查构建目录
if not exist "%BUILD_DIR%" (
    echo [错误] 构建目录不存在: %BUILD_DIR%
    echo.
    echo 提示: 如果是首次构建，请先运行完整配置：
    echo   cmake -S . -B build_ninja2 -G Ninja -DCMAKE_TOOLCHAIN_FILE=D:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows -DCMAKE_BUILD_TYPE=Release
    exit /b 1
)

REM 3. 初始化MSVC环境并构建
echo [初始化] 设置 MSVC 环境...
call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

if errorlevel 1 (
    echo [错误] MSVC 环境初始化失败
    exit /b 1
)

echo [成功] MSVC 环境已初始化
echo.
echo [构建] 开始编译 ktvlv.exe...
echo        配置: %CONFIG%
echo        并行数: %PARALLEL%
echo.

cd /d "%BUILD_DIR%"
cmake --build . --config %CONFIG% --target ktvlv --parallel %PARALLEL%

if errorlevel 1 (
    echo.
    echo [错误] 构建失败，退出代码: %ERRORLEVEL%
    exit /b 1
)

REM 4. 验证构建结果
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
    
    REM 检查DLL
    set "DLL_COUNT=0"
    for %%F in (*.dll) do (
        set /a DLL_COUNT+=1
        if !DLL_COUNT! equ 1 (
            echo 依赖 DLL:
        )
        echo   - %%F
    )
    if !DLL_COUNT! gtr 0 (
        echo.
    )
    
    exit /b 0
) else (
    echo.
    echo [警告] 构建完成但未找到 ktvlv.exe
    exit /b 1
)


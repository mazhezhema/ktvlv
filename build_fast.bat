@echo off
REM ============================================================
REM 快速构建脚本 - 优化版本（快速构建exe专用）
REM 使用增量构建，避免重新配置，自动复制DLL
REM ============================================================

setlocal

REM 检测 Visual Studio 路径
if exist "D:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    set VSROOT=D:\Program Files\Microsoft Visual Studio\2022\Community
) else if exist "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" (
    set VSROOT=D:\Program Files\Microsoft Visual Studio\18\Community
) else (
    echo [错误] 找不到 Visual Studio 安装路径
    exit /b 1
)

REM 初始化 MSVC 环境
call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

REM 检查构建目录是否存在
if not exist "%~dp0build_ninja2" (
    echo [错误] 构建目录不存在，请先运行完整构建脚本进行首次配置
    exit /b 1
)

REM 进入构建目录
cd /d "%~dp0build_ninja2"

REM 快速构建：只构建 ktvlv 目标，使用 8 个并行任务，Release 配置
echo [快速构建] 开始编译 ktvlv.exe...
cmake --build . --parallel 8 --config Release --target ktvlv

if %ERRORLEVEL% EQU 0 (
    if exist ktvlv.exe (
        echo.
        echo [成功] ktvlv.exe 已生成在: %CD%
        echo [信息] DLL已自动复制，可直接运行
        dir /B ktvlv.exe *.dll 2>nul
        echo.
    ) else (
        echo [警告] 构建完成但未找到 ktvlv.exe
    )
) else (
    echo [错误] 构建失败，错误代码: %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

endlocal


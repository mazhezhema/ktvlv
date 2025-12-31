# ============================================================
# PowerShell 构建脚本 - 最佳实践版本
# 自动检测Visual Studio，初始化MSVC环境，构建ktvlv.exe
# ============================================================

param(
    [switch]$Clean = $false,
    [string]$Config = "Release",
    [int]$Parallel = 8
)

$ErrorActionPreference = "Stop"
$script:ProjectRoot = $PSScriptRoot
$script:BuildDir = Join-Path $ProjectRoot "build_ninja2"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "KTVLV 构建脚本" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# 1. 检测Visual Studio路径
function Find-VisualStudio {
    $vsPaths = @(
        "D:\Program Files\Microsoft Visual Studio\2022\Community",
        "D:\Program Files\Microsoft Visual Studio\2022\Professional",
        "D:\Program Files\Microsoft Visual Studio\2022\Enterprise",
        "D:\Program Files\Microsoft Visual Studio\2019\Community",
        "D:\Program Files\Microsoft Visual Studio\2019\Professional",
        "D:\Program Files\Microsoft Visual Studio\2019\Enterprise",
        "D:\Program Files (x86)\Microsoft Visual Studio\2019\Community",
        "D:\Program Files (x86)\Microsoft Visual Studio\2019\Professional",
        "D:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise"
    )
    
    # 尝试使用vswhere工具（如果可用）
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        Write-Host "[检测] 使用vswhere查找Visual Studio..." -ForegroundColor Yellow
        $vsPath = & $vswhere -latest -property installationPath
        if ($vsPath -and (Test-Path (Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"))) {
            Write-Host "[成功] 找到Visual Studio: $vsPath" -ForegroundColor Green
            return $vsPath
        }
    }
    
    # 手动检测
    Write-Host "[检测] 手动检测Visual Studio路径..." -ForegroundColor Yellow
    foreach ($path in $vsPaths) {
        $vcvarsPath = Join-Path $path "VC\Auxiliary\Build\vcvars64.bat"
        if (Test-Path $vcvarsPath) {
            Write-Host "[成功] 找到Visual Studio: $path" -ForegroundColor Green
            return $path
        }
    }
    
    Write-Host "[错误] 未找到Visual Studio安装" -ForegroundColor Red
    Write-Host "请确保已安装Visual Studio 2019或2022，并包含C++桌面开发组件" -ForegroundColor Yellow
    return $null
}

# 2. 初始化MSVC环境（使用cmd包装）
function Initialize-MSVCEvironment {
    param([string]$VSPath)
    
    $vcvarsPath = Join-Path $VSPath "VC\Auxiliary\Build\vcvars64.bat"
    if (-not (Test-Path $vcvarsPath)) {
        throw "vcvars64.bat not found at: $vcvarsPath"
    }
    
    Write-Host "[初始化] 设置MSVC环境..." -ForegroundColor Yellow
    
    # 创建一个临时批处理文件来执行构建
    $tempBat = Join-Path $env:TEMP "ktvlv_build_$(Get-Random).bat"
    $batLines = @(
        '@echo off',
        "call `"$vcvarsPath`" >nul 2>&1",
        "cd /d `"$script:BuildDir`"",
        "cmake --build . --config $Config --target ktvlv --parallel $Parallel",
        'exit /b %ERRORLEVEL%'
    )
    $batContent = $batLines -join "`r`n"
    
    Set-Content -Path $tempBat -Value $batContent -Encoding ASCII -NoNewline
    
    try {
        # 使用cmd执行批处理文件
        $result = & cmd /c $tempBat
        $exitCode = $LASTEXITCODE
        
        if ($exitCode -eq 0) {
            Write-Host "[成功] MSVC环境已初始化并执行构建" -ForegroundColor Green
            return $true
        } else {
            Write-Host "[错误] 构建失败，退出代码: $exitCode" -ForegroundColor Red
            return $false
        }
    } finally {
        if (Test-Path $tempBat) {
            Remove-Item $tempBat -Force
        }
    }
}

# 3. 检查构建目录
function Test-BuildDirectory {
    if (-not (Test-Path $script:BuildDir)) {
        Write-Host "[错误] 构建目录不存在: $script:BuildDir" -ForegroundColor Red
        Write-Host "请先运行完整配置构建（首次构建）" -ForegroundColor Yellow
        return $false
    }
    return $true
}

# 4. 验证构建结果
function Test-BuildResult {
    $exePath = Join-Path $script:BuildDir "ktvlv.exe"
    if (Test-Path $exePath) {
        $exeInfo = Get-Item $exePath
        Write-Host ""
        Write-Host "========================================" -ForegroundColor Green
        Write-Host "[成功] ktvlv.exe 已生成！" -ForegroundColor Green
        Write-Host "========================================" -ForegroundColor Green
        Write-Host "路径: $exePath" -ForegroundColor White
        Write-Host "大小: $([math]::Round($exeInfo.Length / 1KB, 2)) KB" -ForegroundColor White
        Write-Host "时间: $($exeInfo.LastWriteTime)" -ForegroundColor White
        Write-Host ""
        
        # 检查DLL
        $dlls = Get-ChildItem $script:BuildDir -Filter "*.dll" | Select-Object -ExpandProperty Name
        if ($dlls) {
            Write-Host "依赖DLL:" -ForegroundColor Yellow
            foreach ($dll in $dlls) {
                Write-Host "  - $dll" -ForegroundColor Gray
            }
            Write-Host ""
        }
        
        return $true
    } else {
        Write-Host "[警告] 构建完成但未找到 ktvlv.exe" -ForegroundColor Yellow
        return $false
    }
}

# 主流程
try {
    # 查找Visual Studio
    $vsPath = Find-VisualStudio
    if (-not $vsPath) {
        exit 1
    }
    
    # 检查构建目录
    if (-not (Test-BuildDirectory)) {
        Write-Host ""
        Write-Host "提示: 如果是首次构建，请先运行完整配置：" -ForegroundColor Yellow
        Write-Host "  cmake -S . -B build_ninja2 -G Ninja -DCMAKE_TOOLCHAIN_FILE=D:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows -DCMAKE_BUILD_TYPE=Release" -ForegroundColor Gray
        exit 1
    }
    
    Write-Host ""
    Write-Host "[构建] 开始编译 ktvlv.exe..." -ForegroundColor Cyan
    Write-Host "       配置: $Config" -ForegroundColor Gray
    Write-Host "       并行数: $Parallel" -ForegroundColor Gray
    Write-Host ""
    
    # 初始化MSVC环境并执行构建
    $buildSuccess = Initialize-MSVCEvironment -VSPath $vsPath
    
    # 验证构建结果
    if ($buildSuccess) {
        $success = Test-BuildResult
    } else {
        $success = $false
    }
    
    if ($success) {
        exit 0
    } else {
        exit 1
    }
} catch {
    Write-Host ""
    Write-Host "[异常] $($_.Exception.Message)" -ForegroundColor Red
    Write-Host $_.ScriptStackTrace -ForegroundColor Gray
    exit 1
}


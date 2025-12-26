@echo off
setlocal

echo KTVLV Fast Build Starting...

REM Set project and build directories
set PROJECT_DIR=%cd%
set BUILD_DIR=%PROJECT_DIR%\build_ninja2

REM Check if build directory exists
if not exist "%BUILD_DIR%" (
    echo Build directory not found, initializing CMake...
    call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
    cmake -S "%PROJECT_DIR%" -B "%BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DKTV_USE_LV_DRIVERS=OFF
    if %errorlevel% neq 0 goto build_fail
    goto run_ninja
)

REM Check if build rules are missing
echo Checking build rules...
ninja -C "%BUILD_DIR%" -t recompact >nul 2>&1
if %errorlevel% neq 0 (
    echo Build rules changed, re-running CMake...
    call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
    cmake -S "%PROJECT_DIR%" -B "%BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DKTV_USE_LV_DRIVERS=OFF
    if %errorlevel% neq 0 goto build_fail
)

:run_ninja
echo Building...
call "D:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
ninja -C "%BUILD_DIR%" -j %NUMBER_OF_PROCESSORS%
if %errorlevel% neq 0 goto build_fail

REM Copy runtime DLLs to build directory
echo Copying runtime DLLs...
set VCPKG_BIN=D:\vcpkg\installed\x64-windows\bin
if exist "%VCPKG_BIN%\libcurl.dll" (
    copy /Y "%VCPKG_BIN%\libcurl.dll" "%BUILD_DIR%\" >nul 2>&1
    echo   libcurl.dll copied
)
if exist "%VCPKG_BIN%\SDL2.dll" (
    copy /Y "%VCPKG_BIN%\SDL2.dll" "%BUILD_DIR%\" >nul 2>&1
    echo   SDL2.dll copied
)
if exist "%VCPKG_BIN%\zlib.dll" (
    copy /Y "%VCPKG_BIN%\zlib.dll" "%BUILD_DIR%\" >nul 2>&1
    echo   zlib.dll copied
)

REM Copy config file to build directory
if exist "%PROJECT_DIR%\config.ini" (
    copy /Y "%PROJECT_DIR%\config.ini" "%BUILD_DIR%\" >nul 2>&1
    echo   config.ini copied
)

REM Verify exe was generated
echo.
echo Verifying executable...
if exist "%BUILD_DIR%\ktvlv.exe" (
    echo   ktvlv.exe generated successfully
    for %%A in ("%BUILD_DIR%\ktvlv.exe") do (
        echo   File size: %%~zA bytes
        echo   Location: %%~fA
    )
    echo.
    echo ========================================
    echo Build Success! Executable ready to run.
    echo ========================================
    echo.
    echo To run: cd %BUILD_DIR%
    echo          ktvlv.exe
    echo.
) else (
    echo   ERROR: ktvlv.exe not found!
    echo   Build may have failed even though ninja reported success.
    goto build_fail
)

exit /b 0

:build_fail
echo Build failed, check logs
exit /b 1

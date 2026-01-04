cmake_minimum_required(VERSION 3.20)
project(ktvlv LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ------------------------------------------------------------
# Options
# ------------------------------------------------------------
set(LVGL_TAG v8.3.11 CACHE STRING "LVGL version/tag")
set(LV_DRIVERS_TAG master CACHE STRING "lv_drivers branch/tag")

# 指定 lv_conf.h 路径，避免 lvgl 编译时找不到配置文件
set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE STRING "Path to lv_conf.h")

# ------------------------------------------------------------
# Dependencies: LVGL + lv_drivers + SDL2
# ------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(
  lvgl
  GIT_REPOSITORY https://github.com/lvgl/lvgl.git
  GIT_TAG        ${LVGL_TAG}
)

FetchContent_Declare(
  lv_drivers
  GIT_REPOSITORY https://github.com/lvgl/lv_drivers.git
  GIT_TAG        ${LV_DRIVERS_TAG}
)

# 关闭 pkg-config 生成，避免 Windows 仿真阶段找不到 lv-drivers.pc.in
set(LV_ENABLE_PKGCONFIG OFF CACHE BOOL "" FORCE)
set(LV_DRIVERS_ENABLE_PKGCONFIG OFF CACHE BOOL "" FORCE)

# 是否使用 lv_drivers（SDL/FB/evdev等）
option(KTV_USE_LV_DRIVERS "Use lv_drivers (framebuffer/evdev等)" ON)

if (KTV_USE_LV_DRIVERS)
  FetchContent_MakeAvailable(lvgl lv_drivers)
else()
  FetchContent_MakeAvailable(lvgl)
endif()

# SDL2 (expect installed on Windows; e.g., via vcpkg: vcpkg install sdl2)
find_package(SDL2 REQUIRED)

# ------------------------------------------------------------
# inih (INI parser) for配置读取
# ------------------------------------------------------------
FetchContent_Declare(
  inih
  GIT_REPOSITORY https://github.com/benhoyt/inih.git
  GIT_TAG        master
)
FetchContent_MakeAvailable(inih)

# ------------------------------------------------------------
# cJSON for JSON parsing
# ------------------------------------------------------------
FetchContent_Declare(
  cjson
  GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
  GIT_TAG        master
)
FetchContent_MakeAvailable(cjson)

# curl (via vcpkg)
find_package(CURL CONFIG REQUIRED)

# ------------------------------------------------------------
# 平台选择选项（默认使用旧代码，新架构可选）
# ------------------------------------------------------------
option(KTV_USE_NEW_PLATFORM_ARCH "Use new platform abstraction architecture" OFF)
option(KTV_PLATFORM_F133_LINUX "Build for F133 Linux platform" OFF)

if(KTV_USE_NEW_PLATFORM_ARCH)
    if(KTV_PLATFORM_F133_LINUX)
        message(STATUS "Platform: F133 Linux (New Architecture)")
        add_definitions(-DKTV_PLATFORM_F133_LINUX)
        set(PLATFORM_DISPLAY_SRC platform/f133_linux/display_fbdev.c)
        set(PLATFORM_INPUT_SRC platform/f133_linux/input_evdev.c)
        set(PLATFORM_AUDIO_SRC platform/f133_linux/audio_alsa.c)
    else()
        message(STATUS "Platform: Windows SDL (New Architecture)")
        add_definitions(-DKTV_PLATFORM_WINDOWS_SDL)
        set(PLATFORM_DISPLAY_SRC platform/windows_sdl/display_sdl.c)
        set(PLATFORM_INPUT_SRC platform/windows_sdl/input_sdl.c)
        set(PLATFORM_AUDIO_SRC platform/windows_sdl/audio_stub.c)
    endif()
    set(CORE_SRC core/app_main.c)
else()
    message(STATUS "Platform: Windows SDL (Legacy - using src/sdl/)")
endif()

# ------------------------------------------------------------
# Sources
# ------------------------------------------------------------
file(GLOB UI_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/*.cpp")
file(GLOB SERVICE_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/services/*.cpp")
file(GLOB EVENT_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/events/*.cpp")
file(GLOB CONFIG_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/config/*.cpp")

if(KTV_USE_NEW_PLATFORM_ARCH)
    # 使用新架构
    add_executable(ktvlv
      src/main.cpp
      ${UI_SRC}
      ${SERVICE_SRC}
      ${EVENT_SRC}
      ${CONFIG_SRC}
      ${PLATFORM_DISPLAY_SRC}
      ${PLATFORM_INPUT_SRC}
      ${PLATFORM_AUDIO_SRC}
      ${CORE_SRC}
    )
else()
    # 使用旧架构（兼容现有代码）
    file(GLOB SDL_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/sdl/*.cpp")
    add_executable(ktvlv
      src/main.cpp
      ${UI_SRC}
      ${SERVICE_SRC}
      ${EVENT_SRC}
      ${CONFIG_SRC}
      ${SDL_SRC}
    )
endif()

target_include_directories(ktvlv PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${lvgl_SOURCE_DIR}
  ${inih_SOURCE_DIR}
  ${cjson_SOURCE_DIR}
)

# 新架构包含目录
if(KTV_USE_NEW_PLATFORM_ARCH)
  target_include_directories(ktvlv PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers
    ${CMAKE_CURRENT_SOURCE_DIR}/platform
    ${CMAKE_CURRENT_SOURCE_DIR}/core
  )
endif()

target_compile_definitions(ktvlv PRIVATE
  LV_CONF_INCLUDE_SIMPLE
  LV_LVGL_H_INCLUDE_SIMPLE
)

# MSVC 源码统一按 UTF-8 编译，避免中文字符串编码问题
# 启用 C++ 异常处理，避免异常处理警告和问题
if (MSVC)
  target_compile_options(ktvlv PRIVATE /utf-8 /EHsc)
endif()

target_link_libraries(ktvlv PRIVATE
  lvgl
  lvgl::lvgl
  SDL2::SDL2
  SDL2::SDL2main
  CURL::libcurl
  cjson
)

# inih is a single-file library, add source instead of linking
target_sources(ktvlv PRIVATE ${inih_SOURCE_DIR}/ini.c)

# lv_drivers is C-only; add its sources to target
if (KTV_USE_LV_DRIVERS)
  target_include_directories(ktvlv PRIVATE ${lv_drivers_SOURCE_DIR})
  file(GLOB LV_DRIVERS_SRC
    "${lv_drivers_SOURCE_DIR}/SDL/*.c"
  )
  target_sources(ktvlv PRIVATE ${LV_DRIVERS_SRC})
  if (TARGET lv_drivers)
    target_link_libraries(lv_drivers PUBLIC SDL2::SDL2 SDL2::SDL2main)
  endif()
endif()

# Use our local lv_conf.h / lv_drv_conf.h
target_include_directories(ktvlv PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
)

# On Windows, disable console window (optional)
if (WIN32)
  set_target_properties(ktvlv PROPERTIES
    WIN32_EXECUTABLE OFF
  )
endif()

# ------------------------------------------------------------
# 自动复制运行时DLL到输出目录（快速构建优化）
# ------------------------------------------------------------
if (WIN32)
  # 复制SDL2 DLL
  add_custom_command(TARGET ktvlv POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:SDL2::SDL2>
    $<TARGET_FILE_DIR:ktvlv>
    COMMENT "复制 SDL2.dll..."
  )
  
  # 复制libcurl DLL
  add_custom_command(TARGET ktvlv POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:CURL::libcurl>
    $<TARGET_FILE_DIR:ktvlv>
    COMMENT "复制 libcurl.dll..."
  )
  
  # 复制curl的依赖DLL（zlib等）
  # 注意：vcpkg安装的curl可能依赖其他DLL，需要从vcpkg目录复制
  if (DEFINED VCPKG_INSTALLED_DIR)
    # 尝试复制zlib1.dll（curl的常见依赖）
    add_custom_command(TARGET ktvlv POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/zlib1.dll"
      $<TARGET_FILE_DIR:ktvlv>
      COMMENT "复制 zlib1.dll..."
    )
  endif()
  
  # 复制配置文件模板（如果不存在）
  add_custom_command(TARGET ktvlv POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/config.ini"
    $<TARGET_FILE_DIR:ktvlv>
    COMMENT "复制 config.ini..."
  )
endif()


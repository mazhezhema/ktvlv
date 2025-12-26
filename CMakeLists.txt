cmake_minimum_required(VERSION 3.20)

project(ktvlv LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# MSVC: 彻底禁用 external include 机制，提升构建速度 20%+
# 这是最直接的解决方案，确保标准库头文件能被正确找到，同时加速编译
if (MSVC)
  add_compile_options(/external-)
  # 同时设置全局属性，防止 CMake 自动添加
  set_property(GLOBAL PROPERTY CMAKE_MSVC_EXTERNAL_INCLUDE_DIRS "")
  set_property(GLOBAL PROPERTY CMAKE_MSVC_EXTERNAL_WARNING_LEVEL "")
  set(CMAKE_MSVC_EXTERNAL_INCLUDE_DIRS "" CACHE STRING "" FORCE)
  set(CMAKE_MSVC_EXTERNAL_WARNING_LEVEL "" CACHE STRING "" FORCE)
endif()

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

# MSVC: 在 LVGL 配置后，强制禁用 external 机制
# LVGL 的 CMake 可能自动添加了这些标志，我们需要禁用它们
# 注意：已在项目顶层全局禁用 external（add_compile_options(/external-)），
# 这里不需要重复设置，全局设置会覆盖 LVGL 的配置
if (MSVC AND TARGET lvgl)
  # 项目顶层已设置 add_compile_options(/external-)，这里不需要额外设置
endif()

# SDL2 - 手动查找（不使用 vcpkg toolchain）
# 优先使用 vcpkg 安装的版本，但不通过 toolchain 文件
if (NOT SDL2_FOUND)
    # 尝试从 vcpkg 安装目录查找
    set(VCPKG_ROOT "D:/vcpkg" CACHE PATH "vcpkg root directory")
    if (EXISTS "${VCPKG_ROOT}/installed/x64-windows")
        set(SDL2_DIR "${VCPKG_ROOT}/installed/x64-windows/share/sdl2")
        find_package(SDL2 QUIET)
    endif()
endif()
if (NOT SDL2_FOUND)
    # 如果还是找不到，尝试系统路径
    find_package(SDL2 REQUIRED)
endif()

# ------------------------------------------------------------
# plog (header-only) for logging
# ------------------------------------------------------------
FetchContent_Declare(
  plog
  GIT_REPOSITORY https://github.com/SergiusTheBest/plog.git
  GIT_TAG        1.1.9
)
FetchContent_MakeAvailable(plog)

# ------------------------------------------------------------
# moodycamel ConcurrentQueue (header-only) for lock-free queues
# ------------------------------------------------------------
FetchContent_Declare(
  moodycamel
  GIT_REPOSITORY https://github.com/cameron314/concurrentqueue.git
  GIT_TAG        master
)
FetchContent_MakeAvailable(moodycamel)

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
# 禁用 cJSON 测试，避免构建时文件锁定问题
set(ENABLE_CJSON_TEST OFF CACHE BOOL "Enable cJSON tests" FORCE)
set(ENABLE_CJSON_TEST_APP OFF CACHE BOOL "Enable cJSON test app" FORCE)
FetchContent_MakeAvailable(cjson)

# CURL - 手动查找（不使用 vcpkg toolchain）
# 需要先找到 ZLIB（CURL 的依赖）
set(VCPKG_ROOT "D:/vcpkg" CACHE PATH "vcpkg root directory")
if (EXISTS "${VCPKG_ROOT}/installed/x64-windows")
    # 设置 vcpkg 的包路径，让 find_package 能找到
    list(APPEND CMAKE_PREFIX_PATH "${VCPKG_ROOT}/installed/x64-windows")
    # 手动设置 ZLIB（CURL 的依赖）
    set(ZLIB_ROOT "${VCPKG_ROOT}/installed/x64-windows")
    find_package(ZLIB REQUIRED)
    # 然后查找 CURL
    find_package(CURL REQUIRED CONFIG)
else()
    # 如果 vcpkg 不存在，尝试系统路径
    find_package(ZLIB REQUIRED)
    find_package(CURL REQUIRED CONFIG)
endif()

# MSVC: 不使用 vcpkg toolchain，避免 external include 机制冲突
# 我们将手动使用 find_package 查找库，这样可以完全控制编译选项

# ------------------------------------------------------------
# Sources
# ------------------------------------------------------------
file(GLOB UI_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/*.cpp")
file(GLOB SERVICE_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/services/*.cpp")
file(GLOB EVENT_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/events/*.cpp")
file(GLOB LOGGING_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/logging/*.cpp")
file(GLOB CONFIG_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/config/*.cpp")
file(GLOB SDL_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/sdl/*.cpp")

add_executable(ktvlv
  src/main.cpp
  ${UI_SRC}
  ${SERVICE_SRC}
  ${EVENT_SRC}
  ${LOGGING_SRC}
  ${CONFIG_SRC}
  ${SDL_SRC}
)

target_include_directories(ktvlv PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${lvgl_SOURCE_DIR}
  ${plog_SOURCE_DIR}/include
  ${moodycamel_SOURCE_DIR}
  ${inih_SOURCE_DIR}
  ${cjson_SOURCE_DIR}
)

target_compile_definitions(ktvlv PRIVATE
  LV_CONF_INCLUDE_SIMPLE
  LV_LVGL_H_INCLUDE_SIMPLE
)

# MSVC 源码统一按 UTF-8 编译，避免中文字符串编码问题
if (MSVC)
  target_compile_options(ktvlv PRIVATE /utf-8)
  target_compile_definitions(ktvlv PRIVATE
    _CRT_SECURE_NO_WARNINGS
  )
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


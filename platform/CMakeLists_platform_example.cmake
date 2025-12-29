# ============================================================
# 跨平台驱动架构 CMake 配置示例
# ============================================================
# 
# 使用方法：
# 1. 将此文件内容合并到主 CMakeLists.txt
# 2. 或通过 include() 包含此文件
# 
# ============================================================

# ------------------------------------------------------------
# 平台选择选项
# ------------------------------------------------------------
option(KTV_PLATFORM_F133_LINUX "Build for F133 Linux platform" OFF)

if(KTV_PLATFORM_F133_LINUX)
    message(STATUS "Platform: F133 Linux")
    add_definitions(-DKTV_PLATFORM_F133_LINUX)
    set(KTV_PLATFORM_NAME "f133_linux")
else()
    message(STATUS "Platform: Windows SDL (default)")
    add_definitions(-DKTV_PLATFORM_WINDOWS_SDL)
    set(KTV_PLATFORM_NAME "windows_sdl")
endif()

# ------------------------------------------------------------
# 平台特定源文件
# ------------------------------------------------------------
if(KTV_PLATFORM_F133_LINUX)
    # F133 Linux 平台实现
    set(PLATFORM_DISPLAY_SRC
        platform/f133_linux/display_fbdev.c
    )
    set(PLATFORM_INPUT_SRC
        platform/f133_linux/input_evdev.c
    )
    set(PLATFORM_AUDIO_SRC
        platform/f133_linux/audio_alsa.c
    )
    
    # F133 可能需要额外的库
    # find_library(ALSA_LIB asound)
    # target_link_libraries(ktvlv PRIVATE ${ALSA_LIB})
    
else()
    # Windows SDL 平台实现
    set(PLATFORM_DISPLAY_SRC
        platform/windows_sdl/display_sdl.c
    )
    set(PLATFORM_INPUT_SRC
        platform/windows_sdl/input_sdl.c
    )
    set(PLATFORM_AUDIO_SRC
        platform/windows_sdl/audio_stub.c
    )
    
    # SDL2 依赖（Windows）
    find_package(SDL2 REQUIRED)
    target_link_libraries(ktvlv PRIVATE
        SDL2::SDL2
        SDL2::SDL2main
    )
endif()

# ------------------------------------------------------------
# 核心应用入口（跨平台）
# ------------------------------------------------------------
set(CORE_SRC
    core/app_main.c
)

# ------------------------------------------------------------
# 添加到目标
# ------------------------------------------------------------
target_sources(ktvlv PRIVATE
    ${PLATFORM_DISPLAY_SRC}
    ${PLATFORM_INPUT_SRC}
    ${PLATFORM_AUDIO_SRC}
    ${CORE_SRC}
)

# ------------------------------------------------------------
# 包含目录
# ------------------------------------------------------------
target_include_directories(ktvlv PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers
    ${CMAKE_CURRENT_SOURCE_DIR}/platform
    ${CMAKE_CURRENT_SOURCE_DIR}/core
)

# ------------------------------------------------------------
# 平台特定编译选项
# ------------------------------------------------------------
if(KTV_PLATFORM_F133_LINUX)
    # F133 Linux 特定选项
    # 例如：交叉编译工具链设置
    # set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
    # set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)
endif()


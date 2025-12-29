/**
 * @file platform_config.h
 * @brief 平台选择配置
 * 
 * 通过编译时定义来选择平台：
 *   - Windows SDL: 默认或定义 KTV_PLATFORM_WINDOWS_SDL
 *   - F133 Linux: 定义 KTV_PLATFORM_F133_LINUX
 */

#ifndef KTVLV_PLATFORM_PLATFORM_CONFIG_H
#define KTVLV_PLATFORM_PLATFORM_CONFIG_H

// 平台选择（通过 CMake 或编译器定义）
#if defined(KTV_PLATFORM_F133_LINUX)
    #define KTV_PLATFORM_F133
#elif defined(_WIN32) || !defined(KTV_PLATFORM_F133_LINUX)
    #define KTV_PLATFORM_WINDOWS_SDL
#endif

// 平台特定包含
#ifdef KTV_PLATFORM_WINDOWS_SDL
    // Windows SDL 平台实现
    // 注意：在对应的 .c 文件中包含，这里只做说明
#elif defined(KTV_PLATFORM_F133)
    // F133 Linux 平台实现
    // 注意：在对应的 .c 文件中包含，这里只做说明
#endif

#endif  // KTVLV_PLATFORM_PLATFORM_CONFIG_H


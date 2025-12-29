/**
 * @file audio_driver.h
 * @brief 音频驱动抽象接口（跨平台统一接口）
 * 
 * 注意：F133 上音频可能由播放器层直接处理，此接口主要用于系统音效
 */

#ifndef KTVLV_DRIVERS_AUDIO_DRIVER_H
#define KTVLV_DRIVERS_AUDIO_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 音频驱动接口结构体
 */
typedef struct {
    /**
     * @brief 初始化音频驱动
     * @return true 成功, false 失败
     */
    int (*init)(void);
    
    /**
     * @brief 播放系统音效（可选）
     * @param sound_id 音效ID（平台定义）
     * @return true 成功, false 失败
     */
    bool (*play_sound)(uint32_t sound_id);
    
    /**
     * @brief 反初始化音频驱动
     */
    void (*deinit)(void);
} audio_iface_t;

/**
 * @brief 全局音频驱动接口实例
 * 
 * 在平台特定实现中定义并初始化
 * 注意：F133 上可能为 stub（空实现）
 */
extern audio_iface_t AUDIO;

#ifdef __cplusplus
}
#endif

#endif  // KTVLV_DRIVERS_AUDIO_DRIVER_H


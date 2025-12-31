/**
 * @file audio_driver.h
 * @brief 音频驱动抽象接口（跨平台统一接口）
 * 
 * 注意：
 * - F133 上播放器音频由 TPlayer 直接处理，此接口主要用于系统音效和录音
 * - 如果不需要系统音效和录音，可以保持为 stub 实现
 * - 未来功能：语音点歌、唱歌打分等需要录音功能
 */

#ifndef KTVLV_DRIVERS_AUDIO_DRIVER_H
#define KTVLV_DRIVERS_AUDIO_DRIVER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 录音回调函数类型
 * @param data 录音数据缓冲区
 * @param size 数据大小（字节）
 * @param user_data 用户数据
 * @return true 继续录音, false 停止录音
 */
typedef bool (*audio_record_callback_t)(const void* data, size_t size, void* user_data);

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
     * @brief 开始录音（未来功能：语音点歌、唱歌打分）
     * @param sample_rate 采样率（如 44100, 16000）
     * @param channels 声道数（1=单声道, 2=立体声）
     * @param format 采样格式（0=S16_LE, 1=S32_LE等）
     * @param callback 录音数据回调函数（每帧数据回调）
     * @param user_data 用户数据（传递给回调函数）
     * @return true 成功, false 失败
     */
    bool (*start_record)(int sample_rate, int channels, int format,
                        audio_record_callback_t callback, void* user_data);
    
    /**
     * @brief 停止录音
     * @return true 成功, false 失败
     */
    bool (*stop_record)(void);
    
    /**
     * @brief 检查是否正在录音
     * @return true 正在录音, false 未录音
     */
    bool (*is_recording)(void);
    
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



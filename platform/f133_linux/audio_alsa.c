/**
 * @file audio_alsa.c
 * @brief F133 Linux 平台音频驱动实现（ALSA，可选）
 * 
 * 注意：F133 上音频可能由播放器层直接处理，此接口主要用于系统音效
 * 如果不需要系统音效，可以保持为 stub 实现
 */

#include "drivers/audio_driver.h"
#include <stdio.h>

static int audio_alsa_init(void) {
    // TODO: 初始化 ALSA（如果需要系统音效）
    fprintf(stderr, "[ALSA] Audio driver (stub) initialized\n");
    return 1;
}

static bool audio_alsa_play_sound(uint32_t sound_id) {
    (void)sound_id;
    // TODO: 播放系统音效（如果需要）
    return true;
}

static void audio_alsa_deinit(void) {
    // TODO: 清理 ALSA 资源（如果需要）
    fprintf(stderr, "[ALSA] Audio driver (stub) deinitialized\n");
}

// 导出接口实例
audio_iface_t AUDIO = {
    .init = audio_alsa_init,
    .play_sound = audio_alsa_play_sound,
    .deinit = audio_alsa_deinit
};


/**
 * @file audio_stub.c
 * @brief Windows SDL 平台音频驱动（stub 实现）
 * 
 * 注意：Windows 仿真阶段音频可能由播放器层处理，此接口为空实现
 */

#include "drivers/audio_driver.h"
#include <stdio.h>

static int audio_sdl_init(void) {
    fprintf(stderr, "[SDL] Audio driver (stub) initialized\n");
    return 1;
}

static bool audio_sdl_play_sound(uint32_t sound_id) {
    (void)sound_id;
    return true;
}

static void audio_sdl_deinit(void) {
    fprintf(stderr, "[SDL] Audio driver (stub) deinitialized\n");
}

// 导出接口实例
audio_iface_t AUDIO = {
    .init = audio_sdl_init,
    .play_sound = audio_sdl_play_sound,
    .deinit = audio_sdl_deinit
};


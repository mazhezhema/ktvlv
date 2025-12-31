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

// 录音功能 stub（Windows SDL 平台不需要录音）
static bool audio_sdl_start_record(int sample_rate, int channels, int format,
                                    audio_record_callback_t callback, void* user_data) {
    (void)sample_rate; (void)channels; (void)format;
    (void)callback; (void)user_data;
    fprintf(stderr, "[SDL] Recording not supported on Windows SDL platform\n");
    return false;
}

static bool audio_sdl_stop_record(void) {
    return false;
}

static bool audio_sdl_is_recording(void) {
    return false;
}

static void audio_sdl_deinit(void) {
    fprintf(stderr, "[SDL] Audio driver (stub) deinitialized\n");
}

// 导出接口实例
audio_iface_t AUDIO = {
    .init = audio_sdl_init,
    .play_sound = audio_sdl_play_sound,
    .start_record = audio_sdl_start_record,
    .stop_record = audio_sdl_stop_record,
    .is_recording = audio_sdl_is_recording,
    .deinit = audio_sdl_deinit
};



/**
 * @file audio_alsa.c
 * @brief F133 Linux 平台音频驱动实现（ALSA）
 * 
 * 功能：
 * - 系统音效播放（可选，当前为 stub）
 * - 录音功能（未来：语音点歌、唱歌打分）
 * 
 * 注意：
 * - 播放器音频由 TPlayer 直接处理，不经过此接口
 * - 如果不需要系统音效，play_sound 保持为 stub
 * - 录音功能需要 ALSA 库（libasound2）
 */

#include "drivers/audio_driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#ifdef KTV_USE_ALSA_RECORD
#include <alsa/asoundlib.h>

// 录音相关状态
static snd_pcm_t *capture_handle = NULL;
static bool is_recording = false;
static audio_record_callback_t record_callback = NULL;
static void* record_user_data = NULL;
static pthread_t record_thread = 0;
static int record_sample_rate = 16000;
static int record_channels = 1;
static snd_pcm_format_t record_format = SND_PCM_FORMAT_S16_LE;

// 录音线程函数
static void* record_thread_func(void* arg) {
    snd_pcm_uframes_t frames = 1024;
    size_t frame_size = snd_pcm_format_physical_width(record_format) / 8 * record_channels;
    char *buffer = malloc(frames * frame_size);
    
    if (!buffer) {
        fprintf(stderr, "[ALSA] Failed to allocate record buffer\n");
        return NULL;
    }
    
    while (is_recording) {
        snd_pcm_sframes_t read_frames = snd_pcm_readi(capture_handle, buffer, frames);
        if (read_frames < 0) {
            // 处理错误（如 underrun）
            if (read_frames == -EPIPE) {
                snd_pcm_prepare(capture_handle);
            } else {
                fprintf(stderr, "[ALSA] Read error: %s\n", snd_strerror(read_frames));
                break;
            }
        } else if (read_frames > 0) {
            // 调用回调函数处理录音数据
            if (record_callback) {
                size_t data_size = read_frames * frame_size;
                if (!record_callback(buffer, data_size, record_user_data)) {
                    // 回调返回 false，停止录音
                    break;
                }
            }
        }
    }
    
    free(buffer);
    return NULL;
}
#endif

static int audio_alsa_init(void) {
    fprintf(stderr, "[ALSA] Audio driver initialized\n");
    return 1;
}

static bool audio_alsa_play_sound(uint32_t sound_id) {
    (void)sound_id;
    // TODO: 播放系统音效（如果需要）
    // 当前为 stub 实现，因为所有播放由 TPlayer 处理
    return true;
}

#ifdef KTV_USE_ALSA_RECORD
static bool audio_alsa_start_record(int sample_rate, int channels, int format,
                                    audio_record_callback_t callback, void* user_data) {
    if (is_recording) {
        fprintf(stderr, "[ALSA] Already recording\n");
        return false;
    }
    
    // 打开录音设备（默认使用 hw:0,0，可根据实际调整）
    int err = snd_pcm_open(&capture_handle, "hw:0,0", SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        fprintf(stderr, "[ALSA] Failed to open capture device: %s\n", snd_strerror(err));
        return false;
    }
    
    // 设置硬件参数
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_any(capture_handle, hw_params);
    snd_pcm_hw_params_set_access(capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    
    // 设置格式
    record_format = (format == 0) ? SND_PCM_FORMAT_S16_LE : SND_PCM_FORMAT_S32_LE;
    snd_pcm_hw_params_set_format(capture_handle, hw_params, record_format);
    
    // 设置采样率
    unsigned int rate = sample_rate;
    snd_pcm_hw_params_set_rate_near(capture_handle, hw_params, &rate, 0);
    record_sample_rate = rate;
    
    // 设置声道数
    snd_pcm_hw_params_set_channels(capture_handle, hw_params, channels);
    record_channels = channels;
    
    // 应用参数
    err = snd_pcm_hw_params(capture_handle, hw_params);
    if (err < 0) {
        fprintf(stderr, "[ALSA] Failed to set hw params: %s\n", snd_strerror(err));
        snd_pcm_close(capture_handle);
        capture_handle = NULL;
        return false;
    }
    
    // 准备录音
    err = snd_pcm_prepare(capture_handle);
    if (err < 0) {
        fprintf(stderr, "[ALSA] Failed to prepare: %s\n", snd_strerror(err));
        snd_pcm_close(capture_handle);
        capture_handle = NULL;
        return false;
    }
    
    // 保存回调函数
    record_callback = callback;
    record_user_data = user_data;
    is_recording = true;
    
    // 启动录音线程
    if (pthread_create(&record_thread, NULL, record_thread_func, NULL) != 0) {
        fprintf(stderr, "[ALSA] Failed to create record thread\n");
        snd_pcm_close(capture_handle);
        capture_handle = NULL;
        is_recording = false;
        return false;
    }
    
    fprintf(stderr, "[ALSA] Recording started: %dHz, %dch, format=%d\n",
            record_sample_rate, record_channels, format);
    return true;
}

static bool audio_alsa_stop_record(void) {
    if (!is_recording) {
        return false;
    }
    
    is_recording = false;
    
    // 等待录音线程结束
    if (record_thread) {
        pthread_join(record_thread, NULL);
        record_thread = 0;
    }
    
    // 关闭设备
    if (capture_handle) {
        snd_pcm_close(capture_handle);
        capture_handle = NULL;
    }
    
    record_callback = NULL;
    record_user_data = NULL;
    
    fprintf(stderr, "[ALSA] Recording stopped\n");
    return true;
}

static bool audio_alsa_is_recording(void) {
    return is_recording;
}
#else
// 未启用录音功能时的 stub 实现
static bool audio_alsa_start_record(int sample_rate, int channels, int format,
                                    audio_record_callback_t callback, void* user_data) {
    (void)sample_rate; (void)channels; (void)format;
    (void)callback; (void)user_data;
    fprintf(stderr, "[ALSA] Recording not enabled (compile with KTV_USE_ALSA_RECORD)\n");
    return false;
}

static bool audio_alsa_stop_record(void) {
    return false;
}

static bool audio_alsa_is_recording(void) {
    return false;
}
#endif

static void audio_alsa_deinit(void) {
    // 如果正在录音，先停止
    if (is_recording) {
        audio_alsa_stop_record();
    }
    fprintf(stderr, "[ALSA] Audio driver deinitialized\n");
}

// 导出接口实例
audio_iface_t AUDIO = {
    .init = audio_alsa_init,
    .play_sound = audio_alsa_play_sound,
    .start_record = audio_alsa_start_record,
    .stop_record = audio_alsa_stop_record,
    .is_recording = audio_alsa_is_recording,
    .deinit = audio_alsa_deinit
};



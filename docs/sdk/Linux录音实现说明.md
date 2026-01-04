# Linux 上录音（收音）实现说明

> **最后更新**: 2025-12-30

## ❓ Linux 上录音如何实现？

在 Linux 上，录音（收音）主要通过 **ALSA (Advanced Linux Sound Architecture)** 实现。

---

## 🎯 录音实现方式

### 方式1: 使用 ALSA API（编程方式）⭐ **推荐**

**ALSA 录音流程**：

```c
#include <alsa/asoundlib.h>

// 1. 打开录音设备
snd_pcm_t *capture_handle;
snd_pcm_open(&capture_handle, "hw:0,0", SND_PCM_STREAM_CAPTURE, 0);

// 2. 设置参数（采样率、声道、格式）
snd_pcm_hw_params_t *hw_params;
snd_pcm_hw_params_alloca(&hw_params);
snd_pcm_hw_params_any(capture_handle, hw_params);
snd_pcm_hw_params_set_access(capture_handle, hw_params, 
                              SND_PCM_ACCESS_RW_INTERLEAVED);
snd_pcm_hw_params_set_format(capture_handle, hw_params, 
                              SND_PCM_FORMAT_S16_LE);
snd_pcm_hw_params_set_rate_near(capture_handle, hw_params, &rate, 0);
snd_pcm_hw_params_set_channels(capture_handle, hw_params, 2);
snd_pcm_hw_params(capture_handle, hw_params);

// 3. 分配缓冲区
char *buffer;
snd_pcm_hw_params_get_period_size(hw_params, &frames, 0);
buffer = malloc(frames * channels * snd_pcm_format_physical_width(format) / 8);

// 4. 开始录音
snd_pcm_prepare(capture_handle);
while (recording) {
    snd_pcm_readi(capture_handle, buffer, frames);
    // 处理录音数据（保存到文件、实时处理等）
}

// 5. 关闭设备
snd_pcm_close(capture_handle);
```

### 方式2: 使用命令行工具（测试/调试）

**arecord（ALSA 工具）**：
```bash
# 列出录音设备
arecord -l

# 录音（16位，44.1kHz，立体声，10秒）
arecord -D hw:0,0 -f S16_LE -r 44100 -c 2 -d 10 test.wav

# 录音（指定格式）
arecord -D hw:0,0 -f cd -t wav -d 10 test.wav
```

**tinycap（Tina Linux 工具）**：
```bash
# 使用声卡0，单声道，录音10秒
tinycap mic.wav -D 0 -c 1 -T 10

# 双声道录音
tinycap mic.wav -D 0 -c 2 -T 10
```

---

## 🎤 F133 平台录音实现

### F133 支持的录音方式

根据 F133 SDK 文档，支持以下录音方式：

1. **MIC 输入**（模拟麦克风）
   - MIC1、MIC2、MIC3
   - 单通道或双通道录音

2. **DMIC 输入**（数字麦克风）
   - 数字麦克风接口
   - 支持多路同步录音

3. **LINEIN 输入**（线路输入）
   - 外部音频设备输入

### F133 录音示例

**MIC1 单通道录音**：
```bash
# 设置录音通路
tinymix -D 0 "MIC1 Switch" 1
tinymix -D 0 "Input1 Mux" 0

# 开始录音
tinycap mic.wav -D 0 -c 1 -T 10
```

**MIC 双通道录音**：
```bash
# 设置录音通路（mic1 & mic2）
tinymix -D 0 "MIC1 Switch" 1
tinymix -D 0 "MIC2 Switch" 1
tinymix -D 0 "Input1 Mux" 0
tinymix -D 0 "Input2 Mux" 0

# 开始录音
tinycap mic.wav -D 0 -c 2 -T 10
```

---

## 🔧 在 KTV 应用中实现录音

### 是否需要录音功能？

**KTV 应用可能的录音需求**：
1. ✅ **用户唱歌录音** - 录制用户唱歌，保存或上传
2. ⚠️ **语音搜索** - 语音输入搜索歌曲（可选）
3. ❌ **系统音效** - 不需要（由 TPlayer 处理）

### 如果不需要录音功能

**当前实现**：保持 `audio_alsa.c` 为 stub 实现即可，不需要添加录音接口。

### 如果需要录音功能

**需要扩展 `audio_driver.h` 接口**：

```c
// drivers/audio_driver.h
typedef struct {
    int (*init)(void);
    bool (*play_sound)(uint32_t sound_id);  // 播放（可选）
    
    // 录音接口（如果需要）
    bool (*start_record)(const char* filepath, int sample_rate, int channels);
    bool (*stop_record)(void);
    bool (*is_recording)(void);
    
    void (*deinit)(void);
} audio_iface_t;
```

**F133 平台实现**：

```c
// platform/f133_linux/audio_alsa.c
#include <alsa/asoundlib.h>

static snd_pcm_t *capture_handle = NULL;
static bool is_recording = false;

static bool audio_alsa_start_record(const char* filepath, 
                                    int sample_rate, int channels) {
    if (is_recording) return false;
    
    // 打开录音设备
    int err = snd_pcm_open(&capture_handle, "hw:0,0", 
                          SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) return false;
    
    // 设置参数
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_any(capture_handle, hw_params);
    snd_pcm_hw_params_set_access(capture_handle, hw_params, 
                                  SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(capture_handle, hw_params, 
                                  SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_rate_near(capture_handle, hw_params, 
                                     &sample_rate, 0);
    snd_pcm_hw_params_set_channels(capture_handle, hw_params, channels);
    snd_pcm_hw_params(capture_handle, hw_params);
    
    // 打开文件准备写入
    FILE *file = fopen(filepath, "wb");
    if (!file) {
        snd_pcm_close(capture_handle);
        return false;
    }
    
    // 开始录音（在后台线程中）
    is_recording = true;
    // ... 录音循环 ...
    
    return true;
}

static bool audio_alsa_stop_record(void) {
    if (!is_recording) return false;
    is_recording = false;
    if (capture_handle) {
        snd_pcm_close(capture_handle);
        capture_handle = NULL;
    }
    return true;
}
```

---

## 📊 录音 vs 播放对比

| 功能 | 播放 | 录音 |
|------|------|------|
| **用途** | 播放音频（歌曲、MV） | 录制音频（用户唱歌） |
| **实现方式** | TPlayer（播放器） | ALSA API |
| **KTV 应用需求** | ✅ 必需 | ⚠️ 可选 |
| **当前实现** | TPlayer 处理 | ❌ 未实现（stub） |

---

## 🎯 总结

### Linux 上录音实现方式

1. **ALSA API** - 编程方式，使用 `snd_pcm_*` 函数
   - 需要链接 `libasound2` 库
   - 使用 `snd_pcm_open()`、`snd_pcm_readi()` 等函数

2. **arecord** - 命令行工具，用于测试
   - ALSA 工具包提供
   - 适合快速测试录音功能

3. **tinycap** - Tina Linux 工具，F133 平台专用
   - F133 SDK 提供
   - 适合 F133 平台测试

### KTV 应用是否需要录音？

**如果不需要录音功能**（推荐）：
- ✅ 保持当前实现（stub）
- ✅ 不需要添加录音接口
- ✅ 所有音频由 TPlayer 处理
- ✅ 不需要 ALSA 库

**如果需要录音功能**（如用户唱歌录音）：
- ⚠️ 需要扩展 `audio_driver.h` 接口，添加录音函数
- ⚠️ 实现 ALSA 录音 API（`snd_pcm_*`）
- ⚠️ 需要链接 ALSA 库（`libasound2`）
- ⚠️ 需要配置录音设备（MIC1/MIC2/DMIC）

### 推荐方案

**当前版本（MVP）**：
- ✅ **不需要录音功能** - 保持当前 stub 实现
- ✅ **所有音频由 TPlayer 处理** - 播放、音轨切换、音量控制
- ✅ **简化架构** - 避免多路音频处理的复杂性

**未来版本（v2.0+）**：
- ⚠️ **语音点歌功能** - 需要录音功能
- ⚠️ **唱歌打分功能** - 需要录音功能
- ✅ **接口已扩展** - `audio_driver.h` 已添加录音接口
- ✅ **实现方案** - 详见 [语音点歌与唱歌打分功能设计.md](./语音点歌与唱歌打分功能设计.md)

---

## 📚 相关文档

- **ALSA 说明**: [ALSA说明.md](./ALSA说明.md)
- **F133 平台库清单**: [F133平台库清单.md](./F133平台库清单.md)
- **F133 SDK 文档**: `SDK模块开发指南_Linux_Audio_开发指南.pdf`

---

**最后更新**: 2025-12-30


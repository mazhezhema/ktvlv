# F133平台KTV应用移植方案总结

> **文档版本**：v1.0  
> **相关文档**：详见 [项目架构设计总览.md](./项目架构设计总览.md)  
> **实现文档**：详见 [C++架构设计-预分配内存版本.md](./C++架构设计-预分配内存版本.md)  
> **开源库选型**：详见 [开源库选型指南.md](./开源库选型指南.md)

## 一、SDK文档关键信息提取

### 1.1 F133平台多媒体格式支持

根据《Tina_Linux_各平台多媒体格式_支持列表.pdf》：

**F133平台支持：**
- **流媒体协议**：`http、https、hls` ✅
- **音频封装格式**：`m3u8` ✅（在R853/T113等平台明确列出）
- **视频解码**：H.264, H.265, MPEG1/2/4, MJPEG等
- **音频解码**：mp3、ogg、flac、ape、aac、amr、wav、alac、atrac
- **封装格式**：mkv、avi、mp4/m4v、mov、vob、pmp、mpg、flv

**重要发现**：F133平台明确支持HLS流媒体协议，这意味着可以直接播放m3u8格式的HLS流。

### 1.2 TPlayer播放器API（全志官方播放中间件）

根据《Tina_Linux_多媒体解码_开发指南.pdf》：

**核心播放控制API：**
```c
// 创建/销毁
TPlayer* TPlayerCreate();
void TPlayerDestroy(TPlayer* p);

// 数据源设置
int TPlayerSetDataSource(TPlayer* p, const char* pUrl, ...);

// 播放控制
int TPlayerPrepare(TPlayer* p);
int TPlayerStart(TPlayer* p);
int TPlayerPause(TPlayer* p);
int TPlayerStop(TPlayer* p);
int TPlayerSeekTo(TPlayer* p, int msec);

// 音轨切换（关键！）✅
int TPlayerSwitchAudio(TPlayer* p, int nStreamIndex);
int TPlayerGetAudioTrack(TPlayer* p);  // 获取当前音轨序号

// 音量控制
int TPlayerSetVolume(TPlayer* p, float left, float right);
int TPlayerGetVolume(TPlayer* p, float* left, float* right);
int TPlayerSetAudioMute(TPlayer* p, int mute);

// 视频显示控制
int TPlayerSetDisplayRect(TPlayer* p, int x, int y, int w, int h);
int TPlayerSetVideoDisplay(TPlayer* p, int bShow);
```

**关键接口说明：**
- `TPlayerSwitchAudio(p, nStreamIndex)` - 切换音轨，`nStreamIndex`为音轨索引（0=原唱，1=伴奏等）
- `TPlayerGetAudioTrack(p)` - 获取当前播放的音轨序号

### 1.3 显示系统架构

根据《Linux_Display_开发指南.pdf》：

- **显示引擎（DE）**：支持多图层叠加混合
- **Framebuffer驱动**：标准Linux framebuffer接口
- **图层系统**：支持UI通道（RGB图层）和视频通道叠加
- **显示输出**：支持LCD、HDMI等多种输出设备

### 1.4 音频系统

根据《Linux_Audio_开发指南.pdf》：
- 支持ALSA音频框架
- 支持I2S、HDMI音频输出
- 支持多路音频混音

## 二、推荐实现方案

### 2.1 媒体播放层

**使用TPlayer播放器（全志官方推荐）**

**优势：**
1. ✅ 官方支持，稳定可靠
2. ✅ 支持HLS/m3u8流媒体协议
3. ✅ 内置硬件解码（VE引擎）
4. ✅ 提供音轨切换API（`TPlayerSwitchAudio`）
5. ✅ 支持音量控制、暂停/继续等基础功能

**实现流程：**
```c
// 1. 创建播放器
TPlayer* player = TPlayerCreate();

// 2. 设置数据源（m3u8 URL）
TPlayerSetDataSource(player, "http://example.com/song.m3u8", ...);

// 3. 准备播放
TPlayerPrepare(player);

// 4. 开始播放
TPlayerStart(player);

// 5. 切换音轨（原唱/伴奏）
TPlayerSwitchAudio(player, 0);  // 切换到音轨0（原唱）
TPlayerSwitchAudio(player, 1);  // 切换到音轨1（伴奏）

// 6. 播放控制
TPlayerPause(player);   // 暂停
TPlayerStart(player);   // 继续
TPlayerStop(player);    // 停止
```

### 2.2 UI层（LVGL）

**MVP版本UI布局：**
```
┌─────────────────────────────────────────────────────┐
│  顶部菜单栏（固定）                                    │
│  [首页] [历史记录]                                     │
├─────────────────────────────────────────────────────┤
│                                                       │
│  主内容区（根据菜单切换）                              │
│  - 首页：歌曲列表/分类/排行榜                          │
│  - 历史记录：最近50首播放记录                          │
│                                                       │
│  [搜索框] [虚拟键盘]                                  │
│                                                       │
├─────────────────────────────────────────────────────┤
│  底部控制栏（固定）                                    │
│  [已点] [切歌] [伴唱] [暂停] [重唱] [调音] [设置] [返回]│
└─────────────────────────────────────────────────────┘
```

**架构设计：**
```
┌─────────────────────────────────┐
│      LVGL UI层（软件渲染）        │
│  - 顶部菜单栏（首页/历史记录）     │
│  - 歌曲列表、搜索界面              │
│  - 播放控制按钮                    │
│  - 歌词显示（可选）                │
└─────────────────────────────────┘
           ↓
┌─────────────────────────────────┐
│    TPlayer播放器（硬件解码）      │
│  - 视频输出到Display Engine       │
│  - 音频输出到ALSA                 │
└─────────────────────────────────┘
           ↓
┌─────────────────────────────────┐
│    硬件层                        │
│  - VE（视频引擎）                 │
│  - Display Engine（显示引擎）     │
│  - ALSA音频输出                  │
└─────────────────────────────────┘
```

**关键点：**
1. **视频显示**：TPlayer将解码后的视频直接输出到Display Engine的视频图层
2. **UI叠加**：LVGL渲染的UI通过framebuffer输出到Display Engine的UI图层
3. **图层合成**：Display Engine自动将视频图层和UI图层叠加显示
4. **历史记录实现**：使用FIFO队列，最多保存50首，新播放的歌曲自动添加到队列头部，超过50首时删除最旧的记录

### 2.3 多音轨切换实现

**m3u8文件结构要求：**
```m3u8
#EXTM3U
#EXT-X-VERSION:3
#EXT-X-STREAM-INF:BANDWIDTH=2000000,RESOLUTION=1280x720
video.m3u8?video=1&audio=0    # 视频+原唱音轨
#EXT-X-STREAM-INF:BANDWIDTH=2000000,RESOLUTION=1280x720
video.m3u8?video=1&audio=1    # 视频+伴奏音轨
```

或者使用HLS多音轨标准：
```m3u8
#EXTM3U
#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID="audio",NAME="原唱",URI="audio_original.m3u8"
#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID="audio",NAME="伴奏",URI="audio_accompaniment.m3u8"
#EXT-X-STREAM-INF:AUDIO="audio"
video.m3u8
```

**切换逻辑：**
```c
// 获取当前音轨
int current_track = TPlayerGetAudioTrack(player);

// 切换到伴奏（假设音轨1是伴奏）
if (current_track == 0) {
    TPlayerSwitchAudio(player, 1);  // 切换到伴奏
} else {
    TPlayerSwitchAudio(player, 0);  // 切换回原唱
}
```

### 2.4 MVP版本功能清单

**顶部菜单栏（简化版）：**
- ✅ **首页** - 主界面（歌曲浏览、分类、排行榜）
- ✅ **历史记录** - 显示最近播放的50首歌曲（默认，最多50首）
- ❌ ~~猜你喜欢~~ - 已移除
- ❌ ~~我的收藏~~ - 已移除
- ❌ ~~VIP会员中心~~ - 已移除

**保留的核心功能：**
- ✅ 歌曲浏览（列表、分类、排行榜）
- ✅ 搜索（歌名、歌手）
- ✅ 点歌（添加到播放列表）
- ✅ 播放控制（播放、暂停、切歌、重唱）
- ✅ **音轨切换（原唱/伴奏）** - 使用`TPlayerSwitchAudio`
- ✅ 音量控制 - 使用`TPlayerSetVolume`
- ✅ 播放进度显示
- ✅ **历史记录** - 自动记录最近播放的50首歌曲（FIFO队列）

**移除的功能：**
- ❌ VIP会员中心
- ❌ 微信扫码点歌
- ❌ 猜你喜欢（推荐算法）
- ❌ 我的收藏

## 三、开发步骤建议

### 阶段1：环境搭建
1. 配置Tina Linux SDK，选中`tplayer`和`tplayerdemo`模块
2. 编译并烧录到F133开发板
3. 测试`tplayerdemo`播放m3u8文件

### 阶段2：播放器集成
1. 集成TPlayer库到项目
2. 实现基础的播放控制（播放、暂停、停止）
3. 测试音轨切换功能

### 阶段3：LVGL UI开发
1. 搭建LVGL开发环境
2. 实现顶部菜单栏（首页/历史记录两个Tab）
3. 实现首页界面（歌曲列表、分类、排行榜）
4. 实现历史记录界面（显示最近50首，FIFO队列）
5. 实现搜索界面
6. 实现播放控制界面（底部固定控制栏）

### 阶段4：功能整合
1. UI调用TPlayer API
2. 实现播放列表管理（已点歌曲队列）
3. 实现音轨切换按钮（伴唱按钮）
4. 实现历史记录功能（自动记录，最多50首）
5. 实现菜单切换逻辑（首页 ↔ 历史记录）
6. 测试完整流程

### 阶段5：优化与测试
1. 性能优化
2. 内存优化
3. 稳定性测试
4. 弱网环境测试

## 四、关键技术要点

### 4.1 m3u8多音轨支持
- 确保m3u8文件正确声明多个音轨
- 使用`TPlayerGetAudioTrack`获取可用音轨数量
- 使用`TPlayerSwitchAudio`切换音轨

### 4.2 显示叠加
- TPlayer视频输出到Display Engine的视频图层
- LVGL UI输出到framebuffer（UI图层）
- Display Engine自动合成显示

### 4.3 音频输出
- TPlayer音频输出到ALSA
- 支持音量控制和静音
- 支持多路音频混音（如需要）

### 4.4 性能优化
- 使用硬件解码（VE引擎）
- LVGL使用软件渲染（或G2D加速，如果支持）
- 合理设置视频缓冲大小

## 五、参考资源

1. **SDK文档位置**：
   - `Tina_Linux_多媒体解码_开发指南.pdf`
   - `Tina_Linux_各平台多媒体格式_支持列表.pdf`
   - `Linux_Display_开发指南.pdf`
   - `Linux_Audio_开发指南.pdf`

2. **示例代码**：
   - `package/allwinner/tina_multimedia_demo/tplayerdemo` - TPlayer使用示例

3. **测试工具**：
   - `tplayerdemo` - 命令行播放器测试工具

## 六、MVP版本实现细节

### 6.1 历史记录功能实现

**数据结构：**
```c
#define MAX_HISTORY_COUNT 50

typedef struct {
    char song_id[64];
    char song_name[128];
    char artist[128];
    char m3u8_url[256];
    time_t play_time;
} HistoryItem;

typedef struct {
    HistoryItem items[MAX_HISTORY_COUNT];
    int count;  // 当前记录数（最多50）
    int head;   // 队列头部索引（用于FIFO）
} HistoryQueue;
```

**实现逻辑：**
```c
// 添加播放记录（FIFO队列）
void add_to_history(HistoryQueue* queue, const char* song_id, 
                    const char* song_name, const char* artist, 
                    const char* m3u8_url) {
    if (queue->count < MAX_HISTORY_COUNT) {
        // 队列未满，添加到末尾
        int index = queue->count;
        strcpy(queue->items[index].song_id, song_id);
        strcpy(queue->items[index].song_name, song_name);
        strcpy(queue->items[index].artist, artist);
        strcpy(queue->items[index].m3u8_url, m3u8_url);
        queue->items[index].play_time = time(NULL);
        queue->count++;
    } else {
        // 队列已满，覆盖最旧的记录（FIFO）
        int index = queue->head;
        strcpy(queue->items[index].song_id, song_id);
        strcpy(queue->items[index].song_name, song_name);
        strcpy(queue->items[index].artist, artist);
        strcpy(queue->items[index].m3u8_url, m3u8_url);
        queue->items[index].play_time = time(NULL);
        queue->head = (queue->head + 1) % MAX_HISTORY_COUNT;
    }
}

// 保存历史记录到本地文件（可选）
void save_history_to_file(HistoryQueue* queue, const char* filename);

// 从本地文件加载历史记录（可选）
void load_history_from_file(HistoryQueue* queue, const char* filename);
```

### 6.2 顶部菜单栏实现

**LVGL实现示例：**
```c
// 创建顶部Tab栏
lv_obj_t* tabview = lv_tabview_create(lv_scr_act(), LV_DIR_TOP, 50);
lv_obj_t* tab_home = lv_tabview_add_tab(tabview, "首页");
lv_obj_t* tab_history = lv_tabview_add_tab(tabview, "历史记录");

// 首页内容
lv_obj_t* song_list = lv_list_create(tab_home);
// ... 添加歌曲列表项

// 历史记录内容
lv_obj_t* history_list = lv_list_create(tab_history);
// ... 从HistoryQueue加载并显示历史记录
```

## 七、注意事项

1. **音轨切换时机**：建议在播放过程中切换，避免在Prepare阶段切换
2. **回调函数**：注意不要在TPlayer的回调函数中调用TPlayer的其他接口
3. **资源释放**：播放结束后及时调用`TPlayerDestroy`释放资源
4. **错误处理**：检查所有API的返回值，处理错误情况
5. **线程安全**：TPlayer API可能不是线程安全的，注意多线程访问
6. **历史记录持久化**：建议将历史记录保存到本地文件（如JSON格式），应用重启后自动加载
7. **历史记录排序**：按播放时间倒序显示（最新的在最上面）

---

**总结**：F133平台通过TPlayer播放器可以很好地支持m3u8格式的HLS流媒体播放，并且提供了`TPlayerSwitchAudio`接口用于切换音轨，完全满足KTV应用的原唱/伴奏切换需求。结合LVGL实现UI层，可以构建一个精简高效的KTV应用。


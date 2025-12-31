# KTVLV - F133/Tina Linux KTV 点歌系统

> **轻量级嵌入式播放器方案 | LVGL + TPlayer + libcurl + plog**

[![Platform](https://img.shields.io/badge/Platform-F133%20%2F%20Tina%20Linux-orange)](https://www.allwinnertech.com/)
[![UI](https://img.shields.io/badge/UI-LVGL%208.3.11-blue)](https://lvgl.io/)
[![License](https://img.shields.io/badge/License-Proprietary-red)]()

---

## 🎯 一句话介绍

**UI 单线程 + 播放器串行 + 事件回流 + 后台缓存 + plog 全链路记录**

KTV 点歌系统，基于全志 F133 平台，使用 LVGL 作为 UI 框架，TPlayer 作为播放器，支持 m3u8 流媒体播放、双音轨切换（原唱/伴奏）、历史记录、VIP 会员、微信扫码等功能。

---

## ✨ 核心特性

- ✅ **轻量级架构**：LVGL + TPlayer + libcurl + plog，不引重库
- ✅ **双音轨支持**：原唱/伴奏无缝切换（TPlayerSwitchAudio）
- ✅ **智能缓存**：本地优先，弱网友好，离线可用
- ✅ **并发简单**：4线程 + 2队列，业务层无锁
- ✅ **事件驱动**：Command Down / Event Up，职责清晰
- ✅ **全链路日志**：plog 模块化记录，便于调试和维护

---

## 📋 功能清单（第一期 MVP）

### 包含的功能

#### 顶部菜单栏
- ✅ 首页（歌曲浏览、分类、排行榜）
- ✅ 历史记录（最近50首，FIFO队列）
- ✅ VIP会员中心（会员状态、扫码激活）

#### 核心功能
- ✅ 歌曲浏览、搜索、点歌
- ✅ 播放控制（播放、暂停、切歌、重唱）
- ✅ 音轨切换（原唱/伴奏）
- ✅ 音量控制
- ✅ 播放进度显示
- ✅ 历史记录（自动记录，持久化存储）
- ✅ VIP会员功能（会员状态、VIP标识）
- ✅ 微信扫码点歌（扫码激活、扫码点歌）
- ✅ WebSocket 长连接（实时控制）

### 不包含的功能（第一期暂不做）

- ❌ 我的收藏
- ❌ 猜你喜欢
- ❌ 语音点歌（未来功能，v2.0+）
- ❌ 唱歌打分（未来功能，v2.0+）

---

## 🏗️ 技术架构

### 系统架构

```
┌─────────────────────────────────────────┐
│         应用层（C++ + LVGL）              │
│  - 页面管理系统（SPA单例模式）            │
│  - UI组件（列表、搜索、控制栏）            │
│  - 数据模型和服务层                       │
└─────────────────────────────────────────┘
           ↓
┌─────────────────────────────────────────┐
│      业务逻辑层                          │
│  - PlayerAdapter（播放器适配器）          │
│  - 歌曲服务（SongService）               │
│  - 播放列表管理                          │
│  - 历史记录管理（FIFO队列）               │
└─────────────────────────────────────────┘
           ↓
┌─────────────────────────────────────────┐
│      平台抽象层                          │
│  - 显示驱动（framebuffer）              │
│  - 输入驱动（evdev）                     │
│  - 网络驱动（libcurl）                   │
└─────────────────────────────────────────┘
           ↓
┌─────────────────────────────────────────┐
│      播放器层（TPlayer）                  │
│  - m3u8流媒体播放                        │
│  - 硬件解码（VE引擎）                    │
│  - 音轨切换（原唱/伴奏）                  │
└─────────────────────────────────────────┘
           ↓
┌─────────────────────────────────────────┐
│      硬件层（F133）                       │
│  - Display Engine（显示引擎）            │
│  - VE引擎（视频解码）                     │
│  - ALSA音频输出                          │
└─────────────────────────────────────────┘
```

### 并发架构（4线程 + 2队列）

**世界观**：
- Command Down（命令向下）
- Event Up（事件向上）

**线程**：
1. UI 主线程 - LVGL 渲染
2. 播放器线程 - 串行执行播放器命令
3. 网络线程 - HTTP 请求
4. SDK 内部线程 - TPlayer 回调

**队列**：
1. PlayerCmdQueue - UI/网络 → 播放器线程
2. UiEventQueue - 播放器/网络 → UI 线程

---

## 🚀 快速开始

### 环境要求

- **平台**：全志 F133（ARM Cortex-A7）
- **操作系统**：Tina Linux
- **编译器**：GCC 7.3+（支持 C++17）
- **SDK**：Tina Linux SDK（包含 TPlayer）

### 编译构建

```bash
# 配置 Tina Linux SDK
cd tina-sdk
source build/envsetup.sh
lunch   # 选择 F133 平台

# 编译项目
cd package/ktvlv
make menuconfig  # 配置选项
make
make install
```

### 依赖库

**系统库**（Tina Linux SDK 已包含）：
- TPlayer - 全志官方播放器
- libcurl - HTTP 客户端
- OpenSSL/mbedTLS - TLS 支持（WSS）

**第三方库**（FetchContent 自动获取）：
- LVGL 8.3.11 - UI 框架
- cJSON - JSON 解析
- plog - 日志系统
- inih - 配置解析
- libwebsockets - WebSocket 客户端

---

## 📁 项目结构

```
ktvlv/
├── src/
│   ├── main.cpp              # 主入口
│   ├── player/               # 播放器模块
│   │   ├── player_adapter.h/.cpp       # 播放器适配器
│   │   ├── player_cmd_queue.h/.cpp     # 命令队列
│   │   ├── ui_event_queue.h            # UI事件队列
│   │   ├── ui_dispatcher.h/.cpp        # UI调度器
│   │   ├── player_event.h              # 事件定义
│   │   └── player_cmd.h                # 命令定义
│   ├── ui/                   # UI模块
│   │   ├── page_manager.h/.cpp         # 页面管理器
│   │   └── pages/                      # 各个页面
│   ├── services/             # 服务层
│   │   ├── song_service.h/.cpp         # 歌曲服务
│   │   ├── http_service.h/.cpp         # HTTP服务
│   │   └── history_service.h/.cpp      # 历史记录服务
│   └── drivers/              # 驱动抽象层
│       ├── display_driver.h            # 显示驱动
│       ├── input_driver.h              # 输入驱动
│       └── audio_driver.h              # 音频驱动
├── platform/                 # 平台特定实现
│   ├── f133_linux/           # F133 平台
│   │   ├── display_fbdev.c   # framebuffer 显示
│   │   ├── input_evdev.c     # evdev 输入
│   │   └── audio_alsa.c      # ALSA 音频（stub）
│   └── windows_sdl/          # Windows 仿真（开发用）
├── docs/                     # 文档
│   ├── F133第一期需求文档.md
│   ├── KTVLV技术基座（F133_Tina）.md
│   ├── architecture/         # 架构设计
│   ├── guides/               # 开发指南
│   └── api/                  # API文档
├── CMakeLists.txt            # 构建配置
└── README.md                 # 本文件
```

---

## 🔧 使用示例

### 播放器使用

```cpp
// 初始化播放器
PlayerAdapter::instance().start();

// 设置事件监听器
PlayerAdapter::instance().setListener([](const PlayerEvent& ev){
    // 处理播放器事件
});

// 播放歌曲
PlayerAdapter::instance().play("http://example.com/song.m3u8");

// 暂停
PlayerAdapter::instance().pause();

// 切换音轨（原唱/伴奏）
PlayerAdapter::instance().switchTrack(1);

// 设置音量（0-100）
PlayerAdapter::instance().setVolume(80);
```

### UI 开发

```cpp
// 在页面中响应播放器事件
void PagePlayer::onPlayerEvent(const PlayerEvent& ev) {
    switch (ev.type) {
    case PlayerEventType::PLAYING:
        updatePlayButton("暂停");
        break;
    case PlayerEventType::PAUSED:
        updatePlayButton("播放");
        break;
    case PlayerEventType::COMPLETED:
        playNextSong();
        break;
    }
}
```

---

## 📚 文档

- **[F133第一期需求文档](docs/F133第一期需求文档.md)** - 需求和功能范围
- **[KTVLV技术基座](docs/KTVLV技术基座（F133_Tina）.md)** - 技术基座详细说明
- **[并发架构总结构](docs/architecture/并发架构总结构（最终版）.md)** - 并发架构标准答案
- **[开源库清单](docs/开源库清单.md)** - 使用的开源库清单
- **[F133平台库清单](docs/F133平台库清单.md)** - F133平台库清单
- **[Cursor实现提示模板](docs/Cursor实现提示模板.md)** - Cursor实现指导

---

## 🤝 贡献

本项目为内部项目，不接受外部贡献。

---

## 📄 许可证

Proprietary - 仅供内部使用

---

## 📧 联系方式

- **项目负责人**: [待填充]
- **技术支持**: [待填充]

---

**最后更新**: 2025-12-30


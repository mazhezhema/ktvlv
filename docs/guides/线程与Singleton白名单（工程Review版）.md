# 线程与 Singleton 白名单（工程 Review 版）

> **文档版本**：v1.0  
> **最后更新**：2025-12-30  
> **状态**：✅ 核心文档（工程规范 - 白名单）  
> **适用平台**：F133 / Tina Linux  
> **目标**：明确线程和 Singleton 的白名单，确保工程可控性

---

## 🎯 核心原则（一句话）

> **只有白名单中的线程和 Singleton 可以存在，其他一律禁止。**

---

## 📋 目录

1. [线程白名单](#一-线程白名单)
2. [Singleton 白名单](#二-singleton-白名单)
3. [禁止事项（硬规则）](#三-禁止事项硬规则)
4. [生命周期管理](#四-生命周期管理)

---

## 一、线程白名单

### ✅ 允许的线程（白名单）

| 线程 | 类名 | Singleton | 启动方式 | 停止方式 | 状态 |
|------|------|-----------|---------|---------|------|
| **UI/LVGL 主线程** | `UISystem` | ✅ 是 | AppRuntime::startAll() | AppRuntime::stopAll() | ✅ 必须 |
| **Event Loop** | `EventBus` | ✅ 是 | AppRuntime::startAll() | AppRuntime::stopAll() | ✅ 必须 |
| **Network Worker** | `NetworkWorker` | ✅ 是 | AppRuntime::startAll() | AppRuntime::stopAll() | ✅ 必须 |
| **Player Worker** | `PlayerAdapter` | ✅ 是 | AppRuntime::startAll() | AppRuntime::stopAll() | ✅ 必须 |
| **LogUpload** | `LogUploadService` | ✅ 是 | AppRuntime::startAll() | AppRuntime::stopAll() | ✅ 必须 |
| **Upgrade Checker** | `UpgradeService` | ✅ 是 | AppRuntime::startAll() | AppRuntime::stopAll() | ✅ 必须 |

### ❌ 禁止的线程

| 场景 | 错误做法 | 正确做法 |
|------|---------|---------|
| **页面级线程** | 创建 `SearchThread` | 投递到 `NetworkWorker::instance().post()` |
| **临时业务线程** | 创建 `HttpThread` | 投递到 `NetworkWorker::instance().post()` |
| **播放线程** | 每次播放创建线程 | 使用常驻 `PlayerAdapter::instance()` |
| **UI回调线程** | 按钮回调中创建线程 | 发送命令到工作线程队列 |

**硬规则**：

> **白名单外的线程一律禁止创建。**

---

## 二、Singleton 白名单

### ✅ 允许的 Singleton（白名单）

| Singleton | 类名 | 原因 | 生命周期 | 状态 |
|-----------|------|------|---------|------|
| **UI/LVGL 主线程** | `UISystem::instance()` | 全局唯一 display、input | App 生命周期 | ✅ 必须 |
| **Event Dispatch** | `EventBus::getInstance()` | 全局事件总线 | App 生命周期 | ✅ 必须 |
| **Network Worker** | `NetworkWorker::instance()` | libcurl 全局初始化 | App 生命周期 | ✅ 必须 |
| **Player Worker** | `PlayerAdapter::instance()` | TPlayer 全局唯一 | App 生命周期 | ✅ 必须 |
| **LogUpload** | `LogUploadService::instance()` | 全局日志上传服务 | App 生命周期 | ✅ 必须 |
| **Upgrade Checker** | `UpgradeService::instance()` | 全局升级检测 | App 生命周期 | ✅ 必须 |
| **AppRuntime** | `AppRuntime::instance()` | 全局线程生命周期总控 | App 生命周期 | ✅ 必须 |

### ❌ 禁止的 Singleton

| 场景 | 错误做法 | 正确做法 |
|------|---------|---------|
| **页面级 Singleton** | `SearchPage::instance()` | 使用单例页面管理器 |
| **临时业务 Singleton** | `HttpClient::instance()` | 使用 `NetworkWorker` |
| **数据模型 Singleton** | `SongList::instance()` | 使用普通对象或静态数据 |

**硬规则**：

> **白名单外的 Singleton 一律禁止创建。**

---

## 三、禁止事项（硬规则）

### 🚫 硬规则 1：禁止 detach

> **项目中禁止使用 `std::thread::detach()`，所有线程必须可 join、可停止、可回收。**

### 🚫 硬规则 2：禁止线程内抛异常

> **线程主循环中禁止抛出异常，所有异常必须在线程内部捕获并转为错误码/状态上报。**

### 🚫 硬规则 3：禁止 Singleton 构造依赖

> **Singleton 构造函数中禁止调用其他 Singleton::instance()。这是救命规则。**

### 🚫 硬规则 4：禁止死循环线程

> **常驻线程必须可阻塞、可唤醒、可退出，禁止 `while(1)` 死循环。**

### 🚫 硬规则 5：禁止依赖 static 析构顺序

> **不依赖 C++ static 析构顺序，所有线程必须在 App 主流程中显式 stop。**

### 🚫 硬规则 6：禁止各模块自行控制线程生命周期

> **所有线程的 start() / stop() 由 AppRuntime 统一调用，禁止各模块自行控制。**

---

## 四、生命周期管理

### 启动顺序（由 AppRuntime 控制）

```
1. EventBus（事件总线，所有模块依赖）
2. UISystem（UI主线程，所有UI操作依赖）
3. NetworkWorker（网络线程，业务模块可能依赖）
4. PlayerAdapter（播放器线程，播放功能依赖）
5. LogUploadService（日志上传，低优先级）
6. UpgradeService（升级检测，低优先级）
```

### 停止顺序（由 AppRuntime 控制）

```
1. UpgradeService（升级检测，低优先级）
2. LogUploadService（日志上传，低优先级）
3. PlayerAdapter（播放器线程，依赖者先停止）
4. NetworkWorker（网络线程，依赖者先停止）
5. EventBus（事件总线，被依赖者后停止）
6. UISystem（UI主线程，最后停止）
```

---

## 📚 相关文档

- [AppRuntime线程生命周期总控设计.md](../architecture/AppRuntime线程生命周期总控设计.md) ⭐⭐⭐ **必读**
- [KTV_App线程Singleton编码规范（最终版）.md](./KTV_App线程Singleton编码规范（最终版）.md) ⭐⭐⭐ **必读**
- [异常态处理总则.md](../architecture/异常态处理总则.md) ⭐⭐⭐ **必读**

---

**最后更新**: 2025-12-30  
**维护者**: 项目团队  
**状态**: ✅ 核心文档（工程规范 - 白名单）


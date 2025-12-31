# 代码审查 Checklist

> **文档版本**：v1.0  
> **最后更新**：2025-12-30  
> **状态**：✅ 核心文档  
> **用途**：代码审查时必须检查的项目

---

## 🚨 资源管理检查（必须全部通过）

### Singleton 模式

- [ ] 所有 Service 是否使用 Singleton 模式？
  - ✅ 正确：`ServiceName& ServiceName::instance() { static ServiceName inst; return inst; }`
  - ❌ 错误：`ServiceName* service = new ServiceName();`

- [ ] 所有 Page 是否使用 Singleton 模式？
  - ✅ 正确：`PageName& PageName::instance() { static PageName inst; return inst; }`
  - ❌ 错误：每次导航时 `new PageName()`

- [ ] 是否有全局单例对象（UITheme、Logger等）？

### 内存管理

- [ ] 是否使用了 `new`？
  - ❌ 发现即驳回，必须使用 Singleton

- [ ] 是否使用了 `delete`？
  - ❌ 发现即驳回，资源生命周期 = App生命周期

- [ ] 是否使用了 `free`？
  - ❌ 发现即驳回，使用栈对象或Singleton

- [ ] 是否使用了 `lv_obj_del`？
  - ❌ 发现即驳回，使用 `show()/hide()` 代替

### UI 控件管理

- [ ] 控件是否在循环内创建？
  - ❌ 发现即驳回，必须使用控件池

- [ ] 页面切换时是否创建新页面？
  - ❌ 发现即驳回，必须使用 `PageXxx::instance().show()`

- [ ] 页面退出时是否删除控件？
  - ❌ 发现即驳回，必须使用 `hide()`

- [ ] 列表控件是否使用控件池？
  - ✅ 必须预创建固定数量的项

- [ ] 是否在每次数据更新时重新创建控件？
  - ❌ 发现即驳回，必须使用 `update()` 方法

---

## 🔐 并发安全检查

### 线程安全

- [ ] 是否有跨线程直接操作 UI？
  - ❌ 发现即驳回，必须通过 `UiEventBus` 或 `UiDispatcher`

- [ ] 是否有直接调用 `tplayer_*` 函数？
  - ❌ 发现即驳回，必须使用 `PlayerService`

- [ ] 是否有直接使用 `curl_easy_*` 函数？
  - ❌ 发现即驳回，必须使用 `HttpService`

- [ ] 是否有 `pthread_create` 调用？
  - ❌ 发现即驳回，必须使用 Service 内置线程

### 事件和队列

- [ ] 播放控制是否使用 `PlayerService`？
  - ✅ 必须使用 `PlayerService::instance().play()`

- [ ] 网络请求是否使用 `HttpService`？
  - ✅ 必须使用 `HttpService::instance().get()`

- [ ] 事件是否通过 `UiEventBus` 传递？
  - ✅ UI更新事件必须通过 `UiEventBus::post()`

- [ ] 是否有直接操作 `PlayerCmdQueue` 或 `UiEventQueue`？
  - ❌ 发现即驳回，必须通过 Service 接口

---

## 📁 代码组织检查

### 目录结构

- [ ] 代码是否放在正确的目录？
  - `features/` - 业务逻辑
  - `services/` - 服务层（框架层，不修改）
  - `models/` - 数据模型
  - `ui/` - UI层

- [ ] 文件命名是否符合规范？
  - 类名：PascalCase（如 `SearchController`）
  - 文件名：与类名一致

### 依赖关系

- [ ] `features/` 是否依赖 `services/`？
  - ✅ 允许

- [ ] `features/` 是否依赖 `platform/`？
  - ❌ 不允许，必须通过 `services/`

- [ ] `models/` 是否依赖 `services/`？
  - ❌ 不允许

- [ ] `ui/` 是否直接依赖 `services/`？
  - ❌ 不允许，必须通过 Controller

---

## 🎯 架构符合性检查

### Service 层

- [ ] Service 是否提供统一的业务接口？
- [ ] Service 是否隐藏底层实现细节？
- [ ] Service 是否保证线程安全？

### 业务层

- [ ] Controller 是否只调用 Service 接口？
- [ ] Controller 是否不直接操作底层库？
- [ ] Controller 是否通过 `UiEventBus` 更新 UI？

### 数据模型

- [ ] Model 是否提供 `fromJson()` 方法？
- [ ] Model 是否提供 `toJson()` 方法？
- [ ] Model 是否不依赖 Service？

---

## ⚠️ 危险模式检查（立即驳回）

| 危险模式 | 检查项 | 处理方式 |
|---------|--------|---------|
| **每次操作新建页面** | 查找 `new PageXxx()` | 改为 `PageXxx::instance().show()` |
| **循环内创建控件** | 查找 `lv_obj_create()` 在循环中 | 改为控件池 + `update()` |
| **UI线程阻塞网络** | 查找 `HttpService::get()` 在UI线程同步调用 | 改为异步调用或使用事件 |
| **直接调用底层API** | 查找 `tplayer_*`、`curl_easy_*` | 改为使用 Service 接口 |
| **页面退出删除控件** | 查找 `lv_obj_del()` 在页面退出时 | 改为 `hide()` |
| **Service 中直接 new** | 查找 `new` 在 Service 中 | 改为静态实例或控件池 |

---

## ✅ 通过标准

代码审查必须满足以下条件才能通过：

1. ✅ **资源管理**：所有检查项全部通过
2. ✅ **并发安全**：所有检查项全部通过
3. ✅ **代码组织**：所有检查项全部通过
4. ✅ **架构符合性**：所有检查项全部通过
5. ✅ **危险模式**：未发现任何危险模式

---

## 📚 相关文档

- **资源管理规范**：[资源管理规范v1.md](./资源管理规范v1.md)
- **团队开发规范**：[团队开发规范v1.md](./团队开发规范v1.md)
- **服务层API设计**：[服务层API设计文档.md](./服务层API设计文档.md)
- **Cursor开发脚手架**：[Cursor开发脚手架提示.md](./Cursor开发脚手架提示.md)

---

**最后更新**: 2025-12-30  
**状态**: ✅ 核心文档，代码审查Checklist


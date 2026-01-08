# PM代码评审Checklist（3秒检查法）

> **文档版本**：v1.0  
> **最后更新**：2025-12-30  
> **状态**：✅ PM 专用  
> **用途**：3秒判断是否越权，不讨论技术细节

---

## 🎯 核心原则

**3秒检查法**：只问三件事，不对就返工

1. **它放在哪个目录？**
2. **这个目录是谁负责？**
3. **这个人有没有越权？**

---

## ⚡ 快速检查表（按优先级）

### 🔴 第一优先级：目录位置检查（30秒）

| 检查项 | 规则 | 发现即驳回 |
|--------|------|-----------|
| **文件路径** | 检查 `git diff --name-only` | 文件必须在正确的目录 |
| **UI 文件在 service/** | `app/ui/` 下的文件不能在 `app/service/` | ❌ 返工 |
| **Service 文件在 ui/** | `app/service/` 下的文件不能在 `app/ui/` | ❌ 返工 |
| **Adapter 接口被修改** | `app/adapter/*.h` 被非 PM 修改 | ❌ 返工 |
| **Event ID 被修改** | `app/event/AppEventId.h` 被非 PM 修改 | ❌ 返工 |

**检查命令**：
```bash
# 检查新增/修改的文件路径
git diff --name-only HEAD

# 检查是否有越权文件
git diff --name-only HEAD | grep -E "(app/service/.*\.(c|h)|app/ui/.*\.(c|h))"
```

---

### 🟡 第二优先级：角色权限检查（1分钟）

| 目录 | 允许修改的人 | 检查 |
|------|------------|------|
| `app/ui/pages/` | 🎨 工程师 A | ✅ 允许 |
| `app/ui/widgets/` | 🎨 工程师 A | ✅ 允许 |
| `app/service/` | ⚙️ 工程师 B | ✅ 允许 |
| `app/viewmodeldata/` | ⚙️ 工程师 B（PM 定命名） | ⚠️ 检查命名 |
| `app/adapter/*.h` | 🧠 PM | ❌ 非 PM 修改 = 返工 |
| `app/adapter/*.c` | ⚙️ 工程师 B（PM review） | ⚠️ 必须 review |
| `app/event/AppEventId.h` | 🧠 PM | ❌ 非 PM 修改 = 返工 |
| `app/controller/` | 🧠 PM（模板）⚙️ B（实现） | ⚠️ 检查是否符合模板 |

**检查命令**：
```bash
# 检查 Adapter 接口是否被修改
git diff HEAD -- app/adapter/*.h

# 检查 Event ID 是否被修改
git diff HEAD -- app/event/AppEventId.h
```

---

### 🟢 第三优先级：架构符合性检查（2分钟）

| 检查项 | 规则 | 发现即驳回 |
|--------|------|-----------|
| **UI 层 include Service** | `app/ui/` 下不能 `#include "service/` | ❌ 返工 |
| **Service 层 include UI** | `app/service/` 下不能 `#include "ui/` | ❌ 返工 |
| **直接调用底层 API** | 不能直接调用 `tplayer_*`、`curl_easy_*` | ❌ 返工 |
| **跨线程 UI 操作** | 非 UI 线程不能直接操作 LVGL | ❌ 返工 |
| **单例模式错误** | Service/Page 必须使用 Singleton | ❌ 返工 |

**检查命令**：
```bash
# 检查 UI 层是否 include Service
grep -r "#include.*service" app/ui/

# 检查 Service 层是否 include UI
grep -r "#include.*ui" app/service/

# 检查是否直接调用底层 API
grep -r "tplayer_\|curl_easy_" app/ui/ app/service/
```

---

## 📋 完整检查清单（详细版）

### 1. 目录位置检查

- [ ] 新增文件是否在正确的目录？
  - `app/ui/pages/` - 页面文件
  - `app/ui/widgets/` - UI 组件
  - `app/service/` - 业务服务
  - `app/viewmodeldata/` - 数据模型
  - `app/adapter/` - 适配器层

- [ ] 是否有文件放错目录？
  - ❌ UI 文件在 `app/service/` → 返工
  - ❌ Service 文件在 `app/ui/` → 返工

---

### 2. 角色权限检查

- [ ] 工程师 A 是否只修改了 `app/ui/` 下的文件？
  - ✅ 允许：`app/ui/pages/`、`app/ui/widgets/`
  - ❌ 禁止：`app/service/`、`app/adapter/`、`app/viewmodeldata/`

- [ ] 工程师 B 是否只修改了 `app/service/`、`app/viewmodeldata/`、`app/adapter/*.c`？
  - ✅ 允许：`app/service/`、`app/viewmodeldata/`、`app/adapter/*.c`
  - ❌ 禁止：`app/ui/`、`app/adapter/*.h`、`app/event/AppEventId.h`

- [ ] 是否有非 PM 修改了高风险文件？
  - ❌ `app/adapter/*.h` 被非 PM 修改 → 返工
  - ❌ `app/event/AppEventId.h` 被非 PM 修改 → 返工
  - ❌ `app/controller/AppController.h` 被非 PM 修改 → 返工

---

### 3. 架构符合性检查

- [ ] UI 层是否直接 include Service？
  ```c
  // ❌ 错误
  #include "service/CategoryService.h"
  
  // ✅ 正确
  #include "controller/AppController.h"
  #include "event/AppEvent.h"
  ```

- [ ] Service 层是否直接 include UI？
  ```c
  // ❌ 错误
  #include "ui/pages/HomePage.h"
  
  // ✅ 正确
  #include "service/CategoryService.h"
  #include "event/AppEvent.h"
  ```

- [ ] 是否直接调用底层 API？
  ```c
  // ❌ 错误
  tplayer_play(url);
  curl_easy_perform(curl);
  
  // ✅ 正确
  MediaPlayerManager::instance().play(url);
  NetworkClient::instance().get(url);
  ```

- [ ] 是否跨线程操作 UI？
  ```c
  // ❌ 错误（在非 UI 线程）
  lv_obj_set_text(label, "text");
  
  // ✅ 正确
  AppEventDispatcher::instance().post(UI_UPDATE_EVENT);
  ```

---

### 4. 单例模式检查

- [ ] Service 是否使用 Singleton？
  ```c
  // ✅ 正确
  CategoryService& CategoryService::instance() {
      static CategoryService inst;
      return inst;
  }
  
  // ❌ 错误
  CategoryService* service = new CategoryService();
  ```

- [ ] Page 是否使用 Singleton？
  ```c
  // ✅ 正确
  HomePage& HomePage::instance() {
      static HomePage inst;
      return inst;
  }
  
  // ❌ 错误
  HomePage* page = new HomePage();
  ```

---

### 5. 命名规范检查

- [ ] Controller 的 UI 入口是否使用 `onUiXxx`？
  ```c
  // ✅ 正确
  void onUiCategoryClicked(int categoryId);
  
  // ❌ 错误
  void handleCategoryClick(int categoryId);
  ```

- [ ] Controller 的 Service 回调是否使用 `onSvcXxx`？
  ```c
  // ✅ 正确
  void onSvcCategoryDataReady(const CategoryList& list);
  
  // ❌ 错误
  void onCategoryDataReady(const CategoryList& list);
  ```

---

## 🚨 立即驳回的情况

以下情况**立即驳回，不讨论技术细节**：

1. ❌ **文件放错目录** - 工程师 A 在 `app/service/` 下创建文件
2. ❌ **越权修改** - 工程师 B 修改 `app/adapter/*.h`
3. ❌ **越权修改** - 任何人修改 `app/event/AppEventId.h`（除了 PM）
4. ❌ **跨层依赖** - UI 层 include Service
5. ❌ **直接调用底层** - 直接调用 `tplayer_*`、`curl_easy_*`
6. ❌ **跨线程 UI** - 非 UI 线程操作 LVGL

---

## ✅ 通过标准

代码评审必须满足以下条件才能通过：

1. ✅ **目录位置**：所有文件在正确的目录
2. ✅ **角色权限**：没有越权修改
3. ✅ **架构符合性**：没有跨层依赖、没有直接调用底层
4. ✅ **单例模式**：Service/Page 使用 Singleton
5. ✅ **命名规范**：符合命名规范

---

## 📝 评审模板（可直接复制）

```
## 代码评审结果

### ✅ 通过 / ❌ 驳回

### 检查结果

1. **目录位置**：✅ / ❌
   - 说明：...

2. **角色权限**：✅ / ❌
   - 说明：...

3. **架构符合性**：✅ / ❌
   - 说明：...

### 问题清单

1. [问题1]
2. [问题2]

### 处理方式

- [ ] 返工（必须修复）
- [ ] 需要 PM review（高风险区域）
- [ ] 通过
```

---

## 📚 相关文档

- [目录分工标注（定版）.md](./目录分工标注（定版）.md) ⭐⭐⭐ **必读**
- [模块责任表（RACI最终版）.md](./模块责任表（RACI最终版）.md)
- [代码审查Checklist.md](../代码审查Checklist.md)

---

**最后更新**: 2025-12-30  
**状态**: ✅ PM 专用，3秒检查法  
**维护者**: Tech Product Owner


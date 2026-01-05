# Tina KTV 项目 10 条铁律（贴墙版）

> **文档版本**：v1.0  
> **最后更新**：2025-12-30  
> **状态**：✅ 定版（违者返工）  
> **用途**：直接贴墙，直接执行

---

## ⚠️ 重要声明

> **这 10 条不是建议，是"违者返工"**

---

## 铁律 1️⃣：任何工程师不得自行引入线程

**禁止项**：
- ❌ `std::thread`
- ❌ `pthread`
- ❌ `timer`
- ❌ 任何线程创建代码

**规则**：
> **需要线程 = PM 决策**

**检查方式**：
- 代码 review 时搜索 `std::thread`、`pthread_create`
- 发现即返工

---

## 铁律 2️⃣：libcurl 必须是 singleton，且只存在一个入口

**禁止项**：
- ❌ 任何模块直接 `#include <curl/curl.h>`
- ❌ 多个 NetworkService 实例
- ❌ libcurl handle 重复初始化

**规则**：
> **libcurl 只存在于 NetworkService，全局唯一**

**检查方式**：
- 搜索 `curl/curl.h`，只允许在 `NetworkService` 中出现
- 搜索 `curl_easy_init`，只允许在 `NetworkService` 中出现

---

## 铁律 3️⃣：UI 层永远不直接调用 Service / Network

**禁止项**：
- ❌ UI 直接调用 `Service::instance()`
- ❌ UI 直接调用 `NetworkService`
- ❌ UI 直接调用 `PlayerService`
- ❌ UI 做业务决策

**规则**：
> **UI 只发 Event，不做业务决策**

**检查方式**：
- UI 代码中搜索 `Service::`、`NetworkService::`、`PlayerService::`
- 发现即返工

---

## 铁律 4️⃣：Service 永远不操作 LVGL / 控件

**禁止项**：
- ❌ Service 直接调用 `lv_obj_xxx`
- ❌ Service 修改控件状态
- ❌ Service 操作页面对象

**规则**：
> **Service 只产数据，不展示数据**

**检查方式**：
- Service 代码中搜索 `lv_obj_`、`lv_`
- 发现即返工

---

## 铁律 5️⃣：Controller 只能做三件事

**允许项**：
- ✅ 接 Event（`onUiXxx`、`onSvcXxx`）
- ✅ 调 Service（调用 Service 接口）
- ✅ 更新 UI State（更新页面状态）

**禁止项**：
- ❌ Controller 解析 JSON
- ❌ Controller 做业务逻辑
- ❌ Controller 直接操作控件

**规则**：
> **接 Event → 调 Service → 更新 UI State  
> 任何多余逻辑都是越权**

**检查方式**：
- Controller 代码中搜索 JSON 解析代码
- Controller 代码中搜索业务逻辑
- 发现即返工

---

## 铁律 6️⃣：页面必须是 singleton，只 show / hide

**禁止项**：
- ❌ 页面 `create / destroy`
- ❌ 页面反复创建/销毁
- ❌ 页面切换时销毁页面

**规则**：
> **禁止 create / destroy 页面对象**

**检查方式**：
- 搜索页面创建/销毁代码
- 搜索 `new Page`、`delete page`
- 发现即返工

---

## 铁律 7️⃣：Repo 只存数据，不包含业务逻辑

**禁止项**：
- ❌ Repo 做业务判断
- ❌ Repo 做数据转换
- ❌ Repo 做网络请求

**规则**：
> **Repo ≠ Service  
> Repo 只能被读（UI）或写（Service）**

**检查方式**：
- Repo 代码中搜索业务逻辑
- Repo 代码中搜索 Service 调用
- 发现即返工

---

## 铁律 8️⃣：MVP 阶段不做并发、不做预加载、不做重试

**禁止项**：
- ❌ 并发请求
- ❌ 后台预加载
- ❌ 自动重试
- ❌ 缓存策略升级

**规则**：
> **能用 loading 顶住的，一律不用架构升级**

**检查方式**：
- 搜索并发相关代码（`std::async`、`std::future`）
- 搜索预加载相关代码
- 搜索重试相关代码
- 发现即返工

---

## 铁律 9️⃣：任何新增"全局对象"，必须先报 PM

**禁止项**：
- ❌ 未经批准的全局对象
- ❌ 未经批准的 Singleton
- ❌ 未经批准的全局变量

**规则**：
> **未经批准的全局对象 = 架构污染**

**检查方式**：
- 代码 review 时检查新增的全局对象
- 检查新增的 Singleton
- 发现即返工

---

## 铁律 🔟：接口即合同，绕过接口等于返工

**禁止项**：
- ❌ 不通过 header 文件协作
- ❌ 绕过接口直接调用实现
- ❌ 修改接口未经 review

**规则**：
> **不通过 header 协作 = 代码无效**

**检查方式**：
- 检查是否有模块绕过接口直接调用实现
- 检查接口变更是否经过 review
- 发现即返工

---

## 📋 铁律执行方式

### 代码 Review 时

1. **按顺序检查 10 条铁律**
2. **发现违反 = 立即叫停**
3. **不讨论、不解释，直接返工**

### 每日自检

- [ ] 是否有新增线程？
- [ ] 是否有直接调用 libcurl？
- [ ] UI 是否直接调用 Service？
- [ ] Service 是否操作 UI？
- [ ] Controller 是否越权？
- [ ] 页面是否 create/destroy？
- [ ] Repo 是否有业务逻辑？
- [ ] 是否有并发/预加载/重试？
- [ ] 是否有未经批准的全局对象？
- [ ] 是否有绕过接口的调用？

---

## 💬 PM 定版宣言（可直接发群）

> **从今天开始，这个项目只按  
> 《模块责任表》+《10 条铁律》执行。  
>  
> 我负责系统骨架和边界，  
> 你们负责把业务写好。  
>  
> 任何越界不是能力问题，  
> 是执行问题，都会返工。**

---

## 📚 相关文档

- [模块责任表（RACI最终版）.md](./模块责任表（RACI最终版）.md) ⭐⭐⭐ **必读**
- [Tina_KTV_MVP产品与技术协作规范v1.0.md](./Tina_KTV_MVP产品与技术协作规范v1.0.md) ⭐⭐⭐ **必读**
- [PM一页纸作战手册（每日5分钟）.md](./PM一页纸作战手册（每日5分钟）.md) ⭐⭐⭐ **必读**

---

**最后更新**: 2025-12-30  
**状态**: ✅ 定版（违者返工）  
**维护者**: Tech Product Owner


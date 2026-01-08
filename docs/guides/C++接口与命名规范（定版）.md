# C++接口与命名规范（定版，Google 风格 + Status(int) 约束）

> **状态**：✅ 定版（团队统一口径）  
> **适用范围**：本仓库所有 C/C++ 代码（尤其是 `src/`）  
> **目标**：一眼区分：**入参 / 出参 / member / 栈变量 / 返回值**，减少误用与代码评审成本

---

## 一、基准

- **主基准**：Google C++ Style Guide（中文版目录见：[`contents.html`](https://zh-google-styleguide.readthedocs.io/en/latest/google-cpp-styleguide/contents.html)，特性取舍见：[`others.html`](https://zh-google-styleguide.readthedocs.io/en/latest/google-cpp-styleguide/others.html)）
- **项目原则**：在不引入额外复杂度前提下，尽量贴近 Google 风格；历史代码允许渐进式对齐。
- **项目强约束（拍板）**：返回值只干一件事：**表示状态**（`Status(int)`）；数据只走 `out_` 参数。

---

## 二、命名约定（按 Google 风格）

### 2.1 member / 局部变量

- **成员变量（member）**：`lower_snake_case_`（以下划线结尾）  
  - 例：`timeout_seconds_`, `curl_handle_`, `running_`
- **局部变量（栈变量）**：`lower_snake_case`（不加前缀）  
  - 例：`full_url`, `root`, `item`, `ret`

> 注意：避免使用以 `_` 开头的标识符（尤其是 `__xxx` 或 `_Xxx`），这类名字在实现/标准里有保留规则，容易踩坑。

---

## 三、入参 / 出参 / 入出参（接口一眼能看懂）

### 1) 入参（只读）

- **只读输入**：不带 `out_` / `inout_` 前缀  
  - `int timeout_seconds`（基础类型一律按值传入，不用 `int&`）  
  - `const char* url`  
  - `const std::string& base_url`

- **基础类型（强约束）**：`int / bool / double / size_t / uint32_t / uint64_t ...`  
  - ✅ 只允许按值传入（例如 `int timeout_seconds`）  
  - ❌ 不允许 `*` / `&` 出现在参数列表里（例如 `int*` / `int&` / `bool*` / `double&`）

### 2) 出参（输出）

- **输出参数**：使用指针 `T*`，并放在参数列表末尾  
  - `HttpResponse* out_resp`（必须提供）  
  - `ktv::utils::OutInt* out_count`（可选输出，允许 `nullptr`）
- **禁止 `T**`/`T*&` 用作输出**：避免“指针层级/所有权”歧义扩散  
  - ✅ 推荐：用“小包装结构体”或“调用方持有的对象”来承载句柄/节点（见下文 Json 示例）
- **禁止基础类型指针/引用出参**：不允许 `int*` / `int&` / `bool*` / `bool&` / `size_t*` / `double*` 等出现在参数列表中  
  - ✅ 统一改用：`ktv::utils::OutValue<T>*`（例如 `OutInt* / OutBool* / OutDouble*`）

### 3) 入出参（既读又写）

- **入出参统一**：`inout_` 前缀  
  - `std::string& inout_buf`

### 4) 输出缓冲区（C 风格）

- 统一写法：`char* out_buf, size_t out_buf_len`

---

## 四、返回值（Google 风格 + 项目强约束）

### 4.1 错误处理（不使用异常）

- **不使用 C++ 异常**：失败通过返回值表达（Google 也倾向避免异常，详见 [`others.html`](https://zh-google-styleguide.readthedocs.io/en/latest/google-cpp-styleguide/others.html)）
- **返回值统一为 `Status(int)`**：`0` 成功；`<0` 失败（负值可细分错误类型）
- **返回值仅表示状态**：不承载任何业务数据（当前阶段不靠返回值传 `double`/数值）
- **禁止第二通道状态**：不允许 `int*` / `bool*` 等形式返回“状态信息”

示例：

```cpp
int Get(const char* url, HttpResponse* out_resp);                 // 0 / <0
int ParseSong(const char* json, SongInfo* out_song);              // 0 / <0
int FillName(const cJSON* obj, char* out_buf, size_t out_len);    // 0 / <0
```

---

## 五、类/函数命名（Google 风格）

- **类/结构体/枚举**：`PascalCase`（例：`HttpService`, `HttpResponse`, `PlayerState`）
- **函数/方法**：`CamelCase`（例：`GetInstance`, `WriteCallback`）
- **宏**：`UPPER_SNAKE_CASE`（例：`MAX_JSON_SIZE`）

---

## 六、示例（推荐模板）

```cpp
// 业务接口：返回值表达状态；输出参数放最后（指针）
int Post(const char* url,
         const char* json_body,
         HttpResponse* out_resp);

// 输出缓冲区：out_buf + out_buf_len
int GetString(const cJSON* obj,
              const char* key,
              char* out_buf,
              size_t out_buf_len);

// 类内：member 用 *_，局部用 lower_snake_case
class Foo {
public:
    int DoWork(int value, ktv::utils::OutInt* out_result);

private:
    int state_{0};
};
```

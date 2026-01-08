// json_helper.h
// cJSON 封装工具类，业务层禁止直接使用 cJSON API
#pragma once

#include "cJSON.h"
#include <stdbool.h>
#include <stddef.h>
#include "out_value.h"

// JSON 大小上限（64KB）
#define MAX_JSON_SIZE (64 * 1024)

// Forward declaration（JsonDocument 需要 friend JsonHelper）
class JsonHelper;

namespace ktv::utils {

/**
 * JsonDocument - JSON 解析结果容器（RAII）
 *
 * 职责：持有 cJSON* 生命周期，析构时自动释放
 * 用法：声明变量 → 传给 JsonHelper::Parse() → 用 .root() 取值
 */
class JsonDocument {
public:
    JsonDocument() = default;
    ~JsonDocument() { Reset(); }

    // 禁止拷贝
    JsonDocument(const JsonDocument&) = delete;
    JsonDocument& operator=(const JsonDocument&) = delete;

    // 允许移动
    JsonDocument(JsonDocument&& other) noexcept : root_(other.root_) { other.root_ = nullptr; }
    JsonDocument& operator=(JsonDocument&& other) noexcept {
        if (this == &other) return *this;
        Reset();
        root_ = other.root_;
        other.root_ = nullptr;
        return *this;
    }

    void Reset() {
        if (root_) {
            cJSON_Delete(root_);
            root_ = nullptr;
        }
    }

    const cJSON* root() const { return root_; }

private:
    friend class ::JsonHelper;
    cJSON* root_{nullptr};
};

}  // namespace ktv::utils

/**
 * JsonHelper - JSON 安全取值工具（值级 API，定版约束）
 *
 * 一句话定位：
 * 把"又臭又容易出错的 cJSON 解析细节"，变成"工程师可直接使用的安全取值函数"。
 *
 * ✅ 允许做的事（唯一职责）：
 * - 校验字段是否存在
 * - 校验字段类型
 * - 拷贝值到调用方提供的缓冲区/OutValue
 * - 返回明确错误码（0 成功；<0 失败）
 *
 * ❌ 明确禁止（不做这些事）：
 * - 不暴露 JSON 结构/节点（不返回子节点/不提供遍历）
 * - 不提供 IsXxx/类型探测给业务层使用
 * - 不提供修改/构建 JSON 的接口
 *
 * 使用边界（硬规则）：
 * ✅ 允许：Network 层、Service 层（JSON 解析）
 * ❌ 禁止：UI 层、Player 层、LVGL callback、音频线程
 *
 * 对外 API 白名单（只能用这些）：
 * - Parse()
 * - GetString()/GetInt()/GetLong()/GetDouble()/GetBool()
 * - GetArraySize() / GetObjectArraySize()
 * - GetArrayObjectString()/GetArrayObjectInt()/GetArrayObjectBool()
 * - GetRootArrayObjectString()/GetRootArrayObjectInt()/GetRootArrayObjectBool()
 */
class JsonHelper {
public:
    /**
     * 安全解析 JSON（带大小检查）
     * @param str JSON 字符串
     * @param len 字符串长度
     * @param out_doc 输出：解析结果（JsonDocument 自动管理释放，调用方不要手动 cJSON_Delete）
     * @return 0 成功；<0 失败
     */
    static int Parse(const char* str, size_t len, ktv::utils::JsonDocument* out_doc);
    
    /**
     * 安全读取字符串（带缓冲区保护）
     * @param obj cJSON 对象
     * @param key 键名
     * @param out 输出缓冲区
     * @param out_len 输出缓冲区大小
     * @return 0 成功；<0 失败
     */
    static int GetString(const cJSON* obj, const char* key,
                         char* out, size_t out_len);
    
    /**
     * 安全读取整数
     * @param obj cJSON 对象
     * @param key 键名
     * @param out 输出值
     * @return 0 成功；<0 失败
     */
    static int GetInt(const cJSON* obj, const char* key, ktv::utils::OutInt* out_value);
    
    /**
     * 安全读取长整数
     * @param obj cJSON 对象
     * @param key 键名
     * @param out 输出值
     * @return 0 成功；<0 失败
     */
    static int GetLong(const cJSON* obj, const char* key, ktv::utils::OutLong* out_value);
    
    /**
     * 安全读取浮点数
     * @param obj cJSON 对象
     * @param key 键名
     * @param out 输出值
     * @return 0 成功；<0 失败
     */
    static int GetDouble(const cJSON* obj, const char* key, ktv::utils::OutDouble* out_value);
    
    /**
     * 安全读取布尔值
     * @param obj cJSON 对象
     * @param key 键名
     * @param out 输出值
     * @return 0 成功；<0 失败
     */
    static int GetBool(const cJSON* obj, const char* key, ktv::utils::OutBool* out_value);
    
    /**
     * 获取数组大小（root 本身是数组时使用）
     * @param arr cJSON 数组（通常是 doc.root()）
     * @param out_size 输出：数组大小
     * @return 0 成功；<0 失败
     */
    static int GetArraySize(const cJSON* arr, ktv::utils::OutInt* out_size);

    /**
     * 获取对象内嵌套数组的大小
     * 场景：{"items": [...]} 获取 items 数组长度
     * @param root cJSON 对象（通常是 doc.root()）
     * @param array_key 数组字段名（如 "items"）
     * @param out_size 输出：数组大小
     * @return 0 成功；<0 失败
     */
    static int GetObjectArraySize(const cJSON* root, const char* array_key, ktv::utils::OutInt* out_size);
    
    // ------------------------------------------------------------
    // 数组值级访问（禁止返回节点/结构）
    // 场景：items[] 这种"数组-对象列表"解析
    // ------------------------------------------------------------

    // out_buf: 输出字符串字段
    static int GetArrayObjectString(const cJSON* root,
                                    const char* array_key,
                                    int index,
                                    const char* field_key,
                                    char* out_buf,
                                    size_t out_len);

    // out_value: 输出 int 字段
    static int GetArrayObjectInt(const cJSON* root,
                                 const char* array_key,
                                 int index,
                                 const char* field_key,
                                 ktv::utils::OutInt* out_value);

    // out_value: 输出 bool 字段
    static int GetArrayObjectBool(const cJSON* root,
                                  const char* array_key,
                                  int index,
                                  const char* field_key,
                                  ktv::utils::OutBool* out_value);

    // ------------------------------------------------------------
    // 顶层数组（root 为数组）值级访问：用于 HTTP 直接返回 [] 的场景
    // ------------------------------------------------------------

    static int GetRootArrayObjectString(const cJSON* root_array,
                                        int index,
                                        const char* field_key,
                                        char* out_buf,
                                        size_t out_len);

    static int GetRootArrayObjectInt(const cJSON* root_array,
                                     int index,
                                     const char* field_key,
                                     ktv::utils::OutInt* out_value);

    static int GetRootArrayObjectBool(const cJSON* root_array,
                                      int index,
                                      const char* field_key,
                                      ktv::utils::OutBool* out_value);
};

// json_helper.h
// cJSON 封装工具类，业务层禁止直接使用 cJSON API
#pragma once

#include "cJSON.h"
#include <stdbool.h>
#include <stddef.h>

// JSON 大小上限（64KB）
#define MAX_JSON_SIZE (64 * 1024)

/**
 * JsonHelper - cJSON 封装工具类（薄封装）
 * 
 * 设计目的：
 * 防止"cJSON 使用方式在项目中失控"，把复杂度锁在一个点上
 * 
 * 设计原则：
 * 1. 很薄：10-15个函数，无状态，不持有对象，不隐藏cJSON
 * 2. 边界清晰：只被 Network/Service 层调用，禁止 UI/Player 层调用
 * 3. cJSON 对象生命周期 < 1 个函数
 * 4. 解析完立即释放，不跨模块传递
 * 
 * 使用边界：
 * ✅ 允许：Network 层、Service 层（JSON解析）
 * ❌ 禁止：UI 层、Player 层、LVGL callback、音频线程
 */
class JsonHelper {
public:
    /**
     * 安全解析 JSON（带大小检查）
     * @param str JSON 字符串
     * @param len 字符串长度
     * @return cJSON* 解析结果，失败返回 nullptr
     * @note 调用方必须负责调用 cJSON_Delete() 释放
     */
    static cJSON* parse(const char* str, size_t len);
    
    /**
     * 安全读取字符串（带缓冲区保护）
     * @param obj cJSON 对象
     * @param key 键名
     * @param out 输出缓冲区
     * @param out_len 输出缓冲区大小
     * @return true 成功，false 失败
     */
    static bool getString(cJSON* obj, const char* key, 
                          char* out, size_t out_len);
    
    /**
     * 安全读取整数
     * @param obj cJSON 对象
     * @param key 键名
     * @param out 输出值
     * @return true 成功，false 失败
     */
    static bool getInt(cJSON* obj, const char* key, int* out);
    
    /**
     * 安全读取长整数
     * @param obj cJSON 对象
     * @param key 键名
     * @param out 输出值
     * @return true 成功，false 失败
     */
    static bool getLong(cJSON* obj, const char* key, long* out);
    
    /**
     * 安全读取浮点数
     * @param obj cJSON 对象
     * @param key 键名
     * @param out 输出值
     * @return true 成功，false 失败
     */
    static bool getDouble(cJSON* obj, const char* key, double* out);
    
    /**
     * 安全读取布尔值
     * @param obj cJSON 对象
     * @param key 键名
     * @param out 输出值
     * @return true 成功，false 失败
     */
    static bool getBool(cJSON* obj, const char* key, bool* out);
    
    /**
     * 获取数组大小（带检查）
     * @param arr cJSON 数组
     * @return 数组大小，失败返回 0
     */
    static int getArraySize(cJSON* arr);
    
    /**
     * 获取数组项（带检查）
     * @param arr cJSON 数组
     * @param index 索引
     * @return cJSON* 数组项，失败返回 nullptr
     */
    static cJSON* getArrayItem(cJSON* arr, int index);
    
    /**
     * 获取对象项（带检查）
     * @param obj cJSON 对象
     * @param key 键名
     * @return cJSON* 对象项，失败返回 nullptr
     */
    static cJSON* getObjectItem(cJSON* obj, const char* key);
    
    /**
     * 检查是否为数组
     * @param item cJSON 项
     * @return true 是数组，false 不是
     */
    static bool isArray(cJSON* item);
    
    /**
     * 检查是否为对象
     * @param item cJSON 项
     * @return true 是对象，false 不是
     */
    static bool isObject(cJSON* item);
    
    /**
     * 检查是否为字符串
     * @param item cJSON 项
     * @return true 是字符串，false 不是
     */
    static bool isString(cJSON* item);
    
    /**
     * 检查是否为数字
     * @param item cJSON 项
     * @return true 是数字，false 不是
     */
    static bool isNumber(cJSON* item);
};


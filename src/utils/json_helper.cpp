// json_helper.cpp
// cJSON 封装工具类实现
#include "json_helper.h"
#include <string.h>
#include <stdio.h>

// 如果使用 plog，取消注释
// #include "plog/Log.h"

cJSON* JsonHelper::parse(const char* str, size_t len) {
    if (!str || len == 0) {
        // LOG_ERROR("Invalid JSON input");
        return nullptr;
    }
    
    if (len > MAX_JSON_SIZE) {
        // LOG_ERROR("JSON size exceeds limit: %zu > %d", len, MAX_JSON_SIZE);
        return nullptr;
    }
    
    cJSON* root = cJSON_ParseWithLength(str, len);
    if (!root) {
        const char* error = cJSON_GetErrorPtr();
        // LOG_ERROR("JSON parse failed: %s", error ? error : "unknown");
        (void)error;  // 避免未使用警告
    }
    
    return root;
}

bool JsonHelper::getString(cJSON* obj, const char* key, 
                            char* out, size_t out_len) {
    if (!obj || !key || !out || out_len == 0) {
        return false;
    }
    
    cJSON* item = cJSON_GetObjectItem(obj, key);
    if (!item || !cJSON_IsString(item)) {
        return false;
    }
    
    const char* val = cJSON_GetStringValue(item);
    if (!val) {
        return false;
    }
    
    size_t val_len = strlen(val);
    if (val_len >= out_len) {
        // LOG_WARNING("String truncated: %s (len=%zu, max=%zu)", key, val_len, out_len - 1);
        val_len = out_len - 1;
    }
    
    memcpy(out, val, val_len);
    out[val_len] = '\0';
    
    return true;
}

bool JsonHelper::getInt(cJSON* obj, const char* key, int* out) {
    if (!obj || !key || !out) {
        return false;
    }
    
    cJSON* item = cJSON_GetObjectItem(obj, key);
    if (!item || !cJSON_IsNumber(item)) {
        return false;
    }
    
    *out = (int)cJSON_GetNumberValue(item);
    return true;
}

bool JsonHelper::getLong(cJSON* obj, const char* key, long* out) {
    if (!obj || !key || !out) {
        return false;
    }
    
    cJSON* item = cJSON_GetObjectItem(obj, key);
    if (!item || !cJSON_IsNumber(item)) {
        return false;
    }
    
    *out = (long)cJSON_GetNumberValue(item);
    return true;
}

bool JsonHelper::getDouble(cJSON* obj, const char* key, double* out) {
    if (!obj || !key || !out) {
        return false;
    }
    
    cJSON* item = cJSON_GetObjectItem(obj, key);
    if (!item || !cJSON_IsNumber(item)) {
        return false;
    }
    
    *out = cJSON_GetNumberValue(item);
    return true;
}

bool JsonHelper::getBool(cJSON* obj, const char* key, bool* out) {
    if (!obj || !key || !out) {
        return false;
    }
    
    cJSON* item = cJSON_GetObjectItem(obj, key);
    if (!item || !cJSON_IsBool(item)) {
        return false;
    }
    
    *out = cJSON_IsTrue(item) != 0;
    return true;
}

int JsonHelper::getArraySize(cJSON* arr) {
    if (!arr || !cJSON_IsArray(arr)) {
        return 0;
    }
    
    return cJSON_GetArraySize(arr);
}

cJSON* JsonHelper::getArrayItem(cJSON* arr, int index) {
    if (!arr || !cJSON_IsArray(arr)) {
        return nullptr;
    }
    
    return cJSON_GetArrayItem(arr, index);
}

cJSON* JsonHelper::getObjectItem(cJSON* obj, const char* key) {
    if (!obj || !key) {
        return nullptr;
    }
    
    return cJSON_GetObjectItem(obj, key);
}

bool JsonHelper::isArray(cJSON* item) {
    return item && cJSON_IsArray(item);
}

bool JsonHelper::isObject(cJSON* item) {
    return item && cJSON_IsObject(item);
}

bool JsonHelper::isString(cJSON* item) {
    return item && cJSON_IsString(item);
}

bool JsonHelper::isNumber(cJSON* item) {
    return item && cJSON_IsNumber(item);
}


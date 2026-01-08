// json_helper.cpp
// cJSON 封装工具类实现
#include "json_helper.h"
#include <string.h>
#include <stdio.h>

using ktv::utils::JsonDocument;

int JsonHelper::Parse(const char* str, size_t len, JsonDocument* out_doc) {
    if (!out_doc) return -1;
    out_doc->Reset();

    if (!str || len == 0) {
        // LOG_ERROR("Invalid JSON input");
        return -1;
    }

    if (len > MAX_JSON_SIZE) {
        // LOG_ERROR("JSON size exceeds limit: %zu > %d", len, MAX_JSON_SIZE);
        return -2;
    }

    cJSON* root = cJSON_ParseWithLength(str, len);
    if (!root) {
        const char* error = cJSON_GetErrorPtr();
        // LOG_ERROR("JSON parse failed: %s", error ? error : "unknown");
        (void)error;  // 避免未使用警告
        return -6;
    }

    out_doc->root_ = root;
    return 0;
}

int JsonHelper::GetString(const cJSON* obj, const char* key,
                          char* out, size_t out_len) {
    if (!obj || !key || !out || out_len == 0) {
        return -1;
    }
    
    const cJSON* item = cJSON_GetObjectItem(obj, key);
    if (!item) return -3;
    if (!cJSON_IsString(item)) return -4;

    const char* val = cJSON_GetStringValue(item);
    if (!val) return -4;

    size_t val_len = strlen(val);
    int ret = 0;
    if (val_len >= out_len) {
        // LOG_WARNING("String truncated: %s (len=%zu, max=%zu)", key, val_len, out_len - 1);
        val_len = out_len - 1;
        ret = -5; // BufferTooSmall
    }
    
    memcpy(out, val, val_len);
    out[val_len] = '\0';
    
    return ret;
}

int JsonHelper::GetInt(const cJSON* obj, const char* key, ktv::utils::OutInt* out_value) {
    if (!obj || !key || !out_value) {
        return -1;
    }
    
    const cJSON* item = cJSON_GetObjectItem(obj, key);
    if (!item) return -3;
    if (!cJSON_IsNumber(item)) return -4;

    out_value->value = (int)cJSON_GetNumberValue(item);
    return 0;
}

int JsonHelper::GetLong(const cJSON* obj, const char* key, ktv::utils::OutLong* out_value) {
    if (!obj || !key || !out_value) {
        return -1;
    }
    
    const cJSON* item = cJSON_GetObjectItem(obj, key);
    if (!item) return -3;
    if (!cJSON_IsNumber(item)) return -4;

    out_value->value = (long)cJSON_GetNumberValue(item);
    return 0;
}

int JsonHelper::GetDouble(const cJSON* obj, const char* key, ktv::utils::OutDouble* out_value) {
    if (!obj || !key || !out_value) {
        return -1;
    }
    
    const cJSON* item = cJSON_GetObjectItem(obj, key);
    if (!item) return -3;
    if (!cJSON_IsNumber(item)) return -4;

    out_value->value = cJSON_GetNumberValue(item);
    return 0;
}

int JsonHelper::GetBool(const cJSON* obj, const char* key, ktv::utils::OutBool* out_value) {
    if (!obj || !key || !out_value) {
        return -1;
    }
    
    const cJSON* item = cJSON_GetObjectItem(obj, key);
    if (!item) return -3;
    if (!cJSON_IsBool(item)) return -4;

    out_value->value = (cJSON_IsTrue(item) != 0);
    return 0;
}

int JsonHelper::GetArraySize(const cJSON* arr, ktv::utils::OutInt* out_size) {
    if (!arr || !out_size) return -1;
    if (!cJSON_IsArray(arr)) return -4;
    out_size->value = cJSON_GetArraySize(arr);
    return 0;
}

int JsonHelper::GetObjectArraySize(const cJSON* root, const char* array_key, ktv::utils::OutInt* out_size) {
    if (!root || !array_key || !out_size) return -1;

    const cJSON* arr = cJSON_GetObjectItem(root, array_key);
    if (!arr) return -3;
    if (!cJSON_IsArray(arr)) return -4;

    out_size->value = cJSON_GetArraySize(arr);
    return 0;
}

int JsonHelper::GetArrayObjectString(const cJSON* root,
                                     const char* array_key,
                                     int index,
                                     const char* field_key,
                                     char* out_buf,
                                     size_t out_len) {
    if (!root || !array_key || !field_key || !out_buf || out_len == 0) return -1;

    const cJSON* arr = cJSON_GetObjectItem(root, array_key);
    if (!arr) return -3;
    if (!cJSON_IsArray(arr)) return -4;

    const cJSON* obj = cJSON_GetArrayItem(arr, index);
    if (!obj) return -3;
    if (!cJSON_IsObject(obj)) return -4;

    return GetString(obj, field_key, out_buf, out_len);
}

int JsonHelper::GetArrayObjectInt(const cJSON* root,
                                  const char* array_key,
                                  int index,
                                  const char* field_key,
                                  ktv::utils::OutInt* out_value) {
    if (!root || !array_key || !field_key || !out_value) return -1;

    const cJSON* arr = cJSON_GetObjectItem(root, array_key);
    if (!arr) return -3;
    if (!cJSON_IsArray(arr)) return -4;

    const cJSON* obj = cJSON_GetArrayItem(arr, index);
    if (!obj) return -3;
    if (!cJSON_IsObject(obj)) return -4;

    return GetInt(obj, field_key, out_value);
}

int JsonHelper::GetArrayObjectBool(const cJSON* root,
                                   const char* array_key,
                                   int index,
                                   const char* field_key,
                                   ktv::utils::OutBool* out_value) {
    if (!root || !array_key || !field_key || !out_value) return -1;

    const cJSON* arr = cJSON_GetObjectItem(root, array_key);
    if (!arr) return -3;
    if (!cJSON_IsArray(arr)) return -4;

    const cJSON* obj = cJSON_GetArrayItem(arr, index);
    if (!obj) return -3;
    if (!cJSON_IsObject(obj)) return -4;

    return GetBool(obj, field_key, out_value);
}

int JsonHelper::GetRootArrayObjectString(const cJSON* root_array,
                                         int index,
                                         const char* field_key,
                                         char* out_buf,
                                         size_t out_len) {
    if (!root_array || !field_key || !out_buf || out_len == 0) return -1;
    if (!cJSON_IsArray(root_array)) return -4;

    const cJSON* obj = cJSON_GetArrayItem(root_array, index);
    if (!obj) return -3;
    if (!cJSON_IsObject(obj)) return -4;

    return GetString(obj, field_key, out_buf, out_len);
}

int JsonHelper::GetRootArrayObjectInt(const cJSON* root_array,
                                      int index,
                                      const char* field_key,
                                      ktv::utils::OutInt* out_value) {
    if (!root_array || !field_key || !out_value) return -1;
    if (!cJSON_IsArray(root_array)) return -4;

    const cJSON* obj = cJSON_GetArrayItem(root_array, index);
    if (!obj) return -3;
    if (!cJSON_IsObject(obj)) return -4;

    return GetInt(obj, field_key, out_value);
}

int JsonHelper::GetRootArrayObjectBool(const cJSON* root_array,
                                       int index,
                                       const char* field_key,
                                       ktv::utils::OutBool* out_value) {
    if (!root_array || !field_key || !out_value) return -1;
    if (!cJSON_IsArray(root_array)) return -4;

    const cJSON* obj = cJSON_GetArrayItem(root_array, index);
    if (!obj) return -3;
    if (!cJSON_IsObject(obj)) return -4;

    return GetBool(obj, field_key, out_value);
}



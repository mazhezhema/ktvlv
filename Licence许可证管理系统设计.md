# Licence许可证管理系统设计

> **文档版本**：v1.0  
> **相关文档**：详见 [项目架构设计总览.md](./项目架构设计总览.md)  
> **架构设计**：详见 [C++架构设计-预分配内存版本.md](./C++架构设计-预分配内存版本.md)  
> **开源库选型**：详见 [开源库选型指南.md](./开源库选型指南.md)

## ⚠️ 核心需求

**Licence管理流程：**
1. 用户第一次开机启动时输入licence
2. 获取设备MAC地址
3. 传给服务器端进行验证
4. 验证正确后二者绑定
5. 用户可以试用，点唱若干首歌（20首）
6. 超过试用期弹窗提示用户扫码激活
7. 激活后licence正式开始计算时间（12个月、24个月、36个月等）
8. 期限内用户免费使用
9. 过期后不让用户使用，提示用户续费
10. **系统重置恢复**：用户重置系统后，只要MAC地址不变，重新验证licence时服务器返回剩余时间，可以继续使用

### 核心原则

- ✅ **避免造轮子**：严格遵循避免造轮子原则，优先使用开源库
- ✅ **预分配内存**：使用固定大小缓冲区
- ✅ **单线程设计**：无锁设计
- ✅ **本地存储**：licence信息持久化到本地文件

---

## 一、设计概述

### 1.1 工作流程

```
┌─────────────────────────────────────────┐
│  首次启动：用户输入licence               │
└─────────────────────────────────────────┘
           ↓
┌─────────────────────────────────────────┐
│  获取设备MAC地址                         │
└─────────────────────────────────────────┘
           ↓
┌─────────────────────────────────────────┐
│  发送到服务器验证                        │
│  POST /api/licence/verify                │
│  {licence, mac_address}                  │
└─────────────────────────────────────────┘
           ↓
┌─────────────────────────────────────────┐
│  服务器验证成功，返回绑定信息            │
│  {valid: true, duration: 12, start_time}│
└─────────────────────────────────────────┘
           ↓
┌─────────────────────────────────────────┐
│  保存licence信息到本地                   │
│  - licence码                             │
│  - MAC地址                               │
│  - 开始时间                              │
│  - 有效期（月数）                        │
│  - 试用次数（已点唱次数）                │
└─────────────────────────────────────────┘
           ↓
┌─────────────────────────────────────────┐
│  使用过程中：                            │
│  - 每次点唱，试用次数+1                  │
│  - 检查是否超过试用期（20首）            │
│  - 检查是否过期                          │
└─────────────────────────────────────────┘
           ↓
┌─────────────────────────────────────────┐
│  系统重置后恢复：                        │
│  - 用户重新输入licence                  │
│  - 获取MAC地址（不变）                  │
│  - 服务器查询绑定记录                   │
│  - 返回剩余时间信息                     │
│  - 恢复licence状态                      │
└─────────────────────────────────────────┘
```

### 1.2 状态机

```
未激活 → 试用期（0-20首） → 激活（正式使用） → 过期 → 续费
         ↓
      超过试用期（弹窗提示扫码激活）
```

---

## 二、LicenceService设计

### 2.1 LicenceService（单例，预分配版本）

```cpp
#ifndef LICENCE_SERVICE_H
#define LICENCE_SERVICE_H

#include "http_service.h"  // 使用libcurl
#include "cJSON.h"  // JSON解析
#include <array>
#include <cstdint>
#include <ctime>

namespace ktv {

/**
 * Licence状态枚举
 */
enum class LicenceStatus : uint8_t {
    NotActivated = 0,    // 未激活（未输入licence）
    Trial,               // 试用期（0-20首）
    Activated,           // 已激活（正式使用）
    Expired,             // 已过期
    Invalid              // 无效（验证失败）
};

/**
 * Licence信息（预分配版本）
 */
struct LicenceInfo {
    static constexpr size_t MAX_LICENCE_LEN = 64;
    static constexpr size_t MAX_MAC_LEN = 18;  // MAC地址格式：XX:XX:XX:XX:XX:XX
    
    std::array<char, MAX_LICENCE_LEN> licence_code = {0};
    std::array<char, MAX_MAC_LEN> mac_address = {0};
    
    LicenceStatus status = LicenceStatus::NotActivated;
    int32_t duration_months = 0;      // 有效期（月数）：12, 24, 36等
    std::time_t start_time = 0;       // 开始时间（激活时间）
    std::time_t expire_time = 0;      // 过期时间
    int32_t trial_count = 0;          // 试用次数（已点唱次数，最多20）
    
    static constexpr int32_t MAX_TRIAL_COUNT = 20;  // 最大试用次数
    
    const char* getLicenceCode() const { return licence_code.data(); }
    const char* getMacAddress() const { return mac_address.data(); }
    
    void setLicenceCode(const char* code) {
        strncpy(licence_code.data(), code, MAX_LICENCE_LEN - 1);
        licence_code[MAX_LICENCE_LEN - 1] = '\0';
    }
    
    void setMacAddress(const char* mac) {
        strncpy(mac_address.data(), mac, MAX_MAC_LEN - 1);
        mac_address[MAX_MAC_LEN - 1] = '\0';
    }
};

/**
 * Licence服务（单例，预分配版本，无锁设计）
 */
class LicenceService {
public:
    static LicenceService& getInstance() {
        static LicenceService instance;
        return instance;
    }
    
    LicenceService(const LicenceService&) = delete;
    LicenceService& operator=(const LicenceService&) = delete;
    
    // 初始化（加载本地licence信息）
    bool initialize();
    
    // 输入licence并验证
    bool verifyLicence(const char* licence_code);
    
    // 获取当前状态
    LicenceStatus getStatus() const { return licence_info_.status; }
    
    // 检查是否可以使用（未过期且在试用期内或已激活）
    bool canUse() const;
    
    // 增加试用次数（点唱时调用）
    void incrementTrialCount();
    
    // 检查是否需要激活（超过试用期）
    bool needActivation() const;
    
    // 激活licence（扫码激活后调用）
    bool activate();
    
    // 检查是否过期
    bool isExpired() const;
    
    // 获取剩余天数
    int32_t getRemainingDays() const;
    
    // 获取试用剩余次数
    int32_t getRemainingTrialCount() const;
    
    // 获取licence信息（只读）
    const LicenceInfo& getLicenceInfo() const { return licence_info_; }
    
    // 获取MAC地址（系统调用）
    static bool getMacAddress(char* out_mac, size_t max_len);
    
private:
    LicenceService() = default;
    ~LicenceService() = default;
    
    LicenceInfo licence_info_;
    
    // 持久化文件路径
    static constexpr const char* LICENCE_FILE = "/data/ktv_licence.json";
    
    // 保存licence信息到本地文件
    void saveToFile();
    
    // 从本地文件加载licence信息
    bool loadFromFile();
    
    // 计算过期时间
    void calculateExpireTime();
    
    // 禁止动态分配
    void* operator new(size_t) = delete;
    void* operator new[](size_t) = delete;
    void operator delete(void*) = delete;
    void operator delete[](void*) = delete;
};

} // namespace ktv

#endif // LICENCE_SERVICE_H
```

### 2.2 LicenceService实现

```cpp
#include "licence_service.h"
#include <cstring>
#include <cstdio>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>

namespace ktv {

bool LicenceService::initialize() {
    // 从本地文件加载licence信息
    if (loadFromFile()) {
        // 检查状态
        if (licence_info_.status == LicenceStatus::Activated) {
            // 检查是否过期
            if (isExpired()) {
                licence_info_.status = LicenceStatus::Expired;
                saveToFile();
            }
        } else if (licence_info_.status == LicenceStatus::Trial) {
            // 试用期，检查是否超过试用次数
            if (licence_info_.trial_count >= LicenceInfo::MAX_TRIAL_COUNT) {
                // 超过试用期，需要激活
                // 状态保持为Trial，但needActivation()会返回true
            }
        }
        return true;
    }
    
    // 没有licence信息，状态为未激活
    licence_info_.status = LicenceStatus::NotActivated;
    return true;
}

bool LicenceService::verifyLicence(const char* licence_code) {
    // 1. 获取MAC地址
    char mac[18] = {0};
    if (!getMacAddress(mac, sizeof(mac))) {
        return false;
    }
    
    // 2. 发送到服务器验证
    // 使用libcurl发送POST请求
    cJSON* request = cJSON_CreateObject();
    cJSON_AddStringToObject(request, "licence", licence_code);
    cJSON_AddStringToObject(request, "mac_address", mac);
    
    char* json_string = cJSON_Print(request);
    
    HttpResponse response;
    if (!HttpService::getInstance().post("/api/licence/verify", 
                                         json_string, 
                                         response)) {
        cJSON_Delete(request);
        free(json_string);
        return false;
    }
    
    cJSON_Delete(request);
    free(json_string);
    
    // 3. 解析服务器响应
    cJSON* json = cJSON_Parse(response.body.data());
    if (!json) {
        return false;
    }
    
    cJSON* valid = cJSON_GetObjectItem(json, "valid");
    if (!cJSON_IsTrue(valid)) {
        cJSON_Delete(json);
        licence_info_.status = LicenceStatus::Invalid;
        return false;
    }
    
    // 4. 获取服务器返回的信息
    cJSON* duration = cJSON_GetObjectItem(json, "duration");
    cJSON* start_time = cJSON_GetObjectItem(json, "start_time");
    
    if (duration) {
        licence_info_.duration_months = cJSON_GetNumberValue(duration);
    }
    
    if (start_time) {
        licence_info_.start_time = cJSON_GetNumberValue(start_time);
    } else {
        licence_info_.start_time = std::time(nullptr);
    }
    
    // 5. 保存licence信息
    licence_info_.setLicenceCode(licence_code);
    licence_info_.setMacAddress(mac);
    licence_info_.status = LicenceStatus::Trial;  // 初始状态为试用期
    licence_info_.trial_count = 0;
    
    // 计算过期时间（激活后）
    calculateExpireTime();
    
    // 6. 保存到本地文件
    saveToFile();
    
    cJSON_Delete(json);
    return true;
}

bool LicenceService::canUse() const {
    if (licence_info_.status == LicenceStatus::NotActivated) {
        return false;  // 未激活，不能使用
    }
    
    if (licence_info_.status == LicenceStatus::Invalid) {
        return false;  // 无效，不能使用
    }
    
    if (licence_info_.status == LicenceStatus::Expired) {
        return false;  // 已过期，不能使用
    }
    
    if (licence_info_.status == LicenceStatus::Trial) {
        // 试用期，检查是否超过试用次数
        if (licence_info_.trial_count >= LicenceInfo::MAX_TRIAL_COUNT) {
            return false;  // 超过试用期，需要激活
        }
        return true;  // 试用期内，可以使用
    }
    
    if (licence_info_.status == LicenceStatus::Activated) {
        // 已激活，检查是否过期
        if (isExpired()) {
            return false;  // 已过期
        }
        return true;  // 有效期内，可以使用
    }
    
    return false;
}

void LicenceService::incrementTrialCount() {
    if (licence_info_.status == LicenceStatus::Trial) {
        licence_info_.trial_count++;
        saveToFile();  // 保存试用次数
    }
}

bool LicenceService::needActivation() const {
    if (licence_info_.status == LicenceStatus::Trial) {
        // 试用期，检查是否超过试用次数
        return (licence_info_.trial_count >= LicenceInfo::MAX_TRIAL_COUNT);
    }
    return false;
}

bool LicenceService::activate() {
    if (licence_info_.status != LicenceStatus::Trial) {
        return false;  // 只能在试用期激活
    }
    
    // 激活：设置开始时间，计算过期时间
    if (licence_info_.start_time == 0) {
        licence_info_.start_time = std::time(nullptr);
    }
    calculateExpireTime();
    
    licence_info_.status = LicenceStatus::Activated;
    saveToFile();
    
    return true;
}

bool LicenceService::isExpired() const {
    if (licence_info_.status != LicenceStatus::Activated) {
        return false;  // 只有激活状态才检查过期
    }
    
    if (licence_info_.expire_time == 0) {
        return false;  // 未设置过期时间
    }
    
    std::time_t now = std::time(nullptr);
    return (now >= licence_info_.expire_time);
}

int32_t LicenceService::getRemainingDays() const {
    if (licence_info_.status != LicenceStatus::Activated) {
        return 0;
    }
    
    if (licence_info_.expire_time == 0) {
        return 0;
    }
    
    std::time_t now = std::time(nullptr);
    if (now >= licence_info_.expire_time) {
        return 0;  // 已过期
    }
    
    std::time_t diff = licence_info_.expire_time - now;
    return static_cast<int32_t>(diff / (24 * 60 * 60));  // 转换为天数
}

int32_t LicenceService::getRemainingTrialCount() const {
    if (licence_info_.status != LicenceStatus::Trial) {
        return 0;
    }
    
    int32_t remaining = LicenceInfo::MAX_TRIAL_COUNT - licence_info_.trial_count;
    return (remaining > 0) ? remaining : 0;
}

void LicenceService::calculateExpireTime() {
    if (licence_info_.duration_months <= 0) {
        licence_info_.expire_time = 0;
        return;
    }
    
    if (licence_info_.start_time == 0) {
        licence_info_.start_time = std::time(nullptr);
    }
    
    // 计算过期时间（开始时间 + 月数）
    // 简化处理：假设每月30天
    std::time_t seconds_per_month = 30 * 24 * 60 * 60;
    licence_info_.expire_time = licence_info_.start_time + 
                                (seconds_per_month * licence_info_.duration_months);
}

void LicenceService::saveToFile() {
    // 使用cJSON保存licence信息
    cJSON* json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "licence_code", licence_info_.getLicenceCode());
    cJSON_AddStringToObject(json, "mac_address", licence_info_.getMacAddress());
    cJSON_AddNumberToObject(json, "status", static_cast<int>(licence_info_.status));
    cJSON_AddNumberToObject(json, "duration_months", licence_info_.duration_months);
    cJSON_AddNumberToObject(json, "start_time", licence_info_.start_time);
    cJSON_AddNumberToObject(json, "expire_time", licence_info_.expire_time);
    cJSON_AddNumberToObject(json, "trial_count", licence_info_.trial_count);
    
    char* json_string = cJSON_Print(json);
    FILE* file = fopen(LICENCE_FILE, "w");
    if (file) {
        fwrite(json_string, 1, strlen(json_string), file);
        fclose(file);
    }
    
    free(json_string);
    cJSON_Delete(json);
}

bool LicenceService::loadFromFile() {
    FILE* file = fopen(LICENCE_FILE, "r");
    if (!file) {
        return false;
    }
    
    char buffer[1024] = {0};
    size_t len = fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);
    
    cJSON* json = cJSON_ParseWithLength(buffer, len);
    if (!json) {
        return false;
    }
    
    cJSON* licence_code = cJSON_GetObjectItem(json, "licence_code");
    cJSON* mac_address = cJSON_GetObjectItem(json, "mac_address");
    cJSON* status = cJSON_GetObjectItem(json, "status");
    cJSON* duration_months = cJSON_GetObjectItem(json, "duration_months");
    cJSON* start_time = cJSON_GetObjectItem(json, "start_time");
    cJSON* expire_time = cJSON_GetObjectItem(json, "expire_time");
    cJSON* trial_count = cJSON_GetObjectItem(json, "trial_count");
    
    if (licence_code) {
        licence_info_.setLicenceCode(cJSON_GetStringValue(licence_code));
    }
    if (mac_address) {
        licence_info_.setMacAddress(cJSON_GetStringValue(mac_address));
    }
    if (status) {
        licence_info_.status = static_cast<LicenceStatus>(cJSON_GetNumberValue(status));
    }
    if (duration_months) {
        licence_info_.duration_months = cJSON_GetNumberValue(duration_months);
    }
    if (start_time) {
        licence_info_.start_time = cJSON_GetNumberValue(start_time);
    }
    if (expire_time) {
        licence_info_.expire_time = cJSON_GetNumberValue(expire_time);
    }
    if (trial_count) {
        licence_info_.trial_count = cJSON_GetNumberValue(trial_count);
    }
    
    cJSON_Delete(json);
    return true;
}

bool LicenceService::getMacAddress(char* out_mac, size_t max_len) {
    // 获取网络接口的MAC地址
    // 使用标准Linux系统调用
    struct ifreq ifr;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        return false;
    }
    
    // 获取eth0接口的MAC地址（根据实际情况调整接口名）
    strncpy(ifr.ifr_name, "eth0", IFNAMSIZ - 1);
    if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0) {
        close(sock);
        return false;
    }
    
    close(sock);
    
    // 格式化MAC地址
    unsigned char* mac = (unsigned char*)ifr.ifr_hwaddr.sa_data;
    snprintf(out_mac, max_len, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    return true;
}

} // namespace ktv
```

---

## 三、UI集成

### 3.1 首次启动输入Licence

```cpp
// 首次启动时检查licence
void onAppStart() {
    LicenceService& licence_service = LicenceService::getInstance();
    licence_service.initialize();
    
    if (licence_service.getStatus() == LicenceStatus::NotActivated) {
        // 显示输入licence对话框
        showLicenceInputDialog();
    }
}

// 显示licence输入对话框
void showLicenceInputDialog() {
    // 使用LVGL创建输入对话框
    lv_obj_t* dialog = lv_msgbox_create(lv_scr_act(), "输入Licence", 
                                        "请输入您的licence码", NULL, true);
    // ... 添加输入框和按钮
}

// 用户输入licence后验证
void onLicenceInput(const char* licence_code) {
    LicenceService& licence_service = LicenceService::getInstance();
    
    if (licence_service.verifyLicence(licence_code)) {
        // 验证成功
        showMessage("Licence验证成功！");
    } else {
        // 验证失败
        showMessage("Licence验证失败，请检查输入！");
    }
}
```

### 3.2 点唱时检查Licence

```cpp
// 点唱歌曲时检查licence
void onSongItemClicked(const SongItem& song) {
    LicenceService& licence_service = LicenceService::getInstance();
    
    // 检查是否可以使用
    if (!licence_service.canUse()) {
        // 检查原因
        if (licence_service.getStatus() == LicenceStatus::NotActivated) {
            showMessage("请先输入licence！");
            showLicenceInputDialog();
            return;
        }
        
        if (licence_service.needActivation()) {
            // 超过试用期，显示激活提示
            showActivationDialog();
            return;
        }
        
        if (licence_service.isExpired()) {
            // 已过期，显示续费提示
            showRenewalDialog();
            return;
        }
        
        showMessage("Licence无效，无法使用！");
        return;
    }
    
    // 可以使用，增加试用次数（如果是试用期）
    if (licence_service.getStatus() == LicenceStatus::Trial) {
        licence_service.incrementTrialCount();
    }
    
    // 播放歌曲
    PlayerService::getInstance().play(song);
}
```

### 3.3 激活提示对话框

```cpp
// 显示激活提示对话框
void showActivationDialog() {
    LicenceService& licence_service = LicenceService::getInstance();
    int32_t remaining = licence_service.getRemainingTrialCount();
    
    char message[256];
    snprintf(message, sizeof(message), 
             "试用期已用完（已点唱%d首）\n请扫码激活以继续使用", 
             LicenceInfo::MAX_TRIAL_COUNT);
    
    // 使用LVGL创建对话框，包含二维码
    lv_obj_t* dialog = lv_msgbox_create(lv_scr_act(), "需要激活", 
                                        message, NULL, true);
    // ... 添加二维码显示和激活按钮
}

// 扫码激活后调用
void onActivationSuccess() {
    LicenceService& licence_service = LicenceService::getInstance();
    
    if (licence_service.activate()) {
        showMessage("激活成功！");
    } else {
        showMessage("激活失败，请重试！");
    }
}
```

### 3.4 续费提示对话框

```cpp
// 显示续费提示对话框
void showRenewalDialog() {
    LicenceService& licence_service = LicenceService::getInstance();
    int32_t remaining_days = licence_service.getRemainingDays();
    
    char message[256];
    snprintf(message, sizeof(message), 
             "Licence已过期\n请续费以继续使用");
    
    // 使用LVGL创建对话框
    lv_obj_t* dialog = lv_msgbox_create(lv_scr_act(), "需要续费", 
                                        message, NULL, true);
    // ... 添加续费按钮和二维码
}
```

---

## 四、服务器API设计

### 4.1 验证Licence API

**请求：**
```
POST /api/licence/verify
Content-Type: application/json

{
  "licence": "XXXX-XXXX-XXXX-XXXX",
  "mac_address": "AA:BB:CC:DD:EE:FF"
}
```

**响应（成功 - 首次激活）：**
```json
{
  "valid": true,
  "duration": 12,
  "start_time": 1234567890,
  "is_reactivated": false,
  "message": "验证成功"
}
```

**响应（成功 - 系统重置后重新激活）：**
```json
{
  "valid": true,
  "duration": 12,
  "start_time": 1234567890,
  "expire_time": 1234567890,
  "remaining_days": 180,
  "is_reactivated": true,
  "trial_count": 5,
  "message": "验证成功，已恢复剩余时间"
}
```

**响应（失败）：**
```json
{
  "valid": false,
  "message": "Licence无效或已被使用"
}
```

**服务器端逻辑：**
1. 服务器根据licence和MAC地址查询绑定记录
2. 如果找到绑定记录（MAC地址匹配）：
   - 返回`is_reactivated: true`
   - 返回`expire_time`（过期时间）
   - 返回`remaining_days`（剩余天数）
   - 返回`trial_count`（试用次数，如果还在试用期）
3. 如果没有找到绑定记录（首次激活）：
   - 创建新的绑定记录
   - 返回`is_reactivated: false`
   - 返回`duration`和`start_time`

### 4.2 激活Licence API（可选）

**请求：**
```
POST /api/licence/activate
Content-Type: application/json

{
  "licence": "XXXX-XXXX-XXXX-XXXX",
  "mac_address": "AA:BB:CC:DD:EE:FF"
}
```

**响应：**
```json
{
  "success": true,
  "start_time": 1234567890,
  "expire_time": 1234567890,
  "message": "激活成功"
}
```

---

## 五、使用示例

### 5.1 应用启动时初始化

```cpp
int main() {
    // 初始化LVGL
    lv_init();
    
    // 初始化HTTP服务
    HttpService::getInstance().initialize();
    
    // 初始化Licence服务
    LicenceService::getInstance().initialize();
    
    // 检查licence状态
    if (LicenceService::getInstance().getStatus() == LicenceStatus::NotActivated) {
        // 显示输入licence对话框
        showLicenceInputDialog();
    }
    
    // 主循环
    while (1) {
        lv_timer_handler();
        usleep(5000);
    }
    
    return 0;
}
```

### 5.2 点唱时检查

```cpp
void onSongItemClicked(const SongItem& song) {
    LicenceService& licence_service = LicenceService::getInstance();
    
    // 检查是否可以使用
    if (!licence_service.canUse()) {
        handleLicenceError();
        return;
    }
    
    // 增加试用次数（如果是试用期）
    if (licence_service.getStatus() == LicenceStatus::Trial) {
        licence_service.incrementTrialCount();
    }
    
    // 播放歌曲
    PlayerService::getInstance().play(song);
}

void handleLicenceError() {
    LicenceService& licence_service = LicenceService::getInstance();
    
    switch (licence_service.getStatus()) {
        case LicenceStatus::NotActivated:
            showLicenceInputDialog();
            break;
        case LicenceStatus::Trial:
            if (licence_service.needActivation()) {
                showActivationDialog();
            }
            break;
        case LicenceStatus::Expired:
            showRenewalDialog();
            break;
        default:
            showMessage("Licence无效，无法使用！");
            break;
    }
}
```

---

## 六、总结

### 6.1 核心组件

1. **LicenceService**：licence管理服务（单例，预分配版本）
2. **LicenceInfo**：licence信息数据模型（固定大小缓冲区）
3. **服务器API**：licence验证和激活接口
4. **UI对话框**：输入、激活、续费提示

### 6.2 技术要点

- ✅ **使用libcurl**：发送HTTP请求到服务器验证（40,000+ stars，避免造轮子）
- ✅ **使用cJSON**：解析服务器响应和本地存储（10,000+ stars，避免造轮子）
- ✅ **使用标准库**：获取MAC地址（`ioctl`, `socket`等）
- ✅ **预分配内存**：固定大小数组和缓冲区
- ✅ **单线程设计**：无锁设计
- ✅ **本地持久化**：licence信息保存到JSON文件

### 6.3 状态管理

- **NotActivated**：未激活（未输入licence）
- **Trial**：试用期（0-20首）
- **Activated**：已激活（正式使用）
- **Expired**：已过期
- **Invalid**：无效（验证失败）

---

**总结**：licence管理系统包括首次输入验证、试用期管理（20首）、激活流程、时间验证和过期提示。使用libcurl和cJSON实现服务器验证和本地存储，预分配内存，单线程设计，无锁实现。


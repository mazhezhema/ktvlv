/**
 * @file input_driver.h
 * @brief 输入驱动抽象接口（跨平台统一接口）
 * 
 * 支持：
 * - 触摸屏（pointer）
 * - 遥控器/键盘（keypad）
 * - 编码器（encoder，可选）
 */

#ifndef KTVLV_DRIVERS_INPUT_DRIVER_H
#define KTVLV_DRIVERS_INPUT_DRIVER_H

#include <lvgl.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 输入设备类型
 */
typedef enum {
    INPUT_TYPE_POINTER = 0,  // 触摸屏/鼠标
    INPUT_TYPE_KEYPAD,       // 遥控器/键盘
    INPUT_TYPE_ENCODER        // 编码器（可选）
} input_device_type_t;

/**
 * @brief 输入驱动接口结构体
 */
typedef struct {
    /**
     * @brief 初始化输入驱动
     * @return true 成功, false 失败
     */
    int (*init)(void);
    
    /**
     * @brief 注册 LVGL 输入设备
     * 
     * 平台实现负责：
     * 1. 创建 lv_indev_drv_t
     * 2. 设置 read_cb
     * 3. 调用 lv_indev_drv_register()
     * 
     * @param type 输入设备类型
     * @return lv_indev_t* 注册的输入设备指针，失败返回 NULL
     */
    lv_indev_t* (*register_device)(input_device_type_t type);
    
    /**
     * @brief 处理平台特定输入事件（可选）
     * 
     * 用于在主循环中处理平台事件
     * 例如：SDL_Event, evdev 事件等
     * 
     * @param event_data 平台特定事件数据（void* 通用指针）
     * @return true 事件已处理, false 未处理
     */
    bool (*process_event)(void *event_data);
    
    /**
     * @brief 反初始化输入驱动
     */
    void (*deinit)(void);
} input_iface_t;

/**
 * @brief 全局输入驱动接口实例
 * 
 * 在平台特定实现中定义并初始化
 */
extern input_iface_t INPUT;

#ifdef __cplusplus
}
#endif

#endif  // KTVLV_DRIVERS_INPUT_DRIVER_H


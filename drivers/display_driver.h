/**
 * @file display_driver.h
 * @brief 显示驱动抽象接口（跨平台统一接口）
 * 
 * 设计原则：
 * - 所有平台实现必须遵循此接口
 * - UI层和服务层不感知具体平台实现
 * - 支持快速切换平台（Windows SDL <-> F133 Linux）
 */

#ifndef KTVLV_DRIVERS_DISPLAY_DRIVER_H
#define KTVLV_DRIVERS_DISPLAY_DRIVER_H

#include <lvgl.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 显示驱动接口结构体
 * 
 * 所有平台实现必须提供这三个函数指针
 */
typedef struct {
    /**
     * @brief 初始化显示驱动
     * @return true 成功, false 失败
     */
    int (*init)(void);
    
    /**
     * @brief LVGL 刷新回调（flush callback）
     * @param drv 显示驱动指针
     * @param area 需要刷新的区域
     * @param color 颜色数据缓冲区
     * 
     * 注意：实现后必须调用 lv_disp_flush_ready(drv)
     */
    void (*flush)(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color);
    
    /**
     * @brief 反初始化显示驱动（清理资源）
     */
    void (*deinit)(void);
    
    /**
     * @brief 获取显示分辨率（可选，用于动态查询）
     * @param width 输出宽度
     * @param height 输出高度
     * @return true 成功, false 失败
     */
    bool (*get_resolution)(int32_t *width, int32_t *height);
} display_iface_t;

/**
 * @brief 全局显示驱动接口实例
 * 
 * 在平台特定实现中定义并初始化
 * 例如：
 *   - platform/f133_linux/display_fbdev.c: display_iface_t DISPLAY = {...}
 *   - platform/f133_linux/display_fbdev.c: display_iface_t DISPLAY = {...}
 */
extern display_iface_t DISPLAY;

#ifdef __cplusplus
}
#endif

#endif  // KTVLV_DRIVERS_DISPLAY_DRIVER_H




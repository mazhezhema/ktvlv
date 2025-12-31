/**
 * @file input_evdev.h
 * @brief F133 Linux evdev 输入驱动辅助函数
 */

#ifndef KTVLV_PLATFORM_F133_LINUX_INPUT_EVDEV_H
#define KTVLV_PLATFORM_F133_LINUX_INPUT_EVDEV_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 从 evdev 设备读取事件（在主循环中调用）
 * 
 * 注意：此函数需要在主循环中定期调用，以更新输入状态
 */
void evdev_read_events_exported(void);

#ifdef __cplusplus
}
#endif

#endif  // KTVLV_PLATFORM_F133_LINUX_INPUT_EVDEV_H



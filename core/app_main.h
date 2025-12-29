/**
 * @file app_main.h
 * @brief 应用主入口头文件
 */

#ifndef KTVLV_CORE_APP_MAIN_H
#define KTVLV_CORE_APP_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 应用主入口（跨平台统一入口）
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return 退出码（0 成功，非 0 失败）
 */
int app_main(int argc, char* argv[]);

#ifdef __cplusplus
}
#endif

#endif  // KTVLV_CORE_APP_MAIN_H


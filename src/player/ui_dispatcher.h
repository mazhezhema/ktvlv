// ui_dispatcher.h
#pragma once
#include <functional>

class UiDispatcher {
public:
    // 保证 task 在 LVGL 主线程执行
    static void post(const std::function<void()>& task);
};



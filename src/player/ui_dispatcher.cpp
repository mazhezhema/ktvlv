// ui_dispatcher.cpp
#include "ui_dispatcher.h"

extern "C" {
    #include <lvgl.h>
}

void UiDispatcher::post(const std::function<void()>& task) {
    auto heapTask = new std::function<void()>(task);
    lv_async_call(
        [](void* data){
            auto fn = static_cast<std::function<void()>*>(data);
            (*fn)();
            delete fn;
        },
        heapTask
    );
}





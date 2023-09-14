#pragma once

#include "types.h"


#ifdef ESP_PLATFORM
#include <esp_timer.h>
namespace KhawasuOsApi
{
    inline u64 get_microseconds() {
        return esp_timer_get_time();
    }
}
#else
#include <chrono>
namespace KhawasuOsApi
{
    inline u64 get_microseconds() {
        return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    }
}
#endif

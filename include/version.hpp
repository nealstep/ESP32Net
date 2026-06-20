#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#endif // ARDUINO

namespace Version {
    static constexpr const char* const module = "ESP32Net";    
    static constexpr const char* const git_version = GIT_VERSION;
    static constexpr const char* const firmware_version = FIRMWARE_VERSION;
    static constexpr const char* const build_time = __DATE__ " " __TIME__;
}  // namespace Version

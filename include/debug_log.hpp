#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <iostream>
#endif // ARDUINO

#define _DL_OFF 0
#define _DL_ALL 0xFF

// default debug level
#ifndef DB_LVL
#define DB_LVL _DL_OFF
#endif

// actual debug macro, to be used like printf
#if defined(LOG_SERIAL)
#define DEBUG_LOG(lvl, fmt, ...) \
    if ((lvl) & DB_LVL)          \
    LOG_SERIAL.printf("[%s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#elif defined(LOG_STDERR)
#define DEBUG_LOG(lvl, fmt, ...)
if ((lvl)&DB_LVL)
{
    char _log_buf[256];
    std::snprintf(_log_buf, sizeof(_log_buf), fmt, ##__VA_ARGS__);
    std::cerr << "[" << __FILE__ << ":" << __LINE__ << "] " << _log_buf
              << "\n";
}
#else
#define DEBUG_LOG(lvl, fmt, ...)
#endif // LOG_SERIAL

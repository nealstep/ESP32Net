#include <Arduino.h>

#ifdef IS_M5
#include <M5Unified.h>
#endif // IS_M5

#include "debug_log.hpp"

// debug flag (must be unique)
#define _DL_MAIN (1 << 0) // 1

namespace Config
{
  // delays
  static constexpr uint16_t startup_delay = 2000;
}

#include "esp32net.hpp"

void setup()
{
  delay(Config::startup_delay);
  DEBUG_LOG(_DL_MAIN, "Starting");
  esp32Net.init();
  // do starup here
  DEBUG_LOG(_DL_MAIN, "Stared");
}

// handle network events from queue
void events_check(void)
{
  ESP32Net::NetMessage netMsg;
  if (xQueueReceive(esp32Net.netQueue, &netMsg, 0) == pdTRUE)
  {
    switch (netMsg.type)
    {
    case ESP32Net::NetMessage::Connected:
      DEBUG_LOG(_DL_MAIN, "events_check: connected");
      break;
    case ESP32Net::NetMessage::GotIP:
      DEBUG_LOG(_DL_MAIN, "events_check: got ip");
      esp32Net.check_internet();
      break;
    case ESP32Net::NetMessage::Disconnected:
      DEBUG_LOG(_DL_MAIN, "events_check: disconnected");
      break;
    case ESP32Net::NetMessage::TimeSynced:
      if (HAVE_RTC)
      {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo))
        {
#ifdef IS_M5
          M5.Rtc.setDateTime(&timeinfo);
#endif // IS_M5
        }
      }
      break;
    }
  }
  ESP32Net::UDPMessage udpMsg;
  if (xQueueReceive(esp32Net.udpQueue, &udpMsg, 0) == pdTRUE)
  {
    DEBUG_LOG(_DL_MAIN, "events_check: received UDP packet from %s: %s",
              udpMsg.remoteIP.toString().c_str(),
              udpMsg.data);
    // handle_message(udpMsg.data, udpMsg.remoteIP);
  }
}

void loop()
{
  events_check();
  ota_check();
}

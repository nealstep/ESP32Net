#ifdef ARDUINO

#include <Arduino.h>
#include <TaskScheduler.h>

#ifdef IS_M5
#include <M5Unified.h>
#endif  // IS_M5

#include "debug_log.hpp"

// debug flag (must be unique)
#define _DL_MAIN (1 << 0)  // 1

namespace Config {
    // delays
    static constexpr uint16_t startup_delay = 2000;
    static constexpr uint16_t medium_delay = 250;
    static constexpr uint32_t loop_interval = 250000;

    // timing information
    static constexpr uint32_t sec_ms = 1000UL;
    static constexpr uint32_t min_sec = 60UL;
    static constexpr uint32_t send_keep_alive_msg_int = 15 * sec_ms;
    static constexpr uint32_t check_internet_int = 1 * min_sec * sec_ms;

    // sizes
    static constexpr uint8_t name_size = 24;

}  // namespace Config

char device_name[Config::name_size] = "dummy";

#include "esp32net.hpp"

// scheduler
Scheduler runner;

uint32_t loop_counter = 0;

// task wrappers
void keepAliveMsg() {
    esp32Net.broadcast_str("Keep alive ");
}
void checkInternet() {
    esp32Net.check_internet();
}

// create tasks
Task taskSendKeepAliveMsg(Config::send_keep_alive_msg_int, TASK_FOREVER,
                          &keepAliveMsg);
Task taskCheckInternet(Config::check_internet_int, TASK_FOREVER,
                       &checkInternet);

// array of tasks to be added to the scheduler
Task* tasks[] = {&taskSendKeepAliveMsg, &taskCheckInternet};

// terminal death
void die(void) {
    DEBUG_LOG(_DL_MAIN, "died");
    while (true) {
        delay(Config::medium_delay);
    }
}

void setup(void) {
#ifdef LOG_SERIAL
    Serial.begin(SERIAL_SPEED);
#endif  // LOG_SERIAL
    delay(Config::startup_delay);
    DEBUG_LOG(_DL_MAIN, "Starting");
    // do starup here
    Serial.println("Starting setup");
    auto err = esp32Net.init();
    if (err != ESP32Net::Error::Code::NoError) {
        DEBUG_LOG(_DL_MAIN, "ESP32Net failed: %s",
                  ESP32Net::Error::toString(err));
        die();
    }
    // enable and start tasks
    for (auto& task : tasks) {
        runner.addTask(*task);
        task->enable();
    }
    DEBUG_LOG(_DL_MAIN, "Started");
    Serial.println("Finished setup");
}

// handle network events from queue
void events_check(void) {
    ESP32Net::NetMessage netMsg;
    if (xQueueReceive(esp32Net.netQueue, &netMsg, 0) == pdTRUE) {
        switch (netMsg.type) {
            case ESP32Net::NetMessage::Type::Connected:
                DEBUG_LOG(_DL_MAIN, "events_check: connected");
                break;
            case ESP32Net::NetMessage::Type::GotIP:
                DEBUG_LOG(_DL_MAIN, "events_check: got ip");
                esp32Net.check_internet();
                break;
            case ESP32Net::NetMessage::Type::Disconnected:
                DEBUG_LOG(_DL_MAIN, "events_check: disconnected");
                break;
            case ESP32Net::NetMessage::Type::TimeSynced:
                DEBUG_LOG(_DL_MAIN, "events_check: time sync");
                if (HAVE_RTC) {
                    struct tm timeinfo;
                    if (getLocalTime(&timeinfo)) {
#ifdef IS_M5
                        M5.Rtc.setDateTime(&timeinfo);
#endif  // IS_M5
                    }
                }
                break;
            case ESP32Net::NetMessage::Type::InternetConnected:
                DEBUG_LOG(_DL_MAIN, "events_check: internet connected");
                break;
            case ESP32Net::NetMessage::Type::NoInternet:
                DEBUG_LOG(_DL_MAIN, "events_check: no internet");
                break;
            case ESP32Net::NetMessage::Type::TimeSyncFailed:
                DEBUG_LOG(_DL_MAIN, "events_check: time sync failed");
                break;
            default:
                // unknown message
                break;
        }
    }
    ESP32Net::UDPMessage udpMsg;
    if (xQueueReceive(esp32Net.udpQueue, &udpMsg, 0) == pdTRUE) {
        DEBUG_LOG(_DL_MAIN, "events_check: received UDP packet from %s: %s",
                  udpMsg.remoteIP.toString().c_str(), udpMsg.data);
        // handle_message(udpMsg.data, udpMsg.remoteIP);
    }
}

#ifdef IS_M5
// M5 updates, button handling etc may also go here
void updateM5(void) {
    M5.update();
}
#endif

void loop(void) {
    if (loop_counter > Config::loop_interval) {
        DEBUG_LOG(_DL_MAIN, "looped %d times", loop_counter);
        loop_counter = 0;
    }
    events_check();
    ota_check();
#ifdef IS_M5
    updateM5();
#endif
    // check if jobs need running
    runner.execute();
    loop_counter++;
}

#else  // !ARDUINO

#include <iostream>

#include "circular_queue.hpp"

static constexpr size_t test_buffer_size = 10;
static constexpr uint16_t test_entries = 3;

#ifndef PIO_UNIT_TESTING
int main(int argc, char* argv[]) {
    CircularQueue circularQueue(test_buffer_size, test_entries);
    std::cout << "Starting" << std::endl;
    std::cout << "Free Capacity: " << circularQueue.get_free_capacity() << std::endl;
    std::cout << "Free Entries: " << circularQueue.get_free_entries() << std::endl;
    std::cout << "Done" << std::endl;
}
#endif

#endif  // ARDUINO

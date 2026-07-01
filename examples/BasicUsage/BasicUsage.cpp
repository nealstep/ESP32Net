
#include "esp32net.hpp"
#include "log.hpp"

#ifdef ARDUINO

#include <Arduino.h>
#include <TaskScheduler.h>

#ifdef IS_M5
#include <M5Unified.h>
#endif  // IS_M5

#endif  // Arduino

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
}  // namespace Config

namespace Global {
uint32_t loop_counter = 0;
}

#ifdef ARDUINO

// scheduler
Scheduler runner;

// task wrappers
void keepAliveMsg() {
    esp32Net.broadcast_str(lg.get_message(Log::Note::KeepAlive));
}
void checkInternet() { esp32Net.check_internet(); }

// create tasks
Task taskSendKeepAliveMsg(Config::send_keep_alive_msg_int, TASK_FOREVER,
                          &keepAliveMsg);
Task taskCheckInternet(Config::check_internet_int, TASK_FOREVER,
                       &checkInternet);

// array of tasks to be added to the scheduler
Task* tasks[] = {&taskSendKeepAliveMsg, &taskCheckInternet};

// terminal death
void die(void) {
    LOG_E(Log::Uni::Main, Log::Err::Died);
    while (true) {
        delay(Config::medium_delay);
    }
}

void setup() {
#ifdef LOG_SERIAL
    Serial.begin(SERIAL_SPEED);
#endif  // LOG_SERIAL
    delay(Config::startup_delay);
    LOG_N(Log::Uni::Main, Log::Sev::All, Log::Note::Starting);
    // do starup here
    Log::Err err = esp32Net.init();
    if (err != Log::Err::NoError) {
        LOG_E(Log::Uni::Main, err);
        die();
    }
    // enable and start tasks
    for (auto& task : tasks) {
        runner.addTask(*task);
        task->enable();
    }
    LOG_N(Log::Uni::Main, Log::Sev::Inf, Log::Note::Started);
}


// handle network events from queue
void events_check(void) {
    ESP32Net::NetMessage netMsg;
    if (xQueueReceive(esp32Net.netQueue, &netMsg, 0) == pdTRUE) {
        switch (netMsg.type) {
            case ESP32Net::NetMessage::Type::Connected:
                LOG_N(Log::Uni::Main, Log::Sev::All, Log::Note::Connected);
                break;
            case ESP32Net::NetMessage::Type::GotIP:
                LOG_N(Log::Uni::Main, Log::Sev::All, Log::Note::GotIP,
                      esp32Net.get_ip().toString().c_str());
                esp32Net.check_internet();
                break;
            case ESP32Net::NetMessage::Type::Disconnected:
                LOG_N(Log::Uni::Main, Log::Sev::All, Log::Note::Disconnected);
                break;
            case ESP32Net::NetMessage::Type::TimeSynced:
                LOG_N(Log::Uni::Main, Log::Sev::All, Log::Note::TimeSynced);
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
                LOG_N(Log::Uni::Main, Log::Sev::All, Log::Note::InternetConnected);
                break;
            case ESP32Net::NetMessage::Type::NoInternet:
                LOG_E(Log::Uni::Main, Log::Err::NoInternet);
                break;
            case ESP32Net::NetMessage::Type::TimeSyncFailed:
                LOG_E(Log::Uni::Main, Log::Err::TimeSyncFailed);
                break;
            default:
                LOG_E(Log::Uni::Main, Log::Err::UnknownMessage);
                break;
        }
    }
    ESP32Net::UDPMessage udpMsg;
    if (xQueueReceive(esp32Net.udpQueue, &udpMsg, 0) == pdTRUE) {
        LOG_N(Log::Uni::Main, Log::Sev::All, Log::Note::ReceivedUDPPacket,
              udpMsg.remoteIP.toString().c_str(), udpMsg.data);
        // handle_message(udpMsg.data, udpMsg.remoteIP);
    }
}

#ifdef IS_M5
// M5 updates, button handling etc may also go here
void updateM5(void) { M5.update(); }
#endif

void loop(void) {
    if (Global::loop_counter > Config::loop_interval) {
        LOG_N(Log::Uni::Main, Log::Sev::Inf, Log::Note::LoopedN,
              Global::loop_counter);
        Global::loop_counter = 0;
    }
    events_check();
    ota_check();    
#ifdef IS_M5
    updateM5();
#endif
    // check if jobs need running
    runner.execute();
    Global::loop_counter++;
}

#else  // !ARDUINO

#include <iostream>

#ifndef PIO_UNIT_TESTING

void test(void) {
    uint16_t times = 0;
    LOG_N(Log::Uni::Unnamed, Log::Sev::All, Log::Note::Starting);
    LOG_N(Log::Uni::Main, Log::Sev::Err, Log::Note::Started);
    std::cout << std::hex
              << "Unit: " << static_cast<uint32_t>(lg.get_unit_mask())
              << std::endl;
    std::cout << "Sev: " << static_cast<uint16_t>(lg.get_severity())
              << std::endl;
    std::cout << std::dec << "loops: " << Global::loop_counter << std::endl;
    std::cout << "times: " << times << std::endl;
    while (times < 3) {
        if (Global::loop_counter > Config::loop_interval) {
            LOG_N(Log::Uni::Main, Log::Sev::Inf, Log::Note::LoopedN,
                  Global::loop_counter);
            Global::loop_counter = 0;
            times++;
        }
        Global::loop_counter++;
    }
    LOG_E(Log::Uni::Main, Log::Err::UnexpectedError);
    LOG(Log::Uni::Main, Log::Sev::Wrn, "A test %d", 45);
    std::cout << "loops: " << Global::loop_counter << std::endl;
    std::cout << "times: " << times << std::endl;
    LOG_N(Log::Uni::Main, Log::Sev::Inf, Log::Note::Done);
}

int main(int argc, char* argv[]) {
    // basic log test
    std::cout << "Everything" << std::endl;
    test();
    std::cout << "Only Errors+" << std::endl;
    lg.set_severity(Log::Sev::Err);
    test();

    // turn on full logging
    lg.set_unit_mask(Log::Uni::Last);
    lg.set_severity(Log::Sev::Dbg);

    // basic circular queue test

    static constexpr size_t test_buffer_size = 10;
    static constexpr uint16_t test_entries = 3;
    CircularQueue circularQueue(test_buffer_size, test_entries);
    LOG_N(Log::Uni::Main, Log::Sev::Inf, Log::Note::Starting);
    LOG_N(Log::Uni::Main, Log::Sev::Inf, Log::Note::FreeCapacity,
          circularQueue.get_free_capacity());
    LOG_N(Log::Uni::Main, Log::Sev::Inf, Log::Note::FreeEntries,
          circularQueue.get_free_entries());
    LOG_N(Log::Uni::Main, Log::Sev::Inf, Log::Note::Done);
}
#endif  // PIO_UNIT_TESTING

#endif  // ARDUINO else

void log_output_impl(const char* str, bool error, bool truncated) {
    if (error) {
        // there was a an error expanding the string
#ifdef LOG_SERIAL
        LOG_SERIAL.printf("Format!: %s\n", str);
#elifdef LOG_STDERR
        std::cerr << "Format!: " << str << std::endl;
#endif  // LOG_SERIAL LOG_STDERR
    } else if (truncated) {
        // the line got truncated
#ifdef LOG_SERIAL
        LOG_SERIAL.printf("Trunc!: %s\n", str);
#elifdef LOG_STDERR
        std::cerr << "Trunc!: " << str << std::endl;
#endif  // LOG_SERIAL LOG_STDERR
    } else {
        // all is good
#ifdef LOG_SERIAL
        LOG_SERIAL.println(str);
#elifdef LOG_STDERR
        std::cerr << str << std::endl;
#endif  // LOG_SERIAL LOG_STDERR
    }
}

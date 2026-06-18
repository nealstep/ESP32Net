#pragma once

#include <Arduino.h>

// debug flag (must be unique)
#define _DL_NET (1 << 1) // 2

// sizes
#define UDP_MSG_SIZE 200
#define NET_QUEUE_SIZE 10
#define UDP_QUEUE_SIZE 10

// delays
#define MEDIUM_DELAY 250
#define LONG_DELAY 1000

// counts
#define TIME_SYNC_ATTEMPTS 5

// urls
#define INTERNET_CHECK_URL "http://clients3.google.com/generate_204"
#define NTP_SERVER_1 "0.pool.ntp.org"
#define NTP_SERVER_2 "1.pool.ntp.org"
#define NTP_SERVER_3 "2.pool.ntp.org"


class ESP32Net
{
public:
    typedef struct
    {
        enum Type
        {
            Connected,
            GotIP,
            Disconnected,
            TimeSynced
        } type;
        uint8_t code;
    } NetMessage;

    typedef struct
    {
        IPAddress remoteIP;
        char data[UDP_MSG_SIZE];
    } UDPMessage;

    static constexpr uint8_t noerr = 0;
    static constexpr uint8_t create_queue_failed = 1;
    static constexpr uint8_t time_sync_failed = 2;

    QueueHandle_t netQueue;
    QueueHandle_t udpQueue;

    // lazy singleton
    static ESP32Net& getInstance()
    {
        static ESP32Net instance;
        return instance;
    }
    ESP32Net(const ESP32Net &) = delete;
    ESP32Net &operator=(const ESP32Net &) = delete;

    uint8_t init(void);
    bool have_ip(void) { return ip_addr != INADDR_NONE; }
    void set_ip(IPAddress ip) { ip_addr = ip; }
    IPAddress get_ip(void) { return ip_addr; }
    bool have_internet(void) { return internet_connected; }
    bool check_internet(void);
    bool check_clock(void);
    // bool check_send_buffer(void);
    void send_net_msg(NetMessage::Type type, uint8_t code);

protected:
    // flags
    bool internet_connected = false;

    // ip address
    IPAddress ip_addr = INADDR_NONE;

    // hidden creator
    ESP32Net() {};
};

static ESP32Net& esp32Net = ESP32Net::getInstance();

void ota_check(void);

// bool check_internet(void) { return esp32Net.check_internet(); }

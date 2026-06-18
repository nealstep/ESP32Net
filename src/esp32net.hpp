#pragma once

#include <Arduino.h>

// debug flag (must be unique)
#define _DL_NET (1 << 1) // 2

class ESP32Net
{
public:
    class Config
    {
    public:
        // sizes
        static constexpr uint8_t udp_msg_size = 200;
        static constexpr uint8_t net_queue_size = 10;
        static constexpr uint8_t udp_queue_size = 10;

        // delays
        static constexpr uint16_t medium_delay = 250;
        static constexpr uint16_t long_delay = 1000;

        // counts
        static constexpr uint8_t time_sync_attempts = 5;

        // sites
        static constexpr const char* internet_check_url = "http://clients3.google.com/generate_204";
        static constexpr const char* ntp_server_1 = "0.pool.ntp.org";
        static constexpr const char* ntp_server_2 = "1.pool.ntp.org";
        static constexpr const char* ntp_server_3 = "2.pool.ntp.org";
    };

    class Errors
    {
    public:
        static constexpr uint8_t noerr = 0;
        static constexpr uint8_t create_queue_failed = 1;
        static constexpr uint8_t time_sync_failed = 2;
    };

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
        char data[Config::udp_msg_size];
    } UDPMessage;

    QueueHandle_t netQueue;
    QueueHandle_t udpQueue;

    // lazy singleton
    static ESP32Net &getInstance()
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

static ESP32Net &esp32Net = ESP32Net::getInstance();

void ota_check(void);

// bool check_internet_t(void) { return esp32Net.check_internet(); }

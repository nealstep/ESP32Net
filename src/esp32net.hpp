#pragma once

#ifdef ARDUINO

#include <Arduino.h>

// debug flag (must be unique)
#define _DL_NET (1 << 1)  // 2

// broadcast ip
const IPAddress broadcastIP(255, 255, 255, 255);

#define ESP32NET_ERROR_LIST(X)                  \
    X(NoError, "No Error")                      \
    X(CreateQueueFailed, "Create Queue Failed") \
    X(TimeSyncFailed, "Time Sync Failed")       \
    X(NoNetwork, "No Network")                  \
    X(MessageTooBig, "Message Too Big")         \
    X(NoInternet, "No Internet")

class ESP32Net {
    protected:
        // make encryption default if on
        static constexpr bool use_aes = USE_AES;

    public:
        class Config {
            public:
                static constexpr uint8_t tz_size = 16;
                static constexpr uint8_t hex_key_size = 68;
                static constexpr uint8_t ssid_size = 24;
                static constexpr uint8_t passwd_size = 32;

                // variables coming from defines but can be overwritten
                static uint16_t udp_data_port;
                static char tz_full[tz_size];
                static char hex_key[hex_key_size];
                static char ssid[ssid_size];
                static char password[passwd_size];

            public:
                // code
                static constexpr uint8_t no_code = 0;

                // sizes
                static constexpr uint8_t udp_msg_size = 200;
                static constexpr uint8_t net_queue_size = 10;
                static constexpr uint8_t udp_queue_size = 10;

                // delays
                static constexpr uint16_t medium_delay = 250;
                static constexpr uint16_t long_delay = 1000;

                // counts
                static constexpr uint8_t time_sync_attempts = 5;

                // chars
                static constexpr char eos = '\0';

                // codes
                static constexpr uint8_t wait = 0;
                static constexpr uint8_t nocode = 0;
                static constexpr uint8_t httpcode = 204;

                // sites
                static constexpr const char* internet_check_url =
                    "http://clients3.google.com/generate_204";
                static constexpr const char* ntp_server_1 = "0.pool.ntp.org";
                static constexpr const char* ntp_server_2 = "1.pool.ntp.org";
                static constexpr const char* ntp_server_3 = "2.pool.ntp.org";
        };

        struct Error {
#define GENERATE_ENUM(name, string) name,
                enum class Code : uint8_t {
                    ESP32NET_ERROR_LIST(GENERATE_ENUM)
                };
#undef GENERATE_ENUM
#define GENERATE_STRING(name, string) \
    case Code::name:                  \
        return string;
                static const char* toString(Code code) {
                    switch (code) {
                        ESP32NET_ERROR_LIST(GENERATE_STRING)
                        default:
                            return "Unknown Error";
                    }
                }
#undef GENERATE_STRING

        };  // namespace Error

        typedef struct {
                enum class Type : uint8_t {
                    Connected = 0,
                    GotIP,
                    Disconnected,
                    TimeSynced,
                    TimeSyncFailed,
                    InternetConnected,
                    NoInternet
                } type;
                uint8_t code;
        } NetMessage;

        typedef struct {
                IPAddress remoteIP;
                char data[Config::udp_msg_size];
        } UDPMessage;

        // statuses
        bool udp_init = false;
        bool ota_init = false;

        QueueHandle_t netQueue;
        QueueHandle_t udpQueue;

        // lazy singleton
        static ESP32Net& getInstance() {
            static ESP32Net instance;
            return instance;
        }
        ESP32Net(const ESP32Net&) = delete;
        ESP32Net& operator=(const ESP32Net&) = delete;

        Error::Code init(void);
        bool have_ip(void) {
            return ip_addr != INADDR_NONE;
        }
        void set_ip(IPAddress ip) {
            ip_addr = ip;
        }
        void set_subnet_mask(IPAddress smask) {
            subnet_mask = smask;
            subnet_addr = (uint32_t)ip_addr & subnet_mask;
        }
        IPAddress get_ip(void) {
            return ip_addr;
        }
        bool have_internet(void) {
            return internet_connected;
        }
        bool check_internet(void);
        Error::Code check_clock(void);
        void check_queue(void);
        void send_net_msg(NetMessage::Type type, uint8_t code);
        Error::Code broadcast_str(const char* data, bool encrypt = use_aes,
                                  uint16_t port = 0) {
            return send_str(broadcastIP, data, encrypt, port);
        }
        Error::Code send_str(IPAddress ip, const char* data,
                             bool encrypt = use_aes, uint16_t port = 0);

    protected:
        // flags
        bool internet_connected = false;

        // ip address
        IPAddress ip_addr = INADDR_NONE;
        uint32_t subnet_mask = 0;
        uint32_t subnet_addr = 0;

        // hidden creator
        ESP32Net() {};
};

static ESP32Net& esp32Net = ESP32Net::getInstance();

void ota_check(void);

// bool check_internet_t(void) { return esp32Net.check_internet(); }

#endif  // ARDUINO
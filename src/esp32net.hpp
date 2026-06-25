#pragma once

#ifdef ARDUINO

#include <Arduino.h>

#ifdef USE_QUEUE
#include "circular_queue.hpp"
#endif  // USE_QUEUE

// debug flag (must be unique)
#define _DL_NET (1 << 1)  // 2

#ifndef GIT_VERSION
#define GIT_VERSION "Unset"
#endif  // GIT_VERSION

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "Unset"
#endif

// broadcast ip
const IPAddress broadcastIP(255, 255, 255, 255);

#define ESP32NET_ERROR_LIST(X)                  \
    X(NoError, "No Error")                      \
    X(CreateQueueFailed, "Create Queue Failed") \
    X(TimeSyncFailed, "Time Sync Failed")       \
    X(MessageTooBig, "Message Too Big")         \
    X(NoNetwork, "No Network")                  \
    X(NoInternet, "No Internet")                \
    X(SerializeError, "Serialize Error")        \
    X(DeserializeError, "Deserialize Error")    \
    X(StringTooBig, "String Too Big")           \
    X(QueueError, "Queue Error")

class ESP32Net {
    protected:
        // make encryption default if on
        static constexpr bool use_aes = USE_AES;

    public:
        class Version {
                static constexpr const char* const module = "ESP32Net";
                static constexpr const char* const git_version = GIT_VERSION;
                static constexpr const char* const firmware_version =
                    FIRMWARE_VERSION;
                static constexpr const char* const build_time =
                    __DATE__ " " __TIME__;
        };

        class Config {
            public:
                static constexpr uint8_t tz_size = 32 + 1;
                static constexpr uint8_t ssid_size = 32 + 1;
                static constexpr uint8_t passwd_size = 64 + 1;
#ifdef USE_AES
                static constexpr uint8_t hex_key_size = 64 + 1;
                static constexpr uint8_t aes_key_size = 32;
                static constexpr uint8_t iv_size = 12;
                static constexpr uint8_t tag_size = 16;
                static constexpr uint8_t prefix_size = iv_size + tag_size;
                // variables coming from defines cannot be overwritten
                static constexpr size_t local_queue_size = LOCAL_QUEUE_SIZE;
                static constexpr size_t internet_queue_size =
                    INTERNET_QUEUE_SIZE;

#endif  // USE_AES

                // variables coming from defines but can be overwritten
                static uint16_t udp_data_port;
                static char tz_full[tz_size];
                static char ssid[ssid_size];
                static char password[passwd_size];
                static char ota_password[passwd_size];
#ifdef USE_AES
                static char hex_key[hex_key_size];
                static uint8_t aes_key[aes_key_size];
#endif  // USE_AES

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

        class Message {
            public:
                bool encrypt;
                IPAddress destination;
                uint16_t port;
                char str[Config::udp_msg_size];

                // we will coerce IPaddress in a uint32_t for storage
                static constexpr size_t base_size =
                    sizeof(bool) + sizeof(uint32_t) + sizeof(uint16_t) +
                    sizeof(size_t);
                static constexpr size_t max_size =
                    base_size + Config::udp_msg_size;

                size_t size(void) {
                    return base_size + strlen(str) + 1;
                }
                bool serialize(uint8_t* data, size_t len);
                bool deserialize(uint8_t* data, size_t len);
        };

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
            return local_ip != INADDR_NONE;
        }
        void set_ip(IPAddress ip) {
            local_ip = ip;
        }
        void set_subnet_mask(IPAddress smask) {
            subnet_mask = smask;
            subnet_addr = (uint32_t)local_ip & subnet_mask;
        }
        IPAddress get_ip(void) {
            return local_ip;
        }
        bool have_internet(void) {
            return internet_connected;
        }
        bool check_internet(void);
        Error::Code check_clock(void);
        void check_queue(void);
        void queue_net_msg(NetMessage::Type type, uint8_t code);
        Error::Code broadcast_str(const char* str, bool encrypt = use_aes,
                                  uint16_t port = 0) {
            return send_str(broadcastIP, str, encrypt, port);
        }
        Error::Code send_str(IPAddress ip, const char* str,
                             bool encrypt = use_aes, uint16_t port = 0);
        Error::Code update_ssid(const char* new_ssid);
        Error::Code update_password(const char* new_password);
        Error::Code update_ota_password(const char* new_ota_password);
#ifdef USE_AES
        Error::Code update_aes_key(const char* new_hex_key);
#endif  // USE_AES
        void reconnect();

    protected:
        // flags
        bool internet_connected = false;

        // ip address
        IPAddress local_ip = INADDR_NONE;
        uint32_t subnet_mask = 0;
        uint32_t subnet_addr = 0;

#ifdef USE_QUEUE
        CircularQueue local_q = CircularQueue(Config::local_queue_size);
        CircularQueue internet_q = CircularQueue(Config::internet_queue_size);
#endif  // USE_QUEUE

        // hidden creator
        ESP32Net() {};

        Error::Code connection_check(IPAddress ip, bool& local);
#ifdef USE_QUEUE
        Error::Code queue_message(CircularQueue& q, Message& m);
        Error::Code empty_queue(CircularQueue& q);
#endif  // USE_QUEUE
        void send_message(Message& message);
#ifdef USE_AES
        uint8_t nibbleToHex(char nibble);
        void genAesKey();
#endif  // USE_AES
};

static ESP32Net& esp32Net = ESP32Net::getInstance();

void ota_check(void);

// bool check_internet_t(void) { return esp32Net.check_internet(); }

#endif  // ARDUINO

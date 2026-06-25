#include "esp32net.hpp"

#ifdef ARDUINO

#include <ArduinoOTA.h>
#include <AsyncUDP.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "debug_log.hpp"

// initialize config
uint16_t ESP32Net::Config::udp_data_port = UDP_DATA_PORT;
char ESP32Net::Config::tz_full[] = TZ_FULL;
char ESP32Net::Config::ssid[] = WIFI_SSID;
char ESP32Net::Config::password[] = WIFI_PASSWORD;
char ESP32Net::Config::ota_password[] = OTA_PASSWORD;
#ifdef USE_AES
char ESP32Net::Config::hex_key[] = HEX_KEY;
uint8_t ESP32Net::Config::aes_key[ESP32Net::Config::aes_key_size];
#endif  // USE_AES

// async udo
AsyncUDP audp;
WiFiUDP wudp;

void wifi_connected(WiFiEvent_t event, WiFiEventInfo_t info) {
    DEBUG_LOG(_DL_NET, "WiFi connected");
    esp32Net.queue_net_msg(ESP32Net::NetMessage::Type::Connected,
                           ESP32Net::Config::nocode);
}

// udp packet arrived
void on_udp_packet_received(void* arg, AsyncUDPPacket packet) {
    DEBUG_LOG(_DL_NET, "UDP packet received");
    ESP32Net::UDPMessage msg;
    msg.remoteIP = packet.remoteIP();
    size_t len = packet.length();
    if (len + 1 >= ESP32Net::Config::udp_msg_size) {
        DEBUG_LOG(_DL_NET, "Received UDP packet too big for buffer");
        return;
    }
    memcpy(msg.data, packet.data(), len);
    msg.data[len] = ESP32Net::Config::eos;
    if (xQueueSend(esp32Net.udpQueue, &msg, ESP32Net::Config::wait) != pdPASS) {
        DEBUG_LOG(_DL_NET, "Failed to add received UDP packet to queue");
    }
}

// got ip event add to queue to handle in main loop
void wifi_got_ip(WiFiEvent_t event, WiFiEventInfo_t info) {
    IPAddress lip = WiFi.localIP();
    esp32Net.set_ip(lip);
    esp32Net.set_subnet_mask(WiFi.subnetMask());
    DEBUG_LOG(_DL_NET, "WiFi got ip: %s", lip.toString().c_str());
    esp32Net.queue_net_msg(ESP32Net::NetMessage::Type::GotIP,
                           ESP32Net::Config::nocode);
    // setup up udp event listener
    if (!esp32Net.udp_init) {
        if (audp.listen(UDP_DATA_PORT)) {
            DEBUG_LOG(_DL_NET, "UDP listener setup");
            audp.onPacket(on_udp_packet_received);
            esp32Net.udp_init = true;
        } else {
            DEBUG_LOG(_DL_NET, "Failed to setup UDP listener");
        }
    }
    // setup ota
    if (!esp32Net.ota_init) {
        ArduinoOTA.setPassword(ESP32Net::Config::ota_password);
        ArduinoOTA.begin();
        esp32Net.ota_init = true;
    }
}

// disconnected event add to queue to handle in main loop and attempt reconnect
void wifi_disconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
    DEBUG_LOG(_DL_NET, "WiFi disconnected");
    esp32Net.set_ip(INADDR_NONE);
    esp32Net.queue_net_msg(ESP32Net::NetMessage::Type::Disconnected,
                           info.wifi_sta_disconnected.reason);
    WiFi.begin(ESP32Net::Config::ssid, ESP32Net::Config::password);
}

// check for ota
void ota_check(void) {
    if (esp32Net.ota_init) {
        ArduinoOTA.handle();
    }
}

bool ESP32Net::Message::serialize(uint8_t* data, size_t len) {
    DEBUG_LOG(_DL_NET, "Message Serialize: %s", destination.toString().c_str());
    if (len < size()) {
        return false;
    }
    uint8_t* ptr = data;
    uint32_t raw_ip = destination;
    DEBUG_LOG(_DL_NET, "message: %s", str);

    memcpy(ptr, &encrypt, sizeof(bool));
    ptr += sizeof(bool);
    memcpy(ptr, &raw_ip, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    memcpy(ptr, &port, sizeof(uint16_t));
    ptr += sizeof(uint16_t);
    strlcpy(reinterpret_cast<char*>(ptr), str, strlen(str) + 1);
    return true;
}

bool ESP32Net::Message::deserialize(uint8_t* data, size_t len) {
    DEBUG_LOG(_DL_NET, "Message Deserialize");
    if (len > max_size) {
        return false;
    }
    uint8_t* ptr = data;
    uint32_t raw_ip;
    memcpy(&encrypt, ptr, sizeof(bool));
    ptr += sizeof(bool);
    memcpy(&raw_ip, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    destination = IPAddress(raw_ip);
    memcpy(&port, ptr, sizeof(uint16_t));
    ptr += sizeof(uint16_t);
    size_t sz =
        strlcpy(str, reinterpret_cast<char*>(ptr), Config::udp_msg_size);
    if (sz >= Config::udp_msg_size) {
        return false;
    }
    DEBUG_LOG(_DL_NET, "message: %s", str);
    return true;
}

// setup wifi and event handlers
ESP32Net::Error::Code ESP32Net::init(void) {
    DEBUG_LOG(_DL_NET, "ESP32Net init");
    netQueue = xQueueCreate(Config::net_queue_size, sizeof(NetMessage));
    if (netQueue == NULL) {
        DEBUG_LOG(_DL_NET, "Failed to create net queue");
        return Error::Code::CreateQueueFailed;
    }
    udpQueue = xQueueCreate(Config::udp_queue_size, sizeof(UDPMessage));
    if (udpQueue == NULL) {
        DEBUG_LOG(_DL_NET, "Failed to create udp queue");
        return Error::Code::CreateQueueFailed;
    }
#ifdef USE_AES
    genAesKey();
#endif  // USE_AES
    WiFi.onEvent(wifi_connected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
    WiFi.onEvent(wifi_got_ip, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
    WiFi.onEvent(wifi_disconnected,
                 WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    WiFi.begin(Config::ssid, Config::password);
    return Error::Code::NoError;
}

bool ESP32Net::check_internet(void) {
    DEBUG_LOG(_DL_NET, "check_internet (%d): %s", WiFi.status(),
              local_ip.toString().c_str());
    if ((WiFi.status() == WL_CONNECTED) && (local_ip != INADDR_NONE)) {
        HTTPClient http;
        http.setTimeout(Config::long_delay);
        http.begin(Config::internet_check_url);
        int httpCode = http.GET();
        http.end();
        DEBUG_LOG(_DL_NET, "Internet check HTTP code: %d", httpCode);
        if (httpCode == Config::httpcode) {
            if (!internet_connected) {
                DEBUG_LOG(_DL_NET, "Internet connection confirmed");
                esp32Net.queue_net_msg(NetMessage::Type::InternetConnected,
                                       Config::no_code);
            }
            internet_connected = true;
            struct tm timeinfo;
            if (!getLocalTime(&timeinfo)) {
                DEBUG_LOG(_DL_NET, "Failed to get local time, checking clock");
                if (check_clock() == Error::Code::NoError)
                    esp32Net.queue_net_msg(NetMessage::Type::TimeSynced,
                                           Config::no_code);
                else
                    esp32Net.queue_net_msg(NetMessage::Type::TimeSyncFailed,
                                           Config::no_code);
            }
            check_queue();
            return true;
        }
    }
    if (internet_connected) {
        DEBUG_LOG(_DL_NET, "No internet connection");
        esp32Net.queue_net_msg(NetMessage::Type::NoInternet, Config::no_code);
    }
    internet_connected = false;
    check_queue();
    return internet_connected;
}

// sync clock to ntp servers
ESP32Net::Error::Code ESP32Net::check_clock(void) {
    DEBUG_LOG(_DL_NET, "clock_check");
    configTzTime(Config::tz_full, Config::ntp_server_1, Config::ntp_server_2,
                 Config::ntp_server_3);
    struct tm timeinfo;
    uint8_t attempts = 0;
    while (!getLocalTime(&timeinfo)) {
        delay(Config::medium_delay);
        attempts++;
        if (attempts >= Config::time_sync_attempts) {
            DEBUG_LOG(_DL_NET, "Failed to synchronize time");
            return Error::Code::TimeSyncFailed;
        }
    }
    return Error::Code::NoError;
}

void ESP32Net::queue_net_msg(NetMessage::Type type, uint8_t code) {
    ESP32Net::NetMessage msg;
    msg.type = type;
    msg.code = code;
    if (xQueueSend(netQueue, &msg, Config::wait) != pdPASS) {
        DEBUG_LOG(_DL_NET, "Failed add got ip event");
    }
}

#ifdef USE_QUEUE
void ESP32Net::check_queue(void) {
    DEBUG_LOG(_DL_NET, "Check queue");
    if (local_ip == INADDR_NONE)
        empty_queue(local_q);
    if (internet_connected)
        empty_queue(internet_q);
    return;
}
#endif  // USE_QUEUE

ESP32Net::Error::Code ESP32Net::send_str(IPAddress ip, const char* str,
                                         bool encrypt, uint16_t port) {
    DEBUG_LOG(_DL_NET, "Send str: %s", str);
    Message message;
    if (port == 0)
        message.port = Config::udp_data_port;
    else
        message.port = port;
    bool local =
        ((ip == broadcastIP) || (subnet_addr == (uint32_t)ip & subnet_mask));
    message.encrypt = encrypt;
    message.destination = ip;
    size_t sz = strlcpy(message.str, str, Config::udp_msg_size);
    if (sz >= Config::udp_msg_size) {
        return Error::Code::MessageTooBig;
    }
    if (local_ip == INADDR_NONE) {
        DEBUG_LOG(_DL_NET, "No Network");
#ifdef USE_QUEUE
        auto code = queue_message(local_q, message);
        if (code != Error::Code::NoError)
            return code;
#endif  // USE_QUEUE
        return Error::Code::NoNetwork;
    } else if ((!local) && (!internet_connected)) {
        DEBUG_LOG(_DL_NET, "No internet");
#ifdef USE_QUEUE
        auto code = queue_message(internet_q, message);
        if (code != Error::Code::NoError)
            return code;
#endif  // USE_QUEUE
        return Error::Code::NoInternet;
    }
#ifdef USE_QUEUE
    check_queue();
#endif  // USE_QUEUE
    send_message(message);
    return Error::Code::NoError;
}

ESP32Net::Error::Code ESP32Net::update_ssid(const char* new_ssid) {
    // this change requires a reconnect
    size_t sz = strlcpy(Config::ssid, new_ssid, Config::ssid_size);
    if (sz >= Config::ssid_size) {
        return Error::Code::StringTooBig;
    }
    return Error::Code::NoError;
}

ESP32Net::Error::Code ESP32Net::update_password(const char* new_password) {
    // this change requires a reconnect
    size_t sz = strlcpy(Config::password, new_password, Config::passwd_size);
    if (sz >= Config::passwd_size) {
        return Error::Code::StringTooBig;
    }
    return Error::Code::NoError;
}

ESP32Net::Error::Code ESP32Net::update_ota_password(
    // this change is live
    const char* new_ota_password) {
    size_t sz =
        strlcpy(Config::ota_password, new_ota_password, Config::passwd_size);
    if (sz >= Config::passwd_size) {
        return Error::Code::StringTooBig;
    }
    ArduinoOTA.setPassword(ESP32Net::Config::ota_password);
    return Error::Code::NoError;
}

#ifdef USE_AES

ESP32Net::Error::Code ESP32Net::update_aes_key(const char* new_hex_key) {
    // this change is live
    size_t sz = strlcpy(Config::hex_key, new_hex_key, Config::hex_key_size);
    if (sz >= Config::hex_key_size) {
        return Error::Code::StringTooBig;
    }
    genAesKey();
    return Error::Code::NoError;
}

#endif  // USE_AES

void ESP32Net::reconnect() {
    WiFi.disconnect(false, true);
}

#ifdef USE_QUEUE

ESP32Net::Error::Code ESP32Net::queue_message(CircularQueue& q, Message& m) {
    size_t mlen = m.size();
    uint8_t data[mlen];
    if (!m.serialize(data, mlen)) {
        return Error::Code::SerializeError;
    }
    auto code = q.push(data, mlen);
    if (code != CircularQueue::Error::Code::NoError) {
        return Error::Code::QueueError;
    }
    return Error::Code::NoError;
}

ESP32Net::Error::Code ESP32Net::empty_queue(CircularQueue& q) {
    uint8_t data[Config::udp_msg_size];
    Entry dlen;
    Message mesg;

    DEBUG_LOG(_DL_NET, "Empty Queue");
    auto ret = q.pop(data, sizeof(data), dlen);
    while (ret != CircularQueue::Error::Code::QueueEmpty) {
        if (ret != CircularQueue::Error::Code::NoError) {
            DEBUG_LOG(_DL_NET, "Circular Queue Error: %s",
                      CircularQueue::Error::toString(ret));
            return Error::Code::QueueError;
        }
        if (!mesg.deserialize(data, sizeof(data))) {
            DEBUG_LOG(_DL_NET, "Deserialization Error");
            return Error::Code::DeserializeError;
        }
        send_message(mesg);
        ret = q.pop(data, sizeof(data), dlen);
    }
    return Error::Code::NoError;
}

#endif  // USE_QUEUE

void ESP32Net::send_message(Message& mesg) {
    DEBUG_LOG(_DL_NET, "send Message (%s:%d): %s",
              mesg.destination.toString().c_str(), mesg.port, mesg.str);
    size_t len = strlen(mesg.str) + 1;

#ifdef USE_AES
    if (mesg.encrypt) {
        DEBUG_LOG(_DL_NET, "encrypting");
        uint8_t iv[Config::iv_size];
        uint8_t tag[Config::tag_size];
        uint8_t ciphertext[len];

        for (int i = 0; i < 12; i++) {
            iv[i] = esp_random() % 256;
        }

        mbedtls_gcm_context gcm;
        mbedtls_gcm_init(&gcm);
        mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, Config::aes_key, 256);
        mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, len, iv, 12, NULL,
                                  0, reinterpret_cast<const uint8_t*>(mesg.str),
                                  ciphertext, 16, tag);
        mbedtls_gcm_free(&gcm);
        wudp.beginPacket(mesg.destination, mesg.port);
        wudp.write(iv, 12);
        wudp.write(tag, 16);
        wudp.write(ciphertext, len);
        wudp.endPacket();
        return;
    }
#endif  // USE_AES
    size_t payload_len = strlen(mesg.str);
    wudp.beginPacket(mesg.destination, mesg.port);
    wudp.write(reinterpret_cast<const uint8_t*>(mesg.str), len);
    wudp.endPacket();
}

#ifdef USE_AES

// Function to convert a hex character to an integer
uint8_t ESP32Net::nibbleToHex(char nibble) {
    if (nibble >= '0' && nibble <= '9')
        return nibble - '0';
    if (nibble >= 'a' && nibble <= 'f')
        return nibble - 'a' + 10;
    if (nibble >= 'A' && nibble <= 'F')
        return nibble - 'A' + 10;
    return 0;
}

// Function to parse the hex string to your uint8_t array
void ESP32Net::genAesKey() {
    for (int i = 0; i < Config::aes_key_size; i++) {
        uint8_t high = nibbleToHex(Config::hex_key[i * 2]);
        uint8_t low = nibbleToHex(Config::hex_key[i * 2 + 1]);
        // huh why can't we see this
        Config::aes_key[i] = (high << 4) | low;
    }
}

#endif  // USE_AES

#endif  // ARDUINO

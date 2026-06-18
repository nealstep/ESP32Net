#include "esp32net.hpp"

#include "debug_log.hpp"

#include <WiFi.h>
#include <AsyncUDP.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>

// timezone information from secret.ini via platformio.ini
constexpr const char *tz_full = TZ_FULL;
static constexpr char ssid[] = WIFI_SSID;
static constexpr char password[] = WIFI_PASSWORD;
static constexpr uint16_t udp_data_port = UDP_DATA_PORT;

// statuses
static bool udp_init = false;
static bool ota_init = false;

// async udo
AsyncUDP audp;

void wifi_connected(WiFiEvent_t event, WiFiEventInfo_t info)
{
    DEBUG_LOG(_DL_NET, "WiFi connected");
    esp32Net.send_net_msg(ESP32Net::NetMessage::Connected, ESP32Net::noerr);
}

// udp packet arrived
void on_udp_packet_received(void *arg, AsyncUDPPacket packet)
{
    ESP32Net::UDPMessage msg;
    msg.remoteIP = packet.remoteIP();
    size_t len = packet.length();
    if (len + 1 >= UDP_MSG_SIZE)
    {
        DEBUG_LOG(_DL_NET, "Received UDP packet too big for buffer");
        return;
    }
    memcpy(msg.data, packet.data(), len);
    msg.data[len] = '\0';
    if (xQueueSend(esp32Net.udpQueue, &msg, 0) != pdPASS)
    {
        DEBUG_LOG(_DL_NET, "Failed to add received UDP packet to queue");
    }
}

// got ip event add to queue to handle in main loop
void wifi_got_ip(WiFiEvent_t event, WiFiEventInfo_t info)
{
    DEBUG_LOG(_DL_NET, "WiFi got ip");
    esp32Net.send_net_msg(ESP32Net::NetMessage::GotIP, ESP32Net::noerr);
    // setup up udp event listener
    if (!udp_init)
    {
        if (audp.listen(UDP_DATA_PORT))
        {
            DEBUG_LOG(_DL_NET, "UDP listener setup");
            audp.onPacket(on_udp_packet_received);
            udp_init = true;
        }
        else
        {
            DEBUG_LOG(_DL_NET, "Failed to setup UDP listener");
        }
    }
    // setup ota
    if (!ota_init)
    {
        ArduinoOTA.setPassword(OTA_PASSWD);
        ArduinoOTA.begin();
        ota_init = true;
    }
}

// disconnected event add to queue to handle in main loop and attempt reconnect
void wifi_disconnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
    DEBUG_LOG(_DL_NET, "WiFi disconnected");
    esp32Net.set_ip(INADDR_NONE);
    esp32Net.send_net_msg(ESP32Net::NetMessage::Disconnected, info.wifi_sta_disconnected.reason);
    WiFi.begin(ssid, password);
}

// check for ota
void ota_check(void)
{
    if (ota_init)
    {
        ArduinoOTA.handle();
    }
}

// setup wifi and event handlers
uint8_t ESP32Net::init(void)
{
    DEBUG_LOG(_DL_NET, "ESP32Net init");
    netQueue = xQueueCreate(NET_QUEUE_SIZE, sizeof(NetMessage));
    if (netQueue == NULL)
    {
        DEBUG_LOG(_DL_NET, "Failed to create net queue");
        return create_queue_failed;
    }
    udpQueue = xQueueCreate(UDP_QUEUE_SIZE, sizeof(UDPMessage));
    if (udpQueue == NULL)
    {
        DEBUG_LOG(_DL_NET, "Failed to create udp queue");
        return create_queue_failed;
    }
    WiFi.onEvent(wifi_connected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
    WiFi.onEvent(wifi_got_ip, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
    WiFi.onEvent(wifi_disconnected,
                 WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    WiFi.begin(ssid, password);
    return noerr;
}

bool ESP32Net::check_internet(void)
{
    DEBUG_LOG(_DL_NET, "check_internet (%d): %s", WiFi.status(),
              ip_addr.toString().c_str());
    if ((WiFi.status() == WL_CONNECTED) && (ip_addr != INADDR_NONE))
    {
        HTTPClient http;
        http.setTimeout(LONG_DELAY);
        http.begin(INTERNET_CHECK_URL);
        int httpCode = http.GET();
        http.end();
        DEBUG_LOG(_DL_NET, "Internet check HTTP code: %d", httpCode);
        if (httpCode == 204)
        {
            internet_connected = true;
            DEBUG_LOG(_DL_NET, "Internet connection confirmed");
            struct tm timeinfo;
            if (!getLocalTime(&timeinfo))
            {
                DEBUG_LOG(_DL_NET, "Failed to get local time, checking clock");
                if (!check_clock())
                    esp32Net.send_net_msg(ESP32Net::NetMessage::TimeSynced, ESP32Net::noerr);
            }
            // check_send_buffer();
            return true;
        }
    }
    DEBUG_LOG(_DL_NET, "No internet connection");
    internet_connected = false;
    return internet_connected;
}

// sync clock to ntp servers
bool ESP32Net::check_clock(void)
{
    DEBUG_LOG(_DL_NET, "clock_check");
    configTzTime(tz_full, NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);
    struct tm timeinfo;
    uint8_t attempts = 0;
    while (!getLocalTime(&timeinfo))
    {
        delay(MEDIUM_DELAY);
        attempts++;
        if (attempts >= TIME_SYNC_ATTEMPTS)
        {
            DEBUG_LOG(_DL_NET, "Failed to synchronize time");
            return time_sync_failed;
        }
    }
    return noerr;
}

void ESP32Net::send_net_msg(NetMessage::Type type, uint8_t code)
{
    ESP32Net::NetMessage msg;
    msg.type = type;
    msg.code = code;
    if (xQueueSend(netQueue, &msg, 0) != pdPASS)
    {
        DEBUG_LOG(_DL_NET, "Failed add got ip event");
    }
}

#pragma once

#define NET_UNIT_LIST(X) X(Net, "Net")

#define NET_ERROR_LIST(X)                              \
    X(TimeSyncFailed, "Time sync failed")              \
    X(NoNetwork, "No network")                         \
    X(NoInternet, "No Internet")                       \
    X(SerializeError, "Serialize error")               \
    X(DeserializeError, "Deserialize error")           \
    X(CreateQueueFailed, "Create queue failed")        \
    X(FailedToAddToQueue, "Failed to add to queue")    \
    X(UDPListenerFailed, "UDP listener failed")        \
    X(UnknownMessage, "Unknown message")               \
    X(StringTooBig, "String too big")                  \
    X(EncryptSetKeyFailed, "Encrypt set key failed")   \
    X(EncryptCryptError, "Encrypt crypt error")        \
    X(UDPBeginPacketFailed, "UDP begin packet failed") \
    X(UDPWriteFailed, "UDP write failed")              \
    X(UDPEndPacketFailed, "UDP end packet failed")

#define NET_NOTICE_LIST(X)                                       \
    X(Connected, "Connected")                                    \
    X(GotIP, "GotIP %s")                                         \
    X(Disconnected, "Disconnected")                              \
    X(TimeSynced, "Time synced")                                 \
    X(InternetConnected, "Internet connected")                   \
    X(ReceivedUDPPacket, "Received UDP packet from %s: %s")      \
    X(UDPPacket, "UDP packet received")                          \
    X(UDPListenerSetup, "UDP listener setup")                    \
    X(MessageSerialize, "Message Serialize: %s")                 \
    X(Message, "message: %s")                                    \
    X(MessageDesrialize, "Message Deserialize")                  \
    X(ESP32NetInit, "ESP32Net Init")                             \
    X(CheckInternet, "check_internet (%d): %s")                  \
    X(InternetCheckCode, "Internet check HTTP code: %d")         \
    X(CheckingClock, "Failed to get local time, checking clock") \
    X(CheckClock, "Checking clock")                              \
    X(CheckQueue, "Check queue")                                 \
    X(SendStr, "Send str: %s")                                   \
    X(SendMessage, "Send message (%s:%d): %s")                   \
    X(Encrypting, "Encrypting")                                  \
    X(EmptyingQueue, "Emptying Queue")

#define NET_WORD_LIST(X)

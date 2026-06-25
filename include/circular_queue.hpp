
#ifdef ARDUINO
#include <Arduino.h>
#else  // !ARDUINO
#include <cstdint>
#include <cstring>
#endif  // ARDUINO

#include "debug_log.hpp"

// debug flag (must be unique)
#define _DL_CIR (1 << 2)  // 4

#define CIRCULAR_Q_ERROR_LIST(X)  \
    X(NoError, "No Error")        \
    X(QueueEmpty, "Queue Empty")  \
    X(NoQueue, "No Queue")        \
    X(ItemTooBig, "Item Too Big") \
    X(UnknownError, "Unknown Error")

typedef uint16_t Entry;

class CircularQueue {
    private:
        static constexpr uint16_t avg_message_size = 100;

    public:
        struct Error {
#define GENERATE_ENUM(name, string) name,
                enum class Code : uint8_t {
                    CIRCULAR_Q_ERROR_LIST(GENERATE_ENUM)
                };
#undef GENERATE_ENUM
#define GENERATE_STRING(name, string) \
    case Code::name:                  \
        return string;
                static const char* toString(Code code) {
                    switch (code) {
                        CIRCULAR_Q_ERROR_LIST(GENERATE_STRING)
                        default:
                            return "Unknown Error";
                    }
                }
#undef GENERATE_STRING

        };  // struct Error

        CircularQueue(size_t requested_capacity,
                      uint16_t requested_entries = 0) {
            DEBUG_LOG(_DL_CIR, "CQ::CircularQueue(%ld, %d)", requested_capacity,
                      requested_entries);
            // malloc queues
            size_t records_size = requested_entries * sizeof(Entry);
            capacity = requested_capacity;
            if (!requested_entries) {
                requested_entries = static_cast<uint16_t>(requested_capacity /
                                                          avg_message_size);
                DEBUG_LOG(_DL_CIR, "computed entries: %d", requested_entries);
            }
            entries = requested_entries;
#ifdef ARDUINO_ARCH_ESP32
            // use psram if possible
            if (psramFound()) {
                DEBUG_LOG(_DL_CIR, "PSRAM found");
                buffer = (uint8_t*)ps_malloc(requested_capacity);
                if (buffer != nullptr) {
                    DEBUG_LOG(_DL_CIR, "PSRAM buffer created");
                }
                records = (uint16_t*)ps_malloc(records_size);
                if (records != nullptr) {
                    DEBUG_LOG(_DL_CIR, "PSRAM records created");
                }
            }
#endif  // ARDUINO_ARCH_ESP32
            if (buffer == nullptr) {
                buffer = (uint8_t*)malloc(requested_capacity);
                if (buffer == nullptr) {
                    DEBUG_LOG(_DL_CIR, "Malloc buffer failed");
                    capacity = 0;
                } else {
                    DEBUG_LOG(_DL_CIR, "Malloc buffer succeeded");
                }
            }
            if (records == nullptr) {
                records = (uint16_t*)malloc(records_size);
                if (records == nullptr) {
                    DEBUG_LOG(_DL_CIR, "Malloc records failed");
                    entries = 0;
                } else {
                    DEBUG_LOG(_DL_CIR, "Malloc records succeeded");
                }
            }
            // check they were both successfulty created
            if (capacity & !entries) {
                DEBUG_LOG(_DL_CIR, "Malloc failed for entries freeing buffer");
                if (buffer != nullptr) {
                    free(buffer);
                    buffer = nullptr;
                }
                capacity = 0;
            } else if (!capacity & entries) {
                DEBUG_LOG(_DL_CIR, "Malloc failed for buffer freeing entries");
                if (records != nullptr) {
                    free(records);
                    records = nullptr;
                }
                entries = 0;
            }
        }

        ~CircularQueue() {
            DEBUG_LOG(_DL_CIR, "~CQ::CircularQueue()");
            // free allocated space
            if (buffer != nullptr) {
                free(buffer);
                buffer = nullptr;
            }
            if (records != nullptr) {
                free(records);
                records = nullptr;
            }
        }

        Error::Code push(const uint8_t* data, Entry length) {
            DEBUG_LOG(_DL_CIR, "CQ::push: %d", length);
            // sanity
            if (buffer == nullptr)
                return Error::Code::NoQueue;
            if (records == nullptr)
                return Error::Code::NoQueue;
            if (length > capacity) {
                return Error::Code::ItemTooBig;
            }

            // make space if required
            while (
                (capacity - used_bytes < length || used_entries >= entries) &&
                used_entries > 0) {
                evict_oldest_record();
            }
            // not sure this is possible
            if (capacity - used_bytes < length) {
                return Error::Code::UnknownError;
            }

            DEBUG_LOG(_DL_CIR, "ready to copy data");
            // copy data
            size_t space_at_end = capacity - tail_bytes;
            if (length <= space_at_end) {
                DEBUG_LOG(_DL_CIR, "simple copy: %ld %d", tail_bytes, length);
                memcpy(buffer + tail_bytes, data, length);
            } else {
                DEBUG_LOG(_DL_CIR, "broken copy");
                memcpy(buffer + tail_bytes, data, space_at_end);
                memcpy(buffer, data + space_at_end, length - space_at_end);
            }

            // update data pointers
            tail_bytes = (tail_bytes + length) % capacity;
            used_bytes += length;
            records[tail_entries] = length;
            tail_entries = (tail_entries + 1) % entries;
            used_entries++;

            return Error::Code::NoError;
        }

        Error::Code pop(uint8_t* dest_buffer, Entry max_dest_len,
                        Entry& out_length) {
            DEBUG_LOG(_DL_CIR, "CQ::pop: %d", max_dest_len);
            // sanity
            if (buffer == nullptr)
                return Error::Code::NoQueue;
            if (records == nullptr)
                return Error::Code::NoQueue;
            if (used_entries == 0)
                return Error::Code::QueueEmpty;
            Entry next_record_size = records[head_entries];
            if (next_record_size > max_dest_len) {
                return Error::Code::ItemTooBig;
            }

            // Read from circular byte buffer
            size_t space_at_end = capacity - head_bytes;
            if (next_record_size <= space_at_end) {
                DEBUG_LOG(_DL_CIR, "simple copy");
                memcpy(dest_buffer, buffer + head_bytes, next_record_size);
            } else {
                DEBUG_LOG(_DL_CIR, "broken copy");
                memcpy(dest_buffer, buffer + head_bytes, space_at_end);
                memcpy(dest_buffer + space_at_end, buffer,
                       next_record_size - space_at_end);
            }
            out_length = next_record_size;

            // Advance heads
            head_bytes = (head_bytes + next_record_size) % capacity;
            used_bytes -= next_record_size;
            head_entries = (head_entries + 1) % entries;
            used_entries--;

            return Error::Code::NoError;
        }

        bool check_overrun() { return overrrun; }
        void clear_overrun() { overrrun = false; }
        size_t get_used_capacity() const {
            return used_bytes;
        }
        uint16_t get_used_entries() const {
            return used_entries;
        }
        size_t get_free_capacity() const {
            return capacity - used_bytes;
        }
        uint16_t get_free_entries() const {
            return entries - used_entries;
        }

    private:
        size_t capacity = 0;
        uint16_t entries = 0;

        uint8_t* buffer = nullptr;
        Entry* records = nullptr;

        size_t head_bytes = 0;  // Index of oldest byte
        size_t tail_bytes = 0;  // Index where next byte goes
        size_t used_bytes = 0;  // Total bytes currently stored

        uint16_t head_entries = 0;  // Index of oldest record metadata
        uint16_t tail_entries = 0;  // Index where next record metadata goes
        uint16_t used_entries = 0;  // Total entries currently stored

        bool overrrun = false;

        // Helper to pop the oldest record metadata and update byte count
        void evict_oldest_record() {
            if (used_entries == 0)
                return;

            size_t oldest_size = records[head_entries];

            // Advance metadata head
            head_entries = (head_entries + 1) % entries;
            used_entries--;

            // Advance byte buffer head
            head_bytes = (head_bytes + oldest_size) % capacity;
            used_bytes -= oldest_size;

            overrrun = true;
        }
};

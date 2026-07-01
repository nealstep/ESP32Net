#include "log.hpp"

#ifdef ARDUINO
#include <Arduino.h>
#else  // !ARDUINO
#include <cstdint>
#include <cstdlib>
#include <cstring>
#endif  // ARDUINO

#ifdef USE_QUEUE

typedef uint16_t Entry;

class CircularQueue {
   private:
    static constexpr uint16_t avg_message_size = 100;
    static constexpr const char* const s_buffer = "buffer";
    static constexpr const char* const s_records = "entries";

   public:
    CircularQueue(size_t requested_capacity, uint16_t requested_entries = 0) {
        LOG_N(Log::Uni::CirQ, Log::Sev::Dbg, Log::Note::CirQCreate,
              requested_capacity, requested_entries);
        // malloc queues
        capacity = requested_capacity;
        if (!requested_entries) {
            requested_entries =
                static_cast<uint16_t>(requested_capacity / avg_message_size);
            LOG_N(Log::Uni::CirQ, Log::Sev::Inf, Log::Note::ComputedEntries,
                  requested_entries);
        }
        entries = requested_entries;
        size_t records_size = requested_entries * sizeof(Entry);
#ifdef ARDUINO_ARCH_ESP32
        // use psram if possible
        if (psramFound()) {
            LOG_N(Log::Uni::CirQ, Log::Sev::Inf, Log::Note::PSRAMFound);
            buffer = (uint8_t*)ps_malloc(requested_capacity);
            if (buffer != nullptr) {
                LOG_N(Log::Uni::CirQ, Log::Sev::Inf, Log::Note::PSRAMCreated,
                      Log::Word::Buffer);
            }
            records = (uint16_t*)ps_malloc(records_size);
            if (records != nullptr) {
                LOG_N(Log::Uni::CirQ, Log::Sev::Inf, Log::Note::PSRAMCreated,
                      Log::Word::Records);
            }
        }
#endif  // ARDUINO_ARCH_ESP32
        if (buffer == nullptr) {
            buffer = (uint8_t*)malloc(requested_capacity);
            if (buffer == nullptr) {
                LOG_E(Log::Uni::CirQ, Log::Err::MallocFailed, Log::Word::Buffer);
                capacity = 0;
            } else {
                LOG_N(Log::Uni::CirQ, Log::Sev::Inf, Log::Note::MallocCreated,
                      Log::Word::Buffer);
            }
        }
        if (records == nullptr) {
            records = (uint16_t*)malloc(records_size);
            if (records == nullptr) {
                LOG_E(Log::Uni::CirQ, Log::Err::MallocFailed, Log::Word::Records);
                entries = 0;
            } else {
                LOG_N(Log::Uni::CirQ, Log::Sev::Inf, Log::Note::MallocCreated,
                      Log::Word::Records);
            }
        }
        // check they were both successfulty created
        if (capacity & !entries) {
            LOG_E(Log::Uni::CirQ, Log::Err::MallocFailedFreeing);
            if (buffer != nullptr) {
                free(buffer);
                buffer = nullptr;
            }
            capacity = 0;
        } else if (!capacity & entries) {
            LOG_E(Log::Uni::CirQ, Log::Err::MallocFailedFreeing);
            if (records != nullptr) {
                free(records);
                records = nullptr;
            }
            entries = 0;
        }
    }

    ~CircularQueue() {
        LOG_N(Log::Uni::CirQ, Log::Sev::Inf, Log::Note::Destroy);
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

    Log::Err push(const uint8_t* data, Entry length) {
        LOG_N(Log::Uni::CirQ, Log::Sev::Inf, Log::Note::CirQPush, length);
        // sanity
        if (buffer == nullptr) return Log::Err::NoQueue;
        if (records == nullptr) return Log::Err::NoQueue;
        if (length > capacity) {
            return Log::Err::ItemTooBig;
        }

        // make space if required
        while ((capacity - used_bytes < length || used_entries >= entries) &&
               used_entries > 0) {
            evict_oldest_record();
        }
        // not sure this is possible
        if (capacity - used_bytes < length) {
            return Log::Err::UnexpectedError;
        }

        LOG_N(Log::Uni::CirQ, Log::Sev::Dbg, Log::Note::CirQRdyToCopy);
        // copy data
        size_t space_at_end = capacity - tail_bytes;
        if (length <= space_at_end) {
            LOG_N(Log::Uni::CirQ, Log::Sev::Dbg, Log::Note::CirQSimpleCopy);
            memcpy(buffer + tail_bytes, data, length);
        } else {
            LOG_N(Log::Uni::CirQ, Log::Sev::Dbg, Log::Note::CirQBrokenCopy);
            memcpy(buffer + tail_bytes, data, space_at_end);
            memcpy(buffer, data + space_at_end, length - space_at_end);
        }

        // update data pointers
        tail_bytes = (tail_bytes + length) % capacity;
        used_bytes += length;
        records[tail_entries] = length;
        tail_entries = (tail_entries + 1) % entries;
        used_entries++;

        return Log::Err::NoError;
    }

    Log::Err pop(uint8_t* dest_buffer, Entry max_dest_len, Entry& out_length) {
        LOG_N(Log::Uni::CirQ, Log::Sev::Inf, Log::Note::CirQPop, max_dest_len);
        // sanity
        if (buffer == nullptr) return Log::Err::NoQueue;
        if (records == nullptr) return Log::Err::NoQueue;
        if (used_entries == 0) return Log::Err::QueueEmpty;
        Entry next_record_size = records[head_entries];
        if (next_record_size > max_dest_len) {
            return Log::Err::ItemTooBig;
        }

        // Read from circular byte buffer
        size_t space_at_end = capacity - head_bytes;
        if (next_record_size <= space_at_end) {
            LOG_N(Log::Uni::CirQ, Log::Sev::Dbg, Log::Note::CirQSimpleCopy);
            memcpy(dest_buffer, buffer + head_bytes, next_record_size);
        } else {
            LOG_N(Log::Uni::CirQ, Log::Sev::Dbg, Log::Note::CirQBrokenCopy);
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

        return Log::Err::NoError;
    }

    bool check_overrun() { return overrrun; }
    void clear_overrun() { overrrun = false; }
    size_t get_used_capacity() const { return used_bytes; }
    uint16_t get_used_entries() const { return used_entries; }
    size_t get_free_capacity() const { return capacity - used_bytes; }
    uint16_t get_free_entries() const { return entries - used_entries; }

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
        if (used_entries == 0) return;

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

#endif  // USE_QUEUE

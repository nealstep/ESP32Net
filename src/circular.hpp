#pragma once

#ifdef Arduino
#include <Arduino.h>
#else
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#endif  // Arduino

class CircularQueue {
   public:
    enum class Error: uint8_t {
        NoError = 0,
        QueueEmpty,
        Truncated,
        NoQueue,
        MessageTooBig
    };

    // Constructor handles the smart allocation
    CircularQueue(size_t requested_capacity);
    ~CircularQueue() {
        if (buffer_ != nullptr) {
            free(buffer_);
        }
    }

    size_t free_space() const {
        if (capacity_ == 0) return 0;
        return (read_ptr_ - write_ptr_ - 1 + capacity_) % capacity_;
    }

    // if used_space is zero, buffer is empty
    size_t used_space() const {
        if (capacity_ == 0) return 0;
        return (write_ptr_ - read_ptr_ + capacity_) % capacity_;
    }

    // return flags
    bool overrun(void) { return overrun_; }
    bool psram(void) { return using_psram_; }

    // main functions to add and remove messages
    Error enqueue(const char* payload);
    Error dequeue(char *msg, size_t msg_len);

   private:
    char* buffer_ = nullptr;
    size_t capacity_ = 0;
    size_t read_ptr_ = 0;
    size_t write_ptr_ = 0;
    bool using_psram_ = false;
    bool overrun_ = false;

    // low level write handling wrap around
    void write_raw(const char* data, size_t len);
};

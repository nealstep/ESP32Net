#include "circular.hpp"

#ifndef Arduino
#include <cstring>
#endif  // Arduino

#include "debug_log.hpp"

// initialize circular queue with smart allocation
CircularQueue::CircularQueue(size_t requested_capacity) {
    DEBUG_LOG(_DL_CIR, "CircularQueue::CircularQueue: %d", requested_capacity);
    capacity_ = requested_capacity;
#ifdef ARDUINO_ARCH_ESP32
    // use psram if possible
    if (psramFound()) {
        DEBUG_LOG(_DL_CIR, "PSRAM found");
        buffer_ = (char*)ps_malloc(capacity_);
        if (buffer_ != nullptr) {
            DEBUG_LOG(_DL_CIR, "psram buffer created");
            using_psram_ = true;
        }
    }
#endif  // ARDUINO_ARCH_ESP32
    // otherwise use malloc
    if (buffer_ == nullptr) {
        DEBUG_LOG(_DL_CIR, "Trying Malloc");
        buffer_ = (char*)malloc(capacity_);
        using_psram_ = false;
        if (buffer_ == nullptr) {
            DEBUG_LOG(_DL_CIR, "Malloc failed");
            capacity_ = 0;
        } else {
            DEBUG_LOG(_DL_CIR, "Malloc Success");
        }
    }
}

// enqueue a string
CircularQueue::Error CircularQueue::enqueue(const char* payload) {
    DEBUG_LOG(_DL_CIR, "CircularQueue::enqueue: %s", payload);
    if (capacity_ == 0) return CircularQueue::Error::NoQueue;

    size_t len = strlen(payload);
    size_t total_len = len + 1;

    // message is bigger than entire buffer
    if (total_len > capacity_ - 1) {
        DEBUG_LOG(_DL_CIR, "Message too big: %d", total_len);
        return CircularQueue::Error::MessageTooBig;
    }

    // if bigger than free_space remove messages until it fits
    // set overrun if we had to remove anything
    while (total_len > free_space()) {
        overrun_ = true;
        while (buffer_[read_ptr_] != '\0') {
            read_ptr_ = (read_ptr_ + 1) % capacity_;
        }
        read_ptr_ = (read_ptr_ + 1) % capacity_;
    }

    // write it to buffer
    write_raw(payload, len);
    char null_term = '\0';
    write_raw(&null_term, 1);

    DEBUG_LOG(_DL_CIR, "Enqueued message: %s", payload);
    return CircularQueue::Error::NoError;
}

// dequeue a string, return error if buffer empty
// or message truncated by buffer size
CircularQueue::Error CircularQueue::dequeue(char* buffer, size_t len) {
    DEBUG_LOG(_DL_CIR, "CircularQueue::dequeue");
    CircularQueue::Error ret;

    // check if empty
    if (used_space() == 0) {
        overrun_ = false;
        buffer[0] = '\0';
        ret = CircularQueue::Error::QueueEmpty;
        DEBUG_LOG(_DL_CIR, "Dequeued empty");
        return ret;
    }

    size_t sz = 0;
    while (used_space() > 0) {
        char c = buffer_[read_ptr_];
        read_ptr_ = (read_ptr_ + 1) % capacity_;
        if (c != '\0') {
            // if message doesn't fit in buffer
            // truncate and move read pointer to next message
            if ((sz + 1) >= len) {
                buffer[sz] = '\0';
                ret = CircularQueue::Error::Truncated;
                while (c != '\0') {
                    read_ptr_ = (read_ptr_ + 1) % capacity_;
                    c = buffer_[read_ptr_];
                }
                break;
            }
        }
        buffer[sz] = c;
        // all good
        if (c == '\0') {
            ret = CircularQueue::Error::NoError;
            break;
        }
        sz++;
    }
    DEBUG_LOG(_DL_CIR, "Dequeued message (%d): %s", ret, buffer);
    return ret;
}

// low level writing to end of buffer and handling wrap around
void CircularQueue::write_raw(const char* data, size_t len) {
    DEBUG_LOG(_DL_CIR, "CircularQueue::write_raw %d", len);
    size_t space_to_edge = capacity_ - write_ptr_;
    if (len <= space_to_edge) {
        memcpy(&buffer_[write_ptr_], data, len);
    } else {
        memcpy(&buffer_[write_ptr_], data, space_to_edge);
        memcpy(&buffer_[0], data + space_to_edge, len - space_to_edge);
    }
    write_ptr_ = (write_ptr_ + len) % capacity_;
}

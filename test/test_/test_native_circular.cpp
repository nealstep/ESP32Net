#include <unity.h>
#include <cstring>
#include <iostream>

#include "circular.hpp"

CircularQueue* circularQueue;
static constexpr size_t test_buffer_size = Constants::log_buffer_size / 10;

void setUp(void) {
    // Initialize variables, mock data, etc.
    circularQueue = new CircularQueue(test_buffer_size);
}

void tearDown(void) {
    // Clean up after test
    delete circularQueue;
    circularQueue = nullptr;
}

// Write your test function
void test_circular_queue_setup(void) {
    TEST_ASSERT_NOT_NULL(circularQueue);
    TEST_ASSERT_EQUAL(test_buffer_size - 1, circularQueue->free_space());
}

void test_circular_queue(void) {
    TEST_ASSERT_NOT_NULL(circularQueue);
    circularQueue->enqueue("Hello, World!");
    TEST_ASSERT_EQUAL(test_buffer_size - 1 - 14, circularQueue->free_space());
    char buffer[test_buffer_size];
    circularQueue->dequeue(buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL_STRING("Hello, World!", buffer);
}

void test_circular_queue_overrun(void) {
    TEST_ASSERT_NOT_NULL(circularQueue);
    size_t msg_len = snprintf(nullptr, 0, "Test message %d", 100) + 1;
    char msg[msg_len];
    size_t loops = (test_buffer_size / msg_len) + 3;
    std::cerr << "Loops: " << loops << std::endl;
    if (loops > 100) loops = 100;
    for (size_t i = 0; i < loops; i++) {
        snprintf(msg, msg_len, "Test message %d", i);
        std::cerr << "Enqueuing: " << msg << std::endl;
        circularQueue->enqueue(msg);
    }
    std::cout << std::endl; // add a new line
    TEST_ASSERT_TRUE(circularQueue->overrun());
    circularQueue->dequeue(msg, sizeof(msg));
    std::cerr << "Dequeued: " << msg << std::endl;
    TEST_ASSERT_EQUAL_STRING("Test message 2", msg);
}

void test_circular_queue_dequeue(void) {
    TEST_ASSERT_NOT_NULL(circularQueue);
    size_t msg_len = snprintf(nullptr, 0, "Test message %d", 100) + 1;
    char msg[msg_len];
    for (size_t i = 0; i < 5; i++) {
        snprintf(msg, msg_len, "Test message %d", i);
        std::cerr << "Enqueuing: " << msg << std::endl;
        circularQueue->enqueue(msg);
    }
    auto ret = circularQueue->dequeue(msg, sizeof(msg));
    TEST_ASSERT_EQUAL(CircularQueue::Error::NoError, ret);
    while (ret != CircularQueue::Error::QueueEmty) {
        std::cerr << "Dequeue ret: " << static_cast<int>(ret) << ", msg: " << msg << std::endl;
        ret = circularQueue->dequeue(msg, sizeof(msg));
    }
    std::cerr << "Dequeue ret: " << static_cast<int>(ret) << ", msg: " << msg << std::endl;
    TEST_ASSERT_EQUAL(CircularQueue::Error::QueueEmty, ret);
    TEST_ASSERT_EQUAL_STRING("", msg);
}

void test_circular_queue_message_too_big(void) {
    TEST_ASSERT_NOT_NULL(circularQueue);
    char big_msg[test_buffer_size + 10];
    memset(big_msg, 'A', sizeof(big_msg) - 1);
    big_msg[sizeof(big_msg) - 1] = '\0';
    auto ret = circularQueue->enqueue(big_msg);
    TEST_ASSERT_EQUAL(CircularQueue::Error::MessageTooBig, ret);
}

// The main function is required for native tests
int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_circular_queue_setup);
    RUN_TEST(test_circular_queue);
    RUN_TEST(test_circular_queue_overrun);
    RUN_TEST(test_circular_queue_dequeue);
    RUN_TEST(test_circular_queue_message_too_big);

    UNITY_END();
    return 0; 
}

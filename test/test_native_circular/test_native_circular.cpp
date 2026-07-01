#include <unity.h>

#include <cstring>
#include <iostream>

#include "circular_queue.hpp"

CircularQueue* circularQueue;

static constexpr size_t test_buffer_size = 128;
static constexpr uint16_t test_entries = 15;

static constexpr uint8_t extra_loops = 3;
static constexpr size_t small_test = 5;
static constexpr size_t max_test_mesgs = 100;
static constexpr size_t extra_size = 10;
static constexpr const char* simple_mesg = "Hello, World";
static constexpr const char* test_mesg = "Test message %zu";
static constexpr const char* short_test_mesg = "Test %zu";
static constexpr const char* tiny_test_mesg = "T%zu";

void setUp(void) {
    std::cerr << "Setup" << std::endl;
    circularQueue = new CircularQueue(test_buffer_size, test_entries);
    std::cout << std::endl;
}

void tearDown(void) {
    std::cerr << "Tear down" << std::endl;
    delete circularQueue;
    circularQueue = nullptr;
    std::cout << std::endl;
}

void final_checks(CircularQueue& cq) {
    TEST_ASSERT_EQUAL(test_buffer_size, cq.get_free_capacity());
    TEST_ASSERT_EQUAL(0, cq.get_used_capacity());
    TEST_ASSERT_EQUAL(test_entries, cq.get_free_entries());
    TEST_ASSERT_EQUAL(0, cq.get_used_entries());
    TEST_ASSERT_FALSE(cq.check_overrun());
    std::cout << std::endl;
}

void test_circular_queue_setup(void) {
    std::cerr << "Test circular queue setup" << std::endl;
    TEST_ASSERT_NOT_NULL(circularQueue);
    CircularQueue& cq = *circularQueue;
    final_checks(cq);
}

Log::Err push_code(CircularQueue& cq, const char* mesg,
                                     Entry mlen) {
    std::cerr << "Push: " << mesg << " [" << mlen << "]" << std::endl;
    Log::Err code =
        cq.push(reinterpret_cast<const uint8_t*>(mesg), mlen);
    if (code == Log::Err::NoError)
        std::cerr << "Push succeeded" << std::endl;
    else
        std::cerr << "Push error: " << lg.get_message(code) << std::endl;
    return code;
}

Log::Err pop_code(CircularQueue& cq, char* mesg, size_t msiz,
                                    Entry& mlen) {
    Log::Err code =
        cq.pop(reinterpret_cast<uint8_t*>(mesg), msiz, mlen);
    if (code == Log::Err::NoError)
        std::cerr << "Pop succeeded:" << mesg << " [" << mlen << "]"
                  << std::endl;
    else
        std::cerr << "Pop error: " << lg.get_message(code) << std::endl;
    return code;
}

void test_circular_queue(void) {
    Log::Err code;
    std::cerr << "Test circular queue" << std::endl;
    TEST_ASSERT_NOT_NULL(circularQueue);
    CircularQueue& cq = *circularQueue;
    TEST_ASSERT_FALSE(cq.check_overrun());

    Entry smlen = strlen(simple_mesg) + 1;
    code = push_code(cq, simple_mesg, smlen);

    TEST_ASSERT_EQUAL(Log::Err::NoError, code);
    TEST_ASSERT_EQUAL(test_buffer_size - smlen, cq.get_free_capacity());
    TEST_ASSERT_EQUAL(smlen, cq.get_used_capacity());
    TEST_ASSERT_EQUAL(test_entries - 1, cq.get_free_entries());
    TEST_ASSERT_EQUAL(1, cq.get_used_entries());
    TEST_ASSERT_FALSE(cq.check_overrun());

    char mesg[test_buffer_size];
    Entry mlen;
    code = pop_code(cq, mesg, sizeof(mesg), mlen);

    TEST_ASSERT_EQUAL(Log::Err::NoError, code);
    TEST_ASSERT_EQUAL(smlen, mlen);
    TEST_ASSERT_EQUAL_STRING(simple_mesg, mesg);
    std::cerr << "Original: [" << simple_mesg << "]" << std::endl;
    std::cerr << "Returned: [" << mesg << "]" << std::endl;

    final_checks(cq);
}

void do_overrun(CircularQueue& cq, size_t mesg_len, const char* smesg,
                size_t loops) {
    char mesg[mesg_len];
    Log::Err code;
    for (size_t i = 0; i < loops; i++) {
        snprintf(mesg, mesg_len, smesg, i);
        Entry mlen = strlen(mesg) + 1;
        code = push_code(cq, mesg, mlen);
        TEST_ASSERT_EQUAL(Log::Err::NoError, code);
    }
    if (cq.check_overrun())
        std::cerr << "Overrun Detected" << std::endl;
    TEST_ASSERT_TRUE(cq.check_overrun());
    cq.clear_overrun();
    TEST_ASSERT_FALSE(cq.check_overrun());
    if (!cq.check_overrun())
        std::cerr << "Overrun Cleared" << std::endl;

    Entry rmlen;
    char rmesg[mesg_len];
    char last_mesg[mesg_len];
    code = pop_code(cq, rmesg, sizeof(rmesg), rmlen);
    TEST_ASSERT_EQUAL(Log::Err::NoError, code);
    while (code != Log::Err::QueueEmpty) {
        rmesg[0] = '\0';
        code = pop_code(cq, rmesg, sizeof(rmesg), rmlen);
        if (code != Log::Err::QueueEmpty) {
            strlcpy(last_mesg, rmesg, sizeof(last_mesg));
            TEST_ASSERT_EQUAL(Log::Err::NoError, code);
        }
    }

    std::cerr << "pop (" << lg.get_message(code) << "): " << rmesg << std::endl;
    TEST_ASSERT_EQUAL(Log::Err::QueueEmpty, code);
    TEST_ASSERT_EQUAL_STRING("", rmesg);
    std::cerr << "Last original: [" << mesg << "]" << std::endl;
    std::cerr << "Last returned: [" << last_mesg << "]" << std::endl;
    TEST_ASSERT_EQUAL_STRING(mesg, last_mesg);
}

void test_circular_queue_overrun_buffer(void) {
    Log::Err code;
    std::cerr << "Test circular queue overrrun buffer" << std::endl;
    TEST_ASSERT_NOT_NULL(circularQueue);
    CircularQueue& cq = *circularQueue;

    TEST_ASSERT_NOT_NULL(circularQueue);
    TEST_ASSERT_FALSE(cq.check_overrun());

    // figure out how many loops it will take
    size_t mesg_len = snprintf(nullptr, 0, test_mesg, max_test_mesgs) + 1;
    size_t loops = (test_buffer_size / mesg_len) + extra_loops;
    std::cerr << "Loops: " << loops << std::endl;
    if (loops > max_test_mesgs) {
        std::cerr << "invalid test max test messages too small: "
                  << max_test_mesgs << " < " << loops << std::endl;
        TEST_FAIL();
        return;
    }
    if (loops > test_entries) {
        std::cerr << "invalid test test message too smal entries exhausted: "
                  << test_entries << " > " << loops << std::endl;
        TEST_FAIL();
        return;
    }

    do_overrun(cq, mesg_len, test_mesg, loops);

    final_checks(cq);
}

void test_circular_queue_overrun_records(void) {
    Log::Err code;
    std::cerr << "Test circular queue overrrun records" << std::endl;
    TEST_ASSERT_NOT_NULL(circularQueue);
    CircularQueue& cq = *circularQueue;

    TEST_ASSERT_NOT_NULL(circularQueue);
    TEST_ASSERT_FALSE(cq.check_overrun());

    // figure out how many loops it will take
    size_t mesg_len = snprintf(nullptr, 0, tiny_test_mesg, max_test_mesgs) + 1;
    size_t loops = test_entries + extra_loops;
    std::cerr << "Loops: " << loops << std::endl;
    if (loops > max_test_mesgs) {
        std::cerr << "invalid test max test messages too small: "
                  << max_test_mesgs << " < " << loops << std::endl;
        TEST_FAIL();
        return;
    }
    if (loops * mesg_len > test_buffer_size) {
        std::cerr << "invalid test test buffer size too small: "
                  << test_buffer_size << " < " << loops * mesg_len << std::endl;
        TEST_FAIL();
        return;
    }

    do_overrun(cq, mesg_len, tiny_test_mesg, loops);

    final_checks(cq);
}

void test_circular_queue_overrun_both(void) {
    Log::Err code;
    std::cerr << "Test circular queue overrrun records" << std::endl;
    TEST_ASSERT_NOT_NULL(circularQueue);
    CircularQueue& cq = *circularQueue;

    TEST_ASSERT_NOT_NULL(circularQueue);
    TEST_ASSERT_FALSE(cq.check_overrun());

    // figure out how many loops it will take
    size_t mesg_len = snprintf(nullptr, 0, short_test_mesg, max_test_mesgs) + 1;
    size_t loops = test_entries + extra_loops;
    std::cerr << "Loops: " << loops << std::endl;
    if (loops > max_test_mesgs) {
        std::cerr << "invalid test max test messages too small: "
                  << max_test_mesgs << " < " << loops << std::endl;
        TEST_FAIL();
        return;
    }
    if (loops * mesg_len < test_buffer_size) {
        std::cerr << "invalid test test buffer size too big: "
                  << test_buffer_size << " < " << loops * mesg_len << std::endl;
        TEST_FAIL();
        return;
    }

    do_overrun(cq, mesg_len, short_test_mesg, loops);

    final_checks(cq);
}

void test_circular_queue_dequeue(void) {
    Log::Err code;
    std::cerr << "Test circular queue message too big" << std::endl;
    TEST_ASSERT_NOT_NULL(circularQueue);
    CircularQueue& cq = *circularQueue;

    // add some items to the queue
    size_t mesg_len = snprintf(nullptr, 0, test_mesg, max_test_mesgs) + 1;
    char mesg[mesg_len];
    for (size_t i = 0; i < small_test; i++) {
        snprintf(mesg, mesg_len, test_mesg, i);
        Entry mlen = strlen(mesg) + 1;
        code = push_code(cq, mesg, mlen);
        TEST_ASSERT_EQUAL(Log::Err::NoError, code);
    }
    TEST_ASSERT_FALSE(cq.check_overrun());

    // pull them back out again
    Entry rmlen;
    char rmesg[mesg_len];
    char last_mesg[mesg_len];
    code = pop_code(cq, rmesg, sizeof(rmesg), rmlen);
    TEST_ASSERT_EQUAL(Log::Err::NoError, code);
    while (code != Log::Err::QueueEmpty) {
        rmesg[0] = '\0';
        code = pop_code(cq, rmesg, sizeof(rmesg), rmlen);
        if (code != Log::Err::QueueEmpty) {
            strlcpy(last_mesg, rmesg, sizeof(last_mesg));
            TEST_ASSERT_EQUAL(Log::Err::NoError, code);
        }
    }

    std::cerr << "pop (" << lg.get_message(code)
              << "): " << rmesg << std::endl;
    TEST_ASSERT_EQUAL(Log::Err::QueueEmpty, code);
    TEST_ASSERT_EQUAL_STRING("", rmesg);
    std::cerr << "Last original: [" << mesg << "]" << std::endl;
    std::cerr << "Last returned: [" << last_mesg << "]" << std::endl;
    TEST_ASSERT_EQUAL_STRING(mesg, last_mesg);

    final_checks(cq);
}

void test_circular_queue_message_too_big(void) {
    Log::Err code;
    std::cerr << "Test circular queue message too big" << std::endl;
    TEST_ASSERT_NOT_NULL(circularQueue);
    CircularQueue& cq = *circularQueue;
    char big_mesg[test_buffer_size + extra_size];
    memset(big_mesg, 'A', sizeof(big_mesg) - 1);
    big_mesg[sizeof(big_mesg) - 1] = '\0';
    Entry slen = strlen(big_mesg) + 1;
    code = push_code(cq, big_mesg, slen);
    TEST_ASSERT_EQUAL(Log::Err::ItemTooBig, code);

    final_checks(cq);
}

// The main function is required for native tests
int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_circular_queue_setup);
    RUN_TEST(test_circular_queue);
    RUN_TEST(test_circular_queue_overrun_buffer);
    RUN_TEST(test_circular_queue_overrun_records);
    RUN_TEST(test_circular_queue_overrun_both);
    RUN_TEST(test_circular_queue_dequeue);
    RUN_TEST(test_circular_queue_message_too_big);

    UNITY_END();
    return 0;
}

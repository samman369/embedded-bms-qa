#include "unity.h"
#include "frame_reader.h"
#include <string.h>

static frame_reader_t fr;

void setUp(void)
{
    frame_reader_init(&fr);
}

void tearDown(void) {}

/* Feeds a string; returns the result of the LAST byte. */
static fr_result_t feed(const char *s)
{
    fr_result_t r = FR_NONE;
    for (size_t i = 0u; s[i] != '\0'; i++) {
        r = frame_reader_feed(&fr, (uint8_t)s[i]);
    }
    return r;
}

static void feed_n(char c, size_t n)
{
    for (size_t i = 0u; i < n; i++) {
        TEST_ASSERT_EQUAL(FR_NONE, frame_reader_feed(&fr, (uint8_t)c));
    }
}

void test_line_is_reported_on_newline__REQ_COM_001(void)
{
    TEST_ASSERT_EQUAL(FR_NONE, feed("$PING*1F"));
    TEST_ASSERT_EQUAL(FR_FRAME_READY, feed("\n"));
    TEST_ASSERT_EQUAL_STRING("$PING*1F", frame_reader_line(&fr));
}

void test_carriage_return_is_ignored__REQ_COM_001(void)
{
    TEST_ASSERT_EQUAL(FR_FRAME_READY, feed("$PING*1F\r\n"));
    TEST_ASSERT_EQUAL_STRING("$PING*1F", frame_reader_line(&fr));
}

void test_empty_lines_are_ignored__REQ_COM_001(void)
{
    TEST_ASSERT_EQUAL(FR_NONE, feed("\n"));
    TEST_ASSERT_EQUAL(FR_NONE, feed("\r\n"));
}

void test_consecutive_frames_are_separated__REQ_COM_001(void)
{
    TEST_ASSERT_EQUAL(FR_FRAME_READY, feed("$A*00\n"));
    TEST_ASSERT_EQUAL_STRING("$A*00", frame_reader_line(&fr));
    TEST_ASSERT_EQUAL(FR_FRAME_READY, feed("$BB*00\n"));
    TEST_ASSERT_EQUAL_STRING("$BB*00", frame_reader_line(&fr));
}

void test_line_of_exactly_max_length_is_accepted__REQ_COM_004(void)
{
    feed_n('A', PROTO_MAX_FRAME_LEN);
    TEST_ASSERT_EQUAL(FR_FRAME_READY, feed("\n"));
    TEST_ASSERT_EQUAL_size_t(PROTO_MAX_FRAME_LEN, strlen(frame_reader_line(&fr)));
}

void test_line_one_over_max_length_is_too_long__REQ_COM_004(void)
{
    feed_n('A', PROTO_MAX_FRAME_LEN + 1u);
    TEST_ASSERT_EQUAL(FR_FRAME_TOO_LONG, feed("\n"));
}

void test_reader_recovers_after_too_long_line__REQ_COM_004(void)
{
    feed_n('A', 200u);
    TEST_ASSERT_EQUAL(FR_FRAME_TOO_LONG, feed("\n"));
    TEST_ASSERT_EQUAL(FR_FRAME_READY, feed("$PING*1F\n"));
    TEST_ASSERT_EQUAL_STRING("$PING*1F", frame_reader_line(&fr));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_line_is_reported_on_newline__REQ_COM_001);
    RUN_TEST(test_carriage_return_is_ignored__REQ_COM_001);
    RUN_TEST(test_empty_lines_are_ignored__REQ_COM_001);
    RUN_TEST(test_consecutive_frames_are_separated__REQ_COM_001);
    RUN_TEST(test_line_of_exactly_max_length_is_accepted__REQ_COM_004);
    RUN_TEST(test_line_one_over_max_length_is_too_long__REQ_COM_004);
    RUN_TEST(test_reader_recovers_after_too_long_line__REQ_COM_004);
    return UNITY_END();
}

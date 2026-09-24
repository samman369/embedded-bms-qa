/* Integration of ISR -> ring buffer -> frame reader -> protocol -> command -> UART TX. */
#include "unity.h"
#include "app.h"
#include "bms_config.h"
#include "fake_hal.h"
#include <string.h>

void setUp(void)
{
    fake_hal_reset();
    app_init();
}

void tearDown(void) {}

static void rx(const char *s)
{
    for (size_t i = 0u; s[i] != '\0'; i++) {
        app_uart_rx_isr((uint8_t)s[i]);
    }
}

static const char *rx_and_poll(const char *s)
{
    fake_hal_uart_clear();
    rx(s);
    app_poll();
    return fake_hal_uart_output();
}

void test_ping_frame_round_trip__REQ_COM_010(void)
{
    TEST_ASSERT_EQUAL_STRING("$OK,PONG*6E\n", rx_and_poll("$PING*1F\n"));
}

void test_bad_crc_frame_answers_err_crc__REQ_COM_002(void)
{
    TEST_ASSERT_EQUAL_STRING("$ERR,CRC*9E\n", rx_and_poll("$PING*00\n"));
}

void test_garbage_line_answers_err_format__REQ_COM_003(void)
{
    TEST_ASSERT_EQUAL_STRING("$ERR,FORMAT*4D\n", rx_and_poll("hello\n"));
}

void test_too_long_line_answers_err_length_then_recovers__REQ_COM_004(void)
{
    char longline[100];
    memset(longline, 'X', 90u);
    longline[90] = '\n';
    longline[91] = '\0';
    TEST_ASSERT_EQUAL_STRING("$ERR,LENGTH*C7\n", rx_and_poll(longline));
    TEST_ASSERT_EQUAL_STRING("$OK,PONG*6E\n", rx_and_poll("$PING*1F\n"));
}

void test_two_frames_in_one_burst_get_two_answers__REQ_COM_001(void)
{
    TEST_ASSERT_EQUAL_STRING("$OK,PONG*6E\n$OK,PONG*6E\n", rx_and_poll("$PING*1F\n$PING*1F\n"));
}

void test_rx_overflow_is_counted_when_main_loop_is_late__REQ_DRV_001(void)
{
    for (unsigned i = 0u; i < APP_RX_BUFFER_SIZE + 10u; i++) {
        app_uart_rx_isr((uint8_t)'A');
    }
    TEST_ASSERT_EQUAL_UINT16(11u, app_rx_overflow_count());
}

void test_tick_runs_bms_cycle__REQ_BMS_001(void)
{
    app_tick();
    TEST_ASSERT_TRUE(fake_hal_charge_fet());
    TEST_ASSERT_TRUE(fake_hal_discharge_fet());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ping_frame_round_trip__REQ_COM_010);
    RUN_TEST(test_bad_crc_frame_answers_err_crc__REQ_COM_002);
    RUN_TEST(test_garbage_line_answers_err_format__REQ_COM_003);
    RUN_TEST(test_too_long_line_answers_err_length_then_recovers__REQ_COM_004);
    RUN_TEST(test_two_frames_in_one_burst_get_two_answers__REQ_COM_001);
    RUN_TEST(test_rx_overflow_is_counted_when_main_loop_is_late__REQ_DRV_001);
    RUN_TEST(test_tick_runs_bms_cycle__REQ_BMS_001);
    return UNITY_END();
}

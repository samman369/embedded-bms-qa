#include "unity.h"
#include "crc8.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static uint8_t crc_of(const char *s)
{
    return crc8((const uint8_t *)s, strlen(s));
}

void test_standard_check_value_123456789_is_0xF4__REQ_COM_001(void)
{
    /* Published check value for CRC-8/SMBUS */
    TEST_ASSERT_EQUAL_HEX8(0xF4, crc_of("123456789"));
}

void test_empty_input_returns_init_value__REQ_COM_001(void)
{
    TEST_ASSERT_EQUAL_HEX8(CRC8_INIT, crc8((const uint8_t *)"", 0u));
}

void test_null_pointer_returns_init_value__REQ_COM_001(void)
{
    TEST_ASSERT_EQUAL_HEX8(CRC8_INIT, crc8(NULL, 10u));
}

void test_single_byte_0x01_equals_polynomial__REQ_COM_001(void)
{
    const uint8_t b = 0x01u;
    TEST_ASSERT_EQUAL_HEX8(CRC8_POLY, crc8(&b, 1u));
}

void test_single_bit_flip_changes_crc__REQ_COM_001(void)
{
    char msg[] = "GET_STATUS";
    uint8_t original = crc_of(msg);
    msg[3] ^= 0x01;
    TEST_ASSERT_NOT_EQUAL(original, crc_of(msg));
}

void test_known_command_crc_PING__REQ_COM_001(void)
{
    /* Reference value computed independently (Python) — used in the docs examples */
    TEST_ASSERT_EQUAL_HEX8(0x1F, crc_of("PING"));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_standard_check_value_123456789_is_0xF4__REQ_COM_001);
    RUN_TEST(test_empty_input_returns_init_value__REQ_COM_001);
    RUN_TEST(test_null_pointer_returns_init_value__REQ_COM_001);
    RUN_TEST(test_single_byte_0x01_equals_polynomial__REQ_COM_001);
    RUN_TEST(test_single_bit_flip_changes_crc__REQ_COM_001);
    RUN_TEST(test_known_command_crc_PING__REQ_COM_001);
    return UNITY_END();
}

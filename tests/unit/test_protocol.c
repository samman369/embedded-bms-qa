#include "unity.h"
#include "protocol.h"
#include <string.h>

static char payload[PROTO_MAX_FRAME_LEN + 1u];

void setUp(void)
{
    memset(payload, 0, sizeof(payload));
}

void tearDown(void) {}

static proto_status_t decode(const char *frame)
{
    return protocol_decode(frame, payload, sizeof(payload));
}

/* Builds a valid frame (without '\n') with a payload of exactly n characters. */
static void make_frame_with_payload_len(char *frame, size_t frame_size, size_t n)
{
    char body[128];
    memset(body, 'A', n);
    body[n] = '\0';
    size_t len = protocol_encode(body, frame, frame_size);
    TEST_ASSERT_TRUE(len > 0u);
    frame[len - 1u] = '\0';   /* strip '\n' */
}

/* ---------- encode ---------- */

void test_encode_builds_frame_with_crc_and_newline__REQ_COM_001(void)
{
    char out[32];
    TEST_ASSERT_EQUAL_size_t(9u, protocol_encode("PING", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("$PING*1F\n", out);
}

void test_encode_rejects_empty_null_and_illegal_payload__REQ_COM_001(void)
{
    char out[32];
    TEST_ASSERT_EQUAL_size_t(0u, protocol_encode("", out, sizeof(out)));
    TEST_ASSERT_EQUAL_size_t(0u, protocol_encode(NULL, out, sizeof(out)));
    TEST_ASSERT_EQUAL_size_t(0u, protocol_encode("PING", NULL, sizeof(out)));
    TEST_ASSERT_EQUAL_size_t(0u, protocol_encode("A*B", out, sizeof(out)));
    TEST_ASSERT_EQUAL_size_t(0u, protocol_encode("A\nB", out, sizeof(out)));
}

void test_encode_rejects_too_small_buffer__REQ_COM_001(void)
{
    char out[10];                                                  /* needs 9 + NUL */
    TEST_ASSERT_EQUAL_size_t(0u, protocol_encode("PING", out, 9u));
    TEST_ASSERT_EQUAL_size_t(9u, protocol_encode("PING", out, 10u));
}

/* ---------- decode: valid ---------- */

void test_decode_valid_frame__REQ_COM_001(void)
{
    TEST_ASSERT_EQUAL(PROTO_OK, decode("$PING*1F"));
    TEST_ASSERT_EQUAL_STRING("PING", payload);
}

void test_decode_accepts_lowercase_crc_hex__REQ_COM_001(void)
{
    char frame[40];
    make_frame_with_payload_len(frame, sizeof(frame), 3u);        /* "$AAA*XX" */
    frame[5] = (char)((frame[5] >= 'A' && frame[5] <= 'F') ? frame[5] + 32 : frame[5]);
    frame[6] = (char)((frame[6] >= 'A' && frame[6] <= 'F') ? frame[6] + 32 : frame[6]);
    TEST_ASSERT_EQUAL(PROTO_OK, decode(frame));
}

/* ---------- decode: CRC ---------- */

void test_decode_wrong_crc_is_rejected__REQ_COM_002(void)
{
    TEST_ASSERT_EQUAL(PROTO_ERR_CRC, decode("$PING*1E"));
}

void test_decode_corrupted_payload_is_rejected__REQ_COM_002(void)
{
    TEST_ASSERT_EQUAL(PROTO_ERR_CRC, decode("$PONG*1F"));
}

/* ---------- decode: format ---------- */

void test_decode_missing_start_of_frame__REQ_COM_003(void)
{
    TEST_ASSERT_EQUAL(PROTO_ERR_FORMAT, decode("PING*1F"));
}

void test_decode_missing_crc_separator__REQ_COM_003(void)
{
    TEST_ASSERT_EQUAL(PROTO_ERR_FORMAT, decode("$PING5B"));
}

void test_decode_non_hex_crc__REQ_COM_003(void)
{
    TEST_ASSERT_EQUAL(PROTO_ERR_FORMAT, decode("$PING*G1"));
    TEST_ASSERT_EQUAL(PROTO_ERR_FORMAT, decode("$PING*1G"));
}

void test_decode_empty_payload__REQ_COM_003(void)
{
    TEST_ASSERT_EQUAL(PROTO_ERR_FORMAT, decode("$*00"));
}

void test_decode_too_short_and_empty_frames__REQ_COM_003(void)
{
    TEST_ASSERT_EQUAL(PROTO_ERR_FORMAT, decode(""));
    TEST_ASSERT_EQUAL(PROTO_ERR_FORMAT, decode("$"));
    TEST_ASSERT_EQUAL(PROTO_ERR_FORMAT, decode("$A*0"));
}

void test_decode_illegal_character_in_payload__REQ_COM_003(void)
{
    TEST_ASSERT_EQUAL(PROTO_ERR_FORMAT, decode("$PI$NG*1F"));
    TEST_ASSERT_EQUAL(PROTO_ERR_FORMAT, decode("$PI*NG*1F"));
    TEST_ASSERT_EQUAL(PROTO_ERR_FORMAT, decode("$PI\tNG*1F"));
}

void test_decode_null_arguments__REQ_COM_003(void)
{
    TEST_ASSERT_EQUAL(PROTO_ERR_FORMAT, protocol_decode(NULL, payload, sizeof(payload)));
    TEST_ASSERT_EQUAL(PROTO_ERR_FORMAT, protocol_decode("$PING*1F", NULL, 10u));
}

/* ---------- decode: length boundaries ---------- */

void test_decode_frame_of_exactly_max_length_is_accepted__REQ_COM_004(void)
{
    char frame[80];
    make_frame_with_payload_len(frame, sizeof(frame), PROTO_MAX_FRAME_LEN - 4u);
    TEST_ASSERT_EQUAL_size_t(PROTO_MAX_FRAME_LEN, strlen(frame));
    TEST_ASSERT_EQUAL(PROTO_OK, decode(frame));
}

void test_decode_frame_one_over_max_length_is_rejected__REQ_COM_004(void)
{
    char frame[80];
    make_frame_with_payload_len(frame, sizeof(frame), PROTO_MAX_FRAME_LEN - 3u);
    TEST_ASSERT_EQUAL_size_t(PROTO_MAX_FRAME_LEN + 1u, strlen(frame));
    TEST_ASSERT_EQUAL(PROTO_ERR_LENGTH, decode(frame));
}

void test_decode_payload_buffer_too_small__REQ_COM_004(void)
{
    char small[4];
    TEST_ASSERT_EQUAL(PROTO_ERR_LENGTH, protocol_decode("$PING*1F", small, sizeof(small)));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_encode_builds_frame_with_crc_and_newline__REQ_COM_001);
    RUN_TEST(test_encode_rejects_empty_null_and_illegal_payload__REQ_COM_001);
    RUN_TEST(test_encode_rejects_too_small_buffer__REQ_COM_001);
    RUN_TEST(test_decode_valid_frame__REQ_COM_001);
    RUN_TEST(test_decode_accepts_lowercase_crc_hex__REQ_COM_001);
    RUN_TEST(test_decode_wrong_crc_is_rejected__REQ_COM_002);
    RUN_TEST(test_decode_corrupted_payload_is_rejected__REQ_COM_002);
    RUN_TEST(test_decode_missing_start_of_frame__REQ_COM_003);
    RUN_TEST(test_decode_missing_crc_separator__REQ_COM_003);
    RUN_TEST(test_decode_non_hex_crc__REQ_COM_003);
    RUN_TEST(test_decode_empty_payload__REQ_COM_003);
    RUN_TEST(test_decode_too_short_and_empty_frames__REQ_COM_003);
    RUN_TEST(test_decode_illegal_character_in_payload__REQ_COM_003);
    RUN_TEST(test_decode_null_arguments__REQ_COM_003);
    RUN_TEST(test_decode_frame_of_exactly_max_length_is_accepted__REQ_COM_004);
    RUN_TEST(test_decode_frame_one_over_max_length_is_rejected__REQ_COM_004);
    RUN_TEST(test_decode_payload_buffer_too_small__REQ_COM_004);
    return UNITY_END();
}

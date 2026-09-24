#include "unity.h"
#include "ring_buffer.h"

#define STORAGE_SIZE 8u

static uint8_t       storage[STORAGE_SIZE];
static ring_buffer_t rb;

void setUp(void)
{
    TEST_ASSERT_TRUE(rb_init(&rb, storage, STORAGE_SIZE));
}

void tearDown(void) {}

static void fill(uint16_t n)
{
    for (uint16_t i = 0u; i < n; i++) {
        TEST_ASSERT_TRUE(rb_push(&rb, (uint8_t)i));
    }
}

void test_init_rejects_invalid_arguments__REQ_DRV_001(void)
{
    ring_buffer_t other;
    TEST_ASSERT_FALSE(rb_init(NULL, storage, STORAGE_SIZE));
    TEST_ASSERT_FALSE(rb_init(&other, NULL, STORAGE_SIZE));
    TEST_ASSERT_FALSE(rb_init(&other, storage, 1u));
    TEST_ASSERT_TRUE(rb_init(&other, storage, 2u));
}

void test_new_buffer_is_empty__REQ_DRV_001(void)
{
    TEST_ASSERT_TRUE(rb_is_empty(&rb));
    TEST_ASSERT_FALSE(rb_is_full(&rb));
    TEST_ASSERT_EQUAL_UINT16(0u, rb_count(&rb));
}

void test_capacity_is_size_minus_one__REQ_DRV_001(void)
{
    TEST_ASSERT_EQUAL_UINT16(STORAGE_SIZE - 1u, rb_capacity(&rb));
    fill(STORAGE_SIZE - 1u);
    TEST_ASSERT_TRUE(rb_is_full(&rb));
}

void test_bytes_come_out_in_fifo_order__REQ_DRV_001(void)
{
    uint8_t out;
    fill(5u);
    for (uint8_t i = 0u; i < 5u; i++) {
        TEST_ASSERT_TRUE(rb_pop(&rb, &out));
        TEST_ASSERT_EQUAL_UINT8(i, out);
    }
    TEST_ASSERT_TRUE(rb_is_empty(&rb));
}

void test_pop_from_empty_returns_false__REQ_DRV_001(void)
{
    uint8_t out = 0xAAu;
    TEST_ASSERT_FALSE(rb_pop(&rb, &out));
    TEST_ASSERT_EQUAL_HEX8(0xAAu, out);   /* output untouched */
}

void test_overflow_drops_new_byte_and_counts__REQ_DRV_001(void)
{
    uint8_t out;
    fill(STORAGE_SIZE - 1u);
    TEST_ASSERT_FALSE(rb_push(&rb, 0xEEu));
    TEST_ASSERT_FALSE(rb_push(&rb, 0xEFu));
    TEST_ASSERT_EQUAL_UINT16(2u, rb.overflow_count);

    TEST_ASSERT_TRUE(rb_pop(&rb, &out));
    TEST_ASSERT_EQUAL_UINT8(0u, out);     /* oldest data kept, new data dropped */
}

void test_wrap_around_keeps_order__REQ_DRV_001(void)
{
    uint8_t out;
    for (uint8_t round = 0u; round < 20u; round++) {
        TEST_ASSERT_TRUE(rb_push(&rb, round));
        TEST_ASSERT_TRUE(rb_pop(&rb, &out));
        TEST_ASSERT_EQUAL_UINT8(round, out);
    }
    TEST_ASSERT_EQUAL_UINT16(0u, rb.overflow_count);
}

void test_reset_empties_buffer_and_counter__REQ_DRV_001(void)
{
    fill(STORAGE_SIZE - 1u);
    (void)rb_push(&rb, 1u);
    rb_reset(&rb);
    TEST_ASSERT_TRUE(rb_is_empty(&rb));
    TEST_ASSERT_EQUAL_UINT16(0u, rb.overflow_count);
}

void test_null_buffer_is_handled_safely__REQ_DRV_001(void)
{
    uint8_t out;
    TEST_ASSERT_FALSE(rb_push(NULL, 1u));
    TEST_ASSERT_FALSE(rb_pop(NULL, &out));
    TEST_ASSERT_FALSE(rb_pop(&rb, NULL));
    TEST_ASSERT_EQUAL_UINT16(0u, rb_count(NULL));
    TEST_ASSERT_EQUAL_UINT16(0u, rb_capacity(NULL));
    TEST_ASSERT_FALSE(rb_is_full(NULL));
    rb_reset(NULL);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_rejects_invalid_arguments__REQ_DRV_001);
    RUN_TEST(test_new_buffer_is_empty__REQ_DRV_001);
    RUN_TEST(test_capacity_is_size_minus_one__REQ_DRV_001);
    RUN_TEST(test_bytes_come_out_in_fifo_order__REQ_DRV_001);
    RUN_TEST(test_pop_from_empty_returns_false__REQ_DRV_001);
    RUN_TEST(test_overflow_drops_new_byte_and_counts__REQ_DRV_001);
    RUN_TEST(test_wrap_around_keeps_order__REQ_DRV_001);
    RUN_TEST(test_reset_empties_buffer_and_counter__REQ_DRV_001);
    RUN_TEST(test_null_buffer_is_handled_safely__REQ_DRV_001);
    return UNITY_END();
}

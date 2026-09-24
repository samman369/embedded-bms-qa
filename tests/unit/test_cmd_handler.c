#include "unity.h"
#include "bms.h"
#include "cmd_handler.h"
#include "fake_hal.h"

static char resp[APP_RESP_MAX];

void setUp(void)
{
    fake_hal_reset();
    bms_init();
    resp[0] = '\0';
}

void tearDown(void) {}

static const char *run(const char *payload)
{
    cmd_handle(payload, resp, sizeof(resp));
    return resp;
}

void test_ping_returns_pong__REQ_COM_010(void)
{
    TEST_ASSERT_EQUAL_STRING("OK,PONG", run("PING"));
}

void test_get_version__REQ_COM_011(void)
{
    TEST_ASSERT_EQUAL_STRING("OK,VERSION=" FW_VERSION, run("GET_VERSION"));
}

void test_get_status_before_first_cycle__REQ_COM_012(void)
{
    TEST_ASSERT_EQUAL_STRING("OK,STATE=INIT,FAULTS=0x00,VMIN=0,VMAX=0,TEMP=0,CUR=0", run("GET_STATUS"));
}

void test_get_status_reports_measurements__REQ_COM_012(void)
{
    fake_hal_set_cell_mv(0u, 3650u);
    fake_hal_set_cell_mv(3u, 3810u);
    fake_hal_set_temp_dc(-52);
    fake_hal_set_current_ma(-1500);
    bms_tick();
    TEST_ASSERT_EQUAL_STRING("OK,STATE=DISCHARGING,FAULTS=0x00,VMIN=3650,VMAX=3810,TEMP=-52,CUR=-1500",
                             run("GET_STATUS"));
}

void test_get_status_reports_fault_bits__REQ_COM_012(void)
{
    fake_hal_set_temp_dc(650);
    for (int i = 0; i < 3; i++) { bms_tick(); }
    TEST_ASSERT_EQUAL_STRING("OK,STATE=FAULT,FAULTS=0x04,VMIN=3700,VMAX=3700,TEMP=650,CUR=0",
                             run("GET_STATUS"));
}

void test_get_cell_valid_indexes__REQ_COM_013(void)
{
    fake_hal_set_cell_mv(0u, 3601u);
    fake_hal_set_cell_mv(3u, 3604u);
    bms_tick();
    TEST_ASSERT_EQUAL_STRING("OK,CELL0=3601", run("GET_CELL,0"));
    TEST_ASSERT_EQUAL_STRING("OK,CELL3=3604", run("GET_CELL,3"));
}

void test_get_cell_invalid_arguments__REQ_COM_013(void)
{
    TEST_ASSERT_EQUAL_STRING("ERR,ARG", run("GET_CELL,4"));
    TEST_ASSERT_EQUAL_STRING("ERR,ARG", run("GET_CELL,999"));
    TEST_ASSERT_EQUAL_STRING("ERR,ARG", run("GET_CELL,1000"));
    TEST_ASSERT_EQUAL_STRING("ERR,ARG", run("GET_CELL,-1"));
    TEST_ASSERT_EQUAL_STRING("ERR,ARG", run("GET_CELL,A"));
    TEST_ASSERT_EQUAL_STRING("ERR,ARG", run("GET_CELL,1,2"));
    TEST_ASSERT_EQUAL_STRING("ERR,ARG", run("GET_CELL,"));
    TEST_ASSERT_EQUAL_STRING("ERR,ARG", run("GET_CELL"));
}

void test_clear_faults_success_and_rejection__REQ_COM_014(void)
{
    fake_hal_set_temp_dc(650);
    for (int i = 0; i < 3; i++) { bms_tick(); }
    TEST_ASSERT_EQUAL_STRING("ERR,FAULT_ACTIVE", run("CLEAR_FAULTS"));
    fake_hal_set_temp_dc(250);
    bms_tick();
    TEST_ASSERT_EQUAL_STRING("OK,CLEARED", run("CLEAR_FAULTS"));
}

void test_unknown_and_case_sensitive_commands__REQ_COM_005(void)
{
    TEST_ASSERT_EQUAL_STRING("ERR,UNKNOWN_CMD", run("REBOOT"));
    TEST_ASSERT_EQUAL_STRING("ERR,UNKNOWN_CMD", run("ping"));
    TEST_ASSERT_EQUAL_STRING("ERR,UNKNOWN_CMD", run("PING "));
    TEST_ASSERT_EQUAL_STRING("ERR,UNKNOWN_CMD", run("GET_CELLS"));
}

void test_null_and_zero_size_are_ignored__REQ_COM_005(void)
{
    resp[0] = 'X';
    cmd_handle(NULL, resp, sizeof(resp));
    cmd_handle("PING", NULL, sizeof(resp));
    cmd_handle("PING", resp, 0u);
    TEST_ASSERT_EQUAL_CHAR('X', resp[0]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ping_returns_pong__REQ_COM_010);
    RUN_TEST(test_get_version__REQ_COM_011);
    RUN_TEST(test_get_status_before_first_cycle__REQ_COM_012);
    RUN_TEST(test_get_status_reports_measurements__REQ_COM_012);
    RUN_TEST(test_get_status_reports_fault_bits__REQ_COM_012);
    RUN_TEST(test_get_cell_valid_indexes__REQ_COM_013);
    RUN_TEST(test_get_cell_invalid_arguments__REQ_COM_013);
    RUN_TEST(test_clear_faults_success_and_rejection__REQ_COM_014);
    RUN_TEST(test_unknown_and_case_sensitive_commands__REQ_COM_005);
    RUN_TEST(test_null_and_zero_size_are_ignored__REQ_COM_005);
    return UNITY_END();
}

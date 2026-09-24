#include "unity.h"
#include "bms.h"
#include "fake_hal.h"

void setUp(void)
{
    fake_hal_reset();
    bms_init();
}

void tearDown(void) {}

static void tick(unsigned n)
{
    for (unsigned i = 0u; i < n; i++) {
        bms_tick();
    }
}

static bms_state_t state(void)  { return bms_get_status()->state; }
static uint8_t     faults(void) { return bms_get_status()->faults; }

static void assert_fets(bool on)
{
    TEST_ASSERT_EQUAL(on, fake_hal_charge_fet());
    TEST_ASSERT_EQUAL(on, fake_hal_discharge_fet());
}

/* ================= Power-up ================= */

void test_power_up_state_is_INIT_with_fets_off__REQ_BMS_001(void)
{
    TEST_ASSERT_EQUAL(BMS_STATE_INIT, state());
    assert_fets(false);
}

void test_first_cycle_with_nominal_inputs_enters_IDLE_fets_on__REQ_BMS_001(void)
{
    tick(1u);
    TEST_ASSERT_EQUAL(BMS_STATE_IDLE, state());
    TEST_ASSERT_EQUAL_HEX8(BMS_FAULT_NONE, faults());
    assert_fets(true);
}

/* ================= Over-voltage ================= */

void test_ov_does_not_trip_at_4199mV__REQ_BMS_010(void)
{
    fake_hal_set_cell_mv(2u, 4199u);
    tick(10u);
    TEST_ASSERT_EQUAL_HEX8(BMS_FAULT_NONE, faults());
}

void test_ov_trips_at_exactly_4200mV_after_3_cycles__REQ_BMS_010(void)
{
    fake_hal_set_cell_mv(2u, 4200u);
    tick(3u);
    TEST_ASSERT_EQUAL_HEX8(BMS_FAULT_OV, faults());
    TEST_ASSERT_EQUAL(BMS_STATE_FAULT, state());
}

void test_ov_does_not_trip_after_only_2_cycles__REQ_BMS_010(void)
{
    fake_hal_set_cell_mv(0u, 4300u);
    tick(2u);
    TEST_ASSERT_EQUAL_HEX8(BMS_FAULT_NONE, faults());
}

void test_ov_debounce_restarts_when_voltage_dips__REQ_BMS_010(void)
{
    fake_hal_set_cell_mv(0u, 4300u);
    tick(2u);
    fake_hal_set_cell_mv(0u, 4000u);   /* glitch ends */
    tick(1u);
    fake_hal_set_cell_mv(0u, 4300u);
    tick(2u);
    TEST_ASSERT_EQUAL_HEX8(BMS_FAULT_NONE, faults());
    tick(1u);
    TEST_ASSERT_EQUAL_HEX8(BMS_FAULT_OV, faults());
}

void test_ov_stays_active_above_release_threshold__REQ_BMS_011(void)
{
    fake_hal_set_cell_mv(1u, 4250u);
    tick(3u);
    fake_hal_set_cell_mv(1u, 4101u);
    tick(5u);
    TEST_ASSERT_FALSE(bms_clear_faults());
    TEST_ASSERT_EQUAL(BMS_STATE_FAULT, state());
}

void test_ov_releases_at_4100mV_and_can_be_cleared__REQ_BMS_011(void)
{
    fake_hal_set_cell_mv(1u, 4250u);
    tick(3u);
    fake_hal_set_cell_mv(1u, 4100u);
    tick(1u);
    TEST_ASSERT_TRUE(bms_clear_faults());
    TEST_ASSERT_EQUAL(BMS_STATE_IDLE, state());
}

/* ================= Under-voltage ================= */

void test_uv_does_not_trip_at_3001mV__REQ_BMS_020(void)
{
    fake_hal_set_cell_mv(3u, 3001u);
    tick(10u);
    TEST_ASSERT_EQUAL_HEX8(BMS_FAULT_NONE, faults());
}

void test_uv_trips_at_exactly_3000mV_after_3_cycles__REQ_BMS_020(void)
{
    fake_hal_set_cell_mv(3u, 3000u);
    tick(2u);
    TEST_ASSERT_EQUAL_HEX8(BMS_FAULT_NONE, faults());
    tick(1u);
    TEST_ASSERT_EQUAL_HEX8(BMS_FAULT_UV, faults());
}

void test_uv_stays_active_below_release_threshold__REQ_BMS_021(void)
{
    fake_hal_set_all_cells_mv(2900u);
    tick(3u);
    fake_hal_set_all_cells_mv(3099u);
    tick(1u);
    TEST_ASSERT_FALSE(bms_clear_faults());
}

void test_uv_releases_at_3100mV__REQ_BMS_021(void)
{
    fake_hal_set_all_cells_mv(2900u);
    tick(3u);
    fake_hal_set_all_cells_mv(3100u);
    tick(1u);
    TEST_ASSERT_TRUE(bms_clear_faults());
}

/* ================= Over-temperature ================= */

void test_ot_does_not_trip_at_59_9C__REQ_BMS_030(void)
{
    fake_hal_set_temp_dc(599);
    tick(10u);
    TEST_ASSERT_EQUAL_HEX8(BMS_FAULT_NONE, faults());
}

void test_ot_trips_at_60_0C_after_3_cycles__REQ_BMS_030(void)
{
    fake_hal_set_temp_dc(600);
    tick(3u);
    TEST_ASSERT_EQUAL_HEX8(BMS_FAULT_OT, faults());
}

void test_ot_stays_active_at_55_1C__REQ_BMS_031(void)
{
    fake_hal_set_temp_dc(700);
    tick(3u);
    fake_hal_set_temp_dc(551);
    tick(1u);
    TEST_ASSERT_FALSE(bms_clear_faults());
}

void test_ot_releases_at_55_0C__REQ_BMS_031(void)
{
    fake_hal_set_temp_dc(700);
    tick(3u);
    fake_hal_set_temp_dc(550);
    tick(1u);
    TEST_ASSERT_TRUE(bms_clear_faults());
}

/* ================= Over-current ================= */

void test_oc_discharge_boundary_29999_vs_30000mA__REQ_BMS_040(void)
{
    fake_hal_set_current_ma(-29999);
    tick(10u);
    TEST_ASSERT_EQUAL_HEX8(BMS_FAULT_NONE, faults());
    fake_hal_set_current_ma(-30000);
    tick(3u);
    TEST_ASSERT_EQUAL_HEX8(BMS_FAULT_OC, faults());
}

void test_oc_charge_boundary_9999_vs_10000mA__REQ_BMS_040(void)
{
    fake_hal_set_current_ma(9999);
    tick(10u);
    TEST_ASSERT_EQUAL_HEX8(BMS_FAULT_NONE, faults());
    fake_hal_set_current_ma(10000);
    tick(3u);
    TEST_ASSERT_EQUAL_HEX8(BMS_FAULT_OC, faults());
}

void test_oc_releases_as_soon_as_current_is_within_limits__REQ_BMS_040(void)
{
    fake_hal_set_current_ma(-35000);
    tick(3u);
    fake_hal_set_current_ma(0);
    tick(1u);
    TEST_ASSERT_TRUE(bms_clear_faults());
}

/* ================= Sensor plausibility ================= */

void test_sensor_fault_on_0mV_within_one_cycle__REQ_BMS_045(void)
{
    fake_hal_set_cell_mv(0u, 0u);
    tick(1u);
    TEST_ASSERT_EQUAL_HEX8(BMS_FAULT_SENSOR, faults());
}

void test_sensor_valid_window_boundaries__REQ_BMS_045(void)
{
    fake_hal_set_cell_mv(0u, 499u);
    tick(1u);
    TEST_ASSERT_BITS_HIGH(BMS_FAULT_SENSOR, faults());

    setUp();
    fake_hal_set_cell_mv(0u, 5001u);
    tick(1u);
    TEST_ASSERT_BITS_HIGH(BMS_FAULT_SENSOR, faults());

    setUp();
    fake_hal_set_cell_mv(0u, 500u);
    fake_hal_set_cell_mv(1u, 5000u);
    tick(1u);
    TEST_ASSERT_BITS_LOW(BMS_FAULT_SENSOR, faults());
}

void test_invalid_cell_is_excluded_from_min_max__REQ_BMS_045(void)
{
    fake_hal_set_cell_mv(0u, 0u);          /* broken wire: would look like deep UV */
    tick(5u);
    TEST_ASSERT_EQUAL_HEX8(BMS_FAULT_SENSOR, faults());   /* no UV fault */
    TEST_ASSERT_EQUAL_UINT16(3700u, bms_get_status()->vmin_mv);
}

void test_all_cells_invalid_reports_zero_min_max__REQ_BMS_045(void)
{
    fake_hal_set_all_cells_mv(0u);
    tick(5u);
    TEST_ASSERT_EQUAL_HEX8(BMS_FAULT_SENSOR, faults());
    TEST_ASSERT_EQUAL_UINT16(0u, bms_get_status()->vmin_mv);
    TEST_ASSERT_EQUAL_UINT16(0u, bms_get_status()->vmax_mv);
}

/* ================= FET control & latching ================= */

void test_fets_open_in_the_same_cycle_the_fault_is_set__REQ_BMS_050(void)
{
    tick(1u);
    assert_fets(true);
    fake_hal_set_temp_dc(650);
    tick(2u);
    assert_fets(true);
    tick(1u);
    TEST_ASSERT_EQUAL(BMS_STATE_FAULT, state());
    assert_fets(false);
}

void test_fault_is_latched_after_condition_disappears__REQ_BMS_051(void)
{
    fake_hal_set_temp_dc(650);
    tick(3u);
    fake_hal_set_temp_dc(250);
    tick(20u);
    TEST_ASSERT_EQUAL(BMS_STATE_FAULT, state());
    assert_fets(false);
}

void test_clear_is_rejected_while_condition_active__REQ_BMS_051(void)
{
    fake_hal_set_temp_dc(650);
    tick(3u);
    TEST_ASSERT_FALSE(bms_clear_faults());
    TEST_ASSERT_EQUAL(BMS_STATE_FAULT, state());
}

void test_clear_restores_fets_and_operating_state__REQ_BMS_051(void)
{
    fake_hal_set_temp_dc(650);
    tick(3u);
    fake_hal_set_temp_dc(250);
    fake_hal_set_current_ma(2000);
    tick(1u);
    TEST_ASSERT_TRUE(bms_clear_faults());
    TEST_ASSERT_EQUAL(BMS_STATE_CHARGING, state());
    assert_fets(true);
}

void test_clear_without_faults_is_accepted_and_harmless__REQ_BMS_051(void)
{
    tick(1u);
    TEST_ASSERT_TRUE(bms_clear_faults());
    TEST_ASSERT_EQUAL(BMS_STATE_IDLE, state());
}

void test_multiple_faults_are_reported_together__REQ_BMS_051(void)
{
    fake_hal_set_cell_mv(0u, 4300u);
    fake_hal_set_temp_dc(650);
    tick(3u);
    TEST_ASSERT_EQUAL_HEX8(BMS_FAULT_OV | BMS_FAULT_OT, faults());
}

/* ================= Operating state ================= */

void test_state_follows_current_with_100mA_deadband__REQ_BMS_060(void)
{
    const struct { int32_t ma; bms_state_t expected; } cases[] = {
        {     0, BMS_STATE_IDLE },
        {    99, BMS_STATE_IDLE },
        {   100, BMS_STATE_CHARGING },
        {   -99, BMS_STATE_IDLE },
        {  -100, BMS_STATE_DISCHARGING },
        { -5000, BMS_STATE_DISCHARGING },
    };
    for (size_t i = 0u; i < sizeof(cases) / sizeof(cases[0]); i++) {
        fake_hal_set_current_ma(cases[i].ma);
        tick(1u);
        TEST_ASSERT_EQUAL_MESSAGE(cases[i].expected, state(), "current case failed");
    }
}

void test_state_names__REQ_BMS_060(void)
{
    TEST_ASSERT_EQUAL_STRING("INIT", bms_state_name(BMS_STATE_INIT));
    TEST_ASSERT_EQUAL_STRING("IDLE", bms_state_name(BMS_STATE_IDLE));
    TEST_ASSERT_EQUAL_STRING("CHARGING", bms_state_name(BMS_STATE_CHARGING));
    TEST_ASSERT_EQUAL_STRING("DISCHARGING", bms_state_name(BMS_STATE_DISCHARGING));
    TEST_ASSERT_EQUAL_STRING("FAULT", bms_state_name(BMS_STATE_FAULT));
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", bms_state_name((bms_state_t)99));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_power_up_state_is_INIT_with_fets_off__REQ_BMS_001);
    RUN_TEST(test_first_cycle_with_nominal_inputs_enters_IDLE_fets_on__REQ_BMS_001);
    RUN_TEST(test_ov_does_not_trip_at_4199mV__REQ_BMS_010);
    RUN_TEST(test_ov_trips_at_exactly_4200mV_after_3_cycles__REQ_BMS_010);
    RUN_TEST(test_ov_does_not_trip_after_only_2_cycles__REQ_BMS_010);
    RUN_TEST(test_ov_debounce_restarts_when_voltage_dips__REQ_BMS_010);
    RUN_TEST(test_ov_stays_active_above_release_threshold__REQ_BMS_011);
    RUN_TEST(test_ov_releases_at_4100mV_and_can_be_cleared__REQ_BMS_011);
    RUN_TEST(test_uv_does_not_trip_at_3001mV__REQ_BMS_020);
    RUN_TEST(test_uv_trips_at_exactly_3000mV_after_3_cycles__REQ_BMS_020);
    RUN_TEST(test_uv_stays_active_below_release_threshold__REQ_BMS_021);
    RUN_TEST(test_uv_releases_at_3100mV__REQ_BMS_021);
    RUN_TEST(test_ot_does_not_trip_at_59_9C__REQ_BMS_030);
    RUN_TEST(test_ot_trips_at_60_0C_after_3_cycles__REQ_BMS_030);
    RUN_TEST(test_ot_stays_active_at_55_1C__REQ_BMS_031);
    RUN_TEST(test_ot_releases_at_55_0C__REQ_BMS_031);
    RUN_TEST(test_oc_discharge_boundary_29999_vs_30000mA__REQ_BMS_040);
    RUN_TEST(test_oc_charge_boundary_9999_vs_10000mA__REQ_BMS_040);
    RUN_TEST(test_oc_releases_as_soon_as_current_is_within_limits__REQ_BMS_040);
    RUN_TEST(test_sensor_fault_on_0mV_within_one_cycle__REQ_BMS_045);
    RUN_TEST(test_sensor_valid_window_boundaries__REQ_BMS_045);
    RUN_TEST(test_invalid_cell_is_excluded_from_min_max__REQ_BMS_045);
    RUN_TEST(test_all_cells_invalid_reports_zero_min_max__REQ_BMS_045);
    RUN_TEST(test_fets_open_in_the_same_cycle_the_fault_is_set__REQ_BMS_050);
    RUN_TEST(test_fault_is_latched_after_condition_disappears__REQ_BMS_051);
    RUN_TEST(test_clear_is_rejected_while_condition_active__REQ_BMS_051);
    RUN_TEST(test_clear_restores_fets_and_operating_state__REQ_BMS_051);
    RUN_TEST(test_clear_without_faults_is_accepted_and_harmless__REQ_BMS_051);
    RUN_TEST(test_multiple_faults_are_reported_together__REQ_BMS_051);
    RUN_TEST(test_state_follows_current_with_100mA_deadband__REQ_BMS_060);
    RUN_TEST(test_state_names__REQ_BMS_060);
    return UNITY_END();
}

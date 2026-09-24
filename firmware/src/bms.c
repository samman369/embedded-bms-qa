#include "bms.h"
#include "hal.h"
#include <string.h>

/* One debounced, hysteretic monitor per fault type. */
typedef struct {
    uint8_t count;    /* consecutive cycles the trip condition has been seen */
    bool    active;   /* condition present (tripped and not yet released)    */
} monitor_t;

enum { MON_OV = 0, MON_UV, MON_OT, MON_OC, MON_SENSOR, MON_COUNT };

static const uint8_t k_fault_bit[MON_COUNT] = {
    BMS_FAULT_OV, BMS_FAULT_UV, BMS_FAULT_OT, BMS_FAULT_OC, BMS_FAULT_SENSOR
};

static const uint8_t k_debounce[MON_COUNT] = {
    BMS_FAULT_DEBOUNCE_TICKS, BMS_FAULT_DEBOUNCE_TICKS, BMS_FAULT_DEBOUNCE_TICKS,
    BMS_FAULT_DEBOUNCE_TICKS, BMS_SENSOR_DEBOUNCE_TICKS
};

static bms_status_t s_status;
static monitor_t    s_mon[MON_COUNT];

static void monitor_update(monitor_t *m, bool trip, bool release, uint8_t debounce)
{
    if (m->active) {
        if (release) {
            m->active = false;
        }
        m->count = 0u;
    } else if (trip) {
        m->count++;
        if (m->count >= debounce) {
            m->active = true;
            m->count = 0u;
        }
    } else {
        m->count = 0u;
    }
}

static void set_fets(bool on)
{
    hal_set_charge_fet(on);
    hal_set_discharge_fet(on);
}

static void update_state(void)
{
    if (s_status.faults != BMS_FAULT_NONE) {
        s_status.state = BMS_STATE_FAULT;
        set_fets(false);                                   /* REQ-BMS-050 */
        return;
    }

    if (s_status.current_ma >= BMS_CURRENT_DEADBAND_MA) {  /* REQ-BMS-060 */
        s_status.state = BMS_STATE_CHARGING;
    } else if (s_status.current_ma <= -BMS_CURRENT_DEADBAND_MA) {
        s_status.state = BMS_STATE_DISCHARGING;
    } else {
        s_status.state = BMS_STATE_IDLE;
    }
    set_fets(true);
}

static bool cell_valid(uint16_t mv)
{
    return (mv >= BMS_CELL_VALID_MIN_MV) && (mv <= BMS_CELL_VALID_MAX_MV);
}

void bms_init(void)
{
    memset(&s_status, 0, sizeof(s_status));
    memset(s_mon, 0, sizeof(s_mon));
    s_status.state = BMS_STATE_INIT;                       /* REQ-BMS-001 */
    set_fets(false);
}

void bms_tick(void)
{
    bool     sensor_bad = false;
    bool     any_valid = false;
    uint16_t vmin = UINT16_MAX;
    uint16_t vmax = 0u;

    for (uint8_t i = 0u; i < BMS_NUM_CELLS; i++) {
        uint16_t mv = hal_read_cell_mv(i);
        s_status.cell_mv[i] = mv;
        if (!cell_valid(mv)) {                             /* REQ-BMS-045 */
            sensor_bad = true;
            continue;
        }
        any_valid = true;
        if (mv < vmin) { vmin = mv; }
        if (mv > vmax) { vmax = mv; }
    }
    if (!any_valid) {
        vmin = 0u;
        vmax = 0u;
    }
    s_status.vmin_mv = vmin;
    s_status.vmax_mv = vmax;
    s_status.temp_dc = hal_read_temp_dc();
    s_status.current_ma = hal_read_current_ma();

    monitor_update(&s_mon[MON_SENSOR], sensor_bad, !sensor_bad, k_debounce[MON_SENSOR]);

    if (any_valid) {
        monitor_update(&s_mon[MON_OV], vmax >= BMS_OV_TRIP_MV, vmax <= BMS_OV_RELEASE_MV,
                       k_debounce[MON_OV]);
        monitor_update(&s_mon[MON_UV], vmin <= BMS_UV_TRIP_MV, vmin >= BMS_UV_RELEASE_MV,
                       k_debounce[MON_UV]);
    }

    monitor_update(&s_mon[MON_OT], s_status.temp_dc >= BMS_OT_TRIP_DC,
                   s_status.temp_dc <= BMS_OT_RELEASE_DC, k_debounce[MON_OT]);

    bool oc = (s_status.current_ma <= BMS_OC_DSG_TRIP_MA) ||
              (s_status.current_ma >= BMS_OC_CHG_TRIP_MA);
    monitor_update(&s_mon[MON_OC], oc, !oc, k_debounce[MON_OC]);

    for (uint8_t m = 0u; m < (uint8_t)MON_COUNT; m++) {    /* REQ-BMS-051: latch */
        if (s_mon[m].active) {
            s_status.faults = (uint8_t)(s_status.faults | k_fault_bit[m]);
        }
    }

    update_state();
}

bool bms_clear_faults(void)
{
    if (s_status.faults == BMS_FAULT_NONE) {
        return true;
    }
    for (uint8_t m = 0u; m < (uint8_t)MON_COUNT; m++) {
        if (s_mon[m].active) {
            return false;                                  /* REQ-BMS-051 */
        }
    }
    s_status.faults = BMS_FAULT_NONE;
    update_state();
    return true;
}

const bms_status_t *bms_get_status(void)
{
    return &s_status;
}

const char *bms_state_name(bms_state_t state)
{
    switch (state) {
    case BMS_STATE_INIT:        return "INIT";
    case BMS_STATE_IDLE:        return "IDLE";
    case BMS_STATE_CHARGING:    return "CHARGING";
    case BMS_STATE_DISCHARGING: return "DISCHARGING";
    case BMS_STATE_FAULT:       return "FAULT";
    default:                    return "UNKNOWN";
    }
}

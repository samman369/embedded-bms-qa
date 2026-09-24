#ifndef BMS_H
#define BMS_H

#include <stdbool.h>
#include <stdint.h>
#include "bms_config.h"

typedef enum {
    BMS_STATE_INIT = 0,
    BMS_STATE_IDLE,
    BMS_STATE_CHARGING,
    BMS_STATE_DISCHARGING,
    BMS_STATE_FAULT
} bms_state_t;

#define BMS_FAULT_NONE    0x00u
#define BMS_FAULT_OV      0x01u
#define BMS_FAULT_UV      0x02u
#define BMS_FAULT_OT      0x04u
#define BMS_FAULT_OC      0x08u
#define BMS_FAULT_SENSOR  0x10u

typedef struct {
    bms_state_t state;
    uint8_t     faults;                 /* latched fault bits */
    uint16_t    cell_mv[BMS_NUM_CELLS];
    uint16_t    vmin_mv;                /* over valid cells only */
    uint16_t    vmax_mv;
    int16_t     temp_dc;
    int32_t     current_ma;
} bms_status_t;

void                bms_init(void);
void                bms_tick(void);          /* call every BMS_TICK_MS */
bool                bms_clear_faults(void);  /* false if a fault condition is still active */
const bms_status_t *bms_get_status(void);
const char         *bms_state_name(bms_state_t state);

#endif /* BMS_H */

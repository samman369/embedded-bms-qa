#ifndef BMS_CONFIG_H
#define BMS_CONFIG_H

/* Firmware identity */
#define FW_VERSION                 "1.0.0"

/* Pack topology */
#define BMS_NUM_CELLS              4u
#define BMS_TICK_MS                100u

/* Protection thresholds (see docs/requirements.md) */
#define BMS_OV_TRIP_MV             4200u   /* REQ-BMS-010 */
#define BMS_OV_RELEASE_MV          4100u   /* REQ-BMS-011 */
#define BMS_UV_TRIP_MV             3000u   /* REQ-BMS-020 */
#define BMS_UV_RELEASE_MV          3100u   /* REQ-BMS-021 */
#define BMS_OT_TRIP_DC             600     /* REQ-BMS-030, 0.1 degC units */
#define BMS_OT_RELEASE_DC          550     /* REQ-BMS-031 */
#define BMS_OC_DSG_TRIP_MA         (-30000L) /* REQ-BMS-040 */
#define BMS_OC_CHG_TRIP_MA         10000L
#define BMS_CURRENT_DEADBAND_MA    100L    /* REQ-BMS-060 */

/* Sensor plausibility window (REQ-BMS-045) */
#define BMS_CELL_VALID_MIN_MV      500u
#define BMS_CELL_VALID_MAX_MV      5000u

/* Number of consecutive cycles a condition must persist before tripping */
#define BMS_FAULT_DEBOUNCE_TICKS   3u
#define BMS_SENSOR_DEBOUNCE_TICKS  1u

/* Communication */
#define APP_RX_BUFFER_SIZE         128u
#define APP_RESP_MAX               96u

#endif /* BMS_CONFIG_H */

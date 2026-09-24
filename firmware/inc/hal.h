#ifndef HAL_H
#define HAL_H

/*
 * Hardware Abstraction Layer.
 * The application only talks to hardware through these functions, so the
 * same firmware links against:
 *   - a real MCU port (ADC, GPIO, UART drivers),
 *   - firmware/sim/hal_sim.c  (PC simulator used by functional tests),
 *   - tests/unit/fake_hal.c   (fake used by unit tests).
 */

#include <stdbool.h>
#include <stdint.h>

uint16_t hal_read_cell_mv(uint8_t cell);   /* cell voltage in mV            */
int16_t  hal_read_temp_dc(void);           /* pack temperature in 0.1 degC  */
int32_t  hal_read_current_ma(void);        /* + charge / - discharge, in mA */

void hal_set_charge_fet(bool on);
void hal_set_discharge_fet(bool on);

void hal_uart_write(const char *text);

#endif /* HAL_H */

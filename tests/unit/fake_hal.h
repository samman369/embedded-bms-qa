#ifndef FAKE_HAL_H
#define FAKE_HAL_H

/* Fake HAL for unit tests: tests set the "sensor" inputs and inspect the outputs. */

#include <stdbool.h>
#include <stdint.h>

void        fake_hal_reset(void);                 /* nominal: 3700 mV, 25.0 degC, 0 mA */
void        fake_hal_set_cell_mv(uint8_t cell, uint16_t mv);
void        fake_hal_set_all_cells_mv(uint16_t mv);
void        fake_hal_set_temp_dc(int16_t temp_dc);
void        fake_hal_set_current_ma(int32_t current_ma);
bool        fake_hal_charge_fet(void);
bool        fake_hal_discharge_fet(void);
const char *fake_hal_uart_output(void);           /* everything written since reset */
void        fake_hal_uart_clear(void);

#endif /* FAKE_HAL_H */

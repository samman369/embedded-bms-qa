#ifndef HAL_SIM_H
#define HAL_SIM_H

/* Test-bench side of the simulated hardware: inject sensor values, observe outputs. */

#include <stdbool.h>
#include <stdint.h>

void sim_hal_reset(void);
void sim_set_cell_mv(uint8_t cell, uint16_t mv);
void sim_set_temp_dc(int16_t temp_dc);
void sim_set_current_ma(int32_t current_ma);
bool sim_get_charge_fet(void);
bool sim_get_discharge_fet(void);

#endif /* HAL_SIM_H */

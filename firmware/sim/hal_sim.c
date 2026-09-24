#include "hal.h"
#include "hal_sim.h"
#include "bms_config.h"
#include <stdio.h>

static uint16_t s_cell_mv[BMS_NUM_CELLS];
static int16_t  s_temp_dc;
static int32_t  s_current_ma;
static bool     s_chg_fet;
static bool     s_dsg_fet;

void sim_hal_reset(void)
{
    for (uint8_t i = 0u; i < BMS_NUM_CELLS; i++) {
        s_cell_mv[i] = 3700u;          /* nominal Li-ion cell */
    }
    s_temp_dc = 250;                   /* 25.0 degC */
    s_current_ma = 0;
    s_chg_fet = false;
    s_dsg_fet = false;
}

void sim_set_cell_mv(uint8_t cell, uint16_t mv)
{
    if (cell < BMS_NUM_CELLS) {
        s_cell_mv[cell] = mv;
    }
}

void sim_set_temp_dc(int16_t temp_dc)      { s_temp_dc = temp_dc; }
void sim_set_current_ma(int32_t current_ma) { s_current_ma = current_ma; }
bool sim_get_charge_fet(void)              { return s_chg_fet; }
bool sim_get_discharge_fet(void)           { return s_dsg_fet; }

uint16_t hal_read_cell_mv(uint8_t cell)
{
    return (cell < BMS_NUM_CELLS) ? s_cell_mv[cell] : 0u;
}

int16_t hal_read_temp_dc(void)            { return s_temp_dc; }
int32_t hal_read_current_ma(void)         { return s_current_ma; }
void    hal_set_charge_fet(bool on)       { s_chg_fet = on; }
void    hal_set_discharge_fet(bool on)    { s_dsg_fet = on; }

void hal_uart_write(const char *text)
{
    fputs(text, stdout);
    fflush(stdout);
}

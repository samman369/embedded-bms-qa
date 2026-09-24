#include "fake_hal.h"
#include "hal.h"
#include "bms_config.h"
#include <string.h>

static uint16_t s_cell_mv[BMS_NUM_CELLS];
static int16_t  s_temp_dc;
static int32_t  s_current_ma;
static bool     s_chg_fet;
static bool     s_dsg_fet;
static char     s_uart[1024];
static size_t   s_uart_len;

void fake_hal_reset(void)
{
    fake_hal_set_all_cells_mv(3700u);
    s_temp_dc = 250;
    s_current_ma = 0;
    s_chg_fet = true;     /* deliberately "wrong" so tests prove the firmware drives them */
    s_dsg_fet = true;
    fake_hal_uart_clear();
}

void fake_hal_set_cell_mv(uint8_t cell, uint16_t mv)  { s_cell_mv[cell] = mv; }
void fake_hal_set_temp_dc(int16_t temp_dc)            { s_temp_dc = temp_dc; }
void fake_hal_set_current_ma(int32_t current_ma)      { s_current_ma = current_ma; }
bool fake_hal_charge_fet(void)                        { return s_chg_fet; }
bool fake_hal_discharge_fet(void)                     { return s_dsg_fet; }
const char *fake_hal_uart_output(void)                { return s_uart; }

void fake_hal_set_all_cells_mv(uint16_t mv)
{
    for (uint8_t i = 0u; i < BMS_NUM_CELLS; i++) {
        s_cell_mv[i] = mv;
    }
}

void fake_hal_uart_clear(void)
{
    s_uart[0] = '\0';
    s_uart_len = 0u;
}

/* ---- HAL implementation used by the firmware under test ---- */

uint16_t hal_read_cell_mv(uint8_t cell)     { return s_cell_mv[cell]; }
int16_t  hal_read_temp_dc(void)             { return s_temp_dc; }
int32_t  hal_read_current_ma(void)          { return s_current_ma; }
void     hal_set_charge_fet(bool on)        { s_chg_fet = on; }
void     hal_set_discharge_fet(bool on)     { s_dsg_fet = on; }

void hal_uart_write(const char *text)
{
    size_t n = strlen(text);
    if (s_uart_len + n < sizeof(s_uart)) {
        memcpy(&s_uart[s_uart_len], text, n + 1u);
        s_uart_len += n;
    }
}

/*
 * BMS PC simulator — a virtual device under test (DUT).
 *
 * stdin/stdout carry two channels, like a hardware test bench:
 *   - Lines starting with '@' are BENCH commands (inject sensor values,
 *     advance time, read FET outputs). Replies start with '@'.
 *   - Every other line is sent byte-by-byte into the firmware's UART RX
 *     interrupt, exactly as a serial cable would. Firmware replies are frames.
 *
 * Bench commands:
 *   @CELL <i> <mV>   set one cell voltage      @CELLS <mV>  set all cells
 *   @TEMP <0.1degC>  set temperature           @CUR <mA>    set current (+chg/-dsg)
 *   @TICK <n>        run n 100 ms cycles       @FETS?       read FET outputs
 *   @RXOVF?          read UART overflow count  @RESET       power-cycle the DUT
 *   @QUIT            exit
 */

#include "app.h"
#include "bms_config.h"
#include "hal_sim.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE_MAX_LEN 512

static void power_on(void)
{
    sim_hal_reset();
    app_init();
}

static int handle_bench(const char *line)
{
    char cmd[16] = {0};
    long a = 0;
    long b = 0;
    int  n = sscanf(line, "@%15s %ld %ld", cmd, &a, &b);

    if (n < 1) {
        puts("@ERR");
    } else if ((strcmp(cmd, "CELL") == 0) && (n == 3) && (a >= 0) && (a < (long)BMS_NUM_CELLS)) {
        sim_set_cell_mv((uint8_t)a, (uint16_t)b);
        puts("@OK");
    } else if ((strcmp(cmd, "CELLS") == 0) && (n == 2)) {
        for (uint8_t i = 0u; i < BMS_NUM_CELLS; i++) {
            sim_set_cell_mv(i, (uint16_t)a);
        }
        puts("@OK");
    } else if ((strcmp(cmd, "TEMP") == 0) && (n == 2)) {
        sim_set_temp_dc((int16_t)a);
        puts("@OK");
    } else if ((strcmp(cmd, "CUR") == 0) && (n == 2)) {
        sim_set_current_ma((int32_t)a);
        puts("@OK");
    } else if ((strcmp(cmd, "TICK") == 0) && (n == 2) && (a > 0)) {
        for (long i = 0; i < a; i++) {
            app_tick();
        }
        puts("@OK");
    } else if (strcmp(cmd, "FETS?") == 0) {
        printf("@FETS CHG=%d DSG=%d\n", sim_get_charge_fet() ? 1 : 0, sim_get_discharge_fet() ? 1 : 0);
    } else if (strcmp(cmd, "RXOVF?") == 0) {
        printf("@RXOVF %u\n", (unsigned)app_rx_overflow_count());
    } else if (strcmp(cmd, "RESET") == 0) {
        power_on();
        puts("@OK");
    } else if (strcmp(cmd, "QUIT") == 0) {
        return 0;
    } else {
        puts("@ERR");
    }
    fflush(stdout);
    return 1;
}

int main(void)
{
    char line[LINE_MAX_LEN];

    power_on();
    puts("@READY BMS-SIM " FW_VERSION);
    fflush(stdout);

    while (fgets(line, sizeof(line), stdin) != NULL) {
        if (line[0] == '@') {
            if (!handle_bench(line)) {
                break;
            }
            continue;
        }
        /* DUT UART: feed bytes through the RX ISR, main loop polls after each byte. */
        size_t len = strlen(line);
        for (size_t i = 0u; i < len; i++) {
            app_uart_rx_isr((uint8_t)line[i]);
            app_poll();
        }
    }
    return EXIT_SUCCESS;
}

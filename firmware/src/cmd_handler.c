#include "cmd_handler.h"
#include "bms.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define CMD_GET_CELL_PREFIX "GET_CELL,"

/* Parses 1-3 decimal digits and nothing else. */
static bool parse_index(const char *s, unsigned *out)
{
    unsigned value = 0u;
    size_t   n = 0u;

    while (s[n] != '\0') {
        if ((s[n] < '0') || (s[n] > '9') || (n >= 3u)) {
            return false;
        }
        value = (value * 10u) + (unsigned)(s[n] - '0');
        n++;
    }
    if (n == 0u) {
        return false;
    }
    *out = value;
    return true;
}

static void handle_status(char *resp, size_t size)
{
    const bms_status_t *st = bms_get_status();
    (void)snprintf(resp, size,
                   "OK,STATE=%s,FAULTS=0x%02X,VMIN=%u,VMAX=%u,TEMP=%d,CUR=%" PRId32,
                   bms_state_name(st->state), (unsigned)st->faults,
                   (unsigned)st->vmin_mv, (unsigned)st->vmax_mv,
                   (int)st->temp_dc, st->current_ma);
}

static void handle_get_cell(const char *arg, char *resp, size_t size)
{
    unsigned index;
    if (!parse_index(arg, &index) || (index >= BMS_NUM_CELLS)) {
        (void)snprintf(resp, size, "ERR,ARG");
        return;
    }
    (void)snprintf(resp, size, "OK,CELL%u=%u", index, (unsigned)bms_get_status()->cell_mv[index]);
}

void cmd_handle(const char *payload, char *resp, size_t resp_size)
{
    if ((payload == NULL) || (resp == NULL) || (resp_size == 0u)) {
        return;
    }

    if (strcmp(payload, "PING") == 0) {
        (void)snprintf(resp, resp_size, "OK,PONG");
    } else if (strcmp(payload, "GET_VERSION") == 0) {
        (void)snprintf(resp, resp_size, "OK,VERSION=%s", FW_VERSION);
    } else if (strcmp(payload, "GET_STATUS") == 0) {
        handle_status(resp, resp_size);
    } else if ((strcmp(payload, "GET_CELL") == 0) ||
               (strncmp(payload, CMD_GET_CELL_PREFIX, strlen(CMD_GET_CELL_PREFIX)) == 0)) {
        const char *arg = (payload[8] == ',') ? &payload[9] : "";
        handle_get_cell(arg, resp, resp_size);
    } else if (strcmp(payload, "CLEAR_FAULTS") == 0) {
        (void)snprintf(resp, resp_size, bms_clear_faults() ? "OK,CLEARED" : "ERR,FAULT_ACTIVE");
    } else {
        (void)snprintf(resp, resp_size, "ERR,UNKNOWN_CMD");
    }
}

#ifndef CMD_HANDLER_H
#define CMD_HANDLER_H

#include <stddef.h>

/* Executes one decoded command payload and writes the response payload (no framing). */
void cmd_handle(const char *payload, char *resp, size_t resp_size);

#endif /* CMD_HANDLER_H */
